/** @file
 * @brief Test API - Socket API RPC
 *
 * TAPI for remote socket calls implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SEMAPHORE_H
#include <semaphore.h>
#else
#error "We have no semaphores"
#endif
#include <stdarg.h>

#define LGR_USER    "Sockets RPC TAPI"
#define LOG_LEVEL   (ERROR_LVL | WARNING_LVL | RING_LVL | \
                     INFORMATION_LVL/* | VERBOSE_LVL*/)
#include "logger_api.h"

#include "te_defs.h"
#include "te_stdint.h"
#include "te_errno.h"
#include "rcf_api.h"
#include "rcf_rpc.h"
#include "tapi_rpcsock.h"
#include "tarpc.h"


/** @name String buffers for snprintf() operations */
static char str_buf_1[1024];
static char str_buf_2[1024];
/*@}*/


/**
 * Convert I/O vector to array.
 *
 * @param len      total data length
 * @param v        I/O vector
 * @param cnt      number of elements in vector
 *
 * @return Pointer to allocated array or NULL.
 */
static uint8_t *
rpc_iovec_to_array(size_t len, const struct rpc_iovec *v, size_t cnt)
{
    uint8_t *array = malloc(len);
    uint8_t *p     = array;
    size_t   i;

    if (array == NULL)
    {
        ERROR("Allocation of %u bytes failure", len);
        return NULL;
    }

    for (i = 0; i < cnt && len > 0; ++i)
    {
        size_t copylen = (v[i].iov_len < len) ? v[i].iov_len : len;

        memcpy(p, v[i].iov_base, copylen);
        p += copylen;
        len -= copylen;
    }
    if (len != 0)
    {
        ERROR("I/O vector total length is less than length by elements");
        free(array);
        return NULL;
    }
    return array;
}

/* See description in tapi_rpcsock.h */
int
rpc_iovec_cmp(size_t v1len, const struct rpc_iovec *v1, size_t v1cnt,
              size_t v2len, const struct rpc_iovec *v2, size_t v2cnt)
{
    int      result;
    uint8_t *array1 = NULL;
    uint8_t *array2 = NULL;

    if (v1len != v2len)
        return -1;

    array1 = rpc_iovec_to_array(v1len, v1, v1cnt);
    array2 = rpc_iovec_to_array(v2len, v2, v2cnt);
    if (array1 == NULL || array2 == NULL)
    {
        result = -1;
    }
    else
    {
        result = (memcmp(array1, array2, v1len) == 0) ? 0 : -1;
    }
    
    free(array1);
    free(array2);

    return result;
}


/**
 * Convert RCF RPC operation to string.
 *
 * @param op    - RCF RPC operation
 *
 * @return null-terminated string
 */
static const char *
rpcop2str(rcf_rpc_op op)
{
    switch (op)
    {
        case RCF_RPC_CALL:      return " call";
        case RCF_RPC_WAIT:      return " wait";
        case RCF_RPC_CALL_WAIT: return "";
        default:                assert(FALSE);
    }
    return " (unknown)";
}


/* See description in tapi_rpcsock.h */
const char *
sockaddr2str(const struct sockaddr *sa)
{

#define SOCKADDR2STR_ADDRSTRLEN 128
/* Number of buffers used in the function */
#define N_BUFS 10

    static char  buf[N_BUFS][SOCKADDR2STR_ADDRSTRLEN];
    static char  (*cur_buf)[SOCKADDR2STR_ADDRSTRLEN] = 
                                (char (*)[SOCKADDR2STR_ADDRSTRLEN])buf[0];

    char *ptr;

    /*
     * Firt time the function is called we start from the second buffer, but
     * then after a turn we'll use all N_BUFS buffer.
     */
    if (cur_buf == (char (*)[SOCKADDR2STR_ADDRSTRLEN])buf[N_BUFS - 1])
        cur_buf = (char (*)[SOCKADDR2STR_ADDRSTRLEN])buf[0];
    else
        cur_buf++;

    ptr = *cur_buf;

    if (sa == NULL)
    {
        snprintf(ptr, SOCKADDR2STR_ADDRSTRLEN, "NULL");
        return ptr;
    }
    
    switch (sa->sa_family)
    {
        case AF_INET:
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)sa;

            inet_ntop(AF_INET, (void *)&(sin->sin_addr), *cur_buf,
                      SOCKADDR2STR_ADDRSTRLEN);
            snprintf(ptr + strlen(ptr), SOCKADDR2STR_ADDRSTRLEN - strlen(ptr),
                     ":%hu", ntohs(sin->sin_port));
            break;
        }

        default:
             snprintf(ptr, SOCKADDR2STR_ADDRSTRLEN, "(unknown)");
    }

#undef SOCKADDR2STR_ADDRSTRLEN
#undef N_BUFS

    return ptr;
}

/**
 * Convert 'struct timeval' to string.
 *
 * @note Static buffer is used for return value.
 *
 * @param tv    - pointer to 'struct timeval'
 *
 * @return null-terminated string
 */
const char *
timeval2str(const struct timeval *tv)
{

/* Number of buffers used in the function */
#define N_BUFS 10
#define BUF_SIZE 32

    static char buf[N_BUFS][BUF_SIZE];
    static char (*cur_buf)[BUF_SIZE] = (char (*)[BUF_SIZE])buf[0];

    char *ptr;

    /*
     * Firt time the function is called we start from the second buffer, but
     * then after a turn we'll use all N_BUFS buffer.
     */
    if (cur_buf == (char (*)[BUF_SIZE])buf[N_BUFS - 1])
        cur_buf = (char (*)[BUF_SIZE])buf[0];
    else
        cur_buf++;

    ptr = *cur_buf;

    if (tv == NULL)
    {
        snprintf(ptr, BUF_SIZE, "NULL");
    }
    else
    {
        snprintf(ptr, BUF_SIZE, "{%ld,%ld}", tv->tv_sec, tv->tv_usec);
    }

#undef BUF_SIZE
#undef N_BUFS

    return ptr;
}

/**
 * Convert 'struct timespec' to string.
 *
 * @note Static buffer is used for return value.
 *
 * @param tv    - pointer to 'struct timespec'
 *
 * @return null-terminated string
 */
const char *
timespec2str(const struct timespec *tv)
{
    static char buf[32];
    
    if (tv == NULL)
    {
        strcpy(buf, "NULL");
    }
    else
    {
        sprintf(buf, "{%ld,%ld}", tv->tv_sec, tv->tv_nsec);
    }
    return buf;
}


/** Check, if RPC is called successfully */
#define RPC_CALL_OK \
    ((handle->_errno == 0) || \
     ((handle->_errno >= RPC_EPERM) && (handle->_errno <= RPC_EMEDIUMTYPE)))

#define LOG_TE_ERROR(_func) \
    do {                                                        \
        if (!RPC_CALL_OK)                                       \
        {                                                       \
            ERROR("RPC (%s,%s): " #_func "() failed: %s",       \
                  handle->ta, handle->name,                     \
                  errno_rpc2str(RPC_ERRNO(handle)));            \
        }                                                       \
    } while (FALSE)
    
