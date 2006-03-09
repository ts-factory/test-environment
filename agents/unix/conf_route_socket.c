/** @file
 * @brief Unix Test Agent
 *
 * Unix TA routing configuring support using routing sockets interface.
 *
 * Copyright (C) 2006 Test Environment authors (see file AUTHORS
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
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id: conf.c 24823 2006-03-05 07:24:43Z arybchik $
 */

#ifdef USE_ROUTE_SOCKET

#define TE_LGR_USER     "Unix Conf Route Socket"

#include "te_config.h"
#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "te_errno.h"
#include "logger_api.h"
#include "rcf_pch_ta_cfg.h"
#include "unix_internal.h"


/** Current route request sequence number */
static int rt_seq = 0;


/**
 * Convert route message type to string.
 *
 * @param type      Message type (RTM_*)
 *
 * @return String representation of the type ("RTM_*").
 */
static const char *
rt_msghdr_type2str(unsigned int type)
{
    switch (type)
    {
#define RT_MSGHDR_TYPE2STR(_type) \
        case RTM_##_type:   return #_type;

        RT_MSGHDR_TYPE2STR(ADD);
        RT_MSGHDR_TYPE2STR(DELETE);
        RT_MSGHDR_TYPE2STR(CHANGE);
        RT_MSGHDR_TYPE2STR(GET);
        RT_MSGHDR_TYPE2STR(LOSING);
        RT_MSGHDR_TYPE2STR(REDIRECT);
        RT_MSGHDR_TYPE2STR(MISS);
        RT_MSGHDR_TYPE2STR(LOCK);
        RT_MSGHDR_TYPE2STR(OLDADD);
        RT_MSGHDR_TYPE2STR(OLDDEL);
        RT_MSGHDR_TYPE2STR(RESOLVE);
        RT_MSGHDR_TYPE2STR(NEWADDR);
        RT_MSGHDR_TYPE2STR(DELADDR);
        RT_MSGHDR_TYPE2STR(IFINFO);
#ifdef RTM_NEWMADDR
        RT_MSGHDR_TYPE2STR(NEWMADDR);
#endif
#ifdef RTM_DELMADDR
        RT_MSGHDR_TYPE2STR(DELMADDR);
#endif
#ifdef RTM_IFANNOUNCE
        RT_MSGHDR_TYPE2STR(IFANNOUNCE);
#endif
#ifdef RTM_IEEE80211
        RT_MSGHDR_TYPE2STR(IEEE80211);
#endif

#undef RT_MSGHDR_TYPE2STR

        default:    return "<UNKNOWN>";
    }
}

/**
 * Convert route message flags to string.
 *
 * @param flags     Message flags
 *
 * @return String representation of flags.
 *
 * @attentation The function use static buffer for string and,
 * therefore, it is not reenterable.
 */
