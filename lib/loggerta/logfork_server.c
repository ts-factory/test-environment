/** @file
 * @brief Logger subsystem API - TA side
 *
 * TA side Logger functionality for
 * forked TA processes and newly created threads
 * 
 * Copyright (C) 2003-2018 OKTET Labs. All rights reserved.
 *
 * 
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 * @author Mamadou Ngom <Mamadou.Ngom@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "LogFork Server"

#include "te_config.h"

#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/socket.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif 
#if HAVE_STRINGS_H
#include <strings.h>
#endif 
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "te_defs.h"
#include "te_stdint.h"
#include "te_errno.h"
#include "logger_api.h"
#include "te_tools.h"
#include "ta_common.h"

#include "logfork.h"
#include "logfork_int.h"


/** Structure to store process info */
typedef struct list {
    struct list *next;

    char     name[LOGFORK_MAXLEN];    
    pid_t    pid;
    uint32_t tid;
} list;

/** LogFork server data */
typedef struct logfork_data {
    int     sockd;
    list   *proc_list;
} logfork_data;


/** 
 * Find process name by its pid and tid in the internal list of
 * processes
 *
 * @param pid   process or thread identifier
 * @param tid   thread identifier      
 * @param name  pointer to process or thread name
 *
 * @retval  0      found
 * @retval -1      not found
 */
static int 
logfork_find_name_by_pid(list **proc_list, char **name, int pid, 
                         uint32_t tid)
{
    list *tmp = *proc_list;
    
    for (; tmp; tmp = tmp->next)
    {
        if (tmp->pid == pid && tmp->tid == tid)
        {
            *name = tmp->name;
            return 0;
        }
    }
    return -1;
}

/** 
 * Used to add process info in the internal list
 *
 * @param  proc_list  pointer to the list
 * @param  name       pointer to searched process name
 * @param  pid        process pid
 * @param  tid        thread pid
 *
 * @retval  0    success
 * @retval  -1   memory allocation failure
 */
static int
logfork_list_add(list **proc_list, char *name, 
                 pid_t pid, uint32_t tid)
{  
    list *item = NULL;

    if((item = malloc(sizeof(*item))) == NULL)
    {
        return -1;
    }
    
    strcpy(item->name, name);
    item->pid = pid;            
    item->tid = tid;
    if (*proc_list == NULL)
    {
        *proc_list = item; 
        (*proc_list)->next = NULL;
    }
    else
    {
        item->next = *proc_list;
        *proc_list = item;
    }
    
    return 0;
}

/** 
 * Delete process or thread info from the internal list
 *
 * @param  proc_list  pointer to the list
 * @param  pid        process pid
 * @param  tid        thread pid
 *
 * @retval  0    success
 * @retval  -1   memory allocation failure
 */
static int
logfork_list_del(list **proc_list, 
                 pid_t pid, uint32_t tid)
{  
    list *item = *proc_list;
    list *prev = NULL;
    list *tmp = NULL;

    while (item != NULL)
    {
        for ( ; item != NULL && !(item->pid == pid &&
                                  (item->tid == tid || tid == 0));
             prev = item, item = item->next);

        if (item == NULL)
            return 0;

        if (prev == NULL)
            *proc_list = item->next;
        else
            prev->next = item->next;

        tmp = item->next;
        free(item);
        item = tmp;
    }
    
    return 0;
}

/**
 * Destroy the internal list of process info, when some failure
 * happens. When everything is going well this routine is never
 * called. Memory is destroyed when TA is going down. 
 *
 * @param  list   pointer to the list
 */
static void
logfork_destroy_list(list **list)
{
    struct list *tmp;
    
    while (*list != NULL)
    {
        tmp = (*list)->next;
        free(*list);
        *list = tmp;
    }
}

/**
 * Close opened socket and clear the list of process info.
 *
 * @param  sockd  socket descriptor
 */
