/** @file
 * @brief Test API to access Configurator
 *
 * Implementation
 *
 *
 * Copyright (C) 2004 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
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
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 * @author Oleg Kravtsov <Oleg.Kravtsov@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "Configuration TAPI"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_NET_ROUTE_H
#include <net/route.h>
#endif

#include "te_defs.h"
#include "te_errno.h"
#include "te_stdint.h"
#include "conf_api.h"
#include "logger_api.h"
#include "rcf_api.h"

#include "tapi_sockaddr.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg.h"


/** Operations with routing/ARP table */
enum tapi_cfg_oper {
    OP_ADD, /**< Add operation */
    OP_DEL, /**< Delete operation */
};

/* Forward declarations */
static int tapi_cfg_route_op(enum tapi_cfg_oper op, const char *ta,
                             int addr_family,
                             const void *dst_addr, int prefix,
                             const void *gw_addr, const char *dev,
                             uint32_t flags, int metric, int mss, 
                             int win, int irtt, cfg_handle *cfg_hndl);

static int tapi_cfg_arp_op(enum tapi_cfg_oper op, const char *ta,
                           const void *net_addr, const void *link_addr);


/* See description in tapi_cfg.h */
int
tapi_cfg_get_son_mac(const char *father, const char *subid,
                     const char *name, uint8_t *p_mac)
{
    int                 rc;
    cfg_handle          handle;
    cfg_val_type        type;
    struct sockaddr    *p_addr;


    rc = cfg_find_fmt(&handle, "%s/%s:%s", father, subid, name);
    if (rc != 0)
    {
        ERROR("Failed(%x) to get handle of '%s:' son of %s",
            rc, subid, father);
        return rc;
    }
    
    type = CVT_ADDRESS;
    rc = cfg_get_instance(handle, &type, &p_addr);
    if (rc != 0)
    {
        ERROR("Failed(%x) to get MAC address using OID %s/%s:%s",
            rc, father, subid, name);
        return rc;
    }
    if (p_addr->sa_family != AF_LOCAL)
    {
        ERROR("Unexpected address family %d", p_addr->sa_family);
    }
    else
    {
        memcpy(p_mac, p_addr->sa_data, ETHER_ADDR_LEN);
    }
    free(p_addr);

    return rc;
}



/*
 * Temporaraly located here
 */

const char * const cfg_oid_ta_port_admin_status_fmt =
    "/agent:%s/port:%u/admin:/status:";
const char * const cfg_oid_ta_port_oper_status_fmt =
    "/agent:%s/port:%u/state:/status:";
const char * const cfg_oid_ta_port_oper_speed_fmt =
    "/agent:%s/port:%u/state:/speed:";

const char * const cfg_oid_oper_status_fmt =
    "%s/state:/status:";
const char * const cfg_oid_oper_speed_fmt =
    "%s/state:/speed:";

/** Format string for Switch VLAN OID */
static const char * const cfg_oid_ta_vlan_fmt =
    "/agent:%s/vlan:%u";
/** Format string for Switch VLAN port OID */
static const char * const cfg_oid_ta_vlan_port_fmt =
    "/agent:%s/vlan:%u/port:%u";


/**
 * Add VLAN on switch.
 *
 * @param ta_name   - name of the test agent
 * @param vid       - VLAN identifier
 *
 * @return Status code.
 */
int
tapi_cfg_switch_add_vlan(const char *ta_name, uint16_t vid)
{
    /* 
     * Format string without specifiers + TA name + maximum length
     * of printed VID + '\0'
     */
    char        oid[strlen(cfg_oid_ta_vlan_fmt) - 4 +
                    strlen(ta_name) + 5 + 1];
    int         rc;
    cfg_handle  handle;

    
    ENTRY("ta_name=%s vid=%u", ta_name, vid);
    /* Prepare OID */
    sprintf(oid, cfg_oid_ta_vlan_fmt, ta_name, vid);

    rc = cfg_find_str(oid, &handle);
    if (rc == 0)
    {
        int             state;
        cfg_val_type    type;

        VERB("VLAN %u already exists on TA %s", vid, ta_name);
        type = CVT_INTEGER;
        rc = cfg_get_instance(handle, &type, &state);
        if (rc != 0)
        {
            VERB("cfg_get_instance() failed(0x%x)", rc);
            EXIT("0x%x", rc);
            return rc;
        }
        if (state != 1)
        {
            ERROR("VLAN %u disabled on TA %s", vid, ta_name);
            EXIT("ETEENV");
            return ETEENV;
        }
        rc = EEXIST;
        EXIT("EEXIST");
    }
    else
    {
        VERB("Add instance '%s'", oid);
        rc = cfg_add_instance_str(oid, &handle, CVT_INTEGER, 1);
        if (rc != 0)
        {
            ERROR("Addition of VLAN %u on TA %s failed(0x%x)",
                vid, ta_name, rc);
        }
        EXIT("0x%x", rc);
    }

    return rc;
}


