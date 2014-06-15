/** @file
 * @brief Test API - RPC
 *
 * Macros to get parameters in tests.
 *
 *
 * Copyright (C) 2005 Test Environment authors (see file AUTHORS in
 * the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
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
 * @author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 * @author Oleg Kravtsov <Oleg.Kravtsov@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TE_TAPI_RPC_PARAMS_H__
#define __TE_TAPI_RPC_PARAMS_H__


/**
 * The list of values allowed for parameter of type 'rpc_socket_domain'
 */
#define DOMAIN_MAPPING_LIST \
    MAPPING_LIST_ENTRY(PF_UNKNOWN), \
    MAPPING_LIST_ENTRY(PF_INET),    \
    MAPPING_LIST_ENTRY(PF_INET6),   \
    MAPPING_LIST_ENTRY(PF_PACKET),  \
    MAPPING_LIST_ENTRY(PF_LOCAL),   \
    MAPPING_LIST_ENTRY(PF_UNIX),    \
    MAPPING_LIST_ENTRY(PF_UNSPEC)

/**
 * Get the value of parameter of type 'rpc_socket_domain'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_socket_domain' (OUT)
 */
#define TEST_GET_DOMAIN(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, DOMAIN_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_socket_addr_family'
 */
#define ADDR_FAMILY_MAPPING_LIST \
    MAPPING_LIST_ENTRY(AF_UNKNOWN), \
    MAPPING_LIST_ENTRY(AF_INET), \
    MAPPING_LIST_ENTRY(AF_INET6), \
    MAPPING_LIST_ENTRY(AF_PACKET), \
    MAPPING_LIST_ENTRY(AF_LOCAL), \
    MAPPING_LIST_ENTRY(AF_UNIX), \
    MAPPING_LIST_ENTRY(AF_ETHER), \
    MAPPING_LIST_ENTRY(AF_UNSPEC)

/**
 * Get the value of parameter of type 'rpc_socket_addr_family'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_socket_addr_family'
 *                   (OUT)
 */
#define TEST_GET_ADDR_FAMILY(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, ADDR_FAMILY_MAPPING_LIST) 


/**
 * The list of values allowed for parameter of type 'rpc_socket_type'
 */
#define SOCK_TYPE_MAPPING_LIST \
    MAPPING_LIST_ENTRY(SOCK_STREAM), \
    MAPPING_LIST_ENTRY(SOCK_DGRAM), \
    MAPPING_LIST_ENTRY(SOCK_RAW), \
    MAPPING_LIST_ENTRY(SOCK_SEQPACKET), \
    MAPPING_LIST_ENTRY(SOCK_RDM), \
    MAPPING_LIST_ENTRY(SOCK_UNKNOWN), \
    MAPPING_LIST_ENTRY(SOCK_UNSPEC)

/**
 * Get the value of parameter of type 'rpc_socket_type'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_socket_type' (OUT)
 */
#define TEST_GET_SOCK_TYPE(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, SOCK_TYPE_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_shut_how'
 */
#define SHUT_HOW_MAPPING_LIST \
    MAPPING_LIST_ENTRY(SHUT_UNKNOWN), \
    MAPPING_LIST_ENTRY(SHUT_RD), \
    MAPPING_LIST_ENTRY(SHUT_WR), \
    MAPPING_LIST_ENTRY(SHUT_RDWR), \
    MAPPING_LIST_ENTRY(SHUT_NONE)

/**
 * Get the value of parameter of type 'rpc_shut_how'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_shut_how' (OUT)
 */
#define TEST_GET_SHUT_HOW(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, SHUT_HOW_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_socket_proto'
 */
#define PROTOCOL_MAPPING_LIST \
    MAPPING_LIST_ENTRY(IPPROTO_TCP), \
    MAPPING_LIST_ENTRY(IPPROTO_UDP), \
    MAPPING_LIST_ENTRY(PROTO_DEF), \
    MAPPING_LIST_ENTRY(PROTO_UNKNOWN), \
    MAPPING_LIST_ENTRY(IPPROTO_ICMP)

