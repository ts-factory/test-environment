/** @file
 * @brief SUN RPC control interface
 *
 * RCF RPC implementation.
 *
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "RCF RPC"

/* To get pthread_mutexattr_settype() definition in pthread.h */
#define _GNU_SOURCE 

#include "te_config.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SEMAPHORE_H
#include <semaphore.h>
#else
#error "We have no semaphores"
#endif

#include "te_defs.h"
#include "te_stdint.h"
#include "te_errno.h"
#include "logger_api.h"
#include "conf_api.h"
#include "rcf_api.h"
#include "rcf_rpc.h"
#include "rcf_internal.h"
#include "rpc_xdr.h"
#include "tarpc.h"
#include "te_rpc_errno.h"


/** Initialize mutex and forwarding semaphore for RPC server */
static int
rpc_server_sem_init(rcf_rpc_server *rpcs)
{
#ifdef HAVE_PTHREAD_H
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP) != 0 ||
        pthread_mutex_init(&rpcs->lock, &attr) != 0)
    {
        ERROR("Cannot initialize mutex for RPC server");
        return TE_OS_RC(TE_RCF_API, errno); 
    }
#endif
    
    return 0;
}


/**
 * Obtain server handle. RPC server is created/restarted, if necessary.
 *
 * @param ta            a test agent
 * @param name          name of the new server (should not start from
 *                      fork_ or thread_)
 * @param father        father name or NULL (should be NULL if clear is 
 *                      FALSE or existing is TRUE)
 * @param thread        if TRUE, the thread should be created instead 
 *                      process
 * @param existing      get only existing RPC server
 * @param clear         get newly created or restarted RPC server
 * @param p_handle      location for new RPC server handle
 *
 * @return Status code
 */
