/** @file
 * @brief Windows Test Agent
 *
 * Windows Test Agent implementation.
 *
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
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
 * $Id: win32.c 4316 2004-08-09 12:08:22Z arybchik $
 */

#include <winsock2.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#if 0
#include <sys/socket.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/un.h>

#include "te_defs.h"
#include "te_errno.h"
#include "comm_agent.h"
#include "rcf_ch_api.h"
#include "rcf_pch.h"

#ifdef RCF_RPC
#include "win32_rpc.h"
#endif    


#define LGR_USER         "Main"
#include "logger_ta.h"

char *my_execname;


/** Send answer to the TEN */
#define SEND_ANSWER(_fmt...) \
    do {                                                                \
        int rc;                                                         \
                                                                        \
        if ((size_t)snprintf(cbuf + answer_plen, buflen - answer_plen,  \
                             _fmt) >= (buflen - answer_plen))           \
        {                                                               \
            VERB("answer is truncated\n");                              \
        }                                                               \
        rcf_ch_lock();                                                  \
        rc = rcf_comm_agent_reply(handle, cbuf, strlen(cbuf) + 1);      \
        rcf_ch_unlock();                                                \
        return rc;                                                      \
    } while (FALSE)


extern void *rcf_ch_symbol_addr_auto(const char *name, te_bool is_func);
extern char *rcf_ch_symbol_name_auto(const void *addr);

char *ta_name = "(win32)";

int ta_pid;

const char *te_lgr_entity = "(win32)";

static pthread_mutex_t ta_lock = PTHREAD_MUTEX_INITIALIZER;


/* See description in rcf_ch_api.h */
int
rcf_ch_init()
{
    return 0;
}


/* See description in rcf_ch_api.h */
void
rcf_ch_lock()
{
    pthread_mutex_lock(&ta_lock);
}

/* See description in rcf_ch_api.h */
void
rcf_ch_unlock()
{
    pthread_mutex_unlock(&ta_lock);
}


/* See description in rcf_ch_api.h */
int
rcf_ch_shutdown(struct rcf_comm_connection *handle,
                char *cbuf, size_t buflen, size_t answer_plen)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    /* Standard handler is OK */
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_reboot(struct rcf_comm_connection *handle,
              char *cbuf, size_t buflen, size_t answer_plen,
              const uint8_t *ba, size_t cmdlen, const char *params)
{
    UNUSED(ba);
    UNUSED(cmdlen);
    UNUSED(params);

    SEND_ANSWER("0");
    /* FIXME Unreachable */
    system("/sbin/reboot");
}


/* See description in rcf_ch_api.h */
int
rcf_ch_configure(struct rcf_comm_connection *handle,
                 char *cbuf, size_t buflen, size_t answer_plen,
                 const uint8_t *ba, size_t cmdlen,
                 rcf_ch_cfg_op_t op, const char *oid, const char *val)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(ba);
    UNUSED(cmdlen);

    /* Standard handler is OK */
    VERB("Configure: op %d OID <%s> val <%s>\n", op,
         oid == NULL ? "" : oid, val == NULL ? "" : val);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_vread(struct rcf_comm_connection *handle,
             char *cbuf, size_t buflen, size_t answer_plen,
             rcf_var_type_t type, const char *var)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(type);
    UNUSED(var);

    /* Standard handler is OK */
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_vwrite(struct rcf_comm_connection *handle,
              char *cbuf, size_t buflen, size_t answer_plen,
              rcf_var_type_t type, const char *var, ...)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(type);
    UNUSED(var);

    /* Standard handler is OK */
    return -1;
}

/* See description in rcf_ch_api.h */
void *
rcf_ch_symbol_addr(const char *name, te_bool is_func)
{
    return rcf_ch_symbol_addr_auto(name, is_func);
}

/* See description in rcf_ch_api.h */
char *
rcf_ch_symbol_name(const void *addr)
{
    return rcf_ch_symbol_name_auto(addr);
}


/* See description in rcf_ch_api.h */
int
rcf_ch_file(struct rcf_comm_connection *handle,
            char *cbuf, size_t buflen, size_t answer_plen,
            const uint8_t *ba, size_t cmdlen,
            te_bool put, const char *filename)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(ba);
    UNUSED(cmdlen);
    UNUSED(put);
    UNUSED(filename);

    /* Standard handler is OK */
    return -1;
}


