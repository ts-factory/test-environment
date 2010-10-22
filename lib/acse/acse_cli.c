/** @file
 * @brief ACSE CLI
 *
 * ACS Emulator CLI tool
 *
 * @author Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * $Id$
 */
#include "te_config.h"

#include<string.h>
#include<ctype.h>
#include<popt.h>
#include <sys/types.h>
#include <sys/wait.h>



#include "acse_epc.h"
#include "acse_internal.h"
#include "acse_user.h"

#include "te_stdint.h"
#include "te_errno.h"
#include "te_queue.h"
#include "te_defs.h"
#include "te_cwmp.h"
#include "logger_api.h"

#include "logger_file.h"

#include "cli_utils.h"
#include "cwmp_utils.h"

DEFINE_LGR_ENTITY("ACSE");

#ifdef TE_LGR_USER
#undef TE_LGR_USER
#endif
#define TE_LGR_USER     "CLI"

#define NUM_OF(_arr) (sizeof(_arr)/sizeof(_arr [0]))



static char acs_def_name[30] = "";
static char cpe_def_name[50] = "";
static int timeout_def = 0;

static te_errno print_config_response(te_errno status,
                                      acse_epc_config_data_t *cfg_resp);


static te_errno print_cwmp_response(te_errno status,
                                    acse_epc_cwmp_data_t *cwmp_resp);

enum cli_codes {
    CMD_PARAM = 0x1000, 
    CMD_RPC, 
    CMD_CR, 
    CMD_ENV, 

    PARAM_OBTAIN = 0x1010, 
    PARAM_MODIFY, 
    PARAM_LIST, 
    PARAM_ADD, 
    PARAM_DEL, 

    RPC_SEND = 0x1020, 
    RPC_CHECK, 

    CR_SEND = 0x1030, 
    CR_CHECK, 

    ENV_ACS = 0x1040, 
    ENV_CPE, 
    ENV_TIMEOUT, 
};




static int
param_cmd_access(int argc, const int *arg_tags,
                 const char *rest_line, void *opaque)
{
    acse_epc_config_data_t *cfg_data;
    te_errno    rc;

    UNUSED(opaque);

    /* Command here:
       'param acs|cpe modify|obtain <param_name> <value>'
    */
    if (argc < 3)
        return -1;

    acse_conf_prepare(arg_tags[2] /* function */, &cfg_data);

    strncpy(cfg_data->acs, acs_def_name, sizeof(cfg_data->acs));
    if (EPC_CFG_ACS == arg_tags[1])
        cfg_data->cpe[0] = '\0';
    else 
        strncpy(cfg_data->cpe, cpe_def_name, sizeof(cfg_data->cpe));

    rest_line += cli_token_copy(rest_line, cfg_data->oid);

    cfg_data->op.level = arg_tags[1];

    if (EPC_CFG_MODIFY == cfg_data->op.fun)
        cli_token_copy(rest_line, cfg_data->value);
    else
        cfg_data->value[0] = '\0';

    rc = acse_conf_call(&cfg_data);

    if (TE_RC_GET_ERROR(rc) == TE_ENOTCONN)
    {
        printf("Connection broken\n");
        return -1;
    }
    print_config_response(rc, cfg_data);

    return 0;
}

static int
param_cmd_list(int argc, const int *arg_tags,
               const char *rest_line, void *opaque)
{ 
    UNUSED(argc);
    UNUSED(arg_tags);
    UNUSED(rest_line);
    UNUSED(opaque);
    return 0;
}

static int
param_cmd_ad(int argc, const int *arg_tags,
              const char *rest_line, void *opaque)
{
    UNUSED(argc);
    UNUSED(arg_tags);
    UNUSED(rest_line);
    UNUSED(opaque);
    return 0;
}




/**
 * Fill to_cpe field in cwmp_data, get text human inserted info from line.
 * Expects that rpc_cpe is set correctly
 */
