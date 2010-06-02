/** @file
 * @brief Test API for ACSE usage
 *
 * Implementation of Test API to ACSE.
 *
 * Copyright (C) 2009 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * @author Konstantin Abramenko <konst@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "TAPI ACSE"

#include "te_config.h"

#include "logger_api.h"
#include "rcf_common.h"
#include "tapi_acse.h"
#include "tapi_cfg_base.h"

#include "tapi_rpc_tr069.h"
#include "acse_epc.h"
#include "cwmp_data.h"


int va_end_list_var = 10;
void * const va_end_list_ptr = &va_end_list_ptr;


/* see description in tapi_acse.h */
te_errno
tapi_acse_start(const char *ta)
{
    return cfg_set_instance_fmt(CFG_VAL(INTEGER, 1),
                                "/agent:%s/acse:", ta);
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_stop(const char *ta)
{
    te_errno rc;
    char     buf[256];

    TE_SPRINTF(buf, "/agent:%s/acse:", ta);

    if ((rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, 0), "%s", buf)) == 0)
        return cfg_synchronize(buf, TRUE);

    return rc;
}

static inline int
acse_is_int_var(const char *name)
{
    return
        ((0 == strcmp(name, "port"))     || 
         (0 == strcmp(name, "ssl"))      ||
         (0 == strcmp(name, "enabled"))  ||
         (0 == strcmp(name, "cr_state")) ||
         (0 == strcmp(name, "sync_mode")) ||
         (0 == strcmp(name, "hold_requests")) ||
         (0 == strcmp(name, "cwmp_state"))  );
}



/** generic internal method for ACSE manage operations */
static inline te_errno
tapi_acse_manage_vlist(const char *ta, const char *acs_name,
                     const char *cpe_name,
                     acse_op_t opcode, va_list  ap)
{
    te_errno gen_rc = 0;

    char cpe_name_buf[RCF_MAX_PATH] = "";

    if (cpe_name != NULL)
        snprintf(cpe_name_buf, RCF_MAX_PATH, "/cpe:%s", cpe_name);

    if (ACSE_OP_ADD == opcode)
    {
        gen_rc = cfg_add_instance_fmt(NULL, CFG_VAL(NONE, 0),
                          "/agent:%s/acse:/acs:%s%s", ta, acs_name,
                          cpe_name_buf);
    }

    while (1)
    {
        char *name = va_arg(ap, char *);
        char buf[RCF_MAX_PATH];
        te_errno rc = 0;

        snprintf(buf, RCF_MAX_PATH, "/agent:%s/acse:/acs:%s%s/%s:",
                 ta, acs_name, cpe_name_buf, name);

        if (VA_END_LIST == name)
            break;

        if (ACSE_OP_OBTAIN == opcode)
        { 
            cfg_val_type type;

            if (acse_is_int_var(name))
            {/* integer parameters */
                int *p_val = va_arg(ap, int *);
                type = CVT_INTEGER;
                rc = cfg_get_instance_fmt(&type, p_val, "%s", buf);
            }
            else /* string parameters */
            {
                char **p_val = va_arg(ap, char **);
                type = CVT_STRING;
                rc = cfg_get_instance_fmt(&type, p_val, "%s", buf);
            }
        }
        else
        {
            if (acse_is_int_var(name))
            {/* integer parameters */
                int val = va_arg(ap, int);
                rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, val), "%s", buf);
            }
            else /* string parameters */
            {
                char *val = va_arg(ap, char *);
                rc = cfg_set_instance_fmt(CFG_VAL(STRING, val), "%s", buf);
            }
        }
        if (0 == gen_rc) /* store in 'gen_rc' first TE errno */
            gen_rc = rc;
    } 

    return gen_rc;
}



te_errno
tapi_acse_manage_cpe(const char *ta, const char *acs_name,
                     const char *cpe_name,
                     acse_op_t opcode, ...)
{
    va_list  ap;
    te_errno rc;
    va_start(ap, opcode);
    rc = tapi_acse_manage_vlist(ta, acs_name, cpe_name, opcode, ap);
    va_end(ap);
    return rc;
}



/* see description in tapi_acse.h */
te_errno
tapi_acse_manage_acs(const char *ta, const char *acs_name,
                     acse_op_t opcode, ...)
{
    va_list  ap;
    te_errno rc;
    va_start(ap, opcode);
    rc = tapi_acse_manage_vlist(ta, acs_name, NULL, opcode, ap);
    va_end(ap);
    return rc;
}


