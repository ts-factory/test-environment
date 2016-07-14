/** @file
 * @brief Unix Test Agent
 *
 * RPC routines implementation
 *
 * Copyright (C) 2004-2016 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Artem V. Andreev <Artem.Andreev@oktetlabs.ru>
 * @author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "RPC"

#include "rpc_server.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#if HAVE_NETINET_IN_SYSTM_H /* Required for FreeBSD build */
#include <netinet/in_systm.h>
#endif

#ifdef HAVE_SCSI_SG_H
#include <scsi/sg.h>
#elif defined HAVE_CAM_SCSI_SCSI_SG_H
#include <cam/scsi/scsi_sg.h>
#endif

#if HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_LINUX_ETHTOOL_H
#include "te_ethtool.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_LINUX_NET_TSTAMP_H
#include <linux/net_tstamp.h>
#endif

#include "te_defs.h"
#include "te_queue.h"
#include "te_tools.h"

#include "agentlib.h"

#ifndef MSG_MORE
#define MSG_MORE 0
#endif

#ifdef LIO_READ
void *dummy = aio_read;
#endif

/* FIXME: The following are defined inside Agent */
extern const char *ta_name;
extern const char *ta_execname;
extern char ta_dir[RCF_MAX_PATH];


/** User environment */
extern char **environ;

#define SOLARIS (defined(__sun) || defined(sun)) && \
    (defined(__SVR4) || defined(__svr4__))

extern sigset_t         rpcs_received_signals;
extern tarpc_siginfo_t  last_siginfo;

static te_bool   dynamic_library_set = FALSE;
static char      dynamic_library_name[RCF_MAX_PATH];
static void     *dynamic_library_handle = NULL;

/**
 * Set name of the dynamic library to be used to resolve function
 * called via RPC.
 *
 * @param libname       Full name of the dynamic library or NULL
 *
 * @return Status code.
 *
 * @note The dinamic library is opened with RTLD_NODELETE flag.
 * This flag is necessary for all libraries using atfork since there is no
 * way to undo the atfork call.  This flag is also necessary if the library
 * does not have correct _fini.  See man dlopen of other details.
 */
te_errno
tarpc_setlibname(const char *libname)
{
    extern int (*tce_notify_function)(void);
    extern int (*tce_get_peer_function)(void);
    extern const char *(*tce_get_conn_function)(void);

    void (*tce_initializer)(const char *, int) = NULL;

    if (libname == NULL)
        libname = "";

    if (dynamic_library_set)
    {
        char *old = getenv("TARPC_DL_NAME");

        if (old == NULL)
        {
            ERROR("Inconsistent state of dynamic library flag and "
                  "Environment");
            return TE_RC(TE_TA_UNIX, TE_EFAULT);
        }
        if (strcmp(libname, old) == 0)
        {
            /* It is OK, if we try to set the same library once more */
            return 0;
        }
        ERROR("Dynamic library has already been set to %s", old);
        return TE_RC(TE_TA_UNIX, TE_EEXIST);
    }
    dynamic_library_handle = dlopen(*libname == '\0' ? NULL : libname,
                                    RTLD_LAZY
#ifdef HAVE_RTLD_NODELETE
                                    | RTLD_NODELETE
#endif
                                    );
    if (dynamic_library_handle == NULL)
    {
        if (*libname == 0)
        {
            dynamic_library_set = TRUE;
            return 0;
        }
        ERROR("Cannot load shared library '%s': %s", libname, dlerror());
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    }
    if (setenv("TARPC_DL_NAME", libname, 1) != 0)
    {
        ERROR("No enough space in environment to save dynamic library "
              "'%s' name", libname);
        (void)dlclose(dynamic_library_handle);
        dynamic_library_handle = NULL;
        return TE_RC(TE_TA_UNIX, TE_ENOSPC);
    }
    dynamic_library_set = TRUE;
    strncpy(dynamic_library_name, libname, strlen(libname) + 1);
    RING("Dynamic library is set to '%s'", libname);

    if (tce_get_peer_function != NULL)
    {
        tce_initializer = dlsym(dynamic_library_handle,
                                "__bb_init_connection");
        if (tce_initializer != NULL)
        {
            const char *ptc = tce_get_conn_function();

            if (ptc == NULL)
                WARN("tce_init_connect() has not been called");
            else
            {
                if (tce_notify_function != NULL)
                    tce_notify_function();
                tce_initializer(ptc, tce_get_peer_function());
                RING("TCE initialized for dynamic library '%s'",
                     getenv("TARPC_DL_NAME"));
            }
        }
    }

    return 0;
}

/* See the description in tarpc_server.h */
te_bool
tarpc_dynamic_library_loaded(void)
{
    return dynamic_library_set && dynamic_library_handle != NULL;
}

/**
 * Find the function by its name.
 *
 * @param lib   library name or empty string
 * @param name  function name
 * @param func  location for function address
 *
 * @return status code
 */
int
tarpc_find_func(te_bool use_libc, const char *name, api_func *func)
{
    te_errno    rc;
    void       *handle;
    char       *tarpc_dl_name;

    *func = NULL;

    tarpc_dl_name = getenv("TARPC_DL_NAME");
    if (!dynamic_library_set && tarpc_dl_name != NULL &&
        (rc = tarpc_setlibname(tarpc_dl_name)) != 0)
    {
        /* Error is always logged from tarpc_setlibname() */
        return rc;
    }
#if defined (__QNX__)
    /*
     * QNX may set errno to ESRCH even after successful
     * call to 'getenv'.
     */
    errno = 0;
#endif

    if (use_libc || !tarpc_dynamic_library_loaded())
    {
        static void *libc_handle = NULL;
        static te_bool dlopen_null = FALSE;

        if (dlopen_null)
            goto try_ta_symtbl;

        if (libc_handle == NULL)
        {
            if ((libc_handle = dlopen(NULL, RTLD_LAZY)) == NULL)
            {
                dlopen_null = TRUE;
                goto try_ta_symtbl;
            }
        }
        handle = libc_handle;
        VERB("Call from libc");
    }
    else
    {
        /*
         * We get this branch of the code only if user set some
         * library to be used with tarpc_setlibname() function earlier,
         * and so we should use it to find symbol.
         */
        assert(dynamic_library_set == TRUE);
        assert(dynamic_library_handle != NULL);

        handle = dynamic_library_handle;
        VERB("Call from registered library");
    }

    *func = dlsym(handle, name);

try_ta_symtbl:
    if (*func == NULL)
    {
        if ((*func = rcf_ch_symbol_addr(name, 1)) == NULL)
        {
            ERROR("Cannot resolve symbol %s", name);
            return TE_RC(TE_TA_UNIX, TE_ENOENT);
        }
    }
    return 0;
}

/**
 * Find the pointer to function by its name in table.
 * Try to convert string to long int and cast it to the pointer
 * in the case if function is implemented as a static one. Use it
 * for signal handlers only.
 *
 * @param name  function name (or pointer value converted to string)
 * @param handler returned pointer to function or NULL if error
 *
 * @return errno if error or 0
 */
static te_errno
name2handler(const char *name, void **handler)
{
    if (name == NULL || *name == '\0')
    {
        *handler = NULL;
        return 0;
    }

    *handler = rcf_ch_symbol_addr(name, 1);
    if (*handler == NULL)
    {
        char *tmp;
        int   id;

        if (strcmp(name, "SIG_ERR") == 0)
            *handler = (void *)SIG_ERR;
        else if (strcmp(name, "SIG_DFL") == 0)
            *handler = (void *)SIG_DFL;
        else if (strcmp(name, "SIG_IGN") == 0)
            *handler = (void *)SIG_IGN;
        else if (strcmp(name, "NULL") == 0)
            *handler = NULL;
        else
        {
            id = strtol(name, &tmp, 10);
            if (tmp == name || *tmp != '\0')
                return TE_RC(TE_TA_UNIX, TE_ENOENT);

            *handler = rcf_pch_mem_get(id);
        }
    }
    return 0;
}

/**
 * Find the function name in table according to pointer to one.
 * Try to convert pointer value to string in the case if function
 * is implemented as a static one. Use it for signal handlers only.
 *
 * @param handler  pointer to function
 *
 * @return Allocated name or NULL in the case of memory allocation failure
 */
static char *
handler2name(void *handler)
{
    char *tmp;

    if (handler == (void *)SIG_ERR)
        tmp = strdup("SIG_ERR");
    else if (handler == (void *)SIG_DFL)
        tmp = strdup("SIG_DFL");
    else if (handler == (void *)SIG_IGN)
        tmp = strdup("SIG_IGN");
    else if (handler == NULL)
        tmp = strdup("NULL");
    else if ((tmp = rcf_ch_symbol_name(handler)) != NULL)
        tmp = strdup(tmp);
    else if ((tmp = calloc(1, 16)) != NULL)
    {
        /* FIXME */
        int id = rcf_pch_mem_get_id(handler);

        if (id == 0)
        {
            id = rcf_pch_mem_alloc(handler);
            RING("Unknown signal handler 0x%x is registered as "
                 "ID %d in RPC server memory", handler, id);
        }

        /* FIXME */
        sprintf(tmp, "%d", id);
    }

    if (tmp == NULL)
    {
        ERROR("Out of memory");
        /* FIXME */
        return strdup("");
    }

    return tmp;
}


/*-------------- setlibname() -----------------------------*/

bool_t
_setlibname_1_svc(tarpc_setlibname_in *in, tarpc_setlibname_out *out,
                 struct svc_req *rqstp)
{
    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));
    VERB("PID=%d TID=%llu: Entry %s",
         (int)getpid(), (unsigned long long int)pthread_self(),
         "setlibname");

    out->common._errno = tarpc_setlibname((in->libname.libname_len == 0) ?
                                          NULL : in->libname.libname_val);
    out->retval = (out->common._errno == 0) ? 0 : -1;
    out->common.duration = 0;

    return TRUE;
}

/*-------------- rpc_find_func() ----------------------*/

bool_t
_rpc_find_func_1_svc(tarpc_rpc_find_func_in  *in,
                     tarpc_rpc_find_func_out *out,
                     struct svc_req          *rqstp)
{
    api_func func;

    UNUSED(rqstp);

    memset(out, 0, sizeof(*out));

    out->find_result = tarpc_find_func(in->common.use_libc,
                                       in->func_name, &func);
    return TRUE;
}

/*-------------- rpc_is_alive() --------------------------------*/

bool_t
_rpc_is_alive_1_svc(tarpc_rpc_is_alive_in  *in,
                    tarpc_rpc_is_alive_out *out,
                    struct svc_req         *rqstp)
{
    UNUSED(rqstp);
    UNUSED(in);

    memset(out, 0, sizeof(*out));

    return TRUE;
}

/*-------------- sizeof() -------------------------------*/
#define MAX_TYPE_NAME_SIZE 30
typedef struct {
    char           type_name[MAX_TYPE_NAME_SIZE];
    tarpc_ssize_t  type_size;
} type_info_t;

static type_info_t type_info[] =
{
    {"te_bool", sizeof(te_bool)},
    {"char", sizeof(char)},
    {"short", sizeof(short)},
    {"int", sizeof(int)},
    {"long", sizeof(long)},
    {"long long", sizeof(long long)},
    {"te_errno", sizeof(te_errno)},
    {"size_t", sizeof(size_t)},
    {"socklen_t", sizeof(socklen_t)},
    {"struct timeval", sizeof(struct timeval)},
   {"struct linger", sizeof(struct linger)},
    {"struct in_addr", sizeof(struct in_addr)},
    {"struct ip_mreq", sizeof(struct ip_mreq)},
    {"struct tcp_info", sizeof(struct tcp_info)},
    {"struct ip_mreq_source", sizeof(struct ip_mreq_source)},
#if HAVE_STRUCT_IP_MREQN
    {"struct ip_mreqn", sizeof(struct ip_mreqn)},
#endif
    {"struct sockaddr", sizeof(struct sockaddr)},
    {"struct sockaddr_in", sizeof(struct sockaddr_in)},
    {"struct sockaddr_in6", sizeof(struct sockaddr_in6)},
    {"struct sockaddr_storage", sizeof(struct sockaddr_storage)},
};

/*-------------- get_sizeof() ---------------------------------*/
bool_t
_get_sizeof_1_svc(tarpc_get_sizeof_in *in, tarpc_get_sizeof_out *out,
                  struct svc_req *rqstp)
{
    uint32_t i;

    UNUSED(rqstp);

    out->size = -1;

    if (in->typename == NULL)
    {
        ERROR("Name of type not specified");
        return FALSE;
    }

    if (in->typename[0] == '*')
    {
        out->size = sizeof(void *);
        return TRUE;
    }

    for (i = 0; i < sizeof(type_info) / sizeof(type_info_t); i++)
    {
        if (strcmp(in->typename, type_info[i].type_name) == 0)
        {
            out->size = type_info[i].type_size;
            return TRUE;
        }
    }

    ERROR("Unknown type (%s)", in->typename);
#if 0
    out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
#endif
    return TRUE;
}

/*-------------- get_addrof() ---------------------------------*/
bool_t
_get_addrof_1_svc(tarpc_get_addrof_in *in, tarpc_get_addrof_out *out,
                  struct svc_req *rqstp)
{
    void *addr = rcf_ch_symbol_addr(in->name, 0);

    UNUSED(rqstp);

    out->addr = addr == NULL ? 0 : rcf_pch_mem_alloc(addr);

    return TRUE;
}

/*-------------- get_var() ---------------------------------*/
bool_t
_get_var_1_svc(tarpc_get_var_in *in, tarpc_get_var_out *out,
                   struct svc_req *rqstp)
{
    void *addr = rcf_ch_symbol_addr(in->name, 0);

    UNUSED(rqstp);

    if (addr == NULL)
    {
        ERROR("Variable %s is not found", in->name);
        out->found = FALSE;
        return TRUE;
    }

    out->found = TRUE;

    switch (in->size)
    {
        case 1: out->val = *(uint8_t *)addr; break;
        case 2: out->val = *(uint16_t *)addr; break;
        case 4: out->val = *(uint32_t *)addr; break;
        case 8: out->val = *(uint64_t *)addr; break;
        default: return FALSE;
    }

    return TRUE;
}

/*-------------- set_var() ---------------------------------*/
bool_t
_set_var_1_svc(tarpc_set_var_in *in, tarpc_set_var_out *out,
               struct svc_req *rqstp)
{
    void *addr = rcf_ch_symbol_addr(in->name, 0);

    UNUSED(rqstp);
    UNUSED(out);

    if (addr == NULL)
    {
        ERROR("Variable %s is not found", in->name);
        out->found = FALSE;
        return TRUE;
    }

    out->found = TRUE;

    switch (in->size)
    {
        case 1: *(uint8_t *)addr  = in->val; break;
        case 2: *(uint16_t *)addr = in->val; break;
        case 4: *(uint32_t *)addr = in->val; break;
        case 8: *(uint64_t *)addr = in->val; break;
        default: return FALSE;
    }

    return TRUE;
}

/*-------------- create_process() ---------------------------------*/
void
ta_rpc_execve(const char *name)
{
    const char   *argv[5];
    api_func_ptr  func;

    int rc;

    memset(argv, 0, sizeof(argv));
    argv[0] = ta_execname;
    argv[1] = "exec";
    argv[2] = "rcf_pch_rpc_server_argv";
    argv[3] = name;

    VERB("execve() args: %s, %s, %s, %s",
         argv[0], argv[1], argv[2], argv[3]);
    /* Call execve() */
    rc = tarpc_find_func(FALSE, "execve", (api_func *)&func);
    if (rc != 0)
    {
        rc = errno;
        LOG_PRINT("No execve function: errno=%d", rc);
        exit(1);
    }

    rc = func((void *)ta_execname, argv, environ);
    if (rc != 0)
    {
        rc = errno;
        LOG_PRINT("execve() failed: errno=%d", rc);
    }

}

bool_t
_create_process_1_svc(tarpc_create_process_in *in,
                      tarpc_create_process_out *out,
                      struct svc_req *rqstp)
{
    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    out->pid = fork();

    if (out->pid == -1)
    {
        out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
        return TRUE;
    }
    if (out->pid == 0)
    {
        /*
         * Change the process group to allow killing all the children
         * together with this RPC server and to disallow killing of this
         * process when its parent RPC server is killed
         */
        setpgid(getpid(), getpid());

        if (in->flags & RCF_RPC_SERVER_GET_EXEC)
            ta_rpc_execve(in->name.name_val);
        rcf_pch_rpc_server(in->name.name_val);
        exit(EXIT_FAILURE);
    }

    return TRUE;
}

/*-------------- vfork() -------------------------------------*/
bool_t
_vfork_1_svc(tarpc_vfork_in *in,
             tarpc_vfork_out *out,
             struct svc_req *rqstp)
{
    struct timeval  t_start;
    struct timeval  t_finish;
    api_func_void   func;
    int             rc;

    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    rc = tarpc_find_func(in->common.use_libc, "vfork", (api_func *)&func);
    if (rc != 0)
    {
        rc = errno;
        ERROR("No vfork() function: errno=%d", rc);
        out->common._errno = TE_OS_RC(TE_TA_UNIX, rc);
        return TRUE;
    }

    run_vfork_hooks(VFORK_HOOK_PHASE_PREPARE);
    gettimeofday(&t_start, NULL);
    out->pid = func();
    gettimeofday(&t_finish, NULL);
    out->elapsed_time = (t_finish.tv_sec - t_start.tv_sec) * 1000 +
                        (t_finish.tv_usec - t_start.tv_usec) / 1000;

    if (out->pid == -1)
    {
        out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
        run_vfork_hooks(VFORK_HOOK_PHASE_PARENT);
        return TRUE;
    }

    if (out->pid == 0)
    {
        /*
         * Change the process group to allow killing all the children
         * together with this RPC server and to disallow killing of this
         * process when its parent RPC server is killed
         */
        setpgid(getpid(), getpid());

        run_vfork_hooks(VFORK_HOOK_PHASE_CHILD);
        rcf_pch_rpc_server(in->name.name_val);
        exit(EXIT_FAILURE);
    }
    else
    {
        usleep(in->time_to_wait * 1000);
        run_vfork_hooks(VFORK_HOOK_PHASE_PARENT);
    }

    return TRUE;
}

/*-------------- thread_create() -----------------------------*/
bool_t
_thread_create_1_svc(tarpc_thread_create_in *in,
                     tarpc_thread_create_out *out,
                     struct svc_req *rqstp)
{
    pthread_t tid;

    UNUSED(rqstp);

    TE_COMPILE_TIME_ASSERT(sizeof(pthread_t) <= sizeof(tarpc_pthread_t));

    memset(out, 0, sizeof(*out));

    out->retval = pthread_create(&tid, NULL, (void *)rcf_pch_rpc_server,
                                 strdup(in->name.name_val));

    if (out->retval == 0)
        out->tid = (tarpc_pthread_t)tid;

    return TRUE;
}

/*-------------- thread_cancel() -----------------------------*/
bool_t
_thread_cancel_1_svc(tarpc_thread_cancel_in *in,
                     tarpc_thread_cancel_out *out,
                     struct svc_req *rqstp)
{
    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    out->retval = pthread_cancel((pthread_t)in->tid);

    return TRUE;
}

/*-------------- thread_join() -----------------------------*/
bool_t
_thread_join_1_svc(tarpc_thread_join_in *in,
                   tarpc_thread_join_out *out,
                   struct svc_req *rqstp)
{
    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    out->retval = pthread_join((pthread_t)in->tid, NULL);
    return TRUE;
}

/**
 * Check, if some signals were received by the RPC server (as a process)
 * and return the mask of received signals.
 */

bool_t
_sigreceived_1_svc(tarpc_sigreceived_in *in, tarpc_sigreceived_out *out,
                   struct svc_req *rqstp)
{
    static rpc_ptr ptr = 0;

    UNUSED(in);
    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    if (ptr == 0)
        ptr = rcf_pch_mem_alloc(&rpcs_received_signals);
    out->set = ptr;

    return TRUE;
}

/**
 * Get siginfo_t structure for the lastly received signal.
 */
bool_t
_siginfo_received_1_svc(tarpc_siginfo_received_in *in,
                        tarpc_siginfo_received_out *out,
                        struct svc_req *rqstp)
{
    UNUSED(in);
    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    memcpy(&out->siginfo, &last_siginfo, sizeof(last_siginfo));

    return TRUE;
}

/*-------------- execve() ---------------------------------*/
TARPC_FUNC_STANDALONE(execve, {},
{
    /* Wait until main thread sends answer to non-blocking RPC call */
    sleep(1);

    MAKE_CALL(ta_rpc_execve(in->name));
}
)

/*-------------- execve_gen() ---------------------------------*/

/**
 * Convert iovec array to NULL terminated array
 *
 * @param list_ptr  List of arguments for INIT_CHECKED_ARG macros
 * @param iov       iovec array
 * @param arr       Location for NULL terminated array
 */
static void
unistd_iov_to_arr_null(checked_arg_list *arglist, struct tarpc_iovec *iov,
                       size_t len, char *arr[])
{
    size_t i;

    if (len == 0)
        return;

    for (i = 0; i < len; i++)
    {
        INIT_CHECKED_ARG(iov[i].iov_base.iov_base_val,
                         iov[i].iov_base.iov_base_len,
                         iov[i].iov_base.iov_base_len);
        arr[i] = (char *)iov[i].iov_base.iov_base_val;
    }
}

int
execve_gen(const char *filename, char *const argv[], char *const envp[])
{
    api_func_ptr func_execve;

    if (tarpc_find_func(FALSE, "execve",
                        (api_func *)&func_execve) != 0)
    {
        ERROR("Failed to find function execve()");
        return -1;
    }

    return func_execve((void *)filename, argv, envp);
}

TARPC_FUNC(execve_gen, {},
{
    char *argv[in->argv.argv_len];
    char *envp[in->envp.envp_len];

    unistd_iov_to_arr_null(arglist, in->argv.argv_val,
                           in->argv.argv_len, argv);
    unistd_iov_to_arr_null(arglist, in->envp.envp_val,
                           in->envp.envp_len, envp);

    /* Wait until main thread sends answer to non-blocking RPC call */
    sleep(1);

    MAKE_CALL(func_ptr((void *)in->filename,
                       in->argv.argv_len == 0 ? NULL : argv,
                       in->envp.envp_len == 0 ? NULL : envp));
}
)


/*-------------- exit() --------------------------------*/
TARPC_FUNC(exit, {}, { MAKE_CALL(func(in->status)); })

/*-------------- getpid() --------------------------------*/
TARPC_FUNC(getpid, {}, { MAKE_CALL(out->retval = func_void()); })

/*-------------- pthread_self() --------------------------*/
TARPC_FUNC(pthread_self, {},
{
    MAKE_CALL(out->retval = (tarpc_pthread_t)func());
}
)

/*-------------- access() --------------------------------*/
TARPC_FUNC(access, {},
{
    MAKE_CALL(out->retval = func_ptr(in->path.path_val,
        access_mode_flags_rpc2h(in->mode)));
})


/*-------------- gettimeofday() --------------------------------*/
TARPC_FUNC(gettimeofday,
{
    COPY_ARG_NOTNULL(tv);
    COPY_ARG(tz);
},
{
    struct timeval  tv;
    struct timezone tz;

    TARPC_CHECK_RC(timeval_rpc2h(out->tv.tv_val, &tv));
    if (out->tz.tz_len != 0)
        TARPC_CHECK_RC(timezone_rpc2h(out->tz.tz_val, &tz));

    if (out->common._errno != 0)
    {
        out->retval = -1;
    }
    else
    {
        MAKE_CALL(out->retval = func_ptr(&tv,
                                         out->tz.tz_len == 0 ? NULL : &tz));

        TARPC_CHECK_RC(timeval_h2rpc(&tv, out->tv.tv_val));
        if (out->tz.tz_len != 0)
            TARPC_CHECK_RC(timezone_h2rpc(&tz, out->tz.tz_val));
        if (TE_RC_GET_ERROR(out->common._errno) == TE_EH2RPC)
            out->retval = -1;
    }
}
)

/*-------------- gethostname() --------------------------------*/
TARPC_FUNC(gethostname,
{
    COPY_ARG(name);
},
{
    MAKE_CALL(out->retval = func_ptr(out->name.name_val, in->len));
}
)

#if defined(ENABLE_TELEPHONY)
/*-------------- telephony_check_dial_tone() -----------------------*/

TARPC_FUNC(telephony_open_channel, {},
{
    MAKE_CALL(out->retval = func(in->port));
}
)

/*-------------- telephony_check_dial_tone() -----------------------*/

TARPC_FUNC(telephony_close_channel, {},
{
    MAKE_CALL(out->retval = func(in->chan));
}
)

/*-------------- telephony_pickup() -----------------------*/

TARPC_FUNC(telephony_pickup, {},
{
    MAKE_CALL(out->retval = func(in->chan));
}
)

/*-------------- telephony_hangup() -----------------------*/

TARPC_FUNC(telephony_hangup, {},
{
    MAKE_CALL(out->retval = func(in->chan));
}
)

/*-------------- telephony_check_dial_tone() -----------------------*/

TARPC_FUNC(telephony_check_dial_tone, {},
{
    MAKE_CALL(out->retval = func(in->chan, in->plan));
}
)

/*-------------- telephony_dial_number() -----------------------*/

TARPC_FUNC(telephony_dial_number, {},
{
    MAKE_CALL(out->retval = func(in->chan, in->number));
}
)

/*-------------- telephony_dial_number() -----------------------*/

TARPC_FUNC(telephony_call_wait, {},
{
    MAKE_CALL(out->retval = func(in->chan, in->timeout));
}
)
#endif /* ENABLE_TELEPHONY */


/*-------------- socket() ------------------------------*/

TARPC_FUNC(socket, {},
{
    MAKE_CALL(out->fd = func(domain_rpc2h(in->domain),
                             socktype_rpc2h(in->type),
                             proto_rpc2h(in->proto)));
}
)

/*-------------- dup() --------------------------------*/

TARPC_FUNC(dup, {}, { MAKE_CALL(out->fd = func(in->oldfd)); })

/*-------------- dup2() -------------------------------*/

TARPC_FUNC(dup2, {}, { MAKE_CALL(out->fd = func(in->oldfd, in->newfd)); })

/*-------------- dup3() -------------------------------*/

TARPC_FUNC(dup3, {},
{
    MAKE_CALL(out->fd = func(in->oldfd, in->newfd, in->flags));
}
)

/*-------------- close() ------------------------------*/

TARPC_FUNC(close, {}, { MAKE_CALL(out->retval = func(in->fd)); })

/*-------------- closesocket() ------------------------------*/

int
closesocket(tarpc_closesocket_in *in)
{
    api_func close_func;

    if (tarpc_find_func(in->common.use_libc, "close", &close_func) != 0)
    {
        ERROR("Failed to find function \"close\"");
        return -1;
    }
    return close_func(in->s);
}

TARPC_FUNC(closesocket, {}, { MAKE_CALL(out->retval = func_ptr(in)); })

/*-------------- bind() ------------------------------*/

TARPC_FUNC(bind, {},
{
    if (in->addr.flags & TARPC_SA_RAW &&
        in->addr.raw.raw_len > sizeof(struct sockaddr_storage))
    {
        MAKE_CALL(out->retval =
                  func(in->fd,
                       (const struct sockaddr *)(in->addr.raw.raw_val),
                       in->addr.raw.raw_len));
    }
    else
    {
        PREPARE_ADDR(my_addr, in->addr, 0);
        MAKE_CALL(out->retval = func(in->fd, my_addr,
                                     in->fwd_len ? in->len :
                                                   my_addrlen));
    }
}
)

/*------------- rpc_check_port_is_free() ----------------*/

/* Simple socket() and bind() are used instead of tarpc_find_func() to
 * resolve them from the current library.  It is done by purpose: all that
 * things happen at early stage of test, and we do not want to affect the
 * library under test. */
te_bool
check_port_is_free(uint16_t port)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    int rc;

    struct sockaddr_in addr;

    if (fd < 0)
    {
        ERROR("Failed to create TCP socket");
        return FALSE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    rc = bind(fd, SA(&addr), sizeof(addr));
    if (rc != 0)
    {
        close(fd);
        return FALSE;
    }

    close(fd);
    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        ERROR("Failed to create UDP socket");
        return FALSE;
    }

    rc = bind(fd, SA(&addr), sizeof(addr));
    if (rc != 0)
    {
        close(fd);
        return FALSE;
    }
    close(fd);

    return TRUE;
}

TARPC_FUNC(check_port_is_free, {},
{
    MAKE_CALL(out->retval = func(in->port));
}
)

/*-------------- connect() ------------------------------*/

TARPC_FUNC(connect, {},
{
    if (in->addr.flags & TARPC_SA_RAW &&
        in->addr.raw.raw_len > sizeof(struct sockaddr_storage))
    {
        MAKE_CALL(out->retval =
                  func(in->fd,
                       (const struct sockaddr *)(in->addr.raw.raw_val),
                       in->addr.raw.raw_len));
    }
    else
    {
        PREPARE_ADDR(serv_addr, in->addr, 0);
        MAKE_CALL(out->retval = func(in->fd, serv_addr, serv_addrlen));
    }
}
)

/*-------------- listen() ------------------------------*/

TARPC_FUNC(listen, {},
{
    MAKE_CALL(out->retval = func(in->fd, in->backlog));
}
)

/*-------------- accept() ------------------------------*/

TARPC_FUNC(accept,
{
    COPY_ARG(len);
    COPY_ARG_ADDR(addr);
},
{
    PREPARE_ADDR(addr, out->addr,
                 out->len.len_len == 0 ? 0 : *out->len.len_val);

    MAKE_CALL(out->retval = func(in->fd, addr,
                                 out->len.len_len == 0 ? NULL :
                                 out->len.len_val));

    sockaddr_output_h2rpc(addr, addrlen,
                          out->len.len_len == 0 ? 0 :
                              *(out->len.len_val),
                          &(out->addr));
}
)

/*-------------- accept4() ------------------------------*/

TARPC_FUNC(accept4,
{
    COPY_ARG(len);
    COPY_ARG_ADDR(addr);
},
{
    PREPARE_ADDR(addr, out->addr,
                 out->len.len_len == 0 ? 0 : *out->len.len_val);

    MAKE_CALL(out->retval = func(in->fd, addr,
                                 out->len.len_len == 0 ? NULL :
                                 out->len.len_val, in->flags));

    sockaddr_output_h2rpc(addr, addrlen,
                          out->len.len_len == 0 ? 0 :
                              *(out->len.len_val),
                          &(out->addr));
}
)

/*-------------- socket_connect_close() -----------------------*/
int
socket_connect_close(const struct sockaddr *addr,
                     socklen_t addrlen, uint32_t time2run)
{
    int     s;
    int     rc;
    time_t  start;
    time_t  now;

    api_func    socket_func;
    api_func    connect_func;
    api_func    close_func;

    if (tarpc_find_func(FALSE, "socket", &socket_func) != 0)
        return -1;
    if (tarpc_find_func(FALSE, "connect", &connect_func) != 0)
        return -1;
    if (tarpc_find_func(FALSE, "close", &close_func) != 0)
        return -1;

    start = now = time(NULL);
    while ((unsigned int)(now - start) <= time2run)
    {
        now = time(NULL);
        s = socket_func(AF_INET, SOCK_STREAM, 0);
        rc = connect_func(s, addr, addrlen);
        if( rc != 0  && errno != ECONNREFUSED && errno != ECONNABORTED )
            return -1;
        close_func(s);
    }
    return 0;
}

TARPC_FUNC(socket_connect_close, {},
{
    PREPARE_ADDR(serv_addr, in->addr, 0);
    MAKE_CALL(out->retval = func_ptr(serv_addr, serv_addrlen,
                                     in->time2run));
}
)

/*-------------- socket_listen_close() -----------------------*/
int
socket_listen_close(const struct sockaddr *addr,
                    socklen_t addrlen, uint32_t time2run)
{
    int     s;
    int     rc;
    time_t  start;
    time_t  now;

    api_func    socket_func;
    api_func    bind_func;
    api_func    listen_func;
    api_func    close_func;

    if (tarpc_find_func(FALSE, "socket", &socket_func) != 0)
        return -1;
    if (tarpc_find_func(FALSE, "bind", &bind_func) != 0)
        return -1;
    if (tarpc_find_func(FALSE, "listen", &listen_func) != 0)
        return -1;
    if (tarpc_find_func(FALSE, "close", &close_func) != 0)
        return -1;

    start = now = time(NULL);
    while ((unsigned int)(now - start) <= time2run)
    {
        now = time(NULL);
        s = socket_func(AF_INET, SOCK_STREAM, 0);
        rc = bind_func(s, addr, addrlen);
        if( rc != 0 )
        {
            ERROR("%s(): bind() function failed", __FUNCTION__);
            return -1;
        }
        rc = listen_func(s, 1);
        if( rc != 0 )
        {
            ERROR("%s(): listen() function failed", __FUNCTION__);
            return -1;
        }
        close_func(s);
    }
    return 0;
}

TARPC_FUNC(socket_listen_close, {},
{
    PREPARE_ADDR(serv_addr, in->addr, 0);
    MAKE_CALL(out->retval = func_ptr(serv_addr, serv_addrlen,
                                     in->time2run));
}
)

/*-------------- recvfrom() ------------------------------*/


TARPC_FUNC(recvfrom,
{
    COPY_ARG(buf);
    COPY_ARG(fromlen);
    COPY_ARG_ADDR(from);
},
{
    te_bool          free_name = FALSE;
    struct sockaddr *addr_ptr;
    socklen_t        addr_len;

    PREPARE_ADDR(from, out->from, out->fromlen.fromlen_len == 0 ? 0 :
                                        *out->fromlen.fromlen_val);
    if (out->from.raw.raw_len > sizeof(struct sockaddr_storage))
    {
        /*
         * Do not just assign - sockaddr_output_h2rpc()
         * converts RAW address only if it was changed by the
         * function.
         */
        addr_len = out->from.raw.raw_len;
        if (addr_len > 0 &&
            out->from.raw.raw_val != NULL)
        {
            addr_ptr = calloc(1, addr_len);
            if (addr_ptr == NULL)
            {
                ERROR("%s(): Failed to allocate memory for an address",
                      __FUNCTION__);
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                goto finish;
            }
            free_name = TRUE;
            memcpy(addr_ptr, out->from.raw.raw_val, addr_len);
        }
        else
            addr_ptr = (struct sockaddr *)out->from.raw.raw_val;
    }
    else
    {
        addr_ptr = from;
        addr_len = fromlen;
    }

    INIT_CHECKED_ARG(out->buf.buf_val, out->buf.buf_len, in->len);

    MAKE_CALL(out->retval = func(in->fd, out->buf.buf_val, in->len,
                                 send_recv_flags_rpc2h(in->flags), addr_ptr,
                                 out->fromlen.fromlen_len == 0 ? NULL :
                                    out->fromlen.fromlen_val));

    sockaddr_output_h2rpc(addr_ptr, addr_len,
                          out->fromlen.fromlen_len == 0 ? 0 :
                              *(out->fromlen.fromlen_val),
                          &(out->from));

finish:
    if (free_name)
        free(addr_ptr);
}
)



/*-------------- recv() ------------------------------*/

TARPC_FUNC(recv,
{
    COPY_ARG(buf);
},
{
    INIT_CHECKED_ARG(out->buf.buf_val, out->buf.buf_len, in->len);

    MAKE_CALL(out->retval = func(in->fd, out->buf.buf_val, in->len,
                                 send_recv_flags_rpc2h(in->flags)));
}
)

/*-------------- shutdown() ------------------------------*/

TARPC_FUNC(shutdown, {},
{
    MAKE_CALL(out->retval = func(in->fd, shut_how_rpc2h(in->how)));
}
)

/*--------------- fstat() -------------------------------*/



#define FSTAT_COPY(tobuf, outbuf) \
    tobuf->st_dev = outbuf.st_dev;             \
    tobuf->st_ino = outbuf.st_ino;             \
    tobuf->st_mode = outbuf.st_mode;           \
    tobuf->st_nlink = outbuf.st_nlink;         \
    tobuf->st_uid = outbuf.st_uid;             \
    tobuf->st_gid = outbuf.st_gid;             \
    tobuf->st_rdev = outbuf.st_rdev;           \
    tobuf->st_size = outbuf.st_size;           \
    tobuf->st_blksize = outbuf.st_blksize;     \
    tobuf->st_blocks = outbuf.st_blocks;       \
    tobuf->ifsock = S_ISSOCK(outbuf.st_mode);  \
    tobuf->iflnk = S_ISLNK(outbuf.st_mode);    \
    tobuf->ifreg = S_ISREG(outbuf.st_mode);    \
    tobuf->ifblk = S_ISBLK(outbuf.st_mode);    \
    tobuf->ifdir = S_ISDIR(outbuf.st_mode);    \
    tobuf->ifchr = S_ISCHR(outbuf.st_mode);    \
    tobuf->ififo = S_ISFIFO(outbuf.st_mode);   \
    tobuf->te_atime = outbuf.st_atime;         \
    tobuf->te_ctime = outbuf.st_ctime;         \
    tobuf->te_mtime = outbuf.st_mtime

int
te_fstat(te_bool use_libc, int fd, rpc_stat *rpcbuf)
{
#if defined (__QNX__) || defined (__ANDROID__)
    api_func    stat_func;
    int         rc;
    struct stat buf;

    memset(&buf, 0, sizeof(buf));

    if ((rc = fstat(fd, &buf)) < 0)
        return rc;

    rpcbuf->te_atime = buf.st_atime;
    rpcbuf->te_ctime = buf.st_ctime;
    rpcbuf->te_mtime = buf.st_mtime;

    return 0;
#elif defined __linux__
    api_func    stat_func;
    int         rc;
    struct stat buf;

    memset(&buf, 0, sizeof(buf));
    if (tarpc_find_func(use_libc, "__fxstat", &stat_func) != 0)
    {
        ERROR("Failed to find __fxstat function");
        return -1;
    }

    rc = stat_func(_STAT_VER, fd, &buf);
    if (rc < 0)
        return rc;

    FSTAT_COPY(rpcbuf, buf);
    return 0;
#else
    UNUSED(use_libc);
    UNUSED(rpcbuf);

/*
 * #error "fstat family is not currently supported for non-linux unixes."
*/
    errno = EOPNOTSUPP;
    return -1;
#endif
}

