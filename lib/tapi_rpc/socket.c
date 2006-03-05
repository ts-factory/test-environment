/** @file
 * @brief Test API for RPC
 *
 * TAPI for remote calls of Socket API
 *
 *
 * Copyright (C) 2005 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 * Copyright (c) 2005 Level5 Networks Corp.
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
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id$
 */

#include "te_config.h"

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
#else
#define IFNAMSIZ        16
#endif


#include "te_printf.h"
#include "tapi_rpc_internal.h"
#include "tapi_rpc_unistd.h"
#include "tapi_rpc_socket.h"
#include "tapi_rpcsock_macros.h"

int
rpc_socket(rcf_rpc_server *rpcs,
           rpc_socket_domain domain, rpc_socket_type type,
           rpc_socket_proto protocol)
{
    tarpc_socket_in  in;
    tarpc_socket_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(socket, -1);
    }

    rpcs->op = RCF_RPC_CALL_WAIT;
    in.domain = domain;
    in.type = type;
    in.proto = protocol;

    rcf_rpc_call(rpcs, "socket", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(socket, out.fd);

    TAPI_RPC_LOG("RPC (%s,%s): socket(%s, %s, %s) -> %d (%s)",
                 rpcs->ta, rpcs->name,
                 domain_rpc2str(domain), socktype_rpc2str(type),
                 proto_rpc2str(protocol),
                 out.fd, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(socket, out.fd);
}

int
rpc_bind(rcf_rpc_server *rpcs,
         int s, const struct sockaddr *my_addr, socklen_t addrlen)
{
    tarpc_bind_in  in;
    tarpc_bind_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(bind, -1);
    }

    rpcs->op = RCF_RPC_CALL_WAIT;

    in.fd = s;
    if (my_addr != NULL)
    {
        if (addrlen >= SA_COMMON_LEN)
        {
            in.addr.sa_family = addr_family_h2rpc(my_addr->sa_family);
            in.addr.sa_data.sa_data_len = addrlen - SA_COMMON_LEN;
            in.addr.sa_data.sa_data_val = (uint8_t *)(my_addr->sa_data);
        }
        else
        {
            in.addr.sa_family = RPC_AF_UNSPEC;
            in.addr.sa_data.sa_data_len = 0;
            /* Any no-NULL pointer is suitable here */
            in.addr.sa_data.sa_data_val = (uint8_t *)my_addr;
        }
    }
    in.len = addrlen;

    rcf_rpc_call(rpcs, "bind", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(bind, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s): bind(%d, %s, %u) -> %d (%s)",
                 rpcs->ta, rpcs->name,
                 s, sockaddr2str(my_addr), addrlen,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(bind, out.retval);
}

int
rpc_connect(rcf_rpc_server *rpcs,
            int s, const struct sockaddr *addr, socklen_t addrlen)
{
    rcf_rpc_op        op;
    tarpc_connect_in  in;
    tarpc_connect_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(connect, -1);
    }

    op = rpcs->op;
    in.fd = s;
    if (addr != NULL)
    {
        if (addrlen >= SA_COMMON_LEN)
        {
            in.addr.sa_family = addr_family_h2rpc(addr->sa_family);
            in.addr.sa_data.sa_data_len = addrlen - SA_COMMON_LEN;
            in.addr.sa_data.sa_data_val = (uint8_t *)(addr->sa_data);
        }
        else
        {
            in.addr.sa_family = RPC_AF_UNSPEC;
            in.addr.sa_data.sa_data_len = 0;
            /* Any not-NULL pointer is suitable here */
            in.addr.sa_data.sa_data_val = (uint8_t *)addr;
        }
    }
    in.len = addrlen;

    rcf_rpc_call(rpcs, "connect", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(connect, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: connect(%d, %s, %u) -> %d (%s)",
                 rpcs->ta, rpcs->name, rpcop2str(op),
                 s, sockaddr2str(addr), addrlen,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(connect, out.retval);
}

int
rpc_listen(rcf_rpc_server *rpcs, int fd, int backlog)
{
    tarpc_listen_in  in;
    tarpc_listen_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(listen, -1);
    }

    rpcs->op = RCF_RPC_CALL_WAIT;
    in.fd = fd;
    in.backlog = backlog;

    rcf_rpc_call(rpcs, "listen", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(listen, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s): listen(%d, %d) -> %d (%s)",
                 rpcs->ta, rpcs->name, fd, backlog,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(listen, out.retval);
}

int
rpc_accept_gen(rcf_rpc_server *rpcs,
               int s, struct sockaddr *addr, socklen_t *addrlen,
               socklen_t raddrlen)
{
    rcf_rpc_op       op;
    socklen_t        save_addrlen =
                         (addrlen == NULL) ? (socklen_t)-1 : *addrlen;
    tarpc_accept_in  in;
    tarpc_accept_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(accept, -1);
    }

    op = rpcs->op;
    if (addrlen != NULL && *addrlen > raddrlen)
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
        RETVAL_INT(accept, -1);
    }

    in.fd = s;
    if (addrlen != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.len.len_len = 1;
        in.len.len_val = addrlen;
    }
    if (addr != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        if (raddrlen >= SA_COMMON_LEN)
        {
            in.addr.sa_family = addr_family_h2rpc(addr->sa_family);
            in.addr.sa_data.sa_data_len = raddrlen - SA_COMMON_LEN;
            in.addr.sa_data.sa_data_val = addr->sa_data;
        }
        else
        {
            in.addr.sa_family = RPC_AF_UNSPEC;
            in.addr.sa_data.sa_data_len = 0;
            /* Any not-NULL pointer is suitable here */
            in.addr.sa_data.sa_data_val = (uint8_t *)addr;
        }
    }

    rcf_rpc_call(rpcs, "accept", &in, &out);

    if (RPC_IS_CALL_OK(rpcs))
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

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(accept, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: accept(%d, %p[%u], %p(%u)) -> %d (%s) "
                 "peer=%s addrlen=%u",
                 rpcs->ta, rpcs->name, rpcop2str(op),
                 s, addr, raddrlen, addrlen, save_addrlen,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)),
                 sockaddr2str(addr),
                 (addrlen == NULL) ? (socklen_t)-1 : *addrlen);

    RETVAL_INT(accept, out.retval);
}