/** Return with check (for functions returning 0 or -1) */
#define RETVAL_RC(_func) \
    do {                                                                \
        LOG_TE_ERROR(_func);                                            \
        rcf_rpc_free_result(&out, (xdrproc_t)xdr_tarpc_##_func##_out);  \
                                                                        \
        if (!RPC_CALL_OK)                                               \
            return -1;                                                  \
                                                                        \
        if (out.retval != 0 && out.retval != -1)                        \
        {                                                               \
            ERROR("function %s returned incorrect value %d",            \
                  #_func, out.retval);                                  \
            handle->_errno = TE_RC(TE_TAPI, ETECORRUPTED);              \
            return -1;                                                  \
        }                                                               \
        return out.retval;                                              \
    } while(0)

/** Return with check (for functions returning value >= -1) */    
#define RETVAL_VAL(_retval, _func) \
    do {                                                                \
        LOG_TE_ERROR(_func);                                            \
        rcf_rpc_free_result(&out, (xdrproc_t)xdr_tarpc_##_func##_out);  \
                                                                        \
        if (!RPC_CALL_OK)                                               \
            return -1;                                                  \
                                                                        \
        if ((_retval) < -1)                                             \
        {                                                               \
            ERROR("function %s returned incorrect value %d",            \
                  #_func, _retval);                                     \
            handle->_errno = TE_RC(TE_TAPI, ETECORRUPTED);              \
            return -1;                                                  \
        }                                                               \
        return (_retval);                                               \
    } while(0)

/** Return with check (for functions returning pointers) */
#define RETVAL_PTR(_retval, _func) \
    do {                                                                \
        LOG_TE_ERROR(_func);                                            \
        rcf_rpc_free_result(&out, (xdrproc_t)xdr_tarpc_##_func##_out);  \
                                                                        \
        if (!RPC_CALL_OK)                                               \
            return NULL;                                                \
        else                                                            \
            return (void *)(_retval);                                   \
    } while(0)

/**
 * Set name for socket library.
 *
 * @param ta            Test Agent
 * @param libname       name of the library like "/lib/libc.so.6"
 *
 * @return status code
 */
int
rpc_setlibname(const char *ta, const char *libname)
{
    int rc;
    int rc1;
    
    if ((ta == NULL) || (libname == NULL))
        return TE_RC(TE_RCF, EINVAL);

    if ((rc = rcf_ta_call(ta, 0, "setlibname", &rc1, 1, 0,
                          RCF_STRING, libname)) != 0 ||
         (rc = rc1) != 0)
    {
        ERROR("Failed to set socket library name %s on the TA %s", libname, ta);
    }
    
    return rc;
}


int 
rpc_socket(rcf_rpc_server *handle,
           rpc_socket_domain domain, rpc_socket_type type,
           rpc_socket_proto protocol)
{
    tarpc_socket_in  in;
    tarpc_socket_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.domain = domain;
    in.type = type;
    in.proto = protocol;

    rcf_rpc_call(handle, _socket, &in, (xdrproc_t)xdr_tarpc_socket_in,
                 &out, (xdrproc_t)xdr_tarpc_socket_out);

    INFO("RPC (%s,%s): socket(%s, %s, %s) -> %d (%s)",
         handle->ta, handle->name,
         domain_rpc2str(domain), socktype_rpc2str(type),
         proto_rpc2str(protocol), out.fd, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.fd, socket);
}           

int 
rpc_close(rcf_rpc_server *handle, int fd)
{
    tarpc_close_in  in;
    tarpc_close_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.fd = fd;

    rcf_rpc_call(handle, _close, &in, (xdrproc_t)xdr_tarpc_close_in,
                 &out, (xdrproc_t)xdr_tarpc_close_out);
    
    INFO("RPC (%s,%s): close(%d) -> %d (%s)",
         handle->ta, handle->name,
         fd, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(close);
}

int 
rpc_bind(rcf_rpc_server *handle,
         int s, const struct sockaddr *my_addr, socklen_t addrlen)
{
    tarpc_bind_in  in;
    tarpc_bind_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    
    in.fd = s;
    if (my_addr != NULL)
    {
        in.addr.sa_family = addr_family_h2rpc(my_addr->sa_family);
        in.addr.sa_data.sa_data_len = 
            addrlen > SA_COMMON_LEN ? addrlen - SA_COMMON_LEN : 0;
        in.addr.sa_data.sa_data_val = (uint8_t *)my_addr->sa_data;
    }
    in.len = addrlen;
    
    rcf_rpc_call(handle, _bind, &in, (xdrproc_t)xdr_tarpc_bind_in,
                 &out, (xdrproc_t)xdr_tarpc_bind_out);

    INFO("RPC (%s,%s): bind(%d, %s, %u) -> %d (%s)",
         handle->ta, handle->name,
         s, sockaddr2str(my_addr), addrlen,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(bind);
}         

int 
rpc_connect(rcf_rpc_server *handle,
            int s, const struct sockaddr *addr, socklen_t addrlen)
{
    rcf_rpc_op        op = handle->op;
    tarpc_connect_in  in;
    tarpc_connect_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.fd = s;
    if (addr != NULL)
    {
        in.addr.sa_family = addr_family_h2rpc(addr->sa_family);
        in.addr.sa_data.sa_data_len = 
            addrlen > SA_COMMON_LEN ? addrlen - SA_COMMON_LEN : 0;
        in.addr.sa_data.sa_data_val = (uint8_t *)addr->sa_data;
    }
    in.len = addrlen;
    
    rcf_rpc_call(handle, _connect, &in, (xdrproc_t)xdr_tarpc_connect_in,
                 &out, (xdrproc_t)xdr_tarpc_connect_out);

    INFO("RPC (%s,%s)%s: connect(%d, %s, %u) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         s, sockaddr2str(addr), addrlen,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(connect);
}         

int 
rpc_listen(rcf_rpc_server *handle, int fd, int backlog)
{
    tarpc_listen_in  in;
    tarpc_listen_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.fd = fd;
    in.backlog = backlog;
    
    rcf_rpc_call(handle, _listen, &in, (xdrproc_t)xdr_tarpc_listen_in,
                 &out, (xdrproc_t)xdr_tarpc_listen_out);

    INFO("RPC (%s,%s): listen(%d, %d) -> %d (%s)",
         handle->ta, handle->name,
         fd, backlog, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(listen);
}

int 
rpc_accept_gen(rcf_rpc_server *handle,
               int s, struct sockaddr *addr, socklen_t *addrlen,
               socklen_t raddrlen)
{
    rcf_rpc_op       op = handle->op;
    socklen_t        save_addrlen = 
                         (addrlen == NULL) ? (socklen_t)-1 : *addrlen;
    tarpc_accept_in  in;
    tarpc_accept_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (addrlen != NULL && *addrlen > raddrlen)
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }
    
    in.fd = s;
    if (addrlen != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.len.len_len = 1;
        in.len.len_val = addrlen;
    }
    if (addr != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.addr.sa_data.sa_data_len = raddrlen - SA_COMMON_LEN;
        in.addr.sa_data.sa_data_val = addr->sa_data;
        in.addr.sa_family = addr_family_h2rpc(addr->sa_family);
    }
    
    rcf_rpc_call(handle, _accept, &in, (xdrproc_t)xdr_tarpc_accept_in,
                 &out, (xdrproc_t)xdr_tarpc_accept_out);
    
    if (RPC_CALL_OK)
    {
        if (addr != NULL && out.addr.sa_data.sa_data_val != NULL)
        {
            memcpy(addr->sa_data, out.addr.sa_data.sa_data_val, 
                   out.addr.sa_data.sa_data_len);
            addr->sa_family = addr_family_rpc2h(out.addr.sa_family);
        }
        
        if (addrlen != NULL && out.len.len_val != NULL)
            *addrlen = out.len.len_val[0];
    }
    
    INFO("RPC (%s,%s)%s: accept(%d, 0x%x[%u], 0x%x(%u)) -> %d (%s) "
         "peer=%s addrlen=%u",
         handle->ta, handle->name, rpcop2str(op),
         s, addr, raddrlen, addrlen, save_addrlen,
	 out.retval, errno_rpc2str(RPC_ERRNO(handle)),
	 sockaddr2str(addr), (addrlen == NULL) ? (socklen_t)-1 : *addrlen);

    RETVAL_VAL(out.retval, accept);
}             


ssize_t 
rpc_recvfrom_gen(rcf_rpc_server *handle,
                 int s, void *buf, size_t len, rpc_send_recv_flags flags, 
                 struct sockaddr *from, socklen_t *fromlen,
                 size_t rbuflen, socklen_t rfrombuflen)
{
    rcf_rpc_op         op = handle->op;
    socklen_t          save_fromlen = 
                           (fromlen == NULL) ? (socklen_t)-1 : *fromlen;

    tarpc_recvfrom_in  in;
    tarpc_recvfrom_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    if ((fromlen != NULL && *fromlen > rfrombuflen) || 
        (buf != NULL && len > rbuflen))
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }
    
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.fd = s;
    in.len = len;
    if (fromlen != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.fromlen.fromlen_len = 1;
        in.fromlen.fromlen_val = fromlen;
    }
    if (from != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.from.sa_data.sa_data_len = rfrombuflen - SA_COMMON_LEN;
        in.from.sa_data.sa_data_val = from->sa_data;
        in.from.sa_family = addr_family_h2rpc(from->sa_family);
    }
    if (buf != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = rbuflen;
        in.buf.buf_val = buf;
    }
    in.flags = flags;
    
    rcf_rpc_call(handle, _recvfrom, &in, (xdrproc_t)xdr_tarpc_recvfrom_in,
                 &out, (xdrproc_t)xdr_tarpc_recvfrom_out);
    
    if (RPC_CALL_OK)
    {
        if (buf != NULL && out.buf.buf_val != NULL)
            memcpy(buf, out.buf.buf_val, out.buf.buf_len);
        
        if (from != NULL && out.from.sa_data.sa_data_val != NULL)
        {
            memcpy(from->sa_data, out.from.sa_data.sa_data_val, 
                   out.from.sa_data.sa_data_len);
            from->sa_family = addr_family_rpc2h(out.from.sa_family);
        }
        
        if (fromlen != NULL && out.fromlen.fromlen_val != NULL)
            *fromlen = out.fromlen.fromlen_val[0];
    }
    
    INFO("RPC (%s,%s)%s: recvfrom(%d, 0x%x[%u], %u, %s, 0x%x[%u], %u) "
         "-> %d (%s) from=%s fromlen=%u",
         handle->ta, handle->name, rpcop2str(op),
         s, buf, rbuflen, len, send_recv_flags_rpc2str(flags), 
         from, rfrombuflen, save_fromlen,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)),
         sockaddr2str(from),
         (fromlen == NULL) ? (unsigned int)-1 : *fromlen);

    RETVAL_VAL(out.retval, recvfrom);
}             

ssize_t 
rpc_recv_gen(rcf_rpc_server *handle,
                 int s, void *buf, size_t len, rpc_send_recv_flags flags, 
                 size_t rbuflen)
{
    rcf_rpc_op     op = handle->op;
    tarpc_recv_in  in;
    tarpc_recv_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    if (buf != NULL && len > rbuflen)
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }
    
    in.fd = s;
    in.len = len;
    if (buf != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = rbuflen;
        in.buf.buf_val = buf;
    }
    in.flags = flags;
    
    rcf_rpc_call(handle, _recv, &in, (xdrproc_t)xdr_tarpc_recv_in,
                 &out, (xdrproc_t)xdr_tarpc_recv_out);
    
    if (RPC_CALL_OK)
    {
        if (buf != NULL && out.buf.buf_val != NULL)
            memcpy(buf, out.buf.buf_val, out.buf.buf_len);
    }
    
    INFO("RPC (%s,%s)%s: recv(%d, 0x%x[%u], %u, %s) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         s, buf, rbuflen, len, send_recv_flags_rpc2str(flags),
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, recv);
}             

int 
rpc_shutdown(rcf_rpc_server *handle, int s, rpc_shut_how how)
{
    tarpc_shutdown_in  in;
    tarpc_shutdown_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset((char *)&in, 0, sizeof(in));
    memset((char *)&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.fd = s;
    in.how = how;
    
    rcf_rpc_call(handle, _shutdown, &in, (xdrproc_t)xdr_tarpc_shutdown_in,
                 &out, (xdrproc_t)xdr_tarpc_shutdown_out);

    INFO("RPC (%s,%s) shutdown(%d, %s) -> %d (%s)",
         handle->ta, handle->name, s, shut_how_rpc2str(how),
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(shutdown);
}

ssize_t 
rpc_sendto(rcf_rpc_server *handle,
           int s, const void *buf, size_t len,
           int flags,
           const struct sockaddr *to, socklen_t tolen)
{
    rcf_rpc_op       op = handle->op;
    tarpc_sendto_in  in;
    tarpc_sendto_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.fd = s;
    in.len = len;
    if (to != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.to.sa_data.sa_data_len = tolen - SA_COMMON_LEN;
        in.to.sa_data.sa_data_val = (uint8_t *)to->sa_data;
        in.to.sa_family = addr_family_h2rpc(to->sa_family);
    }
    in.tolen = tolen;
    if (buf != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = len;
        in.buf.buf_val = (char *)buf;
    }
    in.flags = flags;
    
    rcf_rpc_call(handle, _sendto, &in, (xdrproc_t)xdr_tarpc_sendto_in,
                 &out, (xdrproc_t)xdr_tarpc_sendto_out);
    
    INFO("RPC (%s,%s)%s: sendto(%d, 0x%x, %u, %s, %s, %u) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         s, buf, len, send_recv_flags_rpc2str(flags),
         sockaddr2str(to), tolen, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, sendto);
}

ssize_t 
rpc_send(rcf_rpc_server *handle,
           int s, const void *buf, size_t len,
           int flags)
{
    rcf_rpc_op     op = handle->op;
    tarpc_send_in  in;
    tarpc_send_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.fd = s;
    in.len = len;
    if (buf != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = len;
        in.buf.buf_val = (char *)buf;
    }
    in.flags = flags;
    
    rcf_rpc_call(handle, _send, &in, (xdrproc_t)xdr_tarpc_send_in,
                 &out, (xdrproc_t)xdr_tarpc_send_out);
    
    INFO("RPC (%s,%s)%s: send(%d, 0x%x, %u, %s) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         s, buf, len, send_recv_flags_rpc2str(flags),
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, send);
}
           

int 
rpc_read_gen(rcf_rpc_server *handle,
             int fd, void *buf, size_t count, size_t rbuflen)
{
    rcf_rpc_op     op = handle->op;
    tarpc_read_in  in;
    tarpc_read_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (buf != NULL && count > rbuflen)
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }
    
    in.fd = fd;
    in.len = count;
    if (buf != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = rbuflen;
        in.buf.buf_val = buf;
    }
    rcf_rpc_call(handle, _read, &in, (xdrproc_t)xdr_tarpc_read_in,
                 &out, (xdrproc_t)xdr_tarpc_read_out);
    
    
    if (RPC_CALL_OK)
    {
        if (buf != NULL && out.buf.buf_val != NULL)
            memcpy(buf, out.buf.buf_val, out.buf.buf_len);
    }

    INFO("RPC (%s,%s)%s: read(%d, 0x%x[%u], %u) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         fd, buf, rbuflen, count, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, read);
}

int 
rpc_write(rcf_rpc_server *handle,
          int fd, const void *buf, size_t count)
{
    rcf_rpc_op      op = handle->op;
    tarpc_write_in  in;
    tarpc_write_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.fd = fd;
    in.len = count;
    if (buf != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = count;
        in.buf.buf_val = (char *)buf;
    }
    
    rcf_rpc_call(handle, _write, &in, (xdrproc_t)xdr_tarpc_write_in,
                 &out, (xdrproc_t)xdr_tarpc_write_out);
    
    INFO("RPC (%s,%s)%s: write(%d, 0x%x, %u) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         fd, buf, count, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, write);
}

int 
rpc_readv_gen(rcf_rpc_server *handle,
              int fd, const struct rpc_iovec *iov,
              size_t iovcnt, size_t riovcnt)
{
    rcf_rpc_op      op = handle->op;
    tarpc_readv_in  in;
    tarpc_readv_out out;

    struct tarpc_iovec iovec_arr[RCF_RPC_MAX_IOVEC];
    
    size_t i;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(iovec_arr, 0, sizeof(iovec_arr));
    
    if (riovcnt > RCF_RPC_MAX_IOVEC)
    {
        handle->_errno = TE_RC(TE_RCF, ENOMEM);
        return -1;
    }
    
    if (iov != NULL && iovcnt > riovcnt)
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }


    in.fd = fd;
    in.count = iovcnt;

    VERB("IN readv(%d, 0x%x[%u], %u)", fd, iov, riovcnt, riovcnt);
    if (iov != NULL)
    {
        in.vector.vector_len = riovcnt;
        in.vector.vector_val = iovec_arr;
        for (i = 0; i < riovcnt; i++)
        {
            VERB("IN readv() I/O vector #%d: 0x%x[%u] %u",
                 i, iov[i].iov_base, iov[i].iov_rlen, iov[i].iov_len);
            iovec_arr[i].iov_base.iov_base_val = iov[i].iov_base;
            iovec_arr[i].iov_base.iov_base_len = iov[i].iov_rlen;
            iovec_arr[i].iov_len = iov[i].iov_len;
        }
    }

    rcf_rpc_call(handle, _readv, &in, (xdrproc_t)xdr_tarpc_readv_in,
                 &out, (xdrproc_t)xdr_tarpc_readv_out);
    
    if (RPC_CALL_OK && iov != NULL && out.vector.vector_val != NULL)
    {
        for (i = 0; i < riovcnt; i++)
        {
            ((struct rpc_iovec *)iov)[i].iov_len = 
                out.vector.vector_val[i].iov_len;
            if ((iov[i].iov_base != NULL) &&
                (out.vector.vector_val[i].iov_base.iov_base_val != NULL))
            {
                memcpy(iov[i].iov_base,
                       out.vector.vector_val[i].iov_base.iov_base_val,
                       iov[i].iov_rlen);
            }
        }
    }

    INFO("RPC (%s,%s)%s: readv() -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, readv);
}
          
int 
rpc_writev(rcf_rpc_server *handle,
           int fd, const struct rpc_iovec *iov, size_t iovcnt)
{
    rcf_rpc_op       op = handle->op;
    tarpc_writev_in  in;
    tarpc_writev_out out;

    struct tarpc_iovec iovec_arr[RCF_RPC_MAX_IOVEC];
    
    size_t i;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(iovec_arr, 0, sizeof(iovec_arr));
    
    if (iovcnt > RCF_RPC_MAX_IOVEC)
    {
        handle->_errno = TE_RC(TE_RCF, ENOMEM);
        return -1;
    }

    for (i = 0; i < iovcnt && iov != NULL; i++)
    {
        iovec_arr[i].iov_base.iov_base_val = iov[i].iov_base;
        iovec_arr[i].iov_base.iov_base_len = iov[i].iov_rlen;
        iovec_arr[i].iov_len = iov[i].iov_len;
    }
    
    if (iov != NULL)
    {
        in.vector.vector_val = iovec_arr;
        in.vector.vector_len = iovcnt;
    }
    
    in.fd = fd;
    in.count = iovcnt;
    
    rcf_rpc_call(handle, _writev, &in, (xdrproc_t)xdr_tarpc_writev_in,
                 &out, (xdrproc_t)xdr_tarpc_writev_out);

    INFO("RPC (%s,%s)%s: writev() -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op), out.retval,
         errno_rpc2str(RPC_ERRNO(handle)));
    
    RETVAL_VAL(out.retval, writev);
}           

int 
rpc_getsockname_gen(rcf_rpc_server *handle,
                    int s, struct sockaddr *name,
                    socklen_t *namelen,
                    socklen_t rnamelen)
{
    socklen_t   namelen_save =
        (namelen == NULL) ? (unsigned int)-1 : *namelen;

    tarpc_getsockname_in  in;
    tarpc_getsockname_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    if (namelen != NULL && *namelen > rnamelen)
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }
    
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    handle->op = RCF_RPC_CALL_WAIT;
    
    in.fd = s;
    if (namelen != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.len.len_len = 1;
        in.len.len_val = namelen;
    }
    if (name != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.addr.sa_data.sa_data_len = rnamelen - SA_COMMON_LEN;
        in.addr.sa_data.sa_data_val = name->sa_data;
        in.addr.sa_family = addr_family_h2rpc(name->sa_family);
    }
    
    rcf_rpc_call(handle, _getsockname, &in, (xdrproc_t)xdr_tarpc_getsockname_in,
                 &out, (xdrproc_t)xdr_tarpc_getsockname_out);
    
    if (RPC_CALL_OK)
    {
        if (name != NULL && out.addr.sa_data.sa_data_val != NULL)
        {
            memcpy(name->sa_data, out.addr.sa_data.sa_data_val, 
                   out.addr.sa_data.sa_data_len);
            name->sa_family = addr_family_rpc2h(out.addr.sa_family);
        }
        
        if (namelen != NULL && out.len.len_val != NULL)
            *namelen = out.len.len_val[0];
    }
    
    INFO("RPC (%s,%s): getsockname(%d, 0x%x[%u], %u) -> %d (%s) "
         "name=%s namelen=%u",
         handle->ta, handle->name,
         s, name, rnamelen, namelen_save,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)),
         sockaddr2str(name),
         (namelen == NULL) ? (unsigned int)-1 : *namelen);

    RETVAL_RC(getsockname);
}             

int 
rpc_getpeername_gen(rcf_rpc_server *handle,
                    int s, struct sockaddr *name,
                    socklen_t *namelen,
                    socklen_t rnamelen)
{
    socklen_t   namelen_save =
        (namelen == NULL) ? (unsigned int)-1 : *namelen;

    tarpc_getpeername_in  in;
    tarpc_getpeername_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    if (namelen != NULL && *namelen > rnamelen)
    {
        handle->_errno = TE_RC(TE_RCF, EINVAL);
        return -1;
    }
    
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    handle->op = RCF_RPC_CALL_WAIT;
    
    in.fd = s;
    if (namelen != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.len.len_len = 1;
        in.len.len_val = namelen;
    }
    if (name != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.addr.sa_data.sa_data_len = rnamelen - SA_COMMON_LEN;
        in.addr.sa_data.sa_data_val = name->sa_data;
        in.addr.sa_family = addr_family_h2rpc(name->sa_family);
    }
    
    rcf_rpc_call(handle, _getpeername, &in, (xdrproc_t)xdr_tarpc_getpeername_in,
                 &out, (xdrproc_t)xdr_tarpc_getpeername_out);
    
    if (RPC_CALL_OK)
    {
        if (name != NULL && out.addr.sa_data.sa_data_val != NULL)
        {
            memcpy(name->sa_data, out.addr.sa_data.sa_data_val, 
                   out.addr.sa_data.sa_data_len);
            name->sa_family = addr_family_rpc2h(out.addr.sa_family);
        }
        
        if (namelen != NULL && out.len.len_val != NULL)
            *namelen = out.len.len_val[0];
    }
    
    INFO("RPC (%s,%s): getpeername(%d, 0x%x[%u], %u) -> %d (%s) "
         "name=%s namelen=%u",
         handle->ta, handle->name,
         s, name, rnamelen, namelen_save,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)),
         sockaddr2str(name),
         (namelen == NULL) ? (unsigned int)-1 : *namelen);

    RETVAL_RC(getpeername);
}             

rpc_fd_set *
rpc_fd_set_new(rcf_rpc_server *handle)
{
    tarpc_fd_set_new_in  in;
    tarpc_fd_set_new_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    
    rcf_rpc_call(handle, _fd_set_new, &in, (xdrproc_t)xdr_tarpc_fd_set_new_in,
                 &out, (xdrproc_t)xdr_tarpc_fd_set_new_out);

    INFO("RPC (%s,%s): fd_set_new() -> 0x%x (%s)",
         handle->ta, handle->name,
         (unsigned int)(out.retval), errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_PTR(out.retval, fd_set_new);
}           


void 
rpc_fd_set_delete(rcf_rpc_server *handle, rpc_fd_set *set)
{
    tarpc_fd_set_delete_in  in;
    tarpc_fd_set_delete_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_fd_set)set;
    
    rcf_rpc_call(handle, _fd_set_delete, &in, (xdrproc_t)xdr_tarpc_fd_set_delete_in,
                 &out, (xdrproc_t)xdr_tarpc_fd_set_delete_out);

    INFO("RPC (%s,%s): fd_set_delete(0x%x) -> (%s)",
         handle->ta, handle->name,
         (unsigned int)set, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(fd_set_delete);
}

void 
rpc_do_fd_zero(rcf_rpc_server *handle, rpc_fd_set *set)
{
    tarpc_do_fd_zero_in  in;
    tarpc_do_fd_zero_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_fd_set)set;
    
    rcf_rpc_call(handle, _do_fd_zero, &in, (xdrproc_t)xdr_tarpc_do_fd_zero_in,
                 &out, (xdrproc_t)xdr_tarpc_do_fd_zero_out);

    INFO("RPC (%s,%s): do_fd_zero(0x%x) -> (%s)",
         handle->ta, handle->name,
         (unsigned int)set, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(do_fd_zero);
}

void 
rpc_do_fd_set(rcf_rpc_server *handle, int fd, rpc_fd_set *set)
{
    tarpc_do_fd_set_in  in;
    tarpc_do_fd_set_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_fd_set)set;
    in.fd = fd;
    
    rcf_rpc_call(handle, _do_fd_set, &in, (xdrproc_t)xdr_tarpc_do_fd_set_in,
                 &out, (xdrproc_t)xdr_tarpc_do_fd_set_out);

    INFO("RPC (%s,%s): do_fd_set(%d, 0x%x) -> (%s)",
         handle->ta, handle->name,
         fd, (unsigned int)set, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(do_fd_set);
}

void 
rpc_do_fd_clr(rcf_rpc_server *handle, int fd, rpc_fd_set *set)
{
    tarpc_do_fd_clr_in  in;
    tarpc_do_fd_clr_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_fd_set)set;
    in.fd = fd;
    
    rcf_rpc_call(handle, _do_fd_clr, &in, (xdrproc_t)xdr_tarpc_do_fd_clr_in,
                 &out, (xdrproc_t)xdr_tarpc_do_fd_clr_out);

    INFO("RPC (%s,%s): do_fd_clr(%d, 0x%x) -> (%s)",
         handle->ta, handle->name,
         fd, (unsigned int)set, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(do_fd_clr);
}

int
rpc_do_fd_isset(rcf_rpc_server *handle, int fd, rpc_fd_set *set)
{
    tarpc_do_fd_isset_in  in;
    tarpc_do_fd_isset_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_fd_set)set;
    in.fd = fd;
    
    rcf_rpc_call(handle, _do_fd_isset,
                 &in, (xdrproc_t)xdr_tarpc_do_fd_isset_in,
                 &out, (xdrproc_t)xdr_tarpc_do_fd_isset_out);

    INFO("RPC (%s,%s): do_fd_isset(%d, 0x%x) -> %d (%s)",
         handle->ta, handle->name,
         fd, (unsigned int)set, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(do_fd_isset);

    if (!RPC_CALL_OK)                     
        return -1;                                        
                                                          
    if (out.retval != 0 && out.retval != 1)               
    {                                                     
        INFO("FD_ISSET() returned %d, not boolean value (0 or 1)",
             out.retval);
    }                                                     
    return out.retval;                                    
}

int 
rpc_select(rcf_rpc_server *handle,
           int n, rpc_fd_set *readfds, rpc_fd_set *writefds,
           rpc_fd_set *exceptfds, struct timeval *timeout)
{
    rcf_rpc_op       op = handle->op;
    tarpc_select_in  in;
    tarpc_select_out out;
    struct timeval   timeout_in;
    struct timeval  *timeout_in_ptr = NULL;
    
    struct tarpc_timeval tv;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.n = n;
    in.readfds = (tarpc_fd_set)readfds;
    in.writefds = (tarpc_fd_set)writefds;
    in.exceptfds = (tarpc_fd_set)exceptfds;
    
    if (timeout != NULL && handle->op != RCF_RPC_WAIT)
    {
        tv.tv_sec = timeout->tv_sec;
        tv.tv_usec = timeout->tv_usec;
        in.timeout.timeout_len = 1;
        in.timeout.timeout_val = &tv;
        
        timeout_in_ptr = &timeout_in;
        timeout_in = *timeout;
    }
    
    rcf_rpc_call(handle, _select, &in, (xdrproc_t)xdr_tarpc_select_in,
                 &out, (xdrproc_t)xdr_tarpc_select_out);

    if (op != RCF_RPC_CALL && timeout != NULL &&
        out.timeout.timeout_val != NULL && RPC_CALL_OK)
    {
        timeout->tv_sec = out.timeout.timeout_val[0].tv_sec;
        timeout->tv_usec = out.timeout.timeout_val[0].tv_usec;
    }

    if (!RPC_CALL_OK)
        out.retval = -1;

    INFO("RPC (%s,%s)%s: select(%d, 0x%x, 0x%x, 0x%x, %s (%s)) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         n, readfds, writefds, exceptfds,
         timeval2str(timeout_in_ptr), timeval2str(timeout),
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, select);
}         


unsigned int 
rpc_if_nametoindex(rcf_rpc_server *handle,
                   const char *ifname)
{
    tarpc_if_nametoindex_in  in;
    tarpc_if_nametoindex_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return 0;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    
    in.ifname.ifname_val = (char *)ifname;
    in.ifname.ifname_len = ifname == NULL ? 0 : strlen(ifname) + 1;

    rcf_rpc_call(handle, _if_nametoindex, &in, 
                 (xdrproc_t)xdr_tarpc_if_nametoindex_in,
                 &out, (xdrproc_t)xdr_tarpc_if_nametoindex_out);

    INFO("RPC (%s,%s): if_nametoindex(%s) -> %d (%s)",
         handle->ta, handle->name, 
         ifname == NULL ? "" : ifname,
         out.ifindex, errno_rpc2str(RPC_ERRNO(handle)));
                 
    RETVAL_VAL((int)out.ifindex, if_nametoindex);  
}                   

char * 
rpc_if_indextoname(rcf_rpc_server *handle,
                   unsigned int ifindex, char *ifname)
{
    tarpc_if_indextoname_in  in;
    tarpc_if_indextoname_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.ifindex = ifindex;
    in.ifname.ifname_val = ifname;
    in.ifname.ifname_len = ifname == NULL ? 0 : strlen(ifname) + 1;

    rcf_rpc_call(handle, _if_indextoname, &in, 
                 (xdrproc_t)xdr_tarpc_if_indextoname_in,
                 &out, (xdrproc_t)xdr_tarpc_if_indextoname_out);
                 
    if (RPC_CALL_OK)
    {
        if (ifname != NULL && out.ifname.ifname_val != NULL)
            memcpy(ifname, out.ifname.ifname_val, out.ifname.ifname_len);
    }

    INFO("RPC (%s,%s): if_indextoname(%d) -> %d (%s)",
         handle->ta, handle->name,
         ifindex, out.ifname.ifname_val != NULL ? ifname : "",
         errno_rpc2str(RPC_ERRNO(handle)));
                 
    RETVAL_PTR(out.ifname.ifname_val != NULL ? ifname : NULL,
               if_indextoname);
}                   

struct if_nameindex * 
rpc_if_nameindex(rcf_rpc_server *handle)
{
    tarpc_if_nameindex_in  in;
    tarpc_if_nameindex_out out;
    
    struct if_nameindex *res = NULL;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    rcf_rpc_call(handle, _if_nameindex, &in, 
                 (xdrproc_t)xdr_tarpc_if_nameindex_in,
                 &out, (xdrproc_t)xdr_tarpc_if_nameindex_out);
                 
    if (RPC_CALL_OK && out.ptr.ptr_val != NULL)
    {
        int i;
        
        if ((res = calloc(sizeof(*res) * out.ptr.ptr_len + 
                          sizeof(unsigned int), 1)) == NULL)
        {
            handle->_errno = TE_RC(TE_RCF, ENOMEM);
            RETVAL_PTR(NULL, if_nameindex);
        }
        
        *(unsigned int *)res = out.mem_ptr;
        res = (struct if_nameindex *)((unsigned int *)res + 1);
        
        for (i = 0; i < (int)out.ptr.ptr_len - 1; i++)
        {
            if ((res[i].if_name = 
                 strdup(out.ptr.ptr_val[i].ifname.ifname_val)) == NULL)
            {
                for (i--; i >= 0; i--)
                    free(res[i].if_name);
                
                res = (struct if_nameindex *)((unsigned int *)res - 1);
                free(res);
                handle->_errno = TE_RC(TE_RCF, ENOMEM);
                break;
            }
        }
    }

    INFO("RPC (%s,%s): if_nameindex() -> 0x%x (%s)",
         handle->ta, handle->name,
         res, errno_rpc2str(RPC_ERRNO(handle)));
    
    RETVAL_PTR(res, if_nameindex);
}

void 
rpc_if_freenameindex(rcf_rpc_server *handle,
                     struct if_nameindex *ptr)
{
    tarpc_if_freenameindex_in  in;
    tarpc_if_freenameindex_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    handle->op = RCF_RPC_CALL_WAIT;

    if (ptr == NULL)
        in.mem_ptr = 0;
    else
    {
        unsigned int *p = (unsigned int *)ptr - 1;
        in.mem_ptr = *p;
        free(p);
    }

    rcf_rpc_call(handle, _if_freenameindex, &in, 
                 (xdrproc_t)xdr_tarpc_if_freenameindex_in,
                 &out, (xdrproc_t)xdr_tarpc_if_freenameindex_out);

    INFO("RPC (%s,%s): if_freenameindex(0x%x) -> (%s)",
         handle->ta, handle->name, 
         ptr, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(if_freenameindex);
}                     

rpc_sigset_t *
rpc_sigset_new(rcf_rpc_server *handle)
{
    tarpc_sigset_new_in  in;
    tarpc_sigset_new_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    
    rcf_rpc_call(handle, _sigset_new, &in, (xdrproc_t)xdr_tarpc_sigset_new_in,
                 &out, (xdrproc_t)xdr_tarpc_sigset_new_out);

    INFO("RPC (%s,%s): sigset_new() -> 0x%x (%s)",
         handle->ta, handle->name,
         (unsigned int)(out.set), errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_PTR(out.set, sigset_new);
}

void 
rpc_sigset_delete(rcf_rpc_server *handle, rpc_sigset_t *set)
{
    tarpc_sigset_delete_in  in;
    tarpc_sigset_delete_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    
    rcf_rpc_call(handle, _sigset_delete, &in, (xdrproc_t)xdr_tarpc_sigset_delete_in,
                 &out, (xdrproc_t)xdr_tarpc_sigset_delete_out);

    INFO("RPC (%s,%s): sigset_delete(0x%x) -> (%s)",
         handle->ta, handle->name,
         (unsigned int)set, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(sigset_delete);
}


int 
rpc_sigprocmask(rcf_rpc_server *handle,
                rpc_sighow how, const rpc_sigset_t *set,
                rpc_sigset_t *oldset)
{
    tarpc_sigprocmask_in  in;
    tarpc_sigprocmask_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    in.oldset = (tarpc_sigset_t)oldset;
    in.how = how;
    
    rcf_rpc_call(handle, _sigprocmask, &in, (xdrproc_t)xdr_tarpc_sigprocmask_in,
                 &out, (xdrproc_t)xdr_tarpc_sigprocmask_out);
    INFO("RPC (%s,%s): sigprocmask(%d, 0x%x, 0x%x) -> %d (%s)",
         handle->ta, handle->name, how,
         (unsigned int)set, (unsigned int)oldset,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));
         
    RETVAL_RC(sigprocmask);
}                


int 
rpc_sigemptyset(rcf_rpc_server *handle, rpc_sigset_t *set)
{
    tarpc_sigemptyset_in  in;
    tarpc_sigemptyset_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    
    rcf_rpc_call(handle, _sigemptyset, &in, (xdrproc_t)xdr_tarpc_sigemptyset_in,
                 &out, (xdrproc_t)xdr_tarpc_sigemptyset_out);

    INFO("RPC (%s,%s): sigemptyset(0x%x) -> %d (%s)",
         handle->ta, handle->name,
         (unsigned int)set, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(sigemptyset);
}

int 
rpc_sigpending(rcf_rpc_server *handle, rpc_sigset_t *set)
{
    tarpc_sigpending_in  in;
    tarpc_sigpending_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    
    rcf_rpc_call(handle, _sigpending, &in, (xdrproc_t)xdr_tarpc_sigpending_in,
                 &out, (xdrproc_t)xdr_tarpc_sigpending_out);

    INFO("RPC (%s,%s): sigpending(0x%x) -> %d (%s)",
         handle->ta, handle->name,
         (unsigned int)set, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(sigpending);
}

int 
rpc_sigsuspend(rcf_rpc_server *handle, const rpc_sigset_t *set)
{
    tarpc_sigsuspend_in  in;
    tarpc_sigsuspend_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.set = (tarpc_sigset_t)set;
    
    rcf_rpc_call(handle, _sigsuspend, &in, (xdrproc_t)xdr_tarpc_sigsuspend_in,
                 &out, (xdrproc_t)xdr_tarpc_sigsuspend_out);

    INFO("RPC (%s,%s): sigsuspend(0x%x) -> %d (%s)",
         handle->ta, handle->name,
         (unsigned int)set, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(sigsuspend);
}

rpc_sigset_t *
rpc_sigreceived(rcf_rpc_server *handle)
{
    tarpc_sigreceived_in  in;
    tarpc_sigreceived_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    
    rcf_rpc_call(handle, _sigreceived, &in, (xdrproc_t)xdr_tarpc_sigreceived_in,
                 &out, (xdrproc_t)xdr_tarpc_sigreceived_out);

    INFO("RPC (%s,%s): sigreceived() -> 0x%x (%s)",
         handle->ta, handle->name,
         (unsigned int)(out.set), errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_PTR(out.set, sigreceived);
}

int 
rpc_sigfillset(rcf_rpc_server *handle, rpc_sigset_t *set)
{
    tarpc_sigfillset_in  in;
    tarpc_sigfillset_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    
    rcf_rpc_call(handle, _sigfillset, &in, (xdrproc_t)xdr_tarpc_sigfillset_in,
                 &out, (xdrproc_t)xdr_tarpc_sigfillset_out);

    INFO("RPC (%s,%s): sigfillset(0x%x) -> %d (%s)",
         handle->ta, handle->name,
         (unsigned int)set, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(sigfillset);
}

int 
rpc_sigaddset(rcf_rpc_server *handle, rpc_sigset_t *set, rpc_signum signum)
{
    tarpc_sigaddset_in  in;
    tarpc_sigaddset_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    in.signum = signum;
    
    rcf_rpc_call(handle, _sigaddset, &in, (xdrproc_t)xdr_tarpc_sigaddset_in,
                 &out, (xdrproc_t)xdr_tarpc_sigaddset_out);

    INFO("RPC (%s,%s): sigaddset(%s, 0x%x) -> %d (%s)",
         handle->ta, handle->name,
         signum_rpc2str(signum), (unsigned int)set,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(sigaddset);
}

int 
rpc_sigdelset(rcf_rpc_server *handle, rpc_sigset_t *set, rpc_signum signum)
{
    tarpc_sigdelset_in  in;
    tarpc_sigdelset_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    in.signum = signum;
    
    rcf_rpc_call(handle, _sigdelset, &in, (xdrproc_t)xdr_tarpc_sigdelset_in,
                 &out, (xdrproc_t)xdr_tarpc_sigdelset_out);

    INFO("RPC (%s,%s): sigdelset(%s, 0x%x) -> %d (%s)",
         handle->ta, handle->name,
         signum_rpc2str(signum), (unsigned int)set,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(sigdelset);
}

int 
rpc_sigismember(rcf_rpc_server *handle,
                const rpc_sigset_t *set, rpc_signum signum)
{
    tarpc_sigismember_in  in;
    tarpc_sigismember_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.set = (tarpc_sigset_t)set;
    in.signum = signum;
    
    rcf_rpc_call(handle, _sigismember,
                 &in, (xdrproc_t)xdr_tarpc_sigismember_in,
                 &out, (xdrproc_t)xdr_tarpc_sigismember_out);

    INFO("RPC (%s,%s): sigismember(%s, 0x%x) -> %d (%s)",
         handle->ta, handle->name,
         signum_rpc2str(signum), (unsigned int)set,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    LOG_TE_ERROR(sigismember);

    if (!RPC_CALL_OK)                     
        return -1;                                        
                                                          
    if (out.retval != 0 && out.retval != 1 && out.retval != -1)
    {                                                     
        ERROR("FD_ISSET returned incorrect value %d", out.retval);                            
        handle->_errno = TE_RC(TE_TAPI, ETECORRUPTED);    
        return -1;                                        
    }                                                     
    return out.retval;                                   
}
                
char *
rpc_signal(rcf_rpc_server *handle,
           rpc_signum signum, const char *handler)
{
    tarpc_signal_in  in;
    tarpc_signal_out out;
    
    char *res = NULL;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    in.signum = signum;
    in.handler.handler_val = (char *)handler;
    in.handler.handler_len = handler == NULL ? 0 : strlen(handler) + 1;
    
    rcf_rpc_call(handle, _signal,
                 &in, (xdrproc_t)xdr_tarpc_signal_in,
                 &out, (xdrproc_t)xdr_tarpc_signal_out);

    INFO("RPC (%s,%s): signal(%s, %s) -> %s (%s)",
         handle->ta, handle->name,
         signum_rpc2str(signum), (handler != NULL) ? handler : "(null)", 
         out.handler.handler_val != NULL ? out.handler.handler_val : "(null)", 
         errno_rpc2str(RPC_ERRNO(handle)));

    /* Yes, I know that it's memory leak, but what do you propose?! */
    if (RPC_CALL_OK && out.handler.handler_val != NULL)
        res = strdup(out.handler.handler_val);

    RETVAL_PTR(res, signal);        
}           

int 
rpc_kill(rcf_rpc_server *handle, pid_t pid, rpc_signum signum)
{
    rcf_rpc_op     op = handle->op;
    tarpc_kill_in  in;
    tarpc_kill_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.signum = signum;
    in.pid = pid;
    
    rcf_rpc_call(handle, _kill, &in, (xdrproc_t)xdr_tarpc_kill_in,
                 &out, (xdrproc_t)xdr_tarpc_kill_out);

    INFO("RPC (%s,%s)%s: kill(%d, %s) -> (%s)",
         handle->ta, handle->name, rpcop2str(op),
         pid, signum_rpc2str(signum), errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(kill);
}

int 
rpc_getsockopt_gen(rcf_rpc_server *handle,
                   int s, rpc_socklevel level, rpc_sockopt optname,
                   void *optval, socklen_t *optlen, socklen_t roptlen)
{
    tarpc_getsockopt_in   in;
    tarpc_getsockopt_out  out;
    struct option_value   val;
    char                  opt_val_str[4096] = {};
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.s = s;
    in.level = level;
    in.optname = optname;
    if (optlen != NULL)
    {
        in.optlen.optlen_len = 1;
        in.optlen.optlen_val = optlen;
    }
    if (optval != NULL)
    {
        memset(&val, 0, sizeof(val));

        in.optval.optval_len = 1;
        in.optval.optval_val = &val;

        switch (optname)
        {
            case RPC_SO_ACCEPTFILTER:
            case RPC_SO_BINDTODEVICE:
                val.opttype = OPT_STRING;
                val.option_value_u.opt_string.opt_string_len = roptlen;
                val.option_value_u.opt_string.opt_string_val = (char *)optval;
                break;

            case RPC_SO_LINGER:
                val.opttype = OPT_LINGER;
                if (roptlen >= sizeof(struct linger))
                {
                    val.option_value_u.opt_linger.l_onoff = 
                        ((struct linger *)optval)->l_onoff;
                    val.option_value_u.opt_linger.l_linger = 
                        ((struct linger *)optval)->l_linger;
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(struct linger)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(struct linger));
                }
                break;

            case RPC_SO_RCVTIMEO:
            case RPC_SO_SNDTIMEO:
                val.opttype = OPT_TIMEVAL;
                if (roptlen >= sizeof(struct timeval))
                {
                    val.option_value_u.opt_timeval.tv_sec =
                        ((struct timeval *)optval)->tv_sec;
                    val.option_value_u.opt_timeval.tv_usec =
                        ((struct timeval *)optval)->tv_usec;
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(struct timeval)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(struct timeval));
                }
                break;

            case RPC_IP_ADD_MEMBERSHIP: 
            case RPC_IP_DROP_MEMBERSHIP:
                val.opttype = OPT_MREQN;
                if (roptlen >= sizeof(struct ip_mreqn))
                {
                    memcpy(val.option_value_u.opt_mreqn.imr_multiaddr,
                           &(((struct ip_mreqn *)optval)->imr_multiaddr),
                           sizeof(((struct ip_mreqn *)optval)->imr_multiaddr));
                    memcpy(val.option_value_u.opt_mreqn.imr_address,
                           &(((struct ip_mreqn *)optval)->imr_address),
                           sizeof(((struct ip_mreqn *)optval)->imr_address));
                    val.option_value_u.opt_mreqn.imr_ifindex = 
                          ((struct ip_mreqn *)optval)->imr_ifindex;
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(struct ip_mreqn)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(struct ip_mreqn));
                }
                break;

            case RPC_IP_MULTICAST_IF:
                val.opttype = OPT_IPADDR;
                if (roptlen >= sizeof(struct in_addr))
                {
                    memcpy(val.option_value_u.opt_ipaddr,
                           optval, sizeof(struct in_addr));
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(struct in_addr)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(struct in_addr));
                }
                break;
            
            case RPC_TCP_INFO:
                val.opttype = OPT_TCP_INFO;
                if (roptlen >= sizeof(struct tcp_info))
                {
#define COPY_TCP_INFO_FIELD(_name) \
                    val.option_value_u.opt_tcp_info._name = \
                        ((struct tcp_info *)optval)->_name
                    
                    COPY_TCP_INFO_FIELD(tcpi_state);
                    COPY_TCP_INFO_FIELD(tcpi_ca_state);
                    COPY_TCP_INFO_FIELD(tcpi_retransmits);
                    COPY_TCP_INFO_FIELD(tcpi_probes);
                    COPY_TCP_INFO_FIELD(tcpi_backoff);
                    COPY_TCP_INFO_FIELD(tcpi_options);
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
#undef COPY_TCP_INFO_FIELD
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(struct tcp_info)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(struct tcp_info));
                }
                break;
                
            default:
                val.opttype = OPT_INT;
                if (roptlen >= sizeof(int))
                {
                    val.option_value_u.opt_int = *(int *)optval;
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(int)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(int));
                }
                break;
        }
    }

    rcf_rpc_call(handle, _getsockopt, &in, (xdrproc_t)xdr_tarpc_getsockopt_in,
                 &out, (xdrproc_t)xdr_tarpc_getsockopt_out);
                 
    if (RPC_CALL_OK)
    {
        if (optlen != NULL)
            *optlen = out.optlen.optlen_val[0];
        if (optval != NULL)
        {
            switch (optname)
            {
                case RPC_SO_ACCEPTFILTER:
                case RPC_SO_BINDTODEVICE:
                {
                    if (optlen != NULL)
                    {
                        char *buf;
                        
                        if ((buf = (char *)malloc(*optlen + 1)) == NULL)
                        {
                            ERROR("No memory available");
                            return TE_RC(TE_TAPI, ENOMEM);
                        }
                        buf[*optlen] = '\0';

                        /* FIXME Is it sufficient space in optval buffer */
                        memcpy(optval, out.optval.optval_val[0].
                               option_value_u.opt_string.opt_string_val,
                               *optlen);
                        memcpy(buf, optval, *optlen);
                        snprintf(opt_val_str, sizeof(opt_val_str), 
                                 "{ %s%s }",
                                 buf, (strlen(buf) == *optlen) ? 
                                 " without trailing zero" : "");

                        free(buf);
                    }
                    break;
                }

                case RPC_SO_LINGER:
                    if (roptlen >= sizeof(struct linger))
                    {
                        ((struct linger *)optval)->l_onoff =
                            out.optval.optval_val[0].option_value_u.
                                opt_linger.l_onoff; 
                        ((struct linger *)optval)->l_linger =
                           out.optval.optval_val[0].option_value_u.
                                opt_linger.l_linger;
                        snprintf(opt_val_str, sizeof(opt_val_str), 
                                 "{ l_onoff: %d, l_linger: %d }",
                                 ((struct linger *)optval)->l_onoff,
                                 ((struct linger *)optval)->l_linger);
                    }
                    break;

                case RPC_SO_RCVTIMEO:
                case RPC_SO_SNDTIMEO:
                    if (roptlen >= sizeof(struct timeval))
                    {
                        ((struct timeval *)optval)->tv_sec =
                            out.optval.optval_val[0].option_value_u.
                                opt_timeval.tv_sec;
                        ((struct timeval *)optval)->tv_usec =
                            out.optval.optval_val[0].option_value_u.
                                opt_timeval.tv_usec;
    
                        snprintf(opt_val_str, sizeof(opt_val_str), 
                                 "{ tv_sec: %ld, tv_usec: %ld }",
                                 ((struct timeval *)optval)->tv_sec,
                                 ((struct timeval *)optval)->tv_usec);
                    }
                    break;

                case RPC_IP_ADD_MEMBERSHIP: 
                case RPC_IP_DROP_MEMBERSHIP:
                    if (roptlen >= sizeof(struct ip_mreqn))
                    {
                        char addr_buf1[INET_ADDRSTRLEN];
                        char addr_buf2[INET_ADDRSTRLEN];
                    
                        memcpy(&(((struct ip_mreqn *)optval)->imr_multiaddr),
                               out.optval.optval_val[0].option_value_u.
                                   opt_mreqn.imr_multiaddr,
                               sizeof(((struct ip_mreqn *)optval)->
                                          imr_multiaddr));
                        memcpy(&(((struct ip_mreqn *)optval)->imr_address),
                               out.optval.optval_val[0].option_value_u.
                                   opt_mreqn.imr_address,
                               sizeof(((struct ip_mreqn *)optval)->
                                      imr_address));
                        ((struct ip_mreqn *)optval)->imr_ifindex =
                            val.option_value_u.opt_mreqn.imr_ifindex;

                        snprintf(opt_val_str, sizeof(opt_val_str), 
                                 "{ imr_multiaddr: %s, imr_address: %s, "
                                 "imr_ifindex: %d}",
                                 inet_ntop(AF_INET, 
                                     &(((struct ip_mreqn *)optval)->
                                         imr_multiaddr),
                                     addr_buf1, sizeof(addr_buf1)),
                                 inet_ntop(AF_INET,
                                     &(((struct ip_mreqn *)optval)->
                                         imr_address),
                                     addr_buf2, sizeof(addr_buf2)),
                                 ((struct ip_mreqn *)optval)->imr_ifindex);
                    }
                    break;
                
                case RPC_IP_MULTICAST_IF:
                    if (roptlen >= sizeof(struct in_addr))
                    {
                        char addr_buf[INET_ADDRSTRLEN];

                        memcpy(optval,
                               out.optval.optval_val[0].option_value_u.
                                   opt_ipaddr,
                               sizeof(struct in_addr));
                        snprintf(opt_val_str, sizeof(opt_val_str), 
                                 "{ addr: %s }",
                                 inet_ntop(AF_INET, optval,
                                           addr_buf, sizeof(addr_buf)));
                    }
                    break;
            
                case RPC_TCP_INFO:
                    if (roptlen >= sizeof(struct tcp_info))
                    {
#define COPY_TCP_INFO_FIELD(_name) \
                        do {                                                 \
                            ((struct tcp_info *)optval)->_name =             \
                                out.optval.optval_val[0].option_value_u.     \
                                opt_tcp_info._name;                          \
                            snprintf(opt_val_str + strlen(opt_val_str),      \
                                     sizeof(opt_val_str) -                   \
                                         strlen(opt_val_str),                \
                                     #_name ": %u ",                         \
                                     out.optval.optval_val[0].               \
                                         option_value_u.opt_tcp_info._name); \
                        } while (0)
                    
                    snprintf(opt_val_str, sizeof(opt_val_str), "{ ");
                    COPY_TCP_INFO_FIELD(tcpi_state);
                    COPY_TCP_INFO_FIELD(tcpi_ca_state);
                    COPY_TCP_INFO_FIELD(tcpi_retransmits);
                    COPY_TCP_INFO_FIELD(tcpi_probes);
                    COPY_TCP_INFO_FIELD(tcpi_backoff);
                    COPY_TCP_INFO_FIELD(tcpi_options);
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
                    snprintf(opt_val_str + strlen(opt_val_str),
                             sizeof(opt_val_str) - strlen(opt_val_str),
                             " }");
#undef COPY_TCP_INFO_FIELD
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(struct tcp_info)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(struct tcp_info));
                }
                break;

                default:
                    if (roptlen >= sizeof(int))
                    {
                        *(int *)optval = 
                            out.optval.optval_val[0].option_value_u.opt_int;
                        snprintf(opt_val_str, sizeof(opt_val_str), "%d",
                                 *(int *)optval);
                    }
                    break;
            }
        }
    }

    INFO("RPC (%s,%s): getsockopt(%d, %s, %s, 0x%x(%s), %d) -> %d (%s)",
         handle->ta, handle->name,
         s, socklevel_rpc2str(level), sockopt_rpc2str(optname), optval, 
         opt_val_str, optlen == NULL ? 0 : *optlen, out.retval,
         errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(getsockopt);
}                   

int 
rpc_setsockopt(rcf_rpc_server *handle,
               int s, rpc_socklevel level, rpc_sockopt optname,
               const void *optval, socklen_t optlen)
{
    tarpc_setsockopt_in  in;
    tarpc_setsockopt_out out;
    struct option_value  val;
    char                 opt_val_str[128] = {};
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out)); 

    in.s = s;
    in.level = level;
    in.optname = optname;
    in.optlen = optlen;
    handle->op = RCF_RPC_CALL_WAIT;
    
    if (optval != NULL)
    {
        in.optval.optval_len = 1;
        in.optval.optval_val = &val;
        switch (optname)
        {
            case RPC_SO_ACCEPTFILTER:
            case RPC_SO_BINDTODEVICE:
            {
                char *buf;

                val.option_value_u.opt_string.opt_string_len = optlen;
                val.option_value_u.opt_string.opt_string_val = (char *)optval;
                val.opttype = OPT_STRING;
                    
                if ((buf = (char *)malloc(optlen + 1)) == NULL)
                {
                    ERROR("No memory available");
                    return TE_RC(TE_TAPI, ENOMEM);
                }
                buf[optlen] = '\0';

                memcpy(buf, optval, optlen);
                snprintf(opt_val_str, sizeof(opt_val_str), 
                         "{ %s%s }",
                         buf, (strlen(buf) == optlen) ? 
                         " without trailing zero" : "");
                free(buf);
    
                break;
            }

            case RPC_SO_LINGER:
                val.option_value_u.opt_linger.l_onoff = 
                    ((struct linger *)optval)->l_onoff;
                val.option_value_u.opt_linger.l_linger = 
                    ((struct linger *)optval)->l_linger;
                val.opttype = OPT_LINGER;

                snprintf(opt_val_str, sizeof(opt_val_str), 
                         "{ l_onoff: %d, l_linger: %d }",
                         ((struct linger *)optval)->l_onoff,
                         ((struct linger *)optval)->l_linger);
                break;

            case RPC_SO_RCVTIMEO:
            case RPC_SO_SNDTIMEO:
                val.option_value_u.opt_timeval.tv_sec =
                    ((struct timeval *)optval)->tv_sec;
                val.option_value_u.opt_timeval.tv_usec =
                    ((struct timeval *)optval)->tv_usec;
                val.opttype = OPT_TIMEVAL;

                snprintf(opt_val_str, sizeof(opt_val_str), 
                         "{ tv_sec: %ld, tv_usec: %ld }",
                         ((struct timeval *)optval)->tv_sec,
                         ((struct timeval *)optval)->tv_usec);
                break;

            case RPC_IP_ADD_MEMBERSHIP: 
            case RPC_IP_DROP_MEMBERSHIP:
            {
                char addr_buf1[INET_ADDRSTRLEN];
                char addr_buf2[INET_ADDRSTRLEN];

                memcpy(val.option_value_u.opt_mreqn.imr_multiaddr,
                       (char *)&(((struct ip_mreqn *)optval)->imr_multiaddr),
                       sizeof(((struct ip_mreqn *)optval)->imr_multiaddr));
                memcpy(val.option_value_u.opt_mreqn.imr_address,
                       (char *)&(((struct ip_mreqn *)optval)->imr_address),
                       sizeof(((struct ip_mreqn *)optval)->imr_address));
                val.option_value_u.opt_mreqn.imr_ifindex = 
                       ((struct ip_mreqn *)optval)->imr_ifindex;
                val.opttype = OPT_MREQN;
                
                snprintf(opt_val_str, sizeof(opt_val_str), 
                         "{ imr_multiaddr: %s, imr_address: %s, "
                         "imr_ifindex: %d}",
                         inet_ntop(AF_INET, 
                                   (char *)&(((struct ip_mreqn *)optval)->imr_multiaddr),
                                   addr_buf1, sizeof(addr_buf1)),
                         inet_ntop(AF_INET,
                                   (char *)&(((struct ip_mreqn *)optval)->imr_address),
                                   addr_buf2, sizeof(addr_buf2)),
                         ((struct ip_mreqn *)optval)->imr_ifindex);
                break;
            }

            case RPC_IP_MULTICAST_IF:
            {
                char addr_buf[INET_ADDRSTRLEN];
                
                memcpy(&(val.option_value_u.opt_ipaddr), optval,
                       sizeof(struct in_addr));
                val.opttype = OPT_IPADDR;

                snprintf(opt_val_str, sizeof(opt_val_str), 
                         "{ addr: %s } ",
                         inet_ntop(AF_INET, (char *)optval,
                                   addr_buf, sizeof(addr_buf)));
                
                break;
            }
            
            default:
                val.option_value_u.opt_int = *(int *)optval;
                val.opttype = OPT_INT;

                snprintf(opt_val_str, sizeof(opt_val_str), "%d",
                         *(int *)optval);
                break;
        }
    }

    rcf_rpc_call(handle, _setsockopt, &in, (xdrproc_t)xdr_tarpc_setsockopt_in,
                 &out, (xdrproc_t)xdr_tarpc_setsockopt_out);

    INFO("RPC (%s,%s): setsockopt(%d, %s, %s, 0x%x(%s), %d) -> %d (%s)",
         handle->ta, handle->name,
         s, socklevel_rpc2str(level), sockopt_rpc2str(optname), optval,
         opt_val_str, optlen, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_RC(setsockopt);
}               

int 
rpc_pselect(rcf_rpc_server *handle,
            int n, rpc_fd_set *readfds, rpc_fd_set *writefds,
            rpc_fd_set *exceptfds, struct timespec *timeout,
            const rpc_sigset_t *sigmask)
{
    rcf_rpc_op        op = handle->op;
    tarpc_pselect_in  in;
    tarpc_pselect_out out;
    
    struct tarpc_timespec tv;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    in.n = n;
    in.readfds = (tarpc_fd_set)readfds;
    in.writefds = (tarpc_fd_set)writefds;
    in.exceptfds = (tarpc_fd_set)exceptfds;
    in.sigmask = (tarpc_sigset_t)sigmask;
    
    if (timeout != NULL && handle->op != RCF_RPC_WAIT)
    {
        tv.tv_sec = timeout->tv_sec;
        tv.tv_nsec = timeout->tv_nsec;
        in.timeout.timeout_len = 1;
        in.timeout.timeout_val = &tv;
    }
    
    rcf_rpc_call(handle, _pselect, &in, (xdrproc_t)xdr_tarpc_pselect_in,
                 &out, (xdrproc_t)xdr_tarpc_pselect_out);

    INFO("RPC (%s,%s)%s: pselect(%d, 0x%x, 0x%x, 0x%x, %s, 0x%x) "
         "-> %d (%s)", handle->ta, handle->name,
         rpcop2str(op), n, readfds, writefds, exceptfds,
         timespec2str(timeout), sigmask,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, pselect);
}

int 
rpc_ioctl(rcf_rpc_server *handle,
          int fd, rpc_ioctl_code request, ...)
{
    tarpc_ioctl_in  in;
    tarpc_ioctl_out out;
    
    ioctl_request   req;
    va_list         ap;
    int             *arg;
    const char      *req_val;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    handle->op = RCF_RPC_CALL_WAIT;
    
    va_start(ap, request);
    arg = va_arg(ap, int *);
    va_end(ap);
    
    in.s = fd;
    in.code = request;
    
    if (arg != NULL)
    {
        memset(&req, 0, sizeof(req));
        in.req.req_val = &req;
        in.req.req_len = 1;
    }
    switch (request)
    {
        case RPC_SIOCGSTAMP:
            in.access = IOCTL_RD;
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_TIMEVAL;
                in.req.req_val[0].ioctl_request_u.req_timeval.tv_sec = 
                    ((struct timeval *)arg)->tv_sec;
                in.req.req_val[0].ioctl_request_u.req_timeval.tv_usec = 
                    ((struct timeval *)arg)->tv_usec;
            }
            break;

        case RPC_FIONBIO:
        case RPC_SIOCSPGRP:
        case RPC_FIOASYNC:
            in.access = IOCTL_WR;
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_INT;
                in.req.req_val[0].ioctl_request_u.req_int = *arg;
            }
            break;

        case RPC_FIONREAD:
        case RPC_SIOCATMARK:
        case RPC_SIOCINQ:
        case RPC_SIOCGPGRP:
        case RPC_SIOUNKNOWN:
            in.access = IOCTL_RD;
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_INT;
                in.req.req_val[0].ioctl_request_u.req_int = *arg;
            }
            break;
        
        case RPC_SIOCGIFCONF:
            in.access = IOCTL_RD;
	    if (arg != NULL)
            {
	        in.req.req_val[0].type = IOCTL_IFCONF;
                in.req.req_val[0].ioctl_request_u.req_ifconf.buflen =
                    ((struct ifconf *)arg)->ifc_len;
            }
            break;
        
        case RPC_SIOCSIFADDR:
        case RPC_SIOCSIFNETMASK:
        case RPC_SIOCSIFBRDADDR: 
        case RPC_SIOCSIFDSTADDR: 
                in.access = IOCTL_WR;
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_IFREQ;
                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_addr.
                    sa_family =
                        addr_family_h2rpc(
                            ((struct ifreq *)arg)->ifr_addr.sa_family);
                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_addr.
                    sa_data.sa_data_val = 
                        ((struct ifreq *)arg)->ifr_addr.sa_data;
                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_addr.
                    sa_data.sa_data_len = sizeof(struct sockaddr);
                memcpy(in.req.req_val[0].ioctl_request_u.req_ifreq.
                           rpc_ifr_name,
                       ((struct ifreq *)arg)->ifr_name,
                       sizeof(((struct ifreq *)arg)->ifr_name));
            }
            break;
                                    
        case RPC_SIOCGIFADDR:
        case RPC_SIOCGIFNETMASK:
        case RPC_SIOCGIFBRDADDR: 
        case RPC_SIOCGIFDSTADDR: 
        case RPC_SIOCGIFHWADDR:
        case RPC_SIOCGIFFLAGS:
        case RPC_SIOCGIFMTU:
            in.access = IOCTL_RD;
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_IFREQ;
                memcpy(in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_name,
                       ((struct ifreq *)arg)->ifr_name,
                       sizeof(((struct ifreq *)arg)->ifr_name));

                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_addr.
                    sa_data.sa_data_val = 
                        ((struct ifreq *)arg)->ifr_addr.sa_data;
                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_addr.
                    sa_data.sa_data_len = sizeof(struct sockaddr) -
                                          SA_COMMON_LEN;
            }
            break;            
            
        case RPC_SIOCSIFFLAGS:
            in.access = IOCTL_WR;
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_IFREQ;
                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_flags = 
                    if_fl_h2rpc((uint32_t)(unsigned short int)((struct ifreq *)arg)->ifr_flags);
                memcpy(in.req.req_val[0].ioctl_request_u.req_ifreq.
                           rpc_ifr_name,
                       ((struct ifreq *)arg)->ifr_name,
                       sizeof(((struct ifreq *)arg)->ifr_name));
            }
            break;

        case RPC_SIOCSIFMTU:
            in.access = IOCTL_WR;            
            if (arg != NULL)
            {
                in.req.req_val[0].type = IOCTL_IFREQ;
                in.req.req_val[0].ioctl_request_u.req_ifreq.rpc_ifr_mtu =
                    ((struct ifreq *)arg)->ifr_mtu;
                memcpy(in.req.req_val[0].ioctl_request_u.req_ifreq.
                           rpc_ifr_name,
                       ((struct ifreq *)arg)->ifr_name,
                       sizeof(((struct ifreq *)arg)->ifr_name));
            }
            break;
        
        default:
            ERROR("Unsupported ioctl code: %d", request);
            handle->_errno = TE_RC(TE_RCF, EOPNOTSUPP);
            return -1;
    }

    rcf_rpc_call(handle, _ioctl, &in, (xdrproc_t)xdr_tarpc_ioctl_in,
                 &out, (xdrproc_t)xdr_tarpc_ioctl_out);
    if (out.retval == 0 && out.req.req_val != NULL && in.access == IOCTL_RD)
    {
        switch (in.req.req_val[0].type)
        {
            case IOCTL_INT:
                *((int *)arg) = out.req.req_val[0].ioctl_request_u.req_int;
                break;
                
            case IOCTL_TIMEVAL:
                ((struct timeval *)arg)->tv_sec =
                    out.req.req_val[0].ioctl_request_u.req_timeval.tv_sec;
                ((struct timeval *)arg)->tv_usec =
                    out.req.req_val[0].ioctl_request_u.req_timeval.tv_usec;
                break;
                
            case IOCTL_IFREQ:
                switch (request)
                {
                    case RPC_SIOCGIFADDR:
                    case RPC_SIOCGIFNETMASK:
                    case RPC_SIOCGIFBRDADDR:
                    case RPC_SIOCGIFDSTADDR:
                    case RPC_SIOCGIFHWADDR:
                        ((struct ifreq *)arg)->ifr_addr.sa_family =
                        addr_family_rpc2h(out.req.req_val[0].ioctl_request_u.               
                        req_ifreq.rpc_ifr_addr.sa_family);

                        memcpy(((struct ifreq *)arg)->ifr_addr.sa_data,
                               out.req.req_val[0].ioctl_request_u.
                               req_ifreq.rpc_ifr_addr.sa_data.sa_data_val,
                               out.req.req_val[0].ioctl_request_u.
                               req_ifreq.rpc_ifr_addr.sa_data.sa_data_len);               
                                            
                        break;
                        
                    case RPC_SIOCGIFMTU:
                        ((struct ifreq *)arg)->ifr_mtu = out.req.req_val[0].
                            ioctl_request_u.req_ifreq.rpc_ifr_mtu;
                        break;
                        
                    case RPC_SIOCGIFFLAGS:
                        ((struct ifreq *)arg)->ifr_flags = 
                            if_fl_rpc2h((uint32_t)(unsigned short int)
                                out.req.req_val[0].ioctl_request_u.
                                    req_ifreq.rpc_ifr_flags);
                        break;
                        
                    default:
                        break;                   
                }
                break;
                
            case IOCTL_IFCONF:
            {
                struct tarpc_ifreq *rpc_req =
                    out.req.req_val[0].ioctl_request_u.req_ifconf.
                    rpc_ifc_req.rpc_ifc_req_val;
                        
                struct ifreq *req = ((struct ifconf *)arg)->ifc_req;
                uint32_t n = ((struct ifconf *)arg)->ifc_len / sizeof(*req);
                uint32_t i;
                int      max_addrlen = sizeof(req->ifr_addr) - SA_COMMON_LEN;
                    
                ((struct ifconf *)arg)->ifc_len = 
                    out.req.req_val[0].ioctl_request_u.req_ifconf.buflen;
                    
                if (req == NULL)
                   break;
                    
                if (n > ((struct ifconf *)arg)->ifc_len / sizeof(*req))
                    n = ((struct ifconf *)arg)->ifc_len / sizeof(*req);
                    
                for (i = 0; i < n; i++, req++, rpc_req++)
                {
                    strcpy(req->ifr_name, rpc_req->rpc_ifr_name);
                    req->ifr_addr.sa_family = 
                        addr_family_rpc2h(rpc_req->rpc_ifr_addr.sa_family);
                        
                    if (rpc_req->rpc_ifr_addr.sa_data.sa_data_val != NULL)
                    {
                        int copy_len = rpc_req->rpc_ifr_addr.sa_data.
                                       sa_data_len;
                                       
                        if (copy_len > max_addrlen)
                            copy_len = max_addrlen;
                                       
                        memcpy(req->ifr_addr.sa_data,
                               rpc_req->rpc_ifr_addr.sa_data.sa_data_val,
                               copy_len);
                    }
                }
                break;
            }
        }
    }
    
    switch (in.req.req_val[0].type)
    {
        case IOCTL_TIMEVAL:
            req_val = timeval2str((struct timeval *)arg);
            break;

        case IOCTL_INT:
        {
            static char int_buf[128];

            snprintf(int_buf, sizeof(int_buf), "%d", *((int *)arg));
            req_val = int_buf;
            break;
        }

        case IOCTL_IFREQ:
        {
            static char ifreq_buf[1024];

            snprintf(ifreq_buf, sizeof(ifreq_buf),
                     " interface %s: ", ((struct ifreq *)arg)->ifr_name);
            req_val = ifreq_buf;

            switch (request)
            {
                case RPC_SIOCGIFADDR:
                case RPC_SIOCSIFADDR:
                case RPC_SIOCGIFNETMASK:
                case RPC_SIOCSIFNETMASK:
                case RPC_SIOCGIFBRDADDR:
                case RPC_SIOCSIFBRDADDR:
                case RPC_SIOCGIFDSTADDR:
                case RPC_SIOCSIFDSTADDR:
                    snprintf(ifreq_buf + strlen(ifreq_buf),
                             sizeof(ifreq_buf) - strlen(ifreq_buf),
                             "%s: %s ",
                             ((request == RPC_SIOCGIFADDR) ? "addr" :
                              (request == RPC_SIOCGIFNETMASK) ? "netmask" :
                              (request == RPC_SIOCGIFBRDADDR) ? "braddr" :
                              (request == RPC_SIOCGIFDSTADDR) ? "dstaddr" : 
                              ""),
                              inet_ntoa(SIN(&(((struct ifreq *)arg)->
                                            ifr_addr))->sin_addr));
                    break;

                case RPC_SIOCGIFHWADDR:
                    snprintf(ifreq_buf + strlen(ifreq_buf),
                             sizeof(ifreq_buf) - strlen(ifreq_buf),
                             "hwaddr: %02x:%02x:%02x:%02x:%02x:%02x ",
                             (unsigned char)((struct ifreq *)arg)->ifr_hwaddr.sa_data[0],
                             (unsigned char)((struct ifreq *)arg)->ifr_hwaddr.sa_data[1],
                             (unsigned char)((struct ifreq *)arg)->ifr_hwaddr.sa_data[2],
                             (unsigned char)((struct ifreq *)arg)->ifr_hwaddr.sa_data[3],
                             (unsigned char)((struct ifreq *)arg)->ifr_hwaddr.sa_data[4],
                             (unsigned char)((struct ifreq *)arg)->ifr_hwaddr.sa_data[5]);
                    break;
                
                case RPC_SIOCGIFMTU:
                case RPC_SIOCSIFMTU:
                    snprintf(ifreq_buf + strlen(ifreq_buf),
                             sizeof(ifreq_buf) - strlen(ifreq_buf),
                             "mtu: %d ",
                             ((struct ifreq *)arg)->ifr_mtu);
                    break;

                case RPC_SIOCGIFFLAGS:
                case RPC_SIOCSIFFLAGS:
                    snprintf(ifreq_buf + strlen(ifreq_buf),
                             sizeof(ifreq_buf) - strlen(ifreq_buf),
                             "flags: %s ",
                             if_fl_rpc2str(if_fl_h2rpc(
                                 (uint32_t)(unsigned short int)
                                     ((struct ifreq *)arg)->ifr_flags)));
                    break;

                default:
                    req_val = " unknown request ";
            }
            break;
        }
        
        default:
            req_val = "";
            break;
    }

    INFO("RPC (%s,%s): ioctl(%d, %s, 0x%x(%s)) -> %d (%s)",
         handle->ta, handle->name, fd, ioctl_rpc2str(request),
         arg, req_val, out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, ioctl);
}

ssize_t 
rpc_sendmsg(rcf_rpc_server *handle,
            int s, const struct rpc_msghdr *msg, rpc_send_recv_flags flags)
{
    rcf_rpc_op        op = handle->op;
    tarpc_sendmsg_in  in;
    tarpc_sendmsg_out out;
    
    struct tarpc_msghdr rpc_msg;

    struct tarpc_iovec iovec_arr[RCF_RPC_MAX_IOVEC];
    
    size_t i;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(&rpc_msg, 0, sizeof(rpc_msg));
    in.s = s;
    in.flags = flags;
    
    if (msg != NULL && handle->op != RCF_RPC_WAIT)
    {    
        in.msg.msg_val = &rpc_msg;
        in.msg.msg_len = 1;
        
        if (msg->msg_riovlen > RCF_RPC_MAX_IOVEC)
        {
            handle->_errno = TE_RC(TE_RCF, ENOMEM);
            ERROR("Length of the I/O vector is too long (%u) - "
                  "increase RCF_RPC_MAX_IOVEC(%u)",
                  msg->msg_riovlen, RCF_RPC_MAX_IOVEC);
            return -1;
        }
        
        if (((msg->msg_iov != NULL) &&
             (msg->msg_iovlen > msg->msg_riovlen)) ||
            ((msg->msg_name != NULL) &&
             (msg->msg_namelen > msg->msg_rnamelen)) ||
            ((msg->msg_control != NULL) &&
             (msg->msg_controllen > msg->msg_rcontrollen)))
        {
            ERROR("Inconsistent real and declared lengths of buffers");
            handle->_errno = TE_RC(TE_RCF, EINVAL);
            return -1;
        }

        for (i = 0; i < msg->msg_riovlen && msg->msg_iov != NULL; i++)
        {
            iovec_arr[i].iov_base.iov_base_val = msg->msg_iov[i].iov_base;
            iovec_arr[i].iov_base.iov_base_len = msg->msg_iov[i].iov_rlen;
            iovec_arr[i].iov_len = msg->msg_iov[i].iov_len;
        }
    
        if (msg->msg_iov != NULL)
        {
            rpc_msg.msg_iov.msg_iov_val = iovec_arr;
            rpc_msg.msg_iov.msg_iov_len = msg->msg_riovlen;
        }
        rpc_msg.msg_iovlen = msg->msg_iovlen;
        
        if (msg->msg_name != NULL)
        {
            rpc_msg.msg_name.sa_data.sa_data_len = 
                msg->msg_rnamelen > SA_COMMON_LEN ?
                msg->msg_rnamelen - SA_COMMON_LEN : 0;
            rpc_msg.msg_name.sa_data.sa_data_val = 
                ((struct sockaddr *)(msg->msg_name))->sa_data;
            rpc_msg.msg_name.sa_family = addr_family_h2rpc(
                ((struct sockaddr *)(msg->msg_name))->sa_family);
        } 
        rpc_msg.msg_namelen = msg->msg_namelen;
        rpc_msg.msg_flags = (int)msg->msg_flags;
        
        if (msg->msg_control != NULL)
        {
            rpc_msg.msg_control.msg_control_val = msg->msg_control;
            rpc_msg.msg_control.msg_control_len = msg->msg_rcontrollen;
        }
        rpc_msg.msg_controllen = msg->msg_controllen;
    }
    
    rcf_rpc_call(handle, _sendmsg, &in, (xdrproc_t)xdr_tarpc_sendmsg_in,
                 &out, (xdrproc_t)xdr_tarpc_sendmsg_out);

    INFO("RPC (%s,%s)%s: sendmsg(%d, 0x%x "
                                    "(msg_name: 0x%x, msg_namelen: %d,"
                                    " msg_iov: 0x%x, msg_iovlen: %d,"
                                    " msg_control: 0x%x, msg_controllen: %d,"
                                    " msg_flags: %s)"
         ", %s) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op), 
         s, msg,
         msg->msg_name, msg->msg_namelen,
         msg->msg_iov, msg->msg_iovlen,
         msg->msg_control, msg->msg_controllen,
         send_recv_flags_rpc2str(msg->msg_flags),
         send_recv_flags_rpc2str(flags),
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));
    
    RETVAL_VAL(out.retval, sendmsg);
}            