/**
 * Get the value of parameter of type 'rpc_socket_proto'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_socket_proto' (OUT)
 */
#define TEST_GET_PROTOCOL(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, PROTOCOL_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_ioctl_code'
 */
#define IOCTL_REQ_MAPPING_LIST \
    MAPPING_LIST_ENTRY(SIOCGSTAMP), \
    MAPPING_LIST_ENTRY(SIOCGSTAMPNS), \
    MAPPING_LIST_ENTRY(SIOCSHWTSTAMP), \
    MAPPING_LIST_ENTRY(FIOASYNC), \
    MAPPING_LIST_ENTRY(FIONBIO), \
    MAPPING_LIST_ENTRY(FIONREAD), \
    MAPPING_LIST_ENTRY(SIOCATMARK), \
    MAPPING_LIST_ENTRY(SIOCINQ), \
    MAPPING_LIST_ENTRY(SIOCOUTQ), \
    MAPPING_LIST_ENTRY(TIOCOUTQ), \
    MAPPING_LIST_ENTRY(SIOCSPGRP), \
    MAPPING_LIST_ENTRY(SIOCGPGRP), \
    MAPPING_LIST_ENTRY(SIOCGIFCONF), \
    MAPPING_LIST_ENTRY(SIOCGIFNAME), \
    MAPPING_LIST_ENTRY(SIOCGIFINDEX), \
    MAPPING_LIST_ENTRY(SIOCGIFFLAGS), \
    MAPPING_LIST_ENTRY(SIOCSIFFLAGS), \
    MAPPING_LIST_ENTRY(SIOCGIFADDR), \
    MAPPING_LIST_ENTRY(SIOCSIFADDR), \
    MAPPING_LIST_ENTRY(SIOCGIFNETMASK), \
    MAPPING_LIST_ENTRY(SIOCSIFNETMASK), \
    MAPPING_LIST_ENTRY(SIOCGIFBRDADDR), \
    MAPPING_LIST_ENTRY(SIOCSIFBRDADDR), \
    MAPPING_LIST_ENTRY(SIOCGIFDSTADDR), \
    MAPPING_LIST_ENTRY(SIOCSIFDSTADDR), \
    MAPPING_LIST_ENTRY(SIOCGIFHWADDR), \
    MAPPING_LIST_ENTRY(SIOCSIFMTU), \
    MAPPING_LIST_ENTRY(SIOCGIFMTU), \
    MAPPING_LIST_ENTRY(SIO_ASSOCIATE_HANDLE), \
    MAPPING_LIST_ENTRY(SIO_ENABLE_CIRCULAR_QUEUEING), \
    MAPPING_LIST_ENTRY(SIO_FIND_ROUTE), \
    MAPPING_LIST_ENTRY(SIO_FLUSH), \
    MAPPING_LIST_ENTRY(SIO_GET_BROADCAST_ADDRESS), \
    MAPPING_LIST_ENTRY(SIO_MULTIPOINT_LOOPBACK), \
    MAPPING_LIST_ENTRY(SIO_MULTICAST_SCOPE), \
    MAPPING_LIST_ENTRY(SIO_TRANSLATE_HANDLE), \
    MAPPING_LIST_ENTRY(SIO_ROUTING_INTERFACE_QUERY), \
    MAPPING_LIST_ENTRY(SIO_ROUTING_INTERFACE_CHANGE), \
    MAPPING_LIST_ENTRY(SIO_ADDRESS_LIST_QUERY), \
    MAPPING_LIST_ENTRY(SIO_ADDRESS_LIST_CHANGE), \
    MAPPING_LIST_ENTRY(SIO_ASSOCIATE_HANDLE), \
    MAPPING_LIST_ENTRY(SIO_CHK_QOS), \
    MAPPING_LIST_ENTRY(SIO_GET_EXTENSION_FUNCTION_POINTER), \
    MAPPING_LIST_ENTRY(SIO_KEEPALIVE_VALS), \
    MAPPING_LIST_ENTRY(SIO_MULTIPOINT_LOOPBACK), \
    MAPPING_LIST_ENTRY(SIO_MULTICAST_SCOPE), \
    MAPPING_LIST_ENTRY(SIO_ROUTING_INTERFACE_QUERY), \
    MAPPING_LIST_ENTRY(SIO_SET_QOS), \
    MAPPING_LIST_ENTRY(SIO_TRANSLATE_HANDLE), \
    MAPPING_LIST_ENTRY(SIO_UDP_CONNRESET), \
    MAPPING_LIST_ENTRY(SIO_INDEX_BIND), \
    MAPPING_LIST_ENTRY(SIO_UCAST_IF), \
    MAPPING_LIST_ENTRY(SIOUNKNOWN)