ssize_t
rpc_recvfrom_gen(rcf_rpc_server *rpcs,
                 int s, void *buf, size_t len, rpc_send_recv_flags flags,
                 struct sockaddr *from, socklen_t *fromlen,
                 size_t rbuflen, socklen_t rfrombuflen)
{
    rcf_rpc_op         op;
    socklen_t          save_fromlen =
                           (fromlen == NULL) ? (socklen_t)-1 : *fromlen;

    tarpc_recvfrom_in  in;
    tarpc_recvfrom_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(recvfrom, -1);
    }

    op = rpcs->op;

    if ((fromlen != NULL && *fromlen > rfrombuflen) ||
        (buf != NULL && len > rbuflen))
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
        RETVAL_INT(recvfrom, -1);
    }

    in.fd = s;
    in.len = len;
    if (fromlen != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.fromlen.fromlen_len = 1;
        in.fromlen.fromlen_val = fromlen;
    }
    if (from != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        if (rfrombuflen >= SA_COMMON_LEN)
        {
            in.from.sa_family = addr_family_h2rpc(from->sa_family);
            in.from.sa_data.sa_data_len = rfrombuflen - SA_COMMON_LEN;
            in.from.sa_data.sa_data_val = from->sa_data;
        }
        else
        {
            in.from.sa_family = RPC_AF_UNSPEC;
            in.from.sa_data.sa_data_len = 0;
            /* Any not-NULL pointer is suitable here */
            in.from.sa_data.sa_data_val = (uint8_t *)from;
        }
    }
    if (buf != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = rbuflen;
        in.buf.buf_val = buf;
    }
    in.flags = flags;

    rcf_rpc_call(rpcs, "recvfrom", &in, &out);

    if (RPC_IS_CALL_OK(rpcs))
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

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(recvfrom, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: recvfrom(%d, %p[%u], %u, %s, %p[%u], %d) "
                 "-> %d (%s) from=%s fromlen=%d",
                 rpcs->ta, rpcs->name, rpcop2str(op),
                 s, buf, rbuflen, len, send_recv_flags_rpc2str(flags),
                 from, rfrombuflen, (int)save_fromlen,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)),
                 sockaddr2str(from),
                 (fromlen == NULL) ? -1 : (int)*fromlen);

    RETVAL_INT(recvfrom, out.retval);
}

ssize_t
rpc_recv_gen(rcf_rpc_server *rpcs,
                 int s, void *buf, size_t len, rpc_send_recv_flags flags,
                 size_t rbuflen)
{
    rcf_rpc_op     op;
    tarpc_recv_in  in;
    tarpc_recv_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(recv, -1);
    }

    op = rpcs->op;

    if (buf != NULL && len > rbuflen)
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
        RETVAL_INT(recv, -1);
    }

    in.fd = s;
    in.len = len;
    if (buf != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = rbuflen;
        in.buf.buf_val = buf;
    }
    in.flags = flags;

    rcf_rpc_call(rpcs, "recv", &in, &out);

    if (RPC_IS_CALL_OK(rpcs))
    {
        if (buf != NULL && out.buf.buf_val != NULL)
            memcpy(buf, out.buf.buf_val, out.buf.buf_len);
    }

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(recv, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: recv(%d, %p[%u], %u, %s) -> %d (%s)",
                 rpcs->ta, rpcs->name, rpcop2str(op),
                 s, buf, rbuflen, len, send_recv_flags_rpc2str(flags),
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(recv, out.retval);
}