/**
 * Delete VLAN from switch.
 *
 * @param ta_name   - name of the test agent
 * @param vid       - VLAN identifier
 *
 * @return Status code.
 */
int
tapi_cfg_switch_del_vlan(const char *ta_name, uint16_t vid)
{
    /* 
     * Format string without specifiers + TA name + maximum length
     * of printed VID + '\0'
     */
    char        oid[strlen(cfg_oid_ta_vlan_fmt) - 4 + 
                    strlen(ta_name) + 5 + 1];
    int         rc;
    cfg_handle  handle;


    /* Prepare OID */
    sprintf(oid, cfg_oid_ta_vlan_fmt, ta_name, vid);

    rc = cfg_find_str(oid, &handle);
    if (rc == 0)
    {
        rc = cfg_del_instance(handle, FALSE);
        if (rc != 0)
        {
            ERROR("Delete of VLAN %u on TA %s failed(0x%x)",
                vid, ta_name, rc);
        }
    }
    else
    {
        ERROR("VLAN %u on TA %s not found (error 0x%x)",
            vid, ta_name, rc);
    }

    return rc;
}

/**
 * Add port to VLAN on switch.
 *
 * @param ta_name   - name of the test agent
 * @param vid       - VLAN identifier
 * @param port      - port number
 *
 * @return Status code.
 */
int
tapi_cfg_switch_vlan_add_port(const char *ta_name, uint16_t vid,
                              unsigned int port)
{
    /* 
     * Format string without specifiers + TA name + maximum length
     * of printed VID + maximum length of printed port + \0'
     */
    char        oid[strlen(cfg_oid_ta_vlan_port_fmt) - 4 +
                    strlen(ta_name) + 5 + 10 + 1];
    int         rc;
    cfg_handle  handle;


    /* Prepare OID */
    sprintf(oid, cfg_oid_ta_vlan_port_fmt, ta_name, vid, port);

    rc = cfg_find_str(oid, &handle);
    if (rc == 0)
    {
        VERB("Port %u already in VLAN %u on TA %s", port, vid, ta_name);
        rc = EEXIST;
    }
    else
    {
        rc = cfg_add_instance_str(oid, &handle, CVT_NONE);
        if (rc != 0)
        {
            ERROR("Addition of port %u to VLAN %u on TA %s failed(0x%x)",
                port, vid, ta_name, rc);
        }
    }

    return rc;
}

/**
 * Delete port from VLAN on switch.
 *
 * @param ta_name   - name of the test agent
 * @param vid       - VLAN identifier
 * @param port      - port number
 *
 * @return Status code.
 */
int
tapi_cfg_switch_vlan_del_port(const char *ta_name, uint16_t vid,
                              unsigned int port)
{
    /* 
     * Format string without specifiers + TA name + maximum length
     * of printed VID + maximum length of printed port + \0'
     */
    char        oid[strlen(cfg_oid_ta_vlan_port_fmt) - 4 +
                    strlen(ta_name) + 5 + 10 + 1];
    int         rc;
    cfg_handle  handle;


    /* Prepare OID */
    sprintf(oid, cfg_oid_ta_vlan_port_fmt, ta_name, vid, port);

    rc = cfg_find_str(oid, &handle);
    if (rc == 0)
    {
        rc = cfg_del_instance(handle, FALSE);
        if (rc != 0)
        {
            ERROR("Delete of port %u from VLAN %u on TA %s failed(0x%x)",
                port, vid, ta_name, rc);
        }
    }
    else
    {
        ERROR("Port %u not in VLAN %u on TA %s (error 0x%x)",
            port, vid, ta_name, rc);
    }

    return rc;
}

/*
 * END of Temporaraly located here
 */

/**
 * Parses instance name and converts its value into routing table entry 
 * data structure.
 *
 * @param inst_name  Instance name that keeps route information
 * @param rt         Routing entry location to be filled in (OUT)
 *
 * @note The function is not thread safe - it uses static memory for 
 * 'rt_dev' field in 'rt' structure.
 *
 * ATENTION - read the text below!
 * @todo This function is used in both places agent/linux/linuxconf.c
 * and lib/tapi/tapi_cfg.c, which is very ugly!
 * We couldn't find the right place to put it in so that it 
 * is accessible from both places. If you modify it you should modify
 * the same function in the second place!
 */