/**
 * Get the value of parameter of type 'rpc_ioctl_code'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_ioctl_code' (OUT)
 */
#define TEST_GET_IOCTL_REQ(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, IOCTL_REQ_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_socklevel'
 */
#define SOCKLEVEL_MAPPING_LIST \
    MAPPING_LIST_ENTRY(SOL_SOCKET), \
    MAPPING_LIST_ENTRY(SOL_IP), \
    MAPPING_LIST_ENTRY(SOL_IPV6), \
    MAPPING_LIST_ENTRY(SOL_TCP), \
    MAPPING_LIST_ENTRY(SOL_UDP), \
    MAPPING_LIST_ENTRY(SOL_UNKNOWN)

/**
 * Get the value of parameter of type 'rpc_socklevel'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_socklevel' (OUT)
 */
#define TEST_GET_SOCKLEVEL(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, SOCKLEVEL_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_sockopt'
 */
#define SOCKOPT_MAPPING_LIST \
    MAPPING_LIST_ENTRY(SO_ACCEPTCONN), \
    MAPPING_LIST_ENTRY(SO_CONDITIONAL_ACCEPT), \
    MAPPING_LIST_ENTRY(SO_BINDTODEVICE), \
    MAPPING_LIST_ENTRY(SO_BROADCAST), \
    MAPPING_LIST_ENTRY(SO_DEBUG), \
    MAPPING_LIST_ENTRY(SO_DONTROUTE), \
    MAPPING_LIST_ENTRY(SO_ERROR), \
    MAPPING_LIST_ENTRY(SO_KEEPALIVE), \
    MAPPING_LIST_ENTRY(SO_LINGER), \
    MAPPING_LIST_ENTRY(SO_DONTLINGER), \
    MAPPING_LIST_ENTRY(SO_OOBINLINE), \
    MAPPING_LIST_ENTRY(SO_PRIORITY), \
    MAPPING_LIST_ENTRY(SO_RCVBUF), \
    MAPPING_LIST_ENTRY(SO_RCVLOWAT), \
    MAPPING_LIST_ENTRY(SO_RCVTIMEO), \
    MAPPING_LIST_ENTRY(SO_REUSEADDR), \
    MAPPING_LIST_ENTRY(SO_REUSEPORT), \
    MAPPING_LIST_ENTRY(SO_EXCLUSIVEADDRUSE), \
    MAPPING_LIST_ENTRY(SO_SNDBUF), \
    MAPPING_LIST_ENTRY(SO_SNDLOWAT), \
    MAPPING_LIST_ENTRY(SO_SNDTIMEO), \
    MAPPING_LIST_ENTRY(SO_TYPE), \
    MAPPING_LIST_ENTRY(SO_USELOOPBACK), \
    MAPPING_LIST_ENTRY(SO_TIMESTAMP), \
    MAPPING_LIST_ENTRY(SO_TIMESTAMPNS), \
    MAPPING_LIST_ENTRY(SO_TIMESTAMPING), \
    MAPPING_LIST_ENTRY(SO_BSDCOMPAT), \
    MAPPING_LIST_ENTRY(SO_DOMAIN), \
    MAPPING_LIST_ENTRY(SO_MARK), \
    MAPPING_LIST_ENTRY(SO_PASSCRED), \
    MAPPING_LIST_ENTRY(SO_PROTOCOL), \
    MAPPING_LIST_ENTRY(SO_RCVBUFFORCE), \
    MAPPING_LIST_ENTRY(SO_SNDBUFFORCE), \
    MAPPING_LIST_ENTRY(IP_ADD_MEMBERSHIP), \
    MAPPING_LIST_ENTRY(IP_DROP_MEMBERSHIP), \
    MAPPING_LIST_ENTRY(IP_MULTICAST_IF), \
    MAPPING_LIST_ENTRY(IP_MULTICAST_LOOP), \
    MAPPING_LIST_ENTRY(IP_MULTICAST_TTL), \
    MAPPING_LIST_ENTRY(MCAST_JOIN_GROUP), \
    MAPPING_LIST_ENTRY(MCAST_LEAVE_GROUP), \
    MAPPING_LIST_ENTRY(IP_OPTIONS), \
    MAPPING_LIST_ENTRY(IP_PKTINFO), \
    MAPPING_LIST_ENTRY(IP_PKTOPTIONS), \
    MAPPING_LIST_ENTRY(IP_RECVERR), \
    MAPPING_LIST_ENTRY(IP_RECVOPTS), \
    MAPPING_LIST_ENTRY(IP_RECVTOS), \
    MAPPING_LIST_ENTRY(IP_RECVTTL), \
    MAPPING_LIST_ENTRY(IP_RETOPTS), \
    MAPPING_LIST_ENTRY(IP_ROUTER_ALERT), \
    MAPPING_LIST_ENTRY(IP_DONTFRAGMENT), \
    MAPPING_LIST_ENTRY(IP_TOS), \
    MAPPING_LIST_ENTRY(IP_TTL), \
    MAPPING_LIST_ENTRY(IP_MTU_DISCOVER), \
    MAPPING_LIST_ENTRY(IP_TOS), \
    MAPPING_LIST_ENTRY(IP_TTL), \
    MAPPING_LIST_ENTRY(IP_FREEBIND), \
    MAPPING_LIST_ENTRY(IP_MULTICAST_ALL), \
    MAPPING_LIST_ENTRY(IP_NODEFRAG), \
    MAPPING_LIST_ENTRY(IP_RECVORIGDSTADDR), \
    MAPPING_LIST_ENTRY(IP_TRANSPARENT), \
    MAPPING_LIST_ENTRY(IP_MTU), \
    MAPPING_LIST_ENTRY(IP_HDRINCL), \
    MAPPING_LIST_ENTRY(IPV6_DSTOPTS), \
    MAPPING_LIST_ENTRY(IPV6_RECVHOPLIMIT), \
    MAPPING_LIST_ENTRY(IPV6_HOPLIMIT), \
    MAPPING_LIST_ENTRY(IPV6_HOPOPTS), \
    MAPPING_LIST_ENTRY(IPV6_NEXTHOP), \
    MAPPING_LIST_ENTRY(IPV6_ROUTER_ALERT), \
    MAPPING_LIST_ENTRY(IPV6_V6ONLY), \
    MAPPING_LIST_ENTRY(IPV6_ADDRFORM), \
    MAPPING_LIST_ENTRY(IPV6_AUTHHDR), \
    MAPPING_LIST_ENTRY(IPV6_CHECKSUM), \
    MAPPING_LIST_ENTRY(IPV6_MULTICAST_HOPS), \
    MAPPING_LIST_ENTRY(IPV6_MULTICAST_LOOP), \
    MAPPING_LIST_ENTRY(IPV6_RECVPKTINFO), \
    MAPPING_LIST_ENTRY(IPV6_UNICAST_HOPS), \
    MAPPING_LIST_ENTRY(IPV6_RECVERR), \
    MAPPING_LIST_ENTRY(IPV6_FLOWINFO), \
    MAPPING_LIST_ENTRY(IPV6_RTHDR), \
    MAPPING_LIST_ENTRY(IPV6_MULTICAST_IF), \
    MAPPING_LIST_ENTRY(IPV6_MTU_DISCOVER), \
    MAPPING_LIST_ENTRY(IPV6_MTU), \
    MAPPING_LIST_ENTRY(IPV6_ADD_MEMBERSHIP), \
    MAPPING_LIST_ENTRY(IPV6_DROP_MEMBERSHIP), \
    MAPPING_LIST_ENTRY(TCP_NODELAY), \
    MAPPING_LIST_ENTRY(TCP_CORK), \
    MAPPING_LIST_ENTRY(TCP_MAXSEG), \
    MAPPING_LIST_ENTRY(TCP_KEEPIDLE), \
    MAPPING_LIST_ENTRY(TCP_KEEPINTVL), \
    MAPPING_LIST_ENTRY(TCP_KEEPCNT), \
    MAPPING_LIST_ENTRY(TCP_DEFER_ACCEPT), \
    MAPPING_LIST_ENTRY(TCP_QUICKACK), \
    MAPPING_LIST_ENTRY(TCP_USER_TIMEOUT), \
    MAPPING_LIST_ENTRY(TCP_LINGER2), \
    MAPPING_LIST_ENTRY(TCP_SYNCNT), \
    MAPPING_LIST_ENTRY(TCP_WINDOW_CLAMP), \
    MAPPING_LIST_ENTRY(TCP_INFO), \
    MAPPING_LIST_ENTRY(UDP_CORK), \
    MAPPING_LIST_ENTRY(SOCKOPT_UNKNOWN)