ssize_t 
rpc_recvmsg(rcf_rpc_server *handle,
            int s, struct rpc_msghdr *msg, rpc_send_recv_flags flags)
{
    char              str_buf[1024];
    rcf_rpc_op        op = handle->op;
    tarpc_recvmsg_in  in;
    tarpc_recvmsg_out out;
    
    struct tarpc_msghdr rpc_msg;

    struct tarpc_iovec iovec_arr[RCF_RPC_MAX_IOVEC];
    
    size_t i;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(&rpc_msg, 0, sizeof(rpc_msg));
    in.s = s;
    in.flags = flags;
    
    if (msg != NULL && handle->op != RCF_RPC_WAIT)
    {    
        in.msg.msg_val = &rpc_msg;
        in.msg.msg_len = 1;
        
        if (msg->msg_riovlen > RCF_RPC_MAX_IOVEC)
        {
            handle->_errno = TE_RC(TE_RCF, ENOMEM);
            ERROR("Length of the I/O vector is too long (%u) - "
                  "increase RCF_RPC_MAX_IOVEC(%u)",
                  msg->msg_riovlen, RCF_RPC_MAX_IOVEC);
            return -1;
        }
        
        if (msg->msg_iovlen > msg->msg_riovlen ||
            msg->msg_namelen > msg->msg_rnamelen ||
            msg->msg_controllen > msg->msg_rcontrollen)
        {
            handle->_errno = TE_RC(TE_RCF, EINVAL);
            return -1;
        }

        for (i = 0; i < msg->msg_riovlen && msg->msg_iov != NULL; i++)
        {
            iovec_arr[i].iov_base.iov_base_val = msg->msg_iov[i].iov_base;
            iovec_arr[i].iov_base.iov_base_len = msg->msg_iov[i].iov_rlen;
            iovec_arr[i].iov_len = msg->msg_iov[i].iov_len;
        }
    
        if (msg->msg_iov != NULL)
        {
            rpc_msg.msg_iov.msg_iov_val = iovec_arr;
            rpc_msg.msg_iov.msg_iov_len = msg->msg_riovlen;
        }
        rpc_msg.msg_iovlen = msg->msg_iovlen;
        
        if (msg->msg_name != NULL)
        {
            rpc_msg.msg_name.sa_data.sa_data_len = 
                msg->msg_rnamelen > SA_COMMON_LEN ?
                msg->msg_rnamelen - SA_COMMON_LEN : 0;
            rpc_msg.msg_name.sa_data.sa_data_val = 
                ((struct sockaddr *)(msg->msg_name))->sa_data;
            rpc_msg.msg_name.sa_family = addr_family_h2rpc(
                ((struct sockaddr *)(msg->msg_name))->sa_family);
        } 
        rpc_msg.msg_namelen = msg->msg_namelen;
        rpc_msg.msg_flags = msg->msg_flags;
        
        if (msg->msg_control != NULL)
        {
            rpc_msg.msg_control.msg_control_val = msg->msg_control;
            rpc_msg.msg_control.msg_control_len = msg->msg_rcontrollen;
        }
        rpc_msg.msg_controllen = msg->msg_controllen;
    }
    
    rcf_rpc_call(handle, _recvmsg, &in, (xdrproc_t)xdr_tarpc_recvmsg_in,
                 &out, (xdrproc_t)xdr_tarpc_recvmsg_out);

    snprintf(str_buf, sizeof(str_buf), "RPC (%s, %s)%s: recvmsg(%d, 0x%x(",
             handle->ta, handle->name, rpcop2str(op),
             s, (unsigned int)msg);

    if (RPC_CALL_OK && msg != NULL && out.msg.msg_val != NULL)
    {
        rpc_msg = out.msg.msg_val[0];

        if (msg->msg_name != NULL)
        {
            ((struct sockaddr *)(msg->msg_name))->sa_family =
                addr_family_rpc2h(rpc_msg.msg_name.sa_family);
            memcpy(((struct sockaddr *)(msg->msg_name))->sa_data,
                   rpc_msg.msg_name.sa_data.sa_data_val, 
                   rpc_msg.msg_name.sa_data.sa_data_len);
        }
        msg->msg_namelen = rpc_msg.msg_namelen;
        
        for (i = 0; i < msg->msg_riovlen && msg->msg_iov != NULL; i++)
        {
            msg->msg_iov[i].iov_len = rpc_msg.msg_iov.msg_iov_val[i].iov_len;
            memcpy(msg->msg_iov[i].iov_base, 
                   rpc_msg.msg_iov.msg_iov_val[i].iov_base.iov_base_val, 
                   msg->msg_iov[i].iov_rlen);
        }
        if (msg->msg_control != NULL)
        {
            memcpy(msg->msg_control, rpc_msg.msg_control.msg_control_val, 
                   msg->msg_rcontrollen);
        }
        msg->msg_controllen = rpc_msg.msg_controllen;

        msg->msg_flags = (rpc_send_recv_flags)rpc_msg.msg_flags;
        
        snprintf(str_buf + strlen(str_buf),
                 sizeof(str_buf) - strlen(str_buf),
                 "msg_name: 0x%x, msg_namelen: %d, "
                 "msg_iov: 0x%x, msg_iovlen: %d, "
                 "msg_control: 0x%x, msg_controllen: %d, msg_flags: %s",
                 (unsigned int)(msg->msg_name), msg->msg_namelen,
                 (unsigned int)(msg->msg_iov), msg->msg_iovlen,
                 (unsigned int)(msg->msg_control), msg->msg_controllen,
                 send_recv_flags_rpc2str(msg->msg_flags));
    }

    snprintf(str_buf + strlen(str_buf), sizeof(str_buf) - strlen(str_buf),
             "), %s) -> %d (%s)",
             send_recv_flags_rpc2str(flags),
             out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    INFO("%s", str_buf);
    
    RETVAL_VAL(out.retval, recvmsg);
}            

/**
 * Convert poll() request to string.
 *
 * @param ufds      Array with requests per fd
 * @param ufds      Number of requests per fd
 * @param buf       Buffer to put string
 * @param buflen    Length of the buffer
 */
static void
pollreq2str(struct rpc_pollfd *ufds, unsigned int nfds,
            char *buf, size_t buflen)
{
    unsigned int    i;
    int             rc;

    do {
        rc = snprintf(buf, buflen, "{");
        if ((size_t)rc > buflen)
            break;
        buflen -= rc;
        buf += rc;
        for (i = 0; i < nfds; ++i)
        {
            rc = snprintf(buf, buflen, "{%d,%s,%s}",
                          ufds->fd, 
                          poll_event_rpc2str(ufds->events),
                          poll_event_rpc2str(ufds->revents));
            if ((size_t)rc > buflen)
                break;
            buflen -= rc;
            buf += rc;
        }
        rc = snprintf(buf, buflen, "}");
        if ((size_t)rc > buflen)
            break;
        buflen -= rc;
        buf += rc;

        return;

    } while (0);
        
    ERROR("Too small buffer for poll request conversion");
}

int 
rpc_poll_gen(rcf_rpc_server *handle,
             struct rpc_pollfd *ufds, unsigned int nfds,
             int timeout, unsigned int rnfds)
{
    rcf_rpc_op       op = handle->op;
    tarpc_poll_in  in;
    tarpc_poll_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    
    /**
     * @attention It's assumed that rpc_pollfd is the same as tarpc_pollfd.
     * It's OK, because both structures are independent from particular 
     * Socket API and used on the same host. 
     */
    in.ufds.ufds_len = rnfds;
    in.ufds.ufds_val = (struct tarpc_pollfd *)ufds;
    in.timeout = timeout;
    in.nfds = nfds;

    pollreq2str(ufds, rnfds, str_buf_1, sizeof(str_buf_1));

    rcf_rpc_call(handle, _poll, &in, (xdrproc_t)xdr_tarpc_poll_in,
                 &out, (xdrproc_t)xdr_tarpc_poll_out);

    if (RPC_CALL_OK && ufds != NULL && out.ufds.ufds_val != NULL)
    {
        memcpy(ufds, out.ufds.ufds_val, rnfds * sizeof(ufds[0]));
    }

    pollreq2str(ufds, rnfds, str_buf_2, sizeof(str_buf_2));

    INFO("RPC (%s,%s)%s: poll(0x%x%s, %d, %d) -> %d (%s) %s",
         handle->ta, handle->name, rpcop2str(op),
         ufds, str_buf_1, nfds, timeout,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)), str_buf_2);

    RETVAL_VAL(out.retval, poll);
}