static te_errno
parse_cwmp_rpc_args(acse_epc_cwmp_data_t *cwmp_data, const char *line)
{
    static char buf[300];

    assert(cwmp_data != NULL);

    switch(cwmp_data->rpc_cpe)
    {
        case CWMP_RPC_get_rpc_methods:
        case CWMP_RPC_NONE:
            cwmp_data->to_cpe.p = NULL;
            break;
        case CWMP_RPC_set_parameter_values: 
        {
            static cwmp_set_parameter_values_t req;
            static ParameterValueList pv_list;
            size_t tok_len; 
            char val_buf[200];
            char type_buf[20];
            int type;

            cwmp_values_array_t *val_arr;

            req.ParameterList = &pv_list; 
            val_arr = cwmp_val_array_alloc(NULL, NULL);
            /* TODO: good parse here with error check */

            while ((tok_len = cli_token_copy(line, buf)) > 0 )
            {
                line += tok_len;
                line += cli_token_copy(line, type_buf);
                line += cli_token_copy(line, val_buf);
                type = cwmp_val_type_s2i(type_buf);

                if (type == SOAP_TYPE_string)
                    cwmp_val_array_add(val_arr, buf, "", type, val_buf,
                                       VA_END_LIST);
                else /* we have int-like type */
                    cwmp_val_array_add(val_arr, buf, "",
                                       type, atoi(val_buf), VA_END_LIST);
            }

            pv_list.__ptrParameterValueStruct = val_arr->items;
            pv_list.__size                    = val_arr->size;
            cwmp_data->to_cpe.set_parameter_values = &req;
        }
        break;
        case CWMP_RPC_get_parameter_values: 
        {
            static ParameterNames             par_list;
            static _cwmp__GetParameterValues  req;
            size_t tok_len;

            string_array_t *names = cwmp_str_array_alloc(NULL, NULL);
            req.ParameterNames_ = &par_list;

            while ((tok_len = cli_token_copy(line, buf)) > 0 )
            {
                cwmp_str_array_add(names, buf, "", VA_END_LIST);
                line += tok_len;
            }

            par_list.__ptrstring = names->items;
            par_list.__size      = names->size;
            cwmp_data->to_cpe.get_parameter_values = &req;
        }
        break;
        case CWMP_RPC_get_parameter_names: 
        {
            static char name[256];
            static char *name_ptr = name;
            static cwmp_get_parameter_names_t req;

            cwmp_data->to_cpe.get_parameter_names = &req;

            req.ParameterPath = &name_ptr;
            line += cli_token_copy(line, buf);
            req.NextLevel = atoi(buf);

            cli_token_copy(line, name);
        }
        break;
        default:
            printf("parse input, RPC %s is not supported yet :(\n", 
                cwmp_rpc_cpe_string(cwmp_data->rpc_cpe));
    }
    return 0;
}

static int
rpc_send(int argc, const int *arg_tags,
         const char *rest_line, void *opaque)
{
    te_errno rc, status;
    acse_epc_cwmp_data_t *cwmp_data = NULL;

    UNUSED(opaque);

    if (argc < 3)
        return -1;

    rc = acse_cwmp_prepare(acs_def_name, cpe_def_name,
                           EPC_RPC_CALL, &cwmp_data);

    /* command here: rpc send <rpcname> [<rpc args>...] */

    cwmp_data->rpc_cpe = arg_tags[2]; 

    rc = parse_cwmp_rpc_args(cwmp_data, rest_line);
    if (rc != 0)
    {
        printf("parse cwmp data failed: %s\n", te_rc_err2str(rc));
        return -1;
    }

    rc = acse_cwmp_call(&status, NULL, &cwmp_data);
    if (0 != rc)
    {
        printf("ACSE call failed: %s\n", te_rc_err2str(rc));
        return -1;
    }
    else
    { 
        printf("status%s, request_id %d\n",
               te_rc_err2str(status), cwmp_data->request_id);
    }

    return 0;
}

static int
rpc_check(int argc, const int *arg_tags,
          const char *rest_line, void *opaque)
{
    te_errno rc, status;
    acse_epc_cwmp_data_t *cwmp_data = NULL;

    UNUSED(opaque);
    UNUSED(arg_tags);

    if (argc < 2)
        return -1;

    rc = acse_cwmp_prepare(acs_def_name, cpe_def_name,
                           EPC_RPC_CHECK, &cwmp_data);

    /* command here: rpc check <request_id> */

    cwmp_data->request_id = atoi(rest_line); 

    rc = acse_cwmp_call(&status, NULL, &cwmp_data);
    if (0 != rc)
    {
        printf("ACSE check failed: %s\n", te_rc_err2str(rc));
        return -1;
    }
    else
        print_cwmp_response(status, cwmp_data);

    return 0;
}