int
te_fstat64(te_bool use_libc, int fd, rpc_stat *rpcbuf)
{
/**
 * To have __USE_LARGEFILE64 defined in Linux, specify
 * -D_GNU_SOURCE (or other related feature test macro) in
 * TE_PLATFORM macro in your builder.conf
 */
#if defined __linux__ && defined __USE_LARGEFILE64
    api_func      stat_func;
    int           rc;
    struct stat64 buf;

    memset(&buf, 0, sizeof(buf));
    if (tarpc_find_func(use_libc, "__fxstat64", &stat_func) != 0)
    {
        ERROR("Failed to find __fxstat64 function");
        return -1;
    }

    rc = stat_func(_STAT_VER, fd, &buf);
    if (rc < 0)
        return rc;

    FSTAT_COPY(rpcbuf, buf);
    return 0;
#else
/*
 * #error "fstat family is not currently supported for non-linux unixes."
 */
    UNUSED(use_libc);
    UNUSED(fd);
    UNUSED(rpcbuf);

    ERROR("fstat64 is not supported");
    return -1;
#endif
}

TARPC_FUNC(te_fstat, {},
{
    MAKE_CALL(out->retval = func(in->common.use_libc, in->fd, &out->buf));
}
)

TARPC_FUNC(te_fstat64, {},
{
    MAKE_CALL(out->retval = func(in->common.use_libc, in->fd, &out->buf));
}
)

#undef FSTAT_COPY

#ifndef TE_POSIX_FS_PROVIDED
/*-------------- link() --------------------------------*/
TARPC_FUNC(link, {},
{
    TARPC_ENSURE_NOT_NULL(path1);
    TARPC_ENSURE_NOT_NULL(path2);
    MAKE_CALL(out->retval = func_ptr(in->path1.path1_val,
                                     in->path2.path2_val));
}
)

/*-------------- symlink() --------------------------------*/
TARPC_FUNC(symlink, {},
{
    TARPC_ENSURE_NOT_NULL(path1);
    TARPC_ENSURE_NOT_NULL(path2);
    MAKE_CALL(out->retval = func_ptr(in->path1.path1_val,
                                     in->path2.path2_val));
}
)

/*-------------- unlink() --------------------------------*/
TARPC_FUNC(unlink, {},
{
    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->retval = func_ptr(in->path.path_val));
}
)

/*-------------- rename() --------------------------------*/
TARPC_FUNC(rename, {},
{
    TARPC_ENSURE_NOT_NULL(path_old);
    TARPC_ENSURE_NOT_NULL(path_new);
    MAKE_CALL(out->retval = func_ptr(in->path_old.path_old_val,
                                     in->path_new.path_new_val));
}
)

/*-------------- mkdir() --------------------------------*/
TARPC_FUNC(mkdir, {},
{
    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->retval = func_ptr(in->path.path_val,
                                     file_mode_flags_rpc2h(in->mode)));
}
)

/*-------------- rmdir() --------------------------------*/
TARPC_FUNC(rmdir, {},
{
    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->retval = func_ptr(in->path.path_val));
}
)

#ifdef HAVE_SYS_STATVFS_H
/*-------------- fstatvfs()-----------------------------*/
TARPC_FUNC(fstatvfs, {},
{
    struct statvfs stat;

    MAKE_CALL(out->retval = func(in->fd, &stat));

    out->buf.f_bsize = stat.f_bsize;
    out->buf.f_blocks = stat.f_blocks;
    out->buf.f_bfree = stat.f_bfree;
}
)

/*-------------- statvfs()-----------------------------*/
TARPC_FUNC(statvfs, {},
{
    struct statvfs stat;

    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->retval = func_ptr(in->path.path_val, &stat));

    out->buf.f_bsize = stat.f_bsize;
    out->buf.f_blocks = stat.f_blocks;
    out->buf.f_bfree = stat.f_bfree;
}
)
#endif /* HAVE_SYS_STATVFS_H */

#ifdef HAVE_DIRENT_H
/* struct_dirent_props */
unsigned int
struct_dirent_props(void)
{
    unsigned int props = 0;

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    props |= RPC_DIRENT_HAVE_D_TYPE;
#endif
#if defined HAVE_STRUCT_DIRENT_D_OFF || defined HAVE_STRUCT_DIRENT_D_OFFSET
    props |= RPC_DIRENT_HAVE_D_OFF;
#endif
#ifdef HAVE_STRUCT_DIRENT_D_NAMELEN
    props |= RPC_DIRENT_HAVE_D_NAMLEN;
#endif
    props |= RPC_DIRENT_HAVE_D_INO;
    return props;
}

TARPC_FUNC(struct_dirent_props, {},
{
    MAKE_CALL(out->retval = func_void());
}
)
#endif /* HAVE_DIRENT_H */

/*-------------- opendir() --------------------------------*/
TARPC_FUNC(opendir, {},
{
    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->mem_ptr =
        rcf_pch_mem_alloc(func_ptr_ret_ptr(in->path.path_val)));
}
)

/*-------------- closedir() --------------------------------*/
TARPC_FUNC(closedir, {},
{
    MAKE_CALL(out->retval = func_ptr(rcf_pch_mem_get(in->mem_ptr)));
    rcf_pch_mem_free(in->mem_ptr);
}
)

/*-------------- readdir() --------------------------------*/
TARPC_FUNC(readdir, {},
{
    struct dirent *dent;

    MAKE_CALL(dent = (struct dirent *)
                        func_ptr(rcf_pch_mem_get(in->mem_ptr)));
    if (dent == NULL)
    {
        out->ret_null = TRUE;
    }
    else
    {
        tarpc_dirent *rpc_dent;

        rpc_dent = (tarpc_dirent *)calloc(1, sizeof(*rpc_dent));
        if (rpc_dent == NULL)
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        else
        {
            out->dent.dent_len = 1;

            out->ret_null = FALSE;
            out->dent.dent_val = rpc_dent;

            rpc_dent->d_name.d_name_val = strdup(dent->d_name);
            rpc_dent->d_name.d_name_len = strlen(dent->d_name) + 1;
            rpc_dent->d_ino = dent->d_ino;
#ifdef HAVE_STRUCT_DIRENT_D_OFF
            rpc_dent->d_off = dent->d_off;
#elif defined HAVE_STRUCT_DIRENT_D_OFFSET
            rpc_dent->d_off = dent->d_offset;
#else
            rpc_dent->d_off = 0;
#endif

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
            rpc_dent->d_type = d_type_h2rpc(dent->d_type);
#else
            rpc_dent->d_type = RPC_DT_UNKNOWN;
#endif
#ifdef HAVE_STRUCT_DIRENT_D_NAMELEN
            rpc_dent->d_namelen = dent->d_namelen;
#else
            rpc_dent->d_namelen = 0;
#endif
            rpc_dent->d_props = struct_dirent_props();
        }
    }
}
)
#endif

/*-------------- sendto() ------------------------------*/

TARPC_FUNC(sendto, {},
{
    PREPARE_ADDR(to, in->to, 0);

    INIT_CHECKED_ARG(in->buf.buf_val, in->buf.buf_len, 0);

    if (!(in->to.flags & TARPC_SA_RAW &&
          in->to.raw.raw_len > sizeof(struct sockaddr_storage)))
    {
        MAKE_CALL(out->retval = func(in->fd, in->buf.buf_val, in->len,
                                     send_recv_flags_rpc2h(in->flags),
                                     to, tolen));
    }
    else
    {
        MAKE_CALL(out->retval = func(in->fd, in->buf.buf_val, in->len,
                                     send_recv_flags_rpc2h(in->flags),
                                     (const struct sockaddr *)
                                                (in->to.raw.raw_val),
                                     in->to.raw.raw_len));
    }
}
)

/*-------------- send() ------------------------------*/

TARPC_FUNC(send, {},
{
    INIT_CHECKED_ARG(in->buf.buf_val, in->buf.buf_len, 0);

    MAKE_CALL(out->retval = func(in->fd, in->buf.buf_val, in->len,
                                 send_recv_flags_rpc2h(in->flags)));
}
)

/*-------------- read() ------------------------------*/

TARPC_FUNC(read,
{
    COPY_ARG(buf);
},
{
    INIT_CHECKED_ARG(out->buf.buf_val, out->buf.buf_len, in->len);

    MAKE_CALL(out->retval = func(in->fd, out->buf.buf_val, in->len));
}
)

/*-------------- read_via_splice() ------------------------------*/

tarpc_ssize_t
read_via_splice(tarpc_read_via_splice_in *in,
                tarpc_read_via_splice_out *out)
{
    api_func_ptr    pipe_func;
    api_func        splice_func;
    api_func        close_func;
    api_func        read_func;
    ssize_t         to_pipe;
    ssize_t         from_pipe = 0;
    int             pipefd[2];
    unsigned int    flags = 0;
    int             ret = 0;

#ifdef SPLICE_F_NONBLOCK
    flags = SPLICE_F_MOVE;
#endif

    if (tarpc_find_func(in->common.use_libc, "pipe",
                        (api_func *)&pipe_func) != 0)
    {
        ERROR("%s(): Failed to resolve pipe() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "splice", &splice_func) != 0)
    {
        ERROR("%s(): Failed to resolve splice() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "close", &close_func) != 0)
    {
        ERROR("%s(): Failed to resolve close() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "read", &read_func) != 0)
    {
        ERROR("%s(): Failed to resolve read() function", __FUNCTION__);
        return -1;
    }

    if (pipe_func(pipefd) != 0)
    {
        ERROR("pipe() failed with error %r", TE_OS_RC(TE_TA_UNIX, errno));
        return -1;
    }
    if (in->fd == pipefd[0] || in->fd == pipefd[1])
    {
        ERROR("Aux pipe fd and in fd is the same",
              TE_OS_RC(TE_TA_UNIX, EFAULT));
        errno = EFAULT;
        ret = -1;
        goto read_via_splice_exit;
    }

    if ((to_pipe = splice_func(in->fd, NULL,
                               pipefd[1], NULL, in->len, flags)) < 0)
    {
        ERROR("splice() to pipe failed with error %r",
              TE_OS_RC(TE_TA_UNIX, errno));
        ret = -1;
        goto read_via_splice_exit;
    }
    if ((from_pipe = read_func(pipefd[0], out->buf.buf_val, in->len)) < 0)
    {
        ERROR("read() from pipe failed with error %r",
              TE_OS_RC(TE_TA_UNIX, errno));
        ret = -1;
        goto read_via_splice_exit;
    }
    if (to_pipe != from_pipe)
    {
        ERROR("read() and splice() calls return different amount of data",
              TE_OS_RC(TE_TA_UNIX, EMSGSIZE));
        errno = EMSGSIZE;
        ret = -1;
    }
read_via_splice_exit:
    if (close_func(pipefd[0]) < 0 ||
        close_func(pipefd[1]) < 0)
        ret = -1;
    return ret == -1 ? ret : from_pipe;
}

TARPC_FUNC(read_via_splice,
{
    COPY_ARG(buf);
},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- write() ------------------------------*/

TARPC_FUNC(write, {},
{
    INIT_CHECKED_ARG(in->buf.buf_val, in->buf.buf_len, 0);

    MAKE_CALL(out->retval = func(in->fd, in->buf.buf_val, in->len));
}
)

/*-------------- write_via_splice() ------------------------------*/

tarpc_ssize_t
write_via_splice(tarpc_write_via_splice_in *in)
{
    api_func_ptr    pipe_func;
    api_func        splice_func;
    api_func        close_func;
    api_func        write_func;
    ssize_t         to_pipe;
    ssize_t         from_pipe = 0;
    int             pipefd[2];
    unsigned int    flags = 0;
    int             ret = 0;

#ifdef SPLICE_F_NONBLOCK
    flags = SPLICE_F_MOVE;
#endif

    if (tarpc_find_func(in->common.use_libc, "pipe",
                        (api_func *)&pipe_func) != 0)
    {
        ERROR("%s(): Failed to resolve pipe() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "splice", &splice_func) != 0)
    {
        ERROR("%s(): Failed to resolve splice() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "close", &close_func) != 0)
    {
        ERROR("%s(): Failed to resolve close() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "write", &write_func) != 0)
    {
        ERROR("%s(): Failed to resolve write() function", __FUNCTION__);
        return -1;
    }

    if (pipe_func(pipefd) != 0)
    {
        ERROR("pipe() failed with error %r", TE_OS_RC(TE_TA_UNIX, errno));
        return -1;
    }
    if (in->fd == pipefd[0] || in->fd == pipefd[1])
    {
        ERROR("Aux pipe fd and in fd is the same",
              TE_OS_RC(TE_TA_UNIX, EFAULT));
        errno = EFAULT;
        ret = -1;
        goto write_via_splice_exit;
    }

    if ((to_pipe = write_func(pipefd[1], in->buf.buf_val, in->len)) < 0)
    {
        ERROR("write() to pipe failed with error %r",
              TE_OS_RC(TE_TA_UNIX, errno));
        ret = -1;
        goto write_via_splice_exit;
    }
    if ((from_pipe = splice_func(pipefd[0], NULL, in->fd, NULL,
                                 in->len, flags)) < 0)
    {
        ERROR("splice() from pipe failed with error %r",
              TE_OS_RC(TE_TA_UNIX, errno));
        ret = -1;
        goto write_via_splice_exit;
    }
    if (to_pipe != from_pipe)
    {
        ERROR("write() and splice() calls return different amount of data",
              TE_OS_RC(TE_TA_UNIX, EMSGSIZE));
        errno = EMSGSIZE;
        ret = -1;
    }
write_via_splice_exit:
    if (close_func(pipefd[0]) < 0 ||
        close_func(pipefd[1]) < 0)
        ret = -1;
    return ret == -1 ? ret : from_pipe;
}

TARPC_FUNC(write_via_splice, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*------------ write_and_close() ----------------------*/
bool_t
_write_and_close_1_svc(tarpc_write_and_close_in *in,
                       tarpc_write_and_close_out *out,
                       struct svc_req *rqstp)
{
    api_func write_func;
    api_func close_func;
    int      rc = 0;

    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));

    if (tarpc_find_func(in->common.use_libc, "write", &write_func) != 0)
    {
        ERROR("Failed to find function \"write\"");
        out->retval =  -1;
    }
    else if (tarpc_find_func(in->common.use_libc, "close",
                             &close_func) != 0)
    {
        ERROR("Failed to find function \"close\"");
        out->retval =  -1;
    }
    else
    {
        out->retval = write_func(in->fd, in->buf.buf_val, in->len);

        if (out->retval >= 0)
        {
            rc = close_func(in->fd);
            if (rc < 0)
                out->retval = rc;
        }
    }

    return TRUE;
}

/*-------------- readbuf() ------------------------------*/
ssize_t
readbuf(tarpc_readbuf_in *in)
{
    api_func read_func;

    if (tarpc_find_func(in->common.use_libc, "read", &read_func) != 0)
    {
        ERROR("Failed to find function \"read\"");
        return -1;
    }

    return read_func(in->fd, rcf_pch_mem_get(in->buf) + in->off,
                     in->len);
}

TARPC_FUNC(readbuf, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*-------------- recvbuf() ------------------------------*/
ssize_t
recvbuf(tarpc_recvbuf_in *in)
{
    api_func recv_func;

    if (tarpc_find_func(in->common.use_libc, "recv", &recv_func) != 0)
    {
        ERROR("Failed to find function \"recv\"");
        return -1;
    }

    return recv_func(in->fd, rcf_pch_mem_get(in->buf) + in->off,
                     in->len, in->flags);
}

TARPC_FUNC(recvbuf, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*-------------- writebuf() ------------------------------*/
ssize_t
writebuf(tarpc_writebuf_in *in)
{
    api_func write_func;

    if (tarpc_find_func(in->common.use_libc, "write", &write_func) != 0)
    {
        ERROR("Failed to find function \"write\"");
        return -1;
    }
    return write_func(in->fd,
                      rcf_pch_mem_get(in->buf) + in->off,
                      in->len);
}

TARPC_FUNC(writebuf, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*-------------- sendbuf() ------------------------------*/
ssize_t
sendbuf(tarpc_sendbuf_in *in)
{
    api_func send_func;

    if (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0)
    {
        ERROR("Failed to find function \"send\"");
        return -1;
    }
    return send_func(in->fd, rcf_pch_mem_get(in->buf) + in->off,
                     in->len, send_recv_flags_rpc2h(in->flags));
}

TARPC_FUNC(sendbuf, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*------------ send_msg_more() --------------------------*/
ssize_t
send_msg_more(tarpc_send_msg_more_in *in)
{
    int res1, res2;

    api_func send_func;

    if (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0)
    {
        ERROR("Failed to find function \"send\"");
        return -1;
    }

    if (-1 == (res1 = send_func(in->fd, rcf_pch_mem_get(in->buf),
                                in->first_len, MSG_MORE)))
        return -1;

    if (-1 == (res2 = send_func(in->fd, rcf_pch_mem_get(in->buf) +
                                        in->first_len,
                                in->second_len, 0)))
        return -1;
    return res1 + res2;
}

TARPC_FUNC(send_msg_more, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*------------ send_one_byte_many() --------------------------*/
ssize_t
send_one_byte_many(tarpc_send_one_byte_many_in *in)
{
    int      sent = 0;
    int      rc;
    char     buf = 'A';
    api_func send_func;

    if (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0)
    {
        ERROR("Failed to find function \"send\"");
        return -1;
    }

    do {
        rc = send_func(in->fd, &buf, 1, MSG_DONTWAIT);
        if (in->delay != 0)
            usleep(in->delay);
    } while (rc > 0 && (sent += rc));
    return sent;
}

TARPC_FUNC(send_one_byte_many, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*-------------- readv() ------------------------------*/

TARPC_FUNC(readv,
{
    if (out->vector.vector_len > RCF_RPC_MAX_IOVEC)
    {
        ERROR("Too long iovec is provided");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    COPY_ARG(vector);
},
{
    struct iovec    iovec_arr[RCF_RPC_MAX_IOVEC];
    unsigned int    i;

    memset(iovec_arr, 0, sizeof(iovec_arr));
    for (i = 0; i < out->vector.vector_len; i++)
    {
        INIT_CHECKED_ARG(out->vector.vector_val[i].iov_base.iov_base_val,
                         out->vector.vector_val[i].iov_base.iov_base_len,
                         out->vector.vector_val[i].iov_len);
        iovec_arr[i].iov_base =
            out->vector.vector_val[i].iov_base.iov_base_val;
        iovec_arr[i].iov_len = out->vector.vector_val[i].iov_len;
    }
    INIT_CHECKED_ARG((char *)iovec_arr, sizeof(iovec_arr), 0);

    MAKE_CALL(out->retval = func(in->fd, iovec_arr, in->count));
}
)

/*-------------- writev() ------------------------------*/

TARPC_FUNC(writev,
{
    if (in->vector.vector_len > RCF_RPC_MAX_IOVEC)
    {
        ERROR("Too long iovec is provided");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
},
{
    struct iovec    iovec_arr[RCF_RPC_MAX_IOVEC];
    unsigned int    i;

    memset(iovec_arr, 0, sizeof(iovec_arr));
    for (i = 0; i < in->vector.vector_len; i++)
    {
        INIT_CHECKED_ARG(in->vector.vector_val[i].iov_base.iov_base_val,
                         in->vector.vector_val[i].iov_base.iov_base_len, 0);
        iovec_arr[i].iov_base =
            in->vector.vector_val[i].iov_base.iov_base_val;
        iovec_arr[i].iov_len = in->vector.vector_val[i].iov_len;
    }
    INIT_CHECKED_ARG((char *)iovec_arr, sizeof(iovec_arr), 0);

    MAKE_CALL(out->retval = func(in->fd, iovec_arr, in->count));
}
)

#ifndef TE_POSIX_FS_PROVIDED
/*-------------- lseek() ------------------------------*/

TARPC_FUNC(lseek, {},
{
    if (sizeof(off_t) == 4)
    {
        if (in->pos > UINT_MAX)
        {
            ERROR("'offset' value passed to lseek "
                  "exceeds 'off_t' data type range");
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        }
        else
        {
            MAKE_CALL(out->retval = func(in->fd, (off_t)in->pos,
                                         lseek_mode_rpc2h(in->mode)));
        }
    }
    else if (sizeof(off_t) == 8)
    {
        MAKE_CALL(out->retval = func_ret_int64(in->fd, in->pos,
                                               lseek_mode_rpc2h(in->mode)));
    }
    else
    {
        ERROR("Unexpected size of 'off_t' for lseek call");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
})
#endif

/*-------------- fsync() ------------------------------*/

TARPC_FUNC(fsync, {},
{
    MAKE_CALL(out->retval = func(in->fd));
})


/*-------------- getsockname() ------------------------------*/
TARPC_FUNC(getsockname,
{
    COPY_ARG(len);
    COPY_ARG_ADDR(addr);
},
{
    PREPARE_ADDR(name, out->addr,
                 out->len.len_len == 0 ? 0 : *out->len.len_val);

    MAKE_CALL(out->retval = func(in->fd, name,
                                 out->len.len_len == 0 ? NULL :
                                 out->len.len_val));

    sockaddr_output_h2rpc(name, namelen,
                          out->len.len_len == 0 ? 0 :
                              *(out->len.len_val),
                          &(out->addr));
}
)

/*-------------- getpeername() ------------------------------*/

TARPC_FUNC(getpeername,
{
    COPY_ARG(len);
    COPY_ARG_ADDR(addr);
},
{
    PREPARE_ADDR(name, out->addr,
                 out->len.len_len == 0 ? 0 : *out->len.len_val);

    MAKE_CALL(out->retval = func(in->fd, name,
                                 out->len.len_len == 0 ? NULL :
                                 out->len.len_val));

    sockaddr_output_h2rpc(name, namelen,
                          out->len.len_len == 0 ? 0 :
                              *(out->len.len_val),
                          &(out->addr));
}
)

/*-------------- fd_set constructor ----------------------------*/

void
fd_set_new(tarpc_fd_set_new_out *out)
{
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        fd_set *ptr = calloc(1, sizeof(fd_set));
        out->retval = RCF_PCH_MEM_INDEX_ALLOC(ptr, ns);
    });
}

TARPC_FUNC_STATIC(fd_set_new, {},
{
    MAKE_CALL(func(out));
})

/*-------------- fd_set destructor ----------------------------*/

te_errno
fd_set_delete(tarpc_fd_set_delete_in *in,
              tarpc_fd_set_delete_out *out)
{
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        free(IN_FDSET_NS(ns));
        return RCF_PCH_MEM_INDEX_FREE(in->set, ns);
    });
    return out->common._errno;
}

TARPC_FUNC_STATIC(fd_set_delete, {},
{
    te_errno rc;
    MAKE_CALL(rc = func(in, out));
    if (out->common._errno == 0)
        out->common._errno = rc;
})

/*-------------- FD_ZERO --------------------------------*/

void
do_fd_zero(tarpc_do_fd_zero_in *in)
{
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        FD_ZERO(IN_FDSET_NS(ns));
    });
}

TARPC_FUNC_STATIC(do_fd_zero, {},
{
    MAKE_CALL(func(in));
})

/*-------------- FD_SET --------------------------------*/

void
do_fd_set(tarpc_do_fd_set_in *in)
{
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        FD_SET(in->fd, IN_FDSET_NS(ns));
    });
}

TARPC_FUNC_STATIC(do_fd_set, {},
{
    MAKE_CALL(func(in));
})

/*-------------- FD_CLR --------------------------------*/

void
do_fd_clr(tarpc_do_fd_clr_in *in)
{
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        FD_CLR(in->fd, IN_FDSET_NS(ns));
    });
}

TARPC_FUNC_STATIC(do_fd_clr, {},
{
    MAKE_CALL(func(in));
})

/*-------------- FD_ISSET --------------------------------*/

void
do_fd_isset(tarpc_do_fd_isset_in *in,
            tarpc_do_fd_isset_out *out)
{
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        out->retval = FD_ISSET(in->fd, IN_FDSET_NS(ns));
    });
}

TARPC_FUNC_STATIC(do_fd_isset, {},
{
    MAKE_CALL(func(in, out));
})

/*-------------- select() --------------------------------*/

TARPC_FUNC(select,
{
    COPY_ARG(timeout);
},
{
    fd_set *rfds;
    fd_set *wfds;
    fd_set *efds;
    struct timeval tv;
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;

    out->retval = -1;
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_FD_SET, );

    if (out->timeout.timeout_len > 0)
        TARPC_CHECK_RC(timeval_rpc2h(out->timeout.timeout_val, &tv));

    if (out->common._errno != 0)
        return;

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(rfds, in->readfds, ns, );
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(wfds, in->writefds, ns, );
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(efds, in->exceptfds, ns, );

    MAKE_CALL(out->retval = func(in->n, rfds, wfds, efds,
                    out->timeout.timeout_len == 0 ? NULL : &tv));

    if (out->timeout.timeout_len > 0)
        TARPC_CHECK_RC(timeval_h2rpc(
                &tv, out->timeout.timeout_val));
    if (TE_RC_GET_ERROR(out->common._errno) == TE_EH2RPC)
        out->retval = -1;
}
)

/*-------------- if_nametoindex() --------------------------------*/

TARPC_FUNC(if_nametoindex, {},
{
    INIT_CHECKED_ARG(in->ifname.ifname_val, in->ifname.ifname_len, 0);
    MAKE_CALL(out->ifindex = func_ptr(in->ifname.ifname_val));
}
)

/*-------------- if_indextoname() --------------------------------*/

TARPC_FUNC(if_indextoname,
{
    COPY_ARG(ifname);
},
{
    if (out->ifname.ifname_val == NULL ||
        out->ifname.ifname_len >= IF_NAMESIZE)
    {
        char *name;

        MAKE_CALL(name = (char *)func_ret_ptr(in->ifindex,
                                              out->ifname.ifname_val));

        if (name != NULL && name != out->ifname.ifname_val)
        {
            ERROR("if_indextoname() returned incorrect pointer");
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ECORRUPTED);
        }
    }
    else
    {
        ERROR("if_indextoname() cannot be called with 'ifname' location "
              "size less than IF_NAMESIZE");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
}
)

#if HAVE_STRUCT_IF_NAMEINDEX
/*-------------- if_nameindex() ------------------------------*/

TARPC_FUNC(if_nameindex, {},
{
    struct if_nameindex *ret;
    tarpc_if_nameindex  *arr = NULL;

    int i = 0;

    MAKE_CALL(ret = (struct if_nameindex *)func_void_ret_ptr());

    if (ret != NULL)
    {
        int j;

        out->mem_ptr = rcf_pch_mem_alloc(ret);
        while (ret[i++].if_index != 0);
        arr = (tarpc_if_nameindex *)calloc(sizeof(*arr) * i, 1);
        if (arr == NULL)
        {
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        }
        else
        {
            for (j = 0; j < i - 1; j++)
            {
                arr[j].ifindex = ret[j].if_index;
                arr[j].ifname.ifname_val = strdup(ret[j].if_name);
                if (arr[j].ifname.ifname_val == NULL)
                {
                    for (j--; j >= 0; j--)
                        free(arr[j].ifname.ifname_val);
                    free(arr);
                    out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                    arr = NULL;
                    i = 0;
                    break;
                }
                arr[j].ifname.ifname_len = strlen(ret[j].if_name) + 1;
            }
        }
    }
    out->ptr.ptr_val = arr;
    out->ptr.ptr_len = i;
}
)

/*-------------- if_freenameindex() ----------------------------*/

TARPC_FUNC(if_freenameindex, {},
{
    MAKE_CALL(func_ptr(rcf_pch_mem_get(in->mem_ptr)));
    rcf_pch_mem_free(in->mem_ptr);
}
)
#endif /* HAVE_STRUCT_IF_NAMEINDEX */

/*-------------- sigset_t constructor ---------------------------*/

bool_t
_sigset_new_1_svc(tarpc_sigset_new_in *in, tarpc_sigset_new_out *out,
                  struct svc_req *rqstp)
{
    sigset_t *set;

    UNUSED(rqstp);
    UNUSED(in);

    memset(out, 0, sizeof(*out));

    errno = 0;
    if ((set = (sigset_t *)calloc(1, sizeof(sigset_t))) == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    else
    {
        out->common._errno = RPC_ERRNO;
        out->set = rcf_pch_mem_alloc(set);
    }

    return TRUE;
}

/*-------------- sigset_t destructor ----------------------------*/

bool_t
_sigset_delete_1_svc(tarpc_sigset_delete_in *in,
                     tarpc_sigset_delete_out *out,
                     struct svc_req *rqstp)
{
    UNUSED(rqstp);

    memset(out, 0, sizeof(*out));

    errno = 0;
    free(IN_SIGSET);
    rcf_pch_mem_free(in->set);
    out->common._errno = RPC_ERRNO;

    return TRUE;
}

/*-------------- sigemptyset() ------------------------------*/

TARPC_FUNC(sigemptyset, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET));
}
)

/*-------------- sigpendingt() ------------------------------*/

TARPC_FUNC(sigpending, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET));
}
)

/*-------------- sigsuspend() ------------------------------*/

TARPC_FUNC(sigsuspend, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET));
}
)

/*-------------- sigfillset() ------------------------------*/

TARPC_FUNC(sigfillset, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET));
}
)

/*-------------- sigaddset() -------------------------------*/

TARPC_FUNC(sigaddset, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET, signum_rpc2h(in->signum)));
}
)

/*-------------- sigdelset() -------------------------------*/

TARPC_FUNC(sigdelset, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET, signum_rpc2h(in->signum)));
}
)

/*-------------- sigismember() ------------------------------*/

TARPC_FUNC(sigismember, {},
{
    INIT_CHECKED_ARG((char *)(IN_SIGSET), sizeof(sigset_t), 0);
    MAKE_CALL(out->retval = func_ptr(IN_SIGSET, signum_rpc2h(in->signum)));
}
)

/*-------------- sigprocmask() ------------------------------*/
TARPC_FUNC(sigprocmask, {},
{
    INIT_CHECKED_ARG((char *)IN_SIGSET, sizeof(sigset_t), 0);
    MAKE_CALL(out->retval = func(sighow_rpc2h(in->how), IN_SIGSET,
                                 (sigset_t *)rcf_pch_mem_get(in->oldset)));
}
)

/*-------------- sigset_cmp() ------------------------------*/

/**
 * Compare two signal masks.
 *
 * @param sig_first     The first signal mask
 * @param sig_second    The second signal mask
 *
 * @return -1, 0 or 1 as a result of comparison
 */
int
sigset_cmp(sigset_t *sig_first, sigset_t *sig_second)
{
    int          i;
    int          in_first;
    int          in_second;
    int          saved_errno = errno;

    for (i = 1; i <= SIGRTMAX; i++)
    {
        in_first = sigismember(sig_first, i);
        in_second = sigismember(sig_second, i);
        if (in_first != in_second)
        {
            errno = saved_errno;
            return (in_first < in_second) ? -1 : 1;
        }
    }

    errno = saved_errno;
    return 0;
}

TARPC_FUNC(sigset_cmp, {},
{
    sigset_t    *sig1 = (sigset_t *)rcf_pch_mem_get(in->first_set);
    sigset_t    *sig2 = (sigset_t *)rcf_pch_mem_get(in->second_set);

    MAKE_CALL(out->retval = func_ptr(sig1, sig2));
}
)

/*-------------- kill() --------------------------------*/

TARPC_FUNC(kill, {},
{
    MAKE_CALL(out->retval = func(in->pid, signum_rpc2h(in->signum)));
}
)

/*-------------- pthread_kill() ------------------------*/

TARPC_FUNC(pthread_kill, {},
{
    MAKE_CALL(out->retval = func(in->tid, signum_rpc2h(in->signum)));
}
)

/*-------------- tgkill() ------------------------------*/

/**
 * Use tgkill() system call.
 *
 * @param tgid      Thread GID
 * @param tid       Thread ID
 * @param sig       Signal to be sent
 *
 * @return Status code
 */
int
call_tgkill(int tgid, int tid, int sig)
{
#ifndef SYS_tgkill
    UNUSED(tgid);
    UNUSED(tid);
    UNUSED(sig);

    ERROR("tgkill() is not defined");
    errno = ENOENT;
    return -1;
#else
    return syscall(SYS_tgkill, tgid, tid, sig);
#endif
}

TARPC_FUNC(call_tgkill, {},
{
    MAKE_CALL(out->retval = func(in->tgid, in->tid,
                                 signum_rpc2h(in->sig)));
}
)

/*-------------- gettid() ------------------------------*/

/**
 * Use gettid() system call.
 *
 * @return Thread ID or -1
 */
int
call_gettid()
{
#ifndef SYS_gettid
    ERROR("gettid() is not defined");
    errno = ENOENT;
    return -1;
#else
    return syscall(SYS_gettid);
#endif
}

TARPC_FUNC(call_gettid, {},
{
    MAKE_CALL(out->retval = func_void());
}
)

/*-------------- waitpid() --------------------------------*/

TARPC_FUNC(waitpid, {},
{
    int             st;
    rpc_wait_status r_st;
    pid_t (*real_func)(pid_t pid, int *status, int options) = func;

    if (!(in->options & RPC_WSYSTEM))
        real_func = ta_waitpid;
    MAKE_CALL(out->pid = real_func(in->pid, &st,
                                   waitpid_opts_rpc2h(in->options)));
    r_st = wait_status_h2rpc(st);
    out->status_flag = r_st.flag;
    out->status_value = r_st.value;
}
)

/*-------------- ta_kill_death() --------------------------------*/

TARPC_FUNC(ta_kill_death, {},
{
    MAKE_CALL(out->retval = func(in->pid));
}
)

sigset_t rpcs_received_signals;

/* See description in unix_internal.h */
void
signal_registrar(int signum)
{
    sigaddset(&rpcs_received_signals, signum);
}

/* Lastly received signal information */
tarpc_siginfo_t last_siginfo;

/* See description in unix_internal.h */
void
signal_registrar_siginfo(int signum, siginfo_t *siginfo, void *context)
{
#define COPY_SI_FIELD(_field) \
    last_siginfo.sig_ ## _field = siginfo->si_ ## _field

    UNUSED(context);

    sigaddset(&rpcs_received_signals, signum);
    memset(&last_siginfo, 0, sizeof(last_siginfo));

    COPY_SI_FIELD(signo);
    COPY_SI_FIELD(errno);
    COPY_SI_FIELD(code);
#ifdef HAVE_SIGINFO_T_SI_TRAPNO
    COPY_SI_FIELD(trapno);
#endif
    COPY_SI_FIELD(pid);
    COPY_SI_FIELD(uid);
    COPY_SI_FIELD(status);
#ifdef HAVE_SIGINFO_T_SI_UTIME
    COPY_SI_FIELD(utime);
#endif
#ifdef HAVE_SIGINFO_T_SI_STIME
    COPY_SI_FIELD(stime);
#endif

    /** 
     * FIXME: si_value, si_ptr and si_addr fields are not
     * supported yet
     */

#ifdef HAVE_SIGINFO_T_SI_INT
    COPY_SI_FIELD(int);
#endif
#ifdef HAVE_SIGINFO_T_SI_OVERRUN
    COPY_SI_FIELD(overrun);
#endif
#ifdef HAVE_SIGINFO_T_SI_TIMERID
    COPY_SI_FIELD(timerid);
#endif
#ifdef HAVE_SIGINFO_T_SI_BAND
    COPY_SI_FIELD(band);
#endif
#ifdef HAVE_SIGINFO_T_SI_FD
    COPY_SI_FIELD(fd);
#endif

#ifdef HAVE_SIGINFO_T_SI_ADDR_LSB
    COPY_SI_FIELD(addr_lsb);
#endif

#undef COPY_SI_FIELD
}


/*-------------- signal() --------------------------------*/

TARPC_FUNC(signal,
{
    if (in->signum == RPC_SIGINT)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EPERM);
        return TRUE;
    }
},
{
    sighandler_t handler;

    if ((out->common._errno = name2handler(in->handler,
                                           (void **)&handler)) == 0)
    {
        int     signum = signum_rpc2h(in->signum);
        void   *old_handler;

        MAKE_CALL(old_handler = func_ret_ptr(signum, handler));

        out->handler = handler2name(old_handler);
        if (old_handler != SIG_ERR)
        {
            /*
             * Delete signal from set of received signals when
             * signal registrar is set for the signal.
             */
            if ((handler == signal_registrar) &&
                RPC_IS_ERRNO_RPC(out->common._errno))
            {
                sigdelset(&rpcs_received_signals, signum);
            }
        }
    }
}
)

/*-------------- bsd_signal() --------------------------------*/

TARPC_FUNC(bsd_signal,
{
    if (in->signum == RPC_SIGINT)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EPERM);
        return TRUE;
    }
},
{
    sighandler_t handler;

    if ((out->common._errno = name2handler(in->handler,
                                           (void **)&handler)) == 0)
    {
        int     signum = signum_rpc2h(in->signum);
        void   *old_handler;

        MAKE_CALL(old_handler = func_ret_ptr(signum, handler));

        out->handler = handler2name(old_handler);
        if (old_handler != SIG_ERR)
        {

            /*
             * Delete signal from set of received signals when
             * signal registrar is set for the signal.
             */
            if ((handler == signal_registrar) &&
                RPC_IS_ERRNO_RPC(out->common._errno))
            {
                sigdelset(&rpcs_received_signals, signum);
            }
        }
    }
}
)

/*-------------- sysv_signal() --------------------------------*/

TARPC_FUNC(sysv_signal,
{
    if (in->signum == RPC_SIGINT)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EPERM);
        return TRUE;
    }
},
{
    sighandler_t handler;

    if ((out->common._errno = name2handler(in->handler,
                                           (void **)&handler)) == 0)
    {
        int     signum = signum_rpc2h(in->signum);
        void   *old_handler;

        MAKE_CALL(old_handler = func_ret_ptr(signum, handler));

        out->handler = handler2name(old_handler);
        if (old_handler != SIG_ERR)
        {
            /*
             * Delete signal from set of received signals when
             * signal registrar is set for the signal.
             */
            if ((handler == signal_registrar) &&
                RPC_IS_ERRNO_RPC(out->common._errno))
            {
                sigdelset(&rpcs_received_signals, signum);
            }
        }
    }
}
)

/*-------------- siginterrupt() --------------------------------*/

TARPC_FUNC(siginterrupt, {},
{
    MAKE_CALL(out->retval = func(signum_rpc2h(in->signum), in->flag));
}
)


/*-------------- sigaction() --------------------------------*/

/** Return pointer to sa_restorer field of the structure or dummy address */
static void **
get_sa_restorer(struct sigaction *sa)
{
#if HAVE_STRUCT_SIGACTION_SA_RESTORER
    return (void **)&(sa->sa_restorer);
#else
    static void *dummy = NULL;

    UNUSED(sa);
    return &dummy;
#endif
}