/**
 * Convert hostent received via RPC to the host struct hostent.
 * Memory allocated by RPC library is partially used and rpc_he is
 * updated properly to avoid releasing of re-used memory.
 *
 * @param rpc_he        pointer to the field of RPC out structure
 *
 * @return pointer to struct hostent or NULL is memory allocation failed
 */
static struct hostent *
hostent_rpc2h(tarpc_hostent *rpc_he)
{
    struct hostent *he;
    
    if ((he = calloc(1, sizeof(*he))) == NULL)
        return NULL;
    
    if (rpc_he->h_aliases.h_aliases_val != NULL &&
        (he->h_aliases = calloc(rpc_he->h_aliases.h_aliases_len, 
                                sizeof(char *))) == NULL)
    {
        free(he);
        return NULL;
    }
    
    if (rpc_he->h_addr_list.h_addr_list_val != NULL &&
        (he->h_addr_list = calloc(rpc_he->h_addr_list.h_addr_list_len, 
                                  sizeof(char *))) == NULL)
    {
        free(he->h_aliases);
        free(he);
    }
        
    he->h_name = rpc_he->h_name.h_name_val;
    rpc_he->h_name.h_name_val = NULL;
    rpc_he->h_name.h_name_len = 0;
    
    if (he->h_aliases != NULL)
    {
        unsigned int i;
        
        for (i = 0; i < rpc_he->h_aliases.h_aliases_len; i++)
        {
            he->h_aliases[i] = rpc_he->h_aliases.h_aliases_val[i].name.name_val;
            rpc_he->h_aliases.h_aliases_val[i].name.name_val = NULL;
            rpc_he->h_aliases.h_aliases_val[i].name.name_len = 0;
        }
    }

    if (he->h_addr_list != NULL)
    {
        unsigned int i;
        
        for (i = 0; i < rpc_he->h_addr_list.h_addr_list_len; i++)
        {
            he->h_addr_list[i] = 
                rpc_he->h_addr_list.h_addr_list_val[i].val.val_val;
            rpc_he->h_addr_list.h_addr_list_val[i].val.val_val = NULL;
            rpc_he->h_addr_list.h_addr_list_val[i].val.val_len = 0;
        }
    }
    
    he->h_length = rpc_he->h_length;
    he->h_addrtype = domain_rpc2h(rpc_he->h_addrtype);
    
    return he;
}