static int
rpc_get_acs(int argc, const int *arg_tags,
            const char *rest_line, void *opaque)
{
    UNUSED(argc);
    UNUSED(arg_tags);
    UNUSED(rest_line);
    UNUSED(opaque);
    printf("get ACS RPC unsupported\n");
    return 0;
}

static int
cr_cmd(int argc, const int *arg_tags,
       const char *rest_line, void *opaque)
{
    te_errno rc, status = 0;
    acse_epc_cwmp_data_t  *cwmp_data;

    UNUSED(opaque);
    UNUSED(rest_line);

    if (argc != 2)
        return -1;

    acse_cwmp_prepare(acs_def_name, cpe_def_name,
                      arg_tags[1], &cwmp_data);

    rc = acse_cwmp_call(&status, NULL, &cwmp_data);
    if (0 != rc)
    {
        printf("CWMP call failed: %s\n", te_rc_err2str(rc));
        return -1;
    }
    print_cwmp_response(status, cwmp_data);
    return 0;
}


static int
env_set(int argc, const int *arg_tags,
        const char *rest_line, void *opaque)
{
    size_t len;
    char new_value[100];
    UNUSED(opaque);
    len = cli_token_copy(rest_line, new_value);

    if (argc < 2)
        return -1;

    if (len > 0) /* Is there new value? */
        switch (arg_tags[1])
        {
            case ENV_ACS:
                strncpy(acs_def_name, new_value, sizeof(acs_def_name));
                break;
            case ENV_CPE:
                strncpy(cpe_def_name, new_value, sizeof(cpe_def_name));
                break;
            case ENV_TIMEOUT:
                timeout_def = atoi(new_value);
                break;
            default:
                printf("env_set: wrong tag!\n");
        }
    else
        switch (arg_tags[1])
        {
            case ENV_ACS:
                printf("%s\n", acs_def_name);
                break;
            case ENV_CPE:
                printf("%s\n", cpe_def_name);
                break;
            case ENV_TIMEOUT:
                printf("%d\n", timeout_def);
                break;
            default:
                printf("env_set: wrong tag!\n");
        }
    return 0;
}


static cli_cmd_descr_t cmd_param_actions[] = {
    {"obtain", EPC_CFG_OBTAIN, "ACS config commands",
            param_cmd_access, NULL},
    {"modify", EPC_CFG_MODIFY, "CPE config commands",
            param_cmd_access, NULL},
    {"list", EPC_CFG_LIST, "ACS config commands",
            param_cmd_list, NULL},
    {"add", EPC_CFG_ADD, "ACS config commands",
            param_cmd_ad, NULL},
    {"del", EPC_CFG_DEL, "CPE config commands",
            param_cmd_ad, NULL},
    END_CMD_ARRAY
};

static cli_cmd_descr_t cmd_param_lev[] = {
    {"acs", EPC_CFG_ACS, "ACS config commands", NULL, cmd_param_actions},
    {"cpe", EPC_CFG_CPE, "CPE config commands", NULL, cmd_param_actions},
    END_CMD_ARRAY
};

static cli_cmd_descr_t cmd_rpc_cpe_kinds[] = {
    {"fin",          CWMP_RPC_NONE, "HTTP 204, finish CWMP session",
                                NULL, NULL},
    {"get_rpc_m",    CWMP_RPC_get_rpc_methods, "GetRPCMethods", NULL, NULL},
    {"get_par_vals", CWMP_RPC_get_parameter_values, 
                    "GetParameterValues", NULL, NULL},
    {"set_par_vals", CWMP_RPC_set_parameter_values,
                    "SetParameterValues", NULL, NULL},
    {"get_names",    CWMP_RPC_get_parameter_names,
                    "GetParameterNames", NULL, NULL},
    END_CMD_ARRAY
};

static cli_cmd_descr_t cmd_rpc_actions[] = {
    {"send",  EPC_RPC_CALL,  "Send CWMP RPC", rpc_send, cmd_rpc_cpe_kinds},
    {"check", EPC_RPC_CHECK, "Check RPC status", rpc_check, NULL},
    {"get",   EPC_RPC_CHECK,  "Get CWMP ACS RPC", rpc_get_acs, NULL},
    END_CMD_ARRAY
};

static cli_cmd_descr_t cmd_cr_actions[] = {
    {"send",  EPC_CONN_REQ, "Send ConnectionRequest", NULL, NULL},
    {"check", EPC_CONN_REQ_CHECK, "Check Conn.Request", NULL, NULL},
    END_CMD_ARRAY
};