TARPC_FUNC(sigaction,
{
    if (in->signum == RPC_SIGINT)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EPERM);
        return TRUE;
    }
    COPY_ARG(oldact);
},
{
    tarpc_sigaction  *out_oldact = out->oldact.oldact_val;

    int               signum = signum_rpc2h(in->signum);
    struct sigaction  act;
    struct sigaction *p_act = NULL;
    struct sigaction  oldact;
    struct sigaction *p_oldact = NULL;
    sigset_t         *oldact_mask = NULL;

    memset(&act, 0, sizeof(act));
    memset(&oldact, 0, sizeof(oldact));

    if (in->act.act_len != 0)
    {
        tarpc_sigaction *in_act = in->act.act_val;
        sigset_t        *act_mask;

        p_act = &act;

        act.sa_flags = sigaction_flags_rpc2h(in_act->flags);
        act_mask = (sigset_t *) rcf_pch_mem_get(in_act->mask);
        if (act_mask == NULL)
        {
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EFAULT);
            out->retval = -1;
            goto finish;
        }
        act.sa_mask = *act_mask;

        out->common._errno =
            name2handler(in_act->handler,
                         (act.sa_flags & SA_SIGINFO) ?
                         (void **)&(act.sa_sigaction) :
                         (void **)&(act.sa_handler));

        if (out->common._errno != 0)
        {
            out->retval = -1;
            goto finish;
        }

        out->common._errno = name2handler(in_act->restorer,
                                          get_sa_restorer(&act));
        if (out->common._errno != 0)
        {
            out->retval = -1;
            goto finish;
        }
    }

    if (out->oldact.oldact_len != 0)
    {
        p_oldact = &oldact;

        oldact.sa_flags = sigaction_flags_rpc2h(out_oldact->flags);
        if ((out_oldact->mask != RPC_NULL) &&
            (oldact_mask =
                (sigset_t *) rcf_pch_mem_get(out_oldact->mask)) == NULL)
        {
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EFAULT);
            out->retval = -1;
            goto finish;
        }
        if (oldact_mask != NULL)
            oldact.sa_mask = *oldact_mask;

        out->common._errno =
            name2handler(out_oldact->handler,
                         (oldact.sa_flags & SA_SIGINFO) ?
                         (void **)&(oldact.sa_sigaction) :
                         (void **)&(oldact.sa_handler));

        if (out->common._errno != 0)
        {
            out->retval = -1;
            goto finish;
        }

        out->common._errno = name2handler(out_oldact->restorer,
                                          get_sa_restorer(&oldact));
        if (out->common._errno != 0)
        {
            out->retval = -1;
            goto finish;
        }
    }

    MAKE_CALL(out->retval = func(signum, p_act, p_oldact));

    if ((out->retval == 0) && (p_act != NULL) &&
        (((p_act->sa_flags & SA_SIGINFO) ?
              (void *)(act.sa_sigaction) :
              (void *)(act.sa_handler)) == signal_registrar))
    {
        /*
         * Delete signal from set of received signals when
         * signal registrar is set for the signal.
         */
        sigdelset(&rpcs_received_signals, signum);
    }

    if (p_oldact != NULL)
    {
        out_oldact->flags = sigaction_flags_h2rpc(oldact.sa_flags);
        if (oldact_mask != NULL)
            *oldact_mask = oldact.sa_mask;
        out_oldact->handler = handler2name((oldact.sa_flags & SA_SIGINFO) ?
                                           (void *)oldact.sa_sigaction :
                                           (void *)oldact.sa_handler);
        out_oldact->restorer = handler2name(*(get_sa_restorer(&oldact)));
    }

    finish:
    ;
}
)

/** Convert tarpc_stack_t to stack_t.
 *
 * @param tarpc_s   Pointer to TARPC structure
 * @param h_s       Pointer to native structure
 *
 * @return @c 0 on success or @c -1
 * */
int
stack_t_tarpc2h(tarpc_stack_t *tarpc_s, stack_t *h_s)
{
    if (tarpc_s == NULL || h_s == NULL)
        return -1;

    h_s->ss_sp = rcf_pch_mem_get(tarpc_s->ss_sp);
    h_s->ss_flags = sigaltstack_flags_rpc2h(tarpc_s->ss_flags);
    h_s->ss_size = tarpc_s->ss_size;

    return 0;
}

/** Convert stack_t to tarpc_stack_t.
 *
 * @param h_s       Pointer to native structure
 * @param tarpc_s   Pointer to TARPC structure
 *
 * @return @c 0 on success or @c -1
 * */
int
stack_t_h2tarpc(stack_t *h_s, tarpc_stack_t *tarpc_s)
{
    if (tarpc_s == NULL || h_s == NULL)
        return -1;

    tarpc_s->ss_sp = rcf_pch_mem_get_id(h_s->ss_sp);
    if (tarpc_s->ss_sp == 0 && h_s->ss_sp != NULL)
        tarpc_s->ss_sp = RPC_UNKNOWN_ADDR;
    tarpc_s->ss_flags = sigaltstack_flags_h2rpc(h_s->ss_flags);
    tarpc_s->ss_size = h_s->ss_size;

    return 0;
}

/*-------------- sigaltstack() -----------------------------*/
TARPC_FUNC(sigaltstack,
{
    COPY_ARG(oss);
},
{
    tarpc_stack_t      *out_ss = NULL;

    stack_t      ss;
    stack_t      oss;
    stack_t     *ss_arg = NULL;
    stack_t     *oss_arg = NULL;

    memset(&ss, 0, sizeof(ss));
    memset(&oss, 0, sizeof(ss));

    if (in->ss.ss_len != 0)
    {
        stack_t_tarpc2h(in->ss.ss_val, &ss);
        ss_arg = &ss;
    }

    if (out->oss.oss_len != 0)
    {
        out_ss = out->oss.oss_val;
        stack_t_tarpc2h(out->oss.oss_val, &oss);
        oss_arg = &oss;
    }

    MAKE_CALL(out->retval = func_ptr(ss_arg, oss_arg));

    if (oss_arg != NULL)
        stack_t_h2tarpc(oss_arg, out_ss);
})

/*-------------- setsockopt() ------------------------------*/

#if HAVE_STRUCT_IPV6_MREQ_IPV6MR_IFINDEX
#define IPV6MR_IFINDEX  ipv6mr_ifindex
#elif HAVE_STRUCT_IPV6_MREQ_IPV6MR_INTERFACE
#define IPV6MR_IFINDEX  ipv6mr_interface
#else
#error No interface index in struct ipv6_mreq
#endif

typedef union sockopt_param {
    int                   integer;
    char                 *str;
    struct timeval        tv;
    struct linger         linger;
    struct in_addr        addr;
    struct in6_addr       addr6;
    struct ip_mreq        mreq;
    struct ip_mreq_source mreq_source;
#if HAVE_STRUCT_IP_MREQN
    struct ip_mreqn       mreqn;
#endif
    struct ipv6_mreq      mreq6;
#if HAVE_STRUCT_TCP_INFO
    struct tcp_info       tcpi;
#endif
#if HAVE_STRUCT_GROUP_REQ
    struct group_req      gr_req;
#endif
} sockopt_param;

static void
tarpc_setsockopt(tarpc_setsockopt_in *in, tarpc_setsockopt_out *out,
                 sockopt_param *param, socklen_t *optlen)
{
    option_value   *in_optval = in->optval.optval_val;

    switch (in_optval->opttype)
    {
        case OPT_INT:
        {
            param->integer = in_optval->option_value_u.opt_int;
            *optlen = sizeof(int);

            if (in->level == RPC_SOL_IP &&
                in->optname == RPC_IP_MTU_DISCOVER)
                param->integer = mtu_discover_arg_rpc2h(param->integer);
#ifdef HAVE_LINUX_NET_TSTAMP_H
            if (in->level == RPC_SOL_SOCKET &&
                in->optname == RPC_SO_TIMESTAMPING)
                param->integer = hwtstamp_instr_rpc2h(param->integer);
#endif
            break;
        }

        case OPT_TIMEVAL:
        {
            param->tv.tv_sec =
                in_optval->option_value_u.opt_timeval.tv_sec;
            param->tv.tv_usec =
                in_optval->option_value_u.opt_timeval.tv_usec;
            *optlen = sizeof(param->tv);
            break;
        }

        case OPT_LINGER:
        {
            param->linger.l_onoff =
                in_optval->option_value_u.opt_linger.l_onoff;
            param->linger.l_linger =
                in_optval->option_value_u.opt_linger.l_linger;
            *optlen = sizeof(param->linger);
            break;
        }

        case OPT_MREQ:
        {
            memcpy(&param->mreq.imr_multiaddr,
                   &in_optval->option_value_u.opt_mreq.imr_multiaddr,
                   sizeof(param->mreq.imr_multiaddr));
            param->mreq.imr_multiaddr.s_addr =
                htonl(param->mreq.imr_multiaddr.s_addr);
            memcpy(&param->mreq.imr_interface,
                   &in_optval->option_value_u.opt_mreq.imr_address,
                   sizeof(param->mreq.imr_interface));
            param->mreq.imr_interface.s_addr =
                htonl(param->mreq.imr_interface.s_addr);
            *optlen = sizeof(param->mreq);
            break;
        }

        case OPT_MREQ_SOURCE:
        {
            memcpy((char *)&(param->mreq_source.imr_multiaddr),
                   &in_optval->option_value_u.opt_mreq_source.imr_multiaddr,
                   sizeof(param->mreq_source.imr_multiaddr));
            param->mreq_source.imr_multiaddr.s_addr =
                htonl(param->mreq_source.imr_multiaddr.s_addr);

            memcpy((char *)&(param->mreq_source.imr_interface),
                   &in_optval->option_value_u.opt_mreq_source.imr_interface,
                   sizeof(param->mreq_source.imr_interface));
            param->mreq_source.imr_interface.s_addr =
                htonl(param->mreq_source.imr_interface.s_addr);

            memcpy((char *)&(param->mreq_source.imr_sourceaddr),
                  &in_optval->option_value_u.opt_mreq_source.imr_sourceaddr,
                   sizeof(param->mreq_source.imr_sourceaddr));
            param->mreq_source.imr_sourceaddr.s_addr =
                htonl(param->mreq_source.imr_sourceaddr.s_addr);

            *optlen = sizeof(param->mreq_source);
            break;
        }

        case OPT_MREQN:
        {
#if HAVE_STRUCT_IP_MREQN
            memcpy((char *)&(param->mreqn.imr_multiaddr),
                   &in_optval->option_value_u.opt_mreqn.imr_multiaddr,
                   sizeof(param->mreqn.imr_multiaddr));
            param->mreqn.imr_multiaddr.s_addr =
                htonl(param->mreqn.imr_multiaddr.s_addr);
            memcpy((char *)&(param->mreqn.imr_address),
                   &in_optval->option_value_u.opt_mreqn.imr_address,
                   sizeof(param->mreqn.imr_address));
            param->mreqn.imr_address.s_addr =
                htonl(param->mreqn.imr_address.s_addr);

            param->mreqn.imr_ifindex =
                in_optval->option_value_u.opt_mreqn.imr_ifindex;
            *optlen = sizeof(param->mreqn);
            break;
#else
            ERROR("'struct ip_mreqn' is not defined");
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
            out->retval = -1;
            break;
#endif
        }
        case OPT_MREQ6:
        {
            memcpy(&(param->mreq6.ipv6mr_multiaddr),
                   &in_optval->option_value_u.opt_mreq6.
                       ipv6mr_multiaddr.ipv6mr_multiaddr_val,
                   sizeof(struct in6_addr));
            param->mreq6.ipv6mr_interface =
                in_optval->option_value_u.opt_mreq6.ipv6mr_ifindex;
            *optlen = sizeof(param->mreq6);
            break;
        }

        case OPT_IPADDR:
        {
            memcpy(&param->addr, &in_optval->option_value_u.opt_ipaddr,
                   sizeof(struct in_addr));
            param->addr.s_addr = htonl(param->addr.s_addr);
            *optlen = sizeof(param->addr);
            break;
        }

        case OPT_IPADDR6:
        {
            memcpy(&param->addr6, in_optval->option_value_u.opt_ipaddr6,
                   sizeof(struct in6_addr));
            *optlen = sizeof(param->addr6);
            break;
        }

        case OPT_GROUP_REQ:
        {
#if HAVE_STRUCT_GROUP_REQ
            sockaddr_rpc2h(&in_optval->option_value_u.
                           opt_group_req.gr_group,
                           (struct sockaddr *)&(param->gr_req.gr_group),
                           sizeof(param->gr_req.gr_group), NULL, NULL);
            param->gr_req.gr_interface =
                in_optval->option_value_u.opt_group_req.gr_interface;
            *optlen = sizeof(param->gr_req);
            break;
#else
            ERROR("'struct group_req' is not defined");
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
            out->retval = -1;
            break;
#endif
        }

        default:
            ERROR("incorrect option type %d is received",
                  in_optval->opttype);
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
            out->retval = -1;
            break;
    }
}

TARPC_FUNC(setsockopt,
{},
{
    if (in->optval.optval_val == NULL)
    {
        MAKE_CALL(out->retval = func(in->s, socklevel_rpc2h(in->level),
                                     sockopt_rpc2h(in->optname),
                                     in->raw_optval.raw_optval_val,
                                     in->raw_optlen));
    }
    else
    {
        sockopt_param  opt;
        socklen_t      optlen;

        tarpc_setsockopt(in, out, &opt, &optlen);
        if (out->retval == 0)
        {
            uint8_t    *val;
            socklen_t   len;

            if (in->raw_optval.raw_optval_val != NULL)
            {
                len = optlen + in->raw_optlen;
                val = malloc(len);
                assert(val != NULL);
                memcpy(val, &opt, optlen);
                memcpy(val + optlen, in->raw_optval.raw_optval_val,
                       in->raw_optval.raw_optval_len);
            }
            else
            {
                len = optlen;
                val = (uint8_t *)&opt;
            }

            INIT_CHECKED_ARG(val, len, 0);

            MAKE_CALL(out->retval = func(in->s, socklevel_rpc2h(in->level),
                                         sockopt_rpc2h(in->optname),
                                         val, len));

            if (val != (uint8_t *)&opt)
                free(val);
        }
    }
}
)


/*-------------- getsockopt() ------------------------------*/

#define COPY_TCP_INFO_FIELD(_name) \
    do {                                          \
        out->optval.optval_val[0].option_value_u. \
            opt_tcp_info._name = info->_name;     \
    } while (0)

#define CONVERT_TCP_INFO_FIELD(_name, _func) \
    do {                                             \
        out->optval.optval_val[0].option_value_u.    \
            opt_tcp_info._name = _func(info->_name); \
    } while (0)

static socklen_t
tarpc_sockoptlen(const option_value *optval)
{
    switch (optval->opttype)
    {
        case OPT_INT:
            return sizeof(int);

        case OPT_TIMEVAL:
            return sizeof(struct timeval);

        case OPT_LINGER:
            return sizeof(struct linger);

        case OPT_MREQN:
#if HAVE_STRUCT_IP_MREQN
            return sizeof(struct ip_mreqn);
#endif

        case OPT_MREQ:
            return sizeof(struct ip_mreq);

        case OPT_MREQ_SOURCE:
            return sizeof(struct ip_mreq_source);

        case OPT_MREQ6:
            return sizeof(struct ipv6_mreq);

        case OPT_IPADDR:
            return sizeof(struct in_addr);

        case OPT_IPADDR6:
            return sizeof(struct in6_addr);

#if HAVE_STRUCT_TCP_INFO
        case OPT_TCP_INFO:
            return sizeof(struct tcp_info);
#endif

        default:
            ERROR("incorrect option type %d is received",
                  optval->opttype);
            return 0;
    }
}

static void
tarpc_getsockopt(tarpc_getsockopt_in *in, tarpc_getsockopt_out *out,
                 const void *opt, socklen_t optlen)
{
    option_value   *out_optval = out->optval.optval_val;

    if (out_optval->opttype == OPT_MREQN
#if HAVE_STRUCT_IP_MREQN
        && optlen < (socklen_t)sizeof(struct ip_mreqn)
#endif
        )
    {
        out_optval->opttype = OPT_MREQ;
    }
    if (out_optval->opttype == OPT_MREQ &&
        optlen < (socklen_t)sizeof(struct ip_mreq))
    {
        out_optval->opttype = OPT_IPADDR;
    }

    switch (out_optval->opttype)
    {
        case OPT_INT:
        {
            /*
             * SO_ERROR socket option keeps the value of the last
             * pending error occurred on the socket, so that we should
             * convert its value to host independend representation,
             * which is RPC errno
             */
            if (in->level == RPC_SOL_SOCKET &&
                in->optname == RPC_SO_ERROR)
            {
                *(int *)opt = errno_h2rpc(*(int *)opt);
            }
            /*
             * SO_TYPE and SO_STYLE socket option keeps the value of
             * socket type they are called for, so that we should
             * convert its value to host independend representation,
             * which is RPC socket type
             */
            else if (in->level == RPC_SOL_SOCKET &&
                     in->optname == RPC_SO_TYPE)
            {
                *(int *)opt = socktype_h2rpc(*(int *)opt);
            }
            else if (in->level == RPC_SOL_SOCKET &&
                     in->optname == RPC_SO_PROTOCOL)
            {
                *(int *)opt = proto_h2rpc(*(int *)opt);
            }
            else if (in->level == RPC_SOL_IP &&
                     in->optname == RPC_IP_MTU_DISCOVER)
            {
                *(int *)opt = mtu_discover_arg_h2rpc(*(int *)opt);
            }

            out_optval->option_value_u.opt_int = *(int *)opt;
            break;
        }

        case OPT_TIMEVAL:
        {
            struct timeval *tv = (struct timeval *)opt;

            out_optval->option_value_u.opt_timeval.tv_sec = tv->tv_sec;
            out_optval->option_value_u.opt_timeval.tv_usec = tv->tv_usec;
            break;
        }

        case OPT_LINGER:
        {
            struct linger *linger = (struct linger *)opt;

            out_optval->option_value_u.opt_linger.l_onoff =
                linger->l_onoff;
            out_optval->option_value_u.opt_linger.l_linger =
                linger->l_linger;
            break;
        }

        case OPT_MREQN:
        {
#if HAVE_STRUCT_IP_MREQN
            struct ip_mreqn *mreqn = (struct ip_mreqn *)opt;

            memcpy(&out_optval->option_value_u.opt_mreqn.imr_multiaddr,
                   &(mreqn->imr_multiaddr), sizeof(mreqn->imr_multiaddr));
            out_optval->option_value_u.opt_mreqn.imr_multiaddr =
                ntohl(out_optval->option_value_u.opt_mreqn.imr_multiaddr);
            memcpy(&out_optval->option_value_u.opt_mreqn.imr_address,
                   &(mreqn->imr_address), sizeof(mreqn->imr_address));
            out_optval->option_value_u.opt_mreqn.imr_address =
                ntohl(out_optval->option_value_u.opt_mreqn.imr_address);
            out_optval->option_value_u.opt_mreqn.imr_ifindex =
                mreqn->imr_ifindex;
#else
            ERROR("'struct ip_mreqn' is not defined");
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
            out->retval = -1;
#endif
            break;
        }

        case OPT_MREQ:
        {
            struct ip_mreq *mreq = (struct ip_mreq *)opt;

            memcpy(&out_optval->option_value_u.opt_mreq.imr_multiaddr,
                   &(mreq->imr_multiaddr), sizeof(mreq->imr_multiaddr));
            out_optval->option_value_u.opt_mreq.imr_multiaddr =
                ntohl(out_optval->option_value_u.opt_mreq.imr_multiaddr);
            memcpy(&out_optval->option_value_u.opt_mreq.imr_address,
                   &(mreq->imr_interface), sizeof(mreq->imr_interface));
            out_optval->option_value_u.opt_mreq.imr_address =
                ntohl(out_optval->option_value_u.opt_mreq.imr_address);
            break;
        }

        case OPT_MREQ_SOURCE:
        {
            struct ip_mreq_source *mreq = (struct ip_mreq_source *)opt;

            memcpy(&out_optval->option_value_u.opt_mreq_source.
                   imr_multiaddr,
                   &(mreq->imr_multiaddr), sizeof(mreq->imr_multiaddr));
            out_optval->option_value_u.opt_mreq_source.imr_multiaddr =
                ntohl(out_optval->option_value_u.opt_mreq_source.
                      imr_multiaddr);

            memcpy(&out_optval->option_value_u.opt_mreq_source.
                   imr_interface,
                   &(mreq->imr_interface), sizeof(mreq->imr_interface));
            out_optval->option_value_u.opt_mreq_source.imr_interface =
                ntohl(out_optval->option_value_u.opt_mreq_source.
                      imr_interface);

            memcpy(&out_optval->option_value_u.opt_mreq_source.
                   imr_sourceaddr,
                   &(mreq->imr_sourceaddr), sizeof(mreq->imr_sourceaddr));
            out_optval->option_value_u.opt_mreq_source.imr_sourceaddr =
                ntohl(out_optval->option_value_u.opt_mreq_source.
                      imr_sourceaddr);
            break;
        }

        case OPT_MREQ6:
        {
            struct ipv6_mreq *mreq6 = (struct ipv6_mreq *)opt;

            memcpy(&out_optval->option_value_u.opt_mreq6.ipv6mr_multiaddr,
                   &(mreq6->ipv6mr_multiaddr), sizeof(struct ipv6_mreq));
            out_optval->option_value_u.opt_mreq6.ipv6mr_ifindex =
                mreq6->IPV6MR_IFINDEX;
            break;
        }

        case OPT_IPADDR:
            memcpy(&out_optval->option_value_u.opt_ipaddr,
                   opt, sizeof(struct in_addr));
            out_optval->option_value_u.opt_ipaddr =
                ntohl(out_optval->option_value_u.opt_ipaddr);
            break;

        case OPT_IPADDR6:
            memcpy(out_optval->option_value_u.opt_ipaddr6,
                   opt, sizeof(struct in6_addr));
            break;

        case OPT_TCP_INFO:
        {
#if HAVE_STRUCT_TCP_INFO
            struct tcp_info *info = (struct tcp_info *)opt;

            CONVERT_TCP_INFO_FIELD(tcpi_state, tcp_state_h2rpc);
            CONVERT_TCP_INFO_FIELD(tcpi_ca_state, tcp_ca_state_h2rpc);
            COPY_TCP_INFO_FIELD(tcpi_retransmits);
            COPY_TCP_INFO_FIELD(tcpi_probes);
            COPY_TCP_INFO_FIELD(tcpi_backoff);
            CONVERT_TCP_INFO_FIELD(tcpi_options, tcpi_options_h2rpc);
            COPY_TCP_INFO_FIELD(tcpi_snd_wscale);
            COPY_TCP_INFO_FIELD(tcpi_rcv_wscale);
            COPY_TCP_INFO_FIELD(tcpi_rto);
            COPY_TCP_INFO_FIELD(tcpi_ato);
            COPY_TCP_INFO_FIELD(tcpi_snd_mss);
            COPY_TCP_INFO_FIELD(tcpi_rcv_mss);
            COPY_TCP_INFO_FIELD(tcpi_unacked);
            COPY_TCP_INFO_FIELD(tcpi_sacked);
            COPY_TCP_INFO_FIELD(tcpi_lost);
            COPY_TCP_INFO_FIELD(tcpi_retrans);
            COPY_TCP_INFO_FIELD(tcpi_fackets);
            COPY_TCP_INFO_FIELD(tcpi_last_data_sent);
            COPY_TCP_INFO_FIELD(tcpi_last_ack_sent);
            COPY_TCP_INFO_FIELD(tcpi_last_data_recv);
            COPY_TCP_INFO_FIELD(tcpi_last_ack_recv);
            COPY_TCP_INFO_FIELD(tcpi_pmtu);
            COPY_TCP_INFO_FIELD(tcpi_rcv_ssthresh);
            COPY_TCP_INFO_FIELD(tcpi_rtt);
            COPY_TCP_INFO_FIELD(tcpi_rttvar);
            COPY_TCP_INFO_FIELD(tcpi_snd_ssthresh);
            COPY_TCP_INFO_FIELD(tcpi_snd_cwnd);
            COPY_TCP_INFO_FIELD(tcpi_advmss);
            COPY_TCP_INFO_FIELD(tcpi_reordering);
#if HAVE_STRUCT_TCP_INFO_TCPI_RCV_RTT
            COPY_TCP_INFO_FIELD(tcpi_rcv_rtt);
            COPY_TCP_INFO_FIELD(tcpi_rcv_space);
            COPY_TCP_INFO_FIELD(tcpi_total_retrans);
#endif

#else
            ERROR("'struct tcp_info' is not defined");
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
            out->retval = -1;
#endif
            break;
        }

        case OPT_IP_PKTOPTIONS:
        {
#define OPTVAL out_optval->option_value_u.opt_ip_pktoptions. \
                                            opt_ip_pktoptions_val
#define OPTLEN out_optval->option_value_u.opt_ip_pktoptions. \
                                            opt_ip_pktoptions_len
            if (optlen > 0)
            {
                int                  rc;

                rc = msg_control_h2rpc((uint8_t *)opt, optlen,
                                       &OPTVAL, &OPTLEN);
                if (rc != 0)
                {
                    ERROR("Failed to process IP_PKTOPTIONS value");
                    out->retval = -1;
                    out->common._errno = TE_RC(TE_TA_UNIX, rc);
                    break;
                }
            }

            break;
#undef OPTVAL
#undef OPTLEN
        }

        default:
            ERROR("incorrect option type %d is received",
                  out_optval->opttype);
            break;
    }
}

TARPC_FUNC(getsockopt,
{
    COPY_ARG(optval);
    COPY_ARG(raw_optval);
    COPY_ARG(raw_optlen);
},
{
    if (out->optval.optval_val == NULL)
    {

        INIT_CHECKED_ARG(out->raw_optval.raw_optval_val,
                         out->raw_optval.raw_optval_len,
                         out->raw_optlen.raw_optlen_val == NULL ? 0 :
                                        *(out->raw_optlen.raw_optlen_val));

        MAKE_CALL(out->retval = func(in->s, socklevel_rpc2h(in->level),
                                     sockopt_rpc2h(in->optname),
                                     out->raw_optval.raw_optval_val,
                                     out->raw_optlen.raw_optlen_val));

        if (in->level == RPC_SOL_IP && in->optname == RPC_IP_PKTOPTIONS)
        {
            out->optval.optval_len = 1;
            out->optval.optval_val = calloc(1,
                                            sizeof(struct option_value));
            assert(out->optval.optval_val != NULL);

            out->optval.optval_val[0].opttype = OPT_IP_PKTOPTIONS;
            out->optval.optval_val[0].option_value_u.opt_ip_pktoptions.
                        opt_ip_pktoptions_val = NULL;
            out->optval.optval_val[0].option_value_u.opt_ip_pktoptions.
                        opt_ip_pktoptions_len = 0;

            if (out->retval >= 0)
                tarpc_getsockopt(in, out, out->raw_optval.raw_optval_val,
                                 out->raw_optlen.raw_optlen_val == NULL ?
                                 0 : *(out->raw_optlen.raw_optlen_val));
        }
    }
    else
    {
        socklen_t   optlen = tarpc_sockoptlen(out->optval.optval_val);
        socklen_t   rlen = optlen + out->raw_optval.raw_optval_len;
        socklen_t   len = optlen +
                          (out->raw_optlen.raw_optlen_val == NULL ? 0 :
                               *out->raw_optlen.raw_optlen_val);
        void       *buf = calloc(1, rlen);

        assert(buf != NULL);
        INIT_CHECKED_ARG(buf, rlen, len);

        MAKE_CALL(out->retval =
                      func(in->s, socklevel_rpc2h(in->level),
                           sockopt_rpc2h(in->optname), buf, &len));

        tarpc_getsockopt(in, out, buf, len);
        free(buf);
    }

}
)

#undef COPY_TCP_INFO_FIELD
#undef CONVERT_TCP_INFO_FIELD

/*-------------- pselect() --------------------------------*/

TARPC_FUNC(pselect,
{
    COPY_ARG(timeout);
},
{
    out->retval = -1;
    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_FD_SET, {
        struct timespec tv;

        if (out->timeout.timeout_len > 0)
        {
            tv.tv_sec = out->timeout.timeout_val[0].tv_sec;
            tv.tv_nsec = out->timeout.timeout_val[0].tv_nsec;
        }
        if (out->common._errno == 0)
        {
            fd_set *rfds = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->readfds, ns);
            fd_set *wfds = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->writefds, ns);
            fd_set *efds = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->exceptfds, ns);
            sigset_t *sigmask = rcf_pch_mem_get(in->sigmask);
            /*
             * The pointer may be a NULL and, therefore, contain
             * uninitialized data, but we want to check that the data are
             * unchanged even in this case.
             */
            INIT_CHECKED_ARG(sigmask, sizeof(sigset_t), 0);

            MAKE_CALL(out->retval = func(in->n, rfds, wfds, efds,
                            out->timeout.timeout_len == 0 ? NULL : &tv,
                            sigmask));

            if (out->timeout.timeout_len > 0)
            {
                out->timeout.timeout_val[0].tv_sec = tv.tv_sec;
                out->timeout.timeout_val[0].tv_nsec = tv.tv_nsec;
            }
        }

#ifdef __linux__
        if (out->retval >= 0 && out->common.errno_changed &&
            out->common._errno == RPC_ENOSYS)
        {
            WARN("pselect() returned non-negative value, but changed "
                 "errno to ENOSYS");
            out->common.errno_changed = 0;
        }
#endif
    });
}
)

/*-------------- fcntl() --------------------------------*/

TARPC_FUNC(fcntl,
{
    COPY_ARG(arg);
},
{
    long int_arg;

    if (in->cmd == RPC_F_GETFD || in->cmd == RPC_F_GETFL ||
        in->cmd == RPC_F_GETSIG
#if defined (F_GETPIPE_SZ)
        || in->cmd == RPC_F_GETPIPE_SZ
#endif
        )
        MAKE_CALL(out->retval = func(in->fd, fcntl_rpc2h(in->cmd)));
#if defined (F_GETOWN_EX) || defined (F_SETOWN_EX)
    else if (in->cmd == RPC_F_GETOWN_EX || in->cmd == RPC_F_SETOWN_EX)
    {
        struct f_owner_ex foex_arg;

        foex_arg.type =
            out->arg.arg_val[0].fcntl_request_u.req_f_owner_ex.type;
        foex_arg.pid =
            out->arg.arg_val[0].fcntl_request_u.req_f_owner_ex.pid;
        MAKE_CALL(out->retval = func(in->fd, fcntl_rpc2h(in->cmd),
                                     &foex_arg));
        out->arg.arg_val[0].fcntl_request_u.req_f_owner_ex.type =
            foex_arg.type;
        out->arg.arg_val[0].fcntl_request_u.req_f_owner_ex.pid =
            foex_arg.pid;
    }
#endif
    else
    {
        int_arg = out->arg.arg_val[0].fcntl_request_u.req_int;
        if (in->cmd == RPC_F_SETFL)
            int_arg = fcntl_flags_rpc2h(int_arg);
        else if (in->cmd == RPC_F_SETSIG)
            int_arg = signum_rpc2h(int_arg);
        MAKE_CALL(out->retval =
                    func(in->fd, fcntl_rpc2h(in->cmd), int_arg));
    }

    if (in->cmd == RPC_F_GETFL)
        out->retval = fcntl_flags_h2rpc(out->retval);
    else if (in->cmd == RPC_F_GETSIG)
        out->retval = signum_h2rpc(out->retval);
}
)


/*-------------- ioctl() --------------------------------*/

typedef union ioctl_param {
    int              integer;
    struct timeval   tv;
    struct timespec  ts;
    struct ifreq     ifreq;
    struct ifconf    ifconf;
    struct arpreq    arpreq;
#ifdef HAVE_STRUCT_SG_IO_HDR
    struct sg_io_hdr sg;
#endif
} ioctl_param;

static void
tarpc_ioctl_pre(tarpc_ioctl_in *in, tarpc_ioctl_out *out,
                ioctl_param *req, checked_arg_list *arglist)
{
    size_t  reqlen;

    assert(in != NULL);
    assert(out != NULL);
    assert(req != NULL);

    switch (out->req.req_val[0].type)
    {
        case IOCTL_INT:
            reqlen = sizeof(int);
            req->integer = out->req.req_val[0].ioctl_request_u.req_int;
            break;

        case IOCTL_TIMEVAL:
            reqlen = sizeof(struct timeval);
            req->tv.tv_sec =
                out->req.req_val[0].ioctl_request_u.req_timeval.tv_sec;
            req->tv.tv_usec =
                out->req.req_val[0].ioctl_request_u.req_timeval.tv_usec;
            break;

        case IOCTL_TIMESPEC:
            reqlen = sizeof(struct timespec);
            req->ts.tv_sec =
                out->req.req_val[0].ioctl_request_u.req_timespec.tv_sec;
            req->ts.tv_nsec =
                out->req.req_val[0].ioctl_request_u.req_timespec.tv_nsec;
            break;

        case IOCTL_IFREQ:
        {
            reqlen = sizeof(struct ifreq);

            /* Copy the whole 'ifr_name' buffer, not just strcpy() */
            memcpy(req->ifreq.ifr_name,
                   out->req.req_val[0].ioctl_request_u.req_ifreq.
                       rpc_ifr_name.rpc_ifr_name_val,
                   sizeof(req->ifreq.ifr_name));

            if (in->code != RPC_SIOCGIFNAME)
                INIT_CHECKED_ARG(req->ifreq.ifr_name,
                                 strlen(req->ifreq.ifr_name) + 1, 0);

            switch (in->code)
            {
                case RPC_SIOCSIFFLAGS:
                    req->ifreq.ifr_flags =
                        if_fl_rpc2h((uint32_t)(unsigned short int)
                            out->req.req_val[0].ioctl_request_u.
                                req_ifreq.rpc_ifr_flags);
                    break;

                    /* QNX does not support SIOCGIFNAME */
#ifdef SIOCGIFNAME
                case RPC_SIOCGIFNAME:
#if SOLARIS
                    req->ifreq.ifr_index =
                        out->req.req_val[0].ioctl_request_u.
                        req_ifreq.rpc_ifr_ifindex;
#else
                    req->ifreq.ifr_ifindex =
                        out->req.req_val[0].ioctl_request_u.
                        req_ifreq.rpc_ifr_ifindex;
#endif
                    break;
#endif /* SIOCGIFNAME */

                case RPC_SIOCSIFMTU:
#if HAVE_STRUCT_IFREQ_IFR_MTU
                    req->ifreq.ifr_mtu =
                        out->req.req_val[0].ioctl_request_u.
                        req_ifreq.rpc_ifr_mtu;
#else
                    WARN("'struct ifreq' has no 'ifr_mtu'");
#endif
                    break;

                case RPC_SIOCSIFADDR:
                case RPC_SIOCSIFNETMASK:
                case RPC_SIOCSIFBRDADDR:
                case RPC_SIOCSIFDSTADDR:
                    sockaddr_rpc2h(&(out->req.req_val[0].
                        ioctl_request_u.req_ifreq.rpc_ifr_addr),
                        &(req->ifreq.ifr_addr),
                        sizeof(req->ifreq.ifr_addr), NULL, NULL);
                   break;

#if HAVE_LINUX_ETHTOOL_H
                case RPC_SIOCETHTOOL:
                    ethtool_data_rpc2h(
                                    &(out->req.req_val[0].ioctl_request_u.
                                      req_ifreq.rpc_ifr_ethtool),
                                    &req->ifreq.ifr_data);
                    break;
#endif /* HAVE_LINUX_ETHTOOL_H */
#if HAVE_LINUX_NET_TSTAMP_H
                case RPC_SIOCSHWTSTAMP:
                case RPC_SIOCGHWTSTAMP:
                    hwtstamp_config_data_rpc2h(
                                    &(out->req.req_val[0].ioctl_request_u.
                                      req_ifreq.rpc_ifr_hwstamp),
                                    &req->ifreq.ifr_data);
                    break;
#endif
            }
            break;
        }
        case IOCTL_IFCONF:
        {
            char *buf = NULL;
            int   buflen = out->req.req_val[0].ioctl_request_u.
                           req_ifconf.nmemb * sizeof(struct ifreq) +
                           out->req.req_val[0].ioctl_request_u.
                           req_ifconf.extra;

            reqlen = sizeof(req->ifconf);

            if (buflen > 0 && (buf = calloc(1, buflen + 64)) == NULL)
            {
                ERROR("Out of memory");
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                return;
            }
            req->ifconf.ifc_buf = buf;
            req->ifconf.ifc_len = buflen;

            if (buf != NULL)
                INIT_CHECKED_ARG(buf, buflen + 64, buflen);
            break;
        }

        case IOCTL_ARPREQ:
            reqlen = sizeof(req->arpreq);

            /* Copy protocol address for all requests */
            sockaddr_rpc2h(&(out->req.req_val[0].ioctl_request_u.
                                 req_arpreq.rpc_arp_pa),
                           &(req->arpreq.arp_pa),
                           sizeof(req->arpreq.arp_pa), NULL, NULL);
            if (in->code == RPC_SIOCSARP)
            {
                /* Copy HW address */
                sockaddr_rpc2h(&(out->req.req_val[0].ioctl_request_u.
                                     req_arpreq.rpc_arp_ha),
                               &(req->arpreq.arp_ha),
                               sizeof(req->arpreq.arp_ha), NULL, NULL);
                /* Copy ARP flags */
                req->arpreq.arp_flags =
                    arp_fl_rpc2h(out->req.req_val[0].ioctl_request_u.
                                     req_arpreq.rpc_arp_flags);
            }

#if HAVE_STRUCT_ARPREQ_ARP_DEV
            if (in->code == RPC_SIOCGARP)
            {
                /* Copy device */
                strcpy(req->arpreq.arp_dev,
                       out->req.req_val[0].ioctl_request_u.
                           req_arpreq.rpc_arp_dev.rpc_arp_dev_val);
            }
#endif
            break;
#ifdef HAVE_STRUCT_SG_IO_HDR
        case IOCTL_SGIO:
            {
                size_t psz = getpagesize();
                reqlen = sizeof(struct sg_io_hdr);

                req->sg.interface_id = out->req.req_val[0].ioctl_request_u.
                    req_sgio.interface_id;
                req->sg.dxfer_direction = out->req.req_val[0].
                    ioctl_request_u.req_sgio.dxfer_direction;
                req->sg.cmd_len = out->req.req_val[0].ioctl_request_u.
                    req_sgio.cmd_len;
                req->sg.mx_sb_len = out->req.req_val[0].ioctl_request_u.
                    req_sgio.mx_sb_len;
                req->sg.iovec_count = out->req.req_val[0].ioctl_request_u.
                    req_sgio.iovec_count;

                req->sg.dxfer_len = out->req.req_val[0].ioctl_request_u.
                    req_sgio.dxfer_len;

                req->sg.flags = out->req.req_val[0].ioctl_request_u.
                    req_sgio.flags;

                req->sg.dxferp = calloc(req->sg.dxfer_len + psz, 1);
                if ((req->sg.flags & SG_FLAG_DIRECT_IO) ==
                    SG_FLAG_DIRECT_IO)
                {
                    req->sg.dxferp =
                        (unsigned char *)
                        (((unsigned long)req->sg.dxferp + psz - 1) &
                                          (~(psz - 1)));
                }
                memcpy(req->sg.dxferp, out->req.req_val[0].ioctl_request_u.
                       req_sgio.dxferp.dxferp_val,
                       req->sg.dxfer_len);

                req->sg.cmdp = calloc(req->sg.cmd_len, 1);
                memcpy(req->sg.cmdp, out->req.req_val[0].ioctl_request_u.
                       req_sgio.cmdp.cmdp_val,
                       req->sg.cmd_len);

                req->sg.sbp = calloc(req->sg.mx_sb_len, 1);
                memcpy(req->sg.sbp, out->req.req_val[0].ioctl_request_u.
                       req_sgio.sbp.sbp_val,
                       req->sg.mx_sb_len);


                req->sg.timeout = out->req.req_val[0].ioctl_request_u.
                    req_sgio.timeout;
                req->sg.pack_id = out->req.req_val[0].ioctl_request_u.
                    req_sgio.pack_id;

                break;
            }
#endif /* HAVE_STRUCT_SG_IO_HDR */
        default:
            ERROR("Incorrect request type %d is received",
                  out->req.req_val[0].type);
            out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
            return;
    }
    if (in->access == IOCTL_WR)
        INIT_CHECKED_ARG(req, reqlen, 0);
}