struct hostent *
rpc_gethostbyname(rcf_rpc_server *handle, const char *name)
{
    tarpc_gethostbyname_in  in;
    tarpc_gethostbyname_out out;
    
    struct hostent *res = NULL;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.name.name_val = (char *)name;
    in.name.name_len = name == NULL ? 0 : strlen(name) + 1;

    rcf_rpc_call(handle, _gethostbyname, &in, 
                 (xdrproc_t)xdr_tarpc_gethostbyname_in,
                 &out, (xdrproc_t)xdr_tarpc_gethostbyname_out);
                 
    if (RPC_CALL_OK && out.res.res_val != NULL)
    {
        if ((res = hostent_rpc2h(out.res.res_val)) == NULL)
            handle->_errno = TE_RC(TE_RCF, ENOMEM);
    }
        
    INFO("RPC (%s,%s): gethostbyname(%s) -> 0x%x (%s)",
         handle->ta, handle->name,
         name == NULL ? "" : name, res, errno_rpc2str(RPC_ERRNO(handle)));
    
    RETVAL_PTR(res, gethostbyname);
}

struct hostent *
rpc_gethostbyaddr(rcf_rpc_server *handle,
                  const char *addr, int len, rpc_socket_addr_family type)
{
    tarpc_gethostbyaddr_in  in;
    tarpc_gethostbyaddr_out out;
    
    struct hostent *res = NULL;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.type = type;
    
    if (addr != NULL)
    {
        in.addr.val.val_val = (char *)addr;
        in.addr.val.val_len = len;
    }

    rcf_rpc_call(handle, _gethostbyaddr, &in, 
                 (xdrproc_t)xdr_tarpc_gethostbyaddr_in,
                 &out, (xdrproc_t)xdr_tarpc_gethostbyaddr_out);
                 
    if (RPC_CALL_OK && out.res.res_val != NULL)
    {
        if ((res = hostent_rpc2h(out.res.res_val)) == NULL)
            handle->_errno = TE_RC(TE_RCF, ENOMEM);
    }
        
    INFO("RPC (%s,%s): gethostbyaddr(0x%x, %d, %d) -> 0x%x (%s)",
         handle->ta, handle->name,
         addr, len, type, res, errno_rpc2str(RPC_ERRNO(handle)));
    
    RETVAL_PTR(res, gethostbyaddr);
}                           