te_errno 
rcf_rpc_server_get(const char *ta, const char *name,
                   const char *father, te_bool thread, 
                   te_bool existing, te_bool clear, 
                   rcf_rpc_server **p_handle)
{
    int   sid;
    char *val0 = NULL;
    int   rc, rc1;
    
    rcf_rpc_server *rpcs = NULL;
    cfg_handle      handle = CFG_HANDLE_INVALID;
    char            val[RCF_RPC_NAME_LEN];
    
    /* Validate parameters */
    if (ta == NULL || name == NULL ||
        strlen(name) >= RCF_RPC_NAME_LEN - strlen("thread_") ||
        strcmp_start("thread_", name) == 0 ||
        strcmp_start("fork_", name) == 0 ||
        ((existing || !clear) && father != NULL) || 
        (thread && father == NULL))
    {
        return TE_RC(TE_RCF_API, TE_EINVAL);
    }
    
    /* Try to find existing RPC server */
    rc = cfg_get_instance_fmt(NULL, NULL, "/agent:%s/rpcserver:%s",
                              ta, name);
 
    if (rc != 0 && existing)
        return TE_RC(TE_RCF_API, TE_ENOENT);
    
    if (cfg_get_instance_fmt(NULL, &sid, 
                             "/rpcserver_sid:%s:%s",
                             ta, name) != 0)
    {
         /* Server is created in the configurator.conf */
        if ((rc1 = rcf_ta_create_session(ta, &sid)) != 0)
        {
            ERROR("Cannot allocate RCF SID");
            return rc1;
        }
            
        if ((rc1 = cfg_add_instance_fmt(&handle, CFG_VAL(INTEGER, sid),
                                        "/rpcserver_sid:%s:%s", 
                                        ta, name)) != 0)
        {
            ERROR("Failed to specify SID for the RPC server %s", name);
            return rc1;
        }
    }        
        
    /* FIXME: thread support to be done */
    if (father != NULL && strcmp(father, "local") != 0 &&
        strcmp(father, "existing") != 0 &&
        cfg_get_instance_fmt(NULL, &val0, "/agent:%s/rpcserver:%s", 
                             ta, father) != 0)
    {
        ERROR("Cannot find father %s to create server %s", father, name);
        return TE_RC(TE_RCF_API, TE_ENOENT);
    }
    
    if (father == NULL)
        *val = 0;
    else if (thread)
        sprintf(val, "thread_%s", father);
    else
        sprintf(val, "fork_%s", father);

    if ((rpcs = (rcf_rpc_server *)
                    calloc(1, sizeof(rcf_rpc_server))) == NULL)
    {
        return TE_RC(TE_RCF_API, TE_ENOMEM);
    }
    
    if ((rc1 = rpc_server_sem_init(rpcs)) != 0)
    {
        free(rpcs);
        return rc1;
    }

#define RETERR(rc, msg...) \
    do {                                \
        free(rpcs);                     \
        free(val0);                     \
        ERROR(msg);                     \
        return TE_RC(TE_RCF_API, rc);   \
    } while (0)
        
    if (rc == 0 && !clear)
    {
#ifdef RCF_RPC_CHECK_USABILITY    
        /* Check that it is not dead */
        tarpc_getpid_in  in;
        tarpc_getpid_out out;
        char             lib = 0;
        
        memset(&in, 0, sizeof(in));
        memset(&out, 0, sizeof(out));

        in.common.op = RCF_RPC_CALL_WAIT;
        in.common.lib = &lib;
 
        if ((rc = rcf_ta_call_rpc(ta, sid, name, 1000, "getpid", 
                                  &in, &out)) != 0)
        {
            if (existing)
            {
                if (TE_RC_GET_ERROR(rc) != TE_EBUSY)
                    RETERR(rc, "RPC server %s is dead and cannot be used",
                           name);
            }
            else
            {
                clear = TRUE;
                WARN("RPC server %s is not usable and "
                     "will be restarted", name);
            }
        }
#endif        
    }
    else if (rc == 0 && clear)
    {
        /* Restart it */
        if ((rc = cfg_del_instance_fmt(FALSE, "/agent:%s/rpcserver:%s", 
                                       ta, name)) != 0 ||
            (rc = cfg_add_instance_fmt(&handle, CVT_STRING, val0, 
                                       "/agent:%s/rpcserver:%s", 
                                       ta, name)) != 0)
        {
            RETERR(rc, "Failed to restart RPC server %s", name);
        }
    }
    else 
    {
        if ((rc = cfg_add_instance_fmt(&handle, CVT_STRING, val, 
                                       "/agent:%s/rpcserver:%s", 
                                       ta, name)) != 0)
        {
            RETERR(rc, "Cannot add RPC server instance");
        }
    }
        
#undef RETERR    

    /* Create RPC server structure and fill it */
    strcpy(rpcs->ta, ta);
    strcpy(rpcs->name, name);
    rpcs->iut_err_jump = TRUE;
    rpcs->op = RCF_RPC_CALL_WAIT;
    rpcs->def_timeout = RCF_RPC_DEFAULT_TIMEOUT;
    rpcs->timeout = RCF_RPC_UNSPEC_TIMEOUT;
    rpcs->sid = sid;
    if (p_handle != NULL)
        *p_handle = rpcs;
    else
        free(rpcs);
    
    free(val0);

    return 0;
}

/* See description in rcf_rpc.h */
te_errno
rcf_rpc_servers_restart_all(void)
{
    const char * const  pattern = "/agent:*/rpcserver:*";

    te_errno        rc;
    unsigned int    num;
    cfg_handle     *handles = NULL;
    unsigned int    i;
    cfg_oid        *oid = NULL;

    rc = cfg_find_pattern(pattern, &num, &handles);
    if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
    {
        return 0;
    }
    else if (rc != 0)
    {
        ERROR("Failed to find by pattern '%s': %r", pattern, rc);
        return rc;
    }

    for (i = 0; i < num; ++i)
    {
        rc = cfg_get_oid(handles[i], &oid);
        if (rc != 0)
        {
            ERROR("%s(): cfg_get_oid() failed for #%u: %r",
                  __FUNCTION__, i, rc);
            break;
        }

        rc = rcf_rpc_server_get(CFG_OID_GET_INST_NAME(oid, 1),
                                CFG_OID_GET_INST_NAME(oid, 2),
                                NULL, FALSE, TRUE, TRUE, NULL);
        if (rc != 0)
        {
            ERROR("%s(): rcf_rpc_server_get() failed for #%u: %r",
                  __FUNCTION__, i, rc);
            break;
        }

        cfg_free_oid(oid); oid = NULL;
    }
    cfg_free_oid(oid);
    free(handles);

    return rc;
}