static void
tarpc_ioctl_post(tarpc_ioctl_in *in, tarpc_ioctl_out *out,
                 ioctl_param *req)
{
    switch (out->req.req_val[0].type)
    {
        case IOCTL_INT:
            out->req.req_val[0].ioctl_request_u.req_int = req->integer;
            break;

        case IOCTL_TIMEVAL:
            out->req.req_val[0].ioctl_request_u.req_timeval.tv_sec =
                req->tv.tv_sec;
            out->req.req_val[0].ioctl_request_u.req_timeval.tv_usec =
                req->tv.tv_usec;
            break;

        case IOCTL_TIMESPEC:
            out->req.req_val[0].ioctl_request_u.req_timespec.tv_sec =
                req->ts.tv_sec;
            out->req.req_val[0].ioctl_request_u.req_timespec.tv_nsec =
                req->ts.tv_nsec;
            break;

        case IOCTL_IFREQ:
            switch (in->code)
            {
                case RPC_SIOCGIFFLAGS:
                case RPC_SIOCSIFFLAGS:
                    out->req.req_val[0].ioctl_request_u.req_ifreq.
                        rpc_ifr_flags = if_fl_h2rpc(
                            (uint32_t)(unsigned short int)
                                req->ifreq.ifr_flags);
                    break;

                case RPC_SIOCGIFMTU:
                case RPC_SIOCSIFMTU:
#if HAVE_STRUCT_IFREQ_IFR_MTU
                    out->req.req_val[0].ioctl_request_u.req_ifreq.
                        rpc_ifr_mtu = req->ifreq.ifr_mtu;
#else
                    WARN("'struct ifreq' has no 'ifr_mtu'");
#endif
                    break;

                case RPC_SIOCGIFNAME:
                    memcpy(out->req.req_val[0].ioctl_request_u.req_ifreq.
                           rpc_ifr_name.rpc_ifr_name_val,
                           req->ifreq.ifr_name,
                           sizeof(req->ifreq.ifr_name));
                    out->req.req_val[0].ioctl_request_u.req_ifreq.
                            rpc_ifr_name.rpc_ifr_name_len =
                            sizeof(req->ifreq.ifr_name);
                    break;

                    /* QNX does not support SIOCGIFINDEX */
#ifdef SIOCGIFINDEX
                case RPC_SIOCGIFINDEX:
#if SOLARIS
                    out->req.req_val[0].ioctl_request_u.req_ifreq.
                            rpc_ifr_ifindex = req->ifreq.ifr_index;
#else
                    out->req.req_val[0].ioctl_request_u.req_ifreq.
                            rpc_ifr_ifindex = req->ifreq.ifr_ifindex;
#endif
                    break;
#endif /* SIOCGIFINDEX */

                case RPC_SIOCGIFADDR:
                case RPC_SIOCSIFADDR:
                case RPC_SIOCGIFNETMASK:
                case RPC_SIOCSIFNETMASK:
                case RPC_SIOCGIFBRDADDR:
                case RPC_SIOCSIFBRDADDR:
                case RPC_SIOCGIFDSTADDR:
                case RPC_SIOCSIFDSTADDR:
                case RPC_SIOCGIFHWADDR:
                    TE_IOCTL_AF_LOCAL2ETHER(req->ifreq.ifr_addr.sa_family);
                    sockaddr_output_h2rpc(&(req->ifreq.ifr_addr),
                                          sizeof(req->ifreq.ifr_addr),
                                          sizeof(req->ifreq.ifr_addr),
                                          &(out->req.req_val[0].
                                              ioctl_request_u.
                                              req_ifreq.rpc_ifr_addr));
                    break;

#if HAVE_LINUX_ETHTOOL_H
                case RPC_SIOCETHTOOL:
                    ethtool_data_h2rpc(
                                    &(out->req.req_val[0].ioctl_request_u.
                                      req_ifreq.rpc_ifr_ethtool),
                                    req->ifreq.ifr_data);
                    free(req->ifreq.ifr_data);
                    break;
#endif /* HAVE_LINUX_ETHTOOL_H */
#if HAVE_LINUX_NET_TSTAMP_H
                case RPC_SIOCSHWTSTAMP:
                case RPC_SIOCGHWTSTAMP:
                    hwtstamp_config_data_h2rpc(
                                    &(out->req.req_val[0].ioctl_request_u.
                                      req_ifreq.rpc_ifr_hwstamp),
                                    req->ifreq.ifr_data);
                    free(req->ifreq.ifr_data);
                    break;
#endif /* HAVE_LINUX_NET_TSTAMP_H */
                default:
                    ERROR("Unsupported IOCTL request %d of type IFREQ",
                          in->code);
                    out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
                    return;
            }
            break;

        case IOCTL_IFCONF:
        {
            struct ifreq       *req_c;
            struct tarpc_ifreq *req_t;

            int n = 1;
            int i;

            n = out->req.req_val[0].ioctl_request_u.req_ifconf.nmemb =
                req->ifconf.ifc_len / sizeof(struct ifreq);
            out->req.req_val[0].ioctl_request_u.req_ifconf.extra =
                req->ifconf.ifc_len % sizeof(struct ifreq);

            if (req->ifconf.ifc_req == NULL)
                break;

            if ((req_t = calloc(n, sizeof(*req_t))) == NULL)
            {
                free(req->ifconf.ifc_buf);
                ERROR("Out of memory");
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                return;
            }
            out->req.req_val[0].ioctl_request_u.req_ifconf.
                rpc_ifc_req.rpc_ifc_req_val = req_t;
            out->req.req_val[0].ioctl_request_u.req_ifconf.
                rpc_ifc_req.rpc_ifc_req_len = n;
            req_c = ((struct ifconf *)req)->ifc_req;

            for (i = 0; i < n; i++, req_t++, req_c++)
            {
                req_t->rpc_ifr_name.rpc_ifr_name_val =
                    calloc(1, sizeof(req_c->ifr_name));
                if (req_t->rpc_ifr_name.rpc_ifr_name_val == NULL)
                {
                    free(req->ifconf.ifc_buf);
                    ERROR("Out of memory");
                    out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                    return;
                }
                memcpy(req_t->rpc_ifr_name.rpc_ifr_name_val,
                       req_c->ifr_name, sizeof(req_c->ifr_name));
                req_t->rpc_ifr_name.rpc_ifr_name_len =
                    sizeof(req_c->ifr_name);

                sockaddr_output_h2rpc(&(req_c->ifr_addr),
                                      sizeof(req_c->ifr_addr),
                                      sizeof(req_c->ifr_addr),
                                      &(req_t->rpc_ifr_addr));
            }
            free(req->ifconf.ifc_buf);
            break;
        }

        case IOCTL_ARPREQ:
        {
            if (in->code == RPC_SIOCGARP)
            {
                /* Copy protocol address */
                sockaddr_output_h2rpc(&(req->arpreq.arp_pa),
                                      sizeof(req->arpreq.arp_pa),
                                      sizeof(req->arpreq.arp_pa),
                                      &(out->req.req_val[0].ioctl_request_u.
                                          req_arpreq.rpc_arp_pa));
                TE_IOCTL_AF_LOCAL2ETHER(req->arpreq.arp_ha.sa_family);
                /* Copy HW address */
                sockaddr_output_h2rpc(&(req->arpreq.arp_ha),
                                      sizeof(req->arpreq.arp_ha),
                                      sizeof(req->arpreq.arp_ha),
                                      &(out->req.req_val[0].ioctl_request_u.
                                          req_arpreq.rpc_arp_ha));

                /* Copy flags */
                out->req.req_val[0].ioctl_request_u.req_arpreq.
                    rpc_arp_flags = arp_fl_h2rpc(req->arpreq.arp_flags);
            }
            break;
        }
#ifdef HAVE_STRUCT_SG_IO_HDR
        case IOCTL_SGIO:
        {
            out->req.req_val[0].ioctl_request_u.req_sgio.
                status = req->sg.status;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                masked_status = req->sg.masked_status;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                msg_status = req->sg.msg_status;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                sb_len_wr = req->sg.sb_len_wr;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                host_status = req->sg.host_status;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                driver_status = req->sg.driver_status;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                resid = req->sg.resid;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                duration = req->sg.duration;
            out->req.req_val[0].ioctl_request_u.req_sgio.
                info = req->sg.info;
            break;
        }
#endif /* HAVE_STRUCT_SG_IO_HDR */
        default:
            assert(FALSE);
    }
}

TARPC_FUNC(ioctl,
{
    COPY_ARG(req);
},
{
    ioctl_param  req_local;
    void        *req_ptr;

    if (out->req.req_val != NULL)
    {
        memset(&req_local, 0, sizeof(req_local));
        req_ptr = &req_local;
        tarpc_ioctl_pre(in, out, req_ptr, arglist);
        if (out->common._errno != 0)
            goto finish;
    }
    else
    {
        req_ptr = NULL;
    }

    MAKE_CALL(out->retval = func(in->s, ioctl_rpc2h(in->code), req_ptr));
    if (req_ptr != NULL)
    {
        tarpc_ioctl_post(in, out, req_ptr);
    }

finish:
    ;
}
)


static const char *
msghdr2str(const struct msghdr *msg)
{
    static char buf[256];

    char   *buf_end = buf + sizeof(buf);
    char   *p = buf;
    int     i;

    p += snprintf(p, buf_end - p, "{name={0x%lx,%u},{",
                  (long unsigned int)msg->msg_name, msg->msg_namelen);
    if (p >= buf_end)
        return "(too long)";
    for (i = 0; i < (int)msg->msg_iovlen; ++i)
    {
        p += snprintf(p, buf_end - p, "%s{0x%lx,%u}",
                      (i == 0) ? "" : ",",
                      (long unsigned int)msg->msg_iov[i].iov_base,
                      (unsigned int)msg->msg_iov[i].iov_len);
        if (p >= buf_end)
            return "(too long)";
    }
    p += snprintf(p, buf_end - p, "},control={0x%lx,%u},flags=0x%x}",
                  (unsigned long int)msg->msg_control,
                  (unsigned int)msg->msg_controllen,
                  (unsigned int)msg->msg_flags);
    if (p >= buf_end)
        return "(too long)";

    return buf;
}

struct mmsghdr_alt {
    struct msghdr msg_hdr;  /* Message header */
    unsigned int  msg_len;  /* Number of received bytes for header */
};

static const char *
mmsghdr2str(const struct mmsghdr_alt *mmsg, int len)
{
    int          i;
    static char  buf[256];
    char        *buf_end = buf + sizeof(buf);
    char        *p = buf;

    for (i = 0; i < len; i++)
    {
        p += snprintf(p, buf_end - p, "%s{%s, %d}%s%s",
                      (i == 0) ? "{" : "",
                      msghdr2str(&mmsg[i].msg_hdr), mmsg[i].msg_len,
                      (i == 0) ? "" : ",", (i == len - 1) ? "" : "}");
        if (p >= buf_end)
            return "(too long)";
    }
    return buf;
}

/** Calculate the auxiliary buffer length for msghdr */
static inline int
calculate_msg_controllen(struct tarpc_msghdr *rpc_msg)
{
    unsigned int i;
    int          len = 0;

    for (i = 0; i < rpc_msg->msg_control.msg_control_len; i++)
        len += CMSG_SPACE(rpc_msg->msg_control.msg_control_val[i].
                          data.data_len);

    return len;
}

/*-------------- sendmsg() ------------------------------*/

/**
 * Release memory allocated for value of type msghdr.
 *
 * @param msg   Pointer to memory to be released
 */
static void
msghdr_free(struct msghdr *msg)
{
    free(msg->msg_iov);
    free(msg->msg_control);
    free(msg->msg_name);
}

/**
 * Transform value of tarpc_msghdr type into value of msghdr type.
 *
 * @param tarpc_msg_    Value of tarpc_msghdr type (handle)
 * @param msg_          Value of msghdr type (handle)
 *
 * @return Status code
 */
#define MSGHDR_TARPC2H(tarpc_msg_, msg_) \
    do {                                                                \
        struct iovec    *iovec_arr;                                     \
                                                                        \
        iovec_arr = TE_ALLOC(sizeof(*iovec_arr) * RCF_RPC_MAX_IOVEC);   \
        if (iovec_arr == NULL)                                          \
        {                                                               \
            ERROR("%s(), line %d: out of memory",                       \
                  __FUNCTION__, __LINE__);                              \
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);          \
            goto finish;                                                \
        }                                                               \
                                                                        \
        memset((msg_), 0, sizeof(*(msg_)));                             \
                                                                        \
        PREPARE_ADDR_GEN(name, (tarpc_msg_)->msg_name, 0, FALSE,        \
                         TRUE);                                         \
        if (out->common._errno != 0)                                    \
        {                                                               \
            ERROR("%s(): failed to prepare address", __FUNCTION__);     \
            goto finish;                                                \
        }                                                               \
                                                                        \
        if ((tarpc_msg_)->msg_namelen <=                                \
                                    sizeof(struct sockaddr_storage))    \
        {                                                               \
            (msg_)->msg_name = name_dup;                                \
            (msg_)->msg_namelen = namelen;                              \
        }                                                               \
        else                                                            \
        {                                                               \
            (msg_)->msg_name = TE_ALLOC((tarpc_msg_)->msg_namelen);     \
            if ((msg_)->msg_name == NULL)                               \
            {                                                           \
                ERROR("%s(): failed to allocate memory for address",    \
                      __FUNCTION__);                                    \
                goto finish;                                            \
            }                                                           \
            memcpy((msg_)->msg_name,                                    \
                   (tarpc_msg_)->msg_name.raw.raw_val,                  \
                   (tarpc_msg_)->msg_namelen);                          \
            (msg_)->msg_namelen = (tarpc_msg_)->msg_namelen;            \
        }                                                               \
                                                                        \
        (msg_)->msg_iovlen = (tarpc_msg_)->msg_iovlen;                  \
                                                                        \
        if ((tarpc_msg_)->msg_iov.msg_iov_val != NULL)                  \
        {                                                               \
            for (i = 0; i < (tarpc_msg_)->msg_iov.msg_iov_len; i++)     \
            {                                                           \
                INIT_CHECKED_ARG(                                       \
                    (tarpc_msg_)->msg_iov.msg_iov_val[i].               \
                                            iov_base.iov_base_val,      \
                    (tarpc_msg_)->msg_iov.msg_iov_val[i].               \
                                            iov_base.iov_base_len,      \
                    0);                                                 \
                iovec_arr[i].iov_base =                                 \
                    (tarpc_msg_)->msg_iov.msg_iov_val[i].               \
                                            iov_base.iov_base_val;      \
                iovec_arr[i].iov_len =                                  \
                    (tarpc_msg_)->msg_iov.msg_iov_val[i].iov_len;       \
            }                                                           \
            (msg_)->msg_iov = iovec_arr;                                \
            INIT_CHECKED_ARG((char *)iovec_arr,                         \
                             sizeof(*iovec_arr) *                       \
                                    RCF_RPC_MAX_IOVEC,                  \
                             0);                                        \
        }                                                               \
                                                                        \
        if ((tarpc_msg_)->msg_control.msg_control_val != NULL)          \
        {                                                               \
            int      len = calculate_msg_controllen((tarpc_msg_));      \
            int      retval;                                            \
                                                                        \
            if (((msg_)->msg_control = calloc(1, len)) == NULL)         \
            {                                                           \
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);      \
                goto finish;                                            \
            }                                                           \
            (msg_)->msg_controllen = len;                               \
                                                                        \
            retval = msg_control_rpc2h(                                 \
                        (tarpc_msg_)->msg_control.msg_control_val,      \
                        (tarpc_msg_)->msg_control.msg_control_len,      \
                        (msg_)->msg_control, &(msg_)->msg_controllen);  \
            if (retval != 0)                                            \
            {                                                           \
                ERROR("%s(): failed to convert control message from "   \
                      "TARPC representation",                           \
                      __FUNCTION__);                                    \
                out->common._errno = TE_RC(TE_TA_UNIX, retval);         \
                goto finish;                                            \
            }                                                           \
                                                                        \
            INIT_CHECKED_ARG((msg_)->msg_control,                       \
                             (msg_)->msg_controllen, 0);                \
        }                                                               \
                                                                        \
        (msg_)->msg_flags =                                             \
                send_recv_flags_rpc2h((tarpc_msg_)->msg_flags);         \
        INIT_CHECKED_ARG((char *)(msg_), sizeof(*(msg_)), 0);           \
    } while (0)