/** 
 * Convert RPC addrinfo to the host native one.
 *
 * @param rpc_ai        RPC addrinfo structure
 * @param ai            pre-allocated host addrinfo structure
 *
 * @return 0 in the case of success or -1 in the case of memory allocation
 * failure
 */
static int
ai_rpc2h(struct tarpc_ai *ai_rpc, struct addrinfo *ai)
{
    ai->ai_flags = ai_flags_rpc2h(ai_rpc->flags);
    ai->ai_family = domain_rpc2h(ai_rpc->family);
    ai->ai_socktype = socktype_rpc2h(ai_rpc->socktype);
    ai->ai_protocol = proto_rpc2h(ai_rpc->protocol);
    ai->ai_addrlen = ai_rpc->addrlen + SA_COMMON_LEN;
    
    if (ai_rpc->addr.sa_data.sa_data_val != NULL)
    {
        if ((ai->ai_addr = calloc(1, ai_rpc->addr.sa_data.sa_data_len + 
                                  SA_COMMON_LEN)) == NULL)
        {
            return -1;
        }
        ai->ai_addr->sa_family = addr_family_rpc2h(ai_rpc->addr.sa_family);
        memcpy(ai->ai_addr->sa_data, ai_rpc->addr.sa_data.sa_data_val, 
               ai_rpc->addr.sa_data.sa_data_len);
    }
    
    if (ai_rpc->canonname.canonname_val != NULL)
    {
        ai->ai_canonname = ai_rpc->canonname.canonname_val;
        ai_rpc->canonname.canonname_val = NULL;
        ai_rpc->canonname.canonname_len = 0;
    }
    
    return 0;
}