static int
route_parse_inst_name(const char *inst_name,
#ifdef __linux__
                      struct rtentry  *rt
#else
                      struct ortentry *rt
#endif
)
{
    char        *tmp, *tmp1;
    int          prefix;
    static char  ifname[IF_NAMESIZE];
    char        *ptr;
    char        *end_ptr;
    char        *term_byte; /* Pointer to the trailing zero byte 
                               in 'inst_name' */
    static char  inst_copy[RCF_MAX_VAL];
    int          int_val;

    memset(rt, 0, sizeof(*rt));
    strncpy(inst_copy, inst_name, sizeof(inst_copy));
    inst_copy[sizeof(inst_copy) - 1] = '\0';

    if ((tmp = strchr(inst_copy, '|')) == NULL)
        return TE_RC(TE_TA_LINUX, ETENOSUCHNAME);

    *tmp = 0;
    rt->rt_dst.sa_family = AF_INET;
    if (inet_pton(AF_INET, inst_copy,
                  &(((struct sockaddr_in *)&(rt->rt_dst))->sin_addr)) <= 0)
    {
        ERROR("Incorrect 'destination address' value in route %s",
              inst_name);
        return TE_RC(TE_TA_LINUX, ETENOSUCHNAME);
    }
    tmp++;
    if (*tmp == '-' ||
        (prefix = strtol(tmp, &tmp1, 10), tmp == tmp1 || prefix > 32))
    {
        ERROR("Incorrect 'prefix length' value in route %s", inst_name);
        return TE_RC(TE_TA_LINUX, ETENOSUCHNAME);
    }
    tmp = tmp1;

#ifdef __linux__
    rt->rt_genmask.sa_family = AF_INET;
    ((struct sockaddr_in *)&(rt->rt_genmask))->sin_addr.s_addr =
        htonl(PREFIX2MASK(prefix));
#endif
    if (prefix == 32)
        rt->rt_flags |= RTF_HOST;

    term_byte = (char *)(tmp + strlen(tmp));

    /* @todo Make a macro to wrap around similar code below */
    if ((ptr = strstr(tmp, "gw=")) != NULL)
    {
        int rc;

        end_ptr = ptr += strlen("gw=");
        while (*end_ptr != ',' && *end_ptr != '\0')
            end_ptr++;
        *end_ptr = '\0';

        rc = inet_pton(AF_INET, ptr,
                       &(((struct sockaddr_in *)
                               &(rt->rt_gateway))->sin_addr));
        if (rc <= 0)
        {
            ERROR("Incorrect format of 'gateway address' value in route %s",
                  inst_name);
            return TE_RC(TE_TA_LINUX, ETENOSUCHNAME);
        }
        if (term_byte != end_ptr)
            *end_ptr = ',';
        rt->rt_gateway.sa_family = AF_INET;
        rt->rt_flags |= RTF_GATEWAY;
    }

    if ((ptr = strstr(tmp, "dev=")) != NULL)
    {
        end_ptr = ptr += strlen("dev=");
        while (*end_ptr != ',' && *end_ptr != '\0')
            end_ptr++;
        *end_ptr = '\0';

        if (strlen(ptr) >= sizeof(ifname))
        {
            ERROR("Interface name is too long: %s in route %s",
                  ptr, inst_name);
            return TE_RC(TE_TA_LINUX, EINVAL);
        }
        strcpy(ifname, ptr);

        if (term_byte != end_ptr)
            *end_ptr = ',';
        rt->rt_dev = ifname;
    }

    if ((ptr = strstr(tmp, "metric=")) != NULL)
    {
        end_ptr = ptr += strlen("metric=");
        while (*end_ptr != ',' && *end_ptr != '\0')
            end_ptr++;
        *end_ptr = '\0';
        
        if (*ptr == '\0' || *ptr == '-' ||
            (int_val = strtol(ptr, &end_ptr, 10), *end_ptr != '\0'))
        {
            ERROR("Incorrect 'route metric' value in route %s",
                  inst_name);
            return TE_RC(TE_TA_LINUX, EINVAL);
        }
        if (term_byte != end_ptr)
            *end_ptr = ',';
        rt->rt_metric = int_val;
    }
    
    if ((ptr = strstr(tmp, "mss=")) != NULL)
    {
        end_ptr = ptr += strlen("mss=");
        while (*end_ptr != ',' && *end_ptr != '\0')
            end_ptr++;
        *end_ptr = '\0';

        if (*ptr == '\0' || *ptr == '-' ||
            (int_val = strtol(ptr, &end_ptr, 10), *end_ptr != '\0'))
        {
            ERROR("Incorrect 'route mss' value in route %s", inst_name);
            return TE_RC(TE_TA_LINUX, EINVAL);
        }
        if (term_byte != end_ptr)
            *end_ptr = ',';

        /* Don't be confused the structure does not have mss field */
        rt->rt_mtu = int_val;
        rt->rt_flags |= RTF_MSS;
    }

    if ((ptr = strstr(tmp, "window=")) != NULL)
    {
        end_ptr = ptr += strlen("window=");
        while (*end_ptr != ',' && *end_ptr != '\0')
            end_ptr++;
        *end_ptr = '\0';

        if (*ptr == '\0' ||  *ptr == '-' ||
            (int_val = strtol(ptr, &end_ptr, 10), *end_ptr != '\0'))
        {
            ERROR("Incorrect 'route window' value in route %s", inst_name);
            return TE_RC(TE_TA_LINUX, EINVAL);
        }
        if (term_byte != end_ptr)
            *end_ptr = ',';
        rt->rt_window = int_val;
        rt->rt_flags |= RTF_WINDOW;
    }

    if ((ptr = strstr(tmp, "irtt=")) != NULL)
    {
        end_ptr = ptr += strlen("irtt=");
        while (*end_ptr != ',' && *end_ptr != '\0')
            end_ptr++;
        *end_ptr = '\0';

        if (*ptr == '\0' || *ptr == '-' ||
            (int_val = strtol(ptr, &end_ptr, 10), *end_ptr != '\0'))
        {
            ERROR("Incorrect 'route irtt' value in route %s", inst_name);
            return TE_RC(TE_TA_LINUX, EINVAL);
        }
        if (term_byte != end_ptr)
            *end_ptr = ',';
        rt->rt_irtt = int_val;
        rt->rt_flags |= RTF_IRTT;
    }

    if (strstr(tmp, "reject") != NULL)
        rt->rt_flags |= RTF_REJECT;

    return 0;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_get_route_table(const char *ta, int addr_family,
                         tapi_rt_entry_t **rt_tbl, int *n)
{
#ifdef __linux__
    struct rtentry   rt;
#else
    struct ortentry  rt;
#endif
    int              rc;
    cfg_handle      *handles;
    tapi_rt_entry_t *tbl;
    char            *rt_name = NULL;
    char            *rt_val = NULL;
    int              num;
    int              i;
    
    UNUSED(addr_family);

    if (ta == NULL || rt_tbl == NULL || n == NULL)
        return TE_RC(TE_TAPI, EINVAL);

    rc = cfg_find_pattern_fmt(&num, &handles, "/agent:%s/route:*", ta);
    if (rc != 0)
        return rc;
    
    if (num == 0)
    {
        *rt_tbl = NULL;
        *n = 0;
        return 0;
    }
    
    if ((tbl = (tapi_rt_entry_t *)calloc(num, sizeof(*tbl))) == NULL)
    {
        return TE_RC(TE_TAPI, ENOMEM);
    }

    for (i = 0; i < num; i++)
    {
        uint32_t     mask;
        cfg_val_type type = CVT_STRING;

        if ((rc = cfg_get_inst_name(handles[i], &rt_name)) != 0)
        {
            ERROR("%s: Route handle cannot be processed", __FUNCTION__);
            free(tbl);
            return rc;
        }
        if ((rc = cfg_get_instance(handles[i], &type, &rt_val)) != 0)
        {
            ERROR("%s: Cannot get the value of the route", __FUNCTION__);
            free(rt_name);
            free(tbl);
            return rc;
        }
        memset(&rt, 0, sizeof(rt));

        rt.rt_dev = tbl[i].dev;

        rc = route_parse_inst_name(rt_name, &rt);
        assert(rc == 0);

        memcpy(&tbl[i].dst, &rt.rt_dst, sizeof(rt.rt_dst));

        mask = ntohl(((struct sockaddr_in *)
                         &(rt.rt_genmask))->sin_addr.s_addr);
        MASK2PREFIX(mask, tbl[i].prefix);
        memcpy(&tbl[i].gw, &rt.rt_gateway, sizeof(rt.rt_gateway));

        tbl[i].flags = rt.rt_flags;
        tbl[i].metric = rt.rt_metric;
        tbl[i].mss = rt.rt_mtu;
        tbl[i].win = rt.rt_window;
        tbl[i].irtt = rt.rt_irtt;

        if (strstr(rt_val, "mod") != NULL)
            tbl[i].flags |= RTF_MODIFIED;
        if (strstr(rt_val, "dyn") != NULL)
            tbl[i].flags |= RTF_DYNAMIC;
        if (strstr(rt_val, "reinstate") != NULL)
            tbl[i].flags |= RTF_REINSTATE;

        tbl[i].hndl = handles[i];

        free(rt_name);
        free(rt_val);
    }
    free(handles);

    *rt_tbl = tbl;
    *n = num;

    return 0;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_add_route(const char *ta, int addr_family,
                   const void *dst_addr, int prefix,
                   const void *gw_addr, const char *dev,
                   uint32_t flags, int metric, int mss, int win, int irtt,
                   cfg_handle *cfg_hndl)
{
    return tapi_cfg_route_op(OP_ADD, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev,
                             flags, metric, mss, win, irtt, cfg_hndl);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_del_route_tmp(const char *ta, int addr_family,
                       const void *dst_addr, int prefix, 
                       const void *gw_addr, const char *dev,
                       uint32_t flags, int metric,
                       int mss, int win, int irtt)
{
    return tapi_cfg_route_op(OP_DEL, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev,
                             flags, metric, mss, win, irtt, NULL);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_del_route(cfg_handle *rt_hndl)
{
    int rc;

    if (rt_hndl == NULL)
        return EINVAL;

    if (*rt_hndl == CFG_HANDLE_INVALID)
        return 0;
    
    rc = cfg_del_instance(*rt_hndl, FALSE);
    if (rc == 0)
    {
        *rt_hndl = CFG_HANDLE_INVALID;
    }

    return rc;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_add_arp_entry(const char *ta,
                       const void *net_addr, const void *link_addr)
{
    return tapi_cfg_arp_op(OP_ADD, ta, net_addr, link_addr);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_del_arp_entry(const char *ta, const void *net_addr)
{
    return tapi_cfg_arp_op(OP_DEL, ta, net_addr, NULL);
}

int
tapi_cfg_del_arp_dynamic(const char *ta)
{
    cfg_handle *hndls = NULL;
    int         num;
    int         i;
    int         rc;

    if ((rc = cfg_find_pattern_fmt(&num, &hndls,
                                   "/agent:%s/volatile:/arp:*", ta)) != 0)
    {
        return rc;
    }
    for (i = 0; i < num; i++)
    {
        if ((rc = cfg_del_instance(hndls[i], FALSE)) != 0)
        {
            return rc;
        }
    }

    free(hndls);

    return 0;
}

/**
 * Perform specified operation with routing table
 * 
 * @param op            Operation
 * @param ta            Test agent
 * @param addr_family   Address family 
 * @param dst_addr      Destination address of the route
 * @param prefix        Prefix for dst_addr
 * @param gw_addr       Gateway address of the route
 * @param cfg_hndl      Handle of the route in Configurator DB (OUT)
 *
 * @return Status code
 *
 * @retval 0  on success
 */                   
static int
tapi_cfg_route_op(enum tapi_cfg_oper op, const char *ta, int addr_family,
                  const void *dst_addr, int prefix, const void *gw_addr,
                  const char *dev, uint32_t flags,
                  int metric, int mss, int win, int irtt,
                  cfg_handle *cfg_hndl)
{
    cfg_handle  handle;
    char        dst_addr_str[INET6_ADDRSTRLEN];
    char        dst_addr_str_orig[INET6_ADDRSTRLEN];
    char        gw_addr_str[INET6_ADDRSTRLEN];
    char        route_inst_name[1024];
    char        rt_val[128];
    int         rc;
    int         netaddr_size = netaddr_get_size(addr_family);
    uint8_t    *dst_addr_copy;
    int         i;
    int         diff;
    uint8_t     mask;

    if (netaddr_size < 0)
    {
        ERROR("%s() unknown address family value", __FUNCTION__);
        return TE_RC(TE_TAPI, EINVAL);
    }
    
    if (prefix < 0 || prefix > (netaddr_size << 3))
    {
        ERROR("%s() fails: Incorrect prefix value specified %d", prefix);
        return TE_RC(TE_TAPI, EINVAL);
    }

    if ((dst_addr_copy = (uint8_t *)malloc(netaddr_size)) == NULL)
    {
        ERROR("%s() cannot allocate %d bytes for the copy of "
              "the network address", __FUNCTION__,
              netaddr_get_size(addr_family));

        return TE_RC(TE_TAPI, ENOMEM);
    }

    if (inet_ntop(addr_family, dst_addr, dst_addr_str_orig, 
                  sizeof(dst_addr_str_orig)) == NULL)
    {
        ERROR("%s() fails converting binary destination address "
              "into a character string", __FUNCTION__);
        return TE_RC(TE_TAPI, errno);
    }

    memcpy(dst_addr_copy, dst_addr, netaddr_size);
    
    /* Check that dst_addr & netmask == dst_addr */
    for (i = 0; i < netaddr_size; i++)
    {
        diff = ((i + 1) << 3) - prefix;
        
        if (diff < 0)
        {
            /* i-th byte is fully under the mask, so skip it */
            continue;
        }
        if (diff < 8)
            mask = 0xff << diff;
        else
            mask = 0;
        
        if ((dst_addr_copy[i] & mask) != dst_addr_copy[i])
        {
            dst_addr_copy[i] &= mask;
        }
    }
    if (memcmp(dst_addr, dst_addr_copy, netaddr_size) != 0)
    {
        inet_ntop(addr_family, dst_addr_copy, dst_addr_str, 
                  sizeof(dst_addr_str));

        WARN("Destination address specified in the route does not "
             "cleared according to the prefix: prefix length %d, "
             "addr: %s expected to be %s. "
             "[The address is cleared anyway]",
             prefix, dst_addr_str_orig, dst_addr_str);
    }

    if (inet_ntop(addr_family, dst_addr_copy, dst_addr_str, 
                  sizeof(dst_addr_str)) == NULL)
    {
        ERROR("%s() fails converting binary destination address "
              "into a character string", __FUNCTION__);
        free(dst_addr_copy);
        return TE_RC(TE_TAPI, errno);
    }
    free(dst_addr_copy);
    
#define PUT_INTO_BUF(buf_, args...) \
    snprintf((buf_) + strlen(buf_), sizeof(buf_) - strlen(buf_), args)

    route_inst_name[0] = '\0';
    PUT_INTO_BUF(route_inst_name, "%s|%d", dst_addr_str, prefix);

    if (gw_addr != NULL)
    {
        if (inet_ntop(addr_family, gw_addr, gw_addr_str,
                      sizeof(gw_addr_str)) == NULL)
        {
            ERROR("%s() fails converting binary gateway address "
                  "into a character string", __FUNCTION__);
            return TE_RC(TE_TAPI, errno);
        }
        PUT_INTO_BUF(route_inst_name, ",gw=%s", gw_addr_str);
    }
    if (dev != NULL)
        PUT_INTO_BUF(route_inst_name, ",dev=%s", dev);
    if (metric != 0)
        PUT_INTO_BUF(route_inst_name, ",metric=%d", metric);
    if (mss != 0)
        PUT_INTO_BUF(route_inst_name, ",mss=%d", mss);
    if (win != 0)
        PUT_INTO_BUF(route_inst_name, ",window=%d", win);
    if (irtt != 0)
        PUT_INTO_BUF(route_inst_name, ",irtt=%d", irtt);
    if (flags & RTF_REJECT)
        PUT_INTO_BUF(route_inst_name, ",reject");

    rt_val[0] = '\0';

    if (flags & RTF_MODIFIED)
        PUT_INTO_BUF(rt_val, " mod");
    if (flags & RTF_DYNAMIC)
        PUT_INTO_BUF(rt_val, " dyn");
    if (flags & RTF_REINSTATE)
        PUT_INTO_BUF(rt_val, " reinstate");

    switch (op)
    {
        case OP_ADD:
            if ((rc = cfg_add_instance_fmt(&handle, CVT_STRING, rt_val,
                                           "/agent:%s/route:%s",
                                           ta, route_inst_name)) != 0)
            {
                ERROR("%s() fails adding a new route %s %s on '%s' Agent "
                      "errno = %X", __FUNCTION__, route_inst_name,
                      rt_val, ta, rc);
                break;
            }
            if (cfg_hndl != NULL)
                *cfg_hndl = handle;
            break;

        case OP_DEL:
            if ((rc = cfg_del_instance_fmt(FALSE, "/agent:%s/route:%s",
                                           ta, route_inst_name)) != 0)
            {
                ERROR("%s() fails deleting route %s on '%s' Agent "
                      "errno = %X", __FUNCTION__, route_inst_name, ta, rc);
            }
            break;

        default:
            ERROR("%s(): Operation %d is not supported", __FUNCTION__, op);
            rc = TE_RC(TE_TAPI, EOPNOTSUPP);
            break;
    }

#undef PUT_INTO_BUF

    return rc;
}

/**
 * Perform specified operation with ARP table
 * 
 * @param op            Operation
 * @param ta            Test agent
 * @param net_addr      Network address
 * @param link_addr     Link-leyer address
 *
 * @return Status code
 *
 * @retval 0  on success
 */                   
static int
tapi_cfg_arp_op(enum tapi_cfg_oper op, const char *ta,
                const void *net_addr, const void *link_addr)
{
    cfg_handle      handle;
    char            net_addr_str[INET_ADDRSTRLEN];
    int             rc;

    if (inet_ntop(AF_INET, net_addr, net_addr_str, 
                  sizeof(net_addr_str)) == NULL)
    {
        ERROR("%s() fails converting binary IPv4 address  "
              "into a character string", __FUNCTION__);
        return TE_RC(TE_TAPI, errno);
    }

    switch (op)
    {
        case OP_ADD:
        {
            struct sockaddr lnk_addr;

            memset(&lnk_addr, 0, sizeof(lnk_addr));
            lnk_addr.sa_family = AF_LOCAL;
            memcpy(&(lnk_addr.sa_data), link_addr, IFHWADDRLEN);

            if ((rc = cfg_add_instance_fmt(&handle, CVT_ADDRESS, &lnk_addr,
                                           "/agent:%s/arp:%s",
                                           ta, net_addr_str)) != 0)
            {
                /* TODO Correct formating */
                ERROR("%s() fails adding a new ARP entry "
                      "%s -> %x:%x:%x:%x:%x:%x on TA '%s'",
                      __FUNCTION__, net_addr_str,
                      *(((uint8_t *)link_addr) + 0),
                      *(((uint8_t *)link_addr) + 1),
                      *(((uint8_t *)link_addr) + 2),
                      *(((uint8_t *)link_addr) + 3),
                      *(((uint8_t *)link_addr) + 4),
                      *(((uint8_t *)link_addr) + 5), ta);
            }
            break;
        }
            
        case OP_DEL:
            if ((rc = cfg_find_fmt(&handle, "/agent:%s/arp:%s",
                          ta, net_addr_str) == TE_RC(TE_CS, ENOENT)))
            {
                RING("There is no ARP entry for %s address on %s Agent",
                     net_addr_str, ta);
                rc = 0;
                break;
            }
            if (rc != 0)
            {
                ERROR("%s() fails finding '/agent:%s/arp:%s' instance "
                      "with errno %X", __FUNCTION__, ta, net_addr_str, rc);
                break;
            }

            if ((rc = cfg_del_instance_fmt(FALSE, "/agent:%s/arp:%s",
                                           ta, net_addr_str)) != 0)
            {
                ERROR("%s() fails deleting ARP entry for %s host "
                      "on TA '%s'", __FUNCTION__, net_addr_str, ta);
            }
            break;

        default:
            ERROR("%s(): Operation %d is not supported", __FUNCTION__, op);
            rc = TE_RC(TE_TAPI, EOPNOTSUPP);
            break;
    }

    return rc;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_get_hwaddr(const char *ta,
                    const char *ifname,
                    void *hwaddr, unsigned int *hwaddr_len)
{
    char     buf[1024];
    int      rc;
    char   *ifname_bkp;
    char   *ptr;

    if (hwaddr == NULL || hwaddr_len == NULL)
    {
        ERROR("%s(): It is not allowed to have NULL 'hwaddr' or "
              "'hwaddr_len' parameter", __FUNCTION__);
        return TE_RC(TE_TAPI, EINVAL);
    }
    if (*hwaddr_len < IFHWADDRLEN)
    {
        ERROR("%s(): 'hwaddr' is too short");
        return TE_RC(TE_TAPI, EMSGSIZE);
    }

    /**
     * Configuration model does not support alias interfaces,
     * so that we should truncate trailing :XX part from the interface name.
     */
    if ((ifname_bkp = (char *)malloc(strlen(ifname) + 1)) == NULL)
    {
        return TE_RC(TE_TAPI, ENOMEM);
    }
    memcpy(ifname_bkp, ifname, strlen(ifname) + 1);

    if ((ptr = strchr(ifname_bkp, ':')) != NULL)
        *ptr = '\0';

    snprintf(buf, sizeof(buf), "/agent:%s/interface:%s",
             ta, ifname_bkp);
    if ((rc = tapi_cfg_base_if_get_mac(buf, hwaddr)) != 0)
    {
        return rc;
    }

    return 0;
}


/* See the description in tapi_cfg.h */
static int
tapi_cfg_alloc_entry_by_handle(cfg_handle parent, cfg_handle *entry)
{
    int             rc;
    cfg_handle      handle;
    cfg_val_type    type = CVT_INTEGER;
    int             value;

    if (entry == NULL)
    {
        ERROR("%s(): Invalid argument", __FUNCTION__);
        return TE_RC(TE_TAPI, EINVAL);
    }

    *entry = CFG_HANDLE_INVALID;

    for (rc = cfg_get_son(parent, &handle);
         rc == 0 && handle != CFG_HANDLE_INVALID;
         rc = cfg_get_brother(handle, &handle))
    {
        rc = cfg_get_instance(handle, &type, &value);
        if (rc != 0)
        {
            ERROR("%s: Failed to get integer value by handle 0x%x: %X",
                  __FUNCTION__, handle, rc);
            break;
        }
        if (value == 0)
        {
            rc = cfg_set_instance(handle, type, 1);
            if (rc != 0)
            {
                ERROR("%s: Failed to set value of handle 0x%x to 1: %X",
                      __FUNCTION__, handle, rc);
            }
            break;
        }
    }

    if (rc == 0)
    {
        if (handle != CFG_HANDLE_INVALID)
        {
            *entry = handle;
            INFO("Pool 0x%x entry with Cfgr handle 0x%x allocated",
                 parent, *entry);
        }
        else
        {
            INFO("No free entries in pool 0x%x", parent);
            rc = TE_RC(TE_TAPI, ENOENT);
        }
    }
#if 0
    else if (TE_RC_GET_ERROR(rc) == ENOENT)
    {
        INFO("No free entries in pool 0x%x", parent);
    }
#endif
    else
    {
        ERROR("Failed to allocate entry in 0x%x: %X", parent, rc);
    }

    return rc;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_alloc_entry(const char *parent_oid, cfg_handle *entry)
{
    int         rc;
    cfg_handle  parent;

    rc = cfg_find_str(parent_oid, &parent);
    if (rc != 0)
    {
        ERROR("%s: Failed to convert OID '%s' to handle: %X",
              __FUNCTION__, parent_oid, rc);
        return rc;
    }
    
    return tapi_cfg_alloc_entry_by_handle(parent, entry);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_free_entry(cfg_handle *entry)
{
    int rc;

    if (entry == NULL)
    {
        ERROR("%s(): Invalid Cfgr handle pointer", __FUNCTION__);
        return TE_RC(TE_TAPI, EINVAL);
    }
    if (*entry == CFG_HANDLE_INVALID)
    {
        return 0;
    }

    rc = cfg_set_instance(*entry, CVT_INTEGER, 0);
    if (rc != 0)
    {
    ERROR("Failed to free entry by handle 0x%x: %X", *entry, rc);
    }
    else
    {
        INFO("Pool entry with Cfgr handle 0x%x freed", *entry);
        *entry= CFG_HANDLE_INVALID;
    }
    return rc;
}


/* See the description in tapi_cfg.h */
int
tapi_cfg_alloc_ip4_addr(cfg_handle ip4_net, cfg_handle *p_entry,
                        struct sockaddr_in **addr)
{
    int             rc;
    char           *ip4_net_oid;
    cfg_handle      pool;
    cfg_handle      entry;
    int             n_entries;
    cfg_handle      n_entries_hndl;
    cfg_val_type    val_type;
    int             prefix;
    char            buf[INET_ADDRSTRLEN];


    rc = cfg_get_oid_str(ip4_net, &ip4_net_oid);
    if (rc != 0)
    {
        ERROR("Failed to get OID by handle 0x%x: %X", ip4_net, rc);
        return rc;
    }

    /* Find or create pool of IPv4 subnet addresses */
    rc = cfg_find_fmt(&pool, "%s/pool:", ip4_net_oid);
    if (TE_RC_GET_ERROR(rc) == ENOENT)
    {
        rc = cfg_add_instance_fmt(&pool, CVT_NONE, NULL,
                                  "%s/pool:", ip4_net_oid);
        if (rc != 0)
        {
            ERROR("Failed to add object instance '%s/pool:': %X",
                  ip4_net_oid, rc);
            free(ip4_net_oid);
            return rc;
        }
    }
    else if (rc != 0)
    {
        ERROR("Failed to find object instance '%s/pool:': %X",
              ip4_net_oid, rc);
        free(ip4_net_oid);
        return rc;
    }
    
    rc = tapi_cfg_alloc_entry_by_handle(pool, &entry);
    if (TE_RC_GET_ERROR(rc) != ENOENT)
    {
        free(ip4_net_oid);
        if (rc == 0)
        {
            /* Get address */
            rc = cfg_get_inst_name_type(entry, CVT_ADDRESS,
                                        (cfg_inst_val *)addr);
            if (rc != 0)
            {
                ERROR("Failed to get IPv4 address as instance name of "
                      "0x%x: %X", entry, rc);
            }
            else if (p_entry != NULL)
            {
                *p_entry = entry;
            }
        }
        return rc;
    }
    
    /* No available entries */

    /* Get number of entries in the pool */
    rc = cfg_find_fmt(&n_entries_hndl, "%s/n_entries:", ip4_net_oid);
    if (TE_RC_GET_ERROR(rc) == ENOENT)
    {
        rc = cfg_add_instance_fmt(&n_entries_hndl, CVT_INTEGER, (void *)0,
                                  "%s/n_entries:", ip4_net_oid);
        if (rc != 0)
        {
            ERROR("Failed to add object instance '%s/n_entries:': %X",
                  ip4_net_oid, rc);
            free(ip4_net_oid);
            return rc;
        }
        n_entries = 0;
    }
    else if (rc != 0)
    {
        ERROR("Failed to find object instance '%s/n_entries:': %X",
              ip4_net_oid, rc);
        free(ip4_net_oid);
        return rc;
    }
    else
    {
        rc = cfg_get_instance(n_entries_hndl, CVT_INTEGER, &n_entries);
        if (rc != 0)
        {
            ERROR("Failed to get number of entries in the pool: %X",
                  rc);
            free(ip4_net_oid);
            return rc;
        }
    }

    /* Create one more entry */
    n_entries++;

    /* Get network prefix length */
    val_type = CVT_INTEGER;
    rc = cfg_get_instance_fmt(&val_type, &prefix,
                              "%s/prefix:", ip4_net_oid);
    if (rc != 0)
    {
        ERROR("Failed to get prefix length of '%s': %X",
              ip4_net_oid, rc);
        free(ip4_net_oid);
        return rc;
    }

    /* Check for sufficient space */
    assert((prefix >= 0) && (prefix <= 32));
    if (n_entries > ((1 << ((sizeof(struct in_addr) << 3) - prefix)) - 2))
    {
        ERROR("All addresses of the subnet '%s' are used",
              ip4_net_oid);
        free(ip4_net_oid);
        return TE_RC(TE_TAPI, ENOENT);
    }

    /* Update number of entries ASAP */
    rc = cfg_set_instance(n_entries_hndl, CVT_INTEGER, n_entries);
    if (rc != 0)
    {
        ERROR("Failed to get number of entries in the pool: %X",
              rc);
        free(ip4_net_oid);
        return rc;
    }

    /* Get subnet address */
    rc = cfg_get_inst_name_type(ip4_net, CVT_ADDRESS,
                                (cfg_inst_val *)addr);
    if (rc != 0)
    {
        ERROR("Failed to get IPv4 subnet address from '%s': %X",
              ip4_net_oid, rc);
        free(ip4_net_oid);
        return rc;
    }

    /* Make address from subnet address */
    (*addr)->sin_addr.s_addr =
        htonl(ntohl((*addr)->sin_addr.s_addr) + n_entries);

    /* Add used entry in the pool */
    rc = cfg_add_instance_fmt(&entry, CVT_INTEGER, (void *)1,
                              "%s/pool:/entry:%s", ip4_net_oid,
                              inet_ntop(AF_INET, &(*addr)->sin_addr,
                                        buf, sizeof(buf)));
    if (rc != 0)
    {
        ERROR("Failed to add entry in IPv4 subnet pool '%s': %X",
              ip4_net_oid, rc);
        free(ip4_net_oid);
        return rc;
    }
    free(ip4_net_oid);

    if (p_entry != NULL)
        *p_entry = entry;

    return 0;
}