int
rpc_shutdown(rcf_rpc_server *rpcs, int s, rpc_shut_how how)
{
    tarpc_shutdown_in  in;
    tarpc_shutdown_out out;

    memset((char *)&in, 0, sizeof(in));
    memset((char *)&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(shutdown, -1);
    }

    rpcs->op = RCF_RPC_CALL_WAIT;
    in.fd = s;
    in.how = how;

    rcf_rpc_call(rpcs, "shutdown", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(shutdown, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s) shutdown(%d, %s) -> %d (%s)",
                 rpcs->ta, rpcs->name, s, shut_how_rpc2str(how),
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(shutdown, out.retval);
}

ssize_t
rpc_sendto(rcf_rpc_server *rpcs,
           int s, const void *buf, size_t len,
           rpc_send_recv_flags flags,
           const struct sockaddr *to, socklen_t tolen)
{
    rcf_rpc_op       op;
    tarpc_sendto_in  in;
    tarpc_sendto_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(sendto, -1);
    }

    op = rpcs->op;

    in.fd = s;
    in.len = len;
    if (to != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        if (tolen >= SA_COMMON_LEN)
        {
            in.to.sa_family = addr_family_h2rpc(to->sa_family);
            in.to.sa_data.sa_data_len = tolen - SA_COMMON_LEN;
            in.to.sa_data.sa_data_val = (uint8_t *)(to->sa_data);
        }
        else
        {
            in.to.sa_family = RPC_AF_UNSPEC;
            in.to.sa_data.sa_data_len = 0;
            /* Any no-NULL pointer is suitable here */
            in.to.sa_data.sa_data_val = (uint8_t *)to;
        }
    }
    in.tolen = tolen;
    if (buf != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = len;
        in.buf.buf_val = (char *)buf;
    }
    in.flags = flags;

    rcf_rpc_call(rpcs, "sendto", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(sendto, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: sendto(%d, %p, %u, %s, %s, %u) -> %d (%s)",
                 rpcs->ta, rpcs->name, rpcop2str(op),
                 s, buf, len, send_recv_flags_rpc2str(flags),
                 sockaddr2str(to), tolen,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(sendto, out.retval);
}

ssize_t
rpc_send(rcf_rpc_server *rpcs,
           int s, const void *buf, size_t len,
           rpc_send_recv_flags flags)
{
    rcf_rpc_op     op;
    tarpc_send_in  in;
    tarpc_send_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(send, -1);
    }

    op = rpcs->op;

    in.fd = s;
    in.len = len;
    if (buf != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.buf.buf_len = len;
        in.buf.buf_val = (char *)buf;
    }
    in.flags = flags;

    rcf_rpc_call(rpcs, "send", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(send, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: send(%d, %p, %u, %s) -> %d (%s)",
                 rpcs->ta, rpcs->name, rpcop2str(op),
                 s, buf, len, send_recv_flags_rpc2str(flags),
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(send, out.retval);
}

ssize_t
rpc_sendmsg(rcf_rpc_server *rpcs,
            int s, const struct rpc_msghdr *msg, rpc_send_recv_flags flags)
{
    rcf_rpc_op        op;
    tarpc_sendmsg_in  in;
    tarpc_sendmsg_out out;

    struct tarpc_msghdr rpc_msg;

    struct tarpc_iovec   iovec_arr[RCF_RPC_MAX_IOVEC];
    struct tarpc_cmsghdr cmsg_hdrs[RCF_RPC_MAX_CMSGHDR];

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(cmsg_hdrs, 0, sizeof(cmsg_hdrs));
    memset(&rpc_msg, 0, sizeof(rpc_msg));

    size_t i;

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(sendmsg, -1);
    }

    op = rpcs->op;
    in.s = s;
    in.flags = flags;

    if (msg != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.msg.msg_val = &rpc_msg;
        in.msg.msg_len = 1;

        if (msg->msg_riovlen > RCF_RPC_MAX_IOVEC)
        {
            rpcs->_errno = TE_RC(TE_RCF, TE_ENOMEM);
            ERROR("Length of the I/O vector is too long (%u) - "
                  "increase RCF_RPC_MAX_IOVEC(%u)",
                  msg->msg_riovlen, RCF_RPC_MAX_IOVEC);
            RETVAL_INT(sendmsg, -1);
        }

        if (((msg->msg_iov != NULL) &&
             (msg->msg_iovlen > msg->msg_riovlen)) ||
            ((msg->msg_name != NULL) &&
             (msg->msg_namelen > msg->msg_rnamelen)))
        {
            ERROR("Inconsistent real and declared lengths of buffers");
            rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
            RETVAL_INT(sendmsg, -1);
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
            if (msg->msg_rnamelen >= SA_COMMON_LEN)
            {
                rpc_msg.msg_name.sa_family = addr_family_h2rpc(
                    ((struct sockaddr *)(msg->msg_name))->sa_family);
                rpc_msg.msg_name.sa_data.sa_data_len =
                    msg->msg_rnamelen - SA_COMMON_LEN;
                rpc_msg.msg_name.sa_data.sa_data_val =
                    ((struct sockaddr *)(msg->msg_name))->sa_data;
            }
            else
            {
                rpc_msg.msg_name.sa_family = RPC_AF_UNSPEC;
                rpc_msg.msg_name.sa_data.sa_data_len = 0;
                /* Any not-NULL pointer is suitable here */
                rpc_msg.msg_name.sa_data.sa_data_val =
                    (uint8_t *)(msg->msg_name);
            }
        }
        rpc_msg.msg_namelen = msg->msg_namelen;
        rpc_msg.msg_flags = (int)msg->msg_flags;

        if (msg->msg_control != NULL)
        {
            struct cmsghdr *c;
            unsigned int    i;

            struct tarpc_cmsghdr *rpc_c = 
                rpc_msg.msg_control.msg_control_val;

            rpc_msg.msg_control.msg_control_val = cmsg_hdrs;
            for (i = 0, c = CMSG_FIRSTHDR((struct msghdr *)msg); 
                 i < RCF_RPC_MAX_CMSGHDR && c != NULL;
                 i++, c = CMSG_NXTHDR((struct msghdr *)msg, c))
            {
                char *data = CMSG_DATA(c);
                
                if ((int)(c->cmsg_len) < (char *)c - data)
                {
                    ERROR("Incorrect content of cmsg is not supported");
                    rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
                    RETVAL_INT(sendmsg, -1);
                }
                
                rpc_c->level = socklevel_h2rpc(c->cmsg_level);
                rpc_c->type = sockopt_h2rpc(c->cmsg_level, c->cmsg_type);
                if ((rpc_c->data.data_len = 
                         c->cmsg_len - ((char *)c - data)) > 0)
                {
                    rpc_c->data.data_val = data;
                }
            }
            
            if (i == RCF_RPC_MAX_CMSGHDR)
            {
                rpcs->_errno = TE_RC(TE_RCF, TE_ENOMEM);
                ERROR("Too many cmsg headers - increase "
                      "RCF_RPC_MAX_CMSGHDR");
                RETVAL_INT(sendmsg, -1);
            }
            
            if (i == 0)
            {
                WARN("Incorrect msg_control length is not supported"); 
                rpc_msg.msg_control.msg_control_val = NULL;
            }
            rpc_msg.msg_control.msg_control_len = i;
        }
    }

    rcf_rpc_call(rpcs, "sendmsg", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(sendmsg, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s)%s: sendmsg(%d, %p "
                                    "(msg_name: %p, msg_namelen: %d,"
                                    " msg_iov: %p, msg_iovlen: %d,"
                                    " msg_control: %p, msg_controllen: %d,"
                                    " msg_flags: %s)"
         ", %s) -> %d (%s)",
         rpcs->ta, rpcs->name, rpcop2str(op),
         s, msg,
         msg != NULL ? msg->msg_name : NULL,
         msg != NULL ? (int)msg->msg_namelen : -1,
         msg != NULL ? msg->msg_iov : NULL,
         msg != NULL ? (int)msg->msg_iovlen : -1,
         msg != NULL ? msg->msg_control : NULL,
         msg != NULL ? (int)msg->msg_controllen : -1,
         msg != NULL ? send_recv_flags_rpc2str(msg->msg_flags) : "",
         send_recv_flags_rpc2str(flags),
         out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(sendmsg, out.retval);
}

ssize_t
rpc_recvmsg(rcf_rpc_server *rpcs,
            int s, struct rpc_msghdr *msg, rpc_send_recv_flags flags)
{
    char              str_buf[1024];
    rcf_rpc_op        op;
    tarpc_recvmsg_in  in;
    tarpc_recvmsg_out out;

    struct tarpc_msghdr rpc_msg;

    struct tarpc_iovec   iovec_arr[RCF_RPC_MAX_IOVEC];
    struct tarpc_cmsghdr cmsg_hdrs[RCF_RPC_MAX_CMSGHDR];

    size_t i;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    memset(iovec_arr, 0, sizeof(iovec_arr));
    memset(&rpc_msg, 0, sizeof(rpc_msg));
    memset(cmsg_hdrs, 0, sizeof(cmsg_hdrs));
    
    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(recvmsg, -1);
    }

    op = rpcs->op;
    in.s = s;
    in.flags = flags;

    if (msg != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.msg.msg_val = &rpc_msg;
        in.msg.msg_len = 1;

        if (msg->msg_riovlen > RCF_RPC_MAX_IOVEC)
        {
            rpcs->_errno = TE_RC(TE_RCF, TE_ENOMEM);
            ERROR("Length of the I/O vector is too long (%u) - "
                  "increase RCF_RPC_MAX_IOVEC(%u)",
                  msg->msg_riovlen, RCF_RPC_MAX_IOVEC);
            RETVAL_INT(recvmsg, -1);
        }

        if (msg->msg_cmsghdr_num > RCF_RPC_MAX_CMSGHDR)
        {
            rpcs->_errno = TE_RC(TE_RCF, TE_ENOMEM);
            ERROR("Too many cmsg headers - increase RCF_RPC_MAX_CMSGHDR");
            RETVAL_INT(recvmsg, -1);
        }
        
        if (msg->msg_control != NULL && msg->msg_cmsghdr_num == 0)
        {
            rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
            ERROR("Number of cmsg headers is incorrect");
            RETVAL_INT(recvmsg, -1);
        }

        if (msg->msg_iovlen > msg->msg_riovlen ||
            msg->msg_namelen > msg->msg_rnamelen)
        {
            rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
            RETVAL_INT(recvmsg, -1);
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
            if (msg->msg_rnamelen >= SA_COMMON_LEN)
            {
                rpc_msg.msg_name.sa_family = addr_family_h2rpc(
                    ((struct sockaddr *)(msg->msg_name))->sa_family);
                rpc_msg.msg_name.sa_data.sa_data_len =
                    msg->msg_rnamelen - SA_COMMON_LEN;
                rpc_msg.msg_name.sa_data.sa_data_val =
                    ((struct sockaddr *)(msg->msg_name))->sa_data;
            }
            else
            {
                rpc_msg.msg_name.sa_family = RPC_AF_UNSPEC;
                rpc_msg.msg_name.sa_data.sa_data_len = 0;
                /* Any not-NULL pointer is suitable here */
                rpc_msg.msg_name.sa_data.sa_data_val =
                    (uint8_t *)(msg->msg_name);
            }
        }
        rpc_msg.msg_namelen = msg->msg_namelen;
        rpc_msg.msg_flags = msg->msg_flags;

        if (msg->msg_control != NULL)
        {
            rpc_msg.msg_control.msg_control_val = cmsg_hdrs;
            rpc_msg.msg_control.msg_control_len = msg->msg_cmsghdr_num;
            cmsg_hdrs[0].data.data_val = msg->msg_control;
            cmsg_hdrs[0].data.data_len = msg->msg_controllen -
                                         msg->msg_cmsghdr_num *
                                         CMSG_ALIGN(sizeof(struct cmsghdr));
        } 
    }

    rcf_rpc_call(rpcs, "recvmsg", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(recvmsg, out.retval);

    snprintf(str_buf, sizeof(str_buf),
             "RPC (%s,%s)%s: recvmsg(%d, %p(",
             rpcs->ta, rpcs->name, rpcop2str(op), s, msg);

    if (RPC_IS_CALL_OK(rpcs) && msg != NULL && out.msg.msg_val != NULL)
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
            msg->msg_iov[i].iov_len =
                rpc_msg.msg_iov.msg_iov_val[i].iov_len;
            memcpy(msg->msg_iov[i].iov_base,
                   rpc_msg.msg_iov.msg_iov_val[i].iov_base.iov_base_val,
                   msg->msg_iov[i].iov_rlen);
        }
        if (msg->msg_control != NULL)
        {
            struct cmsghdr *c;
            unsigned int    i;
            
            struct tarpc_cmsghdr *rpc_c = 
                rpc_msg.msg_control.msg_control_val;
            
            for (i = 0, c = CMSG_FIRSTHDR((struct msghdr *)msg); 
                 i < rpc_msg.msg_control.msg_control_len && c != NULL; 
                 i++, c = CMSG_NXTHDR((struct msghdr *)msg, c), rpc_c++)
            {
                c->cmsg_level = socklevel_rpc2h(rpc_c->level);
                c->cmsg_type = sockopt_rpc2h(rpc_c->type);
                c->cmsg_len = CMSG_LEN(rpc_c->data.data_len);
                if (rpc_c->data.data_val != NULL)
                    memcpy(CMSG_DATA(c), rpc_c->data.data_val, 
                           rpc_c->data.data_len);
            }
            
            if (c == NULL && i < rpc_msg.msg_control.msg_control_len)
            {
                ERROR("Unexpected lack of space in auxiliary buffer");
                rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
                RETVAL_INT(recvmsg, -1);
            }
            
            if (c != NULL)
                msg->msg_controllen = (char *)c - (char *)msg->msg_control;
        }

        msg->msg_flags = (rpc_send_recv_flags)rpc_msg.msg_flags;

        snprintf(str_buf + strlen(str_buf),
                 sizeof(str_buf) - strlen(str_buf),
                 "msg_name: %p, msg_namelen: %" TE_PRINTF_SOCKLEN_T "d, "
                 "msg_iov: %p, msg_iovlen: %" TE_PRINTF_SIZE_T "d, "
                 "msg_control: %p, msg_controllen: "
                 "%" TE_PRINTF_SOCKLEN_T "d, msg_flags: %s",
                 msg->msg_name, msg->msg_namelen,
                 msg->msg_iov, msg->msg_iovlen,
                 msg->msg_control, msg->msg_controllen,
                 send_recv_flags_rpc2str(msg->msg_flags));
    }

    snprintf(str_buf + strlen(str_buf), sizeof(str_buf) - strlen(str_buf),
             "), %s) -> %d (%s)",
             send_recv_flags_rpc2str(flags),
             out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    TAPI_RPC_LOG("%s", str_buf);

    RETVAL_INT(recvmsg, out.retval);
}


int 
rpc_cmsg_data_parse_ip_pktinfo(rcf_rpc_server *rpcs,
                               uint8_t *data, uint32_t data_len,
                               struct in_addr *ipi_addr,
                               int *ipi_ifindex)
{
    tarpc_cmsg_data_parse_ip_pktinfo_in  in;
    tarpc_cmsg_data_parse_ip_pktinfo_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(cmsg_data_parse_ip_pktinfo, -1);
    }

    if (data == NULL || data_len == 0 ||
        ipi_addr == NULL || ipi_ifindex == 0)
    {
        ERROR("%s(): Invalid parameters to function", __FUNCTION__);
        RETVAL_INT(cmsg_data_parse_ip_pktinfo, -1);
    }
    in.data.data_val = data;
    in.data.data_len = data_len;
    
    rcf_rpc_call(rpcs, "cmsg_data_parse_ip_pktinfo", &in, &out);
    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(cmsg_data_parse_ip_pktinfo, 
                                          out.retval);
    if (RPC_IS_CALL_OK(rpcs))
    {
        ipi_addr->s_addr = out.ipi_addr;
        *ipi_ifindex = out.ipi_ifindex;
    }
    TAPI_RPC_LOG("RPC (%s,%s): "
                 "cmsg_data_parse_ip_pktinfo(%p, %p, %p->%s, %p->%d)"
                 " -> %d (%s)",
                 rpcs->ta, rpcs->name,
                 data, data_len, ipi_addr, 
                 (out.retval == 0) ? inet_ntoa(*ipi_addr) : "undetermined",
                 ipi_ifindex,
                 (out.retval == 0) ? *ipi_ifindex : 0,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(cmsg_data_parse_ip_pktinfo, out.retval);
}

int
rpc_getsockname_gen(rcf_rpc_server *rpcs,
                    int s, struct sockaddr *name,
                    socklen_t *namelen,
                    socklen_t rnamelen)
{
    socklen_t   namelen_save =
        (namelen == NULL) ? (unsigned int)-1 : *namelen;

    tarpc_getsockname_in  in;
    tarpc_getsockname_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(getsockname, -1);
    }
    if (namelen != NULL && *namelen > rnamelen)
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
        RETVAL_INT(getsockname, -1);
    }
    rpcs->op = RCF_RPC_CALL_WAIT;

    in.fd = s;
    if (namelen != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.len.len_len = 1;
        in.len.len_val = namelen;
    }
    if (name != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        if (rnamelen >= SA_COMMON_LEN)
        {
            in.addr.sa_family = addr_family_h2rpc(name->sa_family);
            in.addr.sa_data.sa_data_len = rnamelen - SA_COMMON_LEN;
            in.addr.sa_data.sa_data_val = name->sa_data;
        }
        else
        {
            in.addr.sa_family = RPC_AF_UNSPEC;
            in.addr.sa_data.sa_data_len = 0;
            /* Any no-NULL pointer is suitable here */
            in.addr.sa_data.sa_data_val = (uint8_t *)name;
        }
    }

    rcf_rpc_call(rpcs, "getsockname", &in, &out);

    if (RPC_IS_CALL_OK(rpcs))
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

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(getsockname, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s): getsockname(%d, %p[%u], %u) -> %d (%s) "
                 "name=%s namelen=%u",
                 rpcs->ta, rpcs->name,
                 s, name, rnamelen, namelen_save,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)),
                 sockaddr2str(name),
                 (namelen == NULL) ? (unsigned int)-1 : *namelen);

    RETVAL_INT(getsockname, out.retval);
}