/**
 * Perform execve() on the RPC server. Filename of the running process
 * if used as the first argument.
 *
 * @param rpcs          RPC server handle
 *
 * @return status code
 */
te_errno 
rcf_rpc_server_exec(rcf_rpc_server *rpcs)
{
    tarpc_execve_in  in;
    tarpc_execve_out out;
    
    int  rc;
    char lib = 0;

    if (rpcs == NULL)
        return TE_RC(TE_RCF, TE_EINVAL);

    if (pthread_mutex_lock(&rpcs->lock) != 0)
        ERROR("pthread_mutex_lock() failed");
        
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.name = rpcs->name;
    in.common.lib = &lib;

    rpcs->op = RCF_RPC_CALL_WAIT;
    rc = rcf_ta_call_rpc(rpcs->ta, rpcs->sid, rpcs->name, 0xFFFFFFFF, 
                         "execve", &in, &out);
                         
    if (pthread_mutex_unlock(&rpcs->lock) != 0)
        ERROR("pthread_mutex_unlock() failed");

    if (rc == 0)
        RING("RPC (%s,%s): execve() -> (%s)",
             rpcs->ta, rpcs->name, errno_rpc2str(RPC_ERRNO(rpcs)));
    else
        ERROR("RPC (%s,%s): execve() -> (%s), rc=%r",
              rpcs->ta, rpcs->name, errno_rpc2str(RPC_ERRNO(rpcs)), rc);

    return rc;
}

/**
 * Destroy RPC server. The caller should assume the RPC server non-existent 
 * even if the function returned non-zero.
 *
 * @param rpcs          RPC server handle
 *
 * @return status code
 */
te_errno 
rcf_rpc_server_destroy(rcf_rpc_server *rpcs)
{
    int rc;
    
    if (rpcs == NULL)
        return TE_RC(TE_RCF, TE_EINVAL);
    
    VERB("Destroy RPC server %s", rpcs->name);
    
    if (pthread_mutex_lock(&rpcs->lock) != 0)
        ERROR("pthread_mutex_lock() failed");

    if ((rc = cfg_del_instance_fmt(FALSE, "/agent:%s/rpcserver:%s",
                                   rpcs->ta, rpcs->name)) != 0 &&
        TE_RC_GET_ERROR(rc) != TE_ENOENT)
    {
        ERROR("Failed to delete RPC server %s: error=%r", rpcs->name, rc);
        if (pthread_mutex_unlock(&rpcs->lock) != 0)
            ERROR("pthread_mutex_unlock() failed");
        
        return rc;
    }

    free(rpcs->nv_lib);
    free(rpcs);
    
    VERB("RPC server is destroyed successfully");

    return 0;
}

/**
 * Call SUN RPC on the TA via RCF. The function is also used for
 * checking of status of non-blocking RPC call and waiting for
 * the finish of the non-blocking RPC call.
 *
 * @param rpcs          RPC server
 * @param proc          RPC to be called
 * @param in_arg        Input argument
 * @param out_arg       Output argument
 *
 * @attention The status code is returned in rpcs _errno.
 *            If rpcs is NULL the function does nothing.
 */