#ifdef TAD_CH_DUMMY
/* See description in rcf_ch_api.h */
int
rcf_ch_csap_create(struct rcf_comm_connection *handle,
                   char *cbuf, size_t buflen, size_t answer_plen,
                   const uint8_t *ba, size_t cmdlen,
                   const char *stack, const char *params)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(ba);
    UNUSED(cmdlen);

    VERB("CSAP create: stack <%s> params <%s>\n", stack, params);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_csap_destroy(struct rcf_comm_connection *handle,
                    char *cbuf, size_t buflen, size_t answer_plen,
                    csap_handle_t csap)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);

    VERB("CSAP destroy: handle %d\n", csap);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_csap_param(struct rcf_comm_connection *handle,
                  char *cbuf, size_t buflen, size_t answer_plen,
                  csap_handle_t csap, const char *param)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);

    VERB("CSAP param: handle %d param <%s>\n", csap, param);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_trsend_start(struct rcf_comm_connection *handle,
                    char *cbuf, size_t buflen, size_t answer_plen,
                    const uint8_t *ba, size_t cmdlen,
                    csap_handle_t csap, te_bool postponed)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(ba);
    UNUSED(cmdlen);

    VERB("TRSEND start: handle %d %s\n", csap, postponed ? "postponed" : "");
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_trsend_stop(struct rcf_comm_connection *handle,
                   char *cbuf, size_t buflen, size_t answer_plen,
                   csap_handle_t csap)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);

    VERB("TRSEND stop handle %d\n", csap);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_trrecv_start(struct rcf_comm_connection *handle,
                    char *cbuf, size_t buflen,
                    size_t answer_plen, const uint8_t *ba, 
                    size_t cmdlen, csap_handle_t csap, 
                    unsigned int num, te_bool results,
                    unsigned int timeout)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(ba);
    UNUSED(cmdlen);
    UNUSED(csap);
    UNUSED(num);
    UNUSED(results);
    UNUSED(timeout);

    VERB("TRRECV start: handle %d num %u timeout %u %s\n",
         csap, num, timeout, results ? "results" : "");
    return -1;
}

/* See description in rcf_ch_api.h */
int 
rcf_ch_trrecv_wait(struct rcf_comm_connection *handle,
                   char *cbuf, size_t buflen,
                   size_t answer_plen,
                   csap_handle_t csap)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(csap);

    VERB("TRRECV wait: handle %d \n", csap);
    
    return -1;
}

/* See description in rcf_ch_api.h */
int
rcf_ch_trrecv_stop(struct rcf_comm_connection *handle,
                   char *cbuf, size_t buflen, size_t answer_plen,
                   csap_handle_t csap)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);

    VERB("TRRECV stop handle %d\n", csap);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_trrecv_get(struct rcf_comm_connection *handle,
                  char *cbuf, size_t buflen, size_t answer_plen,
                  csap_handle_t csap)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);

    VERB("TRRECV get handle %d\n", csap);
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_trsend_recv(struct rcf_comm_connection *handle,
                   char *cbuf, size_t buflen, size_t answer_plen,
                   const uint8_t *ba, size_t cmdlen, csap_handle_t csap,
                   te_bool results, unsigned int timeout)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(ba);
    UNUSED(cmdlen);

    VERB("TRSEND recv: handle %d timeout %d %s\n",
                        csap, timeout, results ? "results" : "");
    return -1;
}
#endif /* TAD_CH_DUMMY */


/* See description in rcf_ch_api.h */
int
rcf_ch_call(struct rcf_comm_connection *handle,
            char *cbuf, size_t buflen, size_t answer_plen,
            const char *rtn, te_bool is_argv, int argc, uint32_t *params)
{
    UNUSED(handle);
    UNUSED(cbuf);
    UNUSED(buflen);
    UNUSED(answer_plen);
    UNUSED(rtn);
    UNUSED(is_argv);
    UNUSED(argc);
    UNUSED(params);

    /* Standard handler is OK */
    return -1;
}