/*
 * ==================== Useful config ACSE methods =====================
 */

/* see description in tapi_acse.h */
te_errno
tapi_acse_clear_acs(const char *ta_acse, const char *acs_name)
{
    te_errno rc;
    rc = tapi_acse_manage_acs(ta_acse, acs_name, ACSE_OP_MODIFY,
                              "enabled", 0, VA_END_LIST);
    if (0 == rc)
        rc = tapi_acse_manage_acs(ta_acse, acs_name, ACSE_OP_MODIFY,
                                  "enabled", 1, VA_END_LIST);
    return rc;
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_clear_cpe(const char *ta_acse, 
                    const char *acs_name, const char *cpe_name)
{
    te_errno rc;
    rc = tapi_acse_manage_cpe(ta_acse, acs_name, cpe_name,
                              ACSE_OP_MODIFY,
                              "enabled", FALSE, VA_END_LIST);
    if (0 == rc)
        rc = tapi_acse_manage_cpe(ta_acse, acs_name, cpe_name,
                                  ACSE_OP_MODIFY,
                                  "enabled", TRUE, VA_END_LIST);
    return rc;
}



/* see description in tapi_acse.h */
te_errno
tapi_acse_wait_cwmp_state(const char *ta,
                          const char *acs_name, const char *cpe_name,
                          cwmp_sess_state_t want_state, int timeout)
{
    cwmp_sess_state_t cur_state = 0;
    te_errno rc;

    do {
        rc = tapi_acse_manage_cpe(ta, acs_name, cpe_name, ACSE_OP_OBTAIN,
                  "cwmp_state", &cur_state, VA_END_LIST);
        if (rc != 0)
            return rc;

    } while ((timeout < 0 || (timeout--) > 0) &&
             (want_state != cur_state) &&
             (sleep(1) == 0));

    if (0 == timeout && want_state != cur_state)
        return TE_ETIMEDOUT;

    return 0;
}



/* see description in tapi_acse.h */
te_errno
tapi_acse_wait_cr_state(const char *ta,
                          const char *acs_name, const char *cpe_name,
                          acse_cr_state_t want_state, int timeout)
{
    acse_cr_state_t cur_state = 0;
    te_errno rc;

    do {
        rc = tapi_acse_manage_cpe(ta, acs_name, cpe_name, ACSE_OP_OBTAIN,
                  "cr_state", &cur_state, VA_END_LIST);
        if (rc != 0)
            return rc;

    } while ((timeout < 0 || (timeout--) > 0) &&
             (want_state != cur_state) &&
             (sleep(1) == 0));

    if (0 == timeout && want_state != cur_state)
        return TE_ETIMEDOUT;

    return 0;
}

/*
 * =============== Generic methods for CWMP RPC ====================
 */

#define ACSE_BUF_SIZE 65536

te_errno
tapi_acse_cpe_rpc(rcf_rpc_server *rpcs,
                  const char *acs_name, const char *cpe_name,
                  te_cwmp_rpc_cpe_t cpe_rpc_code, int *call_index,
                  cwmp_data_to_cpe_t to_cpe)
{
    uint8_t *buf = malloc(ACSE_BUF_SIZE);
    ssize_t pack_s;

    if (NULL != to_cpe.p)
    {
        pack_s = cwmp_pack_call_data(to_cpe, cpe_rpc_code,
                                     buf, ACSE_BUF_SIZE);
        if (pack_s < 0)
        {
            ERROR("%s(): pack fail", __FUNCTION__);
            return TE_RC(TE_TAPI, TE_EINVAL);
        }
    }
    else 
        pack_s = 0;

    return rpc_cwmp_op_call(rpcs, acs_name, cpe_name,
                            cpe_rpc_code, buf, pack_s, call_index);
}

/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_rpc_response(rcf_rpc_server *rpcs,
                           const char *acs_name, const char *cpe_name,
                           int timeout, int call_index,
                           te_cwmp_rpc_cpe_t *cpe_rpc_code,
                           cwmp_data_from_cpe_t *from_cpe)
{
    te_errno rc;
    uint8_t *cwmp_buf = NULL;
    size_t buflen = 0;
    te_cwmp_rpc_cpe_t cwmp_rpc_loc;

    do {
        rc = rpc_cwmp_op_check(rpcs, acs_name, cpe_name, call_index,
                               0, &cwmp_rpc_loc, &cwmp_buf, &buflen);
    } while ((timeout < 0 || (timeout--) > 0) &&
             (TE_EPENDING == TE_RC_GET_ERROR(rc)) &&
             (sleep(1) == 0));
    RING("%s(): rc %r, cwmp_rpc %d", __FUNCTION__, rc, (int)cwmp_rpc_loc);

    if ((0 == rc || TE_CWMP_FAULT == TE_RC_GET_ERROR(rc)) &&
        NULL != from_cpe)
    {
        ssize_t unp_rc;
        if (NULL == cwmp_buf || 0 == buflen)
        {
            WARN("op_check return success, but buffer is NULL.");
            return 0;
        }
        unp_rc = cwmp_unpack_response_data(cwmp_buf, buflen, cwmp_rpc_loc);

        if (0 == unp_rc)
            from_cpe->p = cwmp_buf;
        else
        {
            from_cpe->p = NULL;
            ERROR("%s(): unpack error, rc %r", __FUNCTION__, unp_rc);
            return TE_RC(TE_TAPI, unp_rc);
        }
        if (NULL != cpe_rpc_code)
            *cpe_rpc_code = cwmp_rpc_loc;

    }
    return rc;
}

/* see description in tapi_acse.h */
te_errno
tapi_acse_get_rpc_acs(rcf_rpc_server *rpcs,
                      const char *acs_name,
                      const char *cpe_name,
                      int timeout, te_cwmp_rpc_acs_t rpc_acs,
                      cwmp_data_from_cpe_t *from_cpe)
{
    te_errno rc;
    uint8_t *cwmp_buf = NULL;
    size_t buflen = 0;

    do {
        rc = rpc_cwmp_op_check(rpcs, acs_name, cpe_name, 0,
                               rpc_acs, NULL, &cwmp_buf, &buflen);
    } while ((timeout < 0 || (timeout--) > 0) &&
             (TE_ENOENT == TE_RC_GET_ERROR(rc)) &&
             (sleep(1) == 0));
    RING("%s(): rc %r", __FUNCTION__, rc);

    if (0 == rc && NULL != from_cpe)
    {
        ssize_t unp_rc;
        if (NULL == cwmp_buf || 0 == buflen)
        {
            WARN("op_check return success, but buffer is NULL.");
            return 0;
        }
        unp_rc = cwmp_unpack_acs_rpc_data(cwmp_buf, buflen, rpc_acs);

        if (0 == unp_rc)
            from_cpe->p = cwmp_buf;
        else
        {
            from_cpe->p = NULL;
            ERROR("%s(): unpack error, rc %r", __FUNCTION__, unp_rc);
            return TE_RC(TE_TAPI, unp_rc);
        }
    }
    return rc;
}

/*
 * ==================== CWMP RPC methods =========================
 */




/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_get_rpc_methods(rcf_rpc_server *rpcs,
                              const char *acs_name,
                              const char *cpe_name,
                              int *call_index)
{
    return rpc_cwmp_op_call(rpcs, acs_name, cpe_name,
                        CWMP_RPC_get_rpc_methods,
                        NULL, 0, call_index);
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_get_rpc_methods_resp(
                               rcf_rpc_server *rpcs,
                               const char *acs_name,
                               const char *cpe_name,
                               int timeout, int call_index,
                               _cwmp__GetRPCMethodsResponse **resp)
{
    cwmp_data_from_cpe_t from_cpe_loc;
    te_errno rc = tapi_acse_cpe_rpc_response(rpcs, acs_name, cpe_name, 
                                             timeout, call_index,
                                             NULL, &from_cpe_loc);
    if (NULL != resp && NULL != from_cpe_loc.p)
        *resp = from_cpe_loc.get_rpc_methods_r;
    return rc;
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_download(rcf_rpc_server *rpcs,
                       const char *acs_name,
                       const char *cpe_name,
                       _cwmp__Download *req,
                       int *call_index)
{
    cwmp_data_to_cpe_t to_cpe_loc;
    to_cpe_loc.download = req;

    return tapi_acse_cpe_rpc(rpcs, acs_name, cpe_name,
                             CWMP_RPC_download, call_index,
                             to_cpe_loc);
}

/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_download_resp(rcf_rpc_server *rpcs,
                           const char *acs_name,
                           const char *cpe_name,
                           int timeout, int call_index,
                           _cwmp__DownloadResponse **resp)
{
    cwmp_data_from_cpe_t from_cpe_loc;
    te_errno rc = tapi_acse_cpe_rpc_response(rpcs, acs_name, cpe_name, 
                                             timeout, call_index,
                                             NULL, &from_cpe_loc);
    if (NULL != resp && NULL != from_cpe_loc.p)
        *resp = from_cpe_loc.download_r;
    return rc;
}



/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_get_parameter_values(rcf_rpc_server *rpcs,
                                   const char *acs_name,
                                   const char *cpe_name,
                                   _cwmp__GetParameterValues *req,
                                   int *call_index)
{
    cwmp_data_to_cpe_t to_cpe_loc;
    to_cpe_loc.get_parameter_values = req;

    return tapi_acse_cpe_rpc(rpcs, acs_name, cpe_name,
                             CWMP_RPC_get_parameter_values, call_index,
                             to_cpe_loc);
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_get_parameter_values_resp(
               rcf_rpc_server *rpcs,
               const char *acs_name, const char *cpe_name,
               int timeout, int call_index,
               _cwmp__GetParameterValuesResponse **resp)
{
    cwmp_data_from_cpe_t from_cpe_loc;
    te_errno rc = tapi_acse_cpe_rpc_response(rpcs, acs_name, cpe_name, 
                                             timeout, call_index,
                                             NULL, &from_cpe_loc);
    if (NULL != resp && NULL != from_cpe_loc.p)
        *resp = from_cpe_loc.get_parameter_values_r;
    return rc;
}



/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_get_parameter_names(rcf_rpc_server *rpcs,
                                   const char *acs_name,
                                   const char *cpe_name,
                                   _cwmp__GetParameterNames *req,
                                   int *call_index)
{
    cwmp_data_to_cpe_t to_cpe_loc;
    to_cpe_loc.get_parameter_names = req;

    return tapi_acse_cpe_rpc(rpcs, acs_name, cpe_name,
                             CWMP_RPC_get_parameter_names, call_index,
                             to_cpe_loc);
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_get_parameter_names_resp(
               rcf_rpc_server *rpcs,
               const char *acs_name, const char *cpe_name,
               int timeout, int call_index,
               _cwmp__GetParameterNamesResponse **resp)
{
    cwmp_data_from_cpe_t from_cpe_loc;
    te_errno rc = tapi_acse_cpe_rpc_response(rpcs, acs_name, cpe_name, 
                                             timeout, call_index,
                                             NULL, &from_cpe_loc);
    if (NULL != resp && NULL != from_cpe_loc.p)
        *resp = from_cpe_loc.get_parameter_names_r;
    return rc;
}

/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_set_parameter_values(rcf_rpc_server *rpcs,
                                   const char *acs_name,
                                   const char *cpe_name,
                                   _cwmp__SetParameterValues *req,
                                   int *call_index)
{
    cwmp_data_to_cpe_t to_cpe_loc;
    to_cpe_loc.set_parameter_values = req;

    return tapi_acse_cpe_rpc(rpcs, acs_name, cpe_name,
                             CWMP_RPC_set_parameter_values, call_index,
                             to_cpe_loc);
}


/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_set_parameter_values_resp(
               rcf_rpc_server *rpcs,
               const char *acs_name, const char *cpe_name,
               int timeout, int call_index,
               _cwmp__SetParameterValuesResponse **resp)
{
    cwmp_data_from_cpe_t from_cpe_loc;
    te_errno rc = tapi_acse_cpe_rpc_response(rpcs, acs_name, cpe_name, 
                                             timeout, call_index,
                                             NULL, &from_cpe_loc);
    if (NULL != resp && NULL != from_cpe_loc.p)
        *resp = from_cpe_loc.set_parameter_values_r;
    return rc;
}





/* see description in tapi_acse.h */
te_errno
tapi_acse_cpe_disconnect(rcf_rpc_server *acse_rpcs,
                         const char *acs_name,
                         const char *cpe_name)
{
    return rpc_cwmp_op_call(acse_rpcs, acs_name, cpe_name,
                            CWMP_RPC_NONE, NULL, 0, NULL);
}



/*
 * ============= Useful routines for prepare CWMP RPC params =============
 */

/* see description in tapi_acse.h */
_cwmp__GetParameterNames *
cwmp_get_names_alloc(const char *name, te_bool next_level)
{
    _cwmp__GetParameterNames *ret = calloc(1, sizeof(*ret));
    ret->NextLevel = next_level;
    ret->ParameterPath = malloc(sizeof(char*));
    if (NULL == name)
        ret->ParameterPath[0] = NULL;
    else    
        ret->ParameterPath[0] = strdup(name);
    return ret;
}

/* see description in tapi_acse.h */
void
cwmp_get_names_free(_cwmp__GetParameterNames *arg)
{
    if (NULL == arg)
        return;
    do {
        if (NULL == arg->ParameterPath)
            break;
        free(arg->ParameterPath[0]);
        free(arg->ParameterPath);
    } while (0);
    free(arg);
    return;
}



/* see description in tapi_acse.h */
void
cwmp_get_names_resp_free(_cwmp__GetParameterNamesResponse *resp)
{
    if (NULL == resp)
        return;
    /* response is assumed to be obtained from this TAPI, with pointers,
        which are filled by cwmp_data_unpack_* methods, so there is only
        one block of allocated memory */
    free(resp);
    return;
}

#define CWMP_VAL_ARR_QUANTUM 8


/* see description in tapi_acse.h */
_cwmp__GetParameterValues *
cwmp_get_values_alloc(const char *b_name, const char *first_name, ...)
{
    va_list     ap;
    size_t      real_arr_len = 0, i = 0;
    const char *v_name = first_name;
    char      **array = NULL;
    size_t      b_len, n_len;

    if (NULL == b_name || NULL == first_name)
        return NULL;

    b_len = strlen(b_name);
    _cwmp__GetParameterValues *ret = malloc(sizeof(*ret));

    va_start(ap, first_name);
    do {
        n_len = (NULL != v_name ? strlen(v_name) : 0);

        if (real_arr_len <= i)
        {
            real_arr_len += CWMP_VAL_ARR_QUANTUM;
            array = realloc(array, real_arr_len * sizeof(char*));
        }
        array[i] = malloc(b_len + n_len + 1);
        strcpy(array[i], b_name);
        if (v_name != NULL)
            strcpy(array[i] + b_len, v_name);

        v_name = va_arg(ap, const char *);
        i++;
    } while (VA_END_LIST != v_name);
    va_end(ap);

    ret->ParameterNames_ = malloc(sizeof(struct ParameterNames));
    ret->ParameterNames_->__size = i;
    ret->ParameterNames_->__ptrstring = array;

    return ret;
} 

/* see description in tapi_acse.h */
te_errno
cwmp_get_values_add(_cwmp__GetParameterValues *req,
                    const char *b_name,
                    const char *f_name, ...)
{
    ERROR("%s(): TODO", __FUNCTION__);
    return TE_EFAIL;
}


/* see description in tapi_acse.h */
void
cwmp_get_values_free(_cwmp__GetParameterValues *req)
{
    size_t i;
    if (NULL == req)
        return;
    do {
        if (NULL == req->ParameterNames_)
            break;
        for (i = 0; i < req->ParameterNames_->__size; i++)
            free(req->ParameterNames_->__ptrstring[i]);
        free(req->ParameterNames_);
    } while (0);
    free(req);
}

/* see description in tapi_acse.h */
void
cwmp_get_values_resp_free(_cwmp__GetParameterValues *resp)
{
    if (NULL == resp)
        return;
    /* response is assumed to be obtained from this TAPI, with pointers,
        which are filled by cwmp_data_unpack_* methods, so there is only
        one block of allocated memory */
    free(resp);
    return;
}




_cwmp__SetParameterValues *
cwmp_set_values_alloc(const char *par_key,
                      const char *base_name, 
                      const char *first_name, ...)
{
    va_list     ap;
    size_t      real_arr_len = 0, i = 0;
    const char *v_name = first_name;
    size_t      b_len, v_len;

    struct cwmp__ParameterValueStruct **array = NULL;

    b_len = (NULL == base_name ? 0 : strlen(base_name));

    _cwmp__SetParameterValues *ret = malloc(sizeof(*ret));

    ret->ParameterKey = strdup(NULL != par_key ? par_key : "tapi acse");

    va_start(ap, first_name);
    do {
        if (real_arr_len <= i)
        {
            real_arr_len += CWMP_VAL_ARR_QUANTUM;
            array = realloc(array, real_arr_len * sizeof(array[0]));
        }
        v_len = (NULL != v_name ? strlen(v_name) : 0);
        array[i] = malloc(sizeof(struct cwmp__ParameterValueStruct));
        array[i]->Name = malloc(b_len + v_len + 1);
        memcpy(array[i]->Name, base_name, b_len);
        memcpy(array[i]->Name + b_len, v_name, v_len);

        array[i]->__type = va_arg(ap, int);
        switch (array[i]->__type)
        {
#define SET_SOAP_TYPE(type_, arg_type_) \
            do { \
                type_ val = va_arg(ap, arg_type_ ); \
                array[i]->Value = malloc(sizeof(type_)); \
                *((type_ *)array[i]->Value) = val; \
            } while (0)

            case SOAP_TYPE_xsd__boolean:         
            case SOAP_TYPE_int:         
                SET_SOAP_TYPE(int, int);  break;
            case SOAP_TYPE_byte:
                SET_SOAP_TYPE(char, int); break;
            case SOAP_TYPE_unsignedInt:
                SET_SOAP_TYPE(uint32_t, int); break;
            case SOAP_TYPE_unsignedByte:
                SET_SOAP_TYPE(uint8_t, int); break;
            case SOAP_TYPE_time:
                SET_SOAP_TYPE(time_t, time_t); break;

            case SOAP_TYPE_string:
            case SOAP_TYPE_SOAP_ENC__base64:
                {
                    const char * val = va_arg(ap, const char *);
                    array[i]->Value = strdup(NULL != val ? val : "");
                }
                break;
        }

        v_name = va_arg(ap, const char *);
        i++;
    } while (VA_END_LIST != v_name);
    va_end(ap);

    ret->ParameterList = malloc(sizeof(struct ParameterValueList));
    ret->ParameterList->__size = i;
    ret->ParameterList->__ptrParameterValueStruct = array;

    return ret;
}


void
cwmp_set_values_free(_cwmp__SetParameterValues *req)
{
    size_t i;
    if (NULL == req)
        return;
    do {
        free(req->ParameterKey);

        if (NULL == req->ParameterList)
            break;
        for (i = 0; i < req->ParameterList->__size; i++)
        {
            struct cwmp__ParameterValueStruct *par_val = 
                req->ParameterList->__ptrParameterValueStruct[i];
            free(par_val->Name);
            free(par_val->Value);
            free(par_val);
        }
        free(req->ParameterList);
    } while (0);
    free(req);
}

static inline const char *
soap_simple_type_string(int type)
{
    static char buf[10];
    switch (type)
    {
        case SOAP_TYPE_int:          return "SOAP_TYPE_int";
        case SOAP_TYPE_xsd__boolean: return "SOAP_TYPE_boolean";
        case SOAP_TYPE_byte:         return "SOAP_TYPE_byte";
        case SOAP_TYPE_string:       return "SOAP_TYPE_string";
        case SOAP_TYPE_unsignedInt:  return "SOAP_TYPE_unsignedInt";
        case SOAP_TYPE_unsignedByte: return "SOAP_TYPE_unsignedByte";
        case SOAP_TYPE_time:         return "SOAP_TYPE_time";
        case SOAP_TYPE_SOAP_ENC__base64: return "SOAP_TYPE_base64";
    }
    snprintf(buf, sizeof(buf), "<unknown: %d>", type);
    return buf;
}

size_t
snprint_ParamValueStruct(char *buf, size_t len, 
                         cwmp__ParameterValueStruct *p_v)
{
    size_t rest_len = len;
    size_t used;

    char *p = buf;
    void *v  = p_v->Value;
    int  type = p_v->__type;

    used = sprintf(p, "%s (type %s) = ", p_v->Name, 
            soap_simple_type_string(p_v->__type));
    p+= used; rest_len -= used;
    switch(type)
    {
        case SOAP_TYPE_string:
        case SOAP_TYPE_SOAP_ENC__base64:
            p+= snprintf(p, rest_len, "'%s'", (char *)v);
            break;
        case SOAP_TYPE_time:
            p+= snprintf(p, rest_len, "time %dsec", (int)(*((time_t *)v)));
            break;
        case SOAP_TYPE_byte:
            p+= snprintf(p, rest_len, "%d", (int)(*((char *)v)));
            break;
        case SOAP_TYPE_int:    
            p+= snprintf(p, rest_len, "%d", *((int *)v));
            break;
        case SOAP_TYPE_unsignedInt:
            p+= snprintf(p, rest_len, "%u", *((uint32_t *)v));
            break;
        case SOAP_TYPE_unsignedByte:
            p+= snprintf(p, rest_len, "%u", (uint32_t)(*((uint8_t *)v)));
            break;
        case SOAP_TYPE_xsd__boolean:
            p+= snprintf(p, rest_len, *((int *)v) ? "True" : "False");
            break;
    }
    return p - buf;
}