void
rcf_rpc_call(rcf_rpc_server *rpcs, const char *proc, 
             void *in_arg, void *out_arg)
{
    tarpc_in_arg  *in = (tarpc_in_arg *)in_arg;
    tarpc_out_arg *out = (tarpc_out_arg *)out_arg;
    
    te_bool op_is_done;

    if (rpcs == NULL)
    {
        ERROR("Invalid RPC server handle is passed to rcf_rpc_call()");
        return;
    }
        
    if (in_arg == NULL || out_arg == NULL || proc == NULL)
    {
        rpcs->_errno = TE_RC(TE_RCF_API, TE_EINVAL);
        return;
    }
    
    op_is_done = strcmp(proc, "rpc_is_op_done") == 0;

    VERB("Calling RPC %s", proc);
        
    pthread_mutex_lock(&rpcs->lock);
    rpcs->_errno = 0;
    rpcs->err_log = FALSE;
    if (rpcs->timeout == RCF_RPC_UNSPEC_TIMEOUT)
        rpcs->timeout = rpcs->def_timeout;
    
    if (rpcs->op == RCF_RPC_CALL || rpcs->op == RCF_RPC_CALL_WAIT)
    {
        if (rpcs->tid0 != 0)
        {
            rpcs->_errno = TE_RC(TE_RCF_API, TE_EBUSY);
            pthread_mutex_unlock(&rpcs->lock);
            return;
        }
    }
    else 
    {
        if (rpcs->tid0 == 0)
        {
            ERROR("Try to wait not called RPC");
            rpcs->_errno = TE_RC(TE_RCF_API, TE_EALREADY);
            pthread_mutex_unlock(&rpcs->lock);
            return;
        }
        else if (strcmp(rpcs->proc, proc) != 0 && !op_is_done)
        {
            ERROR("Try to wait RPC %s instead called RPC %s", 
                  proc, rpcs->proc);
            rpcs->_errno = TE_RC(TE_RCF_API, TE_EPERM);
            pthread_mutex_unlock(&rpcs->lock);
            return;
        }
    }
    
    in->start = rpcs->start;
    in->op = rpcs->op;
    in->tid = rpcs->tid0;
    in->done = rpcs->is_done_ptr;
    in->lib = rpcs->lib;

    if (!op_is_done)
        strcpy(rpcs->proc, proc); 

    rpcs->_errno = rcf_ta_call_rpc(rpcs->ta, rpcs->sid, rpcs->name, 
                                   rpcs->timeout, proc, in, out);

    if (rpcs->op != RCF_RPC_CALL)
        rpcs->timeout = RCF_RPC_UNSPEC_TIMEOUT;
    rpcs->start = 0;
    *(rpcs->lib) = '\0';
    if (TE_RC_GET_ERROR(rpcs->_errno) == TE_ERPCTIMEOUT ||
        TE_RC_GET_ERROR(rpcs->_errno) == TE_ETIMEDOUT ||
        TE_RC_GET_ERROR(rpcs->_errno) == TE_ERPCDEAD)
    {
        rpcs->timed_out = TRUE;
    }
    
    if (rpcs->_errno == 0)
    {
        rpcs->duration = out->duration; 
        rpcs->_errno = out->_errno;
        rpcs->timed_out = FALSE;
        if (rpcs->op == RCF_RPC_CALL)
        {
            rpcs->tid0 = out->tid;
            rpcs->is_done_ptr = out->done;
            rpcs->op = RCF_RPC_WAIT;
        }
        else if (rpcs->op == RCF_RPC_WAIT)
        {
            rpcs->tid0 = 0;
            rpcs->is_done_ptr = 0;
            rpcs->op = RCF_RPC_CALL_WAIT;
        }
        else if (rpcs->op == RCF_RPC_IS_DONE)
        {
            rpcs->op = RCF_RPC_WAIT;
        }
    }

    pthread_mutex_unlock(&rpcs->lock);
}

/* See description in rcf_rpc.h */
te_errno
rcf_rpc_server_is_op_done(rcf_rpc_server *rpcs, te_bool *done)
{
    tarpc_rpc_is_op_done_in  in;
    tarpc_rpc_is_op_done_out out;

    if (rpcs == NULL || done == NULL)
        return TE_RC(TE_RCF, TE_EINVAL);

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.common.done = rpcs->is_done_ptr;

    rpcs->op = RCF_RPC_IS_DONE;
    rcf_rpc_call(rpcs, "rpc_is_op_done", &in, &out);
    
    if (rpcs->_errno != 0)
    {
        ERROR("Failed to call rpc_is_op_done() on the RPC server %s",
              rpcs->name);
        return rpcs->_errno;
    }

    *done = (out.common.done != 0);

    return 0;
}