TARPC_FUNC(sendmsg,
{
    if (in->msg.msg_val != NULL &&
        in->msg.msg_val[0].msg_iov.msg_iov_val != NULL &&
        in->msg.msg_val[0].msg_iov.msg_iov_len > RCF_RPC_MAX_IOVEC)
    {
        ERROR("Too long iovec is provided");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
},
{
    unsigned int i;

    if (in->msg.msg_val == NULL)
    {
        MAKE_CALL(out->retval = func(in->s, NULL,
                                     send_recv_flags_rpc2h(in->flags)));
    }
    else
    {
        struct msghdr        msg;
        struct tarpc_msghdr *rpc_msg = in->msg.msg_val;

        MSGHDR_TARPC2H(rpc_msg, &msg);

        VERB("sendmsg(): s=%d, msg=%s, flags=0x%x", in->s,
             msghdr2str(&msg), send_recv_flags_rpc2h(in->flags));

        MAKE_CALL(out->retval = func(in->s, &msg,
                                     send_recv_flags_rpc2h(in->flags)));
        msghdr_free(&msg);
    }
    finish:
    ;
}
)

/*-------------- recvmsg() ------------------------------*/

TARPC_FUNC(recvmsg,
{
    if (in->msg.msg_val != NULL &&
        in->msg.msg_val[0].msg_iov.msg_iov_val != NULL &&
        in->msg.msg_val[0].msg_iov.msg_iov_len > RCF_RPC_MAX_IOVEC)
    {
        ERROR("Too long iovec is provided");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    COPY_ARG(msg);
},
{
    struct iovec iovec_arr[RCF_RPC_MAX_IOVEC];

    unsigned int  i;
    struct msghdr msg;
    te_bool       free_name = FALSE;

    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(&msg, 0, sizeof(msg));

    if (out->msg.msg_val == NULL)
    {
        MAKE_CALL(out->retval = func(in->s, NULL,
                                     send_recv_flags_rpc2h(in->flags)));
    }
    else
    {
        struct tarpc_msghdr *rpc_msg = out->msg.msg_val;

        PREPARE_ADDR(name, rpc_msg->msg_name, rpc_msg->msg_namelen);

        if (rpc_msg->msg_namelen <=
                (tarpc_socklen_t)sizeof(struct sockaddr_storage))
            msg.msg_name = name;
        else
        {
            /*
             * Do not just assign - sockaddr_output_h2rpc()
             * converts RAW address only if it was changed by the
             * function.
             */
            if (rpc_msg->msg_name.raw.raw_len > 0 &&
                rpc_msg->msg_name.raw.raw_val != NULL)
            {
                msg.msg_name = calloc(1, rpc_msg->msg_name.raw.raw_len);
                if (msg.msg_name == NULL)
                {
                    out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                    goto finish;
                }
                free_name = TRUE;
                memcpy(msg.msg_name, rpc_msg->msg_name.raw.raw_val,
                       rpc_msg->msg_name.raw.raw_len);
            }
            else
                msg.msg_name = rpc_msg->msg_name.raw.raw_val;
        }
        msg.msg_namelen = rpc_msg->msg_namelen;

        msg.msg_iovlen = rpc_msg->msg_iovlen;
        if (rpc_msg->msg_iov.msg_iov_val != NULL)
        {
            for (i = 0; i < rpc_msg->msg_iov.msg_iov_len; i++)
            {
                INIT_CHECKED_ARG(
                    rpc_msg->msg_iov.msg_iov_val[i].iov_base.iov_base_val,
                    rpc_msg->msg_iov.msg_iov_val[i].iov_base.iov_base_len,
                    rpc_msg->msg_iov.msg_iov_val[i].iov_len);
                iovec_arr[i].iov_base =
                    rpc_msg->msg_iov.msg_iov_val[i].iov_base.iov_base_val;
                iovec_arr[i].iov_len =
                    rpc_msg->msg_iov.msg_iov_val[i].iov_len;
            }
            msg.msg_iov = iovec_arr;
            INIT_CHECKED_ARG((char *)iovec_arr, sizeof(iovec_arr), 0);
        }
        if (rpc_msg->msg_control.msg_control_val != NULL)
        {
            int len = calculate_msg_controllen(rpc_msg);
            int rlen = len * 2;
            int data_len = rpc_msg->msg_control.msg_control_val[0].
                           data.data_len;

            free(rpc_msg->msg_control.msg_control_val[0].data.data_val);
            free(rpc_msg->msg_control.msg_control_val);
            rpc_msg->msg_control.msg_control_val = NULL;
            rpc_msg->msg_control.msg_control_len = 0;

            msg.msg_controllen = len;
            if ((msg.msg_control = calloc(1, rlen)) == NULL)
            {
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                goto finish;
            }
            CMSG_FIRSTHDR(&msg)->cmsg_len = CMSG_LEN(data_len);
            INIT_CHECKED_ARG((char *)(msg.msg_control), rlen, len);
        }
        msg.msg_flags = send_recv_flags_rpc2h(rpc_msg->msg_flags);
        rpc_msg->in_msg_flags = send_recv_flags_h2rpc(msg.msg_flags);

        /*
         * msg_name, msg_iov, msg_iovlen and msg_control MUST NOT be
         * changed.
         *
         * msg_namelen, msg_controllen and msg_flags MAY be changed.
         */
        INIT_CHECKED_ARG((char *)&msg.msg_name,
                         sizeof(msg.msg_name), 0);
        INIT_CHECKED_ARG((char *)&msg.msg_iov,
                         sizeof(msg.msg_iov), 0);
        INIT_CHECKED_ARG((char *)&msg.msg_iovlen,
                         sizeof(msg.msg_iovlen), 0);
        INIT_CHECKED_ARG((char *)&msg.msg_control,
                         sizeof(msg.msg_control), 0);

        VERB("recvmsg(): in msg=%s", msghdr2str(&msg));
        MAKE_CALL(out->retval = func(in->s, &msg,
                                     send_recv_flags_rpc2h(in->flags)));
        VERB("recvmsg(): out msg=%s", msghdr2str(&msg));

        rpc_msg->msg_flags = send_recv_flags_h2rpc(msg.msg_flags);

        if (msg.msg_namelen <=
                (tarpc_socklen_t)sizeof(struct sockaddr_storage))
            sockaddr_output_h2rpc(msg.msg_name,
                                  namelen > 0 ?
                                    namelen : rpc_msg->msg_name.raw.raw_len,
                                  msg.msg_namelen,
                                  &(rpc_msg->msg_name));
        else
        {
            RING("Address length %d is bigger than size %d of "
                 "sockaddr_storage structure",
                 rpc_msg->msg_namelen, sizeof(struct sockaddr_storage));
            if (rpc_msg->msg_name.raw.raw_val != NULL &&
                msg.msg_name != NULL)
                memcpy(rpc_msg->msg_name.raw.raw_val, msg.msg_name,
                       rpc_msg->msg_name.raw.raw_len < msg.msg_namelen ?
                        rpc_msg->msg_name.raw.raw_len : msg.msg_namelen);
        }
        rpc_msg->msg_namelen = msg.msg_namelen;

        if (rpc_msg->msg_iov.msg_iov_val != NULL)
        {
            for (i = 0; i < rpc_msg->msg_iov.msg_iov_len; i++)
            {
                rpc_msg->msg_iov.msg_iov_val[i].iov_len =
                    iovec_arr[i].iov_len;
            }
        }

        rpc_msg->msg_controllen = msg.msg_controllen;
        /* in case retval < 0 cmsg is not filled */
        if (out->retval >= 0 && msg.msg_control != NULL)
        {
            int                   rc;
            unsigned int          rpc_len;
            struct tarpc_cmsghdr *rpc_c;

            TE_SCM_RIGHTS2TE(msg.msg_control);
            rc = msg_control_h2rpc(msg.msg_control,
                                   msg.msg_controllen,
                                   &rpc_c, &rpc_len);
            if (rc != 0)
            {
                ERROR("%s(): failed cmsghdr conversion",
                      __FUNCTION__);
                out->common._errno = TE_RC(TE_TA_UNIX, rc);
                goto finish;
            }

            rpc_msg->msg_control.msg_control_val = rpc_c;
            rpc_msg->msg_control.msg_control_len = rpc_len;
        }
    }
    finish:
    if (free_name)
        free(msg.msg_name);
    free(msg.msg_control);
}
)

/*-------------- poll() --------------------------------*/

TARPC_FUNC(poll,
{
    if (in->ufds.ufds_len > RPC_POLL_NFDS_MAX)
    {
        ERROR("Too big nfds is passed to the poll()");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    COPY_ARG(ufds);
},
{
    struct pollfd ufds[RPC_POLL_NFDS_MAX];

    unsigned int i;

    VERB("poll(): IN ufds=0x%lx[%u] nfds=%u timeout=%d",
         (unsigned long int)out->ufds.ufds_val, out->ufds.ufds_len,
         in->nfds, in->timeout);
    for (i = 0; i < out->ufds.ufds_len; i++)
    {
        ufds[i].fd = out->ufds.ufds_val[i].fd;
        INIT_CHECKED_ARG((char *)&(ufds[i].fd), sizeof(ufds[i].fd), 0);
        ufds[i].events = poll_event_rpc2h(out->ufds.ufds_val[i].events);
        INIT_CHECKED_ARG((char *)&(ufds[i].events),
                         sizeof(ufds[i].events), 0);
        ufds[i].revents = poll_event_rpc2h(out->ufds.ufds_val[i].revents);
        VERB("poll(): IN fd=%d events=%hx(rpc %hx) revents=%hx",
             ufds[i].fd, ufds[i].events, out->ufds.ufds_val[i].events,
             ufds[i].revents);
    }

    VERB("poll(): call with ufds=0x%lx, nfds=%u, timeout=%d",
         (unsigned long int)ufds, in->nfds, in->timeout);
    MAKE_CALL(out->retval = func_ptr(ufds, in->nfds, in->timeout));
    VERB("poll(): retval=%d", out->retval);

    for (i = 0; i < out->ufds.ufds_len; i++)
    {
        out->ufds.ufds_val[i].revents = poll_event_h2rpc(ufds[i].revents);
        VERB("poll(): OUT host-revents=%hx rpc-revents=%hx",
             ufds[i].revents, out->ufds.ufds_val[i].revents);
    }
}
)

/*-------------- ppoll() --------------------------------*/

TARPC_FUNC(ppoll,
{
    if (in->ufds.ufds_len > RPC_POLL_NFDS_MAX)
    {
        ERROR("Too big nfds is passed to the ppoll()");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    COPY_ARG(ufds);
    COPY_ARG(timeout);
},
{
    struct pollfd ufds[RPC_POLL_NFDS_MAX];
    struct timespec tv;
    unsigned int i;

    if (out->timeout.timeout_len > 0)
    {
        tv.tv_sec = out->timeout.timeout_val[0].tv_sec;
        tv.tv_nsec = out->timeout.timeout_val[0].tv_nsec;
    }
    INIT_CHECKED_ARG((char *)rcf_pch_mem_get(in->sigmask),
                     sizeof(sigset_t), 0);

    VERB("ppoll(): IN ufds=0x%lx[%u] nfds=%u",
         (unsigned long int)out->ufds.ufds_val, out->ufds.ufds_len,
         in->nfds);
    for (i = 0; i < out->ufds.ufds_len; i++)
    {
        ufds[i].fd = out->ufds.ufds_val[i].fd;
        INIT_CHECKED_ARG((char *)&(ufds[i].fd), sizeof(ufds[i].fd), 0);
        ufds[i].events = poll_event_rpc2h(out->ufds.ufds_val[i].events);
        INIT_CHECKED_ARG((char *)&(ufds[i].events),
                         sizeof(ufds[i].events), 0);
        ufds[i].revents = poll_event_rpc2h(out->ufds.ufds_val[i].revents);
        VERB("ppoll(): IN fd=%d events=%hx(rpc %hx) revents=%hx",
             ufds[i].fd, ufds[i].events, out->ufds.ufds_val[i].events,
             ufds[i].revents);
    }

    VERB("ppoll(): call with ufds=0x%lx, nfds=%u, timeout=%p",
         (unsigned long int)ufds, in->nfds,
         out->timeout.timeout_len > 0 ? out->timeout.timeout_val : NULL);
    MAKE_CALL(out->retval = func_ptr(ufds, in->nfds,
                                     out->timeout.timeout_len == 0 ? NULL :
                                                                     &tv,
                                     rcf_pch_mem_get(in->sigmask)));
    VERB("ppoll(): retval=%d", out->retval);

    if (out->timeout.timeout_len > 0)
    {
        out->timeout.timeout_val[0].tv_sec = tv.tv_sec;
        out->timeout.timeout_val[0].tv_nsec = tv.tv_nsec;
    }

    for (i = 0; i < out->ufds.ufds_len; i++)
    {
        out->ufds.ufds_val[i].revents = poll_event_h2rpc(ufds[i].revents);
        VERB("ppoll(): OUT host-revents=%hx rpc-revents=%hx",
             ufds[i].revents, out->ufds.ufds_val[i].revents);
    }
}
)

#if HAVE_STRUCT_EPOLL_EVENT
/*-------------- epoll_create() ------------------------*/

TARPC_FUNC(epoll_create, {},
{
    MAKE_CALL(out->retval = func(in->size));
}
)

/*-------------- epoll_create1() ------------------------*/

TARPC_FUNC(epoll_create1, {},
{
    MAKE_CALL(out->retval = func(epoll_flags_rpc2h(in->flags)));
}
)

/*-------------- epoll_ctl() --------------------------------*/

TARPC_FUNC(epoll_ctl, {},
{
    struct epoll_event  event;
    struct epoll_event *ptr;

    if (in->event.event_len)
    {
        ptr = &event;
        event.events = epoll_event_rpc2h(in->event.event_val[0].events);
        /* TODO: Should be substituted by correct handling of union */
        event.data.fd = in->fd;
    }
    else
        ptr = NULL;

    VERB("epoll_ctl(): call with epfd=%d op=%d fd=%d event=0x%lx",
         in->epfd, in->op, in->fd,
         (in->event.event_len) ? (unsigned long int)in->event.event_val :
                                 0);

    MAKE_CALL(out->retval = func(in->epfd, in->op, in->fd, ptr));
    VERB("epoll_ctl(): retval=%d", out->retval);
}
)

/*-------------- epoll_wait() --------------------------------*/

TARPC_FUNC(epoll_wait,
{
    /* TODO: RPC_POLL_NFDS_MAX should be substituted */
    if (in->events.events_len > RPC_POLL_NFDS_MAX)
    {
        ERROR("Too many events is passed to the epoll_wait()");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    COPY_ARG(events);
},
{
    /* TODO: RPC_POLL_NFDS_MAX should be substituted */
    struct epoll_event *events = NULL;
    int len = out->events.events_len;
    unsigned int i;

    if (len)
        events = calloc(len, sizeof(struct epoll_event));

    VERB("epoll_wait(): call with epfd=%d, events=0x%lx, maxevents=%d,"
         " timeout=%d",
         in->epfd, (unsigned long int)events, in->maxevents, in->timeout);
    MAKE_CALL(out->retval = func(in->epfd, events, in->maxevents,
                                 in->timeout));
    VERB("epoll_wait(): retval=%d", out->retval);

    for (i = 0; i < out->events.events_len; i++)
    {
        out->events.events_val[i].events =
            epoll_event_h2rpc(events[i].events);
        /* TODO: should be substituted by correct handling of union */
        out->events.events_val[i].data.type = TARPC_ED_INT;
        out->events.events_val[i].data.tarpc_epoll_data_u.fd =
            events[i].data.fd;
    }
    free(events);
}
)

/*-------------- epoll_pwait() --------------------------------*/

TARPC_FUNC(epoll_pwait,
{
    /* TODO: RPC_POLL_NFDS_MAX should be substituted */
    if (in->events.events_len > RPC_POLL_NFDS_MAX)
    {
        ERROR("Too many events is passed to the epoll_pwait()");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    COPY_ARG(events);
},
{
    /* TODO: RPC_POLL_NFDS_MAX should be substituted */
    struct epoll_event *events = NULL;
    int len = out->events.events_len;
    unsigned int i;

    if (len)
        events = calloc(len, sizeof(struct epoll_event));

    VERB("epoll_pwait(): call with epfd=%d, events=0x%lx, maxevents=%d,"
         " timeout=%d sigmask=%p",
         in->epfd, (unsigned long int)events, in->maxevents, in->timeout,
         in->sigmask);

    /*
     * The pointer may be a NULL and, therefore, contain uninitialized
     * data, but we want to check that the data are unchanged even in
     * this case.
     */
    INIT_CHECKED_ARG((char *)rcf_pch_mem_get(in->sigmask),
                     sizeof(sigset_t), 0);

    MAKE_CALL(out->retval = func(in->epfd, events, in->maxevents,
                                 in->timeout,
                                 rcf_pch_mem_get(in->sigmask)));
    VERB("epoll_pwait(): retval=%d", out->retval);

    for (i = 0; i < out->events.events_len; i++)
    {
        out->events.events_val[i].events =
            epoll_event_h2rpc(events[i].events);
        /* TODO: should be substituted by correct handling of union */
        out->events.events_val[i].data.type = TARPC_ED_INT;
        out->events.events_val[i].data.tarpc_epoll_data_u.fd =
            events[i].data.fd;
    }
    free(events);
}
)
#endif

/**
 * Convert host representation of the hostent to the RPC one.
 * The memory is allocated by the routine.
 *
 * @param he   source structure
 *
 * @return RPC structure or NULL is memory allocation failed
 */
static tarpc_hostent *
hostent_h2rpc(struct hostent *he)
{
    tarpc_hostent *rpc_he = (tarpc_hostent *)calloc(1, sizeof(*rpc_he));

    unsigned int i;
    unsigned int k;

    if (rpc_he == NULL)
        return NULL;

    if (he->h_name != NULL)
    {
        if ((rpc_he->h_name.h_name_val = strdup(he->h_name)) == NULL)
            goto release;
        rpc_he->h_name.h_name_len = strlen(he->h_name) + 1;
    }

    if (he->h_aliases != NULL)
    {
        char **ptr;

        for (i = 1, ptr = he->h_aliases; *ptr != NULL; ptr++, i++);

        if ((rpc_he->h_aliases.h_aliases_val =
             (tarpc_h_alias *)calloc(i, sizeof(tarpc_h_alias))) == NULL)
        {
            goto release;
        }
        rpc_he->h_aliases.h_aliases_len = i;

        for (k = 0; k < i - 1; k++)
        {
            if ((rpc_he->h_aliases.h_aliases_val[k].name.name_val =
                 strdup((he->h_aliases)[k])) == NULL)
            {
                goto release;
            }
            rpc_he->h_aliases.h_aliases_val[k].name.name_len =
                strlen((he->h_aliases)[k]) + 1;
        }
    }

    rpc_he->h_addrtype = domain_h2rpc(he->h_addrtype);
    rpc_he->h_length = he->h_length;

    if (he->h_addr_list != NULL)
    {
        char **ptr;

        for (i = 1, ptr = he->h_addr_list; *ptr != NULL; ptr++, i++);

        if ((rpc_he->h_addr_list.h_addr_list_val =
             (tarpc_h_addr *)calloc(i, sizeof(tarpc_h_addr))) == NULL)
        {
            goto release;
        }
        rpc_he->h_addr_list.h_addr_list_len = i;

        for (k = 0; k < i - 1; k++)
        {
            if ((rpc_he->h_addr_list.h_addr_list_val[i].val.val_val =
                 calloc(1, rpc_he->h_length)) == NULL)
            {
                goto release;
            }
            rpc_he->h_addr_list.h_addr_list_val[i].val.val_len =
                rpc_he->h_length;
            memcpy(rpc_he->h_addr_list.h_addr_list_val[i].val.val_val,
                   he->h_addr_list[i], rpc_he->h_length);
        }
    }

    return rpc_he;

release:
    /* Release the memory in the case of failure */
    free(rpc_he->h_name.h_name_val);
    if (rpc_he->h_aliases.h_aliases_val != NULL)
    {
        for (i = 0; i < rpc_he->h_aliases.h_aliases_len - 1; i++)
             free(rpc_he->h_aliases.h_aliases_val[i].name.name_val);
        free(rpc_he->h_aliases.h_aliases_val);
    }
    if (rpc_he->h_addr_list.h_addr_list_val != NULL)
    {
        for (i = 0; i < rpc_he->h_addr_list.h_addr_list_len - 1; i++)
            free(rpc_he->h_addr_list.h_addr_list_val[i].val.val_val);
        free(rpc_he->h_addr_list.h_addr_list_val);
    }
    free(rpc_he);
    return NULL;
}

/*-------------- gethostbyname() -----------------------------*/

TARPC_FUNC(gethostbyname, {},
{
    struct hostent *he;

    MAKE_CALL(he = (struct hostent *)func_ptr_ret_ptr(in->name.name_val));
    if (he != NULL)
    {
        if ((out->res.res_val = hostent_h2rpc(he)) == NULL)
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        else
            out->res.res_len = 1;
    }
}
)

/*-------------- gethostbyaddr() -----------------------------*/

TARPC_FUNC(gethostbyaddr, {},
{
    struct hostent *he;

    INIT_CHECKED_ARG(in->addr.val.val_val, in->addr.val.val_len, 0);

    MAKE_CALL(he = (struct hostent *)
                       func_ptr_ret_ptr(in->addr.val.val_val,
                                        in->addr.val.val_len,
                                        addr_family_rpc2h(in->type)));
    if (he != NULL)
    {
        if ((out->res.res_val = hostent_h2rpc(he)) == NULL)
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        else
            out->res.res_len = 1;
    }
}
)


/*-------------- getaddrinfo() -----------------------------*/

/**
 * Convert host native addrinfo to the RPC one.
 *
 * @param ai            host addrinfo structure
 * @param rpc_ai        pre-allocated RPC addrinfo structure
 *
 * @return 0 in the case of success or -1 in the case of memory allocation
 * failure
 */
static int
ai_h2rpc(struct addrinfo *ai, struct tarpc_ai *ai_rpc)
{
    ai_rpc->flags = ai_flags_h2rpc(ai->ai_flags);
    ai_rpc->family = domain_h2rpc(ai->ai_family);
    ai_rpc->socktype = socktype_h2rpc(ai->ai_socktype);
    ai_rpc->protocol = proto_h2rpc(ai->ai_protocol);
    ai_rpc->addrlen = ai->ai_addrlen - SA_COMMON_LEN;

    sockaddr_output_h2rpc(ai->ai_addr, sizeof(*ai->ai_addr),
                          sizeof(*ai->ai_addr), &ai_rpc->addr);

    if (ai->ai_canonname != NULL)
    {
        if ((ai_rpc->canonname.canonname_val =
             strdup(ai->ai_canonname)) == NULL)
        {
            return -1;
        }
        ai_rpc->canonname.canonname_len = strlen(ai->ai_canonname) + 1;
    }

    return 0;
}

/* I do not understand, which function may be found by dynamic lookup */
TARPC_FUNC_STATIC(getaddrinfo, {},
{
    struct addrinfo  hints;
    struct addrinfo *info = NULL;
    struct addrinfo *res = NULL;

    struct sockaddr_storage addr;
    struct sockaddr        *a;

    memset(&hints, 0, sizeof(hints));
    if (in->hints.hints_val != NULL)
    {
        info = &hints;
        hints.ai_flags = ai_flags_rpc2h(in->hints.hints_val[0].flags);
        hints.ai_family = domain_rpc2h(in->hints.hints_val[0].family);
        hints.ai_socktype = socktype_rpc2h(in->hints.hints_val[0].socktype);
        hints.ai_protocol = proto_rpc2h(in->hints.hints_val[0].protocol);
        hints.ai_addrlen = in->hints.hints_val[0].addrlen + SA_COMMON_LEN;
        sockaddr_rpc2h(&(in->hints.hints_val[0].addr), SA(&addr),
                       sizeof(addr), &a, NULL);
        hints.ai_addr = a;
        hints.ai_canonname = in->hints.hints_val[0].canonname.canonname_val;
        INIT_CHECKED_ARG(in->hints.hints_val[0].canonname.canonname_val,
                         in->hints.hints_val[0].canonname.canonname_len, 0);
        hints.ai_next = NULL;
        INIT_CHECKED_ARG((char *)info, sizeof(*info), 0);
    }
    INIT_CHECKED_ARG(in->node.node_val, in->node.node_len, 0);
    INIT_CHECKED_ARG(in->service.service_val,
                     in->service.service_len, 0);
    MAKE_CALL(out->retval = func(in->node.node_val,
                                 in->service.service_val, info, &res));
    /* GLIBC getaddrinfo clean up errno on success */
    out->common.errno_changed = FALSE;
    if (out->retval != 0 && res != NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ECORRUPTED);
        res = NULL;
    }
    if (res != NULL)
    {
        int i;

        struct tarpc_ai *arr;

        for (i = 0, info = res; info != NULL; i++, info = info->ai_next);

        if ((arr = calloc(i, sizeof(*arr))) != NULL)
        {
            int k;

            for (k = 0, info = res; k < i; k++, info = info->ai_next)
            {
                if (ai_h2rpc(info, arr + k) < 0)
                {
                    for (k--; k >= 0; k--)
                    {
                        free(arr[k].canonname.canonname_val);
                    }
                    free(arr);
                    arr = NULL;
                    break;
                }
            }
        }
        if (arr == NULL)
        {
            out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
            freeaddrinfo(res);
        }
        else
        {
            out->mem_ptr = rcf_pch_mem_alloc(res);
            out->res.res_val = arr;
            out->res.res_len = i;
        }
    }
}
)

/*-------------- freeaddrinfo() -----------------------------*/
/* I do not understand, which function may be found by dynamic lookup */
TARPC_FUNC_STATIC(freeaddrinfo, {},
{
    MAKE_CALL(func(rcf_pch_mem_get(in->mem_ptr)));
    rcf_pch_mem_free(in->mem_ptr);
}
)

/*-------------- pipe() --------------------------------*/
TARPC_FUNC(pipe,
{
    COPY_ARG(filedes);
},
{
    MAKE_CALL(out->retval = func_ptr(out->filedes.filedes_len > 0 ?
                                     out->filedes.filedes_val : NULL));
}
)


/*-------------- pipe2() --------------------------------*/
TARPC_FUNC(pipe2,
{
    COPY_ARG(filedes);
},
{
    MAKE_CALL(out->retval = func_ptr(out->filedes.filedes_len > 0 ?
                                     out->filedes.filedes_val : NULL,
                                     in->flags));
}
)

/*-------------- socketpair() ------------------------------*/

TARPC_FUNC(socketpair,
{
    COPY_ARG(sv);
},
{
    MAKE_CALL(out->retval = func(domain_rpc2h(in->domain),
                                 socktype_rpc2h(in->type),
                                 proto_rpc2h(in->proto),
                                 (out->sv.sv_len > 0) ?
                                     out->sv.sv_val : NULL));
}
)

#ifndef TE_POSIX_FS_PROVIDED
/*-------------- open() --------------------------------*/
TARPC_FUNC(open, {},
{
    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->fd = func_ptr(in->path.path_val,
                                 fcntl_flags_rpc2h(in->flags),
                                 file_mode_flags_rpc2h(in->mode)));
}
)
#endif

/*-------------- open64() --------------------------------*/
TARPC_FUNC(open64, {},
{
    TARPC_ENSURE_NOT_NULL(path);
    MAKE_CALL(out->fd = func_ptr(in->path.path_val,
                                 fcntl_flags_rpc2h(in->flags),
                                 file_mode_flags_rpc2h(in->mode)));
}
)

/*-------------- fopen() --------------------------------*/
TARPC_FUNC(fopen, {},
{
    MAKE_CALL(out->mem_ptr =
                  rcf_pch_mem_alloc(func_ptr_ret_ptr(in->path,
                                                     in->mode)));
}
)

/*-------------- fdopen() --------------------------------*/
TARPC_FUNC(fdopen, {},
{
    MAKE_CALL(out->mem_ptr =
                  rcf_pch_mem_alloc(func_ret_ptr(in->fd,
                                                     in->mode)));
}
)

/*-------------- fclose() -------------------------------*/
TARPC_FUNC(fclose, {},
{
    MAKE_CALL(out->retval = func_ptr(rcf_pch_mem_get(in->mem_ptr)));
    rcf_pch_mem_free(in->mem_ptr);
}
)

/*-------------- fileno() --------------------------------*/
TARPC_FUNC(fileno, {},
{
    MAKE_CALL(out->fd = func_ptr(rcf_pch_mem_get(in->mem_ptr)));
}
)

/*-------------- popen() --------------------------------*/
TARPC_FUNC(popen, {},
{
    MAKE_CALL(out->mem_ptr =
                  rcf_pch_mem_alloc(func_ptr_ret_ptr(in->cmd,
                                                     in->mode)));
}
)

/*-------------- pclose() -------------------------------*/
TARPC_FUNC(pclose, {},
{
    MAKE_CALL(out->retval = func_ptr(rcf_pch_mem_get(in->mem_ptr)));
    rcf_pch_mem_free(in->mem_ptr);
}
)

/*-------------- te_shell_cmd() --------------------------------*/
TARPC_FUNC(te_shell_cmd, {},
{
    MAKE_CALL(out->pid =
              func_ptr(in->cmd.cmd_val, in->uid,
                       in->in_fd ? &out->in_fd : NULL,
                       in->out_fd ? &out->out_fd : NULL,
                       in->err_fd ? &out->err_fd : NULL));
}
)

/*-------------- system() ----------------------------------*/
TARPC_FUNC_STANDALONE(system, {},
{
    int             st;
    rpc_wait_status r_st;

    MAKE_CALL(st = ta_system(in->cmd.cmd_val));
    r_st = wait_status_h2rpc(st);
    out->status_flag = r_st.flag;
    out->status_value = r_st.value;
}
)

/*-------------- chroot() --------------------------------*/
TARPC_FUNC(chroot, {},
{
    char *chroot_path = NULL;
    char *ta_dir_path = NULL;
    char *ta_execname_path = NULL;
    char *port_path = getenv("TE_RPC_PORT");

    chroot_path = realpath(in->path.path_val, NULL);
    ta_dir_path = realpath(ta_dir, NULL);
    ta_execname_path = realpath(ta_execname, NULL);
    port_path = realpath(port_path, NULL);

    if (chroot_path == NULL || ta_dir_path == NULL ||
        ta_execname_path == NULL || port_path == NULL)
    {
        if (chroot_path == NULL)
            ERROR("%s(): failed to determine absolute path of "
                  "chroot() argument", __FUNCTION__);
        if (ta_dir_path == NULL)
            ERROR("%s(): failed to determine absolute path of ta_dir",
                  __FUNCTION__);
        if (ta_execname_path == NULL)
            ERROR("%s(): failed to determine absolute path "
                  "of ta_execname", __FUNCTION__);
        /**
         * Path for port can be undefined if we do not use
         * AF_UNIX sockets for communication.
         */
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        out->retval = -1;
        goto finish;
    }

    if (strstr(ta_dir_path, chroot_path) != ta_dir_path ||
        strstr(ta_execname_path, chroot_path) != ta_execname_path ||
        (port_path != NULL && strstr(port_path, chroot_path) != port_path))
    {
        ERROR("%s(): argument of chroot() must be such that TA "
              "folder is inside new root tree");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        out->retval = -1;
        goto finish;
    }

    MAKE_CALL(out->retval = func_ptr(chroot_path));

    if (out->retval == 0)
    {
        /*
         * Change paths used by TE so that they will be
         * inside a new root.
         */

        strcpy(ta_dir, ta_dir_path + strlen(chroot_path));
        strcpy((char *)ta_execname,
               ta_execname_path + strlen(chroot_path));
        if (port_path != NULL)
            setenv("TE_RPC_PORT",
                   strdup(port_path + strlen(chroot_path)), 1);
   }

finish:

    free(chroot_path);
    free(ta_dir_path);
    free(ta_execname_path);
    free(port_path);
}
)

/*-------------- copy_ta_libs ---------------------------*/
/** Maximum shell command lenght */
#define MAX_CMD 1000

/**
 * Check that string was not truncated.
 *
 * @param _call     snprintf() call
 * @param _str      String
 * @param _size     Maximum available size
 */
#define CHECK_SNPRINTF(_call, _str, _size) \
    do {                                        \
        int _rc;                                \
        _rc = _call;                            \
        if (_rc >= (int)(_size))                \
        {                                       \
            ERROR("%s(): %s was truncated",     \
                  __FUNCTION__, (_str));        \
            return -1;                          \
        }                                       \
    } while (0)

/**
 * Call system() and check result.
 *
 * @param _cmd  Shell command to execute
 */
#define SYSTEM(_cmd) \
    if (system(_cmd) < 0)                               \
    {                                                   \
        if (errno == ECHILD)                            \
            errno = 0;                                  \
        else                                            \
        {                                               \
            ERROR("%s(): system(%s) failed with %r",    \
                  __FUNCTION__, cmd, errno);            \
            return -1;                                  \
        }                                               \
    }

/**
 * Obtain string wihtout spaces on both ends.
 *
 * @param str   String
 *
 * @return Pointer to first non-space position in
 *         str.
 */
char *
trim(char *str)
{
    int i = 0;

    for (i = strlen(str) - 1; i >= 0; i--)
    {
        if (str[i] == ' ' || str[i] == '\t'
            || str[i] == '\n' || str[i] == '\r')
            str[i] = '\0';
        else
            break;
    }

    for (i = 0; i < (int)strlen(str); i++)
        if (str[i] != ' ' && str[i] != '\t')
            break;

    return str + i;
}

/**
 * Copy shared libraries to TA folder.
 *
 * @param path Path to TA folder.
 *
 * @return 0 on success or -1
 */
int
copy_ta_libs(char *path)
{
    char    path_to_lib[RCF_MAX_PATH];
    char    path_to_chmod[RCF_MAX_PATH];
    char    str[MAX_CMD];
    char    cmd[MAX_CMD];
    char   *begin_path = 0;
    te_bool was_cut = FALSE;
    char   *s;
    FILE   *f;
    FILE   *f_list;
    te_bool ld_found = FALSE;
    int     saved_errno = errno;

    struct stat file_stat;

    errno = 0;

    CHECK_SNPRINTF(
        snprintf(str, MAX_CMD, "%s/ta_libs_list", path),
        str, MAX_CMD);
    f_list = fopen(str, "w");
    if (f_list == NULL)
    {
        ERROR("%s(): failed to create file to store list of libs",
              __FUNCTION__);
        return -1;
    }

    CHECK_SNPRINTF(snprintf(cmd, MAX_CMD,
                            "(ldd %s | sed \"s/.*=>[ \t]*//\" "
                            "| sed \"s/(0x[0-9a-f]*)$//\")", ta_execname),
                   cmd, MAX_CMD);

    if (dynamic_library_set && strlen(dynamic_library_name) != 0)
        CHECK_SNPRINTF(
                snprintf(cmd + strlen(cmd), MAX_CMD - strlen(cmd),
                         " && (ldd %s | sed \"s/.*=>[ \t]*//\" "
                         "| sed \"s/(0x[0-9a-f]*)$//\") && "
                         "(echo \"%s\")", dynamic_library_name,
                         dynamic_library_name),
                cmd + strlen(cmd), MAX_CMD - strlen(cmd));

    f = popen(cmd, "r");
    if (f == NULL)
    {
        ERROR("%s(): failed to obtain ldd output for TA", __FUNCTION__);
        return -1;
    }

    while (fgets(str, RCF_MAX_PATH, f) != NULL)
    {
        begin_path = trim(str);

        if (strstr(begin_path, "/ld-") != NULL ||
            strstr(begin_path, "/ld.") != NULL)
            ld_found = TRUE;

        if (stat(begin_path, &file_stat) >= 0)
        {
            CHECK_SNPRINTF(snprintf(path_to_lib, RCF_MAX_PATH,
                                    "%s/%s", path, begin_path),
                           path_to_lib, RCF_MAX_PATH);

            fprintf(f_list, "%s\n", path_to_lib);
            was_cut = FALSE;

            while ((s = strrchr(path_to_lib, '/')) != NULL)
            {
                if (stat(path_to_lib, &file_stat) >= 0)
                    break;
                *s = '\0';
                was_cut = TRUE;
            }
            if (was_cut)
            {
                s = path_to_lib + strlen(path_to_lib);
                *s = '/';
            }

            fprintf(f_list, "%s\n", path_to_lib);
            memcpy(path_to_chmod, path_to_lib, RCF_MAX_PATH);

            CHECK_SNPRINTF(snprintf(path_to_lib, RCF_MAX_PATH, "%s/%s",
                                    path, begin_path),
                           path_to_lib, RCF_MAX_PATH);
            s = strrchr(path_to_lib, '/');

            if (s == NULL)
            {
                ERROR("%s(): incorrect path %s", __FUNCTION__,
                      path_to_lib);
                return -1;
            }
            else
                *s = '\0';

            CHECK_SNPRINTF(snprintf(cmd, MAX_CMD, "mkdir -p \"%s\" && "
                                    "cp \"%s\" \"%s\" && "
                                    "chmod -R a+rwx \"%s\"",
                                    path_to_lib, begin_path, path_to_lib,
                                    path_to_chmod),
                           cmd, MAX_CMD);
            SYSTEM(cmd);
        }
    }

    if (!ld_found)
    {
        CHECK_SNPRINTF(snprintf(cmd, MAX_CMD, "cp /lib/ld.* \"%s/lib\"",
                                path),
                       cmd, MAX_CMD);
        SYSTEM(cmd);
    }

    if (pclose(f) < 0)
    {
        if (errno == ECHILD)
            errno = 0;
        else
        {
            ERROR("%s(): pclose() failed with %r",
                  __FUNCTION__, errno);
            return -1;
        }
    }

    fclose(f_list);

    if (errno == 0)
        errno = saved_errno;
    return 0;
}

TARPC_FUNC(copy_ta_libs, {},
{
    MAKE_CALL(out->retval = func_ptr(in->path.path_val));
}
)

/*-------------- rm_ta_libs ---------------------------*/
/**
 * Remove libraries copied by copy_ta_libs().
 *
 * @param path From where to remove.
 *
 * @return 0 on success or -1
 */
int
rm_ta_libs(char *path)
{
    char    str[MAX_CMD];
    char    cmd[RCF_MAX_PATH];
    char   *s;
    FILE   *f_list;
    int     saved_errno = errno;

    errno = 0;

    CHECK_SNPRINTF(snprintf(str, MAX_CMD, "%s/ta_libs_list", path),
                            str, MAX_CMD);
    f_list = fopen(str, "r");
    if (f_list == NULL)
    {
        ERROR("%s(): failed to create file to store list of libs",
              __FUNCTION__);
        return -1;
    }

    while (fgets(str, RCF_MAX_PATH, f_list) != NULL)
    {
        s = trim(str);
        if (strstr(s, path) != s)
            ERROR("Attempt to delete %s not in TA folder", s);
        else
        {
            CHECK_SNPRINTF(snprintf(cmd, RCF_MAX_PATH, "rm -rf %s", s),
                           cmd, RCF_MAX_PATH);
            SYSTEM(cmd);
        }
    }

    fclose(f_list);
    CHECK_SNPRINTF(snprintf(cmd, RCF_MAX_PATH,
                           "rm -rf %s/ta_libs_list", ta_dir),
             cmd, RCF_MAX_PATH);
    SYSTEM(cmd);

    if (errno == 0)
        errno = saved_errno;
    return 0;
}

TARPC_FUNC(rm_ta_libs, {},
{
    MAKE_CALL(out->retval = func_ptr(in->path.path_val));
}
)

#undef SYSTEM
#undef MAX_CMD

/*-------------- vlan_get_parent----------------------*/
bool_t
_vlan_get_parent_1_svc(tarpc_vlan_get_parent_in *in,
                       tarpc_vlan_get_parent_out *out,
                       struct svc_req *rqstp)
{
    char *str;

    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));
    VERB("PID=%d TID=%llu: Entry %s",
         (int)getpid(), (unsigned long long int)pthread_self(),
         "vlan_get_parent");

    if ((str = (char *)calloc(IF_NAMESIZE, 1)) == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    else
    {
        out->ifname.ifname_val = str;
        out->ifname.ifname_len = IF_NAMESIZE;
    }

    out->common._errno = ta_vlan_get_parent(in->ifname.ifname_val,
                                            out->ifname.ifname_val);

    out->retval = (out->common._errno == 0) ? 0 : -1;

    return TRUE;
}

/*-------------- bond_get_slaves----------------------*/
bool_t
_bond_get_slaves_1_svc(tarpc_bond_get_slaves_in *in,
                       tarpc_bond_get_slaves_out *out,
                       struct svc_req *rqstp)
{
    char slaves[16][IFNAMSIZ];
    int i;

    UNUSED(rqstp);
    memset(out, 0, sizeof(*out));
    VERB("PID=%d TID=%llu: Entry %s",
         (int)getpid(), (unsigned long long int)pthread_self(),
         "bond_get_slaves");

    out->slaves_num = in->slaves_num;
    out->common._errno = ta_bond_get_slaves(in->ifname.ifname_val,
                                            slaves, &(out->slaves_num));

    if ((out->slaves.slaves_val =
            (tarpc_ifname *)calloc(out->slaves_num,
                                   sizeof(tarpc_ifname))) == NULL)
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    out->slaves.slaves_len = out->slaves_num;

    for (i = 0; i < out->slaves_num; i++)
        memcpy(out->slaves.slaves_val[i].ifname, slaves[i], IFNAMSIZ);
    out->retval = (out->common._errno == 0) ? 0 : -1;

    return TRUE;
}

/*-------------- getenv() --------------------------------*/
TARPC_FUNC(getenv, {},
{
    char *val;

    MAKE_CALL(val = func_ptr_ret_ptr(in->name));
    /*
     * fixme kostik: dirty hack as we can't encode
     * NULL string pointer - STRING differs from pointer
     * in RPC representation
     */
    out->val_null = (val == NULL);
    out->val = strdup(val ? val : "");
}
)

/*-------------- setenv() --------------------------------*/
TARPC_FUNC(setenv, {},
{
    MAKE_CALL(out->retval = func_ptr(in->name, in->val,
                                     (int)(in->overwrite)));
}
)

/*-------------- unsetenv() --------------------------------*/
TARPC_FUNC(unsetenv, {},
{
    MAKE_CALL(out->retval = func_ptr(in->name));
}
)


/*-------------- getpwnam() --------------------------------*/
#define PUT_STR(_field) \
        do {                                                            \
            out->passwd._field._field##_val = strdup(pw->pw_##_field);  \
            if (out->passwd._field._field##_val == NULL)                \
            {                                                           \
                ERROR("Failed to duplicate string '%s'",                \
                      pw->pw_##_field);                                 \
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);      \
                return -1;                                              \
            }                                                           \
            out->passwd._field._field##_len =                           \
                strlen(out->passwd._field._field##_val) + 1;            \
        } while (0)

/**
 * Copy the content of 'struct passwd' to RPC output structure.
 *
 * @param out   RPC output structure to fill in
 * @param pw    system native 'struct passwd' structure
 *
 * @return Status of the operation
 * @retval 0   on success
 * @retval -1  on failure
 *
 * @note We added this function because some systems might not have
 *       all the fields and we need to track this. For example Android
 *       does not export 'gecos' field.
 */
static int
copy_passwd_struct(struct tarpc_getpwnam_out *out, struct passwd *pw)
{
    PUT_STR(name);
    PUT_STR(passwd);
    out->passwd.uid = pw->pw_uid;
    out->passwd.gid = pw->pw_gid;
#ifdef HAVE_STRUCT_PASSWD_PW_GECOS
    PUT_STR(gecos);
#endif
    PUT_STR(dir);
    PUT_STR(shell);

    return 0;
}

TARPC_FUNC(getpwnam, {},
{
    struct passwd *pw;

    MAKE_CALL(pw = (struct passwd *)func_ptr_ret_ptr(in->name.name_val));
    /* GLIBC getpwnam clean up errno on success */
    out->common.errno_changed = FALSE;

    if (pw != NULL)
    {
        copy_passwd_struct(out, pw);
    }
    else
    {
        ERROR("getpwnam() returned NULL");
    }

    if (!RPC_IS_ERRNO_RPC(out->common._errno))
    {
        free(out->passwd.name.name_val);
        free(out->passwd.passwd.passwd_val);
        free(out->passwd.gecos.gecos_val);
        free(out->passwd.dir.dir_val);
        free(out->passwd.shell.shell_val);
        memset(&(out->passwd), 0, sizeof(out->passwd));
    }
    ;
}
)

#undef PUT_STR

/*-------------- uname() --------------------------------*/

#define PUT_STR(_dst, _field)                                       \
        do {                                                        \
            out->buf._dst._dst##_val = strdup(uts._field);          \
            if (out->buf._dst._dst##_val == NULL)                   \
            {                                                       \
                ERROR("Failed to duplicate string '%s'",            \
                      uts._field);                                  \
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);  \
                goto finish;                                        \
            }                                                       \
            out->buf._dst._dst##_len =                              \
                strlen(out->buf._dst._dst##_val) + 1;               \
        } while (0)

TARPC_FUNC(uname, {},
{
    struct utsname uts;

    UNUSED(in);

    MAKE_CALL(out->retval = func_ptr(&uts));
/* inequality because Solaris' uname() returns
 * "non-negative value" in case of success
 */
    if (out->retval >= 0)
    {
        out->retval = 0;
        PUT_STR(sysname, sysname);
        PUT_STR(nodename, nodename);
        PUT_STR(release, release);
        PUT_STR(osversion, version);
        PUT_STR(machine, machine);
    }
    else
    {
        ERROR("uname() returned error");
    }
finish:
    if (!RPC_IS_ERRNO_RPC(out->common._errno))
    {
        free(out->buf.sysname.sysname_val);
        free(out->buf.nodename.nodename_val);
        free(out->buf.release.release_val);
        free(out->buf.osversion.osversion_val);
        free(out->buf.machine.machine_val);
        memset(&(out->buf), 0, sizeof(out->buf));
    }
    ;
}
)

#undef PUT_STR


/*-------------- getuid() --------------------------------*/
TARPC_FUNC(getuid, {}, { MAKE_CALL(out->uid = func_void()); })

/*-------------- getuid() --------------------------------*/
TARPC_FUNC(geteuid, {}, { MAKE_CALL(out->uid = func_void()); })

/*-------------- setuid() --------------------------------*/
TARPC_FUNC(setuid, {}, { MAKE_CALL(out->retval = func(in->uid)); })

/*-------------- seteuid() --------------------------------*/
TARPC_FUNC(seteuid, {}, { MAKE_CALL(out->retval = func(in->uid)); })


#ifdef WITH_TR069_SUPPORT
/*-------------- cwmp_op_call() -------------------*/
TARPC_FUNC(cwmp_op_call, {},
{
    MAKE_CALL(func_ptr(in, out));
}
)

/*-------------- cwmp_op_check() -------------------*/
TARPC_FUNC(cwmp_op_check, {},
{
    MAKE_CALL(func_ptr(in, out));
}
)

/*-------------- cwmp_conn_req() -------------------*/
TARPC_FUNC(cwmp_conn_req, {}, { MAKE_CALL(func_ptr(in, out)); })

/*-------------- cwmp_acse_start() -------------------*/
TARPC_FUNC(cwmp_acse_start, {}, { MAKE_CALL(func_ptr(in, out)); })

#endif


/*-------------- generic iomux functions --------------------------*/

/* TODO: iomux_funcs should include iomux type, making arg list for all the
 * functions shorter. */
typedef union iomux_funcs {
    api_func        select;
    api_func_ptr    poll;
#if HAVE_STRUCT_EPOLL_EVENT
    struct {
        api_func    wait;
        api_func    create;
        api_func    ctl;
        api_func    close;
    } epoll;
#endif
} iomux_funcs;

#define IOMUX_MAX_POLLED_FDS 64
typedef union iomux_state {
    struct {
        int maxfds;
        fd_set rfds, wfds, exfds;
        int nfds;
        int fds[IOMUX_MAX_POLLED_FDS];
    } select;
    struct {
        int nfds;
        struct pollfd fds[IOMUX_MAX_POLLED_FDS];
    } poll;
#if HAVE_STRUCT_EPOLL_EVENT
    int epoll;
#endif
} iomux_state;

typedef union iomux_return {
    struct {
        fd_set rfds, wfds, exfds;
    } select;
#if HAVE_STRUCT_EPOLL_EVENT
    struct {
        struct epoll_event events[IOMUX_MAX_POLLED_FDS];
        int nevents;
    } epoll;
#endif
} iomux_return;

/** Iterator for iomux_return structure. */
typedef int iomux_return_iterator;
#define IOMUX_RETURN_ITERATOR_START 0
#define IOMUX_RETURN_ITERATOR_END   -1


/**
 * To have these constants defined in Linux, specify
 * -D_GNU_SOURCE (or other related feature test macro) in
 * TE_PLATFORM macro in your builder.conf
 */
#ifndef POLLRDNORM
#define POLLRDNORM  0
#endif
#ifndef POLLWRNORM
#define POLLWRNORM  0
#endif
#ifndef POLLRDBAND
#define POLLRDBAND  0
#endif
#ifndef POLLWRBAND
#define POLLWRBAND  0
#endif

/* Mapping to/from select and POLL*.  Copied from Linux kernel. */
#define IOMUX_SELECT_READ \
    (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define IOMUX_SELECT_WRITE \
    (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR)
#define IOMUX_SELECT_EXCEPT (POLLPRI)

static iomux_func
get_default_iomux()
{
    char    *default_iomux = getenv("TE_RPC_DEFAULT_IOMUX");
    return (default_iomux == NULL) ? FUNC_POLL :
                str2iomux(default_iomux);
}

/** Resolve all functions used by particular iomux and store them into
 * iomux_funcs. */
static inline int
iomux_find_func(te_bool use_libc, iomux_func *iomux, iomux_funcs *funcs)
{
    int rc = 0;

    if (*iomux == FUNC_DEFAULT_IOMUX)
        *iomux = get_default_iomux();

    switch (*iomux)
    {
        case FUNC_SELECT:
            rc = tarpc_find_func(use_libc, "select", &funcs->select);
            break;
        case FUNC_PSELECT:
            rc = tarpc_find_func(use_libc, "pselect", &funcs->select);
            break;
        case FUNC_POLL:
            rc = tarpc_find_func(use_libc, "poll",
                                 (api_func *)&funcs->poll);
            break;
        case FUNC_PPOLL:
            rc = tarpc_find_func(use_libc, "ppoll",
                                 (api_func *)&funcs->poll);
            break;
#if HAVE_STRUCT_EPOLL_EVENT
        case FUNC_EPOLL:
        case FUNC_EPOLL_PWAIT:
            if (*iomux == FUNC_EPOLL)
                rc = tarpc_find_func(use_libc, "epoll_wait",
                                     &funcs->epoll.wait);
            else
                rc = tarpc_find_func(use_libc, "epoll_pwait",
                                     &funcs->epoll.wait);
            rc = rc ||
                 tarpc_find_func(use_libc, "epoll_ctl",
                                 &funcs->epoll.ctl)  ||
                 tarpc_find_func(use_libc, "epoll_create",
                                 &funcs->epoll.create);
                 tarpc_find_func(use_libc, "close", &funcs->epoll.close);
            break;
#endif
        default:
            rc = -1;
            errno = ENOENT;
    }

    return rc;
}

/** Initialize iomux_state so that it is safe to call iomux_close() */
static inline void
iomux_state_init_invalid(iomux_func iomux, iomux_state *state)
{
#if HAVE_STRUCT_EPOLL_EVENT
    if (iomux == FUNC_EPOLL || iomux == FUNC_EPOLL_PWAIT)
        state->epoll = -1;
#endif
}

/** Initialize iomux_state with zero value. Possibly, we should pass
 * maximum number of fds and use that number instead of
 * IOMUX_MAX_POLLED_FDS. */
static inline int
iomux_create_state(iomux_func iomux, iomux_funcs *funcs,
                   iomux_state *state)
{
    switch (iomux)
    {
        case FUNC_SELECT:
        case FUNC_PSELECT:
            FD_ZERO(&state->select.rfds);
            FD_ZERO(&state->select.wfds);
            FD_ZERO(&state->select.exfds);
            state->select.maxfds = 0;
            state->select.nfds = 0;
            break;
        case FUNC_POLL:
        case FUNC_PPOLL:
            state->poll.nfds = 0;
            break;
#if HAVE_STRUCT_EPOLL_EVENT
        case FUNC_EPOLL:
        case FUNC_EPOLL_PWAIT:
            state->epoll = funcs->epoll.create(IOMUX_MAX_POLLED_FDS);
            return (state->epoll >= 0) ? 0 : -1;
#endif
        case FUNC_DEFAULT_IOMUX:
            ERROR("%s() function can't be used with default iomux",
                  __FUNCTION__);
            return -1;

    }
    return 0;
}

static inline void
iomux_select_set_state(iomux_state *state, int fd, int events,
                       te_bool do_clear)
{
    /* Hack: POLERR is present in both read and write. Do not set both if
     * not really necessary */
    if ((events & POLLERR))
    {
        if ((events & ((IOMUX_SELECT_READ | IOMUX_SELECT_WRITE) &
                       ~POLLERR)) == 0)
        {
            events |= POLLIN;
        }
        events &= ~POLLERR;
    }

    /* Set and clear events */
    if ((events & IOMUX_SELECT_READ))
        FD_SET(fd, &state->select.rfds);
    else if (do_clear)
        FD_CLR(fd, &state->select.rfds);
    if ((events & IOMUX_SELECT_WRITE))
        FD_SET(fd, &state->select.wfds);
    else if (do_clear)
        FD_CLR(fd, &state->select.wfds);
    if ((events & IOMUX_SELECT_EXCEPT))
        FD_SET(fd, &state->select.exfds);
    else if (do_clear)
        FD_CLR(fd, &state->select.exfds);
}

/** Add fd to the list of watched fds, with given events (in POLL-events).
 * For select, all fds are added to exception list.
 * For some iomuxes, the function will produce error when adding the same
 * fd twice, so iomux_mod_fd() should be used. */
static inline int
iomux_add_fd(iomux_func iomux, iomux_funcs *funcs, iomux_state *state,
             int fd, int events)
{

#define IOMUX_CHECK_LIMIT(_nfds)                                \
do {                                                              \
    if (_nfds >= IOMUX_MAX_POLLED_FDS)                            \
    {                                                             \
        ERROR("%s(): failed to add file descriptor to the list "  \
              "for %s(), it has reached the limit %d",            \
              __FUNCTION__, iomux2str(iomux),                     \
              IOMUX_MAX_POLLED_FDS);                              \
        errno = ENOSPC;                                           \
        return -1;                                                \
    }                                                             \
} while(0)


    switch (iomux)
    {
        case FUNC_SELECT:
        case FUNC_PSELECT:
            IOMUX_CHECK_LIMIT(state->select.nfds);
            iomux_select_set_state(state, fd, events, FALSE);
            state->select.maxfds = MAX(state->select.maxfds, fd);
            state->select.fds[state->select.nfds] = fd;
            state->select.nfds++;
            break;

        case FUNC_POLL:
        case FUNC_PPOLL:
            IOMUX_CHECK_LIMIT(state->poll.nfds);
            state->poll.fds[state->poll.nfds].fd = fd;
            state->poll.fds[state->poll.nfds].events = events;
            state->poll.nfds++;
            break;

#if HAVE_STRUCT_EPOLL_EVENT
        case FUNC_EPOLL:
        case FUNC_EPOLL_PWAIT:
        {
            struct epoll_event ev;
            ev.events = events;
            ev.data.fd = fd;
            return funcs->epoll.ctl(state->epoll, EPOLL_CTL_ADD, fd, &ev);
        }
#endif
        case FUNC_DEFAULT_IOMUX:
            ERROR("%s() function can't be used with default iomux",
                  __FUNCTION__);
            return -1;

        default:
            ERROR("Incorrect value of iomux function");
            return -1;
    }

#undef IOMUX_CHECK_LIMIT

    return 0;
}

/** Modify events for already-watched fds. */
static inline int
iomux_mod_fd(iomux_func iomux, iomux_funcs *funcs, iomux_state *state,
             int fd, int events)
{
    switch (iomux)
    {
        case FUNC_SELECT:
        case FUNC_PSELECT:
            iomux_select_set_state(state, fd, events, TRUE);
            return 0;

        case FUNC_POLL:
        case FUNC_PPOLL:
        {
            int i;

            for (i = 0; i < state->poll.nfds; i++)
            {
                if (state->poll.fds[i].fd != fd)
                    continue;
                state->poll.fds[i].events = events;
                return 0;
            }
            errno = ENOENT;
            return -1;
        }

#if HAVE_STRUCT_EPOLL_EVENT
        case FUNC_EPOLL:
        case FUNC_EPOLL_PWAIT:
        {
            struct epoll_event ev;
            ev.events = events;
            ev.data.fd = fd;
            return funcs->epoll.ctl(state->epoll, EPOLL_CTL_MOD, fd, &ev);
            break;
        }
#endif
        case FUNC_DEFAULT_IOMUX:
            ERROR("%s() function can't be used with default iomux",
                  __FUNCTION__);
            return -1;

        default:
            ERROR("Incorrect value of iomux function");
            return -1;

    }
    return 0;
}

/* iomux_return may be null if user is not interested in the event list
 * (for example, when only one event is possible) */
static inline int
iomux_wait(iomux_func iomux, iomux_funcs *funcs, iomux_state *state,
           iomux_return *ret, int timeout)
{
    int rc;

    INFO("%s: %s, timeout=%d", __FUNCTION__, iomux2str(iomux), timeout);
    switch (iomux)
    {
        case FUNC_SELECT:
        case FUNC_PSELECT:
        {
            iomux_return   sret;

            if (ret == NULL)
                ret = &sret;

            memcpy(&ret->select.rfds, &state->select.rfds,
                   sizeof(state->select.rfds));
            memcpy(&ret->select.wfds, &state->select.wfds,
                   sizeof(state->select.wfds));
            memcpy(&ret->select.exfds, &state->select.exfds,
                   sizeof(state->select.exfds));
            if (iomux == FUNC_SELECT)
            {
                struct timeval tv;
                tv.tv_sec = timeout / 1000UL;
                tv.tv_usec = timeout % 1000UL;
                rc = funcs->select(state->select.maxfds + 1,
                                   &ret->select.rfds,
                                   &ret->select.wfds,
                                   &ret->select.exfds,
                                   &tv);
            }
            else /* FUNC_PSELECT */
            {
                struct timespec ts;
                ts.tv_sec = timeout / 1000UL;
                ts.tv_nsec = (timeout % 1000UL) * 1000UL;
                rc = funcs->select(state->select.maxfds + 1,
                                   &ret->select.rfds,
                                   &ret->select.wfds,
                                   &ret->select.exfds,
                                   &ts, NULL);
            }
#if 0
            ERROR("got %d: %x %x %x", rc,
                  ((int *)&ret->select.rfds)[0],
                  ((int *)&ret->select.wfds)[0],
                  ((int *)&ret->select.exfds)[0]
                  );
#endif
            break;
        }
        case FUNC_POLL:
            rc = funcs->poll(&state->poll.fds[0], state->poll.nfds,
                             timeout);
            break;

        case FUNC_PPOLL:
        {
            struct timespec ts;
            ts.tv_sec = timeout / 1000UL;
            ts.tv_nsec = (timeout % 1000UL) * 1000UL;
            rc = funcs->poll(&state->poll.fds[0], state->poll.nfds,
                             &ts, NULL);
            break;
        }
#if HAVE_STRUCT_EPOLL_EVENT
        case FUNC_EPOLL:
        case FUNC_EPOLL_PWAIT:
        {
            if (ret != NULL)
            {
                rc = (iomux == FUNC_EPOLL) ?
                    funcs->epoll.wait(state->epoll, &ret->epoll.events[0],
                                      IOMUX_MAX_POLLED_FDS, timeout) :
                    funcs->epoll.wait(state->epoll, &ret->epoll.events[0],
                                      IOMUX_MAX_POLLED_FDS, timeout, NULL);
                ret->epoll.nevents = rc;
            }
            else
            {
                struct epoll_event ev[IOMUX_MAX_POLLED_FDS];
                rc = (iomux == FUNC_EPOLL) ?
                        funcs->epoll.wait(state->epoll, ev,
                                          IOMUX_MAX_POLLED_FDS, timeout) :
                        funcs->epoll.wait(state->epoll, ev,
                                          IOMUX_MAX_POLLED_FDS, timeout,
                                          NULL);
            }
            break;
        }
#endif

        default:
            errno = ENOENT;
            rc = -1;
    }
    INFO("%s done: %s, rc=%d", __FUNCTION__, iomux2str(iomux), rc);

    return rc;
}

/** Iterate through all iomux result and return fds and events.  See also
 * IOMUX_RETURN_ITERATOR_START and IOMUX_RETURN_ITERATOR_END. */
static inline iomux_return_iterator
iomux_return_iterate(iomux_func iomux, iomux_state *st, iomux_return *ret,
                     iomux_return_iterator it, int *p_fd, int *p_events)
{
    INFO("%s: %s, it=%d", __FUNCTION__, iomux2str(iomux), it);
    switch (iomux)
    {
        case FUNC_SELECT:
        case FUNC_PSELECT:
        {
            int i;
            int events = 0;

            for (i = it; i < st->select.nfds; i++)
            {
                int fd = st->select.fds[i];

                /* TODO It is incorrect, but everything works.
                 * In any case, we can't do better: POLLHUP is reported as
                 * part of rdset only... */
                if (FD_ISSET(fd, &ret->select.rfds))
                    events |= IOMUX_SELECT_READ;
                if (FD_ISSET(fd, &ret->select.wfds))
                    events |= IOMUX_SELECT_WRITE;
                if (FD_ISSET(fd, &ret->select.exfds))
                    events |= IOMUX_SELECT_EXCEPT;
                if (events != 0)
                {
                    *p_fd = fd;
                    *p_events = events;
                    it = i + 1;
                    goto out;
                }
            }
            it = IOMUX_RETURN_ITERATOR_END;
            break;
        }

        case FUNC_POLL:
        case FUNC_PPOLL:
        {
            int i;

            for (i = it; i < st->poll.nfds; i++)
            {
                if (st->poll.fds[i].revents == 0)
                    continue;
                *p_fd = st->poll.fds[i].fd;
                *p_events = st->poll.fds[i].revents;
                it = i + 1;
                goto out;
            }
            it = IOMUX_RETURN_ITERATOR_END;
            break;
        }

#if HAVE_STRUCT_EPOLL_EVENT
        case FUNC_EPOLL:
        case FUNC_EPOLL_PWAIT:
            if (it >= ret->epoll.nevents)
            {
                it = IOMUX_RETURN_ITERATOR_END;
                break;
            }
            *p_fd = ret->epoll.events[it].data.fd;
            *p_events = ret->epoll.events[it].events;
            it++;
            break;
#endif
        default:
            it = IOMUX_RETURN_ITERATOR_END;
    }
out:
    INFO("%s done: %s, it=%d", __FUNCTION__, iomux2str(iomux), it);
    return it;
}

/** Close iomux state when necessary. */
static inline int
iomux_close(iomux_func iomux, iomux_funcs *funcs, iomux_state *state)
{
#if HAVE_STRUCT_EPOLL_EVENT
    if (iomux == FUNC_EPOLL || iomux == FUNC_EPOLL_PWAIT)
        return funcs->epoll.close(state->epoll);
#endif
    return 0;
}

/*-------------- simple_sender() -------------------------*/
/**
 * Simple sender.
 *
 * @param in                input RPC argument
 *
 * @return number of sent bytes or -1 in the case of failure
 */
int
simple_sender(tarpc_simple_sender_in *in, tarpc_simple_sender_out *out)
{
    int         errno_save = errno;
    api_func    send_func;
    char       *buf;

    int size = rand_range(in->size_min, in->size_max);
    int delay = rand_range(in->delay_min, in->delay_max);

    time_t start;
    time_t now;

#ifdef TA_DEBUG
    uint64_t control = 0;
#endif

    out->bytes = 0;

    RING("%s() started", __FUNCTION__);

    if (in->size_min > in->size_max || in->delay_min > in->delay_max)
    {
        ERROR("Incorrect size or delay parameters");
        return -1;
    }

    if (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0)
        return -1;

    if ((buf = malloc(in->size_max)) == NULL)
    {
        ERROR("Out of memory");
        return -1;
    }

    memset(buf, 'A', in->size_max);

    for (start = now = time(NULL);
         (unsigned int)(now - start) <= in->time2run;
         now = time(NULL))
    {
        int len;

        if (!in->size_rnd_once)
            size = rand_range(in->size_min, in->size_max);

        if (!in->delay_rnd_once)
            delay = rand_range(in->delay_min, in->delay_max);

        if (TE_US2SEC(delay) > (int)(in->time2run) - (now - start) + 1)
            break;

        usleep(delay);

        len = send_func(in->s, buf, size, 0);

        if (len < 0)
        {
            if (!in->ignore_err)
            {
                ERROR("send() failed in simple_sender(): errno %s(%x)",
                      strerror(errno), errno);
                free(buf);
                return -1;
            }
            else
            {
                len = errno = 0;
                continue;
            }
        }
        out->bytes += len;
    }

    RING("simple_sender() stopped, sent %llu bytes",
         out->bytes);

    free(buf);

    /* Clean up errno */
    errno = errno_save;

    return 0;
}

TARPC_FUNC(simple_sender, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*--------------simple_receiver() --------------------------*/
#define MAX_PKT (1024 * 1024)

/**
 * Simple receiver.
 *
 * @param in                input RPC argument
 *
 * @return number of received bytes or -1 in the case of failure
 */
int
simple_receiver(tarpc_simple_receiver_in *in,
                tarpc_simple_receiver_out *out)
{
    iomux_funcs     iomux_f;
    api_func        recv_func;
    char           *buf;
    int             rc;
    ssize_t         len;
    iomux_func      iomux = get_default_iomux();

    time_t          start;
    time_t          now;

    iomux_state             iomux_st;
    iomux_return            iomux_ret;

    int                     fd = -1;
    int                     events = 0;

    out->bytes = 0;

    RING("%s() started", __FUNCTION__);

    if (iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0 ||
        tarpc_find_func(in->common.use_libc, "recv", &recv_func) != 0)
    {
        ERROR("failed to resolve function(s)");
        return -1;
    }

    if ((buf = malloc(MAX_PKT)) == NULL)
    {
        ERROR("Out of memory");
        return -1;
    }

    /* Create iomux status and fill it with our fds. */
    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
        return rc;
    if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->s, POLLIN)))
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    for (start = now = time(NULL);
         (in->time2run != 0) ?
          ((unsigned int)(now - start) <= in->time2run) : TRUE;
         now = time(NULL))
    {
        rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                        1000);
        if (rc < 0 || rc > 1)
        {
            if (rc < 0)
                ERROR("%s() failed in %s(): errno %r",
                      iomux2str(iomux), __FUNCTION__,
                      TE_OS_RC(TE_TA_UNIX, errno));
            else
                ERROR("%s() returned more then one fd",
                      iomux2str(iomux));
            free(buf);
            return -1;
        }
        else if (rc == 0)
        {
            if ((in->time2run != 0) || (out->bytes == 0))
                continue;
            else
                break;
        }

        iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                             IOMUX_RETURN_ITERATOR_START, &fd, &events);

        if (fd != in->s || !(events & POLLIN))
        {
            ERROR("%s() returned strange event or socket",
                  iomux2str(iomux));
            free(buf);
            return -1;
        }

        len = recv_func(in->s, buf, MAX_PKT, 0);
        if (len < 0)
        {
            ERROR("recv() failed in %s(): errno %r",
                  __FUNCTION__, TE_OS_RC(TE_TA_UNIX, errno));
            free(buf);
            return -1;
        }
        if (len == 0)
        {
            RING("recv() returned 0 in %s() because of "
                 "peer shutdown", __FUNCTION__);
            break;
        }

        if (out->bytes == 0)
            RING("First %d bytes are received", len);
        out->bytes += len;
    }

    free(buf);
    RING("%s() stopped, received %llu bytes", __FUNCTION__, out->bytes);

    return 0;
}

#undef MAX_PKT

TARPC_FUNC(simple_receiver, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*--------------wait_readable() --------------------------*/
/**
 * Wait until the socket becomes readable.
 *
 * @param in                input RPC argument
 *
 * @return number of received bytes or -1 in the case of failure
 */
int
wait_readable(tarpc_wait_readable_in *in,
              tarpc_wait_readable_out *out)
{
    iomux_funcs     iomux_f;
    int             rc;
    iomux_func      iomux = get_default_iomux();

    iomux_state             iomux_st;
    iomux_return            iomux_ret;

    int                     fd = -1;
    int                     events = 0;

    UNUSED(out);

    RING("%s() started", __FUNCTION__);

    if (iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
    {
        return -1;
    }

    /* Create iomux status and fill it with our fds. */
    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
        return rc;
    if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->s, POLLIN)))
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                    in->timeout);
    if (rc < 0)
    {
        ERROR("%s() failed in wait_readable(): errno %r",
              iomux2str(iomux), TE_OS_RC(TE_TA_UNIX, errno));
        return -1;
    }
    else if ((rc > 0) && (fd != in->s || !(events & POLLIN)))
    {
        ERROR("%s() waited for reading on the socket, "
              "returned %d, but returned incorrect socket or event",
              iomux2str(iomux), rc);
        return -1;
    }

    return rc;
}

TARPC_FUNC(wait_readable, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- recv_verify() --------------------------*/
#define RCV_VF_BUF (1024)

/**
 * Simple receiver.
 *
 * @param in                input RPC argument
 *
 * @return number of received bytes or -1 in the case of failure
 */
int
recv_verify(tarpc_recv_verify_in *in, tarpc_recv_verify_out *out)
{
    api_func   recv_func;
    char           *rcv_buf;
    char           *pattern_buf;
    int             rc;

    out->retval = 0;

    RING("%s() started", __FUNCTION__);

    if (tarpc_find_func(in->common.use_libc, "recv", &recv_func) != 0)
    {
        return -1;
    }

    if ((rcv_buf = malloc(RCV_VF_BUF)) == NULL)
    {
        ERROR("Out of memory");
        return -1;
    }

    while (1)
    {
        rc = recv_func(in->s, rcv_buf, RCV_VF_BUF, MSG_DONTWAIT);
        if (rc < 0)
        {
            if (errno == EAGAIN)
            {
                errno = 0;
                RING("recv() returned -1(EGAIN) in recv_verify(), "
                     "no more data just now");
                break;
            }
            else
            {
                ERROR("recv() failed in recv_verify(): errno %x", errno);
                free(rcv_buf);
                out->retval = -1;
                return -1;
            }
        }
        if (rc == 0)
        {
            RING("recv() returned 0 in recv_verify() because of "
                 "peer shutdown");
            break;
        }

        /* TODO: check data here, set reval to -2 if not matched. */
        UNUSED(pattern_buf);
        out->retval += rc;
    }

    free(rcv_buf);
    RING("recv_verify() stopped, received %d bytes", out->retval);

    return 0;
}

#undef RCV_VF_BUF

TARPC_FUNC(recv_verify, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- flooder() --------------------------*/
#define FLOODER_ECHOER_WAIT_FOR_RX_EMPTY        1
#define FLOODER_BUF                             4096

/**
 * Routine which receives data from specified set of sockets and sends data
 * to specified set of sockets with maximum speed using I/O multiplexing.
 *
 * @param pco       - PCO to be used
 * @param rcvrs     - set of receiver sockets
 * @param rcvnum    - number of receiver sockets
 * @param sndrs     - set of sender sockets
 * @param sndnum    - number of sender sockets
 * @param bulkszs   - sizes of data bulks to send for each sender
 *                    (in bytes, 1024 bytes maximum)
 * @param time2run  - how long send data (in seconds)
 * @param time2wait - how long wait data (in seconds)
 * @param iomux     - type of I/O Multiplexing function
 *                    (@b select(), @b pselect(), @b poll())
 *
 * @return 0 on success or -1 in the case of failure
 */
int
flooder(tarpc_flooder_in *in)
{
    int errno_save = errno;

    iomux_funcs iomux_f;
    api_func send_func;
    api_func recv_func;
    api_func ioctl_func;

    int        *rcvrs = in->rcvrs.rcvrs_val;
    int         rcvnum = in->rcvrs.rcvrs_len;
    int        *sndrs = in->sndrs.sndrs_val;
    int         sndnum = in->sndrs.sndrs_len;
    int         bulkszs = in->bulkszs;
    int         time2run = in->time2run;
    int         time2wait = in->time2wait;
    iomux_func  iomux = in->iomux;

    uint64_t   *tx_stat = in->tx_stat.tx_stat_val;
    uint64_t   *rx_stat = in->rx_stat.rx_stat_val;

    int      i;
    int      j;
    int      rc;
    char     rcv_buf[FLOODER_BUF];
    char     snd_buf[FLOODER_BUF];

    iomux_state             iomux_st;
    iomux_return            iomux_ret;
    iomux_return_iterator   it;

    struct timeval  timeout;   /* time when we should go out */
    int             iomux_timeout;
    te_bool         time2run_expired = FALSE;
    te_bool         session_rx;

    INFO("%d flooder start", getpid());
    memset(rcv_buf, 0x0, FLOODER_BUF);
    memset(snd_buf, 'X', FLOODER_BUF);

    if ((iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)    ||
        (tarpc_find_func(in->common.use_libc, "recv", &recv_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "ioctl", &ioctl_func) != 0))
    {
        ERROR("failed to resolve function");
        return -1;
    }

    if (bulkszs > (int)sizeof(snd_buf))
    {
        ERROR("Size of sent data is too long");
        return -1;
    }
    /* Create iomux status and fill it with our fds. */
    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }
    for (i = 0; i < sndnum; i++)
    {
        if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                               sndrs[i], POLLOUT)))
        {
            iomux_close(iomux, &iomux_f, &iomux_st);
            return rc;
        }
    }
    for (i = 0; i < rcvnum; i++)
    {
        int found = FALSE;

        for (j = 0; j < sndnum; j++)
        {
            if (sndrs[j] != rcvrs[i])
                continue;
            if ((rc = iomux_mod_fd(iomux, &iomux_f, &iomux_st,
                                   rcvrs[i], POLLIN | POLLOUT)))
            {
                iomux_close(iomux, &iomux_f, &iomux_st);
                return rc;
            }
            found = TRUE;
            break;
        }

        if (!found &&
            (rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                               rcvrs[i], POLLIN)))
        {
            iomux_close(iomux, &iomux_f, &iomux_st);
            return rc;
        }
    }

    if (gettimeofday(&timeout, NULL))
    {
        ERROR("%s(): gettimeofday(timeout) failed: %d",
              __FUNCTION__, errno);
        return -1;
    }
    timeout.tv_sec += time2run;
    iomux_timeout = TE_SEC2MS(time2run);

    INFO("%s(): time2run=%d, timeout=%ld.%06ld", __FUNCTION__,
         time2run, (long)timeout.tv_sec, (long)timeout.tv_usec);

    do {
        int fd = -1;    /* Shut up compiler warning */
        int events = 0; /* Shut up compiler warning */

        session_rx = FALSE;
        rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                        iomux_timeout);

        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            ERROR("%s(): %s wait failed: %d", __FUNCTION__,
                  iomux2str(iomux), errno);
            iomux_close(iomux, &iomux_f, &iomux_st);
            return -1;
        }

        it = IOMUX_RETURN_ITERATOR_START;
        for (it = iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                                       it, &fd, &events);
             it != IOMUX_RETURN_ITERATOR_END;
             it = iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                                       it, &fd, &events))
        {
            int sent;
            int received;
            int eperm_cnt = 0;

            if (!time2run_expired && (events & POLLOUT))
            {
                sent = send_func(fd, snd_buf, bulkszs, 0);
                while ((sent < 0) && (errno == EPERM) &&
                       (++eperm_cnt) < 10)
                {
                    /* Don't stop on EPERM, but report it */
                    if (eperm_cnt == 1)
                        ERROR("%s(): send(%d) failed: %d",
                              __FUNCTION__, fd, errno);
                    usleep(10000);
                    sent = send_func(fd, snd_buf, bulkszs, 0);
                }

                if ((sent < 0) && (errno != EINTR) &&
                    (errno != EAGAIN) && (errno != EWOULDBLOCK))
                {
                    ERROR("%s(): send(%d) failed: %d",
                          __FUNCTION__, fd, errno);
                    iomux_close(iomux, &iomux_f, &iomux_st);
                    return -1;
                }
                else if ((sent > 0) && (tx_stat != NULL))
                {
                    for (i = 0; i < sndnum; i++)
                    {
                        if (sndrs[i] != fd)
                            continue;
                        tx_stat[i] += sent;
                        break;
                    }
                }
            }
            if ((events & POLLIN))
            {
                /* We use recv() instead of read() here to avoid false
                 * positives from iomux functions.  On linux, select()
                 * sometimes return false read events.
                 * Such misbihaviour may be tested in separate functions,
                 * not here. */
                received = recv_func(fd, rcv_buf, sizeof(rcv_buf),
                                     MSG_DONTWAIT);
                if ((received < 0) && (errno != EINTR) &&
                    (errno != EAGAIN) && (errno != EWOULDBLOCK))
                {
                    ERROR("%s(): recv(%d) failed: %d",
                          __FUNCTION__, fd, errno);
                    iomux_close(iomux, &iomux_f, &iomux_st);
                    return -1;
                }
                else if (received > 0)
                {
                    session_rx = TRUE;
                    if (rx_stat != NULL)
                    {
                        for (i = 0; i < rcvnum; i++)
                        {
                            if (rcvrs[i] != fd)
                                continue;
                            rx_stat[i] += received;
                            break;
                        }
                    }
                    if (time2run_expired)
                        VERB("FD=%d Rx=%d", fd, received);
                }
            }
#ifdef DEBUG
            if ((time2run_expired) && ((events & POLLIN)))
            {
                WARN("%s() returned unexpected events: 0x%x",
                     iomux2str(iomux), events);
            }
#endif
        }

        if (!time2run_expired)
        {
            struct timeval now;

            if (gettimeofday(&now, NULL))
            {
                ERROR("%s(): gettimeofday(now) failed): %d",
                      __FUNCTION__, errno);
                iomux_close(iomux, &iomux_f, &iomux_st);
                return -1;
            }
            iomux_timeout = TE_SEC2MS(timeout.tv_sec  - now.tv_sec) +
                TE_US2MS(timeout.tv_usec - now.tv_usec);
            if (iomux_timeout < 0)
            {
                time2run_expired = TRUE;

                /* Clean up POLLOUT requests for all descriptors */
                for (i = 0; i < sndnum; i++)
                {
                    fd = sndrs[i];
                    events = 0;

                    for (j = 0; j < rcvnum; j++)
                    {
                        if (sndrs[i] != rcvrs[j])
                            continue;
                        events = POLLIN;
                        break;
                    }
                    if (iomux_mod_fd(iomux, &iomux_f, &iomux_st,
                                     fd, events) != 0)
                    {
                            ERROR("%s(): iomux_mod_fd() function failed "
                                  "with iomux=%s", __FUNCTION__,
                                  iomux2str(iomux));
                            iomux_close(iomux, &iomux_f, &iomux_st);
                            return -1;
                    }
                }

                /* Just to make sure that we'll get all from buffers */
                session_rx = TRUE;
                INFO("%s(): time2run expired", __FUNCTION__);
            }
        }

        if (time2run_expired)
        {
            iomux_timeout = TE_SEC2MS(time2wait);
            VERB("%s(): Waiting for empty Rx queue, Rx=%d",
                 __FUNCTION__, session_rx);
        }

    } while (!time2run_expired || session_rx);

    iomux_close(iomux, &iomux_f, &iomux_st);
    INFO("%s(): OK", __FUNCTION__);

    /* Clean up errno */
    errno = errno_save;

    return 0;
}