static const char *
rt_msghdr_flags2str(unsigned int flags)
{
    static char     buf[256];
    char           *p = buf;
    size_t          rlen = sizeof(buf);
    int             out;

    *p = '\0';

#define RT_MSGHDR_FLAG2STR(_flag) \
    do {                                                \
        if ((RTF_##_flag) & flags)                      \
        {                                               \
            flags &= ~(RTF_##_flag);                    \
            out = snprintf(p, rlen, " " #_flag);        \
            if ((size_t)out >= rlen)                    \
            {                                           \
                ERROR("Too short buffer for RT flags"); \
                return buf;                             \
            }                                           \
            p += out;                                   \
            rlen -= out;                                \
        }                                               \
    } while (0)

    RT_MSGHDR_FLAG2STR(UP);
    RT_MSGHDR_FLAG2STR(GATEWAY);
    RT_MSGHDR_FLAG2STR(HOST);
    RT_MSGHDR_FLAG2STR(REJECT);
    RT_MSGHDR_FLAG2STR(DYNAMIC);
    RT_MSGHDR_FLAG2STR(MODIFIED);
    RT_MSGHDR_FLAG2STR(DONE);
#ifdef RTF_MASK
    RT_MSGHDR_FLAG2STR(MASK);
#endif
    RT_MSGHDR_FLAG2STR(CLONING);
    RT_MSGHDR_FLAG2STR(XRESOLVE);
    RT_MSGHDR_FLAG2STR(LLINFO);
    RT_MSGHDR_FLAG2STR(STATIC);
    RT_MSGHDR_FLAG2STR(BLACKHOLE);
#ifdef RTF_PRIVATE
    RT_MSGHDR_FLAG2STR(PRIVATE);
#endif
    RT_MSGHDR_FLAG2STR(PROTO2);
    RT_MSGHDR_FLAG2STR(PROTO1);
#ifdef RTF_MULTIRT
    RT_MSGHDR_FLAG2STR(MULTIRT);
#endif
#ifdef RTF_SETSRC
    RT_MSGHDR_FLAG2STR(SETSRC);
#endif
#ifdef RTF_PRCLONING
    RT_MSGHDR_FLAG2STR(PRCLONING);
#endif
#ifdef RTF_WASCLONED
    RT_MSGHDR_FLAG2STR(WASCLONED);
#endif
#ifdef RTF_PROTO3
    RT_MSGHDR_FLAG2STR(PROTO3);
#endif
#ifdef RTF_PINNED
    RT_MSGHDR_FLAG2STR(PINNED);
#endif
#ifdef RTF_LOCAL
    RT_MSGHDR_FLAG2STR(LOCAL);
#endif
#ifdef RTF_BROADCAST
    RT_MSGHDR_FLAG2STR(BROADCAST);
#endif
#ifdef RTF_MULTICAST
    RT_MSGHDR_FLAG2STR(MULTICAST);
#endif

#undef RT_MSGHDR_FLAG2STR

    if (flags != 0)
    {
        out = snprintf(p, rlen, " <UNKNOWN>");
        if ((size_t)out >= rlen)
        {
            ERROR("Too short buffer for RT flags");
            return buf;
        }
    }

    return buf;
}

/**
 * Convert route message addresses flags to string.
 *
 * @param flags     Message addresses flags
 *
 * @return String representation of addresses flags.
 *
 * @attentation The function use static buffer for string and,
 * therefore, it is not reenterable.
 */
static const char *
rt_msghdr_addrs2str(unsigned int addrs)
{
    static char     buf[256];
    char           *p = buf;
    size_t          rlen = sizeof(buf);
    int             out;

    *p = '\0';

#define RT_MSGHDR_ADDR2STR(_addr) \
    do {                                                            \
        if ((RTA_##_addr) & addrs)                                  \
        {                                                           \
            addrs &= ~(RTA_##_addr);                                \
            out = snprintf(p, rlen, " " #_addr);                    \
            if ((size_t)out >= rlen)                                \
            {                                                       \
                ERROR("Too short buffer for RT addresses flags");   \
                return buf;                                         \
            }                                                       \
            p += out;                                               \
            rlen -= out;                                            \
        }                                                           \
    } while (0)

    RT_MSGHDR_ADDR2STR(DST);
    RT_MSGHDR_ADDR2STR(GATEWAY);
    RT_MSGHDR_ADDR2STR(NETMASK);
    RT_MSGHDR_ADDR2STR(GENMASK);
    RT_MSGHDR_ADDR2STR(IFP);
    RT_MSGHDR_ADDR2STR(IFA);
    RT_MSGHDR_ADDR2STR(AUTHOR);
    RT_MSGHDR_ADDR2STR(BRD);
#ifdef RTA_SRC
    RT_MSGHDR_ADDR2STR(SRC);
#endif
#ifdef RTA_SRCIFP
    RT_MSGHDR_ADDR2STR(SRCIFP);
#endif

#undef RT_MSGHDR_ADDR2STR

    if (addrs != 0)
    {
        out = snprintf(p, rlen, " <UNKNOWN>");
        if ((size_t)out >= rlen)
        {
            ERROR("Too short buffer for RT addresses flags");
            return buf;
        }
    }
    return buf;
}

/**
 * Convert route message metrics flags to string.
 *
 * @param flags     Message metrics flags
 *
 * @return String representation of metrics flags.
 */
static const char *
rt_msghdr_metrics2str(unsigned int metrics, char *buf, size_t buflen)
{
    char   *p = buf;
    size_t  rlen = buflen;
    int     out;

    if (buflen <= 0)
    {
        ERROR("Too short buffer for RT metrics flags");
        return "";
    }

    *p = '\0';

#define RT_MSGHDR_METRIC2STR(_metric) \
    do {                                                            \
        if ((RTV_##_metric) & metrics)                              \
        {                                                           \
            metrics &= ~(RTV_##_metric);                            \
            out = snprintf(p, rlen, " " #_metric);                  \
            if ((size_t)out >= rlen)                                \
            {                                                       \
                ERROR("Too short buffer for RT metrics flags");     \
                return buf;                                         \
            }                                                       \
            p += out;                                               \
            rlen -= out;                                            \
        }                                                           \
    } while (0)

    RT_MSGHDR_METRIC2STR(MTU);
    RT_MSGHDR_METRIC2STR(HOPCOUNT);
    RT_MSGHDR_METRIC2STR(EXPIRE);
    RT_MSGHDR_METRIC2STR(RPIPE);
    RT_MSGHDR_METRIC2STR(SPIPE);
    RT_MSGHDR_METRIC2STR(SSTHRESH);
    RT_MSGHDR_METRIC2STR(RTT);
    RT_MSGHDR_METRIC2STR(RTTVAR);

#undef RT_MSGHDR_METRIC2STR

    if (metrics != 0)
    {
        out = snprintf(p, rlen, " <UNKNOWN>");
        if ((size_t)out >= rlen)
        {
            ERROR("Too short buffer for RT metrics flags");
            return buf;
        }
    }
    return buf;
}

/** Log routing socket message */
static void
route_log(const struct rt_msghdr *rtm)
{
    char                    inits[64];
    char                    locks[64];
    char                    addrs[RTAX_MAX][100];
    const struct sockaddr  *addr = (const struct sockaddr *)(rtm + 1);
    socklen_t               addrlen;
    unsigned int            i;

    for (i = 0; i < RTAX_MAX; i++)
    {
        if (rtm->rtm_addrs & (1 << i))
        {
            if (addr->sa_family == AF_INET)
            {
                inet_ntop(AF_INET, &SIN(addr)->sin_addr,
                          addrs[i], sizeof(addrs[i]));
                addrlen = sizeof(struct sockaddr_in);
            }
            else if (addr->sa_family == AF_INET6)
            {
                inet_ntop(AF_INET6, &SIN6(addr)->sin6_addr,
                          addrs[i], sizeof(addrs[i]));
                addrlen = sizeof(struct sockaddr_in6);
            }
            else
            {
                ERROR("Unknown address family %u", addr->sa_family);
                break;
            }
            addr = (const struct sockaddr *)(((uint8_t *)addr) + addrlen);
        }
        else
        {
            addrs[i][0] = '\0';
        }
    }

    PRINT("len=%u ver=%u type=%s index=%u pid=%ld seq=%d errno=%d use=%d\n"
         "addrs=%s\nflags=%s\ninits=%s\nlocks=%s\n"
         "mtu=%u hops=%u expire=%u recvpipe=%u sendpipe=%u\n"
         "ssthresh=%u rtt=%u rttvar=%u pksent=%u\n"
         "dst=%s\n"
         "gateway=%s\n"
         "netmask=%s\n"
         "genmask=%s\n"
         "ifp=%s\n"
         "ifa=%s\n"
         "author=%s\n"
         "brd=%s\n"
#ifdef RTAX_SRC
         "src=%s\n"
#endif
#ifdef RTAX_SRCIFP
         "srcifp=%s\n"
#endif
         ,
         rtm->rtm_msglen, rtm->rtm_version,
         rt_msghdr_type2str(rtm->rtm_type), rtm->rtm_index,
         (long)rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno, rtm->rtm_use,
         rt_msghdr_addrs2str(rtm->rtm_addrs),
         rt_msghdr_flags2str(rtm->rtm_flags),
         rt_msghdr_metrics2str(rtm->rtm_inits, inits, sizeof(inits)),
         rt_msghdr_metrics2str(rtm->rtm_rmx.rmx_locks, locks, sizeof(locks)),
         (unsigned)rtm->rtm_rmx.rmx_mtu,
         (unsigned)rtm->rtm_rmx.rmx_hopcount,
         (unsigned)rtm->rtm_rmx.rmx_expire,
         (unsigned)rtm->rtm_rmx.rmx_recvpipe,
         (unsigned)rtm->rtm_rmx.rmx_sendpipe,
         (unsigned)rtm->rtm_rmx.rmx_ssthresh,
         (unsigned)rtm->rtm_rmx.rmx_rtt,
         (unsigned)rtm->rtm_rmx.rmx_rttvar,
         (unsigned)rtm->rtm_rmx.rmx_pksent,
         addrs[RTAX_DST], addrs[RTAX_GATEWAY], addrs[RTAX_NETMASK],
         addrs[RTAX_GENMASK], addrs[RTAX_IFP], addrs[RTAX_IFA],
         addrs[RTAX_AUTHOR], addrs[RTAX_BRD]
#ifdef RTAX_SRC
         , addrs[RTAX_SRC]
#endif
#ifdef RTAX_SRCIFP
         , addrs[RTAX_SRCIFP]
#endif
         );
}

/**
 * Convert routing socket message to system-independent route
 * representation.
 *
 * @param msg       Routing socket message
 * @param rt_info   Location for system-independed route representation
 *
 * @return Status code.
 */
static te_errno
rt_msghdr_to_ta_rt_info(const struct rt_msghdr *msg, ta_rt_info_t *rt_info)
{
    const struct sockaddr  *addr = (const struct sockaddr *)(msg + 1);
    socklen_t               addrlen;

    rt_info->flags = 0;

    if (msg->rtm_addrs & RTA_DST)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        memcpy(&rt_info->dst, addr, addrlen);
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_GATEWAY)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        memcpy(&rt_info->gw, addr, addrlen);
        rt_info->flags |= TA_RT_INFO_FLG_GW;
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_NETMASK)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        if (addr->sa_family == AF_INET)
        {
            MASK2PREFIX(ntohl(SIN(addr)->sin_addr.s_addr),
                        rt_info->prefix);
        }
        else if (addr->sa_family == AF_INET6)
            rt_info->prefix = 0; /* FIXME */
        else
            assert(FALSE);
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_GENMASK)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_IFP)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_IFA)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_AUTHOR)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

    if (msg->rtm_addrs & RTA_BRD)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }

#ifdef RTA_SRC
    if (msg->rtm_addrs & RTA_SRC)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }
#endif

#ifdef RTA_SRCIFP
    if (msg->rtm_addrs & RTA_SRCIFP)
    {
#if HAVE_STRUCT_SOCKADDR_SA_LEN
        addrlen = addr->sa_len;
#else
        if (addr->sa_family == AF_INET)
            addrlen = sizeof(struct sockaddr_in);
        else if (addr->sa_family == AF_INET6)
            addrlen = sizeof(struct sockaddr_in6);
        else
        {
            ERROR("Unknown address family %u", addr->sa_family);
            return TE_RC(TE_TA_UNIX, TE_EAFNOSUPPORT);
        }
#endif
        addr = (const struct sockaddr *)(((const uint8_t *)addr) + addrlen);
    }
#endif

    rt_info->ifname[0] = '\0';

    rt_info->metric = (msg->rtm_inits & RTV_HOPCOUNT) ?
        (rt_info->flags |= TA_RT_INFO_FLG_METRIC,
         msg->rtm_rmx.rmx_hopcount) : 0;

    rt_info->mtu = (msg->rtm_inits & RTV_MTU) ?
        (rt_info->flags |= TA_RT_INFO_FLG_MTU, msg->rtm_rmx.rmx_mtu) : 0;
    
    rt_info->win = 0; /* No such metric */
    
    rt_info->irtt = (msg->rtm_inits & RTV_RTT) ?
        (rt_info->flags |= TA_RT_INFO_FLG_IRTT, msg->rtm_rmx.rmx_rtt) : 0;
    
    rt_info->tos = 0; /* No such metric/field */

    /* TODO: Wiser mapping */
    rt_info->type = (msg->rtm_flags & RTF_BLACKHOLE) ?
                        TA_RT_TYPE_BLACKHOLE : TA_RT_TYPE_UNICAST;

    return 0;
}


/* See the description in conf_route.h */
te_errno
ta_unix_conf_route_find(ta_rt_info_t *rt_info)
{
    int                 rt_sock = -1;
    size_t              rt_buflen = sizeof(struct rt_msghdr) +
                                    sizeof(struct sockaddr_in6) * RTAX_MAX;
                        /* Assume that IPv6 sockaddr is the biggest */
    uint8_t            *rt_buf = NULL;
    pid_t               rt_pid = getpid();
    te_errno            rc;
    struct rt_msghdr   *rtm;
    socklen_t           addrlen;
    ssize_t             ret;

    /*
     * 'man -s 7P route' on SunOS 5.X suggests to use AF_* as the last
     * argument.
     */
    rt_sock = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC);
    if (rt_sock < 0)
    {
        rc = TE_OS_RC(TE_TA_UNIX, errno);
        ERROR("Cannot open routing socket: %r", rc);
        goto cleanup;
    }

    rt_buf = calloc(1, rt_buflen);
    if (rt_buf == NULL)
    {
        rc = TE_OS_RC(TE_TA_UNIX, errno);
        assert(rc != 0);
        goto cleanup;
    }

    rtm = (struct rt_msghdr *)rt_buf;
    rtm->rtm_msglen = sizeof(*rtm);
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type = RTM_GET;
    rtm->rtm_addrs = RTA_DST;
    rtm->rtm_pid = rt_pid;
    rtm->rtm_seq = ++rt_seq;

    addrlen = (rt_info->dst.ss_family == AF_INET) ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    memcpy(rt_buf + rtm->rtm_msglen, &rt_info->dst, addrlen);
    rtm->rtm_msglen += addrlen;

    assert(rtm->rtm_msglen <= rt_buflen);

    ret = write(rt_sock, rt_buf, rtm->rtm_msglen);
    if (ret != rtm->rtm_msglen)
    {
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        ERROR("Failed to send route request to kernel");
        goto cleanup;
    }

    do {

        ret = read(rt_sock, rt_buf, rt_buflen);
        if (ret < 0)
        {
            rc = TE_OS_RC(TE_TA_UNIX, errno);
            ERROR("Failed to receive route reply from kernel");
            goto cleanup;
        }
        if (ret != rtm->rtm_msglen)
        {
            rc = TE_RC(TE_TA_UNIX, TE_EIO);
            ERROR("Unexpected route reply from kernel");
            goto cleanup;
        }

    } while ((rtm->rtm_type != RTM_GET) || (rtm->rtm_seq != rt_seq) ||
             (rtm->rtm_pid != rt_pid));

#if 1
    /* Reply got */
    route_log(rtm);
#endif

    rc = rt_msghdr_to_ta_rt_info(rtm, rt_info);

cleanup:
    free(rt_buf);
    close(rt_sock);

    return rc;
}

/* See the description in conf_route.h. */
te_errno
ta_unix_conf_route_change(ta_cfg_obj_action_e  action,
                          ta_rt_info_t        *rt_info)
{
    UNUSED(action);
    UNUSED(rt_info);
    return TE_RC(TE_TA_UNIX, TE_ENOSYS);
}

/* See the description in conf_route.h */
te_errno
ta_unix_conf_route_blackhole_list(char **list)
{
    *list = NULL;
    return 0;
}

/* See the description in conf_route.h */
te_errno
ta_unix_conf_route_blackhole_add(ta_rt_info_t *rt_info)
{
    UNUSED(rt_info);
    return TE_RC(TE_TA_UNIX, TE_ENOSYS);
}

/* See the description in conf_route.h */
te_errno 
ta_unix_conf_route_blackhole_del(ta_rt_info_t *rt_info)
{
    UNUSED(rt_info);
    return TE_RC(TE_TA_UNIX, TE_ENOSYS);
}


#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>

#if 0
/*
 * Kernel routing table read by sysctl function.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/* Kernel routing table read up by sysctl function. */
int
route_read ()
{
  caddr_t buf, end, ref;
  size_t bufsiz;
  struct rt_msghdr *rtm;
  void rtm_read (struct rt_msghdr *);
  
#define MIBSIZ 6
  int mib[MIBSIZ] = 
  {
    CTL_NET,
    PF_ROUTE,
    0,
    0,
    NET_RT_DUMP,
    0
  };
		      
  /* Get buffer size. */
  if (sysctl (mib, MIBSIZ, NULL, &bufsiz, NULL, 0) < 0) 
    {
      zlog_warn ("sysctl fail: %s", strerror (errno));
      return -1;
    }

  /* Allocate buffer. */
  ref = buf = XMALLOC (MTYPE_TMP, bufsiz);
  
  /* Read routing table information by calling sysctl(). */
  if (sysctl (mib, MIBSIZ, buf, &bufsiz, NULL, 0) < 0) 
    {
      zlog_warn ("sysctl() fail by %s", strerror (errno));
      return -1;
    }

  for (end = buf + bufsiz; buf < end; buf += rtm->rtm_msglen) 
    {
      rtm = (struct rt_msghdr *) buf;
      rtm_read (rtm);
    }

  /* Free buffer. */
  XFREE (MTYPE_TMP, ref);

  return 0;
}
#endif

/* See the description in conf_route.h */
te_errno
ta_unix_conf_route_list(char **list)
{
    WARN("List of routes via sysctl is not implemented yet");
    *list = NULL;
    return 0;
}

#endif /* HAVE_SYS_SYSCTL_H && !defined(USE_NETLINK) */

#endif /* USE_ROUTE_SOCKET */