static cli_cmd_descr_t cmd_env[] = {
    {"acs",     ENV_ACS, "default ACS name", NULL, NULL},
    {"cpe",     ENV_CPE, "default CPE name", NULL, NULL},
    {"timeout", ENV_TIMEOUT, "CPE config commands", NULL, NULL},
    END_CMD_ARRAY
};

static cli_cmd_descr_t acse_cmd_list[] = {
    {"param",CMD_PARAM,"config parameters", NULL, cmd_param_lev},
    {"rpc",  CMD_RPC,  "CWMP RPC commands", NULL, cmd_rpc_actions},
    {"cr",   CMD_CR,   "Connection Req. commands", cr_cmd, cmd_cr_actions},
    {"env",  CMD_ENV,  "Current environment", env_set, cmd_env},
    END_CMD_ARRAY
};

/* TODO some normal way to parse command line?.. */


#if 0

static te_errno
cli_args_acs_cpe(const char *args, size_t *offset, char *acs, char *cpe)
{
    int i;
    const char *start_args = args;

    if (!(args && acs && cpe))
        return TE_EINVAL;

    while (isspace(*args)) args++;

    for (i = 0; args[i] && (!isspace(args[i])) && (args[i] != '/'); i++)
        acs[i] = args[i];
    acs[i] = '\0';
    args += i;

    while((*args) && (isspace(*args) || (*args == '/')))
        args++;
    
    if ((i = cli_token_copy(args, cpe)) == 0)
    {
        fprintf(stderr, "Call CR fails, args '%s', CPE name not detected\n",
              start_args);
        return TE_EFAIL;
    }
    args += i;

    if (offset)
        *offset = args - start_args;

    return 0;
}




static te_errno
cli_cfg_list(const char *args, acse_cfg_level_t level)
{
    acse_epc_msg_t msg;
    acse_epc_config_data_t cfg_data;

    msg.opcode = EPC_CONFIG_CALL;
    msg.data.cfg = &cfg_data;
    msg.length = sizeof(cfg_data);
    msg.status = 0;

    if (level == EPC_CFG_CPE)
        cli_token_copy(args, cfg_data.acs);
    
    cfg_data.op.magic = EPC_CONFIG_MAGIC;
    cfg_data.op.level = level;
    cfg_data.op.fun = EPC_CFG_LIST;

    acse_epc_send(&msg);
    return 0;
}


static te_errno
cli_cfg_add(const char *args, acse_cfg_level_t level)
{
    acse_epc_msg_t         msg;
    acse_epc_config_data_t cfg_data;

    msg.opcode = EPC_CONFIG_CALL;
    msg.data.cfg = &cfg_data;
    msg.length = sizeof(cfg_data);
    msg.status = 0;

    args += cli_token_copy(args, cfg_data.acs);
    
    if (level == EPC_CFG_CPE)
        cli_token_copy(args, cfg_data.cpe);
    else
        cfg_data.cpe[0] = '\0';

    cfg_data.op.magic = EPC_CONFIG_MAGIC;
    cfg_data.op.level = level;
    cfg_data.op.fun = EPC_CFG_ADD;

    acse_epc_send(&msg);

    return 0;
}

static te_errno
cli_acs_config(const char *args, acse_cfg_op_t fun)
{
    acse_epc_msg_t         msg;
    acse_epc_config_data_t cfg_data;

    msg.opcode = EPC_CONFIG_CALL;
    msg.data.cfg = &cfg_data;
    msg.length = sizeof(cfg_data);
    msg.status = 0;

    args += cli_token_copy(args, cfg_data.acs);

    args += cli_token_copy(args, cfg_data.oid);

    cli_token_copy(args, cfg_data.value);

    cfg_data.op.magic = EPC_CONFIG_MAGIC;
    cfg_data.op.level = EPC_CFG_ACS;
    cfg_data.op.fun = fun;
    cfg_data.cpe[0] = '\0';

    acse_epc_send(&msg);

    return 0;
}