/**
 * Get the value of parameter of type 'rpc_sockopt'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_sockopt' (OUT)
 */
#define TEST_GET_SOCKOPT(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, SOCKOPT_MAPPING_LIST)

/**
 * Get the value of parameter of type 'rpc_poll_event'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_poll_event' (OUT)
 */
#define TEST_GET_POLL_EVT(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, POLL_EVENT_MAPPING_LIST)


/**
 * The list of values allowed for parameter of type 'rpc_sockopt'
 */
#define SOCKOPT_NAME_MAPPING_LIST \
        MAPPING_LIST_ENTRY(SO_ACCEPTCONN), \
        MAPPING_LIST_ENTRY(SO_BINDTODEVICE), \
        MAPPING_LIST_ENTRY(SO_BROADCAST), \
        MAPPING_LIST_ENTRY(SO_DEBUG), \
        MAPPING_LIST_ENTRY(SO_DONTROUTE), \
        MAPPING_LIST_ENTRY(SO_ERROR), \
        MAPPING_LIST_ENTRY(SO_KEEPALIVE), \
        MAPPING_LIST_ENTRY(SO_LINGER), \
        MAPPING_LIST_ENTRY(SO_OOBINLINE), \
        MAPPING_LIST_ENTRY(SO_PRIORITY), \
        MAPPING_LIST_ENTRY(SO_RCVBUF), \
        MAPPING_LIST_ENTRY(SO_RCVLOWAT), \
        MAPPING_LIST_ENTRY(SO_RCVTIMEO), \
        MAPPING_LIST_ENTRY(SO_REUSEADDR), \
        MAPPING_LIST_ENTRY(SO_SNDBUF), \
        MAPPING_LIST_ENTRY(SO_SNDLOWAT), \
        MAPPING_LIST_ENTRY(SO_SNDTIMEO), \
        MAPPING_LIST_ENTRY(SO_TYPE), \
        MAPPING_LIST_ENTRY(SO_TIMESTAMP), \
        MAPPING_LIST_ENTRY(SO_TIMESTAMPNS), \
        MAPPING_LIST_ENTRY(SO_TIMESTAMPING), \
        MAPPING_LIST_ENTRY(IP_ADD_MEMBERSHIP), \
        MAPPING_LIST_ENTRY(IP_DROP_MEMBERSHIP), \
        MAPPING_LIST_ENTRY(IP_HDRINCL), \
        MAPPING_LIST_ENTRY(IP_MULTICAST_IF), \
        MAPPING_LIST_ENTRY(IP_MULTICAST_LOOP), \
        MAPPING_LIST_ENTRY(IP_MULTICAST_TTL), \
        MAPPING_LIST_ENTRY(MCAST_JOIN_GROUP), \
        MAPPING_LIST_ENTRY(MCAST_LEAVE_GROUP), \
        MAPPING_LIST_ENTRY(IP_OPTIONS), \
        MAPPING_LIST_ENTRY(IP_PKTINFO), \
        MAPPING_LIST_ENTRY(IP_PKTOPTIONS), \
        MAPPING_LIST_ENTRY(IP_MTU_DISCOVER), \
        MAPPING_LIST_ENTRY(IP_RECVERR), \
        MAPPING_LIST_ENTRY(IP_RECVOPTS), \
        MAPPING_LIST_ENTRY(IP_RECVTOS), \
        MAPPING_LIST_ENTRY(IP_RECVTTL), \
        MAPPING_LIST_ENTRY(IP_RETOPTS), \
        MAPPING_LIST_ENTRY(IP_ROUTER_ALERT), \
        MAPPING_LIST_ENTRY(IP_TOS), \
        MAPPING_LIST_ENTRY(IP_TTL), \
        MAPPING_LIST_ENTRY(TCP_MAXSEG), \
        MAPPING_LIST_ENTRY(TCP_NODELAY), \
        MAPPING_LIST_ENTRY(SOCKOPT_UNKNOWN)