/* See description in rcf_ch_api.h */
int
rcf_ch_start_task(struct rcf_comm_connection *handle,
                  char *cbuf, size_t buflen, size_t answer_plen,
                  int priority, const char *rtn, te_bool is_argv,
                  int argc, uint32_t *params)
{
    void *addr = rcf_ch_symbol_addr(rtn, TRUE);
    int   pid;

    VERB("Start task handler is executed");

    if (addr != NULL)
    {
        VERB("fork process with entry point '%s'", rtn);

        if ((pid = fork()) == 0)
        {
            if (is_argv)
                ((rcf_rtn)(addr))(argc, params);
            else
                ((rcf_rtn)(addr))(params[0], params[1], params[2], params[3],
                                  params[4], params[5], params[6], params[7],
                                  params[8], params[9]);
            exit(0);
        }

        SEND_ANSWER("%d %d", 0, pid);
    }

    /* Try shell process */
    if (is_argv)
    {
        char check_cmd[RCF_MAX_PATH];

        sprintf(check_cmd,
                "TMP=`which %s 2>/dev/null` ; test -n \"$TMP\" ;", rtn);
        if (system(check_cmd) != 0)
            SEND_ANSWER("%d", ETENOSUCHNAME);

        if ((pid = fork()) == 0)
        {
            execlp(rtn, rtn, params[0], params[1], params[2], params[3],
                             params[4], params[5], params[6], params[7],
                             params[8], params[9]);
            exit(0);
        }

#if 0
#ifdef HAVE_SYS_RESOURCE_H
        if (setpriority(PRIO_PROCESS, pid, priority) != 0)
        {
            ERROR("setpriority() failed - continue", errno);
            /* Continue with default priority */
        }
#else
        UNUSED(priority);
        ERROR("Unable to set task priority, ignore it.");
#endif
#endif

        SEND_ANSWER("%d %d", 0, pid);
    }

    SEND_ANSWER("%d", ETENOSUCHNAME);
}


/* See description in rcf_ch_api.h */
int
rcf_ch_kill_task(struct rcf_comm_connection *handle,
                 char *cbuf, size_t buflen, size_t answer_plen,
                 unsigned int pid)
{
    kill(pid, SIGTERM);
#if 0    
    kill(pid, SIGKILL); 
#endif    
    SEND_ANSWER("0");
}



/**
 * Routine to be executed remotely to run any program from shell.
 *
 * @param argc      - number of arguments in array
 * @param argv      - array with pointer to string arguments
 *
 * @return Status code.
 *
 * @todo Use system-dependent maximum command line length.
 */
int
shell(int argc, char * const argv[])
{
    char            cmdbuf[2048];
    unsigned int    cmdlen = sizeof(cmdbuf);
    int             i;
    unsigned int    used = 0;
    int             rc;


    for (i = 0; (i < argc) && (used < cmdlen); ++i)
    {
        used += snprintf(cmdbuf + used, cmdlen - used, "%s ", argv[i]);
    }
    if (used >= cmdlen)
        return TE_RC(TE_TA_WIN32, ETESMALLBUF);

    rc = system(cmdbuf);
    if (rc == -1)
        return TE_RC(TE_TA_WIN32, errno);

#ifdef WCOREDUMP
    if (WCOREDUMP(rc))
        ERROR("Command executed in shell dumped core");
#endif
    if (!WIFEXITED(rc))
        ERROR("Abnormal termination of command executed in shell");

    return TE_RC(TE_TA_WIN32, WEXITSTATUS(rc));
}


/**
 * Restart system service.
 *
 * @param service       name of the service (e.g. dhcpd)
 *
 * @return 0 (success) or system error
 */
int
restart_service(char *service)
{
    char cmd[RCF_MAX_PATH];
    int  rc;

    sprintf(cmd, "/etc/rc.d/init.d/%s restart", service);
    rc = system(cmd);

    if (rc < 0)
        rc = EPERM;

    return TE_RC(TE_TA_WIN32, rc);
}

/**
 * Create a file with specified size filled by the specified pattern.
 *
 * @return 0 (success) or system error
 */