int
rpc_getpeername_gen(rcf_rpc_server *rpcs,
                    int s, struct sockaddr *name,
                    socklen_t *namelen,
                    socklen_t rnamelen)
{
    socklen_t   namelen_save =
        (namelen == NULL) ? (unsigned int)-1 : *namelen;

    tarpc_getpeername_in  in;
    tarpc_getpeername_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(getpeername, -1);
    }

    if (namelen != NULL && *namelen > rnamelen)
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
        RETVAL_INT(getpeername, -1);
    }
    rpcs->op = RCF_RPC_CALL_WAIT;

    in.fd = s;
    if (namelen != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        in.len.len_len = 1;
        in.len.len_val = namelen;
    }
    if (name != NULL && rpcs->op != RCF_RPC_WAIT)
    {
        if (rnamelen >= SA_COMMON_LEN)
        {
            in.addr.sa_family = addr_family_h2rpc(name->sa_family);
            in.addr.sa_data.sa_data_len = rnamelen - SA_COMMON_LEN;
            in.addr.sa_data.sa_data_val = name->sa_data;
        }
        else
        {
            in.addr.sa_family = RPC_AF_UNSPEC;
            in.addr.sa_data.sa_data_len = 0;
            /* Any no-NULL pointer is suitable here */
            in.addr.sa_data.sa_data_val = (uint8_t *)name;
        }
    }

    rcf_rpc_call(rpcs, "getpeername", &in, &out);

    if (RPC_IS_CALL_OK(rpcs))
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

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(getpeername, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s): getpeername(%d, %p[%u], %u) -> %d (%s) "
                 "name=%s namelen=%u",
                 rpcs->ta, rpcs->name,
                 s, name, rnamelen, namelen_save,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)),
                 sockaddr2str(name),
                 (namelen == NULL) ? (unsigned int)-1 : *namelen);

    RETVAL_INT(getpeername, out.retval);
}