static te_errno
cli_cpe_config(const char *args, acse_cfg_op_t fun)
{
    te_errno    rc;
    size_t      offset = 0;

    acse_epc_msg_t         msg;
    acse_epc_config_data_t cfg_data;

    msg.opcode = EPC_CONFIG_CALL;
    msg.data.cfg = &cfg_data;
    msg.length = sizeof(cfg_data);
    msg.status = 0;

    rc = cli_args_acs_cpe(args, &offset, cfg_data.acs, cfg_data.cpe);
    if (rc != 0)
    {
        fprintf(stderr, "Parse error 0x%x\n", rc);
        return rc;
    }
    args += offset;

    args += cli_token_copy(args, cfg_data.oid);

    if (fun == EPC_CFG_MODIFY)
        cli_token_copy(args, cfg_data.value);
    else
        cfg_data.value[0] = '\0';

    cfg_data.op.magic = EPC_CONFIG_MAGIC;
    cfg_data.op.level = EPC_CFG_CPE;
    cfg_data.op.fun = fun;

    acse_epc_send(&msg);
    return 0;
}

static te_errno
cli_cpe_cr(const char *args)
{
    te_errno rc;
    size_t offset = 0;

    acse_epc_msg_t msg;
    acse_epc_cwmp_data_t c_data;

    msg.opcode = EPC_CWMP_CALL;
    msg.data.cwmp = &c_data;
    msg.length = sizeof(c_data);

    memset(&c_data, 0, sizeof(c_data));

    if (strncmp(args, "call ", 5) == 0)
        c_data.op = EPC_CONN_REQ;
    else if (strncmp(args, "show ", 5) == 0)
        c_data.op = EPC_CONN_REQ_CHECK;
    else
    {
        printf("unsupported command for 'cpe cr'\n");
        return TE_EFAIL;
    }
    args += 5;

    rc = cli_args_acs_cpe(args, &offset, c_data.acs, c_data.cpe);
    if (rc != 0)
    {
        fprintf(stderr, "Parse error 0x%x\n", rc);
        return rc;
    }
    args += offset;

    rc = acse_epc_send(&msg);
    if (rc != 0)
        ERROR("%s(): EPC send failed %r", __FUNCTION__, rc);

    return 0;
}


static te_errno
cli_cpe_rpc(const char *args)
{
    te_errno rc;
    size_t offset = 0;

    acse_epc_msg_t msg;
    acse_epc_cwmp_data_t c_data;

    msg.opcode = EPC_CWMP_CALL;
    msg.data.cwmp = &c_data;
    msg.length = sizeof(c_data);

    memset(&c_data, 0, sizeof(c_data));

    if (strncmp(args, "call ", 5) == 0)
    {
        args += 5;
        c_data.op = EPC_RPC_CALL ;
        
        /* todo full parsing */
        c_data.rpc_cpe = CWMP_RPC_get_rpc_methods; 
        c_data.to_cpe.p = NULL; 
    }
    else if (strncmp(args, "show ", 5) == 0)
    {
        args += 5;
        c_data.op = EPC_RPC_CHECK;
        c_data.request_id = atoi(args);

        while (isdigit(*args)) args++;
        while (isspace(*args)) args++;
    }
    else
    {
        printf("unsupported command for 'cpe cr'\n");
        return TE_EFAIL;
    }

    rc = cli_args_acs_cpe(args, &offset, c_data.acs, c_data.cpe);
    if (rc != 0)
    {
        fprintf(stderr, "Parse error 0x%x\n", rc);
        return rc;
    }
    args += offset;

    rc = acse_epc_send(&msg);
    if (rc != 0)
        ERROR("%s(): EPC send failed %r", __FUNCTION__, rc);

    return 0;
}


static te_errno
cli_cpe_inform(const char *args)
{
    te_errno rc;
    size_t offset = 0;

    acse_epc_msg_t msg;
    acse_epc_cwmp_data_t c_data;

    msg.opcode = EPC_CWMP_CALL;
    msg.data.cwmp = &c_data;
    msg.length = sizeof(c_data);

    memset(&c_data, 0, sizeof(c_data));

    c_data.op = EPC_GET_INFORM;
    c_data.request_id = atoi(args);

    while (isdigit(*args)) args++;
    while (isspace(*args)) args++;

    rc = cli_args_acs_cpe(args, &offset, c_data.acs, c_data.cpe);
    if (rc != 0)
    {
        fprintf(stderr, "Parse error 0x%x\n", rc);
        return rc;
    }
    args += offset;

    rc = acse_epc_send(&msg);
    if (rc != 0)
        ERROR("%s(): EPC send failed %r", __FUNCTION__, rc);

    return 0;
}