TARPC_FUNC(flooder, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
    COPY_ARG(tx_stat);
    COPY_ARG(rx_stat);
}
)

/*-------------- echoer() --------------------------*/

typedef struct buffer {
    TAILQ_ENTRY(buffer)    links;
    char                   buf[FLOODER_BUF];
    int                    size;
} buffer;

typedef TAILQ_HEAD(buffers, buffer) buffers;

/**
 * Routine to free buffers queue.
 *
 * @param p     Buffers queue head pointer
 */
void free_buffers(buffers *p)
{
    buffer  *q;

    if (p == NULL)
        return;

    while ((q = TAILQ_FIRST(p)) != NULL)
    {
        TAILQ_REMOVE(p, q, links);
        free(q);
    }
}

/**
 * Routine which receives data from specified set of
 * sockets using I/O multiplexing and sends them back
 * to the socket.
 *
 * @param pco       - PCO to be used
 * @param sockets   - set of sockets to be processed
 * @param socknum   - number of sockets to be processed
 * @param time2run  - how long send data (in seconds)
 * @param iomux     - type of I/O Multiplexing function
 *                    (@b select(), @b pselect(), @b poll())
 *
 * @return 0 on success or -1 in the case of failure
 */
int
echoer(tarpc_echoer_in *in)
{
    iomux_funcs iomux_f;
    api_func write_func;
    api_func read_func;

    int        *sockets = in->sockets.sockets_val;
    int         socknum = in->sockets.sockets_len;
    int         time2run = in->time2run;

    uint64_t   *tx_stat = in->tx_stat.tx_stat_val;
    uint64_t   *rx_stat = in->rx_stat.rx_stat_val;
    iomux_func  iomux = in->iomux;

    int      i;
    int      rc;

    buffers                 buffs;
    buffer                 *buf = NULL;

    iomux_state             iomux_st;
    iomux_return            iomux_ret;
    iomux_return_iterator   it;

    struct timeval  timeout;   /* time when we should go out */
    int             iomux_timeout;
    te_bool         time2run_expired = FALSE;
    te_bool         session_rx;

    TAILQ_INIT(&buffs);

    if ((iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)    ||
        (tarpc_find_func(in->common.use_libc, "read", &read_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "write", &write_func) != 0))
    {
        return -1;
    }

    /* Create iomux status and fill it with our fds. */
    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    for (i = 0; i < socknum; i++)
    {
        if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                               sockets[i], POLLIN | POLLOUT)) != 0)
        {
            ERROR("%s(): failed to add fd to iomux list", __FUNCTION__);
            iomux_close(iomux, &iomux_f, &iomux_st);
            return rc;
        }
    }

    if (gettimeofday(&timeout, NULL))
    {
        ERROR("%s(): gettimeofday(timeout) failed: %d",
              __FUNCTION__, errno);
        iomux_close(iomux, &iomux_f, &iomux_st);
        return -1;
    }
    timeout.tv_sec += time2run;
    iomux_timeout = TE_SEC2MS(time2run);

    INFO("%s(): time2run=%d, timeout timestamp=%ld.%06ld", __FUNCTION__,
         time2run, (long)timeout.tv_sec, (long)timeout.tv_usec);

    do {
        int fd = -1;
        int events = 0;

        session_rx = FALSE;
        rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                        iomux_timeout);

        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            ERROR("%s(): %spoll() failed: %d", __FUNCTION__,
                  iomux2str(iomux), errno);
            iomux_close(iomux, &iomux_f, &iomux_st);
            free_buffers(&buffs);
            return -1;
        }

        for (it = IOMUX_RETURN_ITERATOR_START;
             it != IOMUX_RETURN_ITERATOR_END;
             it = iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                                       it, &fd, &events))
        {
            int sent = 0;
            int received = 0;

            if ((events & POLLIN))
            {
                buf = TE_ALLOC(sizeof(*buf));
                if (buf == NULL)
                {
                    ERROR("%s(): out of memory", __FUNCTION__);
                    iomux_close(iomux, &iomux_f, &iomux_st);
                    free_buffers(&buffs);
                    return - 1;
                }

                TAILQ_INSERT_HEAD(&buffs, buf, links);
                received = buf->size = read_func(fd, buf->buf,
                                                 sizeof(buf->buf));
                if (received < 0)
                {
                    ERROR("%s(): read() failed: %d", __FUNCTION__, errno);
                    iomux_close(iomux, &iomux_f, &iomux_st);
                    free_buffers(&buffs);
                    return -1;
                }
                session_rx = TRUE;
            }
            if ((events & POLLOUT) &&
                (buf = TAILQ_LAST(&buffs, buffers)) != NULL)
            {
                sent = write_func(fd, buf->buf, buf->size);
                if (sent < 0)
                {
                    ERROR("%s(): write() failed: %d", __FUNCTION__, errno);
                    iomux_close(iomux, &iomux_f, &iomux_st);
                    free_buffers(&buffs);
                    return -1;
                }
                TAILQ_REMOVE(&buffs, buf, links);
                free(buf);
            }

            if ((received > 0 && rx_stat != NULL) ||
                (sent > 0 && tx_stat != NULL))
            {
                for (i = 0; i < socknum; i++)
                {
                    if (sockets[i] != fd)
                        continue;
                    if (rx_stat != NULL)
                        rx_stat[i] += received;
                    if (tx_stat != NULL)
                        tx_stat[i] += sent;
                    break;
                }
            }
        }

        if (!time2run_expired)
        {
            struct timeval now;

            if (gettimeofday(&now, NULL))
            {
                ERROR("%s(): gettimeofday(now) failed: %d",
                      __FUNCTION__, errno);
                iomux_close(iomux, &iomux_f, &iomux_st);
                free_buffers(&buffs);
                return -1;
            }
            iomux_timeout = TE_SEC2MS(timeout.tv_sec  - now.tv_sec) +
                TE_US2MS(timeout.tv_usec - now.tv_usec);
            if (iomux_timeout < 0)
            {
                time2run_expired = TRUE;
                /* Just to make sure that we'll get all from buffers */
                session_rx = TRUE;
                INFO("%s(): time2run expired", __FUNCTION__);
            }
        }

        if (time2run_expired)
        {
            iomux_timeout = FLOODER_ECHOER_WAIT_FOR_RX_EMPTY;
            VERB("%s(): Waiting for empty Rx queue", __FUNCTION__);
        }

    } while (!time2run_expired || session_rx);

    iomux_close(iomux, &iomux_f, &iomux_st);
    free_buffers(&buffs);
    INFO("%s(): OK", __FUNCTION__);

    return 0;
}

TARPC_FUNC(echoer, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
    COPY_ARG(tx_stat);
    COPY_ARG(rx_stat);
}
)

/*-------------- pattern_sender() --------------------------*/
/* Count of numbers in a sequence (should not be greater that 65280). */
#define SEQUENCE_NUM 10000
/* Period of a sequence */
#define SEQUENCE_PERIOD_NUM 255 + (SEQUENCE_NUM - 255) * 2

/**
 * Get nth element of a string which is a concatenation of
 * a periodic sequence 1, 2, 3, ..., SEQUENCE_PERIOD_NUM, 1, 2, ...
 * where numbers are written in a positional base 256 system.
 *
 * @param n     Number
 *
 * @return Character code
 */
static char
get_nth_elm(int n)
{
    int m;

    n = n % SEQUENCE_PERIOD_NUM + 1;

    if (n <= 255)
        return n;
    else
    {
        m = n - 256;
        return m % 2 == 0 ? (m / 2 / 255) + 1 : (m / 2) % 255 + 1;
    }
}

/**
 * Fill a buffer with values provided by @b get_nth_elm().
 *
 * @param buf       Buffer
 * @param size      Buffer size
 * @param start_n   Starting number in a sequence
 *
 * @return 0 on success
 */
te_errno
fill_buff_with_sequence(char *buf, int size, uint64_t start_n)
{
    int i;
    start_n = start_n % SEQUENCE_PERIOD_NUM;

    for (i = 0; i < size; i++)
    {
        buf[i] = get_nth_elm(start_n + i);
    }

    return 0;
}

/**
 * Pattern sender.
 *
 * @param in                input RPC argument
 * @param out               output RPC argument
 *
 * @return 0 on success or -1 in the case of failure
 */
int
pattern_sender(tarpc_pattern_sender_in *in, tarpc_pattern_sender_out *out)
{
    int             errno_save = errno;
    api_func_ptr    pattern_gen_func;
    api_func        send_func;
    iomux_funcs     iomux_f;
    char           *buf;
    iomux_func      iomux = in->iomux;

    int size = rand_range(in->size_min, in->size_max);
    int delay = rand_range(in->delay_min, in->delay_max);

    int fd = -1;
    int events = 0;
    int rc = 0;

    int                     iomux_timeout;
    iomux_state             iomux_st;
    iomux_return            iomux_ret;
    iomux_return_iterator   itr;

    struct timeval tv_start;
    struct timeval tv_now;

    out->bytes = 0;

    RING("%s() started", __FUNCTION__);

    if (in->size_min > in->size_max || in->delay_min > in->delay_max)
    {
        ERROR("Incorrect size or delay parameters");
        return -1;
    }

    if (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0 ||
        (pattern_gen_func =
                rcf_ch_symbol_addr(in->fname.fname_val, TRUE)) == NULL ||
        iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
        return -1;

    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->s, POLLOUT)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    if ((buf = malloc(in->size_max)) == NULL)
    {
        ERROR("Out of memory");
        return -1;
    }

#define PTRN_SEND_ERROR \
    do {                                             \
        iomux_close(iomux, &iomux_f, &iomux_st); \
        free(buf);                                   \
        return -1;                                   \
    } while (0)

#define MSEC_DIFF \
    (TE_SEC2MS(tv_now.tv_sec - tv_start.tv_sec) + \
     TE_US2MS(tv_now.tv_usec - tv_start.tv_usec))

    for (gettimeofday(&tv_start, NULL), gettimeofday(&tv_now, NULL);
         MSEC_DIFF <= (int)TE_SEC2MS(in->time2run);
         gettimeofday(&tv_now, NULL))
    {
        int len;

        if (!in->size_rnd_once)
            size = rand_range(in->size_min, in->size_max);

        if ((rc = pattern_gen_func(buf, size, out->bytes)) != 0)
        {
            ERROR("%s(): failed to generate a pattern", __FUNCTION__);
            PTRN_SEND_ERROR;
        }

        if (!in->delay_rnd_once)
            delay = rand_range(in->delay_min, in->delay_max);

        if (TE_US2MS(delay) > (int)TE_SEC2MS(in->time2run) - MSEC_DIFF)
            break;

        usleep(delay);
        gettimeofday(&tv_now, NULL);
        iomux_timeout = (int)TE_SEC2MS(in->time2run) - MSEC_DIFF;
        if (iomux_timeout <= 0)
            break;

        rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                        iomux_timeout);

        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            ERROR("%s(): %s wait failed: %d", __FUNCTION__,
                  iomux2str(iomux), errno);
            PTRN_SEND_ERROR;
        }
        else if (rc > 1)
        {
            ERROR("%s(): %s wait returned more then one fd", __FUNCTION__,
                  iomux2str(iomux));
            PTRN_SEND_ERROR;
        }
        else if (rc == 0)
            break;

        itr = IOMUX_RETURN_ITERATOR_START;
        itr = iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                                   itr, &fd, &events);
        if (fd != in->s)
        {
            ERROR("%s(): %s wait returned incorrect fd %d instead of %d",
                  __FUNCTION__, iomux2str(iomux), fd, in->s);
            PTRN_SEND_ERROR;
        }

        if (!(events & POLLOUT))
        {
            ERROR("%s(): %s wait successeed but the socket is "
                  "not writable", __FUNCTION__, iomux2str(iomux));
            PTRN_SEND_ERROR;
        }

        len = send_func(in->s, buf, size, 0);

        if (len < 0)
        {
            if (!in->ignore_err)
            {
                ERROR("send() failed in pattern_sender(): errno %s (%x)",
                      strerror(errno), errno);
                out->func_failed = TRUE;
                PTRN_SEND_ERROR;
            }
            else
            {
                len = errno = 0;
                continue;
            }
        }
        out->bytes += len;
    }
#undef PTRN_SEND_ERROR
#undef MSEC_DIFF

    RING("pattern_sender() stopped, sent %llu bytes",
         out->bytes);

    iomux_close(iomux, &iomux_f, &iomux_st);
    free(buf);

    /* Clean up errno */
    errno = errno_save;

    return 0;
}

TARPC_FUNC(pattern_sender, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- pattern_receiver() --------------------------*/
/**
 * Pattern receiver.
 *
 * @param in                input RPC argument
 * @param out               output RPC argument
 *
 * @return 0 on success, -2 in case of data not matching the pattern
 *         received or -1 in the case of another failure
 */
int
pattern_receiver(tarpc_pattern_receiver_in *in,
                 tarpc_pattern_receiver_out *out)
{
#define MAX_PKT (1024 * 1024)
    int             errno_save = errno;
    api_func_ptr    pattern_gen_func;
    api_func        recv_func;
    iomux_funcs     iomux_f;
    char           *buf;
    char           *check_buf;
    iomux_func      iomux = in->iomux;

    int fd = -1;
    int events = 0;
    int rc = 0;

    int                     iomux_timeout;
    iomux_state             iomux_st;
    iomux_return            iomux_ret;
    iomux_return_iterator   itr;

    struct timeval tv_start;
    struct timeval tv_now;

    out->bytes = 0;

    RING("%s() started", __FUNCTION__);

    if (tarpc_find_func(in->common.use_libc, "recv", &recv_func) != 0 ||
        (pattern_gen_func =
                rcf_ch_symbol_addr(in->fname.fname_val, TRUE)) == NULL ||
        iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
        return -1;

    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->s, POLLIN)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    if ((buf = malloc(MAX_PKT)) == NULL ||
        (check_buf = malloc(MAX_PKT)) == NULL)
    {
        ERROR("Out of memory");
        return -1;
    }

#define PTRN_RECV_ERROR \
    do {                                         \
        iomux_close(iomux, &iomux_f, &iomux_st); \
        free(buf);                               \
        return -1;                               \
    } while (0)

#define MSEC_DIFF \
    (TE_SEC2MS(tv_now.tv_sec - tv_start.tv_sec) + \
     TE_US2MS(tv_now.tv_usec - tv_start.tv_usec))

    for (gettimeofday(&tv_start, NULL), gettimeofday(&tv_now, NULL);
         MSEC_DIFF <= (int)TE_SEC2MS(in->time2run);
         gettimeofday(&tv_now, NULL))
    {
        int len;

        iomux_timeout = (int)TE_SEC2MS(in->time2run) - MSEC_DIFF;
        if (iomux_timeout <= 0)
            break;

        rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                        iomux_timeout);

        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            ERROR("%s(): %s wait failed: %d", __FUNCTION__,
                  iomux2str(iomux), errno);
            PTRN_RECV_ERROR;
        }
        else if (rc > 1)
        {
            ERROR("%s(): %s wait returned more then one fd", __FUNCTION__,
                  iomux2str(iomux));
            PTRN_RECV_ERROR;
        }
        else if (rc == 0)
            break;

        itr = IOMUX_RETURN_ITERATOR_START;
        itr = iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                                   itr, &fd, &events);
        if (fd != in->s)
        {
            ERROR("%s(): %s wait returned incorrect fd %d instead of %d",
                  __FUNCTION__, iomux2str(iomux), fd, in->s);
            PTRN_RECV_ERROR;
        }

        if (!(events & POLLIN))
        {
            ERROR("%s(): %s wait successeed but the socket is "
                  "not writable", __FUNCTION__, iomux2str(iomux));
            PTRN_RECV_ERROR;
        }

        len = recv_func(in->s, buf, MAX_PKT, MSG_DONTWAIT);

        if (len < 0)
        {
            int recv_errno = errno;

            ERROR("recv() failed in pattern_receiver(): errno %s (%x)",
                  strerror(errno), errno);
            out->func_failed = TRUE;
            if (recv_errno != ECONNRESET)
                PTRN_RECV_ERROR;
            else
                len = 0;
        }
        else
        {
            if ((rc = pattern_gen_func(check_buf, len, out->bytes)) != 0)
            {
                ERROR("%s(): failed to generate a pattern", __FUNCTION__);
                PTRN_RECV_ERROR;
            }

            if (memcmp(buf, check_buf, len) != 0)
            {
                ERROR("%s(): received data doesn't match a pattern",
                      __FUNCTION__);
                iomux_close(iomux, &iomux_f, &iomux_st);
                free(buf);
                return -2;
            }
        }

        out->bytes += len;
    }
#undef PTRN_RECV_ERROR
#undef MSEC_DIFF

    RING("pattern_receiver() stopped, received %llu bytes",
         out->bytes);

    iomux_close(iomux, &iomux_f, &iomux_st);
    free(buf);

    /* Clean up errno */
    errno = errno_save;

    return 0;
#undef MAX_PKT
}