int
rpc_getsockopt_gen(rcf_rpc_server *rpcs,
                   int s, rpc_socklevel level, rpc_sockopt optname,
                   void *optval, socklen_t *optlen, socklen_t roptlen)
{
    tarpc_getsockopt_in   in;
    tarpc_getsockopt_out  out;    
    struct option_value   val;
    char                  opt_val_str[4096] = {};
    
    socklen_t             optlen_copy;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(getsockopt, -1);
    }

    /*
     * Validate roptlen vs *optlen.
     * roptlen makes sence, iff optval is not NULL.
     */
    if (optlen != NULL && optval != NULL && *optlen > roptlen)
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
        RETVAL_INT(getsockopt, -1);
    }

    /* Copy parameters to tarpc_getsockopt_in structure */
    rpcs->op = RCF_RPC_CALL_WAIT;
    in.s = s;
    in.level = level;
    in.optname = optname;
    if (optlen != NULL)
    {
        in.optlen.optlen_len = 1;
        optlen_copy = *optlen;
        in.optlen.optlen_val = &optlen_copy;
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
                val.option_value_u.opt_string.opt_string_len = 
                    (roptlen == RPC_OPTLEN_AUTO)?
                    MIN(IFNAMSIZ, strlen(optval) + 1) : roptlen;
                val.option_value_u.opt_string.opt_string_val =
                    (char *)optval;
                break;

            case RPC_SO_LINGER:
                val.opttype = OPT_LINGER;
                if (roptlen >= sizeof(tarpc_linger))
                {
                    val.option_value_u.opt_linger.l_onoff =
                        ((tarpc_linger *)optval)->l_onoff;
                    val.option_value_u.opt_linger.l_linger =
                        ((tarpc_linger *)optval)->l_linger;
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(tarpc_linger)=%u, value is ignored",
                         sockopt_rpc2str(optname),
                         sizeof(tarpc_linger));
                }
                if (optlen_copy == sizeof(tarpc_linger))
                    optlen_copy = RPC_OPTLEN_AUTO;
                break;

            case RPC_SO_RCVTIMEO:
            case RPC_SO_SNDTIMEO:
                val.opttype = OPT_TIMEVAL;
                if (roptlen >= sizeof(tarpc_timeval))
                {
                    val.option_value_u.opt_timeval =
                        *((tarpc_timeval *)optval);
                }
                else
                {
                    WARN("Length of socket option %s value is less than "
                         "sizeof(tarpc_timeval)=%u, value is ignored",
                         sockopt_rpc2str(optname), sizeof(tarpc_timeval));
                }
                if (optlen_copy == sizeof(tarpc_timeval))
                    optlen_copy = RPC_OPTLEN_AUTO;
                break;
                
            case RPC_IPV6_PKTOPTIONS:
                ERROR("IPV6_PKTOPTIONS is not supported yet");
                RETVAL_INT(getsockopt, -1);
                break;
    
            case RPC_IPV6_ADD_MEMBERSHIP:
            case RPC_IPV6_DROP_MEMBERSHIP:
            case RPC_IPV6_JOIN_ANYCAST:
            case RPC_IPV6_LEAVE_ANYCAST:
            {
                struct ipv6_mreq *opt = (struct ipv6_mreq *)optval;
                
                val.opttype = OPT_MREQ6;

                memcpy(&val.option_value_u.opt_mreq6.ipv6mr_multiaddr.
                       ipv6mr_multiaddr_val,
                       &opt->ipv6mr_multiaddr, sizeof(struct in6_addr));
                val.option_value_u.opt_mreq6.ipv6mr_ifindex =
                    opt->ipv6mr_interface;

                if (optlen_copy == sizeof(struct ipv6_mreq))
                    optlen_copy = RPC_OPTLEN_AUTO;
                
                break;
            }

            case RPC_IP_ADD_MEMBERSHIP:
            case RPC_IP_DROP_MEMBERSHIP:
            {
                tarpc_mreqn *opt = (tarpc_mreqn *)optval;
                /* 
                 * Default type is OPT_MREQN (as one containing data of
                 * all types allowed for these options, but returned value
                 * has type OPT_IPADDR. Type is changed on Agent.
                 */
                val.opttype = OPT_MREQN; 
                   
                memcpy(&val.option_value_u.opt_mreqn.imr_multiaddr,
                       &opt->multiaddr, sizeof(opt->multiaddr));
                memcpy(&val.option_value_u.opt_mreqn.imr_address,
                       &opt->address, sizeof(opt->address));
                val.option_value_u.opt_mreqn.imr_ifindex = opt->ifindex;
                        
                if (opt->len_diff == 0)
                    optlen_copy = RPC_OPTLEN_AUTO;
                break;
            }

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
                if (optlen_copy == sizeof(struct tcp_info))
                    optlen_copy = RPC_OPTLEN_AUTO;
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
                if (optlen_copy == sizeof(int))
                    optlen_copy = RPC_OPTLEN_AUTO;
                break;
        }
    }

    rcf_rpc_call(rpcs, "getsockopt", &in, &out);

    if (RPC_IS_CALL_OK(rpcs))
    {
        if (optlen != NULL && out.optlen.optlen_val[0] != RPC_OPTLEN_AUTO)
        {
            *optlen = out.optlen.optlen_val[0];
        }
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

                        *optlen = strlen(out.optval.optval_val[0].
                                        option_value_u.opt_string.
                                        opt_string_val);

                        if ((buf = (char *)malloc(*optlen + 1)) == NULL)
                        {
                            ERROR("No memory available");
                            RETVAL_INT(getsockopt, -1);
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
                    if (roptlen >= sizeof(tarpc_linger))
                    {
                        ((tarpc_linger *)optval)->l_onoff =
                            out.optval.optval_val[0].option_value_u.
                                opt_linger.l_onoff;
                        ((tarpc_linger *)optval)->l_linger =
                           out.optval.optval_val[0].option_value_u.
                                opt_linger.l_linger;
                        snprintf(opt_val_str, sizeof(opt_val_str),
                                 "{ l_onoff: %d, l_linger: %d }",
                                 ((tarpc_linger *)optval)->l_onoff,
                                 ((tarpc_linger *)optval)->l_linger);
                    }
                    break;

                case RPC_SO_RCVTIMEO:
                case RPC_SO_SNDTIMEO:
                    if (roptlen >= sizeof(tarpc_timeval))
                    {
                        *((tarpc_timeval *)optval) =
                            out.optval.optval_val[0].option_value_u.
                                opt_timeval;

                        snprintf(opt_val_str, sizeof(opt_val_str),
                                 "{ tv_sec: %" TE_PRINTF_64 "d, "
                                 "tv_usec: %" TE_PRINTF_64 "d }",
                                 ((tarpc_timeval *)optval)->tv_sec,
                                 ((tarpc_timeval *)optval)->tv_usec);
                    }
                    break;

                case RPC_IP_ADD_MEMBERSHIP:
                case RPC_IP_DROP_MEMBERSHIP:
                case RPC_IP_MULTICAST_IF:
                {
                    tarpc_mreqn *opt = (tarpc_mreqn *)optval;
                    char         addr_buf[INET_ADDRSTRLEN];
 
                    memset(opt, 0, sizeof(*opt));
                    opt->type = out.optval.optval_val[0].opttype;
                    memcpy(&opt->address, &out.optval.optval_val[0].
                           option_value_u.opt_ipaddr,
                           sizeof(opt->address));
                    opt->len_diff = sizeof(opt->address) - sizeof(*opt);
                    snprintf(opt_val_str, sizeof(opt_val_str),
                             "{ addr: %s }",
                             inet_ntop(AF_INET, &opt->address,
                                       addr_buf, sizeof(addr_buf)));
                    break;
                }

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
                        if (level == RPC_SOL_SOCKET &&
                            optname == RPC_SO_ERROR)
                        {
                            snprintf(opt_val_str, sizeof(opt_val_str),
                                     "%s", te_rc_err2str(*(int *)optval));
                        }
                        else
                        {
                            snprintf(opt_val_str, sizeof(opt_val_str),
                                     "%d", *(int *)optval);
                        }
                    }
                    break;
            }
        }
    }

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(getsockopt, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s): getsockopt(%d, %s, %s, %p(%s), %p(%d)) "
                 "-> %d (%s)", rpcs->ta, rpcs->name,
                 s, socklevel_rpc2str(level), sockopt_rpc2str(optname),
                 optval, opt_val_str,
                 optlen, optlen == NULL ? 0 : *optlen,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(getsockopt, out.retval);
}