static te_errno
cli_parse_exec_cpe(const char *args)
{
    if (strncmp(args, "add ", 4) == 0)
        return cli_cfg_add(args + 4, EPC_CFG_CPE);
    if (strncmp(args, "list ", 5) == 0)
        return cli_cfg_list(args + 5, EPC_CFG_CPE);
    if (strncmp(args, "cr ", 3) == 0)
        return cli_cpe_cr(args + 3);
    if (strncmp(args, "inform ", 7) == 0)
        return cli_cpe_inform(args + 7);
    if (strncmp(args, "rpc ", 4) == 0)
        return cli_cpe_rpc(args + 4);
    if (strncmp(args, "modify ", 7) == 0)
        return cli_cpe_config(args + 7, EPC_CFG_MODIFY);
    if (strncmp(args, "obtain ", 7) == 0)
        return cli_cpe_config(args + 7, EPC_CFG_OBTAIN);
    return 0;
}

static te_errno
cli_parse_exec_acs(const char *args)
{
    if (strncmp(args, "add ", 4) == 0)
        return cli_cfg_add(args + 4, EPC_CFG_ACS);
    if (strncmp(args, "list", 4) == 0)
        return cli_cfg_list(args + 4, EPC_CFG_ACS);
    if (strncmp(args, "modify ", 7) == 0)
        return cli_acs_config(args + 7, EPC_CFG_MODIFY);
    if (strncmp(args, "obtain ", 7) == 0)
        return cli_acs_config(args + 7, EPC_CFG_OBTAIN);
    return 0;
}

static te_errno
epc_parse_cli(const char *buf, size_t len)
{
    if (strncmp(buf, "cpe ", 4) == 0)
        return cli_parse_exec_cpe(buf + 4);

    if (strncmp(buf, "acs ", 4) == 0)
        return cli_parse_exec_acs(buf + 4);

    return 0;
}

#endif

static void
print_rpc_response(acse_epc_cwmp_data_t *cwmp_resp)
{
    switch (cwmp_resp->rpc_cpe)
    {
    case CWMP_RPC_get_rpc_methods: 
    {
        MethodList *mlist;
        if ((mlist = cwmp_resp->from_cpe.get_rpc_methods_r->MethodList_)
                != NULL)
        {
            int i;
            printf("RPC methods: ");
            for (i = 0; i < mlist->__size; i++)
                printf("'%s', ", mlist->__ptrstring[i]);
            printf("\n");
        }
    }
        break;
    case CWMP_RPC_set_parameter_values: 
        printf("Set status: %d\n", 
               cwmp_resp->from_cpe.set_parameter_values_r->Status);
        break;
    case CWMP_RPC_get_parameter_values: 
    {
        char buf[300];
        int i;
        ParameterValueList *pv_list =
            cwmp_resp->from_cpe.get_parameter_values_r->ParameterList;

        for (i = 0; i < pv_list->__size; i++)
        {
            snprint_ParamValueStruct(buf, sizeof(buf),
                            pv_list->__ptrParameterValueStruct[i]);
            printf("  %s\n", buf);
        }
    }
    break;
    case CWMP_RPC_get_parameter_names: 
    {
        int i;
        struct cwmp__ParameterInfoStruct *item;
        ParameterInfoList *pi_list =
            cwmp_resp->from_cpe.get_parameter_names_r->ParameterList;

        for (i = 0; i < pi_list->__size; i++)
        {
            item = pi_list->__ptrParameterInfoStruct[i];
            printf("  (%c) %s\n", item->Writable ? 'W' : '-', item->Name);
        }
    }
    break;
    case CWMP_RPC_NONE:
    case CWMP_RPC_set_parameter_attributes: 
    case CWMP_RPC_get_parameter_attributes: 
    case CWMP_RPC_add_object: 
    case CWMP_RPC_delete_object: 
    case CWMP_RPC_reboot: 
    case CWMP_RPC_download: 
    case CWMP_RPC_upload: 
    case CWMP_RPC_factory_reset: 
    case CWMP_RPC_get_queued_transfers: 
    case CWMP_RPC_get_all_queued_transfers: 
    case CWMP_RPC_schedule_inform: 
    case CWMP_RPC_set_vouchers: 
    case CWMP_RPC_get_options: 
    case CWMP_RPC_FAULT: 
        printf("TODO... \n");
        break;
    }
}