TARPC_FUNC(pattern_receiver, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- sendfile() ------------------------------*/

#if (SIZEOF_OFF_T == 8)
typedef off_t   ta_off64_t;
#else
typedef uint64_t ta_off64_t;
#endif

/* FIXME: sort off the correct prototype */
TARPC_FUNC_DYNAMIC_UNSAFE(sendfile,
{
    COPY_ARG(offset);
},
{
    if (in->force64 == TRUE)
    {
        do {
            int         rc;
            api_func    real_func = func;
            api_func    func64;
            ta_off64_t  offset = 0;
            const char *real_func_name = "sendfile64";

            if ((rc = tarpc_find_func(in->common.use_libc,
                                      real_func_name, &func64)) == 0)
            {
                real_func = func64;
            }
            else if (sizeof(off_t) == 8)
            {
                INFO("Using sendfile() instead of sendfile64() since "
                     "sizeof(off_t) is 8");
                real_func_name = "sendfile";
            }
            else
            {
                ERROR("Cannot find sendfile64() function.\n"
                      "Unable to use sendfile() since sizeof(off_t) "
                      "is %u", (unsigned)sizeof(off_t));
                out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOENT);
                break;
            }

            assert(real_func != NULL);

            if (out->offset.offset_len > 0)
                offset = *out->offset.offset_val;

            VERB("Call %s(out=%d, int=%d, offset=%lld, count=%d)",
                 real_func_name, in->out_fd, in->in_fd,
                 (long long)offset, in->count);

            MAKE_CALL(out->retval =
                      real_func(in->out_fd, in->in_fd,
                                out->offset.offset_len == 0 ? NULL : &offset,
                                in->count));

            VERB("%s() returns %d, errno=%d, offset=%lld",
                 real_func_name, out->retval, errno, (long long)offset);

            if (out->offset.offset_len > 0)
                out->offset.offset_val[0] = (tarpc_off_t)offset;

        } while (0);
    }
    else
    {
        off_t offset = 0;

        if (out->offset.offset_len > 0)
            offset = *out->offset.offset_val;

        MAKE_CALL(out->retval =
            func(in->out_fd, in->in_fd,
                 out->offset.offset_len == 0 ? NULL : &offset,
                 in->count));
        if (out->offset.offset_len > 0)
            out->offset.offset_val[0] = (tarpc_off_t)offset;
    }
}
)

/*-------------- sendfile_via_splice() ------------------------------*/

tarpc_ssize_t
sendfile_via_splice(tarpc_sendfile_via_splice_in *in,
                    tarpc_sendfile_via_splice_out *out)
{
    api_func_ptr    pipe_func;
    api_func        splice_func;
    api_func        close_func;
    ssize_t         to_pipe;
    ssize_t         from_pipe = 0;
    int             pipefd[2];
    unsigned int    flags = 0;
    off_t           offset = 0;
    int             ret = 0;

#ifdef SPLICE_F_NONBLOCK
    flags = SPLICE_F_NONBLOCK | SPLICE_F_MOVE;
#endif

    if (tarpc_find_func(in->common.use_libc, "pipe",
                        (api_func *)&pipe_func) != 0)
    {
        ERROR("%s(): Failed to resolve pipe() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "splice", &splice_func) != 0)
    {
        ERROR("%s(): Failed to resolve splice() function", __FUNCTION__);
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "close", &close_func) != 0)
    {
        ERROR("%s(): Failed to resolve close() function", __FUNCTION__);
        return -1;
    }

    if (pipe_func(pipefd) != 0)
    {
        ERROR("pipe() failed with error %r", TE_OS_RC(TE_TA_UNIX, errno));
        return -1;
    }

    if (out->offset.offset_len > 0)
            offset = *out->offset.offset_val;
    if ((to_pipe = splice_func(in->in_fd,
                               out->offset.offset_len == 0 ? NULL : &offset,
                               pipefd[1], NULL, in->count, flags)) < 0)
    {
        ERROR("splice() to pipe failed with error %r",
              TE_OS_RC(TE_TA_UNIX, errno));
        ret = -1;
        goto sendfile_via_splice_exit;
    }
    if (out->offset.offset_len > 0)
            out->offset.offset_val[0] = (tarpc_off_t)offset;

    if ((from_pipe = splice_func(pipefd[0], NULL, in->out_fd, NULL,
                                 in->count, flags)) < 0)
    {
        ERROR("splice() from pipe failed with error %r",
              TE_OS_RC(TE_TA_UNIX, errno));
        ret = -1;
        goto sendfile_via_splice_exit;
    }
    if (to_pipe != from_pipe)
    {
        ERROR("Two splice() calls return different amount of data",
              TE_OS_RC(TE_TA_UNIX, EMSGSIZE));
        errno = EMSGSIZE;
        ret = -1;
    }
sendfile_via_splice_exit:
    if (close_func(pipefd[0]) < 0 ||
        close_func(pipefd[1]) < 0)
        ret = -1;
    return ret == -1 ? ret : from_pipe;
}

TARPC_FUNC(sendfile_via_splice,
{
    COPY_ARG(offset);
},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- splice() ------------------------------*/
TARPC_FUNC(splice,
{
    COPY_ARG(off_in);
    COPY_ARG(off_out);
},
{
    off_t off_in = 0;
    off_t off_out = 0;

    if (out->off_in.off_in_len > 0)
        off_in = *out->off_in.off_in_val;
    if (out->off_out.off_out_len > 0)
        off_out = *out->off_out.off_out_val;

    MAKE_CALL(out->retval =
        func(in->fd_in,
             out->off_in.off_in_len == 0 ? NULL : &off_in,
             in->fd_out,
             out->off_out.off_out_len == 0 ? NULL : &off_out,
             in->len, splice_flags_rpc2h(in->flags)));
    if (out->off_in.off_in_len > 0)
        out->off_in.off_in_val[0] = (tarpc_off_t)off_in;
    if (out->off_out.off_out_len > 0)
        out->off_out.off_out_val[0] = (tarpc_off_t)off_out;
}
)

/*-------------- socket_to_file() ------------------------------*/
#define SOCK2FILE_BUF_LEN  4096

/**
 * Routine which receives data from socket and write data
 * to specified path.
 *
 * @return -1 in the case of failure or some positive value in other cases
 */
int
socket_to_file(tarpc_socket_to_file_in *in)
{
    iomux_funcs  iomux_f;
    api_func     write_func;
    api_func     read_func;
    api_func     close_func;
    api_func_ptr open_func;
    iomux_func   iomux = get_default_iomux();

    int      sock = in->sock;
    char    *path = in->path.path_val;
    long     time2run = in->timeout;

    int      rc = 0;
    int      file_d = -1;
    int      written;
    int      received;
    size_t   total = 0;
    char     buffer[SOCK2FILE_BUF_LEN];

    iomux_state             iomux_st;
    iomux_return            iomux_ret;

    struct timeval  timeout;
    struct timeval  timestamp;
    int             iomux_timeout;
    te_bool         time2run_expired = FALSE;
    te_bool         session_rx;

    path[in->path.path_len] = '\0';

    INFO("%s() called with: sock=%d, path=%s, timeout=%ld",
         __FUNCTION__, sock, path, time2run);

    if ((iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0) ||
        (tarpc_find_func(in->common.use_libc, "read", &read_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "write", &write_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "close", &close_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "open",
                         (api_func *)&open_func) != 0))
    {
        ERROR("Failed to resolve functions addresses");
        rc = -1;
        goto local_exit;
    }

    file_d = open_func(path, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (file_d < 0)
    {
        ERROR("%s(): open(%s, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO) "
              "failed: %d", __FUNCTION__, path, errno);
        rc = -1;
        goto local_exit;
    }
    INFO("%s(): file '%s' opened with descriptor=%d", __FUNCTION__,
         path, file_d);

    /* Create iomux status and fill it with our fds. */
    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
    {
        rc = -1;
        goto local_exit;
    }
    if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           sock, POLLIN)) != 0)
        goto local_exit;

    if (gettimeofday(&timeout, NULL))
    {
        ERROR("%s(): gettimeofday(timeout) failed: %d",
              __FUNCTION__, errno);
        rc = -1;
        goto local_exit;
    }
    timeout.tv_sec += time2run;
    iomux_timeout = TE_SEC2MS(time2run);

    INFO("%s(): time2run=%ld, timeout timestamp=%ld.%06ld", __FUNCTION__,
         time2run, (long)timeout.tv_sec, (long)timeout.tv_usec);

    do {
        int fd = -1;
        int events = 0;
        session_rx = FALSE;

        rc = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                        iomux_timeout);
        if (rc < 0)
        {
            ERROR("%s(): %s() failed: %d", __FUNCTION__, iomux2str(iomux),
                  errno);
            break;
        }
        VERB("%s(): %s finishes for waiting of events", __FUNCTION__,
             iomux2str(iomux));

        iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                             IOMUX_RETURN_ITERATOR_START, &fd, &events);

        /* Receive data from socket that are ready */
        if (events & POLLIN)
        {
            VERB("%s(): %s observes data for reading on the "
                 "socket=%d", __FUNCTION__, iomux2str(iomux), sock);
            received = read_func(sock, buffer, sizeof(buffer));
            VERB("%s(): read() retrieve %d bytes", __FUNCTION__, received);
            if (received < 0)
            {
                ERROR("%s(): read() failed: %d", __FUNCTION__, errno);
                rc = -1;
                break;
            }
            else if (received > 0)
            {
                session_rx = TRUE;

                total += received;
                VERB("%s(): write retrieved data to file", __FUNCTION__);
                written = write_func(file_d, buffer, received);
                VERB("%s(): %d bytes are written to file",
                     __FUNCTION__, written);
                if (written < 0)
                {
                    ERROR("%s(): write() failed: %d", __FUNCTION__, errno);
                    rc = -1;
                    break;
                }
                if (written != received)
                {
                    ERROR("%s(): write() cannot write all received in "
                          "the buffer data to the file "
                          "(received=%d, written=%d): %d",
                          __FUNCTION__, received, written, errno);
                    rc = -1;
                    break;
                }
            }
        }

        if (!time2run_expired)
        {
            if (gettimeofday(&timestamp, NULL))
            {
                ERROR("%s(): gettimeofday(timestamp) failed): %d",
                      __FUNCTION__, errno);
                rc = -1;
                break;
            }
            iomux_timeout = TE_SEC2MS(timeout.tv_sec  - timestamp.tv_sec) +
                TE_US2MS(timeout.tv_usec - timestamp.tv_usec);
            if (iomux_timeout < 0)
            {
                time2run_expired = TRUE;
                /* Just to make sure that we'll get all from buffers */
                session_rx = TRUE;
                INFO("%s(): time2run expired", __FUNCTION__);
            }
#ifdef DEBUG
            else if (iomux_timeout < TE_SEC2MS(time2run))
            {
                VERB("%s(): timeout %d", __FUNCTION__, iomux_timeout);
                time2run >>= 1;
            }
#endif
        }

        if (time2run_expired)
        {
            iomux_timeout = TE_SEC2MS(FLOODER_ECHOER_WAIT_FOR_RX_EMPTY);
            VERB("%s(): Waiting for empty Rx queue, Rx=%d",
                 __FUNCTION__, session_rx);
        }

    } while (!time2run_expired || session_rx);

local_exit:
    iomux_close(iomux, &iomux_f, &iomux_st);
    RING("Stop to get data from socket %d and put to file %s, %s, "
         "received %u", sock, path,
         (time2run_expired) ? "timeout expired" :
                              "unexpected failure",
         total);
    INFO("%s(): %s", __FUNCTION__, (rc == 0) ? "OK" : "FAILED");

    if (file_d != -1)
        close_func(file_d);

    /* Clean up errno */
    if (rc == 0)
    {
        errno = 0;
        rc = total;
    }
    return rc;
}

TARPC_FUNC(socket_to_file, {},
{
   MAKE_CALL(out->retval = func_ptr(in));
}
)

/*-------------- ftp_open() ------------------------------*/

TARPC_FUNC(ftp_open, {},
{
    MAKE_CALL(out->fd = func_ptr(in->uri.uri_val,
                                 in->rdonly ? O_RDONLY : O_WRONLY,
                                 in->passive, in->offset,
                                 (in->sock.sock_len == 0) ? NULL:
                                     in->sock.sock_val));
    if (in->sock.sock_len > 0)
        out->sock = in->sock.sock_val[0];
}
)

/*-------------- ftp_close() ------------------------------*/

TARPC_FUNC(ftp_close, {},
{
    MAKE_CALL(out->ret = func(in->sock));
}
)

/*-------------- overfill_buffers() -----------------------------*/
int
overfill_buffers(tarpc_overfill_buffers_in *in,
                 tarpc_overfill_buffers_out *out)
{
    int             ret = 0;
    ssize_t         sent = 0;
    int             errno_save = errno;
    api_func        ioctl_func;
    api_func        send_func;
    iomux_funcs     iomux_f;
    iomux_func      iomux = in->iomux;
    size_t          max_len = 4096;
    uint8_t        *buf = NULL;
    uint64_t        total = 0;
    int             unchanged = 0;
    iomux_state     iomux_st;

    out->bytes = 0;

    buf = calloc(1, max_len);
    if (buf == NULL)
    {
        ERROR("%s(): Out of memory", __FUNCTION__);
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        ret = -1;
        goto overfill_buffers_exit;
    }

    memset(buf, 0xAD, sizeof(max_len));

    if (tarpc_find_func(in->common.use_libc, "ioctl", &ioctl_func) != 0)
    {
        ERROR("%s(): Failed to resolve ioctl() function", __FUNCTION__);
        ret = -1;
        goto overfill_buffers_exit;
    }

    if (tarpc_find_func(in->common.use_libc, "send", &send_func) != 0)
    {
        ERROR("%s(): Failed to resolve send() function", __FUNCTION__);
        ret = -1;
        goto overfill_buffers_exit;
    }

    if (iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
    {
        ERROR("%s(): Failed to resolve iomux %s function(s)",
              __FUNCTION__, iomux2str(iomux));
        ret = -1;
        goto overfill_buffers_exit;
    }
    iomux_state_init_invalid(iomux, &iomux_st);

#ifdef __sun__
    /* SunOS has MSG_DONTWAIT flag, but does not support it for send */
    if (!in->is_nonblocking)
    {
        int val = 1;

        if (ioctl_func(in->sock, FIONBIO, &val) != 0)
        {
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): ioctl() failed: %r", __FUNCTION__,
                  out->common._errno);
            ret = -1;
            goto overfill_buffers_exit;
        }
    }
#endif

    /* Create iomux status and fill it with out fd. */
    if ((ret = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0 ||
        (ret = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->sock, POLLOUT)) != 0)
    {
        ERROR("%s(): failed to set up iomux %s state", __FUNCTION__,
              iomux2str(iomux));
        goto overfill_buffers_exit;
    }

    /*
     * If total bytes is left unchanged after 3 attempts the socket
     * can be considered as not writable.
     */
    do {
        ret = iomux_wait(iomux, &iomux_f, &iomux_st, NULL, 1000);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue; /* probably, SIGCHLD */
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): select() failed", __FUNCTION__);
            goto overfill_buffers_exit;
        }

        sent = 0;
        do {
            out->bytes += sent;
            sent = send_func(in->sock, buf, max_len, MSG_DONTWAIT);
            if ((ret > 0) && (sent <= 0))
            {
                if (errno_h2rpc(errno) == RPC_EAGAIN)
                    ERROR("%s(): I/O multiplexing has returned write "
                          "event, but send() function with MSG_DONTWAIT "
                          "hasn't sent any data", __FUNCTION__);
                else
                    ERROR("Send operation failed with %r",
                          errno_h2rpc(errno));
                ret = -1;
                goto overfill_buffers_exit;
            }
            ret = 0;
        } while (sent > 0);
        if (errno != EAGAIN)
        {
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): send() failed", __FUNCTION__);
            goto overfill_buffers_exit;
        }

        if (total != out->bytes)
        {
            total = out->bytes;
            unchanged = 0;
        }
        else
        {
            unchanged++;
            ret = 0;
        }
    } while (unchanged != 4);

overfill_buffers_exit:
    iomux_close(iomux, &iomux_f, &iomux_st);

#ifdef __sun__
    if (!in->is_nonblocking)
    {
        int val = 0;

        if (ioctl_func(in->sock, FIONBIO, &val) != 0)
        {
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): ioctl() failed: %r", __FUNCTION__,
                  out->common._errno);
            ret = -1;
        }
    }
#endif

    free(buf);
    if (ret == 0)
        errno = errno_save;
    return ret;
}

TARPC_FUNC(overfill_buffers,{},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- iomux_splice() -----------------------------*/
int
iomux_splice(tarpc_iomux_splice_in *in,
             tarpc_iomux_splice_out *out)
{
    int                     ret = 0;
    api_func                splice_func;
    iomux_funcs             iomux_f;
    iomux_func              iomux = in->iomux;
    iomux_state             iomux_st;
    iomux_state             iomux_st_rd;
    iomux_return            iomux_ret;
    iomux_return_iterator   itr;
    struct timeval          now;
    struct timeval          end;
    te_bool                 out_ev = FALSE;
    int                     fd = -1;
    int                     events = 0;

    if (gettimeofday(&end, NULL))
    {
        ERROR("%s(): gettimeofday(now) failed): %d",
              __FUNCTION__, errno);
        return -1;
    }
    end.tv_sec += (time_t)(in->time2run);

    if (tarpc_find_func(in->common.use_libc, "splice", &splice_func) != 0)
    {
        ERROR("%s(): Failed to resolve splice() function", __FUNCTION__);
        ret = -1;
        goto iomux_splice_exit;
    }

    if (iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
    {
        ERROR("%s(): Failed to resolve iomux %s function(s)",
              __FUNCTION__, iomux2str(iomux));
        ret = -1;
        goto iomux_splice_exit;
    }
    iomux_state_init_invalid(iomux, &iomux_st);
    iomux_state_init_invalid(iomux, &iomux_st_rd);

    /* Create iomux status and fill it with in and out fds. */
    if ((ret = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0 ||
        (ret = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->fd_in, POLLIN)) != 0 ||
        (ret = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->fd_out, POLLOUT)) != 0)
    {
        ERROR("%s(): failed to set up iomux %s state", __FUNCTION__,
              iomux2str(iomux));
        goto iomux_splice_exit;
    }
    /* Create iomux status and fill it with in fd. */
    if ((ret = iomux_create_state(iomux, &iomux_f,
                                  &iomux_st_rd)) != 0 ||
        (ret = iomux_add_fd(iomux, &iomux_f, &iomux_st_rd,
                            in->fd_in, POLLIN)) != 0)
    {
        ERROR("%s(): failed to set up iomux %s state", __FUNCTION__,
              iomux2str(iomux));
        goto iomux_splice_exit;
    }

    do {
        if (out_ev)
            ret = iomux_wait(iomux, &iomux_f, &iomux_st_rd, NULL, 1000);
        else
            ret = iomux_wait(iomux, &iomux_f, &iomux_st, &iomux_ret,
                             1000);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue; /* probably, SIGCHLD */
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): %s() failed", __FUNCTION__, iomux2str(iomux));
            break;
        }

        if (gettimeofday(&now, NULL))
        {
            ERROR("%s(): gettimeofday(now) failed): %d",
                  __FUNCTION__, errno);
            ret = -1;
            break;
        }

        if (ret == 1 && !out_ev)
        {
            itr = IOMUX_RETURN_ITERATOR_START;
            itr = iomux_return_iterate(iomux, &iomux_st, &iomux_ret,
                                       itr, &fd, &events);
            if (!(events & POLLOUT))
            {
                usleep(10000);
                continue;
            }
            out_ev = TRUE;
            continue;
        }

        if (out_ev && ret == 0)
            continue;
        if (ret == 0)
        {
            usleep(10000);
            continue;
        }

        ret = splice_func(in->fd_in, NULL, in->fd_out, NULL, in->len,
                          splice_flags_rpc2h(in->flags));
        if (ret != (int)in->len)
        {
            ERROR("splice() returned %d instead of %d",
                  ret, in->len);
            ret = -1;
            break;
        }
        out_ev = FALSE;
    } while (end.tv_sec > now.tv_sec);

iomux_splice_exit:
    iomux_close(iomux, &iomux_f, &iomux_st);

    return (ret > 0) ? 0 : ret;
}

TARPC_FUNC(iomux_splice,{},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- overfill_fd() -----------------------------*/
int
overfill_fd(tarpc_overfill_fd_in *in,
              tarpc_overfill_fd_out *out)
{
    int             ret = 0;
    ssize_t         sent = 0;
    int             errno_save = errno;
    api_func        fcntl_func;
    api_func        write_func;
    size_t          max_len = 4096;
    uint8_t        *buf = NULL;
    int             fdflags = -1;

    buf = calloc(1, max_len);
    if (buf == NULL)
    {
        ERROR("%s(): Out of memory", __FUNCTION__);
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        ret = -1;
        goto overfill_fd_exit;
    }

    memset(buf, 0xAD, sizeof(max_len));

    if (tarpc_find_func(in->common.use_libc, "fcntl", &fcntl_func) != 0)
    {
        ERROR("%s(): Failed to resolve fcntl() function", __FUNCTION__);
        ret = -1;
        goto overfill_fd_exit;
    }

    if (tarpc_find_func(in->common.use_libc, "write", &write_func) != 0)
    {
        ERROR("%s(): Failed to resolve write() function", __FUNCTION__);
        ret = -1;
        goto overfill_fd_exit;
    }

    if ((fdflags = fcntl_func(in->write_end, F_GETFL, O_NONBLOCK)) == -1)
    {
        out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
        ERROR("%s(): fcntl(F_GETFL) failed: %r", __FUNCTION__,
              out->common._errno);
        ret = -1;
        goto overfill_fd_exit;
    }

    if (!(fdflags & O_NONBLOCK))
    {
        if (fcntl_func(in->write_end, F_SETFL, O_NONBLOCK) == -1)
        {
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): fcntl(F_SETFL) failed: %r", __FUNCTION__,
                  out->common._errno);
            ret = -1;
            goto overfill_fd_exit;
        }
    }

    sent = 0;
    do {
        out->bytes += sent;
        sent = write_func(in->write_end, buf, max_len);
    } while (sent > 0);

    if (errno != EAGAIN)
    {
        out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
        ERROR("%s(): write() failed", __FUNCTION__);
        goto overfill_fd_exit;
    }

overfill_fd_exit:

    if (fdflags != -1 && !(fdflags & O_NONBLOCK))
    {
        if (fcntl_func(in->write_end, F_SETFL, fdflags) == -1)
        {
            out->common._errno = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("%s(): cleanup fcntl(F_SETFL) failed: %r", __FUNCTION__,
                  out->common._errno);
            ret = -1;
        }
    }

    free(buf);
    if (ret == 0)
        errno = errno_save;
    return ret;
}

TARPC_FUNC(overfill_fd,{},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- multiple_iomux() ----------------------*/
int
multiple_iomux(tarpc_multiple_iomux_in *in,
               tarpc_multiple_iomux_out *out)
{
    iomux_funcs     iomux_f;
    iomux_func      iomux = in->iomux;
    iomux_state     iomux_st;
    int             ret;
    int             events;
    int             i;
    int             saved_errno = 0;
    int             zero_ret = 0;
    struct timeval  tv_start;
    struct timeval  tv_finish;

    if (iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
    {
        ERROR("%s(): Failed to resolve iomux %s function(s)",
              __FUNCTION__, iomux2str(iomux));
        return -1;
    }

    if ((ret = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
    {
        ERROR("%s(): failed to set up iomux %s state", __FUNCTION__,
              iomux2str(iomux));
        return -1;
    }

    events = poll_event_rpc2h(in->events);

    if ((ret = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                            in->fd, events)) != 0)
    {
        ERROR("%s(): failed to set up iomux %s state", __FUNCTION__,
              iomux2str(iomux));
        goto multiple_iomux_exit;
    }

    if (in->duration != -1)
        gettimeofday(&tv_start, NULL);

    for (i = 0; i < in->count || in->count == -1; i++)
    {
        ret = iomux_wait(iomux, &iomux_f, &iomux_st, NULL, 0);
        if (ret == 0)
            zero_ret++;
        else if (ret < 0)
        {
            saved_errno = errno;
            ERROR("%s(): iomux failed with errno %s",
                  strerror(saved_errno));
            break;
        }
        else if (ret != in->exp_rc)
        {
            ERROR("%s(): unexpected value %d was returned by iomux call",
                  __FUNCTION__, ret);
            break;
        }

        if (in->duration != -1)
        {
            gettimeofday(&tv_finish, NULL);
            if (in->duration < (tv_finish.tv_sec - tv_start.tv_sec) * 1000 +
                              (tv_finish.tv_usec - tv_start.tv_usec) / 1000)
            break;
        }
    }

    out->number = i;
    out->last_rc = ret;
    out->zero_rc = zero_ret;

multiple_iomux_exit:

    iomux_close(iomux, &iomux_f, &iomux_st);

    if (saved_errno != 0)
        errno = saved_errno;

    if (ret != in->exp_rc || out->number < in->count)
        return -1;

    return 0;
}

TARPC_FUNC(multiple_iomux,{},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

#ifdef LIO_READ

#ifdef HAVE_UNION_SIGVAL_SIVAL_PTR
#define SIVAL_PTR sival_ptr
#elif defined(HAVE_UNION_SIGVAL_SIGVAL_PTR)
#define SIVAL_PTR sigval_ptr
#else
#error "Failed to discover memeber names of the union sigval."
#endif

#ifdef HAVE_UNION_SIGVAL_SIVAL_INT
#define SIVAL_INT       sival_int
#elif defined(HAVE_UNION_SIGVAL_SIGVAL_INT)
#define SIVAL_INT       sigval_int
#else
#error "Failed to discover memeber names of the union sigval."
#endif

#ifdef SIGEV_THREAD
static te_errno
fill_sigev_thread(struct sigevent *sig, char *function)
{
    if (strlen(function) > 0)
    {
        if ((sig->sigev_notify_function =
                rcf_ch_symbol_addr(function, 1)) == NULL)
        {
            if (strcmp(function, AIO_WRONG_CALLBACK) == 0)
                sig->sigev_notify_function =
                    (void *)(long)rand_range(1, 0xFFFFFFFF);
            else
                WARN("Failed to find address of AIO callback %s - "
                     "use NULL callback", function);
        }
    }
    else
    {
        sig->sigev_notify_function = NULL;
    }
    sig->sigev_notify_attributes = NULL;

    return 0;
}
#else
static te_errno
fill_sigev_thread(struct sigevent *sig, char *function)
{
    UNUSED(sig);
    UNUSED(function);
    return TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
}
#endif

/*-------------- AIO control block constructor -------------------------*/
bool_t
_create_aiocb_1_svc(tarpc_create_aiocb_in *in, tarpc_create_aiocb_out *out,
                    struct svc_req *rqstp)
{
    struct aiocb *cb;

    UNUSED(rqstp);
    UNUSED(in);

    memset(out, 0, sizeof(*out));

    errno = 0;
    if ((cb = (struct aiocb *)malloc(sizeof(struct aiocb))) == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    else
    {
        memset((void *)cb, 0, sizeof(*cb));
        out->cb = rcf_pch_mem_alloc(cb);
        out->common._errno = RPC_ERRNO;
    }

    return TRUE;
}

/*-------------- AIO control block constructor --------------------------*/
bool_t
_fill_aiocb_1_svc(tarpc_fill_aiocb_in *in,
                  tarpc_fill_aiocb_out *out,
                  struct svc_req *rqstp)
{
    struct aiocb *cb = IN_AIOCB;

    UNUSED(rqstp);

    memset(out, 0, sizeof(*out));

    if (cb == NULL)
    {
        ERROR("Try to fill NULL AIO control block");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        return TRUE;
    }

    cb->aio_fildes = in->fildes;
    cb->aio_lio_opcode = lio_opcode_rpc2h(in->lio_opcode);
    cb->aio_reqprio = in->reqprio;
    cb->aio_buf = rcf_pch_mem_get(in->buf);
    cb->aio_nbytes = in->nbytes;
    if (in->sigevent.value.pointer)
    {
        cb->aio_sigevent.sigev_value.SIVAL_PTR =
            rcf_pch_mem_get(in->sigevent.value.tarpc_sigval_u.sival_ptr);
    }
    else
    {
        cb->aio_sigevent.sigev_value.SIVAL_INT =
            in->sigevent.value.tarpc_sigval_u.sival_int;
    }

    cb->aio_sigevent.sigev_signo = signum_rpc2h(in->sigevent.signo);
    cb->aio_sigevent.sigev_notify = sigev_notify_rpc2h(in->sigevent.notify);
    out->common._errno = fill_sigev_thread(&(cb->aio_sigevent),
                                           in->sigevent.function);
    return TRUE;
}


/*-------------- AIO control block destructor --------------------------*/
bool_t
_delete_aiocb_1_svc(tarpc_delete_aiocb_in *in,
                     tarpc_delete_aiocb_out *out,
                     struct svc_req *rqstp)
{
    UNUSED(rqstp);

    memset(out, 0, sizeof(*out));

    errno = 0;
    free(IN_AIOCB);
    rcf_pch_mem_free(in->cb);
    out->common._errno = RPC_ERRNO;

    return TRUE;
}

/*---------------------- aio_read() --------------------------*/
TARPC_FUNC(aio_read, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_AIOCB));
}
)

/*---------------------- aio_write() --------------------------*/
TARPC_FUNC(aio_write, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_AIOCB));
}
)

/*---------------------- aio_return() --------------------------*/
TARPC_FUNC(aio_return, {},
{
    MAKE_CALL(out->retval = func_ptr(IN_AIOCB));
}
)

/*---------------------- aio_error() --------------------------*/
TARPC_FUNC(aio_error, {},
{
    MAKE_CALL(out->retval = TE_OS_RC(TE_RPC, func_ptr(IN_AIOCB)));
}
)

/*---------------------- aio_cancel() --------------------------*/
TARPC_FUNC(aio_cancel, {},
{
    MAKE_CALL(out->retval =
                  aio_cancel_retval_h2rpc(func(in->fd, IN_AIOCB)));
}
)

/*---------------------- aio_fsync() --------------------------*/
TARPC_FUNC(aio_fsync, {},
{
    MAKE_CALL(out->retval = func(fcntl_flags_rpc2h(in->op), IN_AIOCB));
}
)

/*---------------------- aio_suspend() --------------------------*/
TARPC_FUNC(aio_suspend, {},
{
    struct aiocb  **cb = NULL;
    struct timespec tv;
    int             i;

    if (in->timeout.timeout_len > 0)
    {
        tv.tv_sec = in->timeout.timeout_val[0].tv_sec;
        tv.tv_nsec = in->timeout.timeout_val[0].tv_nsec;
        INIT_CHECKED_ARG((char *)&tv, sizeof(tv), 0);
    }

    if (in->cb.cb_len > 0 &&
        (cb = (struct aiocb **)calloc(in->cb.cb_len,
                                      sizeof(void *))) == NULL)
    {
        ERROR("Out of memory");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        goto finish;
    }

    for (i = 0; i < (int)(in->cb.cb_len); i++)
        cb[i] = (struct aiocb *)rcf_pch_mem_get(in->cb.cb_val[i]);

    INIT_CHECKED_ARG((void *)cb, sizeof(void *) * in->cb.cb_len,
                     sizeof(void *) * in->cb.cb_len);

    MAKE_CALL(out->retval = func_ptr((const struct aiocb * const *)cb, in->n,
                                     in->timeout.timeout_len == 0 ?
                                     NULL : &tv));
    free(cb);

    finish:
    ;
}
)


/*---------------------- lio_listio() --------------------------*/
TARPC_FUNC(lio_listio, {},
{
    struct aiocb  **cb = NULL;
    struct sigevent sig;
    int             i;

    if (in->sig.sig_len > 0)
    {
        tarpc_sigevent *ev = in->sig.sig_val;

        if (ev->value.pointer)
        {
            sig.sigev_value.SIVAL_PTR =
                rcf_pch_mem_get(ev->value.tarpc_sigval_u.sival_ptr);
        }
        else
        {
            sig.sigev_value.SIVAL_INT =
                ev->value.tarpc_sigval_u.sival_int;
        }

        sig.sigev_signo = signum_rpc2h(ev->signo);
        sig.sigev_notify = sigev_notify_rpc2h(ev->notify);
        out->common._errno = fill_sigev_thread(&sig, ev->function);
        INIT_CHECKED_ARG((char *)&sig, sizeof(sig), 0);
    }

    if (in->cb.cb_len > 0 &&
        (cb = (struct aiocb **)calloc(in->cb.cb_len,
                                      sizeof(void *))) == NULL)
    {
        ERROR("Out of memory");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        goto finish;
    }

    for (i = 0; i < (int)(in->cb.cb_len); i++)
        cb[i] = (struct aiocb *)rcf_pch_mem_get(in->cb.cb_val[i]);

    INIT_CHECKED_ARG((void *)cb, sizeof(void *) * in->cb.cb_len,
                     sizeof(void *) * in->cb.cb_len);

    MAKE_CALL(out->retval = func(lio_mode_rpc2h(in->mode), cb, in->nent,
                                 in->sig.sig_len == 0 ? NULL : &sig));
    free(cb);

    finish:
    ;
}
)

#endif

/*--------------------------- malloc ---------------------------------*/
TARPC_FUNC(malloc, {},
{
    void *buf;

    buf = func_ret_ptr(in->size);

    if (buf == NULL)
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    else
        out->retval = rcf_pch_mem_alloc(buf);
}
)

/*--------------------------- free ---------------------------------*/
TARPC_FUNC(free, {},
{
    UNUSED(out);
    func_ptr(rcf_pch_mem_get(in->buf));
    rcf_pch_mem_free(in->buf);
}
)

/*------------ get_addr_by_id ---------------------------*/
bool_t
_get_addr_by_id_1_svc(tarpc_get_addr_by_id_in  *in,
                      tarpc_get_addr_by_id_out *out,
                      struct svc_req           *rqstp)
{
    UNUSED(rqstp);
    out->retval =
            (uint64_t)((uint8_t *)rcf_pch_mem_get(in->id) -
                       (uint8_t *)NULL);

    return TRUE;
}

/*------------ raw2integer ---------------------------*/
/**
 * Convert raw data to integer.
 *
 * @param in    Input data
 * @param out   Output data
 *
 * @return @c 0 on success or @c -1
 */
int
raw2integer(tarpc_raw2integer_in *in,
            tarpc_raw2integer_out *out)
{
    uint8_t     single_byte;
    uint16_t    two_bytes;
    uint32_t    four_bytes;
    uint64_t    eight_bytes;

    if (in->data.data_val == NULL ||
        in->data.data_len == 0)
    {
        RING("%s(): trying to convert zero-length value",
             __FUNCTION__);
        return 0;
    }

    if (in->data.data_len == sizeof(single_byte))
    {
        single_byte = *(uint8_t *)(in->data.data_val);
        out->number = single_byte;
    }
    else if (in->data.data_len == sizeof(two_bytes))
    {
        two_bytes = *(uint16_t *)(in->data.data_val);
        out->number = two_bytes;
    }
    else if (in->data.data_len == sizeof(four_bytes))
    {
        four_bytes = *(uint32_t *)(in->data.data_val);
        out->number = four_bytes;
    }
    else if (in->data.data_len == sizeof(eight_bytes))
    {
        eight_bytes = *(uint64_t *)(in->data.data_val);
        out->number = eight_bytes;
    }
    else if (in->data.data_len <= sizeof(out->number))
    {
        int      x;
        uint8_t *p;

        WARN("%s(): incorrect length %d for raw data, "
             "trying to interpret according to endianness",
             __FUNCTION__, in->data.data_len);

        out->number = 0;
        x = 1;
        p = (uint8_t *)&x;
        if (p[sizeof(x) - 1] > 0)
        {
            /* Big-endian */
            p = (uint8_t *)&out->number +
                    (sizeof(out->number) - in->data.data_len);
            memcpy(p, in->data.data_val, in->data.data_len);
        }
        else /* Little-endian */
            memcpy(&out->number, in->data.data_val,
                   in->data.data_len);
    }
    else
    {
        ERROR("%s(): incorrect length %d for integer data",
              __FUNCTION__, in->data.data_len);
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        return -1;
    }

    return 0;
}

TARPC_FUNC(raw2integer, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*------------ integer2raw ---------------------------*/
/**
 * Convert integer value to raw representation.
 *
 * @param in    Input data
 * @param out   Output data
 *
 * @return @c 0 on success or @c -1
 */
int
integer2raw(tarpc_integer2raw_in *in,
            tarpc_integer2raw_out *out)
{
    uint8_t     single_byte;
    uint16_t    two_bytes;
    uint32_t    four_bytes;
    uint64_t    eight_bytes;

    void *p = NULL;

    if (in->len == 0)
    {
        RING("%s(): trying to convert zero-length value",
             __FUNCTION__);
        return 0;
    }

    out->data.data_val = NULL;
    out->data.data_len = 0;

    if (in->len == sizeof(single_byte))
    {
        single_byte = (uint8_t)in->number;
        p = &single_byte;
    }
    else if (in->len == sizeof(two_bytes))
    {
        two_bytes = (uint16_t)in->number;
        p = &two_bytes;
    }
    else if (in->len == sizeof(four_bytes))
    {
        four_bytes = (uint32_t)in->number;
        p = &four_bytes;
    }
    else if (in->len == sizeof(eight_bytes))
    {
        eight_bytes = (uint64_t)in->number;
        p = &eight_bytes;
    }
    else
    {
        ERROR("%s(): incorrect length %d for integer data",
              __FUNCTION__, in->len);
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        return -1;
    }

    out->data.data_val = calloc(1, in->len);
    if (out->data.data_val == NULL)
    {
        ERROR("%s(): failed to allocate space for integer data",
              __FUNCTION__);
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return -1;
    }

    memcpy(out->data.data_val, p, in->len);
    out->data.data_len = in->len;

    return 0;
}

TARPC_FUNC(integer2raw, {},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
}
)

/*-------------- memalign() ------------------------------*/

/* FIXME: provide prototype (proper header inclusion?) */
TARPC_FUNC_DYNAMIC_UNSAFE(memalign, {},
{
    void *buf;

    buf = func_ret_ptr(in->alignment, in->size);

    if (buf == NULL)
        out->common._errno = TE_RC(TE_TA_UNIX, errno);
    else
        out->retval = rcf_pch_mem_alloc(buf);
}
)

/*-------------- memcmp() ------------------------------*/

TARPC_FUNC(memcmp, {},
{
    out->retval = func_void(rcf_pch_mem_get(in->s1_base) + in->s1_off,
                            rcf_pch_mem_get(in->s2_base) + in->s2_off,
                            in->n);
}
)

/*-------------------------- Fill buffer ----------------------------*/
void
set_buf(const char *src_buf,
        tarpc_ptr dst_buf_base, size_t dst_offset, size_t len)
{
    char *dst_buf = rcf_pch_mem_get(dst_buf_base);

    if (dst_buf != NULL && len != 0)
        memcpy(dst_buf + dst_offset, src_buf, len);
    else if (len != 0)
        errno = EFAULT;
}

TARPC_FUNC(set_buf, {},
{
    MAKE_CALL(func_ptr(in->src_buf.src_buf_val, in->dst_buf, in->dst_off,
                       in->src_buf.src_buf_len));
}
)

/*-------------------------- Read buffer ----------------------------*/
void
get_buf(tarpc_ptr src_buf_base, size_t src_offset,
        char **dst_buf, size_t *len)
{
    char *src_buf = rcf_pch_mem_get(src_buf_base);

    *dst_buf = NULL;
    if (src_buf != NULL && *len != 0)
    {
        char *buf = malloc(*len);

        if (buf == NULL)
        {
            RING("%s(): failed to allocate %ld bytes",
                 __FUNCTION__, (long int)*len);
            *len = 0;
            errno = ENOMEM;
        }
        else
        {
            memcpy(buf, src_buf + src_offset, *len);
            *dst_buf = buf;
        }
    }
    else if (*len != 0)
    {
        RING("%s(): trying to get bytes from NULL address",
             __FUNCTION__);
        errno = EFAULT;
        *len = 0;
    }
}