/**
 * Get the value of parameter of type 'rpc_sockopt'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_sockopt' (OUT)
 */
#define TEST_GET_SOCKOPT_NAME(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, SOCKOPT_NAME_MAPPING_LIST)

/**
 * Get the value of parameter of type 'rpc_tcpstate'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_tcpstate' (OUT)
 */
#define TEST_GET_TCP_STATE(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, TCP_STATE_MAPPING_LIST)

/**
 * The list of values allowed for parameter of type 'rpc_errno'
 */
#define ERRNO_MAPPING_LIST \
    MAPPING_LIST_ENTRY(EPERM), \
    MAPPING_LIST_ENTRY(ENOENT), \
    MAPPING_LIST_ENTRY(ESRCH), \
    MAPPING_LIST_ENTRY(EINTR), \
    MAPPING_LIST_ENTRY(EIO), \
    MAPPING_LIST_ENTRY(ENXIO), \
    MAPPING_LIST_ENTRY(E2BIG), \
    MAPPING_LIST_ENTRY(ENOEXEC), \
    MAPPING_LIST_ENTRY(EBADF), \
    MAPPING_LIST_ENTRY(ECHILD), \
    MAPPING_LIST_ENTRY(EAGAIN), \
    MAPPING_LIST_ENTRY(ENOMEM), \
    MAPPING_LIST_ENTRY(EACCES), \
    MAPPING_LIST_ENTRY(EFAULT), \
    MAPPING_LIST_ENTRY(ENOTBLK), \
    MAPPING_LIST_ENTRY(EBUSY), \
    MAPPING_LIST_ENTRY(EEXIST), \
    MAPPING_LIST_ENTRY(EXDEV), \
    MAPPING_LIST_ENTRY(ENODEV), \
    MAPPING_LIST_ENTRY(ENOTDIR), \
    MAPPING_LIST_ENTRY(EISDIR), \
    MAPPING_LIST_ENTRY(EINVAL), \
    MAPPING_LIST_ENTRY(ENFILE), \
    MAPPING_LIST_ENTRY(EMFILE), \
    MAPPING_LIST_ENTRY(ENOTTY), \
    MAPPING_LIST_ENTRY(ETXTBSY), \
    MAPPING_LIST_ENTRY(EFBIG), \
    MAPPING_LIST_ENTRY(ENOSPC), \
    MAPPING_LIST_ENTRY(ESPIPE), \
    MAPPING_LIST_ENTRY(EROFS), \
    MAPPING_LIST_ENTRY(EMLINK), \
    MAPPING_LIST_ENTRY(EPIPE), \
    MAPPING_LIST_ENTRY(EDOM), \
    MAPPING_LIST_ENTRY(ERANGE), \
    MAPPING_LIST_ENTRY(EDEADLK), \
    MAPPING_LIST_ENTRY(ENAMETOOLONG), \
    MAPPING_LIST_ENTRY(ENOLCK), \
    MAPPING_LIST_ENTRY(ENOSYS), \
    MAPPING_LIST_ENTRY(ENOTEMPTY), \
    MAPPING_LIST_ENTRY(ELOOP), \
    MAPPING_LIST_ENTRY(EWOULDBLOCK), \
    MAPPING_LIST_ENTRY(ENOMSG), \
    MAPPING_LIST_ENTRY(EIDRM), \
    MAPPING_LIST_ENTRY(ECHRNG), \
    MAPPING_LIST_ENTRY(EL2NSYNC), \
    MAPPING_LIST_ENTRY(EL3HLT), \
    MAPPING_LIST_ENTRY(EL3RST), \
    MAPPING_LIST_ENTRY(ELNRNG), \
    MAPPING_LIST_ENTRY(EUNATCH), \
    MAPPING_LIST_ENTRY(ENOCSI), \
    MAPPING_LIST_ENTRY(EL2HLT), \
    MAPPING_LIST_ENTRY(EBADE), \
    MAPPING_LIST_ENTRY(EBADR), \
    MAPPING_LIST_ENTRY(EXFULL), \
    MAPPING_LIST_ENTRY(ENOANO), \
    MAPPING_LIST_ENTRY(EBADRQC), \
    MAPPING_LIST_ENTRY(EBADSLT), \
    MAPPING_LIST_ENTRY(EDEADLOCK), \
    MAPPING_LIST_ENTRY(EBFONT), \
    MAPPING_LIST_ENTRY(ENOSTR), \
    MAPPING_LIST_ENTRY(ENODATA), \
    MAPPING_LIST_ENTRY(ETIME), \
    MAPPING_LIST_ENTRY(ENOSR), \
    MAPPING_LIST_ENTRY(ENONET), \
    MAPPING_LIST_ENTRY(ENOPKG), \
    MAPPING_LIST_ENTRY(EREMOTE), \
    MAPPING_LIST_ENTRY(ENOLINK), \
    MAPPING_LIST_ENTRY(EADV), \
    MAPPING_LIST_ENTRY(ESRMNT), \
    MAPPING_LIST_ENTRY(ECOMM), \
    MAPPING_LIST_ENTRY(EPROTO), \
    MAPPING_LIST_ENTRY(EMULTIHOP), \
    MAPPING_LIST_ENTRY(EDOTDOT), \
    MAPPING_LIST_ENTRY(EBADMSG), \
    MAPPING_LIST_ENTRY(EOVERFLOW), \
    MAPPING_LIST_ENTRY(ENOTUNIQ), \
    MAPPING_LIST_ENTRY(EBADFD), \
    MAPPING_LIST_ENTRY(EREMCHG), \
    MAPPING_LIST_ENTRY(ELIBACC), \
    MAPPING_LIST_ENTRY(ELIBBAD), \
    MAPPING_LIST_ENTRY(ELIBSCN), \
    MAPPING_LIST_ENTRY(ELIBMAX), \
    MAPPING_LIST_ENTRY(ELIBEXEC), \
    MAPPING_LIST_ENTRY(EILSEQ), \
    MAPPING_LIST_ENTRY(ERESTART), \
    MAPPING_LIST_ENTRY(ESTRPIPE), \
    MAPPING_LIST_ENTRY(EUSERS), \
    MAPPING_LIST_ENTRY(ENOTSOCK), \
    MAPPING_LIST_ENTRY(EDESTADDRREQ), \
    MAPPING_LIST_ENTRY(EMSGSIZE), \
    MAPPING_LIST_ENTRY(EPROTOTYPE), \
    MAPPING_LIST_ENTRY(ENOPROTOOPT), \
    MAPPING_LIST_ENTRY(EPROTONOSUPPORT), \
    MAPPING_LIST_ENTRY(ESOCKTNOSUPPORT), \
    MAPPING_LIST_ENTRY(EOPNOTSUPP), \
    MAPPING_LIST_ENTRY(EPFNOSUPPORT), \
    MAPPING_LIST_ENTRY(EAFNOSUPPORT), \
    MAPPING_LIST_ENTRY(EADDRINUSE), \
    MAPPING_LIST_ENTRY(EADDRNOTAVAIL), \
    MAPPING_LIST_ENTRY(ENETDOWN), \
    MAPPING_LIST_ENTRY(ENETUNREACH), \
    MAPPING_LIST_ENTRY(ENETRESET), \
    MAPPING_LIST_ENTRY(ECONNABORTED), \
    MAPPING_LIST_ENTRY(ECONNRESET), \
    MAPPING_LIST_ENTRY(ENOBUFS), \
    MAPPING_LIST_ENTRY(EISCONN), \
    MAPPING_LIST_ENTRY(ENOTCONN), \
    MAPPING_LIST_ENTRY(ESHUTDOWN), \
    MAPPING_LIST_ENTRY(ETOOMANYREFS), \
    MAPPING_LIST_ENTRY(ETIMEDOUT), \
    MAPPING_LIST_ENTRY(ECONNREFUSED), \
    MAPPING_LIST_ENTRY(EHOSTDOWN), \
    MAPPING_LIST_ENTRY(EHOSTUNREACH), \
    MAPPING_LIST_ENTRY(EALREADY), \
    MAPPING_LIST_ENTRY(EINPROGRESS), \
    MAPPING_LIST_ENTRY(ESTALE), \
    MAPPING_LIST_ENTRY(EUCLEAN), \
    MAPPING_LIST_ENTRY(ENOTNAM), \
    MAPPING_LIST_ENTRY(ENAVAIL), \
    MAPPING_LIST_ENTRY(EISNAM), \
    MAPPING_LIST_ENTRY(EREMOTEIO), \
    MAPPING_LIST_ENTRY(EDQUOT), \
    MAPPING_LIST_ENTRY(ENOMEDIUM), \
    MAPPING_LIST_ENTRY(EMEDIUMTYPE), \
    MAPPING_LIST_ENTRY(EUNKNOWN), \
    { "0", 0 }