static te_errno
print_cwmp_response(te_errno status, acse_epc_cwmp_data_t *cwmp_resp)
{
    switch(cwmp_resp->op)
    {
    case EPC_CONN_REQ:
    case EPC_CONN_REQ_CHECK:
        printf("Connection request to %s/%s, state %d\n",
                cwmp_resp->acs, cwmp_resp->cpe,
                (int)cwmp_resp->from_cpe.cr_state);
        break;
    case EPC_RPC_CALL:
        printf("RPC call '%s' to %s/%s, id %d\n",
                cwmp_rpc_cpe_string(cwmp_resp->rpc_cpe),
                cwmp_resp->acs, cwmp_resp->cpe, cwmp_resp->request_id);
        break;
    case EPC_RPC_CHECK:
        printf("RPC check, '%s' to %s/%s, status %s\n",
                cwmp_rpc_cpe_string(cwmp_resp->rpc_cpe),
                cwmp_resp->acs, cwmp_resp->cpe,
                te_rc_err2str(status));
        if (0 == status)
            print_rpc_response(cwmp_resp);
        if (TE_CWMP_FAULT == TE_RC_GET_ERROR(status))
        {
#define FAULT_BUF_SIZE 0x8000
            char *fault_buf = malloc(FAULT_BUF_SIZE);
            snprint_cwmpFault(fault_buf, FAULT_BUF_SIZE,
                              cwmp_resp->from_cpe.fault);
            printf("%s\n", fault_buf);
            free(fault_buf);
        }
        break;
    case EPC_GET_INFORM:
        {
            _cwmp__Inform *inform = cwmp_resp->from_cpe.inform;
            printf("Get Inform from %s/%s, id %d\n",
                    cwmp_resp->acs, cwmp_resp->cpe, cwmp_resp->request_id);
            if (status != 0)
            {
                printf("failed, status '%s'\n", te_rc_err2str(status));
                break;
            }
            printf("Device OUI: '%s'\n", inform->DeviceId->OUI);
            if (inform->Event != NULL)
            {
                int i;
                for(i = 0; i < inform->Event->__size; i++)
                {
                    cwmp__EventStruct *ev =
                        inform->Event->__ptrEventStruct[i];
                    if (ev != NULL)
                        printf("Event[%d]: '%s'\n", i, ev->EventCode);
                }
            }
        }
        break;
    }
    return 0;
}

static te_errno
print_config_response(te_errno status, acse_epc_config_data_t *cfg_resp)
{ 

    if (status != 0)
        printf("ERROR in response: %s\n",
            te_rc_err2str(status));
    else 
        printf("Result: %s\n", cfg_resp->value);
    return 0;
}

#define BUF_SIZE 256

static void
cli_exit_handler(void)
{
    RING("Normal exit from CLI");
    acse_epc_close();
}

char *epc_sock_name = NULL;
char *script_name = NULL;
int   acse_fork = 0;

char *cli_logfile = NULL;
char *acse_logfile = NULL;

struct poptOption acse_cli_opts[] = 
{
    {"epc-socket", 'e', POPT_ARG_STRING, &epc_sock_name, 0,
            "filename for EPC socket", "EPC socket"},
    {"fork",       'f', POPT_ARG_NONE,   &acse_fork, 0,
            "whether to fork", "flag to fork"},
    {"script",     's', POPT_ARG_STRING, &script_name, 0, 
            "filename with list of commands to perform before operation",
            "script"},
#ifndef CLI_SINGLE
    {"daemon-logfile",'d', POPT_ARG_STRING, &acse_logfile, 0,
            "filename for ACSE daemon logfile", "CLI logfile"},
#endif
    {"cli-logfile",'c', POPT_ARG_STRING,   &cli_logfile, 0,
            "filename for CLI logfile", "CLI logfile"},
    {NULL, 0, 0, NULL, 0, NULL, NULL}
};

static int
dummy_init()
{
    acs_t *acs;
    cpe_t *cpe;
    
    db_add_acs(acs_def_name);
    db_add_cpe(acs_def_name, cpe_def_name);

    acs = db_find_acs(acs_def_name);
    cpe = db_find_cpe(acs, acs_def_name, cpe_def_name);

    acs->port = 8080;

    cpe->acs_auth.login =
        strdup("000261-Home Gateway-V601L622R1A0-1001742119");
    cpe->acs_auth.passwd = strdup("z7cD7CTDA1DrQKUb");

    cpe->cr_auth.login  = strdup(cpe->acs_auth.login);
    cpe->cr_auth.passwd = strdup(cpe->acs_auth.passwd);
            
    /* acse_enable_acs(acs); */
    return 0;
}