int
create_data_file(char *pathname, char c, int len)
{
    char  buf[1024];
    FILE *f;
    
    if ((f = fopen(pathname, "w")) == NULL)
        return TE_RC(TE_TA_WIN32, errno);
    
    memset(buf, c, sizeof(buf));
    
    while (len > 0)
    {
        int copy_len = ((unsigned int)len  > sizeof(buf)) ?
                           (int)sizeof(buf) : len;

        if ((copy_len = fwrite((void *)buf, sizeof(char), copy_len, f)) < 0)
        {
            fclose(f);
            return TE_RC(TE_TA_WIN32, errno);
        }
        len -= copy_len;
    }
    if (fclose(f) < 0)
    {
        ERROR("fclose() failed errno=%d", errno);
        return TE_RC(TE_TA_WIN32, errno);
    }

    return 0;
}

/* Declarations of routines to be linked with the agent */
int
ta_rtn_unlink(char *arg)
{
    int rc = unlink(arg);

    VERB("%s(): arg=%s rc=%d errno=%d", __FUNCTION__,
         (arg == NULL) ? "(null)" : "" /*arg*/, rc, errno);
    return (rc == 0) ? 0 : errno;
}


/**
 * Signal handler to be registered for SIGINT signal.
 * 
 * @param sig   Signal number
 */
static void
ta_sigint_handler(int sig)
{
    /*
     * We can't log here using TE logging facilities, but we need
     * to make a mark, that TA was killed.
     */
    fprintf(stderr, "Test Agent killed by %d signal\n", sig);
    exit(EXIT_FAILURE);
}

/**
 * Signal handler to be registered for SIGPIPE signal.
 * 
 * @param sig   Signal number
 */
static void
ta_sigpipe_handler(int sig)
{
    WARN("Test Agent received SIGPIPE signal");
}


/**
 * Entry point of the Windows Test Agent.
 *
 * Usage:
 *     tawin32 <ta_name> <communication library configuration string>
 */
int
main(int argc, char **argv)
{
    int rc, retval = 0;
    
    char buf[16];

#if 0
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,0),&wsaData) != 0)
    {
        fprintf(stdout, "Socket Initialization Error. Program aborted\n");
        return -1;
    }
#endif
    
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    if (argc < 3)
    {
        fprintf(stderr, "Invalid number of arguments\n");
        return -1;
    }

    my_execname = argv[0];
    
    fprintf(stdout, "Starting win32 test agent\n");

    fprintf(stdout, "Try to call tarpc_init()\n");
    
#ifdef RCF_RPC
    /* After execve */
    if (strcmp(argv[1], "rpcserver") == 0)
    {
        fprintf(stdout, "Try to call tarpc_init()\n");
        tarpc_init(argc, argv);
        return 0;
    }
#endif

    fprintf(stdout, "Try to call get_pid()\n");
    ta_pid = getpid();

    fprintf(stdout, "Try to call signal(SIGINT)\n");
    (void)signal(SIGINT, ta_sigint_handler);
    fprintf(stdout, "Try to call signal(SIGPIPE)\n");
    (void)signal(SIGPIPE, ta_sigpipe_handler);

    fprintf(stdout, "Try to call init_log()\n");
    if ((rc = log_init()) != 0)
    {
        fprintf(stderr, "log_init() failed: error=%d\n", rc);
        return rc;
    }

    te_lgr_entity = ta_name = argv[1];
    VERB("Started\n");

    sprintf(buf, "PID %u", getpid());

    fprintf(stdout, "Try to call rcf_pch_run(%s, %s)\n", argv[2], buf);

    rc = rcf_pch_run(argv[2], buf);
    if (rc != 0)
    {
        fprintf(stderr, "rcf_pch_run() failed: error=%d\n", rc);
        if (retval == 0)
            retval = rc;
    }
    fprintf(stdout, "rcf_pch_run() returned %d (0x%08x)\n", rc, rc);

    fprintf(stdout, "Try to call log_shutdown()\n");
    rc = log_shutdown();
    if (rc != 0)
    {
        fprintf(stderr, "log_shutdown() failed: error=%d\n", rc);
        if (retval == 0)
            retval = rc;
    }

#ifdef RCF_RPC
    fprintf(stdout, "Try to call tarpc_destroy_all()\n");
    tarpc_destroy_all();
#endif    

    fprintf(stdout, "Finishing win32 test agent\n");

    fprintf(stdout, "Finish()\n");

    return retval;
}