/* See description in rcf_rpc.h */
te_errno
rcf_rpc_setlibname(rcf_rpc_server *rpcs, const char *libname)
{
    tarpc_setlibname_in  in;
    tarpc_setlibname_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    in.libname.libname_len = (libname == NULL) ? 0 : (strlen(libname) + 1);
    in.libname.libname_val = (char *)libname;

    rpcs->op = RCF_RPC_CALL_WAIT;
    rcf_rpc_call(rpcs, "setlibname", &in, &out);

    if (!RPC_IS_CALL_OK(rpcs))
        out.retval = -1;

    LOG_MSG(out.retval != 0 ? TE_LL_ERROR : TE_LL_RING,
            "RPC (%s,%s) setlibname(%s) -> %d (%s)",
            rpcs->ta, rpcs->name, libname ? : "(NULL)",
            out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    if (out.retval == 0)
    {
        free(rpcs->nv_lib);
        rpcs->nv_lib = (libname != NULL) ? strdup(libname) : NULL;
        assert((rpcs->nv_lib == NULL) == (libname == NULL));
    }

    return out.retval;
}

extern int rcf_send_recv_msg(rcf_msg *send_buf, size_t send_size,
                             rcf_msg *recv_buf, size_t *recv_size, 
                             rcf_msg **p_answer);

/**
 * Call SUN RPC on the TA.
 *
 * @param ta_name       Test Agent name                 
 * @param session       TA session or 0
 * @param rpcserver     Name of the RPC server
 * @param timeout       RPC timeout in milliseconds or 0 (unlimited)
 * @param rpc_name      Name of the RPC (e.g. "bind")
 * @param in            Input parameter C structure
 * @param in            Output parameter C structure
 *
 * @return Status code
 */
te_errno 
rcf_ta_call_rpc(const char *ta_name, int session, 
                const char *rpcserver, int timeout,
                const char *rpc_name, void *in, void *out)
{
/** Length of RPC data inside the message */     
#define INSIDE_LEN \
    (sizeof(((rcf_msg *)0)->file) + sizeof(((rcf_msg *)0)->value))
    
/** Length of message part before RPC data */
#define PREFIX_LEN  \
    (sizeof(rcf_msg) - INSIDE_LEN)

    char     msg_buf[PREFIX_LEN + RCF_RPC_BUF_LEN];
    rcf_msg *msg = (rcf_msg *)msg_buf;
    rcf_msg *ans = NULL;
    int      rc;
    size_t   anslen = sizeof(msg_buf);
    size_t   len;
    
    if (ta_name == NULL || strlen(ta_name) >= RCF_MAX_NAME || 
        rpcserver == NULL || rpc_name == NULL || 
        in == NULL || out == NULL)
    {
        return TE_RC(TE_RCF_API, TE_EINVAL);
    }
    
    if (strlen(rpc_name) >= RCF_RPC_MAX_NAME)
    {
        ERROR("Too long RPC name: %s - change RCF_RPC_MAX_NAME constant", 
              rpc_name);
        return TE_RC(TE_RCF_API, TE_EINVAL);
    }
    
    len = RCF_RPC_BUF_LEN;
    if ((rc = rpc_xdr_encode_call(rpc_name, msg->file, &len, in)) != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
        {
            ERROR("Unknown RPC %s", rpc_name);
            return rc;
        }
        
        /* Try to allocate more memory */
        len = RCF_RPC_HUGE_BUF_LEN;
        anslen = PREFIX_LEN + RCF_RPC_HUGE_BUF_LEN;
        if ((msg = (rcf_msg *)malloc(anslen)) == NULL)
            return TE_RC(TE_RCF_API, TE_ENOMEM);

        if ((rc = rpc_xdr_encode_call(rpc_name, msg->file, &len, in)) != 0)
        {
            ERROR("Encoding of RPC %s input parameters failed: error %r",
                  rpc_name, rc);
            free(msg);
            return rc;
        }
    }
    
    memset(msg, 0, PREFIX_LEN);
    msg->opcode = RCFOP_RPC;
    msg->sid = session;
    strcpy(msg->ta, ta_name);
    strcpy(msg->id, rpcserver);
    msg->timeout = timeout;
    msg->intparm = len;
    msg->data_len = len - INSIDE_LEN;

    rc = rcf_send_recv_msg(msg, PREFIX_LEN + len, msg, &anslen, &ans);
                           
    if (ans != NULL)
    {
        if ((char *)msg != msg_buf)
            free(msg);
            
        msg = ans;
    }
    
    if (rc != 0 || (rc = msg->error) != 0)
    {
       if ((char *)msg != msg_buf)
            free(msg);
        return rc;
    }
    
    rc = rpc_xdr_decode_result(rpc_name, msg->file, msg->intparm, out);
    
    if (rc != 0)
        ERROR("Decoding of RPC %s output parameters failed: error %r",
              rpc_name, rc);
        
    if ((char *)msg != msg_buf)
        free(msg);
        
    return rc;        
    
#undef INSIDE_LEN    
#undef PREFIX_LEN    
}

/**
 * Check is the RPC server has thread children.
 *
 * @param rpcs          RPC server
 *
 * @return TRUE if RPC server has children
 */
te_bool
rcf_rpc_server_has_children(rcf_rpc_server *rpcs)
{
    unsigned int num, i;
    cfg_handle  *servers;
    
    if (rpcs == NULL)
        return FALSE;
    
    if (cfg_find_pattern_fmt(&num, &servers, "/agent:%s/rpcserver:*",
                             rpcs->ta) != 0)
    {
        return FALSE;
    }
    
    for (i = 0; i < num; i++)
    {
        char *name;
        
        if (cfg_get_instance(servers[i], NULL, &name) != 0)
            continue;
            
        if (strcmp_start("thread_", name) == 0 &&
            strcmp(name + strlen("thread_"), rpcs->name) == 0)
        {
            free(name);
            free(servers);
            return TRUE;
        }
        free(name);
    }              
    
    free(servers);
    return FALSE;    
}

/**
 * Fork RPC server with non-default conditions.
 *
 * @param rpcs          existing RPC server handle
 * @param name          name of the new server
 * @param params        additional parameters for process creation
 * @param p_new         location for new RPC server handle
 *
 * @return Status code
 */
te_errno 
rcf_rpc_server_create_process(rcf_rpc_server *rpcs, 
                              const char *name,
                              rcf_rpc_cp_params *params,
                              rcf_rpc_server **p_new)
{
    tarpc_create_process_in  in;
    tarpc_create_process_out out;
    char                     lib = 0;
    te_errno                 rc;

    if (rpcs == NULL)
        return TE_RC(TE_RCF_API, TE_EINVAL);

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.common.op = RCF_RPC_CALL_WAIT;
    in.common.lib = &lib;
    in.name.name_len = strlen(name) + 1;
    in.name.name_val = strdup(name);
    in.inherit = params->inherit;
    in.net_init = params->net_init;
    
    if ((rc = rcf_ta_call_rpc(rpcs->ta, rpcs->sid, rpcs->name,
                              1000, "create_process", &in, &out)) != 0)
    {                              
        return rc;
    }
    
    free(in.name.name_val);
        
    if (out.pid < 0)
    {
        ERROR("RPC create_process() failed on the server %s with errno %r", 
              rpcs->name, out.common._errno);
        return (out.common._errno != 0) ?
                   out.common._errno : TE_RC(TE_RCF_API, TE_ECORRUPTED);
    }
    
    rc = rcf_rpc_server_get(rpcs->ta, name, "existing", 
                            FALSE, FALSE, TRUE, p_new);
    
    if (rc != 0)
    {
        ERROR("Failed to register created RPC server %s on TA: %r", rc, 
              name);
              
        if (rcf_ta_kill_task(rpcs->ta, 0, out.pid) != 0)
            ERROR("Failed to kill created RPC server");
    }
    
    return rc;
}                              