int 
main(int argc, const char **argv)
{
    te_errno rc;
    int rpoll, rpopt;
    poptContext cont;
    pid_t acse_main_pid = 0;
    int acse_main_status;

    strcpy(acs_def_name, "A");
    strcpy(cpe_def_name, "box");

    cont = poptGetContext(NULL, argc, argv, acse_cli_opts, 0);
    
    rpopt = poptGetNextOpt(cont); /* this really parse all command line */

#ifndef CLI_SINGLE
    if (acse_fork)
    {
        acse_main_pid = fork();
        if (acse_main_pid == 0)
        {

            te_lgr_entity = "ACSE daemon";
            if (acse_logfile != NULL)
            {
                FILE   *log_fd = fopen(acse_logfile, "a");
                if (log_fd == NULL)
                {
                    perror("open ACSE logfile failed");
                    exit(1);
                }
                te_log_message_file_out = log_fd;
            }
            dummy_init();

            if ((rc = acse_epc_disp_init(NULL, NULL)) != 0)
            {
                ERROR("Fail create EPC dispatcher %r", rc);
                return 1;
            }

            acse_loop();
            exit(0);
        }
        if (acse_main_pid < 0)
        {
            perror("fork failed");
            exit(2);
        }
        /* Now we in CLI process, just continue.*/
    }
#endif /* CLI_SINGLE */
    if (cli_logfile != NULL)
    {
        FILE   *log_fd = fopen(cli_logfile, "a");
        if (log_fd == NULL)
        {
            perror("open CLI logfile failed");
            exit(1);
        }
        te_log_message_file_out = log_fd;
    }

    if ((rc = acse_epc_open(epc_sock_name, NULL, ACSE_EPC_CLIENT))
        != 0)
    {
        ERROR("open EPC failed %r", rc);
        return 1;
    }
    atexit(&cli_exit_handler);

    printf("\n> "); fflush(stdout);

    /* TODO process command line script */
    /* main loop */
    while (1)
    {
        struct pollfd pfd[2];

        pfd[0].fd = 0; /* stdin */
        pfd[0].events = POLLIN;
        pfd[0].revents = 0;
        pfd[1].fd = acse_epc_socket();
        pfd[1].events = POLLIN;
        pfd[1].revents = 0;

        rpoll = poll(pfd, 2, -1);
        if (rpoll > 0)
        {
            if (pfd[0].revents)
            {
                char buf[BUF_SIZE];
                ssize_t r = read(pfd[0].fd, buf, BUF_SIZE);

                if (r < 0)
                {
                    perror("read fail");
                    break;
                }
                if (r == 0) /* The end of input */
                    break;

                buf[r] = '\0';
#if 0
                rc = epc_parse_cli(buf, r);
                if (rc != 0)
                    RING("parse error %r", rc);
#else
                cli_perform_cmd(acse_cmd_list, buf);
                printf("> "); fflush(stdout);
#endif
            }
            if (pfd[1].revents)
            {
                acse_epc_msg_t msg_resp;
                rc = acse_epc_recv(&msg_resp);
                if (TE_RC_GET_ERROR(rc) == TE_ENOTCONN)
                    break;
                else if (rc != 0)
                    RING("EPC recv error %r", rc);
                switch (msg_resp.opcode)
                {
                    case EPC_CONFIG_RESPONSE:
                        print_config_response(msg_resp.status,
                                              msg_resp.data.cfg);
                        break;
                    case EPC_CWMP_RESPONSE:
                        print_cwmp_response(msg_resp.status,
                                            msg_resp.data.cwmp);
                        break;
                    default:
                        ERROR("Unexpected opcode 0x%x from EPC",
                             msg_resp.opcode);
                }
                printf("> "); fflush(stdout);
            }
        }
    }
    if ((rc = acse_epc_close()) != 0)
    {
        ERROR("CLI: EPC close failed %r", rc);
    }
#ifndef CLI_SINGLE
    acse_main_status = 0;
    waitpid(acse_main_pid, &acse_main_status, 0);
    if (acse_main_status)
    {
        WARN("ACSE finished with status %d", acse_main_status);
    }
#endif
    return 0;
}