int
rpc_setsockopt(rcf_rpc_server *rpcs,
               int s, rpc_socklevel level, rpc_sockopt optname,
               const void *optval, socklen_t optlen)
{
    tarpc_setsockopt_in  in;
    tarpc_setsockopt_out out;
    struct option_value  val;
    char                 opt_val_str[128] = {};
    socklen_t            optlen_cpy = optlen;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rpcs == NULL)
    {
        ERROR("%s(): Invalid RPC server handle", __FUNCTION__);
        RETVAL_INT(setsockopt, -1);
    }

    in.s = s;
    in.level = level;
    in.optname = optname;
    in.optlen = optlen; /* optlen is corrected to auto automatically! */
    rpcs->op = RCF_RPC_CALL_WAIT;

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
                val.option_value_u.opt_string.opt_string_val =
                    (char *)optval;
                val.opttype = OPT_STRING;

                if ((buf = (char *)malloc(optlen + 1)) == NULL)
                {
                    ERROR("No memory available");
                    RETVAL_INT(setsockopt, -1);
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
                val.opttype = OPT_LINGER;
                val.option_value_u.opt_linger = *((tarpc_linger *)optval);
                
                if (optlen == sizeof(tarpc_linger))
                    in.optlen = RPC_OPTLEN_AUTO;

                snprintf(opt_val_str, sizeof(opt_val_str),
                         "{ l_onoff: %d, l_linger: %d }",
                         ((tarpc_linger *)optval)->l_onoff,
                         ((tarpc_linger *)optval)->l_linger);
                break;

            case RPC_SO_RCVTIMEO:
            case RPC_SO_SNDTIMEO:
                val.opttype = OPT_TIMEVAL;
                val.option_value_u.opt_timeval =
                    *((tarpc_timeval *)optval);

                if (optlen == sizeof(tarpc_timeval))
                    in.optlen = RPC_OPTLEN_AUTO;

                snprintf(opt_val_str, sizeof(opt_val_str),
                         "{ tv_sec: %" TE_PRINTF_64 "d, "
                         "tv_usec: %" TE_PRINTF_64 "d }",
                         ((tarpc_timeval *)optval)->tv_sec,
                         ((tarpc_timeval *)optval)->tv_usec);
                break;

            case RPC_IPV6_PKTOPTIONS:
                ERROR("IPV6_PKTOPTIONS is not supported yet");
                RETVAL_INT(getsockopt, -1);
                break;
    
            case RPC_IPV6_ADD_MEMBERSHIP:
            case RPC_IPV6_DROP_MEMBERSHIP:
            case RPC_IPV6_JOIN_ANYCAST:
            case RPC_IPV6_LEAVE_ANYCAST:
            {
                char buf[INET6_ADDRSTRLEN];

                inet_ntop(AF_INET6,
                          &((struct ipv6_mreq *)optval)->ipv6mr_multiaddr,
                          buf, sizeof(buf));
                
                val.opttype = OPT_MREQ6;
                val.option_value_u.opt_mreq6.ipv6mr_multiaddr.
                ipv6mr_multiaddr_val = (uint32_t *)
                    &((struct ipv6_mreq *)optval)->ipv6mr_multiaddr;
                
                val.option_value_u.opt_mreq6.ipv6mr_multiaddr.
                ipv6mr_multiaddr_len = sizeof(struct in6_addr);

                val.option_value_u.opt_mreq6.ipv6mr_ifindex =
                    ((struct ipv6_mreq *)optval)->ipv6mr_interface;

                if (optlen == sizeof(struct ipv6_mreq))
                    in.optlen = RPC_OPTLEN_AUTO;

                snprintf(opt_val_str, sizeof(opt_val_str),
                         "{ multiaddr: %s, ifindex: %d }",
                         buf, val.option_value_u.opt_mreq6.ipv6mr_ifindex);
                break;
            }

            case RPC_IP_ADD_MEMBERSHIP:
            case RPC_IP_DROP_MEMBERSHIP:
            case RPC_IP_MULTICAST_IF:
            {
                tarpc_mreqn *opt = (tarpc_mreqn *)optval;

                char addr_buf1[INET_ADDRSTRLEN];
                char addr_buf2[INET_ADDRSTRLEN];

                val.opttype = opt->type;
                optlen_cpy = (signed)optlen_cpy + opt->len_diff;
                in.optlen = (opt->len_diff == 0)?
                            RPC_OPTLEN_AUTO : optlen_cpy;
                        
                switch (opt->type)
                {
                    case OPT_IPADDR:
                    {
                        if (optname != RPC_IP_MULTICAST_IF)
                        {
                            ERROR("IP_MULTICAST_IF socket option does not"
                                  " support 'struct in_addr' argument");
                            return TE_RC(TE_TAPI, TE_EINVAL);
                        }
                        
                        memcpy(&val.option_value_u.opt_ipaddr,
                               (char *)&(opt->address),
                               sizeof(opt->address));
                        snprintf(opt_val_str, sizeof(opt_val_str),
                                 "{ address: %s }",
                                 inet_ntop(AF_INET,
                                           (char *)&(opt)->address,
                                           addr_buf1,
                                           sizeof(addr_buf1)));
                        break;
                    }
                    case OPT_MREQ:
                    {
                        memcpy(&val.option_value_u.opt_mreq.imr_multiaddr,
                              (char *)&(opt->multiaddr),
                              sizeof(opt->multiaddr));
                        memcpy(&val.option_value_u.opt_mreq.imr_address,
                               (char *)&(opt->address),
                               sizeof(opt->address));

                        snprintf(opt_val_str, sizeof(opt_val_str),
                                 "{ imr_multiaddr: %s, imr_address: %s }",
                                 inet_ntop(AF_INET,
                                           (char *)&(opt->multiaddr),
                                            addr_buf1, sizeof(addr_buf1)),
                                 inet_ntop(AF_INET,
                                           (char *)&(opt->address),
                                            addr_buf2, sizeof(addr_buf2)));
                        
                        break;
                    }
                    case OPT_MREQN:
                    {
                        memcpy(&val.option_value_u.opt_mreqn.imr_multiaddr,
                              (char *)&(opt->multiaddr),
                              sizeof(opt->multiaddr));
                        memcpy(&val.option_value_u.opt_mreqn.imr_address,
                               (char *)&(opt->address),
                               sizeof(opt->address));
                        val.option_value_u.opt_mreqn.imr_ifindex =
                        opt->ifindex;

                        snprintf(opt_val_str, sizeof(opt_val_str),
                                 "{ imr_multiaddr: %s, imr_address: %s, "
                                 "imr_ifindex: %d}",
                                 inet_ntop(AF_INET,
                                           (char *)&(opt->multiaddr),
                                            addr_buf1, sizeof(addr_buf1)),
                                 inet_ntop(AF_INET,
                                           (char *)&(opt->address),
                                            addr_buf2, sizeof(addr_buf2)),
                                 opt->ifindex);
                        
                        break;
                    } 
                    
                   default:
                   {
                       ERROR("Invalid argument type for socket option");
                       return TE_RC(TE_TAPI, TE_EINVAL); 
                   }
                   
                }

                break;
            }
            
            case RPC_SO_UPDATE_ACCEPT_CONTEXT:
            {
                val.opttype = OPT_HANDLE;
                val.option_value_u.opt_handle = *(int *)optval;
                if (optlen == sizeof(int))
                    in.optlen = RPC_OPTLEN_AUTO;
                break;

            }

            default:
                val.option_value_u.opt_int = *(int *)optval;
                val.opttype = OPT_INT;

                if (optlen == sizeof(int))
                    in.optlen = RPC_OPTLEN_AUTO;

                snprintf(opt_val_str, sizeof(opt_val_str), "%d",
                         *(int *)optval);
                break;
        }
    }

    rcf_rpc_call(rpcs, "setsockopt", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(setsockopt, out.retval);

    TAPI_RPC_LOG("RPC (%s,%s): setsockopt(%d, %s, %s, %p(%s), %d) "
                 "-> %d (%s)", rpcs->ta, rpcs->name,
                 s, socklevel_rpc2str(level), sockopt_rpc2str(optname),
                 optval, opt_val_str, optlen_cpy,
                 out.retval, errno_rpc2str(RPC_ERRNO(rpcs)));

    RETVAL_INT(setsockopt, out.retval);
}