TARPC_FUNC(get_buf, {},
{
    size_t len = in->len;

    MAKE_CALL(func(in->src_buf, in->src_off,
                   &out->dst_buf.dst_buf_val,
                   &len));

    out->dst_buf.dst_buf_len = len;
}
)

/*---------------------- Fill buffer by the pattern ----------------------*/
void
set_buf_pattern(int pattern,
                tarpc_ptr dst_buf_base, size_t dst_offset, size_t len)
{
    char *dst_buf = rcf_pch_mem_get(dst_buf_base);

    if (dst_buf != NULL && len != 0)
    {
        if (pattern < TAPI_RPC_BUF_RAND)
            memset(dst_buf + dst_offset, pattern, len);
        else
        {
            unsigned int i;

            for (i = 0; i < len; i++)
                dst_buf[i] = rand() % TAPI_RPC_BUF_RAND;
        }
    }
    else if (len != 0)
        errno = EFAULT;

}

TARPC_FUNC(set_buf_pattern, {},
{
    MAKE_CALL(func(in->pattern, in->dst_buf, in->dst_off, in->len));
}
)

/*-------------- setrlimit() ------------------------------*/

TARPC_FUNC(setrlimit, {},
{
    struct rlimit rlim;

    rlim.rlim_cur = in->rlim.rlim_val->rlim_cur;
    rlim.rlim_max = in->rlim.rlim_val->rlim_max;

    MAKE_CALL(out->retval = func(rlimit_resource_rpc2h(in->resource),
                                 &rlim));
}
)

/*-------------- getrlimit() ------------------------------*/

TARPC_FUNC(getrlimit,
{
    COPY_ARG(rlim);
},
{
    struct rlimit rlim;

    if (out->rlim.rlim_len > 0)
    {
        rlim.rlim_cur = out->rlim.rlim_val->rlim_cur;
        rlim.rlim_max = out->rlim.rlim_val->rlim_max;
    }

    MAKE_CALL(out->retval = func(rlimit_resource_rpc2h(in->resource),
                                 &rlim));

    if (out->rlim.rlim_len > 0)
    {
        out->rlim.rlim_val->rlim_cur = rlim.rlim_cur;
        out->rlim.rlim_val->rlim_max = rlim.rlim_max;
    }
}
)

/*-------------- sysconf() ------------------------------*/

TARPC_FUNC(sysconf, {},
{
    MAKE_CALL(out->retval = func(sysconf_name_rpc2h(in->name)));
}
)

#if defined(ENABLE_POWER_SW)
/*------------ power_sw() -----------------------------------*/
/* FIXME: provide proper prototype */
TARPC_FUNC(power_sw, {},
{
    MAKE_CALL(out->retval = func(in->type, in->dev, in->mask, in->cmd));
}
)
#endif

/*------------ mcast_join_leave() ---------------------------*/
void
mcast_join_leave(tarpc_mcast_join_leave_in  *in,
                 tarpc_mcast_join_leave_out *out)
{
    api_func            setsockopt_func;
    api_func_ret_ptr    if_indextoname_func;
    api_func            ioctl_func;

    if (tarpc_find_func(in->common.use_libc, "setsockopt",
                        &setsockopt_func) != 0 ||
        tarpc_find_func(in->common.use_libc, "if_indextoname",
                        (api_func *)&if_indextoname_func) != 0 ||
        tarpc_find_func(in->common.use_libc, "ioctl",
                        &ioctl_func) != 0)
    {
        ERROR("Cannot resolve function name");
        out->retval = -1;
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
        return;
    }

    memset(out, 0, sizeof(tarpc_mcast_join_leave_out));
    if (in->family == RPC_AF_INET6)
    {
        assert(in->multiaddr.multiaddr_len == sizeof(struct in6_addr));
        switch (in->how)
        {
            case TARPC_MCAST_ADD_DROP:
            {
#ifdef IPV6_ADD_MEMBERSHIP
                struct ipv6_mreq mreq;

                memcpy(&mreq.ipv6mr_multiaddr, in->multiaddr.multiaddr_val,
                       sizeof(struct in6_addr));
                mreq.ipv6mr_interface = in->ifindex;
                out->retval = setsockopt_func(in->fd, IPPROTO_IPV6,
                                              in->leave_group ?
                                                  IPV6_DROP_MEMBERSHIP :
                                                  IPV6_ADD_MEMBERSHIP,
                                              &mreq, sizeof(mreq));
                if (out->retval != 0)
                {
                    ERROR("Attempt to join IPv6 multicast group failed");
                    out->common._errno = TE_RC(TE_TA_UNIX, errno);
                }
#else
                ERROR("IPV6_ADD_MEMBERSHIP is not supported "
                      "for current Agent type");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
#endif
                break;
            }

            case TARPC_MCAST_JOIN_LEAVE:
            {
#ifdef MCAST_LEAVE_GROUP
                struct group_req     gr_req;
                struct sockaddr_in6 *sin6;

                sin6 = SIN6(&gr_req.gr_group);
                sin6->sin6_family = AF_INET6;
                memcpy(&sin6->sin6_addr, in->multiaddr.multiaddr_val,
                       sizeof(struct in6_addr));
                gr_req.gr_interface = in->ifindex;
                out->retval = setsockopt_func(in->fd, IPPROTO_IPV6,
                                              in->leave_group ?
                                                  MCAST_LEAVE_GROUP :
                                                  MCAST_JOIN_GROUP,
                                              &gr_req, sizeof(gr_req));
                if (out->retval != 0)
                {
                    ERROR("Attempt to join IPv6 multicast group failed");
                    out->common._errno = TE_RC(TE_TA_UNIX, errno);
                }
#else
                ERROR("MCAST_LEAVE_GROUP is not supported "
                      "for current Agent type");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
#endif
                break;
            }
            default:
                ERROR("Unknown multicast join method");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        }
        return;
    }
    else if (in->family == RPC_AF_INET)
    {
        assert(in->multiaddr.multiaddr_len == sizeof(struct in_addr));
        switch (in->how)
        {
            case TARPC_MCAST_ADD_DROP:
            {
#if HAVE_STRUCT_IP_MREQN
                struct ip_mreqn mreq;

                memset(&mreq, 0, sizeof(mreq));
                mreq.imr_ifindex = in->ifindex;
#else
                char              if_name[IFNAMSIZ];
                struct ifreq      ifrequest;
                struct ip_mreq    mreq;

                memset(&mreq, 0, sizeof(mreq));

                if (in->ifindex != 0)
                {
                    if (if_indextoname_func(in->ifindex, if_name) == NULL)
                    {
                        ERROR("Invalid interface index specified");
                        out->retval = -1;
                        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENXIO);
                        return;
                    }
                    else
                    {
                        memset(&ifrequest, 0, sizeof(struct ifreq));
                        memcpy(&(ifrequest.ifr_name), if_name, IFNAMSIZ);
                        if (ioctl_func(in->fd, SIOCGIFADDR, &ifrequest) < 0)
                        {
                            ERROR("No IPv4 address on interface %s",
                                  if_name);
                            out->retval = -1;
                            out->common._errno = TE_RC(TE_TA_UNIX,
                                                       TE_ENXIO);
                            return;
                        }

                        memcpy(&mreq.imr_interface,
                               &SIN(&ifrequest.ifr_addr)->sin_addr,
                               sizeof(struct in_addr));
                    }
                }
#endif
                memcpy(&mreq.imr_multiaddr, in->multiaddr.multiaddr_val,
                       sizeof(struct in_addr));
                out->retval = setsockopt_func(in->fd, IPPROTO_IP,
                                              in->leave_group ?
                                                  IP_DROP_MEMBERSHIP :
                                                  IP_ADD_MEMBERSHIP,
                                              &mreq, sizeof(mreq));
                if (out->retval != 0)
                {
                    ERROR("Attempt to join IPv4 multicast group failed");
                    out->common._errno = TE_RC(TE_TA_UNIX, errno);
                }
                break;
            }

            case TARPC_MCAST_JOIN_LEAVE:
            {
#ifdef MCAST_LEAVE_GROUP
                struct group_req     gr_req;
                struct sockaddr_in  *sin;

                sin = SIN(&gr_req.gr_group);
                sin->sin_family = AF_INET;
                memcpy(&sin->sin_addr, in->multiaddr.multiaddr_val,
                       sizeof(struct in_addr));
                gr_req.gr_interface = in->ifindex;
                out->retval = setsockopt_func(in->fd, IPPROTO_IP,
                                              in->leave_group ?
                                                  MCAST_LEAVE_GROUP :
                                                  MCAST_JOIN_GROUP,
                                              &gr_req, sizeof(gr_req));
                if (out->retval != 0)
                {
                    ERROR("Attempt to join IP multicast group failed");
                    out->common._errno = TE_RC(TE_TA_UNIX, errno);
                }
#else
                ERROR("MCAST_LEAVE_GROUP is not supported "
                      "for current Agent type");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
#endif
                break;
            }
            default:
                ERROR("Unknown multicast join method");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        }
    }
    else
    {
        ERROR("Unknown multicast address family %d", in->family);
        out->retval = -1;
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
}

TARPC_FUNC(mcast_join_leave, {},
{
    MAKE_CALL(func_ptr(in, out));
})

/*------------ mcast_source_join_leave() -----------------------*/
void
mcast_source_join_leave(tarpc_mcast_source_join_leave_in  *in,
                        tarpc_mcast_source_join_leave_out *out)
{
    api_func            setsockopt_func;
    api_func_ret_ptr    if_indextoname_func;
    api_func            ioctl_func;

    if (tarpc_find_func(in->common.use_libc, "setsockopt",
                        &setsockopt_func) != 0 ||
        tarpc_find_func(in->common.use_libc, "if_indextoname",
                        (api_func *)&if_indextoname_func) != 0 ||
        tarpc_find_func(in->common.use_libc, "ioctl",
                        &ioctl_func) != 0)
    {
        ERROR("Cannot resolve function name");
        out->retval = -1;
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
        return;
    }

    memset(out, 0, sizeof(tarpc_mcast_source_join_leave_out));
    if (in->family == RPC_AF_INET)
    {
        assert(in->multiaddr.multiaddr_len == sizeof(struct in_addr));
        assert(in->sourceaddr.sourceaddr_len == sizeof(struct in_addr));
        switch (in->how)
        {
            case TARPC_MCAST_SOURCE_ADD_DROP:
            {
#ifdef IP_DROP_SOURCE_MEMBERSHIP
                char                    if_name[IFNAMSIZ];
                struct ifreq            ifrequest;
                struct ip_mreq_source   mreq;

                memset(&mreq, 0, sizeof(mreq));

                if (in->ifindex != 0)
                {
                    if (if_indextoname_func(in->ifindex, if_name) == NULL)
                    {
                        ERROR("Invalid interface index specified");
                        out->retval = -1;
                        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENXIO);
                        return;
                    }
                    else
                    {
                        memset(&ifrequest, 0, sizeof(struct ifreq));
                        memcpy(&(ifrequest.ifr_name), if_name, IFNAMSIZ);
                        if (ioctl_func(in->fd, SIOCGIFADDR, &ifrequest) < 0)
                        {
                            ERROR("No IPv4 address on interface %s",
                                  if_name);
                            out->retval = -1;
                            out->common._errno = TE_RC(TE_TA_UNIX,
                                                       TE_ENXIO);
                            return;
                        }

                        memcpy(&mreq.imr_interface,
                               &SIN(&ifrequest.ifr_addr)->sin_addr,
                               sizeof(struct in_addr));
                    }
                }
                memcpy(&mreq.imr_multiaddr, in->multiaddr.multiaddr_val,
                       sizeof(struct in_addr));
                memcpy(&mreq.imr_sourceaddr, in->sourceaddr.sourceaddr_val,
                       sizeof(struct in_addr));
                out->retval = setsockopt_func(in->fd, IPPROTO_IP,
                                              in->leave_group ?
                                                IP_DROP_SOURCE_MEMBERSHIP :
                                                IP_ADD_SOURCE_MEMBERSHIP,
                                              &mreq, sizeof(mreq));
                if (out->retval != 0)
                {
                    ERROR("Attempt to join IPv4 multicast group failed");
                    out->common._errno = TE_RC(TE_TA_UNIX, errno);
                }
#else
                ERROR("MCAST_DROP_SOURCE_MEMBERSHIP is not supported "
                      "for current Agent type");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
#endif
                break;
            }

            case TARPC_MCAST_SOURCE_JOIN_LEAVE:
            {
#ifdef MCAST_LEAVE_SOURCE_GROUP
                struct group_source_req     gsr_req;
                struct sockaddr_in         *sin_multicast;
                struct sockaddr_in         *sin_source;

                sin_multicast = SIN(&gsr_req.gsr_group);
                sin_multicast->sin_family = AF_INET;
                memcpy(&sin_multicast->sin_addr,
                       in->multiaddr.multiaddr_val,
                       sizeof(struct in_addr));
                sin_source = SIN(&gsr_req.gsr_source);
                sin_source->sin_family = AF_INET;
                memcpy(&sin_source->sin_addr, in->sourceaddr.sourceaddr_val,
                       sizeof(struct in_addr));
                gsr_req.gsr_interface = in->ifindex;
                out->retval = setsockopt_func(in->fd, IPPROTO_IP,
                                              in->leave_group ?
                                                  MCAST_LEAVE_SOURCE_GROUP :
                                                  MCAST_JOIN_SOURCE_GROUP,
                                              &gsr_req, sizeof(gsr_req));
                if (out->retval != 0)
                {
                    ERROR("Attempt to join IP multicast group failed");
                    out->common._errno = TE_RC(TE_TA_UNIX, errno);
                }
#else
                ERROR("MCAST_LEAVE_SOURCE_GROUP is not supported "
                      "for current Agent type");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
#endif
                break;
            }
            default:
                ERROR("Unknown multicast source join method");
                out->retval = -1;
                out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
        }
    }
    else
    {
        ERROR("Unsupported multicast address family %d", in->family);
        out->retval = -1;
        out->common._errno = TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
}

TARPC_FUNC(mcast_source_join_leave, {},
{
    MAKE_CALL(func_ptr(in, out));
})

/*-------------- dlopen() --------------------------*/
/**
 * Loads the dynamic labrary file.
 *
 * @param filename  the name of the file to load
 * @param flag      dlopen flags.
 *
 * @return dynamic library handle on success or NULL in the case of failure
 */
void *
ta_dlopen(tarpc_ta_dlopen_in *in)
{
    api_func_ptr_ret_ptr    dlopen_func;
    api_func_void_ret_ptr   dlerror_func;

    if ((tarpc_find_func(in->common.use_libc, "dlopen",
                         (api_func *)&dlopen_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "dlerror",
                         (api_func *)&dlerror_func) != 0))
    {
        ERROR("Failed to resolve functions, %s", __FUNCTION__);
        return NULL;
    }

    return dlopen_func(in->filename, dlopen_flags_rpc2h(in->flag));
}

TARPC_FUNC(ta_dlopen, {},
{
    MAKE_CALL(out->retval =
                        (tarpc_dlhandle)((uintptr_t)func_ptr_ret_ptr(in)));
}
)

/*-------------- dlsym() --------------------------*/
/**
 * Returns the address where a certain symbol from dynamic labrary
 * is loaded into memory.
 *
 * @param handle    handle of a dynamic library returned by dlopen()
 * @param symbol    null-terminated symbol name
 *
 * @return address of the symbol or NULL if symbol is not found.
 */
void *
ta_dlsym(tarpc_ta_dlsym_in *in)
{
    api_func_ptr_ret_ptr    dlsym_func;
    api_func_void_ret_ptr   dlerror_func;

    if ((tarpc_find_func(in->common.use_libc, "dlsym",
                         (api_func *)&dlsym_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "dlerror",
                         (api_func *)&dlerror_func) != 0))
    {
        ERROR("Failed to resolve functions, %s", __FUNCTION__);
        return NULL;
    }

    return dlsym_func((void *)((uintptr_t)in->handle), in->symbol);
}

TARPC_FUNC(ta_dlsym, {},
{
    MAKE_CALL(out->retval =
                    (tarpc_dlsymaddr)((uintptr_t)func_ptr_ret_ptr(in)));
}
)

/*-------------- dlsym_call() --------------------------*/
/**
 * Calls a certain symbol from dynamic library as a function with
 * no arguments and returns its return code.
 *
 * @param handle    handle of a dynamic library returned by dlopen()
 * @param symbol    null-terminated symbol name
 *
 * @return return code of called symbol or -1 on error.
 */
int
ta_dlsym_call(tarpc_ta_dlsym_call_in *in)
{
    api_func_ptr_ret_ptr    dlsym_func;
    api_func_void_ret_ptr   dlerror_func;
    char                   *error;

    int (*func)(void);

    if ((tarpc_find_func(in->common.use_libc, "dlsym",
                         (api_func *)&dlsym_func) != 0) ||
        (tarpc_find_func(in->common.use_libc, "dlerror",
                         (api_func *)&dlerror_func) != 0))
    {
        ERROR("Failed to resolve functions, %s", __FUNCTION__);
        return -1;
    }

    dlerror_func();

    *(void **) (&func) = dlsym_func((void *)((uintptr_t)in->handle),
                                    in->symbol);
    if ((error = (char *)dlerror_func()) != NULL)
    {
        ERROR("%s: dlsym call failed, %s", __FUNCTION__, error);
        return -1;
    }

    return (*func)();
}

TARPC_FUNC(ta_dlsym_call, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*-------------- dlerror() --------------------------*/
/**
 * Returns a human readable string describing the most recent error
 * that occured from dlopen(), dlsym() or dlclose().
 *
 * @return a pointer to string or NULL if no errors occured.
 */
char *
ta_dlerror(tarpc_ta_dlerror_in *in)
{
    api_func_void_ret_ptr   dlerror_func;

    if (tarpc_find_func(in->common.use_libc, "dlerror",
                        (api_func *)&dlerror_func) != 0)
    {
        ERROR("Failed to resolve functions, %s", __FUNCTION__);
        return NULL;
    }

    return (char *)dlerror_func();
}

TARPC_FUNC(ta_dlerror, {},
{
    MAKE_CALL(out->retval = func_ptr_ret_ptr(in));
}
)

/*-------------- dlclose() --------------------------*/
/**
 * Decrements the reference count on the dynamic library handle.
 * If the reference count drops to zero and no other loaded libraries
 * use symbols in it, then the dynamic library is unloaded.
 *
 * @param handle    handle of a dynamic library returned by dlopen()
 *
 * @return 0 on success, and non-zero on error.
 */
int
ta_dlclose(tarpc_ta_dlclose_in *in)
{
    api_func_ptr    dlclose_func;

    if (tarpc_find_func(in->common.use_libc, "dlclose",
                        (api_func *)&dlclose_func) != 0)
    {
        ERROR("Failed to resolve functions, %s", __FUNCTION__);
        return -1;
    }

    return dlclose_func((void *)((uintptr_t)in->handle));
}

TARPC_FUNC(ta_dlclose, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

#ifdef NO_DL
taprc_dlhandle
dlopen(const char *filename, int flag)
{
    UNUSED(filename);
    UNUSED(flag);
    return (tarpc_dlhandle)NULL;
}

const char *
dlerror(void)
{
    return "Unsupported";
}

tarpc_dlsymaddr
dlsym(void *handle, const char *symbol)
{
    UNUSED(handle);
    UNUSED(symbol);

    return (tarpc_dlsymaddr)NULL;
}

int
dlclose(void *handle)
{
    UNUSED(handle);
    return 0;
}
#endif

/*------------ recvmmsg_alt() ---------------------------*/
int
recvmmsg_alt(int fd, struct mmsghdr_alt *mmsghdr, unsigned int vlen,
             unsigned int flags, struct timespec *timeout, te_bool use_libc)
{
    api_func            recvmmsg_func;

    if (tarpc_find_func(use_libc, "recvmmsg",
                        &recvmmsg_func) == 0)
        return recvmmsg_func(fd, mmsghdr, vlen, flags, timeout);
    else
#if defined (__QNX__)
        return -1;
#else
        return syscall(SYS_recvmmsg, fd, mmsghdr, vlen, flags, timeout);
#endif
}

TARPC_FUNC(recvmmsg_alt,
{
    unsigned int i;
    struct tarpc_msghdr msg;
    if (in->mmsg.mmsg_val != NULL &&
        in->mmsg.mmsg_len > RCF_RPC_MAX_MSGHDR)
    {
        ERROR("Too long mmsghdr is provided");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    if (in->mmsg.mmsg_val != NULL)
       for (i = 0; i < in->mmsg.mmsg_len; i++)
       {
           msg = in->mmsg.mmsg_val[i].msg_hdr;
           if (msg.msg_iov.msg_iov_val != NULL &&
               msg.msg_iov.msg_iov_len > RCF_RPC_MAX_IOVEC)
           {
               ERROR("Too long iovec is provided");
               out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
               return TRUE;
           }
       }
    COPY_ARG(mmsg);
},
{
    struct iovec        iovec_arr[RCF_RPC_MAX_MSGHDR][RCF_RPC_MAX_IOVEC];
    struct mmsghdr_alt  mmsg[RCF_RPC_MAX_MSGHDR];
    te_bool             free_name[RCF_RPC_MAX_MSGHDR];

    unsigned int  i;
    unsigned int  j;
    struct timespec  tv;
    struct timespec *ptv = NULL;

    memset(free_name, 0, sizeof(free_name));

    if (in->timeout.timeout_len > 0)
    {
        tv.tv_sec = in->timeout.timeout_val[0].tv_sec;
        tv.tv_nsec = in->timeout.timeout_val[0].tv_nsec;
        ptv = &tv;
    }

    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(mmsg, 0, sizeof(mmsg));

    if (out->mmsg.mmsg_val == NULL)
    {
        MAKE_CALL(out->retval = func(in->fd, NULL, in->vlen,
                                     send_recv_flags_rpc2h(in->flags),
                                     ptv, in->common.use_libc));
    }
    else
    {
        te_errno                name_rc;
        struct sockaddr_storage name_st[RCF_RPC_MAX_MSGHDR];
        socklen_t               name_len[RCF_RPC_MAX_MSGHDR];
        struct sockaddr        *name[RCF_RPC_MAX_MSGHDR];
        struct tarpc_msghdr    *rpc_msg;
        struct msghdr          *msg;

        for (j = 0; j < out->mmsg.mmsg_len; j++)
        {
            free_name[j] = FALSE;
            mmsg[j].msg_len = out->mmsg.mmsg_val[j].msg_len;
            msg = &mmsg[j].msg_hdr;
            rpc_msg = &(out->mmsg.mmsg_val[j].msg_hdr);
            name_len[j] = 0;

            if (!(rpc_msg->msg_name.flags & TARPC_SA_RAW &&
                  rpc_msg->msg_name.raw.raw_len >
                  sizeof(struct sockaddr_storage)))
            {
                name_rc = sockaddr_rpc2h(&(rpc_msg->msg_name),
                                         SA(&name_st[j]),
                                         sizeof(struct sockaddr_storage),
                                         &name[j], &name_len[j]);

                if (name_rc != 0)
                {
                    out->common._errno = name_rc;
                    goto finish;
                }

                INIT_CHECKED_ARG((char *)name[j], name_len[j],
                                 rpc_msg->msg_namelen);
                msg->msg_name = name[j];
            }
            else
            {
                if (rpc_msg->msg_name.raw.raw_len > 0 &&
                    rpc_msg->msg_name.raw.raw_val != NULL)
                {
                    msg->msg_name = calloc(1,
                                           rpc_msg->msg_name.raw.raw_len);
                    if (msg->msg_name == NULL)
                    {
                        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                        goto finish;
                    }
                    free_name[j] = TRUE;
                    memcpy(msg->msg_name, rpc_msg->msg_name.raw.raw_val,
                           rpc_msg->msg_name.raw.raw_len);
                }
                else
                    msg->msg_name = rpc_msg->msg_name.raw.raw_val;
            }
            msg->msg_namelen = rpc_msg->msg_namelen;

            msg->msg_iovlen = rpc_msg->msg_iovlen;
            if (rpc_msg->msg_iov.msg_iov_val != NULL)
            {
                for (i = 0; i < rpc_msg->msg_iov.msg_iov_len; i++)
                {
                    INIT_CHECKED_ARG(
                     rpc_msg->msg_iov.msg_iov_val[i].iov_base.iov_base_val,
                     rpc_msg->msg_iov.msg_iov_val[i].iov_base.iov_base_len,
                     rpc_msg->msg_iov.msg_iov_val[i].iov_len);
                    iovec_arr[j][i].iov_base =
                        rpc_msg->msg_iov.msg_iov_val[i].iov_base.
                            iov_base_val;
                    iovec_arr[j][i].iov_len =
                        rpc_msg->msg_iov.msg_iov_val[i].iov_len;
                }
                msg->msg_iov = iovec_arr[j];
                INIT_CHECKED_ARG((char *)iovec_arr[j], sizeof(iovec_arr[j]),
                                 0);
            }
            if (rpc_msg->msg_control.msg_control_val != NULL)
            {
                int len = calculate_msg_controllen(rpc_msg);
                int rlen = len * 2;
                int data_len = rpc_msg->msg_control.msg_control_val[0].
                                data.data_len;

                free(rpc_msg->msg_control.msg_control_val[0].data.data_val);
                free(rpc_msg->msg_control.msg_control_val);
                rpc_msg->msg_control.msg_control_val = NULL;
                rpc_msg->msg_control.msg_control_len = 0;

                msg->msg_controllen = len;
                if ((msg->msg_control = calloc(1, rlen)) == NULL)
                {
                    out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
                    goto finish;
                }
                CMSG_FIRSTHDR(msg)->cmsg_len = CMSG_LEN(data_len);
                INIT_CHECKED_ARG((char *)(msg->msg_control), rlen, len);
            }
            msg->msg_flags = send_recv_flags_rpc2h(rpc_msg->msg_flags);
            rpc_msg->in_msg_flags = send_recv_flags_h2rpc(msg->msg_flags);

            /*
             * msg_name, msg_iov, msg_iovlen and msg_control MUST NOT be
             * changed.
             *
             * msg_namelen, msg_controllen and msg_flags MAY be changed.
             */
            INIT_CHECKED_ARG((char *)&msg->msg_name,
                             sizeof(msg->msg_name), 0);
            INIT_CHECKED_ARG((char *)&msg->msg_iov,
                             sizeof(msg->msg_iov), 0);
            INIT_CHECKED_ARG((char *)&msg->msg_iovlen,
                             sizeof(msg->msg_iovlen), 0);
            INIT_CHECKED_ARG((char *)&msg->msg_control,
                             sizeof(msg->msg_control), 0);
        }

        VERB("recvmmsg_alt(): in mmsg=%s",
             mmsghdr2str(mmsg, out->mmsg.mmsg_len));
        MAKE_CALL(out->retval = func(in->fd, mmsg, in->vlen,
                                     send_recv_flags_rpc2h(in->flags),
                                     ptv, in->common.use_libc));
        VERB("recvmmsg_alt(): out mmsg=%s",
             mmsghdr2str(mmsg, out->retval));

        for (j = 0; j < out->mmsg.mmsg_len; j++)
        {
            out->mmsg.mmsg_val[j].msg_len = mmsg[j].msg_len;
            msg = &mmsg[j].msg_hdr;
            rpc_msg = &(out->mmsg.mmsg_val[j].msg_hdr);

            rpc_msg->msg_flags = send_recv_flags_h2rpc(msg->msg_flags);

            if (msg->msg_namelen <
                    (tarpc_socklen_t)sizeof(struct sockaddr_storage))
                sockaddr_output_h2rpc(msg->msg_name,
                                      name_len[j] > 0 ?
                                        name_len[j] :
                                        rpc_msg->msg_name.raw.raw_len,
                                      msg->msg_namelen,
                                      &(rpc_msg->msg_name));
            else
            {
                if (msg->msg_name != NULL &&
                    rpc_msg->msg_name.raw.raw_val != NULL)
                    memcpy(rpc_msg->msg_name.raw.raw_val, msg->msg_name,
                           rpc_msg->msg_name.raw.raw_len <
                                                msg->msg_namelen ?
                                rpc_msg->msg_name.raw.raw_len :
                                msg->msg_namelen);
            }

            rpc_msg->msg_namelen = msg->msg_namelen;

            if (rpc_msg->msg_iov.msg_iov_val != NULL)
            {
                for (i = 0; i < rpc_msg->msg_iov.msg_iov_len; i++)
                {
                    rpc_msg->msg_iov.msg_iov_val[i].iov_len =
                        iovec_arr[j][i].iov_len;
                }
            }

            /* in case retval < 0 cmsg is not filled */
            if (out->retval >= 0 && msg->msg_control != NULL)
            {
                int                   rc;
                unsigned int          rpc_len;
                struct tarpc_cmsghdr *rpc_c;

                rc = msg_control_h2rpc(msg->msg_control,
                                       msg->msg_controllen,
                                       &rpc_c, &rpc_len);
                if (rc != 0)
                {
                    ERROR("%s(): failed cmsghdr conversion",
                          __FUNCTION__);
                    out->common._errno = TE_RC(TE_TA_UNIX, rc);
                    goto finish;
                }

                rpc_msg->msg_control.msg_control_val = rpc_c;
                rpc_msg->msg_control.msg_control_len = rpc_len;
            }
        }
    }
    finish:
    for (j = 0; j < out->mmsg.mmsg_len; j++)
    {
        free(mmsg[j].msg_hdr.msg_control);
        if (free_name[j])
            free(mmsg[j].msg_hdr.msg_name);
    }
}
)

/*------------ sendmmsg_alt() ---------------------------*/
int
sendmmsg_alt(int fd, struct mmsghdr_alt *mmsghdr, unsigned int vlen,
             unsigned int flags, te_bool use_libc)
{
    api_func            sendmmsg_func;

    if (tarpc_find_func(use_libc, "sendmmsg",
                        &sendmmsg_func) == 0)
        return sendmmsg_func(fd, mmsghdr, vlen, flags);
    else
#if defined (__QNX__)
        return -1;
#else
        return syscall(SYS_sendmmsg, fd, mmsghdr, vlen, flags);
#endif
}

TARPC_FUNC(sendmmsg_alt,
{
    unsigned int i;
    struct tarpc_msghdr msg;
    if (in->mmsg.mmsg_val != NULL &&
        in->mmsg.mmsg_len > RCF_RPC_MAX_MSGHDR)
    {
        ERROR("Too long mmsghdr is provided");
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return TRUE;
    }
    if (in->mmsg.mmsg_val != NULL)
       for (i = 0; i < in->mmsg.mmsg_len; i++)
       {
           msg = in->mmsg.mmsg_val[i].msg_hdr;
           if (msg.msg_iov.msg_iov_val != NULL &&
               msg.msg_iov.msg_iov_len > RCF_RPC_MAX_IOVEC)
           {
               ERROR("Too long iovec is provided");
               out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
               return TRUE;
           }
       }
    COPY_ARG(mmsg);
},
{
    struct iovec        iovec_arr[RCF_RPC_MAX_MSGHDR][RCF_RPC_MAX_IOVEC];
    struct mmsghdr_alt  mmsg[RCF_RPC_MAX_MSGHDR];

    unsigned int  i;
    unsigned int  j;

    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(mmsg, 0, sizeof(mmsg));

    if (out->mmsg.mmsg_val == NULL)
    {
        MAKE_CALL(out->retval = func(in->fd, NULL, in->vlen,
                                     send_recv_flags_rpc2h(in->flags),
                                     in->common.use_libc));
    }
    else
    {
        struct tarpc_msghdr    *rpc_msg;
        struct msghdr          *msg;

        for (j = 0; j < out->mmsg.mmsg_len; j++)
        {
            mmsg[j].msg_len = out->mmsg.mmsg_val[j].msg_len;
            msg = &mmsg[j].msg_hdr;
            rpc_msg = &(out->mmsg.mmsg_val[j].msg_hdr);

            MSGHDR_TARPC2H(rpc_msg, msg);
       }

        VERB("sendmmsg_alt(): in mmsg=%s",
             mmsghdr2str(mmsg, out->mmsg.mmsg_len));
        MAKE_CALL(out->retval = func(in->fd, mmsg, in->vlen,
                                     send_recv_flags_rpc2h(in->flags),
                                     in->common.use_libc));

        for (j = 0; j < out->mmsg.mmsg_len; j++)
        {
            msg = &mmsg[j].msg_hdr;
            out->mmsg.mmsg_val[j].msg_len = mmsg[j].msg_len;
            msghdr_free(msg);
       }
    }
finish:
    ;
}
)

/*------------ vfork_pipe_exec() -----------------------*/
int
vfork_pipe_exec(tarpc_vfork_pipe_exec_in *in)
{
    api_func_ptr        pipe_func;
    api_func_void       vfork_func;
    api_func            read_func;
    api_func_ptr        execve_func;
    api_func            write_func;

    int pipefd[2];
    int pid;
    te_errno      rc;
    struct pollfd fds;

    static volatile int global_var = 1;
    volatile int stack_var = 1;

    memset(&fds, 0, sizeof(fds));

    char       *argv[4];

    if (tarpc_find_func(in->common.use_libc, "pipe",
                        (api_func *)&pipe_func) != 0)
    {
        ERROR("Failed to find function \"pipe\"");
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "vfork",
                        (api_func *)&vfork_func) != 0)
    {
        ERROR("Failed to find function \"vfork\"");
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "read",
                        (api_func *)&read_func) != 0)
    {
        ERROR("Failed to find function \"read\"");
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "execve",
                        (api_func *)&execve_func) != 0)
    {
        ERROR("Failed to find function \"execve\"");
        return -1;
    }
    if (tarpc_find_func(in->common.use_libc, "write",
                        (api_func *)&write_func) != 0)
    {
        ERROR("Failed to find function \"write\"");
        return -1;
    }

    if ((rc = pipe_func(pipefd)) != 0)
    {
        ERROR("pipe() failed with error %r", TE_OS_RC(TE_TA_UNIX, errno));
        return -1;
    }

    pid = vfork_func();

    if (pid < 0)
    {
        ERROR("vfork() failed with error %r", TE_OS_RC(TE_TA_UNIX, errno));
        return pid;
    }

    if (pid > 0)
    {
        write(pipefd[1], "Test message", 12);
        RING("Parent process is unblocked");
        if (global_var != 2)
        {
            ERROR("'global_var' was not changed from the child process");
            return -1;
        }
        if (stack_var != 2)
        {
            ERROR("'stack_var' was not changed from the child process");
            return -1;
        }
        return 0;
    }
    else
    {
        sleep(1);
        global_var = 2;
        stack_var = 2;
        fds.fd = pipefd[0];
        fds.events = POLLIN;
        if (poll(&fds, 1, 1000) != 0)
        {
            ERROR("vfork() doesn't hang!");
            return -1;
        }
        else
            RING("Parent process is still hanging");

        if (in->use_exec)
        {
            memset(argv, 0, sizeof(argv));
            argv[0] = (char *)ta_execname;
            argv[1] = "exec";
            argv[2] = "sleep_and_print";

            if ((rc = execve_func((void *)ta_execname, argv, environ)) < 0)
            {
                ERROR("execve() failed with error %r",
                      TE_OS_RC(TE_TA_UNIX, errno));
                return rc;
            }
            return 0;
        }
        else
        {
            RING("Child process is finished.");
            _exit(0);
        }
    }

    return 0;
}

int
sleep_and_print()
{
    sleep(1);
    return 0;
}

TARPC_FUNC(vfork_pipe_exec, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
}
)

/*------------ namespace_id2str() -----------------------*/

te_errno
namespace_id2str(tarpc_namespace_id2str_in *in,
                 tarpc_namespace_id2str_out *out)
{
    te_errno rc;
    const char *buf = NULL;
    rc = rcf_pch_mem_ns_get_string(in->id, &buf);
    if (rc != 0)
        return rc;
    if (buf == NULL)
        return TE_RC(TE_RPC, TE_ENOENT);

    const int str_len = strlen(buf);
    out->str.str_val = malloc(str_len);
    if (out->str.str_val == NULL)
        return TE_RC(TE_RPC, TE_ENOMEM);

    out->str.str_len = str_len;
    memcpy(out->str.str_val, buf, str_len);
    return 0;
}

TARPC_FUNC(namespace_id2str,
{
    /* Only blocking operation is supported for @b namespace_id2str */
    in->common.op = RCF_RPC_CALL_WAIT;
},
{
    MAKE_CALL(out->retval = func_ptr(in, out));
})

/*------------ release_rpc_ptr() -----------------------*/

TARPC_FUNC_STANDALONE(release_rpc_ptr, {},
{
    MAKE_CALL(
        RPC_PCH_MEM_WITH_NAMESPACE(ns, in->ns_string, {
            RCF_PCH_MEM_INDEX_FREE(in->ptr, ns);
    }));
})

/*------------ get_rw_ability() -----------------------*/
int
get_rw_ability(tarpc_get_rw_ability_in *in)
{
    iomux_funcs iomux_f;
    iomux_func  iomux = get_default_iomux();
    iomux_state iomux_st;

    int rc;

    if (iomux_find_func(in->common.use_libc, &iomux, &iomux_f) != 0)
    {
        ERROR("failed to resolve iomux function");
        return -1;
    }

    /* Create iomux status and fill it with our fds. */
    if ((rc = iomux_create_state(iomux, &iomux_f, &iomux_st)) != 0)
        return rc;
    if ((rc = iomux_add_fd(iomux, &iomux_f, &iomux_st,
                           in->sock,
                           in->check_rd ? POLLIN : POLLOUT)) != 0)
    {
        iomux_close(iomux, &iomux_f, &iomux_st);
        return rc;
    }

    rc = iomux_wait(iomux, &iomux_f, &iomux_st, NULL, in->timeout);

    iomux_close(iomux, &iomux_f, &iomux_st);
    return rc;
}

TARPC_FUNC(get_rw_ability, {},
{
    MAKE_CALL(out->retval = func_ptr(in));
})

/*------------ rpcserver_plugin_enable() -----------------------*/

TARPC_FUNC(rpcserver_plugin_enable, {},
{
    MAKE_CALL(out->retval = func_ptr(
               in->install,
               in->action,
               in->uninstall));
})

/*------------ rpcserver_plugin_disable() -----------------------*/

TARPC_FUNC(rpcserver_plugin_disable, {},
{
    MAKE_CALL(out->retval = func_ptr());
})