int 
rpc_getaddrinfo(rcf_rpc_server *handle,
                const char *node, const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res)
{
    tarpc_getaddrinfo_in  in;
    tarpc_getaddrinfo_out out;
    
    struct addrinfo *list = NULL;
    struct tarpc_ai  rpc_hints;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (node != NULL)
    {
        in.node.node_val = (char *)node;
        in.node.node_len = strlen(node) + 1;
    }

    if (service != NULL)
    {
        in.service.service_val = (char *)service;
        in.service.service_len = strlen(service) + 1;
    }
    
    if (hints != NULL)
    {
        rpc_hints.flags = ai_flags_h2rpc(hints->ai_flags);
        rpc_hints.family = addr_family_h2rpc(hints->ai_family);
        rpc_hints.socktype = socktype_h2rpc(hints->ai_socktype);
        rpc_hints.protocol = proto_h2rpc(hints->ai_protocol);
        rpc_hints.addrlen = hints->ai_addrlen - SA_COMMON_LEN;
    
        if (hints->ai_addr != NULL)
        {
            rpc_hints.addr.sa_family =
                addr_family_h2rpc(hints->ai_addr->sa_family);
            rpc_hints.addr.sa_data.sa_data_val = hints->ai_addr->sa_data;
            rpc_hints.addr.sa_data.sa_data_len = 
                hints->ai_addrlen > SA_COMMON_LEN ?
                hints->ai_addrlen - SA_COMMON_LEN : 0;
        }
    
        if (hints->ai_canonname != NULL)
        {
            rpc_hints.canonname.canonname_val = hints->ai_canonname;
            rpc_hints.canonname.canonname_len = strlen(hints->ai_canonname) + 1;
        }
    
        in.hints.hints_val = &rpc_hints;
        in.hints.hints_len = 1;
    }

    rcf_rpc_call(handle, _getaddrinfo, &in, 
                 (xdrproc_t)xdr_tarpc_getaddrinfo_in,
                 &out, (xdrproc_t)xdr_tarpc_getaddrinfo_out);
                 
    if (RPC_CALL_OK && out.res.res_val != NULL)
    {
        int i;
        
        if ((list = calloc(1, out.res.res_len * sizeof(*list) + 
                           sizeof(int))) == NULL)
        {
            handle->_errno = TE_RC(TE_RCF, ENOMEM);
            RETVAL_RC(getaddrinfo);
        }
        *(int *)list = out.mem_ptr;
        list = (struct addrinfo *)((int *)list + 1);
        
        for (i = 0; i < (int)out.res.res_len; i++)
        {
            if (ai_rpc2h(out.res.res_val, list + i) < 0)
            {
                for (i--; i >= 0; i--)
                {
                    free(list[i].ai_addr);
                    free(list[i].ai_canonname);
                }
                free((int *)list - 1);
                handle->_errno = TE_RC(TE_RCF, ENOMEM);
                RETVAL_RC(getaddrinfo);
            }
            list[i].ai_next = (i == (int)out.res.res_len - 1) ? NULL 
                                                              : list + i + 1;
        }
        *res = list;
    }
        
    INFO("RPC (%s,%s): getaddrinfo(%s, %s, 0x%x, 0x%x) -> %d (%s)",
         handle->ta, handle->name,
         node, service, hints, res, out.retval,
         errno_rpc2str(RPC_ERRNO(handle)));
    
    RETVAL_RC(getaddrinfo);
}                

void 
rpc_freeaddrinfo(rcf_rpc_server *handle,
                 struct addrinfo *res)
{
    tarpc_freeaddrinfo_in  in;
    tarpc_freeaddrinfo_out out;
    
    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    if (res != NULL)
        in.mem_ptr = *((int *)res - 1);
    
    rcf_rpc_call(handle, _freeaddrinfo, &in, (xdrproc_t)xdr_tarpc_freeaddrinfo_in,
                 &out, (xdrproc_t)xdr_tarpc_freeaddrinfo_out);

    INFO("RPC (%s,%s): freeaddrinfo(0x%x) -> (%s)",
         handle->ta, handle->name,
         (unsigned int)res, errno_rpc2str(RPC_ERRNO(handle)));
         
    free((int *)res - 1); 

    LOG_TE_ERROR(freeaddrinfo); 
}                 

int 
rpc_pipe(rcf_rpc_server *handle,
         int filedes[2])
{
    tarpc_pipe_in  in;
    tarpc_pipe_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    rcf_rpc_call(handle, _pipe, &in, (xdrproc_t)xdr_tarpc_pipe_in,
                 &out, (xdrproc_t)xdr_tarpc_pipe_out);

    INFO("RPC (%s,%s): pipe() -> %d %d %d (%s)",
         handle->ta, handle->name,
         out.retval, out.filedes[0], out.filedes[1],
         errno_rpc2str(RPC_ERRNO(handle)));

    if (RPC_CALL_OK && filedes != NULL)
        memcpy(filedes, out.filedes, sizeof(out.filedes));

    RETVAL_RC(pipe);
}     
                
int 
rpc_socketpair(rcf_rpc_server *handle, 
               rpc_socket_domain domain, rpc_socket_type type,
               rpc_socket_proto protocol, int sv[2])
{
    tarpc_socketpair_in  in;
    tarpc_socketpair_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.domain = domain;
    in.type = type;
    in.proto = protocol;

    rcf_rpc_call(handle, _socketpair, &in, (xdrproc_t)xdr_tarpc_socketpair_in,
                 &out, (xdrproc_t)xdr_tarpc_socketpair_out);

    INFO("RPC (%s,%s): socketpair(%s, %s, %s) -> %d %d %d (%s)",
         handle->ta, handle->name,
         domain_rpc2str(domain), socktype_rpc2str(type),
         proto_rpc2str(protocol), out.retval, 
         out.sv[0], out.sv[1], errno_rpc2str(RPC_ERRNO(handle))); 

    if (RPC_CALL_OK && sv != NULL)
        memcpy(sv, out.sv, sizeof(out.sv));

    RETVAL_RC(socketpair);
}           