static void
logfork_cleanup(void *opaque)
{
    logfork_data   *data = opaque;

    if (data == NULL)
        return;

    (void)close(data->sockd);
    logfork_destroy_list(&data->proc_list);
}


/** Thread entry point */
void 
logfork_entry(void)
{
    logfork_data        data = { -1, NULL };

    struct sockaddr_in  servaddr;
    socklen_t           addrlen;
    
    char *name;
    char  name_pid[LOGFORK_MAXLEN];
    char  port[16];
    
    logfork_msg msg;

#if HAVE_PTHREAD_H
    /* It seems, recv() is not a cancelation point on Solaris. */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);    

    pthread_cleanup_push(logfork_cleanup, &data);
#endif

    do {
        data.sockd = socket(PF_INET, SOCK_DGRAM, 0);
        if (data.sockd < 0)
        {
            fprintf(stderr, "logfork_entry(): cannot create socket\n");
            break;
        }

#if HAVE_FCNTL_H
        /* 
         * Try to set close-on-exec flag, but ignore failures, 
         * since it's not critical.
         */
        (void)fcntl(data.sockd, F_SETFD, FD_CLOEXEC);
#endif

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        servaddr.sin_port = htons(0);
        
        if (bind(data.sockd, CONST_SA(&servaddr), sizeof(servaddr)) < 0)
        {
            ERROR("logfork_entry(): bind() failed; errno %d", errno);
            break;
        }
        
        addrlen = sizeof(servaddr);
        if (getsockname(data.sockd, SA(&servaddr), &addrlen) < 0)
        {
            ERROR("logfork_entry(): getsockname() failed; errno %d", errno);
            break;
        }
        
        sprintf(port, "%d", ntohs(servaddr.sin_port));
        if (setenv("TE_LOG_PORT", port, 1) < 0)
        {
            int err = TE_OS_RC(TE_RCF_PCH, errno);
            
            ERROR("Failed to set TE_LOG_PORT environment variable: "
                  "error=%r", err);
        }

        while (1)
        {         
            int len;
            
            if ((len = recv(data.sockd, (char *)&msg, sizeof(msg), 0)) <= 0)
            {
                WARN("logfork_entry(): recv() failed, len=%d; errno %d", 
                     len, errno);
                continue;
            }
            
            if (len != sizeof(msg))
            {
                ERROR("logfork_entry(): log message length is %d instead %d", 
                      len, sizeof(msg));
                continue;
            }
            
            /* If udp message */
            if (!msg.is_notif)
            {   
                if (logfork_find_name_by_pid(&data.proc_list, &name, 
                                             msg.pid, msg.tid) != 0)
                {
                    name = "Unnamed";
                }
                TE_SPRINTF(name_pid, "%s.%u.%u",
                           name, (unsigned)msg.pid, (unsigned)msg.tid);

                te_log_message_ts(__FILE__, __LINE__, 
                                  msg.__log_sec, msg.__log_usec,
                                  msg.__log_level, TE_LGR_ENTITY, 
                                  msg.__lgr_user, 
                                  "%s: %s", name_pid, msg.__log_msg);
            }
            else if (!msg.__to_delete) 
            {
                if (logfork_find_name_by_pid(&data.proc_list, &name, 
                                             msg.pid, msg.tid) == 0)
                {
                    snprintf(name, LOGFORK_MAXUSER, "%s", msg.__name);
                    continue;
                }

                if (logfork_list_add(&data.proc_list, msg.__name, 
                                     msg.pid, msg.tid) != 0)
                {
                    ERROR("logfork_entry(): out of Memory");
                    break;
                }
            }
            else
            {
                if (logfork_list_del(&data.proc_list, 
                                     msg.pid, msg.tid) != 0)
                {
                    ERROR("logfork_entry(): failed to delete a "
                          "entry %s from processes/threads list",
                          msg.__name);
                    break;
                }
            }
            
        } /* while(1) */

    } while (0);
    
#if HAVE_PTHREAD_H
    pthread_cleanup_pop(!0);
#else
    logfork_cleanup(&data);
#endif
}