/**
 * Get the value of parameter of type 'rpc_sockopt'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_sockopt' (OUT)
 */
#define TEST_GET_ERRNO_PARAM(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, ERRNO_MAPPING_LIST)


/** The list of values allowed for parameter of type 'rpc_signum' */
#define SIGNUM_MAPPING_LIST \
    MAPPING_LIST_ENTRY(SIGHUP),     \
    MAPPING_LIST_ENTRY(SIGINT),     \
    MAPPING_LIST_ENTRY(SIGQUIT),    \
    MAPPING_LIST_ENTRY(SIGILL),     \
    MAPPING_LIST_ENTRY(SIGABRT),    \
    MAPPING_LIST_ENTRY(SIGFPE),     \
    MAPPING_LIST_ENTRY(SIGKILL),    \
    MAPPING_LIST_ENTRY(SIGSEGV),    \
    MAPPING_LIST_ENTRY(SIGPIPE),    \
    MAPPING_LIST_ENTRY(SIGALRM),    \
    MAPPING_LIST_ENTRY(SIGTERM),    \
    MAPPING_LIST_ENTRY(SIGUSR1),    \
    MAPPING_LIST_ENTRY(SIGUSR2),    \
    MAPPING_LIST_ENTRY(SIGCHLD),    \
    MAPPING_LIST_ENTRY(SIGCONT),    \
    MAPPING_LIST_ENTRY(SIGSTOP),    \
    MAPPING_LIST_ENTRY(SIGTSTP),    \
    MAPPING_LIST_ENTRY(SIGTTIN),    \
    MAPPING_LIST_ENTRY(SIGTTOU),    \
    MAPPING_LIST_ENTRY(SIGIO)

/**
 * Get the value of parameter of type 'rpc_signum'
 *
 * @param var_name_  Name of the variable used to get the value of
 *                   "var_name_" parameter of type 'rpc_signum' (OUT)
 */
#define TEST_GET_SIGNUM(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, SIGNUM_MAPPING_LIST)


#endif /* !__TE_TAPI_RPC_PARAMS_H__ */
