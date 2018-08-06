/** @file
 * @brief Test API to access Configurator
 *
 * Implementation
 *
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights reserved.
 *
 * 
 *
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 * @author Oleg Kravtsov <Oleg.Kravtsov@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "Configuration TAPI"

#include "te_config.h"

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
#include "te_string.h"

#include "tapi_sockaddr.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg.h"

/** Operations with routing/ARP table */
enum tapi_cfg_oper {
    OP_ADD,   /**< Add operation */
    OP_DEL,   /**< Delete operation */
    OP_GET,   /**< Get operation */
    OP_MODIFY /**< Modify operation */
};

/* Forward declarations */
static int tapi_cfg_route_op(enum tapi_cfg_oper op, const char *ta,
                             int addr_family,
                             const void *dst_addr, int prefix,
                             const void *gw_addr, const char *dev,
                             const void *src_addr, const char *type,
                             uint32_t flags, int metric, int tos, int mtu,
                             int win, int irtt, int table,
                             tapi_cfg_rt_nexthop *hops, size_t hops_num,
                             cfg_handle *cfg_hndl);

static int tapi_cfg_neigh_op(enum tapi_cfg_oper op, const char *ta,
                             const char *ifname,
                             const struct sockaddr *net_addr,
                             const void *link_addr,
                             void *ret_addr, te_bool *is_static,
                             cs_neigh_entry_state *state);


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
            VERB("cfg_get_instance() failed(%r)", rc);
            EXIT("%r", rc);
            return rc;
        }
        if (state != 1)
        {
            ERROR("VLAN %u disabled on TA %s", vid, ta_name);
            EXIT("TE_EENV");
            return TE_EENV;
        }
        rc = TE_EEXIST;
        EXIT("EEXIST");
    }
    else
    {
        VERB("Add instance '%s'", oid);
        rc = cfg_add_instance_str(oid, &handle, CVT_INTEGER, 1);
        if (rc != 0)
        {
            ERROR("Addition of VLAN %u on TA %s failed(%r)",
                vid, ta_name, rc);
        }
        EXIT("%r", rc);
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
            ERROR("Delete of VLAN %u on TA %s failed(%r)",
                vid, ta_name, rc);
        }
    }
    else
    {
        ERROR("VLAN %u on TA %s not found (error %r)",
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
        rc = TE_EEXIST;
    }
    else
    {
        rc = cfg_add_instance_str(oid, &handle, CVT_NONE);
        if (rc != 0)
        {
            ERROR("Addition of port %u to VLAN %u on TA %s failed(%r)",
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
            ERROR("Delete of port %u from VLAN %u on TA %s failed(%r)",
                port, vid, ta_name, rc);
        }
    }
    else
    {
        ERROR("Port %u not in VLAN %u on TA %s (error %r)",
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
 */
static int
route_parse_inst_name(const char *inst_name, tapi_rt_entry_t *rt)
{
    char        *tmp, *tmp1;
    int          prefix;
    char        *ptr;
    static char  inst_copy[RCF_MAX_VAL];

    int          family = (strchr(inst_name, ':') == NULL) ?
                          AF_INET : AF_INET6;

    strncpy(inst_copy, inst_name, sizeof(inst_copy));
    inst_copy[sizeof(inst_copy) - 1] = '\0';

    if ((tmp = strchr(inst_copy, '|')) == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    *tmp ='\0';
    rt->dst.ss_family = family;

    if (inet_pton(family, inst_copy,
                  te_sockaddr_get_netaddr(SA(&(rt->dst)))) <= 0)
    {
        ERROR("Incorrect 'destination address' value in route %s",
              inst_name);
        return TE_RC(TE_TAPI, TE_ENOENT);
    }
    tmp++;
    if (*tmp == '-' || (prefix = strtol(tmp, &tmp1, 10), tmp == tmp1) ||
        ((unsigned)prefix > (te_netaddr_get_size(rt->dst.ss_family) << 3)))
    {
        ERROR("Incorrect 'prefix length' value in route %s", inst_name);
        return TE_RC(TE_TAPI, TE_ENOENT);
    }
    tmp = tmp1;

    rt->prefix = prefix;

    if ((ptr = strstr(tmp, "metric=")) != NULL)
    {
        ptr += strlen("metric=");
        rt->metric = atoi(ptr);
        rt->flags |= TAPI_RT_METRIC;
    }

    if ((ptr = strstr(tmp, "tos=")) != NULL)
    {
        ptr += strlen("tos=");
        rt->metric = atoi(ptr);
        rt->flags |= TAPI_RT_TOS;
    }

    if ((ptr = strstr(tmp, "table=")) != NULL)
    {
        ptr += strlen("table=");
        rt->table = atoi(ptr);
        rt->flags |= TAPI_RT_TABLE;
    }
    else
        rt->table = TAPI_RT_TABLE_MAIN;

    return 0;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_get_route_table(const char *ta, int addr_family,
                         tapi_rt_entry_t **rt_tbl, unsigned int *n)
{
    int              rc;
    cfg_handle       handle1;
    cfg_handle       handle2;
    cfg_handle      *handles;
    tapi_rt_entry_t *tbl;
    char            *rt_name = NULL;
    unsigned int     num, rt_num = 0;
    unsigned int     i, j;
    struct sockaddr  *addr;

    if (ta == NULL || rt_tbl == NULL || n == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    rc = cfg_find_pattern_fmt(&num, &handles, "/agent:%s/route:*", ta);
    if (rc != 0)
        return rc;

    for (i = 0; i < num; i++)
    {
        if ((rc = cfg_get_instance(handles[i], NULL, &addr)) != 0)
        {
            ERROR("%s: Cannot obtain route instance value", __FUNCTION__);
            free(handles);
            return rc;
        }

        if (addr->sa_family == addr_family)
            rt_num++;
    }

    if (rt_num == 0)
    {
        *rt_tbl = NULL;
        *n = 0;
        free(handles);
        return 0;
    }

    if ((tbl = (tapi_rt_entry_t *)calloc(rt_num, sizeof(*tbl))) == NULL)
    {
        free(handles);
        return TE_RC(TE_TAPI, TE_ENOMEM);
    }

    i = 0;
    for (j = 0; j < num; j++)
    {
        if ((rc = cfg_get_instance(handles[j], NULL, &addr)) != 0)
        {
            ERROR("%s: Cannot obtain route instance value", __FUNCTION__);
            break;
        }

        if (addr->sa_family != addr_family)
            continue;

        if ((addr->sa_family == AF_INET &&
             SIN(addr)->sin_addr.s_addr != htonl(INADDR_ANY)) ||
            (addr->sa_family == AF_INET6 &&
             !IN6_IS_ADDR_UNSPECIFIED(&SIN6(addr)->sin6_addr)))
        {
            tbl[i].flags |= TAPI_RT_GW;
            memcpy(&tbl[i].gw, addr, sizeof(struct sockaddr_storage));
        }

        free(addr);

        if ((rc = cfg_get_inst_name(handles[j], &rt_name)) != 0)
        {
            ERROR("%s: Route handle cannot be processed", __FUNCTION__);
            break;
        }

        rc = route_parse_inst_name(rt_name, &tbl[i]);

        free(rt_name);

        assert(rc == 0);

        /* Get route attributes */
        if ((rc = cfg_get_son(handles[j], &handle2)) != 0 ||
            handle2 == CFG_HANDLE_INVALID)
        {
            ERROR("%s: Cannot find any attribute of the route %r",
                  __FUNCTION__, rc);
            break;
        }

        do {
            cfg_val_type    type;
            char           *name;
            char           *dev_name;
            cfg_oid        *oid;
            void           *val_p;
            char           *type_val;

            handle1 = handle2;

            if ((rc = cfg_get_oid(handle1, &oid)) != 0)
            {
                ERROR("%s: Cannot get route attribute name %r",
                      __FUNCTION__, rc);
                break;
            }
            name = ((cfg_inst_subid *)
                            (oid->ids))[oid->len - 1].subid;

            type = CVT_INTEGER;
            if (strcmp(name, "dev") == 0)
            {
                val_p = &dev_name;
                type = CVT_STRING;
            }
            else if (strcmp(name, "mtu") == 0)
                val_p = &tbl[i].mtu;
            else if (strcmp(name, "win") == 0)
                val_p = &tbl[i].win;
            else if (strcmp(name, "irtt") == 0)
                val_p = &tbl[i].irtt;
            else if (strcmp(name, "type") == 0)
            {
                val_p = &type_val;
                type = CVT_STRING;
            }
            else if (strcmp(name, "src") == 0)
            {
                type = CVT_ADDRESS;
                tbl[i].flags |= TAPI_RT_SRC;
                val_p = &addr;
            }
            else
            {
                ERROR("%s(): Unknown route attribute found %s",
                      __FUNCTION__, name);
                free(oid);
                rc = TE_RC(TE_TAPI, TE_EINVAL);
                break;
            }

            if ((rc = cfg_get_instance(handle1, &type, val_p)) != 0)
            {
                ERROR("%s(): Cannot get value of %s route attribute %r",
                      __FUNCTION__, name, rc);
                free(oid);
                break;
            }

            if (strcmp(name, "src") == 0)
            {
                memcpy(&tbl[i].src, addr, sizeof(struct sockaddr_storage));
                free(addr);
            }
            else if (strcmp(name, "dev") == 0)
            {
                strncpy(tbl[i].dev, dev_name, sizeof(tbl[i].dev) - 1);
                free(dev_name);
            }
            else if (strcmp(name, "type") == 0)
            {
                strncpy(tbl[i].type, type_val, sizeof(tbl[i].type) - 1);
                free(type_val);
            }

            free(oid);

            rc = cfg_get_brother(handle1, &handle2);
            if (rc != 0)
            {
                ERROR("%s(): Cannot get brother of route attribute %r",
                      __FUNCTION__, rc);
                break;
            }
        } while (handle2 != CFG_HANDLE_INVALID);

        if (rc != 0)
            break;

        assert(handle2 == CFG_HANDLE_INVALID);

        tbl[i].hndl = handles[j];
        i++;
    }
    free(handles);

    if (rc != 0)
    {
        free(tbl);
        return rc;
    }

    *rt_tbl = tbl;
    *n = rt_num;

    return 0;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_add_route(const char *ta, int addr_family,
                   const void *dst_addr, int prefix,
                   const void *gw_addr, const char *dev,
                   const void *src_addr,
                   uint32_t flags, int metric, int tos, int mtu,
                   int win, int irtt, cfg_handle *cfg_hndl)
{
    return tapi_cfg_route_op(OP_ADD, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev, src_addr, NULL,
                             flags, metric, tos, mtu, win, irtt,
                             TAPI_RT_TABLE_MAIN, NULL, 0, cfg_hndl);
}

static int
tapi_cfg_add_blackhole(const char *ta, int addr_family,
                       const void *dst_addr, int prefix,
                       cfg_handle *handle)
{
    char        dst_addr_str[INET6_ADDRSTRLEN];
    int         netaddr_size = te_netaddr_get_size(addr_family);

    if (netaddr_size == 0)
    {
        ERROR("%s() unknown address family value", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if (prefix != netaddr_size * CHAR_BIT)
    {
        ERROR("%s() fails: Incorrect prefix value specified %d "
              "(must be %d for blackhole routes)",
              __FUNCTION__,
              prefix, netaddr_size);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    if (inet_ntop(addr_family, dst_addr, dst_addr_str,
                  sizeof(dst_addr_str)) == NULL)
    {
        ERROR("%s() fails converting binary destination address "
              "into a character string", __FUNCTION__);
        return TE_OS_RC(TE_TAPI, errno);
    }
    return cfg_add_instance_fmt(handle, CFG_VAL(NONE, NULL),
                                "/agent:%s/blackhole:%s|%d",
                                ta, dst_addr_str, prefix);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_add_full_route(const char *ta, int addr_family,
                        const void *dst_addr, int prefix,
                        const void *gw_addr, const char *dev,
                        const void *src_addr, const char *type,
                        uint32_t flags, int metric, int tos, int mtu,
                        int win, int irtt, int table, cfg_handle *cfg_hndl)
{
    if (type != NULL && strcmp(type, "blackhole") == 0)
    {
        return tapi_cfg_add_blackhole(ta, addr_family, dst_addr,
                                      prefix, cfg_hndl);
    }
    if (type != NULL &&
        strcmp(type, "unicast") != 0 &&
        strcmp(type, "local") != 0)
    {
        ERROR("Route type '%s' is not supported yet", type);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    return tapi_cfg_route_op(OP_ADD, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev, src_addr, type,
                             flags, metric, tos, mtu, win, irtt, table,
                             NULL, 0, cfg_hndl);
}



/* See the description in tapi_cfg.h */
int
tapi_cfg_modify_route(const char *ta, int addr_family,
                   const void *dst_addr, int prefix,
                   const void *gw_addr, const char *dev,
                   const void *src_addr,
                   uint32_t flags, int metric, int tos, int mtu,
                   int win, int irtt, cfg_handle *cfg_hndl)
{
    return tapi_cfg_route_op(OP_MODIFY, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev, src_addr, NULL,
                             flags, metric, tos, mtu, win, irtt,
                             TAPI_RT_TABLE_MAIN, NULL, 0, cfg_hndl);
}


/* See the description in tapi_cfg.h */
int
tapi_cfg_modify_full_route(const char *ta, int addr_family,
                            const void *dst_addr, int prefix,
                            const void *gw_addr, const char *dev,
                            const void *src_addr, const char *type,
                            uint32_t flags, int metric, int tos, int mtu,
                            int win, int irtt, int table,
                            cfg_handle *cfg_hndl)
{
    if (type != NULL &&
        strcmp(type, "unicast") != 0 &&
        strcmp(type, "local") != 0)
    {
        ERROR("Route type '%s' is not supported yet", type);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    return tapi_cfg_route_op(OP_MODIFY, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev, src_addr, type,
                             flags, metric, tos, mtu, win, irtt, table,
                             NULL, 0, cfg_hndl);
}

/* See the description in tapi_cfg.h */
void
tapi_cfg_rt_params_init(tapi_cfg_rt_params *params)
{
    memset(params, 0, sizeof(*params));
    params->table = TAPI_RT_TABLE_MAIN;
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_add_route2(const char *ta,
                    tapi_cfg_rt_params *params,
                    cfg_handle *rt_hndl)
{
    int   addr_family = AF_UNSPEC;
    void *dst_addr = NULL;
    void *gw_addr = NULL;
    void *src_addr = NULL;

    if (params->dst_addr != NULL)
    {
        addr_family = params->dst_addr->sa_family;
        dst_addr = te_sockaddr_get_netaddr(params->dst_addr);
    }

    if (params->gw_addr != NULL &&
        params->gw_addr->sa_family != AF_UNSPEC)
    {
        gw_addr = te_sockaddr_get_netaddr(params->gw_addr);
    }

    if (params->src_addr != NULL &&
        params->src_addr->sa_family != AF_UNSPEC)
    {
        src_addr = te_sockaddr_get_netaddr(params->src_addr);
    }

    return tapi_cfg_route_op(OP_ADD, ta, addr_family, dst_addr,
                             params->prefix,
                             gw_addr, params->dev, src_addr,
                             params->type, params->flags,
                             params->metric, params->tos, params->mtu,
                             params->win, params->irtt, params->table,
                             params->hops, params->hops_num, rt_hndl);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_del_route_tmp(const char *ta, int addr_family,
                       const void *dst_addr, int prefix,
                       const void *gw_addr, const char *dev,
                       const void *src_addr,
                       uint32_t flags, int metric, int tos,
                       int mtu, int win, int irtt)
{
    return tapi_cfg_route_op(OP_DEL, ta, addr_family,
                             dst_addr, prefix, gw_addr, dev, src_addr, NULL,
                             flags, metric, tos, mtu, win, irtt,
                             TAPI_RT_TABLE_MAIN, NULL, 0, NULL);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_del_route(cfg_handle *rt_hndl)
{
    int rc;

    if (rt_hndl == NULL)
        return TE_EINVAL;

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
te_errno
tapi_cfg_get_neigh_entry(const char *ta, const char *ifname,
                         const struct sockaddr *net_addr,
                         void *ret_addr, te_bool *is_static,
                         cs_neigh_entry_state *state)
{
    return tapi_cfg_neigh_op(OP_GET, ta, ifname, net_addr, NULL,
                             ret_addr, is_static, state);
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_add_neigh_entry(const char *ta, const char *ifname,
                         const struct sockaddr *net_addr,
                         const void *link_addr, te_bool is_static)
{
    return tapi_cfg_neigh_op(OP_ADD, ta, ifname, net_addr, link_addr, NULL,
                             &is_static, NULL);
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_del_neigh_entry(const char *ta, const char *ifname,
                         const struct sockaddr *net_addr)
{
    return tapi_cfg_neigh_op(OP_DEL, ta, ifname, net_addr, NULL,
                             NULL, NULL, NULL);
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_del_neigh_dynamic(const char *ta, const char *ifname)
{
    cfg_handle     *hndls = NULL;
    unsigned int    num;
    unsigned int    i;
    te_errno        rc;
    te_errno        result = 0;

    if (ifname == NULL)
    {
        if ((rc = cfg_find_pattern_fmt(&num, &hndls,
                                       "/agent:%s/interface:*",
                                       ta)) != 0)
        {
            return rc;
        }
        for (i = 0; i < num; i++)
        {
            char *name = NULL;

            if ((rc = cfg_get_inst_name(hndls[i], &name)) != 0 ||
                (rc = tapi_cfg_del_neigh_dynamic(ta, name)) != 0)
            {
                TE_RC_UPDATE(result, rc);
            }
            free(name);
        }
        free(hndls);

        return result;
    }

    if ((rc = cfg_synchronize_fmt(TRUE, "/agent:%s/interface:%s",
                                  ta, ifname)) != 0)
        return rc;

    if ((rc = cfg_find_pattern_fmt(&num, &hndls,
                                   "/agent:%s/interface:*/neigh_dynamic:*",
                                   ta)) != 0)
    {
        return rc;
    }
    for (i = 0; i < num; i++)
    {
        if ((rc = cfg_del_instance(hndls[i], FALSE)) != 0)
        {
            if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
                TE_RC_UPDATE(result, rc);
        }
    }

    free(hndls);

    return result;
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
 * @param dev           Interface name
 * @param type          Route type
 * @param table         Route table ID
 * @param hops          Array of nexthops for a multipath route
 * @param hops_num      Number of elements in @p hops
 * @param cfg_hndl      Handle of the route in Configurator DB (OUT)
 *
 * @return Status code
 *
 * @retval 0  on success
 */
static int
tapi_cfg_route_op(enum tapi_cfg_oper op, const char *ta, int addr_family,
                  const void *dst_addr, int prefix, const void *gw_addr,
                  const char *dev, const void *src_addr, const char *type,
                  uint32_t flags, int metric, int tos, int mtu, int win,
                  int irtt, int table,
                  tapi_cfg_rt_nexthop *hops, size_t hops_num,
                  cfg_handle *cfg_hndl)
{
    cfg_handle  handle;
    char        dst_addr_str[INET6_ADDRSTRLEN];
    char        dst_addr_str_orig[INET6_ADDRSTRLEN];
    char        route_inst_name[1024];
    int         rc;
    int         netaddr_size = te_netaddr_get_size(addr_family);
    uint8_t    *dst_addr_copy;
    uint32_t    i;
    int         diff;
    uint8_t     mask;

    struct sockaddr_storage ss;
    struct sockaddr_storage src;

    UNUSED(flags);

    if (netaddr_size == 0)
    {
        ERROR("%s() unknown address family value", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if (prefix < 0 || prefix > (netaddr_size << 3))
    {
        ERROR("%s() fails: Incorrect prefix value specified %d",
              __FUNCTION__, prefix);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if ((dst_addr_copy = (uint8_t *)malloc(netaddr_size)) == NULL)
    {
        ERROR("%s() cannot allocate %d bytes for the copy of "
              "the network address", __FUNCTION__,
              te_netaddr_get_size(addr_family));

        return TE_RC(TE_TAPI, TE_ENOMEM);
    }

    if (inet_ntop(addr_family, dst_addr, dst_addr_str_orig,
                  sizeof(dst_addr_str_orig)) == NULL)
    {
        ERROR("%s() fails converting binary destination address "
              "into a character string", __FUNCTION__);
        return TE_OS_RC(TE_TAPI, errno);
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

        WARN("Destination address specified in the route is not "
             "cleared according to the prefix: prefix length %d, "
             "addr: %s expected to be %s. "
             "[The address %s is used as destination]",
             prefix, dst_addr_str_orig, dst_addr_str, dst_addr_str);
    }

    if (inet_ntop(addr_family, dst_addr_copy, dst_addr_str,
                  sizeof(dst_addr_str)) == NULL)
    {
        ERROR("%s() fails converting binary destination address "
              "into a character string", __FUNCTION__);
        free(dst_addr_copy);
        return TE_OS_RC(TE_TAPI, errno);
    }
    free(dst_addr_copy);

#define PUT_INTO_BUF(buf_, args...) \
    snprintf((buf_) + strlen(buf_), sizeof(buf_) - strlen(buf_), args)

    route_inst_name[0] = '\0';
    PUT_INTO_BUF(route_inst_name, "%s|%d", dst_addr_str, prefix);

    if (metric > 0)
        PUT_INTO_BUF(route_inst_name, ",metric=%d", metric);

    if (tos > 0)
        PUT_INTO_BUF(route_inst_name, ",tos=%d", tos);

    if (table != TAPI_RT_TABLE_MAIN)
        PUT_INTO_BUF(route_inst_name, ",table=%d", table);

    /* Prepare structure with gateway address */
    memset(&ss, 0, sizeof(ss));
    memset(&src, 0, sizeof(src));
    src.ss_family = ss.ss_family = addr_family;

    if (gw_addr != NULL)
    {
        te_sockaddr_set_netaddr(SA(&ss), gw_addr);
    }

    if (src_addr != NULL)
    {
        te_sockaddr_set_netaddr(SA(&src), src_addr);
    }

    switch (op)
    {
        case OP_MODIFY:
        {

/*
 * Macro makes local set for route attribute whose name specified
 * with 'field_' parameter.
 * Macro uses 'break' statement and is intended to be called inside
 * 'do {} while (FALSE)' brace.
 *
 * You may use this macro with trailing ';', which will just
 * add empty statement, but do not worry about that.
 */
#define CFG_RT_SET_LOCAL(field_) \
            if ((rc = cfg_set_instance_local_fmt(               \
                          CFG_VAL(INTEGER, field_),             \
                          "/agent:%s/route:%s/" #field_ ":",    \
                          ta, route_inst_name)) != 0)           \
            {                                                   \
                ERROR("%s() fails to set " #field_ " to %d "    \
                      "on route %s on '%s' Agent errno = %r",   \
                      __FUNCTION__,                             \
                      field_, route_inst_name, ta, rc);         \
                break;                                          \
            }

            do {
                if ((rc = cfg_set_instance_local_fmt(CFG_VAL(ADDRESS, &ss),
                                                     "/agent:%s/route:%s",
                                                     ta, route_inst_name))
                    != 0)
                {
                    ERROR("%s() fails to set value of route %s on '%s'"
                           " Agent errno = %r",
                           __FUNCTION__, dev, route_inst_name, ta, rc);
                    break;
                }

                if ((src_addr != NULL) &&
                    ((rc = cfg_set_instance_local_fmt(
                               CFG_VAL(ADDRESS, &src),
                               "/agent:%s/route:%s/src:",
                               ta, route_inst_name)) != 0))
                {
                    ERROR("%s() fails to set source for route %s on '%s'"
                          " Agent errno = %r",
                           __FUNCTION__, route_inst_name, ta, rc);
                    break;
                }

                if ((dev != NULL) &&
                    ((rc = cfg_set_instance_local_fmt(CFG_VAL(STRING, dev),
                          "/agent:%s/route:%s/dev:", ta, route_inst_name))
                      != 0))
                {
                    ERROR("%s() fails to set dev to %s "
                          "on route %s on '%s' Agent errno = %r",
                          __FUNCTION__, dev, route_inst_name, ta, rc);
                    break;
                }

                if ((type != NULL) &&
                    ((rc = cfg_set_instance_local_fmt(CFG_VAL(STRING, type),
                          "/agent:%s/route:%s/type:", ta, route_inst_name))
                      != 0))
                {
                    ERROR("%s() fails to set type to %s "
                          "on route %s on '%s' Agent errno = %r",
                          __FUNCTION__, type, route_inst_name, ta, rc);
                    break;
                }

                CFG_RT_SET_LOCAL(win);
                CFG_RT_SET_LOCAL(mtu);
                CFG_RT_SET_LOCAL(irtt);
            } while (FALSE);

#undef CFG_RT_SET_LOCAL

            if (rc == 0)
            {
                rc = cfg_commit_fmt("/agent:%s/route:%s",
                                    ta, route_inst_name);
            }

            break;
        }

        case OP_ADD:
        {

            rc = cfg_add_instance_local_fmt(&handle,
                                            CFG_VAL(ADDRESS, &ss),
                                            "/agent:%s/route:%s",
                                            ta, route_inst_name);

            if (rc != 0)
            {
                ERROR("%s() fails adding a new route %s on '%s' Agent "
                      "errno = %r", __FUNCTION__,
                      route_inst_name, ta, rc);
                break;
            }

#define CFG_RT_SET_LOCAL(field_) \
            if (field_ != 0 &&                                  \
                (rc = cfg_set_instance_local_fmt(               \
                          CFG_VAL(INTEGER, field_),               \
                          "/agent:%s/route:%s/" #field_ ":",    \
                          ta, route_inst_name)) != 0)           \
            {                                                   \
                ERROR("%s() fails to set " #field_ " to %d "    \
                      "on a new route %s on '%s' "              \
                      "Agent errno = %r", __FUNCTION__,         \
                      field_, route_inst_name, ta, rc);         \
                break;                                          \
            }

            do {
                if ((dev != NULL) &&
                    (rc = cfg_set_instance_local_fmt(
                              CFG_VAL(STRING, dev),
                              "/agent:%s/route:%s/dev:",
                              ta, route_inst_name)) != 0)
                {
                    ERROR("%s() fails to set dev to %s "
                          "on a new route %s on '%s' Agent errno = %r",
                          __FUNCTION__, dev, route_inst_name, ta, rc);
                    break;
                }

                if ((src_addr != NULL) &&
                    ((rc = cfg_set_instance_local_fmt(
                               CFG_VAL(ADDRESS, &src),
                               "/agent:%s/route:%s/src:",
                               ta, route_inst_name)) != 0))
                {
                    ERROR("%s() fails to set source address on a new "
                          "route %s on '%s' Agent errno = %r",
                           __FUNCTION__, route_inst_name, ta, rc);
                    break;
                }

                if ((type != NULL) &&
                    ((rc = cfg_set_instance_local_fmt(CFG_VAL(STRING, type),
                          "/agent:%s/route:%s/type:", ta, route_inst_name))
                      != 0))
                {
                    ERROR("%s() fails to set type to %s "
                          "on route %s on '%s' Agent errno = %r",
                          __FUNCTION__, type, route_inst_name, ta, rc);
                    break;
                }

                CFG_RT_SET_LOCAL(win);
                CFG_RT_SET_LOCAL(mtu);
                CFG_RT_SET_LOCAL(irtt);
            } while (FALSE);

            if (rc == 0)
            {
                for (i = 0; i < hops_num; i++)
                {
                    rc = cfg_add_instance_local_fmt(
                                        NULL, CVT_NONE, NULL,
                                        "/agent:%s/route:%s/nexthop:%u",
                                        ta, route_inst_name, i);

                    if (rc != 0)
                    {
                        ERROR("%s() failed to add a new nexthop "
                              "for route %s on '%s' Agent, rc = %r",
                              __FUNCTION__, route_inst_name, ta, rc);
                        break;
                    }

                    rc = cfg_set_instance_local_fmt(
                              CFG_VAL(INTEGER, hops[i].weight),
                              "/agent:%s/route:%s/nexthop:%u/weight:",
                              ta, route_inst_name, i);
                    if (rc != 0)
                    {
                        ERROR("%s() failed to set weight for nexthop, "
                              "rc = %r", __FUNCTION__, rc);
                        break;
                    }

                    rc = cfg_set_instance_local_fmt(
                              CFG_VAL(STRING, hops[i].ifname),
                              "/agent:%s/route:%s/nexthop:%u/dev:",
                              ta, route_inst_name, i);
                    if (rc != 0)
                    {
                        ERROR("%s() failed to set dev for nexthop, "
                              "rc = %r", __FUNCTION__, rc);
                        break;
                    }

                    if (hops[i].gw.ss_family != AF_UNSPEC)
                    {
                        rc = cfg_set_instance_local_fmt(
                                CFG_VAL(ADDRESS, &hops[i].gw),
                                "/agent:%s/route:%s/nexthop:%u/gw:",
                                ta, route_inst_name, i);

                        if (rc != 0)
                        {
                            ERROR("%s() failed to set gw for nexthop, "
                                  "rc = %r", __FUNCTION__, rc);
                            break;
                        }
                    }

                }
            }

#undef CFG_RT_SET_LOCAL

            if (rc != 0)
            {
                cfg_del_instance(handle, TRUE);
                break;
            }

            if ((rc = cfg_commit_fmt("/agent:%s/route:%s",
                                     ta, route_inst_name)) != 0)
            {
                ERROR("%s() fails to commit a new route %s "
                      "on '%s' Agent errno = %r",
                      __FUNCTION__, route_inst_name, ta, rc);
            }

            if (rc == 0 && cfg_hndl != NULL)
                *cfg_hndl = handle;
            break;
        }

        case OP_DEL:
            if ((rc = cfg_del_instance_fmt(FALSE, "/agent:%s/route:%s",
                                           ta, route_inst_name)) != 0)
            {
                ERROR("%s() fails deleting route %s on '%s' Agent "
                      "errno = %r", __FUNCTION__, route_inst_name, ta, rc);
            }
            break;

        default:
            ERROR("%s(): Operation %d is not supported", __FUNCTION__, op);
            rc = TE_RC(TE_TAPI, TE_EOPNOTSUPP);
            break;
    }

#undef PUT_INTO_BUF

    return rc;
}

/**
 * Perform specified operation with neighbour cache.
 *
 * @param op            Operation
 * @param ta            Test agent
 * @param ifname        Interface name
 * @param net_addr      Network address
 * @param link_addr     Link-leyer address
 * @param ret_addr      Returned by OP_GET operation link_addr
 * @param state         NULL or location for state of dynamic entry
 *
 * @return Status code
 *
 * @retval 0  on success
 */
static int
tapi_cfg_neigh_op(enum tapi_cfg_oper op, const char *ta,
                  const char *ifname, const struct sockaddr *net_addr,
                  const void *link_addr, void *ret_addr,
                  te_bool *is_static, cs_neigh_entry_state *state)
{
    cfg_handle handle;
    char       net_addr_str[INET6_ADDRSTRLEN];
    int        rc = 0;

    if (ta == NULL || net_addr == NULL || ifname == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    if (inet_ntop(net_addr->sa_family,
                  te_sockaddr_get_netaddr(net_addr),
                  net_addr_str, sizeof(net_addr_str)) == NULL)
    {
        ERROR("%s() fails converting binary IPv4 address  "
              "into a character string", __FUNCTION__);
        return TE_OS_RC(TE_TAPI, errno);
    }

    switch (op)
    {
        case OP_GET:
        {
            struct sockaddr *lnk_addr = NULL;

            rc = cfg_synchronize_fmt(TRUE, "/agent:%s/interface:%s/"
                                           "neigh_static:%s",
                                     ta, ifname, net_addr_str);
            if (rc != 0)
                break;

            rc = cfg_get_instance_fmt(NULL, &lnk_addr,
                                      "/agent:%s/interface:%s/"
                                      "neigh_static:%s",
                                      ta, ifname, net_addr_str);

            if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
            {
                rc = cfg_synchronize_fmt(TRUE, "/agent:%s/interface:%s",
                                         ta, ifname);
                if (rc != 0)
                    break;

                rc = cfg_get_instance_fmt(NULL, &lnk_addr,
                                          "/agent:%s/interface:%s/"
                                          "neigh_dynamic:%s",
                                          ta, ifname, net_addr_str);
                if (rc == 0)
                {
                    if (is_static != NULL)
                        *is_static = FALSE;

                    if (state != NULL)
                    {
                        rc = cfg_get_instance_fmt(NULL, state,
                                                  "/agent:%s/interface:%s/"
                                                  "neigh_dynamic:%s/state:",
                                                  ta, ifname, net_addr_str);
                    }
                }
            }
            else if ((rc == 0) && (is_static != NULL))
            {
                *is_static = TRUE;
                if (state != NULL)
                    *state = CS_NEIGH_REACHABLE;
            }

            if (rc == 0)
            {
                if (ret_addr != NULL)
                {
                    memcpy(ret_addr, &(lnk_addr->sa_data), IFHWADDRLEN);
                }
                free(lnk_addr);
            }
            else if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
            {
                ERROR("%s() cfg_get_instance_fmt() failed for neighbour "
                      "entry %s on interface %s of TA %s with error %r",
                      __FUNCTION__, net_addr_str, ifname, ta, rc);
            }
            break;
        }

        case OP_ADD:
        {
            struct sockaddr lnk_addr;

            if (link_addr == NULL || is_static == NULL)
                return TE_RC(TE_TAPI, TE_EINVAL);

            memset(&lnk_addr, 0, sizeof(lnk_addr));
            lnk_addr.sa_family = AF_LOCAL;
            memcpy(&(lnk_addr.sa_data), link_addr, IFHWADDRLEN);

            rc = cfg_add_instance_fmt(&handle,
                                      CFG_VAL(ADDRESS, &lnk_addr),
                                      "/agent:%s/interface:%s/neigh_%s:%s",
                                      ta, ifname,
                                      (*is_static) ? "static" :
                                                     "dynamic",
                                      net_addr_str);
            /* Error is logged by CS */
            break;
        }

        case OP_DEL:
            rc = cfg_find_fmt(&handle,
                              "/agent:%s/interface:%s/neigh_static:%s",
                              ta, ifname, net_addr_str);
            if (rc == 0)
            {
                rc = cfg_del_instance(handle, FALSE);
                /* Error is logged by CS */
            }
            else if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
            {
                rc = cfg_synchronize_fmt(TRUE, "/agent:%s/interface:%s",
                                         ta, ifname);
                if (rc != 0)
                    break;

                rc = cfg_find_fmt(&handle,
                                  "/agent:%s/interface:%s/neigh_dynamic:%s",
                                  ta, ifname, net_addr_str);
                if (rc == 0)
                {
                    rc = cfg_del_instance(handle, FALSE);
                    /* Error is logged by CS */
                }

                if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
                {
                    RING("There is no neighbour entry for %s on "
                         "interface %s of TA %s", net_addr_str, ifname, ta);
                    rc = 0;
                }
            }
            break;

        default:
            ERROR("%s(): Operation %d is not supported", __FUNCTION__, op);
            rc = TE_RC(TE_TAPI, TE_EOPNOTSUPP);
            break;
    }

    return rc;
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_get_hwaddr(const char *ta,
                    const char *ifname,
                    void *hwaddr, size_t *hwaddr_len)
{
    char     buf[1024];
    int      rc;
    char   *ifname_bkp;
    char   *ptr;

    if (hwaddr == NULL || hwaddr_len == NULL)
    {
        ERROR("%s(): It is not allowed to have NULL 'hwaddr' or "
              "'hwaddr_len' parameter", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    if (*hwaddr_len < IFHWADDRLEN)
    {
        ERROR("%s(): 'hwaddr' is too short", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EMSGSIZE);
    }

    /**
     * Configuration model does not support alias interfaces,
     * so that we should truncate trailing :XX part from the interface name.
     */
    if ((ifname_bkp = (char *)malloc(strlen(ifname) + 1)) == NULL)
    {
        return TE_RC(TE_TAPI, TE_ENOMEM);
    }
    memcpy(ifname_bkp, ifname, strlen(ifname) + 1);

    if ((ptr = strchr(ifname_bkp, ':')) != NULL)
        *ptr = '\0';

    snprintf(buf, sizeof(buf), "/agent:%s/interface:%s",
             ta, ifname_bkp);
    free(ifname_bkp);
    if ((rc = tapi_cfg_base_if_get_mac(buf, hwaddr)) != 0)
    {
        return rc;
    }

    return 0;
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_set_hwaddr(const char *ta,
                    const char *ifname,
                    const void *hwaddr, unsigned int hwaddr_len)
{
    char        buf[1024];
    char       *ifname_bkp;
    char       *ptr;

    if (hwaddr == NULL || hwaddr_len == 0)
    {
        ERROR("%s(): It is not allowed to have NULL 'hwaddr' or "
              "'hwaddr_len' parameter", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    if (hwaddr_len < IFHWADDRLEN)
    {
        ERROR("%s(): 'hwaddr' is too short", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EMSGSIZE);
    }

    /**
     * Configuration model does not support alias interfaces,
     * so that we should truncate trailing :XX part from the interface name.
     */
    if ((ifname_bkp = (char *)malloc(strlen(ifname) + 1)) == NULL)
    {
        return TE_RC(TE_TAPI, TE_ENOMEM);
    }
    memcpy(ifname_bkp, ifname, strlen(ifname) + 1);

    if ((ptr = strchr(ifname_bkp, ':')) != NULL)
        *ptr = '\0';

    snprintf(buf, sizeof(buf), "/agent:%s/interface:%s",
             ta, ifname_bkp);
    free(ifname_bkp);
    return tapi_cfg_base_if_set_mac(buf, hwaddr);
}

/* See the description in tapi_cfg.h */
int
tapi_cfg_get_bcast_hwaddr(const char *ta,
                          const char *ifname,
                          void *hwaddr, size_t *hwaddr_len)
{
    char     buf[1024];
    int      rc;
    char    *ifname_bkp;
    char    *ptr;

    if (hwaddr == NULL || hwaddr_len == NULL)
    {
        ERROR("%s(): It is not allowed to have NULL 'hwaddr' or "
              "'hwaddr_len' parameter", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    if (*hwaddr_len < IFHWADDRLEN)
    {
        ERROR("%s(): 'hwaddr' is too short", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EMSGSIZE);
    }

    /**
     * Configuration model does not support alias interfaces,
     * so that we should truncate trailing :XX part from the interface name.
     */
    if ((ifname_bkp = (char *)malloc(strlen(ifname) + 1)) == NULL)
    {
        return TE_RC(TE_TAPI, TE_ENOMEM);
    }
    memcpy(ifname_bkp, ifname, strlen(ifname) + 1);

    if ((ptr = strchr(ifname_bkp, ':')) != NULL)
        *ptr = '\0';

    snprintf(buf, sizeof(buf), "/agent:%s/interface:%s",
             ta, ifname_bkp);
    free(ifname_bkp);
    if ((rc = tapi_cfg_base_if_get_bcast_mac(buf, hwaddr)) != 0)
    {
        return rc;
    }

    return 0;
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_set_bcast_hwaddr(const char *ta, const char *ifname,
                          const void *hwaddr, unsigned int hwaddr_len)
{
    char        buf[1024];
    char       *ifname_bkp;
    char       *ptr;

    if (hwaddr == NULL || hwaddr_len == 0)
    {
        ERROR("%s(): It is not allowed to have NULL 'hwaddr' or "
              "'hwaddr_len' parameter", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    if (hwaddr_len < IFHWADDRLEN)
    {
        ERROR("%s(): 'hwaddr' is too short", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EMSGSIZE);
    }

    /**
     * Configuration model does not support alias interfaces,
     * so that we should truncate trailing :XX part from the interface name.
     */
    if ((ifname_bkp = (char *)malloc(strlen(ifname) + 1)) == NULL)
    {
        return TE_RC(TE_TAPI, TE_ENOMEM);
    }
    memcpy(ifname_bkp, ifname, strlen(ifname) + 1);

    if ((ptr = strchr(ifname_bkp, ':')) != NULL)
        *ptr = '\0';

    snprintf(buf, sizeof(buf), "/agent:%s/interface:%s",
             ta, ifname_bkp);
    free(ifname_bkp);
    return tapi_cfg_base_if_set_bcast_mac(buf, hwaddr);
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_get_if_parent(const char *ta,
                       const char *ifname,
                       char *parent_ifname,
                       size_t len)
{
    char          *parent = NULL;
    cfg_val_type   type;
    int            rc = 0;
    size_t         len_got;

    type = CVT_STRING;
    rc = cfg_get_instance_fmt(&type, &parent,
                              "/agent:%s/interface:%s/parent:",
                              ta, ifname);
    if (rc != 0)
        return rc;

    len_got = strlen(parent) + 1; /* 1 is for terminating '\0' */
    if (len_got > len)
    {
        ERROR("%s(): obtained interface name is too long "
              "to fit in the provided buffer: "
              "%" TE_PRINTF_SIZE_T "u vs %" TE_PRINTF_SIZE_T "u",
              __FUNCTION__, len_got, len);
        free(parent);
        return TE_RC(TE_TAPI, TE_EOVERFLOW);
    }

    strncpy(parent_ifname, parent, len);
    free(parent);

    return 0;
}

/* Network interface kinds mapping array. */
static const char *te_interface_kinds[TE_INTERFACE_KIND_END] =
    {
        [TE_INTERFACE_KIND_NONE] = "",
        [TE_INTERFACE_KIND_VLAN] = "vlan",
        [TE_INTERFACE_KIND_MACVLAN] = "macvlan",
        [TE_INTERFACE_KIND_VETH] = "veth",
        [TE_INTERFACE_KIND_BOND] = "bond",
        [TE_INTERFACE_KIND_TEAM] = "team",
        [TE_INTERFACE_KIND_BRIDGE] = "bridge",
    };

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_get_if_kind(const char *ta, const char *ifname,
                     te_interface_kind *kind)
{
    te_errno rc;
    char    *val;
    int      i;

    rc = cfg_get_instance_fmt(NULL, &val, "/agent:%s/interface:%s/kind:",
                              ta, ifname);
    if (rc != 0)
    {
        ERROR("Failed to get kind of interface %s/%s: %r", ta, ifname, rc);
        return rc;
    }

    rc = TE_RC(TE_TAPI, TE_EINVAL);
    for (i = 0; i < TE_INTERFACE_KIND_END; i++)
    {
        if (strcmp(val, te_interface_kinds[i]) == 0)
        {
            rc = 0;
            *kind = i;
            break;
        }
    }

    if (rc != 0)
        ERROR("Unknown interface kind '%s'", val);

    free(val);

    return rc;
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_get_if_last_ancestor(const char *ta,
                              const char *ifname,
                              char *ancestor_ifname,
                              size_t len)
{
    char      parent_ifname[IF_NAMESIZE];
    te_errno  rc = 0;

    if (len == 0)
        return TE_RC(TE_TAPI, TE_ESMALLBUF);

    strncpy(ancestor_ifname, ifname, len);
    if (ancestor_ifname[len - 1] != '\0')
    {
        ERROR("%s(): interface name is too long "
              "to fit into provided buffer", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_ESMALLBUF);
    }

    do {
        rc = tapi_cfg_get_if_parent(ta, ancestor_ifname,
                                    parent_ifname,
                                    sizeof(parent_ifname));
        if (rc != 0)
            return rc;

        if (parent_ifname[0] == '\0')
            break;

        strncpy(ancestor_ifname, parent_ifname, len);
        if (ancestor_ifname[len - 1] != '\0')
        {
            ERROR("%s(): interface name is too long",
                  __FUNCTION__);
            return TE_RC(TE_TAPI, TE_ESMALLBUF);
        }

    } while (TRUE);

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
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    *entry = CFG_HANDLE_INVALID;

    for (rc = cfg_get_son(parent, &handle);
         rc == 0 && handle != CFG_HANDLE_INVALID;
         rc = cfg_get_brother(handle, &handle))
    {
        rc = cfg_get_instance(handle, &type, &value);
        if (rc != 0)
        {
            ERROR("%s: Failed to get integer value by handle 0x%x: %r",
                  __FUNCTION__, handle, rc);
            break;
        }
        if (value == 0)
        {
            rc = cfg_set_instance(handle, type, 1);
            if (rc != 0)
            {
                ERROR("%s: Failed to set value of handle 0x%x to 1: %r",
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
            rc = TE_RC(TE_TAPI, TE_ENOENT);
        }
    }
#if 0
    else if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
    {
        INFO("No free entries in pool 0x%x", parent);
    }
#endif
    else
    {
        ERROR("Failed to allocate entry in 0x%x: %r", parent, rc);
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
        ERROR("%s: Failed to convert OID '%s' to handle: %r",
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
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    if (*entry == CFG_HANDLE_INVALID)
    {
        return 0;
    }

    rc = cfg_set_instance(*entry, CVT_INTEGER, 0);
    if (rc != 0)
    {
    ERROR("Failed to free entry by handle 0x%x: %r", *entry, rc);
    }
    else
    {
        INFO("Pool entry with Cfgr handle 0x%x freed", *entry);
        *entry= CFG_HANDLE_INVALID;
    }
    return rc;
}


/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_add_net(const char *net_pool, const struct sockaddr *net_addr,
                 unsigned int prefix, int state, cfg_handle *entry)
{
    te_errno                rc;
    cfg_handle              pool;
    cfg_handle              net;
    char                    buf[INET6_ADDRSTRLEN];
    struct sockaddr_storage addr;

    *entry = CFG_HANDLE_INVALID;

    rc = cfg_find_str(net_pool, &pool);
    if (rc != 0)
    {
        ERROR("%s: Failed to find '%s' instance: %r",
              __FUNCTION__, net_pool, rc);
        return rc;
    }

    assert(sizeof(addr) >= te_sockaddr_get_size(net_addr));
    memcpy(&addr, net_addr, te_sockaddr_get_size(net_addr));
    rc = te_sockaddr_cleanup_to_prefix(SA(&addr), prefix);
    if (rc != 0)
        return rc;

    if (inet_ntop(addr.ss_family, te_sockaddr_get_netaddr(SA(&addr)),
                  buf, sizeof(buf)) == NULL)
    {
        rc = TE_OS_RC(TE_TAPI, errno);
        ERROR("%s: Failed to convert address to string", __FUNCTION__);
        return rc;
    }

    /* Check for interference with existing nets in the pool */
    for (rc = cfg_get_son(pool, &net);
         rc == 0 && net != CFG_HANDLE_INVALID;
         rc = cfg_get_brother(net, &net))
    {
        cfg_val_type        val_type;
        char               *net_oid;
        struct sockaddr    *net_sa;
        unsigned int        net_prefix;
        struct in_addr      net_mask;

        rc = cfg_get_inst_name_type(net, CVT_ADDRESS,
                                    (cfg_inst_val *)&net_sa);
        if (rc != 0)
        {
            ERROR("%s: Cannot get pool net name by handle 0x%x "
                  "as address: %r", __FUNCTION__, net, rc);
            return rc;
        }
        if (net_sa->sa_family != addr.ss_family)
        {
            ERROR("%s: Pool %s contains addresses of different family",
                  __FUNCTION__, net_pool);
            free(net_sa);
            return rc;
        }

        rc = cfg_get_oid_str(net, &net_oid);
        if (rc != 0)
        {
            ERROR("%s: Cannot get pool net OID by handle 0x%x: %r",
                  __FUNCTION__, net, rc);
            free(net_sa);
            return rc;
        }
        val_type = CVT_INTEGER;
        rc = cfg_get_instance_fmt(&val_type, &net_prefix,
                                  "%s/prefix:", net_oid);
        if (rc != 0)
        {
            ERROR("%s: Cannot get pool net prefix for %s: %r",
                  __FUNCTION__, net_oid, rc);
            free(net_oid);
            free(net_sa);
            return rc;
        }

        /* Compare net from pool with net to be added */
        net_mask.s_addr = htonl(PREFIX2MASK(net_prefix < prefix ?
                                            net_prefix : prefix));
        if ((SIN(net_sa)->sin_addr.s_addr & net_mask.s_addr) ==
            (SIN(&addr)->sin_addr.s_addr & net_mask.s_addr))
        {
            ERROR("%s: Cannot add network %s/%u to pool: it interferes "
                  "with %s/%u", __FUNCTION__, buf, prefix, net_oid,
                  net_prefix);
            free(net_oid);
            free(net_sa);
            return TE_EEXIST;
        }

        free(net_oid);
        free(net_sa);
    }

    /* Add new entry to the pool */
    rc = cfg_add_instance_fmt(&net, CFG_VAL(INTEGER, state),
                              "%s/entry:%s", net_pool, buf);
    if (rc != 0)
    {
        ERROR("%s: Failed to add '%s/entry:%s' to the pool: %r",
              __FUNCTION__, net_pool, buf, rc);
        return rc;
    }
    rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, prefix),
                              "%s/entry:%s/prefix:", net_pool, buf);
    if (rc != 0)
    {
        ERROR("%s: Failed to add '%s/entry:%s/prefix' to the pool: %r",
              __FUNCTION__, net_pool, buf, rc);
        return rc;
    }
    rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, 0),
                              "%s/entry:%s/n_entries:", net_pool, buf);
    if (rc != 0)
    {
        ERROR("%s: Failed to add %s/entry:%s/n_instance to the pool: %r",
              __FUNCTION__, net_pool, buf, rc);
        return rc;
    }

    *entry = net;
    RING("Network %s/%u is added to the pool", buf, prefix);

    return 0;
}


/**
 * Internal implementation of tapi_cfg_add_net_addr() and
 * tapi_cfg_alloc_net_addr(). Add new entry in subnet from subnets pool.
 *
 * @param net_pool_entry    Subnet handle
 * @param add_addr          Address to add or NULL to allocate any
 *                          free address from pool
 * @param p_entry           Location for Cfgr handle of new entry
 * @param addr              Location for added address
 *
 * @return Status code.
 */
static int
tapi_cfg_insert_net_addr(cfg_handle        net_pool_entry,
                         struct sockaddr  *add_addr,
                         cfg_handle       *p_entry,
                         struct sockaddr **addr)
{
    te_errno        rc;
    char           *net_oid;
    cfg_handle      pool;
    cfg_handle      entry;
    int             n_entries;
    cfg_val_type    val_type;
    int             prefix;
    unsigned int    net_addr_bits;
    char            buf[INET6_ADDRSTRLEN];
    int             entry_state;


    rc = cfg_get_oid_str(net_pool_entry, &net_oid);
    if (rc != 0)
    {
        ERROR("Failed to get OID by handle 0x%x: %r", net_pool_entry, rc);
        return rc;
    }

    /* Find or create pool of IPv4 subnet addresses */
    rc = cfg_find_fmt(&pool, "%s/pool:", net_oid);
    if (rc != 0)
    {
        ERROR("Failed to find object instance '%s/pool:': %r",
              net_oid, rc);
        free(net_oid);
        return rc;
    }

    rc = tapi_cfg_alloc_entry_by_handle(pool, &entry);
    if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
    {
        free(net_oid);
        if (rc == 0)
        {
            /* Get address */
            rc = cfg_get_inst_name_type(entry, CVT_ADDRESS,
                                        (cfg_inst_val *)addr);
            if (rc != 0)
            {
                ERROR("Failed to get network address as instance "
                      "name of 0x%x: %r", entry, rc);
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
    val_type = CVT_INTEGER;
    rc = cfg_get_instance_fmt(&val_type, &n_entries,
                              "%s/n_entries:", net_oid);
    if (rc != 0)
    {
        ERROR("Failed to get number of entries in the pool: "
              "'%s/n_entries:' : %r", net_oid, rc);
        free(net_oid);
        return rc;
    }

    /* Create one more entry */
    n_entries++;

    /* Get subnet address */
    rc = cfg_get_inst_name_type(net_pool_entry, CVT_ADDRESS,
                                (cfg_inst_val *)addr);
    if (rc != 0)
    {
        ERROR("Failed to get IPv4 subnet address from '%s': %r",
              net_oid, rc);
        free(net_oid);
        return rc;
    }

    /* Get network prefix length */
    val_type = CVT_INTEGER;
    rc = cfg_get_instance_fmt(&val_type, &prefix, "%s/prefix:", net_oid);
    if (rc != 0)
    {
        ERROR("Failed to get prefix length of '%s': %r",
              net_oid, rc);
        free(*addr); *addr = NULL;
        free(net_oid);
        return rc;
    }

    net_addr_bits = te_netaddr_get_size((*addr)->sa_family) << 3;
    if (prefix < 0 || (unsigned int)prefix > net_addr_bits)
    {
        ERROR("%s(): Invalid length of the prefix for the address "
              "family", __FUNCTION__);
        free(*addr); *addr = NULL;
        free(net_oid);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    /* Check for sufficient space */
    if ((net_addr_bits - prefix) <= (sizeof(unsigned int) << 3) &&
        n_entries > ((1 << (net_addr_bits - prefix)) - 2))
    {
        ERROR("All addresses of the subnet '%s' are used",
              net_oid);
        free(*addr); *addr = NULL;
        free(net_oid);
        return TE_RC(TE_TAPI, TE_ENOENT);
    }

    if (add_addr == NULL)
    {
        uint32_t *p = te_sockaddr_get_netaddr(*addr);

        /* Dynamic allocation of IP */
        /* TODO: Optimize free address search */
        do {
            /* Make address from subnet address */
            switch ((*addr)->sa_family)
            {
                case AF_INET:
                    *p = htonl(ntohl(*p) + 1);
                    break;

                case AF_INET6:
                    p[sizeof(struct in6_addr) / sizeof(*p) - 1] =
                        htonl(ntohl(p[sizeof(struct in6_addr) /
                                      sizeof(*p) - 1]) + 1);
                    break;

                default:
                    ERROR("%s: Address family %u is not supported",
                          __FUNCTION__, (*addr)->sa_family);
                    p = NULL;
                    break;
            }
            if (inet_ntop((*addr)->sa_family, p, buf, sizeof(buf)) == NULL)
            {
                ERROR("%s: Failed to convert address to string",
                      __FUNCTION__);
                rc = TE_RC(TE_TAPI, TE_EINVAL);
            }
            else
            {
                /* Check if the entry already exists */
                val_type = CVT_INTEGER;
                rc = cfg_get_instance_fmt(&val_type, &entry_state,
                                          "%s/pool:/entry:%s",
                                          net_oid, buf);
            }
        } while (rc == 0);
    }
    else
    {
        /* Insert predefined IP */
        uint32_t    mask = PREFIX2MASK(prefix);

        inet_ntop(AF_INET, &SIN(add_addr)->sin_addr, buf, sizeof(buf));
        if ((ntohl(SIN(add_addr)->sin_addr.s_addr) & mask) !=
            (ntohl(SIN(*addr)->sin_addr.s_addr) & mask))
        {
            ERROR("Cannot add address %s to '%s': does not fit",
                  buf, net_oid);
            free(*addr); *addr = NULL;
            free(net_oid);
            return TE_EINVAL;
        }

        /* Check if the entry already exists */
        val_type = CVT_INTEGER;
        rc = cfg_get_instance_fmt(&val_type, &entry_state,
                                  "%s/pool:/entry:%s", net_oid, buf);
    }

    if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
    {
        ERROR("Failed to get '%s/pool:/entry:%s' instance while "
              "checking for free address: %r", net_oid, buf, rc);
        free(*addr); *addr = NULL;
        free(net_oid);
        return rc;
    }

    /* Add used entry in the pool */
    rc = cfg_add_instance_fmt(&entry, CFG_VAL(INTEGER, 1),
                              "%s/pool:/entry:%s", net_oid, buf);
    if (rc != 0)
    {
        ERROR("Failed to add entry in IPv4 subnet pool '%s': %r",
              net_oid, rc);
        free(*addr); *addr = NULL;
        free(net_oid);
        return rc;
    }

    /* Update number of entries ASAP */
    rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, n_entries),
                              "%s/n_entries:", net_oid);
    if (rc != 0)
    {
        ERROR("Failed to get number of entries in the pool: %r",
              rc);
        free(*addr); *addr = NULL;
        free(net_oid);
        return rc;
    }
    RING("Address %s is added to pool entry '%s'", buf, net_oid);
    free(net_oid);

    if (p_entry != NULL)
        *p_entry = entry;

    return 0;
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_add_net_addr(cfg_handle net_pool_entry, struct sockaddr *add_addr,
                      cfg_handle *p_entry)
{
    te_errno         rc;
    struct sockaddr *addr = NULL;

    rc = tapi_cfg_insert_net_addr(net_pool_entry, add_addr, p_entry, &addr);
    free(addr);

    return rc;
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_alloc_net_addr(cfg_handle net_pool_entry, cfg_handle *p_entry,
                        struct sockaddr **addr)
{
    return tapi_cfg_insert_net_addr(net_pool_entry, NULL, p_entry, addr);
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_set_loglevel(const char *agent, int level)
{
    return cfg_set_instance_fmt(CFG_VAL(INTEGER, level),
                                "/agent:%s/sys:/console_loglevel:", agent);
}

/* See the description in tapi_cfg.h */
te_errno
tapi_cfg_set_loglevel_save(const char *ta, int new_val, int *old_val)
{
    int       prev_val = -1;
    te_errno  rc = 0;

    if (old_val != NULL)
    {
        rc = cfg_get_instance_fmt(CVT_INTEGER, &prev_val,
                                  "/agent:%s/sys:/console_loglevel:", ta);
        if (rc != 0)
        {
            ERROR("%s(): failed to get current kernel log level",
                  __FUNCTION__);
            *old_val = -1;
            return rc;
        }

        *old_val = prev_val;
    }

    if (new_val != prev_val && new_val >= 0)
    {
        rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, new_val),
                                  "/agent:%s/sys:/console_loglevel:", ta);
        if (rc != 0)
        {
            ERROR("%s(): failed to set kernel log level to %d",
                  __FUNCTION__, new_val);
            return rc;
        }
    }

    return 0;
}

/* See description in tapi_cfg.h */
te_errno
tapi_cfg_get_int_str(int *value, const char *oid)
{
    te_errno      rc = 0;
    cfg_val_type  val_type = CVT_INTEGER;

    rc = cfg_get_instance_str(&val_type, value,
                              oid);
    if (rc != 0)
        ERROR("Failed to get %s", oid);

    return rc;
}

/* See description in tapi_cfg.h */
te_errno
tapi_cfg_get_int_fmt(int *value, const char *format, ...)
{
    va_list       valist;
    te_string     str = TE_STRING_INIT;
    te_errno      rc = 0;

    va_start(valist, format);
    rc = te_string_append_va(&str, format, valist);
    va_end(valist);

    if (rc != 0)
    {
        ERROR("%s(): failed to construct oid", __FUNCTION__);
        goto cleanup;
    }

    rc = tapi_cfg_get_int_str(value, str.ptr);

cleanup:
    te_string_free(&str);
    return rc;
}

/* See description in tapi_cfg.h */
te_errno
tapi_cfg_set_int_str(int value, int *old_value, const char *oid)
{
    te_errno      rc = 0;

    if (old_value != NULL)
    {
        rc = tapi_cfg_get_int_str(old_value, oid);
        if (rc != 0)
            return rc;
    }

    rc = cfg_set_instance_str(CFG_VAL(INTEGER, value), oid);
    if (rc != 0)
        ERROR("Failed to set %s to %d", oid, value);

    return rc;
}

/* See description in tapi_cfg.h */
te_errno
tapi_cfg_set_int_fmt(int value, int *old_value, const char *format, ...)
{
    va_list       valist;
    te_string     str = TE_STRING_INIT;
    te_errno      rc = 0;

    va_start(valist, format);
    rc = te_string_append_va(&str, format, valist);
    va_end(valist);

    if (rc != 0)
    {
        ERROR("%s(): failed to construct oid", __FUNCTION__);
        goto cleanup;
    }

    rc = tapi_cfg_set_int_str(value, old_value, str.ptr);

cleanup:

    te_string_free(&str);
    return rc;
}

/* See description in tapi_cfg.h */
te_errno
tapi_cfg_alloc_net_addr_pair(struct sockaddr **addr1, struct sockaddr **addr2,
                             int *prefix)
{
    cfg_handle  cfgh_net;
    char       *net_pool;
    te_errno    rc;

    rc = tapi_cfg_alloc_ip4_net(&cfgh_net);
    if (rc != 0)
        return rc;

    rc = tapi_cfg_alloc_net_addr(cfgh_net, NULL, addr1);
    if (rc != 0)
        return rc;

    rc = tapi_cfg_alloc_net_addr(cfgh_net, NULL, addr2);
    if (rc != 0)
    {
        free(addr1);
        return rc;
    }

    if (prefix != NULL)
    {
        rc = cfg_get_oid_str(cfgh_net, &net_pool);
        if (rc != 0)
        {
            free(addr1);
            free(addr2);
            return rc;
        }

        rc = cfg_get_instance_fmt(CVT_INTEGER, prefix, "%s/prefix:",
                                  net_pool);
        free(net_pool);
        if (rc != 0)
        {
            free(addr1);
            free(addr2);
            return rc;
        }
    }

    return 0;
}