FILE *
rpc_fopen(rcf_rpc_server *handle, 
          const char *path, const char *mode)
{
    tarpc_fopen_in  in;
    tarpc_fopen_out out;

    if (handle == NULL || path == NULL || mode == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return NULL;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.path.path_len = strlen(path) + 1;
    in.path.path_val = (char *)path;
    in.mode.mode_len = strlen(mode) + 1;
    in.mode.mode_val = (char *)mode;

    rcf_rpc_call(handle, _fopen, &in, (xdrproc_t)xdr_tarpc_fopen_in,
                 &out, (xdrproc_t)xdr_tarpc_fopen_out);

    INFO("RPC (%s,%s): fopen(%s, %s) -> 0x%x (%s)",
         handle->ta, handle->name,
         path, mode, out.mem_ptr, errno_rpc2str(RPC_ERRNO(handle))); 

    RETVAL_PTR(out.mem_ptr, fopen);
}      

int 
rpc_fileno(rcf_rpc_server *handle, 
           FILE *f)
{
    tarpc_fileno_in  in;
    tarpc_fileno_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.mem_ptr = (unsigned int)f;

    rcf_rpc_call(handle, _fileno, &in, (xdrproc_t)xdr_tarpc_fileno_in,
                 &out, (xdrproc_t)xdr_tarpc_fileno_out);

    INFO("RPC (%s,%s): fileno(0x%x) -> %d (%s)",
         handle->ta, handle->name,
         f, out.fd, errno_rpc2str(RPC_ERRNO(handle))); 

    RETVAL_VAL(out.fd, fileno);
}       

uid_t 
rpc_getuid(rcf_rpc_server *handle)
{
    tarpc_getuid_in  in;
    tarpc_getuid_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    rcf_rpc_call(handle, _getuid, &in, (xdrproc_t)xdr_tarpc_getuid_in,
                 &out, (xdrproc_t)xdr_tarpc_getuid_out);

    INFO("RPC (%s,%s): getuid() -> %d (%s)",
         handle->ta, handle->name,
         out.uid, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL((int)out.uid, getuid);
}

int
rpc_setuid(rcf_rpc_server *handle, 
           uid_t uid)
{
    tarpc_setuid_in  in;
    tarpc_setuid_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.uid = uid;

    rcf_rpc_call(handle, _setuid, &in, (xdrproc_t)xdr_tarpc_setuid_in,
                 &out, (xdrproc_t)xdr_tarpc_setuid_out);

    INFO("RPC (%s,%s): setuid(%d) -> %d (%s)",
         handle->ta, handle->name,
         uid, out.retval, errno_rpc2str(RPC_ERRNO(handle))); 

    RETVAL_RC(setuid);
}       

uid_t 
rpc_geteuid(rcf_rpc_server *handle)
{
    tarpc_geteuid_in  in;
    tarpc_geteuid_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;

    rcf_rpc_call(handle, _geteuid, &in, (xdrproc_t)xdr_tarpc_geteuid_in,
                 &out, (xdrproc_t)xdr_tarpc_geteuid_out);

    INFO("RPC (%s,%s): geteuid() -> %d (%s)",
         handle->ta, handle->name,
         out.uid, errno_rpc2str(RPC_ERRNO(handle))); 

    RETVAL_VAL((int)out.uid, geteuid);
}

int
rpc_seteuid(rcf_rpc_server *handle, 
            uid_t uid)
{
    tarpc_seteuid_in  in;
    tarpc_seteuid_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    handle->op = RCF_RPC_CALL_WAIT;
    in.uid = uid;

    rcf_rpc_call(handle, _seteuid, &in, (xdrproc_t)xdr_tarpc_seteuid_in,
                 &out, (xdrproc_t)xdr_tarpc_seteuid_out);

    INFO("RPC (%s,%s): seteuid() -> %d (%s)",
         handle->ta, handle->name,
         uid, out.retval, errno_rpc2str(RPC_ERRNO(handle))); 

    RETVAL_RC(seteuid);
}        

/**
 * Simple sender. 
 *
 * @param handle            RPC server
 * @param s                 a socket to be user for sending
 * @param size_min          minimum size of the message in bytes
 * @param size_max          maximum size of the message in bytes
 * @param size_rnd_once     if true, random size should be calculated
 *                          only once and used for all messages;
 *                          if false, random size is calculated for
 *                          each message
 * @param delay_min         minimum delay between messages in microseconds
 * @param delay_max         maximum delay between messages in microseconds
 * @param delay_rnd_once    if true, random delay should be calculated
 *                          only once and used for all messages;
 *                          if false, random delay is calculated for
 *                          each message
 * @param time2run          how long run (in seconds)
 * @param sent              location for number of sent bytes
 *
 * @return number of sent bytes or -1 in the case of failure
 */
int 
rpc_simple_sender(rcf_rpc_server *handle,
                  int s, int size_min, int size_max, 
                  int size_rnd_once, int delay_min, int delay_max,
                  int delay_rnd_once, int time2run, uint64_t *sent)
{
    tarpc_simple_sender_in  in;
    tarpc_simple_sender_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.s = s;
    in.size_min = size_min;
    in.size_max = size_max;
    in.size_rnd_once = size_rnd_once;
    in.delay_min = delay_min;
    in.delay_max = delay_max;
    in.delay_rnd_once = delay_rnd_once;
    in.time2run = time2run;

    rcf_rpc_call(handle, _simple_sender, &in, 
                 (xdrproc_t)xdr_tarpc_simple_sender_in,
                 &out, (xdrproc_t)xdr_tarpc_simple_sender_out);

    if (out.retval == 0)
        *sent = ((uint64_t)(out.bytes_high) << 32) + out.bytes_low;
        
    INFO("RPC (%s,%s): "
         "simple_sender(%d, %d, %d, %d, %d, %d, %d, %d) -> %d %u (%s)",
         handle->ta, handle->name,
         s, size_min, size_max, size_rnd_once, delay_min, delay_max,
         delay_rnd_once, time2run,
         out.retval, (unsigned long)*sent, errno_rpc2str(RPC_ERRNO(handle))); 
 
    RETVAL_RC(simple_sender);
}                  

/**
 * Simple receiver. 
 *
 * @param handle            RPC server
 * @param s                 a socket to be user for receiving
 * @param received          location for number of received bytes
 *
 * @return number of received bytes or -1 in the case of failure
 */
int 
rpc_simple_receiver(rcf_rpc_server *handle, 
                    int s, uint64_t *received)
{
    tarpc_simple_receiver_in  in;
    tarpc_simple_receiver_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.s = s;

    rcf_rpc_call(handle, _simple_receiver, &in, 
                 (xdrproc_t)xdr_tarpc_simple_receiver_in,
                 &out, (xdrproc_t)xdr_tarpc_simple_receiver_out);

    if (out.retval == 0)
        *received = ((uint64_t)(out.bytes_high) << 32) + out.bytes_low;
        
    INFO("RPC (%s,%s): "
         "simple_receiver(%d) -> %d %u (%s)",
         handle->ta, handle->name,
         s, out.retval, (unsigned long)*received, 
         errno_rpc2str(RPC_ERRNO(handle))); 

    RETVAL_RC(simple_receiver);
}


/* See description in tapi_rpcsock.h */
int
rpc_iomux_flooder(rcf_rpc_server *handle,
                  int *sndrs, int sndnum, int *rcvrs, int rcvnum, 
                  int bulkszs, int time2run, int iomux, te_bool rx_nonblock,
                  unsigned long *tx_stat, unsigned long *rx_stat)
{
    rcf_rpc_op        op = handle->op;
    tarpc_flooder_in  in;
    tarpc_flooder_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (sndrs != NULL)
    {
        in.sndrs.sndrs_val = sndrs;
        in.sndrs.sndrs_len = sndnum;
    }
    if (rcvrs != NULL)
    {
        in.rcvrs.rcvrs_val = rcvrs;
        in.rcvrs.rcvrs_len = rcvnum;
    }
    in.bulkszs = bulkszs;
    in.time2run = time2run;
    in.iomux = iomux;
    in.rx_nonblock = rx_nonblock;
    if (tx_stat != NULL)
    {
        in.tx_stat.tx_stat_val = tx_stat;
        in.tx_stat.tx_stat_len = sndnum;
    }
    if (rx_stat != NULL)
    {
        in.rx_stat.rx_stat_val = rx_stat;
        in.rx_stat.rx_stat_len = rcvnum;
    }

    rcf_rpc_call(handle, _flooder, &in, 
                 (xdrproc_t)xdr_tarpc_flooder_in,
                 &out, (xdrproc_t)xdr_tarpc_flooder_out);

    INFO("RPC (%s,%s)%s: "
         "flooder(0x%x, %d, 0x%x, %d, %d, %d, %d, 0x%x, 0x%x) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         rcvrs, rcvnum, sndrs, sndnum, bulkszs, time2run, iomux,
         tx_stat, rx_stat,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    if (RPC_CALL_OK)
    {
        if (tx_stat != NULL)
            memcpy(tx_stat, out.tx_stat.tx_stat_val,
                   out.tx_stat.tx_stat_len * sizeof(tx_stat[0]));
        if (rx_stat != NULL)
            memcpy(rx_stat, out.rx_stat.rx_stat_val,
                   out.rx_stat.rx_stat_len * sizeof(rx_stat[0]));
    }

    RETVAL_RC(flooder);
}

/* See description in tapi_rpcsock.h */
int
rpc_iomux_echoer(rcf_rpc_server *handle,
                 int *sockets, int socknum, int time2run, int iomux,
                 unsigned long *tx_stat, unsigned long *rx_stat)
{
    rcf_rpc_op       op = handle->op;
    tarpc_echoer_in  in;
    tarpc_echoer_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }
    if (sockets == NULL)
    {
        handle->_errno = RPC_EINVAL;
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.sockets.sockets_val = sockets;
    in.sockets.sockets_len = socknum;
    in.time2run = time2run;
    in.iomux = iomux;
    if (tx_stat != NULL)
    {
        in.tx_stat.tx_stat_val = tx_stat;
        in.tx_stat.tx_stat_len = socknum;
    }
    if (rx_stat != NULL)
    {
        in.rx_stat.rx_stat_val = rx_stat;
        in.rx_stat.rx_stat_len = socknum;
    }

    rcf_rpc_call(handle, _echoer, &in, 
                 (xdrproc_t)xdr_tarpc_echoer_in,
                 &out, (xdrproc_t)xdr_tarpc_echoer_out);

    INFO("RPC (%s,%s)%s: echoer(0x%x, %d, %d, %d) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         sockets, socknum, time2run, iomux,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    if (RPC_CALL_OK)
    {
        if (tx_stat != NULL)
            memcpy(tx_stat, out.tx_stat.tx_stat_val,
                   out.tx_stat.tx_stat_len * sizeof(tx_stat[0]));
        if (rx_stat != NULL)
            memcpy(rx_stat, out.rx_stat.rx_stat_val,
                   out.rx_stat.rx_stat_len * sizeof(rx_stat[0]));
    }

    RETVAL_RC(echoer);
}

/**
 * Asynchronous read test procedure.
 *
 * @param handle            RPC server
 * @param s                 a socket to be user for receiving
 * @param signum            signal to be used as notification event
 * @param timeout           timeout for blocking on select() in seconds
 * @param buf               buffer for received data
 * @param buflen            buffer length to be passed to the read
 * @param rlen              real buffer length
 * @param diag              location for diagnostic message
 * @param diag_len          maximum length of the diagnostic message
 *
 * @return number of received bytes or -1 in the case of failure
 */
int 
rpc_aio_read_test(rcf_rpc_server *handle,
                  int s, rpc_signum signum, int timeout,
                  char *buf, int buflen, int rlen,
                  char *diag, int diag_len)
{
    tarpc_aio_read_test_in  in;
    tarpc_aio_read_test_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.s = s;
    in.signum = signum;
    in.t = timeout;
    in.buf.buf_val = buf;
    in.buf.buf_len = rlen;
    in.buflen = buflen;
    in.diag.diag_val = diag;
    in.diag.diag_len = diag_len;

    rcf_rpc_call(handle, _aio_read_test, &in, 
                 (xdrproc_t)xdr_tarpc_aio_read_test_in,
                 &out, (xdrproc_t)xdr_tarpc_aio_read_test_out);

    INFO("RPC (%s,%s): "
         "aio_read_test(%d, %s, %d, %x, %d, %d) -> %d (%s)",
         handle->ta, handle->name,
         s, signum_rpc2str(signum), timeout, (unsigned int)buf, buflen, rlen,
         out.retval, errno_rpc2str(RPC_ERRNO(handle))); 

    if (RPC_CALL_OK)
    {
        if (buf != NULL && out.buf.buf_val != NULL)
            memcpy(buf, out.buf.buf_val, out.buf.buf_len);
            
        memcpy(diag, out.diag.diag_val, out.diag.diag_len);
    }

    RETVAL_VAL(out.retval, aio_read_test);
}                  

/**
 * Asynchronous error processing test procedure.
 *
 * @param handle            RPC server
 * @param diag              location for diagnostic message
 * @param diag_len          maximum length of the diagnostic message
 *
 * @return number of received bytes or -1 in the case of failure
 */
int 
rpc_aio_error_test(rcf_rpc_server *handle,
                   char *diag, int diag_len)
{
    tarpc_aio_error_test_in  in;
    tarpc_aio_error_test_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.diag.diag_val = diag;
    in.diag.diag_len = diag_len;

    rcf_rpc_call(handle, _aio_error_test, &in, 
                 (xdrproc_t)xdr_tarpc_aio_error_test_in,
                 &out, (xdrproc_t)xdr_tarpc_aio_error_test_out);

    INFO("RPC (%s,%s): "
         "aio_error_test() -> %d (%s)",
         handle->ta, handle->name,
         out.retval, errno_rpc2str(RPC_ERRNO(handle))); 

    if (RPC_CALL_OK)
        memcpy(diag, out.diag.diag_val, out.diag.diag_len);

    RETVAL_RC(aio_error_test);
}                   

/*
 * Asynchronous write test procedure.
 *
 * @param handle            RPC server
 * @param s                 a socket to be user for receiving
 * @param signum            signal to be used as notification event
 * @param buf               buffer for data to be sent
 * @param buflen            data length
 * @param diag              location for diagnostic message
 * @param diag_len          maximum length of the diagnostic message
 *
 * @return number of sent bytes or -1 in the case of failure
 */
int 
rpc_aio_write_test(rcf_rpc_server *handle,
                   int s, rpc_signum signum, 
                   char *buf, int buflen, 
                   char *diag, int diag_len)
{                   
    tarpc_aio_write_test_in  in;
    tarpc_aio_write_test_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.s = s;
    in.signum = signum;
    in.buf.buf_val = buf;
    in.buf.buf_len = buflen;
    in.diag.diag_val = diag;
    in.diag.diag_len = diag_len;

    rcf_rpc_call(handle, _aio_write_test, &in, 
                 (xdrproc_t)xdr_tarpc_aio_write_test_in,
                 &out, (xdrproc_t)xdr_tarpc_aio_write_test_out);

    INFO("RPC (%s,%s): "
         "aio_write_test(%d, %s, %x, %d) -> %d (%s)",
         handle->ta, handle->name,
         s, signum_rpc2str(signum), (unsigned int)buf, buflen, 
         out.retval, 
         errno_rpc2str(RPC_ERRNO(handle))); 

    if (RPC_CALL_OK)
        memcpy(diag, out.diag.diag_val, out.diag.diag_len);

    RETVAL_VAL(out.retval, aio_write_test);
}
                   
/**
 * Susending on asynchronous events test procedure.
 *
 * @param handle            RPC server
 * @param s                 a socket to be user for receiving
 * @param s_aux             dummy socket
 * @param signum            signal to be used as notification event
 * @param timeout           timeout for blocking on ai_suspend() in seconds
 * @param buf               buffer for received data
 * @param buflen            buffer length to be passed to the read
 * @param diag              location for diagnostic message
 * @param diag_len          maximum length of the diagnostic message
 *
 * @return number of received bytes or -1 in the case of failure
 */
int 
rpc_aio_suspend_test(rcf_rpc_server *handle,
                     int s, int s_aux, rpc_signum signum, 
                     int timeout, char *buf, int buflen, 
                     char *diag, int diag_len)
{                     
    tarpc_aio_suspend_test_in  in;
    tarpc_aio_suspend_test_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.s = s;
    in.s_aux = s_aux;
    in.signum = signum;
    in.t = timeout;
    in.buf.buf_val = buf;
    in.buf.buf_len = buflen;
    in.diag.diag_val = diag;
    in.diag.diag_len = diag_len;

    rcf_rpc_call(handle, _aio_suspend_test, &in, 
                 (xdrproc_t)xdr_tarpc_aio_suspend_test_in,
                 &out, (xdrproc_t)xdr_tarpc_aio_suspend_test_out);

    INFO("RPC (%s,%s): "
         "aio_suspend_test(%d, %d, %s, %d, %x, %d, %d) -> %d (%s)",
         handle->ta, handle->name,
         s, s_aux, signum_rpc2str(signum), timeout, (unsigned int)buf, buflen, 
         out.retval, errno_rpc2str(RPC_ERRNO(handle))); 

    if (RPC_CALL_OK)
    {
        if (buf != NULL && out.buf.buf_val != NULL)
            memcpy(buf, out.buf.buf_val, out.buf.buf_len);
            
        memcpy(diag, out.diag.diag_val, out.diag.diag_len);
    }

    RETVAL_VAL(out.retval, aio_suspend_test);
}                    


ssize_t
rpc_sendfile(rcf_rpc_server *handle, int out_fd, int in_fd,
             off_t *offset, size_t count)
{
    rcf_rpc_op         op = handle->op;
    tarpc_sendfile_in  in;
    tarpc_sendfile_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.out_fd = out_fd;
    in.in_fd = in_fd;
    in.count = count;
    if (offset != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.offset.offset_len = 1;
        in.offset.offset_val = offset;
    }

    rcf_rpc_call(handle, _sendfile, &in, (xdrproc_t)xdr_tarpc_sendfile_in,
                 &out, (xdrproc_t)xdr_tarpc_sendfile_out);
    if (RPC_CALL_OK)
    {
        if (offset != NULL && out.offset.offset_val != NULL)
            *offset = out.offset.offset_val[0];
    }

    INFO("RPC (%s,%s)%s: sendfile(%d, %d, 0x%x, %u) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         out_fd, in_fd, *offset, count,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, sendfile);
}


/* See description in tapi_rpcsock.h */
ssize_t
rpc_socket_to_file(rcf_rpc_server *handle, int sock,
                   const char *path, long timeout)
{
    rcf_rpc_op               op = handle->op;
    tarpc_socket_to_file_in  in;
    tarpc_socket_to_file_out out;

    if (handle == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        return -1;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.sock = sock;
    in.timeout = timeout;
    if (path != NULL && handle->op != RCF_RPC_WAIT)
    {
        in.path.path_len = strlen(path);
        in.path.path_val = (char *)path;
    }

    rcf_rpc_call(handle, _socket_to_file,
                 &in, (xdrproc_t)xdr_tarpc_socket_to_file_in,
                 &out, (xdrproc_t)xdr_tarpc_socket_to_file_out);

    INFO("RPC (%s,%s)%s: socket_to_file(%d, %s, %u) -> %d (%s)",
         handle->ta, handle->name, rpcop2str(op),
         sock, path, timeout,
         out.retval, errno_rpc2str(RPC_ERRNO(handle)));

    RETVAL_VAL(out.retval, socket_to_file);
}

