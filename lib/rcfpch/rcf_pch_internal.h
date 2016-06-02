/** @file
 * @brief RCF Portable Command Handler
 *
 * Internal definition of the PCH library.
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
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
 * @author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TE_RCF_PCH_INTERNAL_H__
#define __TE_RCF_PCH_INTERNAL_H__

#define TE_LGR_USER     "RCF PCH"

#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "te_defs.h"
#include "comm_agent.h"
#include "rcf_ch_api.h"
#include "logger_api.h"


#define PRINT(msg...) \
    do { printf(msg); printf("\n"); fflush(stdout); } while (0)

/** Size of the log data sent in one request */
#define RCF_PCH_LOG_BULK        8192

/**
 * Skip spaces in the command.
 *
 * @param _ptr  - pointer to string
 */
#define SKIP_SPACES(_ptr) \
    do {                        \
        while (*(_ptr) == ' ')  \
            (_ptr)++;           \
    } while (FALSE)

/**
 * Send answer to the TEN and return (is called from default command
 * handlers).
 *
 * It is assumed that local variables cbuf (command buffer pointer),
 * buflen (length of the command buffer), answer_plen (length of data
 * to be copied from the command to the answer), conn (connection to
 * TEN handle) are defined in the context from which the macro is
 * called.
 *
 * @param _fmt  - format string
 */
#define SEND_ANSWER(_fmt...) \
    do {                                                        \
        int    _rc;                                             \
        size_t _len = answer_plen;                              \
                                                                \
        /*                                                      \
         * Add how much characters would be printed including   \
         * trailing '\0'.                                       \
         */                                                     \
        _len += snprintf(cbuf + answer_plen,                    \
                         buflen - answer_plen, _fmt) + 1;       \
        if (_len > buflen)                                      \
        {                                                       \
            ERROR("Answer is truncated");                       \
            cbuf[buflen - 1] = '\0';                            \
            _len = buflen;                                      \
        }                                                       \
        RCF_CH_LOCK;                                            \
        _rc = rcf_comm_agent_reply(conn, cbuf, _len);           \
        RCF_CH_UNLOCK;                                          \
        EXIT("%d", _rc);                                        \
        return _rc;                                             \
    } while (FALSE)


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write string to the answer buffer (inserting '\' before '"')
 * in double quotes.
 *
 * @param dst     pointer in the answer buffer where string should be
 *                placed
 * @param src     string to be written to
 * @param len     maximum number of symbols to be copied
 */
static inline void
write_str_in_quotes(char *dst, const char *src, size_t len)
{
    char   *p = dst;
    size_t  i;

    *p++ = ' ';
    *p++ = '\"';
    for (i = 0; (*src != '\0') && (i < len); ++i)
    {
        /* Encode '\n' also */
        if (*src == '\n')
        {
            *p++ ='\\';
            *p++ = 'n';
            src++;
            continue;
        }

        if (*src == '\"' || *src == '\\')
        {
            *p++ = '\\';
        }
        *p++ = *src++;
    }
    *p++ = '\"';
    *p = '\0';
}


/*
 * When ANSI C compiler mode is enabled, the following functions are
 * missing in standard headers 'string.h' and 'stdlib.h'.
 *
 * @todo Investigate and fix it in right way.
 */
#ifdef __STRICT_ANSI__
extern char *strdup(const char *s);
extern long long int strtoll(const char *nptr, char **endptr, int base);
#endif

/** Data corresponding to one RPC server */
struct rpcserver;

/** Data corresponding to one RPC server plugin */
struct rpcserver_plugin;

/** Function definition which helps to call the RPC functions */
typedef int (*rcf_pch_rpc_call)(
        struct rpcserver *rpcs, char *name, void *in, void *out);

/**
 * Find the RPC server with specified @p name.
 *
 * @param name  The name of RPC server
 *
 * @return  RPC server handle or @c NULL
 */
extern struct rpcserver *rcf_pch_find_rpcserver(const char *name);

/**
 * Get the first element of RPC server list.
 *
 * @return  the first RPC server from list of RPC servers
 */
extern struct rpcserver *rcf_pch_rpcserver_first(void);

/**
 * Get the next element of RPC server list from current RPC server.
 *
 * @param rpcs  current RPC server
 *
 * @return      the next RPC server from current RPC server or @c NULL
 */
extern struct rpcserver *rcf_pch_rpcserver_next(struct rpcserver *rpcs);

/**
 * Get the name of RPC server.
 *
 * @param rpcs  RPC server
 *
 * @return      the name of RPC server
 */
extern const char *rcf_pch_rpcserver_get_name(const struct rpcserver *rpcs);

/**
 * Add the node rpcserver_plugin in configuration tree,
 * initialize the mutex and the RPC call.
 *
 * @param rcf_pch_lock  Lock for protect RPC servers and plugins list
 * @param rcf_pch_call  Function which helps to call the RPC functions
 */
extern void rcf_pch_rpcserver_plugin_init(
        pthread_mutex_t *rcf_pch_lock,
        rcf_pch_rpc_call rcf_pch_call);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !__TE_RCF_PCH_INTERNAL_H__ */
