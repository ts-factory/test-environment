/** @file
 * @brief Windows Test Agent
 *
 * Windows TA configuring support
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
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
 *
 * $Id$
 */

#define TE_LGR_USER     "Windows Conf"

#include "windows.h"
#include "w32api/iphlpapi.h"
#undef _SYS_TYPES_FD_SET
#include <w32api/winsock2.h>
#include "te_defs.h"
#include "iprtrmib.h"

#undef ERROR

#include "te_errno.h"
#include "te_defs.h"
#include "logger_api.h"
#include "comm_agent.h"
#include "rcf_ch_api.h"
#include "rcf_pch.h"
#include "rcf_pch_ta_cfg.h"
#include "cs_common.h"

/* TA name pointer */
extern char *ta_name;

/* Auxiliary buffer */
static char  buf[2048 * 32] = {0, };

#define PRINT(msg...) \
    do {                                                \
       printf(msg); printf("\n"); fflush(stdout);       \
    } while (0)

/** Route is direct "local interface" in terms of RFC 1354 */
#define FORW_TYPE_LOCAL  3

/** Route is indirect "remote destination" in terms of RFC 1354 */
#define FORW_TYPE_REMOTE 4

/** Static ARP entry */
#define ARP_STATIC      4

/** Dynamic ARP entry */
#define ARP_DYNAMIC     3

/** Fast conversion of the network mask to prefix */
#define MASK2PREFIX(mask, prefix)            \
    switch (ntohl(mask))                     \
    {                                        \
        case 0x0: prefix = 0; break;         \
        case 0x80000000: prefix = 1; break;  \
        case 0xc0000000: prefix = 2; break;  \
        case 0xe0000000: prefix = 3; break;  \
        case 0xf0000000: prefix = 4; break;  \
        case 0xf8000000: prefix = 5; break;  \
        case 0xfc000000: prefix = 6; break;  \
        case 0xfe000000: prefix = 7; break;  \
        case 0xff000000: prefix = 8; break;  \
        case 0xff800000: prefix = 9; break;  \
        case 0xffc00000: prefix = 10; break; \
        case 0xffe00000: prefix = 11; break; \
        case 0xfff00000: prefix = 12; break; \
        case 0xfff80000: prefix = 13; break; \
        case 0xfffc0000: prefix = 14; break; \
        case 0xfffe0000: prefix = 15; break; \
        case 0xffff0000: prefix = 16; break; \
        case 0xffff8000: prefix = 17; break; \
        case 0xffffc000: prefix = 18; break; \
        case 0xffffe000: prefix = 19; break; \
        case 0xfffff000: prefix = 20; break; \
        case 0xfffff800: prefix = 21; break; \
        case 0xfffffc00: prefix = 22; break; \
        case 0xfffffe00: prefix = 23; break; \
        case 0xffffff00: prefix = 24; break; \
        case 0xffffff80: prefix = 25; break; \
        case 0xffffffc0: prefix = 26; break; \
        case 0xffffffe0: prefix = 27; break; \
        case 0xfffffff0: prefix = 28; break; \
        case 0xfffffff8: prefix = 29; break; \
        case 0xfffffffc: prefix = 30; break; \
        case 0xfffffffe: prefix = 31; break; \
        case 0xffffffff: prefix = 32; break; \
         /* Error indication */              \
        default: prefix = 33; break;         \
    }

/** Fast conversion of the prefix to network mask */
#define PREFIX2MASK(prefix) htonl(prefix == 0 ? 0 : (~0) << (32 - (prefix)))

/* TA name pointer */
extern char *ta_name;

#define METRIC_DEFAULT  20

/*
 * Access routines prototypes (comply to procedure types
 * specified in rcf_ch_api.h).
 */
static te_errno interface_list(unsigned int, const char *, char **);

static te_errno net_addr_add(unsigned int, const char *, const char *,
                             const char *, const char *);
static te_errno net_addr_del(unsigned int, const char *,
                             const char *, const char *);
static te_errno net_addr_list(unsigned int, const char *, char **,
                              const char *);

static te_errno prefix_get(unsigned int, const char *, char *,
                           const char *, const char *);
static te_errno prefix_set(unsigned int, const char *, const char *,
                           const char *, const char *);


static te_errno broadcast_get(unsigned int, const char *, char *,
                              const char *, const char *);
static te_errno broadcast_set(unsigned int, const char *, const char *,
                              const char *, const char *);

static te_errno link_addr_get(unsigned int, const char *, char *,
                              const char *);

static te_errno bcast_link_addr_get(unsigned int, const char *,
                                    char *, const char *);

static te_errno ifindex_get(unsigned int, const char *, char *,
                            const char *);

static te_errno status_get(unsigned int, const char *, char *,
                           const char *);
static te_errno status_set(unsigned int, const char *, const char *,
                           const char *);

static te_errno promisc_get(unsigned int, const char *, char *,
                            const char *);
static te_errno promisc_set(unsigned int, const char *, const char *,
                            const char *);

static te_errno mtu_get(unsigned int, const char *, char *,
                        const char *);

static te_errno neigh_state_get(unsigned int, const char *, char *,
                                const char *, const char *);
                           
static te_errno neigh_get(unsigned int, const char *, char *,
                          const char *, const char *);
static te_errno neigh_set(unsigned int, const char *, const char *,
                          const char *, const char *);
static te_errno neigh_add(unsigned int, const char *, const char *,
                          const char *, const char *);
static te_errno neigh_del(unsigned int, const char *,
                          const char *, const char *);
static te_errno neigh_list(unsigned int, const char *, char **,
                           const char *);

//Multicast
static te_errno mcast_link_addr_add(unsigned int, const char *,
                                    const char *, const char *,
                                    const char *);
static te_errno mcast_link_addr_del(unsigned int, const char *,
                                    const char *, const char *);
static te_errno mcast_link_addr_list(unsigned int, const char *, char **,
                                     const char *);
//Multicast

/* 
 * This is a bit of hack - there are same handlers for static and dynamic
 * branches, handler discovers dynamic subtree by presence of
 * "dynamic" in OID. But list method does not contain the last subid.
 */
static te_errno
neigh_dynamic_list(unsigned int gid, const char *oid, char **list, 
                   const char *ifname)
{
    UNUSED(oid);
    
    return neigh_list(gid, "dynamic", list, ifname);
}                   

static te_errno route_dev_get(unsigned int, const char *, char *,
                              const char *);
static te_errno route_dev_set(unsigned int, const char *, const char *,
                              const char *);
static te_errno route_add(unsigned int, const char *, const char *,
                          const char *);
static te_errno route_del(unsigned int, const char *,
                          const char *);
static te_errno route_get(unsigned int, const char *, char *,
                          const char *);
static te_errno route_set(unsigned int, const char *, const char *,
                          const char *);
static te_errno route_list(unsigned int, const char *, char **);

static te_errno route_commit(unsigned int, const cfg_oid *);

/*
 * Access routines prototypes (comply to procedure types
 * specified in rcf_ch_api.h).
 */
static te_errno env_get(unsigned int, const char *, char *,
                        const char *);
static te_errno env_set(unsigned int, const char *, const char *,
                        const char *);
static te_errno env_add(unsigned int, const char *, const char *,
                        const char *);
static te_errno env_del(unsigned int, const char *,
                        const char *);
static te_errno env_list(unsigned int, const char *, char **);

/** Environment variables hidden in list operation */
static const char * const env_hidden[] = {
    "SSH_CLIENT",
    "SSH_CONNECTION",
    "SUDO_COMMAND",
    "TE_RPC_PORT",
    "TE_LOG_PORT",
    "TARPC_DL_NAME",
    "TCE_CONNECTION",
    "TZ",
    "_",
    "CYGWIN"
};

/** win32 statistics */
#ifndef ENABLE_IFCONFIG_STATS
#define ENABLE_IFCONFIG_STATS
#endif

#ifndef ENABLE_NET_SNMP_STATS
#define ENABLE_NET_SNMP_STATS
#endif

#ifdef ENABLE_IFCONFIG_STATS
extern te_errno ta_win32_conf_net_if_stats_init();
#endif

#ifdef ENABLE_NET_SNMP_STATS
extern te_errno ta_win32_conf_net_snmp_stats_init();
#endif

/* win32 Test Agent configuration tree */


static rcf_pch_cfg_object node_route;

RCF_PCH_CFG_NODE_RWC(node_route_dev, "dev", NULL, NULL,
                     route_dev_get, route_dev_set, &node_route);
static rcf_pch_cfg_object node_route =
    { "route", 0, &node_route_dev, NULL,
      (rcf_ch_cfg_get)route_get, (rcf_ch_cfg_set)route_set,
      (rcf_ch_cfg_add)route_add, (rcf_ch_cfg_del)route_del,
      (rcf_ch_cfg_list)route_list,
      (rcf_ch_cfg_commit)route_commit, NULL};

RCF_PCH_CFG_NODE_RO(node_neigh_state, "state",
                    NULL, NULL,
                    (rcf_ch_cfg_list)neigh_state_get);

static rcf_pch_cfg_object node_neigh_dynamic =
    { "neigh_dynamic", 0, &node_neigh_state, NULL,
      (rcf_ch_cfg_get)neigh_get, (rcf_ch_cfg_set)neigh_set,
      (rcf_ch_cfg_add)neigh_add, (rcf_ch_cfg_del)neigh_del,
      (rcf_ch_cfg_list)neigh_dynamic_list, NULL, NULL};

static rcf_pch_cfg_object node_neigh_static =
    { "neigh_static", 0, NULL, &node_neigh_dynamic,
      (rcf_ch_cfg_get)neigh_get, (rcf_ch_cfg_set)neigh_set,
      (rcf_ch_cfg_add)neigh_add, (rcf_ch_cfg_del)neigh_del,
      (rcf_ch_cfg_list)neigh_list, NULL, NULL};

//Multicast
static rcf_pch_cfg_object node_mcast_link_addr =
 { "mcast_link_addr", 0, NULL, &node_neigh_static,
  NULL, NULL, (rcf_ch_cfg_add)mcast_link_addr_add,
  (rcf_ch_cfg_del)mcast_link_addr_del,
  (rcf_ch_cfg_list)mcast_link_addr_list, NULL, NULL };
//

RCF_PCH_CFG_NODE_RW(node_promisc, "promisc", NULL, &node_mcast_link_addr,
                    promisc_get, promisc_set);

RCF_PCH_CFG_NODE_RW(node_status, "status", NULL, &node_promisc,
                    status_get, status_set);

RCF_PCH_CFG_NODE_RW(node_mtu, "mtu", NULL, &node_status,
                    mtu_get, NULL);

RCF_PCH_CFG_NODE_RO(node_bcast_link_addr, "bcast_link_addr", NULL,
                    &node_mtu, bcast_link_addr_get);

RCF_PCH_CFG_NODE_RO(node_link_addr, "link_addr", NULL,
                    &node_bcast_link_addr,
                    link_addr_get);

RCF_PCH_CFG_NODE_RW(node_broadcast, "broadcast", NULL, NULL,
                    broadcast_get, broadcast_set);
RCF_PCH_CFG_NODE_RW(node_prefix, "prefix", NULL, &node_broadcast,
                    prefix_get, prefix_set);

static rcf_pch_cfg_object node_net_addr =
    { "net_addr", 0, &node_prefix, &node_link_addr,
      (rcf_ch_cfg_get)prefix_get, (rcf_ch_cfg_set)prefix_set,
      (rcf_ch_cfg_add)net_addr_add, (rcf_ch_cfg_del)net_addr_del,
      (rcf_ch_cfg_list)net_addr_list, NULL, NULL };

RCF_PCH_CFG_NODE_RO(node_ifindex, "index", NULL, &node_net_addr,
                    ifindex_get);

RCF_PCH_CFG_NODE_COLLECTION(node_interface, "interface",
                            &node_ifindex, &node_route,
                            NULL, NULL, interface_list, NULL);

static rcf_pch_cfg_object node_env =
    { "env", 0, NULL, &node_interface,
      (rcf_ch_cfg_get)env_get, (rcf_ch_cfg_set)env_set,
      (rcf_ch_cfg_add)env_add, (rcf_ch_cfg_del)env_del,
      (rcf_ch_cfg_list)env_list, NULL, NULL };

RCF_PCH_CFG_NODE_AGENT(node_agent, &node_env);

const char *te_lockdir = "/tmp";

/** Mapping of EF ports to interface indices */
static DWORD ef_index[2] = { 0, 0 };

/* Convert wide string to usual one */
static char *
w2a(WCHAR *str)
{
    static char buf[256];
    BOOL        b;
    
    WideCharToMultiByte(CP_ACP, 0, str, -1, buf, sizeof(buf), "-", &b);
        
    return buf;        
}

static te_errno
efport2ifindex(void)
{
/* Path to network components in the registry */
#define NET_PATH        "SYSTEM\\CurrentControlSet\\Control\\Class\\" \
                        "{4D36E972-E325-11CE-BFC1-08002bE10318}"
#define ENV_PATH        "SYSTEM\\CurrentControlSet\\Control\\" \
                        "Session Manager\\Environment"

#define NDIS_EFAB       "dev_c101_ndis_"
#define NDIS_SF         "sfe_ndis_"
#define BUFSIZE         256
#define AMOUNT_OF_GUIDS         5

    HKEY key, subkey;
    
    static char subkey_name[BUFSIZE];
    static char subkey_path[BUFSIZE];
    static char value[BUFSIZE];
    
    static char guid1[AMOUNT_OF_GUIDS][BUFSIZE];
    static char guid2[AMOUNT_OF_GUIDS][BUFSIZE];
    static int driver_type_reported = 0;

    static int guid1_amount = 0, guid2_amount = 0;
    static int guids_found = 0;
    
    DWORD subkey_size;
    DWORD value_size = BUFSIZE;
    
    FILETIME tmp;
    ULONG    size = 0;
    
    PIP_INTERFACE_INFO iftable;

    char driver_type[BUFSIZE];
    
    int i, j;
    unsigned int old_ef_index[2];
    
    driver_type[0] = 0;

    /* Querying environment variable TE_USE_EFAB_DRIVER value */

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, ENV_PATH, 0, 
                     KEY_READ, &key) != ERROR_SUCCESS) 
    {
        ERROR("RegOpenKeyEx() failed with errno %lu", GetLastError());
        return TE_RC(TE_TA_WIN32, TE_EFAULT);
    }
    RegQueryValueEx(key, "TE_USE_EFAB_DRIVER", 
                            NULL, NULL, driver_type, 
                            &value_size);
    RegCloseKey(key);

    if (driver_type[0] != 0)
    {
      int t = atoi(driver_type);
      if (t)
      {
        strcpy(driver_type, NDIS_EFAB);
        if (!driver_type_reported)
        {
          RING("Efab drivers will be used to resolve ef* interfaces,"
               " if any");
        }
      }
      else
      {
        strcpy(driver_type, NDIS_SF);
        if (!driver_type_reported)
        {
          RING("Solarflare drivers will be used to resolve ef* interfaces,"
               " if any");
        }
      }
    }
    else
    {
      strcpy(driver_type, NDIS_SF);
      if (!driver_type_reported)
      {
        RING("Solarflare drivers will be used to resolve ef* interfaces, "
             "if any");
      }
    }
    driver_type_reported = 1;

    if (!guids_found)
    {
        /* Obtaining interface indexes */
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, NET_PATH, 0, 
                         KEY_READ, &key) != ERROR_SUCCESS) 
        {
            ERROR("RegOpenKeyEx() failed with errno %lu", GetLastError());
            return TE_RC(TE_TA_WIN32, TE_EFAULT);
        }

        for (i = 0, subkey_size = value_size = BUFSIZE; 
             RegEnumKeyEx(key, i, subkey_name, &subkey_size, 
                          NULL, NULL, NULL, &tmp) != ERROR_NO_MORE_ITEMS;
             i++, subkey_size = value_size = BUFSIZE)
        { 
            sprintf(subkey_path, "%s\\%s", NET_PATH, subkey_name);
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey_path, 
                             0, KEY_READ, &subkey) != ERROR_SUCCESS)
            {
               continue;
            }
        
            if (RegQueryValueEx(subkey, "MatchingDeviceId", 
                                NULL, NULL, (unsigned char *)value, 
                                &value_size) != ERROR_SUCCESS) 
            {
                /* Field with device ID is absent, may its virtual device */
                RegCloseKey(subkey);
                continue;
            }
            if ((strstr(value, driver_type) != NULL))
            {
                char driver[BUFSIZE];
                unsigned char *guid;
                
                strcpy(driver, driver_type);
                strcat(driver, "0");
           
                guid = strstr(value, driver) != NULL ? 
                              guid1[guid1_amount++] : guid2[guid2_amount++];
                    
                value_size = BUFSIZE;
                if (RegQueryValueEx(subkey, "NetCfgInstanceId", 
                                    NULL, NULL, guid, &value_size) != 0)
                {
                    ERROR("RegQueryValueEx(%s) failed with errno %u", 
                          subkey_path, GetLastError());
                    RegCloseKey(subkey);
                    RegCloseKey(key);
                    return TE_RC(TE_TA_WIN32, TE_EFAULT);
                }
            }
            RegCloseKey(subkey);
        } 
        RegCloseKey(key);
        
        if (guid1_amount == 0 || guid2_amount == 0)
            return 0;
        guids_found = 1;
    }

    if ((iftable = (PIP_INTERFACE_INFO)malloc(sizeof(*iftable))) == NULL) 
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);
                                                                        
    if (GetInterfaceInfo(iftable, &size) == ERROR_INSUFFICIENT_BUFFER)
    {     
        PIP_INTERFACE_INFO new_table = 
            (PIP_INTERFACE_INFO)realloc(iftable, size);

        if (new_table == NULL)
        {
            free(iftable);
            return TE_RC(TE_TA_WIN32, TE_ENOMEM);
        }
        
        iftable = new_table;            
    }                                                               
    else 
    {                                                               
        free(iftable); 
        ERROR("GetInterfaceInfo() failed");
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);
    }                                                               
                                                                    
    if (GetInterfaceInfo(iftable, &size) != NO_ERROR)                  
    {                                                               
        ERROR("GetInterfaceInfo() failed");
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);
    }                                                               

    old_ef_index[0] = ef_index[0];
    old_ef_index[1] = ef_index[1];
    ef_index[0] = 0;
    ef_index[1] = 0;
    for (i = 0; i < iftable->NumAdapters; i++)
    {
        for(j = 0; j < guid1_amount; j++)
        {
          if (strstr(w2a(iftable->Adapter[i].Name), guid1[j]) != NULL)
            ef_index[0] = iftable->Adapter[i].Index;
        }
        for(j = 0; j < guid2_amount; j++)
        {
          if (strstr(w2a(iftable->Adapter[i].Name), guid2[j]) != NULL)
            ef_index[1] = iftable->Adapter[i].Index;
        }
    }
    free(iftable); 
    
    if (ef_index[0] > 0)
    {
        if (old_ef_index[0] != ef_index[0])
            RING("Interface index for EF port 1: %d", ef_index[0]);
    }
    else
    {
        if (old_ef_index[0] != 0)
        {
            RING("Can't find index for EF port 1");
        }
    }

    if (ef_index[1] > 0)
    {
        if (old_ef_index[1] != ef_index[1])
        {
            RING("Interface index for EF port 2: %d", ef_index[1]);
        }
    }
    else
    {
        if (old_ef_index[1] != 0)
        {
            RING("Can't find index for EF port 2");
        }
    }
    
    return 0;

#undef AMOUNT_OF_GUIDS
#undef BUFSIZE    
#undef NET_PATH
#undef NDIS_SF
#undef NDIS_EFAB
}

static MIB_IFROW if_entry;

/** Convert interface name to interface index */
static DWORD
ifname2ifindex(const char *ifname)
{
    char   *s;
    char   *tmp;                                   
    te_bool ef = FALSE;
    DWORD   index;

    if (ifname == NULL)
        return 0;

    if (strcmp_start("intf", ifname) == 0)
        s = (char *)ifname + strlen("intf");
    else if (strcmp_start("ef", ifname) == 0)
    {
        s = (char *)ifname + strlen("ef");
        ef = TRUE;
    }
    else
        return 0;
    index = strtol(s, &tmp, 10);
    if (tmp == s || *tmp != 0)
        return 0;
        
    if (!ef)
        return index;
        
    if (index < 1 || index > 2)
        return 0;

    efport2ifindex();
        
    return ef_index[index - 1];
}

/** Convert interface index to interface name */
char *
ifindex2ifname(DWORD ifindex)
{
    static char ifname[16];
    
    if (ef_index[0] == ifindex)
        sprintf(ifname, "ef1");
    else if (ef_index[1] == ifindex)
        sprintf(ifname, "ef2");
    else
        sprintf(ifname, "intf%u", (unsigned int)ifindex);
        
    return ifname;        
}

/** Update information in if_entry. Local variable ifname should exist */
#define GET_IF_ENTRY \
    do {                                                        \
        if ((if_entry.dwIndex = ifname2ifindex(ifname)) == 0 || \
            GetIfEntry(&if_entry) != 0)                         \
            return TE_RC(TE_TA_WIN32, TE_ENOENT);               \
    } while (0)

#define RETURN_BUF

/**
 * Allocate memory and get some SNMP-like table; variable table of the
 * type _type should be defined.
 */
#define GET_TABLE(_type, _func) \
    do {                                                                \
        DWORD size = 0, rc;                                             \
                                                                        \
        if ((table = (_type *)malloc(sizeof(_type))) == NULL)           \
            return TE_RC(TE_TA_WIN32, TE_ENOMEM);                       \
                                                                        \
        if ((rc = _func(table, &size, 0)) == ERROR_INSUFFICIENT_BUFFER) \
        {                                                               \
            _type *new_table = (_type *)realloc(table, size);           \
                                                                        \
            if (new_table == NULL)                                      \
            {                                                           \
                free(table);                                            \
                return TE_RC(TE_TA_WIN32, TE_ENOMEM);                   \
            }                                                           \
            table = new_table;                                          \
        }                                                               \
        else if (rc == NO_ERROR)                                        \
        {                                                               \
            free(table);                                                \
            table = NULL;                                               \
        }                                                               \
        else                                                            \
        {                                                               \
            ERROR("%s failed, error %d", #_func, rc);                   \
            free(table);                                                \
            return TE_RC(TE_TA_WIN32, TE_EWIN);                         \
        }                                                               \
                                                                        \
        if ((rc = _func(table, &size, 0)) != NO_ERROR)                  \
        {                                                               \
            ERROR("%s failed, error %d", #_func, rc);                   \
            free(table);                                                \
            return TE_RC(TE_TA_WIN32, TE_EWIN);                         \
        }                                                               \
                                                                        \
        if (table->dwNumEntries == 0)                                   \
        {                                                               \
            free(table);                                                \
            table = NULL;                                               \
        }                                                               \
    } while (0)


/** Find an interface for destination IP */
static int
find_ifindex(DWORD addr, DWORD *ifindex)
{
    MIB_IPFORWARDTABLE *table;
    
    DWORD index = 0;
    DWORD mask_max = 0;
    int   i;

    GET_TABLE(MIB_IPFORWARDTABLE, GetIpForwardTable);
    
    if (table == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        if ((table->table[i].dwForwardDest & 
             table->table[i].dwForwardMask) !=
            (addr &  table->table[i].dwForwardMask))
        {
            continue;
        }
        
        if (ntohl(table->table[i].dwForwardMask) > mask_max || index == 0)
        {
            mask_max = table->table[i].dwForwardMask;
            index = table->table[i].dwForwardIfIndex;
            if (mask_max == 0xFFFFFFFF)
                break;
        }
    }
    free(table);

    if (index == 0)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);
        
    *ifindex = index;
    
    return 0;
}


/**
 * Get root of the tree of supported objects.
 *
 * @return root pointer
 */
rcf_pch_cfg_object *
rcf_ch_conf_root()
{
    static te_bool init = FALSE;
    
    /* Link RPC nodes */
    if (!init)
    {
        if (efport2ifindex() != 0)
            return NULL;
        
        init = TRUE;
#ifdef RCF_RPC
        rcf_pch_rpc_init();
#endif
#ifdef WITH_ISCSI
        if (iscsi_initiator_conf_init() != 0)
        {
            return NULL;
        }
#endif
        rcf_pch_rsrc_init();
        rcf_pch_rsrc_info("/agent/interface", 
                          rcf_pch_rsrc_grab_dummy,
                          rcf_pch_rsrc_release_dummy);


#ifdef ENABLE_IFCONFIG_STATS
        if (ta_win32_conf_net_if_stats_init() != 0)
            return NULL;
#endif
#ifdef ENABLE_NET_SNMP_STATS
        if (ta_win32_conf_net_snmp_stats_init() != 0)
            return NULL;
#endif

    }


    return &node_agent;
}

/**
 * Get Test Agent name.
 *
 * @return name pointer
 */
const char *
rcf_ch_conf_agent()
{
    return ta_name;
}

/**
 * Release resources allocated for configuration support.
 */
void
rcf_ch_conf_release()
{
}

/**
 * Get instance list for object "agent/interface".
 *
 * @param gid           group identifier (unused)
 * @param id            full identifier of the father instance
 * @param list          location for the list pointer
 *
 * @return status code
 * @retval 0            success
 * @retval TE_ENOMEM       cannot allocate memory
 */
static te_errno
interface_list(unsigned int gid, const char *oid, char **list)
{
    MIB_IFTABLE *table;
    char        *s = buf;
    int          i;

    UNUSED(gid);
    UNUSED(oid);

    GET_TABLE(MIB_IFTABLE, GetIfTable);
    if (table == NULL)
    {
        if ((*list = strdup(" ")) == NULL)
            return TE_RC(TE_TA_WIN32, TE_ENOMEM);
        else
            return 0;
    }

    buf[0] = 0;

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        s += snprintf(s, sizeof(buf) - (s - buf),
                      "%s ", ifindex2ifname(table->table[i].dwIndex));
    }                      

    free(table);

    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);

    return 0;
}

/** Convert interface name to interface index */
static int
name2index(const char *ifname, DWORD *ifindex)
{
    GET_IF_ENTRY;
    
    *ifindex = if_entry.dwIndex;
    
    return 0;
}

/**
 * Get index of the interface.
 *
 * @param gid           request group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         location for interface index
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
ifindex_get(unsigned int gid, const char *oid, char *value,
            const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;

    sprintf(value, "%lu", if_entry.dwIndex);

    return 0;
}

/** Check, if IP address exist for the current if_entry */
static int
ip_addr_exist(DWORD addr, MIB_IPADDRROW *data)
{
    MIB_IPADDRTABLE *table;
    int              i;

    GET_TABLE(MIB_IPADDRTABLE, GetIpAddrTable);

    if (table == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        if (table->table[i].dwIndex == if_entry.dwIndex &&
            table->table[i].dwAddr == addr)
        {
            if (data != NULL)
                *data = table->table[i];
            free(table);
            return 0;
        }
    }

    free(table);

    return TE_RC(TE_TA_WIN32, TE_ENOENT);
}

/** Parse address and fill mask by specified or default value */
static int
get_addr_mask(const char *addr, const char *value, DWORD *p_a, DWORD *p_m)
{
    DWORD a;
    int   prefix;
    char *end;
    
    if ((a = inet_addr(addr)) == INADDR_NONE ||
        (a & 0xe0000000) == 0xe0000000)
        return TE_RC(TE_TA_WIN32, TE_EINVAL);
        
    prefix = strtol(value, &end, 10);
    if (value == end || *end != 0)
    {
        ERROR("Invalid value '%s' of prefix length", value);
        return TE_RC(TE_TA_WIN32, TE_EFMT);
    }

    if (prefix > 32)
    {
        ERROR("Invalid prefix '%s' to be set", value);
        return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }
    
    if (prefix != 0)
        *p_m = PREFIX2MASK(prefix);
    else
        *p_m = (a & htonl(0x80000000)) == 0 ? htonl(0xFF000000) :
               (a & htonl(0xC0000000)) == htonl(0x80000000) ?
               htonl(0xFFFF0000) : htonl(0xFFFFFF00);
     *p_a = a;         
     
     return 0;
}

/**
 * Configure IPv4 address for the interface.
 * If the address does not exist, alias interface is created.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value string (unused)
 * @param ifname        name of the interface (like "eth0")
 * @param addr          IPv4 address in dotted notation
 *
 * @return status code
 */
static te_errno
net_addr_add(unsigned int gid, const char *oid, const char *value,
             const char *ifname, const char *addr)
{
    DWORD a, m;

    ULONG nte_context;
    ULONG nte_instance;

    int rc;

    UNUSED(gid);
    UNUSED(oid);
    
    if ((rc = get_addr_mask(addr, value, &a, &m)) != 0)
        return rc;

    GET_IF_ENTRY;

    if ((rc = AddIPAddress(*(IPAddr *)&a, *(IPAddr *)&m, if_entry.dwIndex,
                            &nte_context, &nte_instance)) != NO_ERROR)
    {
        ERROR("AddIpAddress() failed, error %d", rc);
        return TE_RC(TE_TA_WIN32, TE_EWIN);
    }

    return 0;
}

/**
 * Clear interface address of the down interface.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param ifname        name of the interface (like "eth0")
 * @param addr          IPv4 address in dotted notation
 *
 * @return status code
 */

static te_errno
net_addr_del(unsigned int gid, const char *oid,
             const char *ifname, const char *addr)
{
    PIP_ADAPTER_INFO table, info; 
    DWORD            size = 0; 

    DWORD a;
    int   rc;

    UNUSED(gid);
    UNUSED(oid);

    if ((a = inet_addr(addr)) == INADDR_NONE)
        return TE_RC(TE_TA_WIN32, TE_EINVAL);

    GET_IF_ENTRY;

    GetAdaptersInfo(NULL, &size);
    table = (PIP_ADAPTER_INFO)malloc(size);                      
                                                                    
    if ((rc = GetAdaptersInfo(table, &size)) != NO_ERROR)                  
    {                                                               
        ERROR("GetAdaptersInfo failed, error %d", rc);
        free(table);                                                
        return TE_RC(TE_TA_WIN32, TE_EWIN);                          
    }                                                               

    for (info = table; info != NULL; info = info->Next)
    {
       IP_ADDR_STRING *addrlist;
       
       if (info->Index != if_entry.dwIndex)
           continue;
       
       for (addrlist = &(info->IpAddressList); 
            addrlist != NULL;
            addrlist = addrlist->Next)
        {
            if (strcmp(addr, addrlist->IpAddress.String) == 0)
            {
                if ((rc = DeleteIPAddress(addrlist->Context)) != NO_ERROR)
                {
                    ERROR("DeleteIPAddress() failed; error %d\n", rc);
                    free(table);
                    return TE_RC(TE_TA_WIN32, TE_EWIN);;
                }
                free(table);
                return 0;
            }
        }
    }
    free(table);

    return TE_RC(TE_TA_WIN32, TE_ENOENT);
}

/**
 * Get instance list for object "agent/interface/net_addr".
 *
 * @param gid           group identifier (unused)
 * @param id            full identifier of the father instance
 * @param list          location for the list pointer
 * @param ifname        interface name
 *
 * @return status code
 * @retval 0                    success
 * @retval TE_ENOENT            no such instance
 * @retval TE_ENOMEM               cannot allocate memory
 */
static te_errno
net_addr_list(unsigned int gid, const char *oid, char **list,
              const char *ifname)
{
    MIB_IPADDRTABLE *table;
    int              i;
    char            *s = buf;

    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;
    GET_TABLE(MIB_IPADDRTABLE, GetIpAddrTable);
    if (table == NULL)
    {
        if ((*list = strdup(" ")) == NULL)
            return TE_RC(TE_TA_WIN32, TE_ENOMEM);
        else
            return 0;
    }

    buf[0] = 0;

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        if (table->table[i].dwIndex != if_entry.dwIndex)
            continue;

        s += snprintf(s, sizeof(buf) - (s - buf), "%s ",
                      inet_ntoa(*(struct in_addr *)
                                    &(table->table[i].dwAddr)));
    }

    free(table);

    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);

    return 0;
}

/**
 * Get netmask (prefix) of the interface address.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         netmask location (netmask is presented in dotted
 *                      notation)
 * @param ifname        name of the interface (like "eth0")
 * @param addr          IPv4 address in dotted notation
 *
 * @return status code
 */
static te_errno
prefix_get(unsigned int gid, const char *oid, char *value,
            const char *ifname, const char *addr)
{
    MIB_IPADDRROW data;
    DWORD         a;
    int           rc;
    int           prefix;

    UNUSED(gid);
    UNUSED(oid);

    if ((a = inet_addr(addr)) == INADDR_NONE)
    {
        if (strcmp(addr, "255.255.255.255") != 0)
            return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }

    GET_IF_ENTRY;

    if ((rc = ip_addr_exist(a, &data)) != 0)
        return rc;

    MASK2PREFIX(data.dwMask, prefix);

    snprintf(value, RCF_MAX_VAL, "%d", prefix);

    return 0;
}

/**
 * Change netmask (prefix) of the interface address.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         pointer to the new network mask in dotted notation
 * @param ifname        name of the interface (like "eth0")
 * @param addr          IPv4 address in dotted notation
 *
 * @return status code
 */
static te_errno
prefix_set(unsigned int gid, const char *oid, const char *value,
            const char *ifname, const char *addr)
{
    DWORD a, m;
    int   rc;

    ULONG nte_context;
    ULONG nte_instance;

    UNUSED(gid);
    UNUSED(oid);

    if ((rc = get_addr_mask(addr, value, &a, &m)) != 0)
        return rc;
    
    if ((a = inet_addr(addr)) == INADDR_NONE)
        return TE_RC(TE_TA_WIN32, TE_EINVAL);

    if ((rc = net_addr_del(0, NULL, ifname, addr)) != 0)
        return rc;

    if ((rc = AddIPAddress(*(IPAddr *)&a, *(IPAddr *)&m,
                           if_entry.dwIndex, &nte_context, 
                           &nte_instance)) != NO_ERROR)
    {
        ERROR("AddIpAddr() failed, error %d", rc);
        return TE_RC(TE_TA_WIN32, TE_EWIN);
    }

    return 0;
}

/**
 * Get broadcast address of the interface address.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         netmask location (netmask is presented in dotted
 *                      notation)
 * @param ifname        name of the interface (like "eth0")
 * @param addr          IPv4 address in dotted notation
 *
 * @return status code
 */
static te_errno
broadcast_get(unsigned int gid, const char *oid, char *value,
              const char *ifname, const char *addr)
{
    MIB_IPADDRROW data;
    DWORD         a;
    DWORD         b;
    int           rc;

    UNUSED(gid);
    UNUSED(oid);

    if ((a = inet_addr(addr)) == INADDR_NONE)
    {
        if (strcmp(addr, "255.255.255.255") != 0)
            return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }

    GET_IF_ENTRY;

    if ((rc = ip_addr_exist(a, &data)) != 0)
        return rc;

    b = (~data.dwMask) | (a & data.dwMask);

    snprintf(value, RCF_MAX_VAL, "%s", inet_ntoa(*(struct in_addr *)&b));

    return 0;
}

/**
 * Change broadcast address of the interface address - does nothing.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         pointer to the new network mask in dotted notation
 * @param ifname        name of the interface (like "eth0")
 * @param addr          IPv4 address in dotted notation
 *
 * @return status code
 */
static te_errno
broadcast_set(unsigned int gid, const char *oid, const char *value,
              const char *ifname, const char *addr)
{
    MIB_IPADDRROW data;
    DWORD         a;
    int           rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);

    if ((a = inet_addr(addr)) == INADDR_NONE)
    {
        if (strcmp(addr, "255.255.255.255") != 0)
            return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }

    GET_IF_ENTRY;

    if ((rc = ip_addr_exist(a, &data)) != 0)
        return rc;
    return 0;
}

/**
 * Get hardware address of the interface.
 * Only MAC addresses are supported now.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         location for hardware address (address is returned
 *                      as XX:XX:XX:XX:XX:XX)
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
link_addr_get(unsigned int gid, const char *oid, char *value,
              const char *ifname)
{
    uint8_t *ptr = if_entry.bPhysAddr;

    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;

    if (if_entry.dwPhysAddrLen != 6)
        snprintf(value, RCF_MAX_VAL, "00:00:00:00:00:00");
    else
        snprintf(value, RCF_MAX_VAL, "%02x:%02x:%02x:%02x:%02x:%02x",
                 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);

    return 0;
}


/**
 * Get broadcast hardware address of the interface.
 * Because we can not manipulate broadcast MAC addresses on windows
 * the ff:ff:ff:ff:ff:ff is returned for test purposes.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         broadcast hardware address (it should be
 *                      provided as XX:XX:XX:XX:XX:XX string)
 * @param ifname        name of the interface (like "eth0")
 *
 * @return              Status code
 */
static te_errno
bcast_link_addr_get(unsigned int gid, const char *oid,
                    char *value, const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;

    if (value == NULL)
    {
        ERROR("A buffer for broadcast link layer address is not provided");
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }

    snprintf(value, RCF_MAX_VAL, "ff:ff:ff:ff:ff:ff");
    return 0;
}


/**
 * Get MTU of the interface.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value location
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
mtu_get(unsigned int gid, const char *oid, char *value,
        const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;

    sprintf(value, "%lu", if_entry.dwMtu);

    return 0;
}

/**
 * Get status of the interface ("0" - down or "1" - up).
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value location
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
status_get(unsigned int gid, const char *oid, char *value,
           const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;

    sprintf(value, "%d",
            if_entry.dwOperStatus == MIB_IF_OPER_STATUS_CONNECTED ||
            if_entry.dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL ?
                1 : 0);

    return 0;
}

/**
 * Change status of the interface. If virtual interface is put to down
 * state, it is de-installed and information about it is stored in the
 * list of down interfaces.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         new value pointer
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
status_set(unsigned int gid, const char *oid, const char *value,
           const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    GET_IF_ENTRY;

    if (strcmp(value, "0") == 0)
        if_entry.dwAdminStatus = MIB_IF_ADMIN_STATUS_DOWN;
    else if (strcmp(value, "1") == 0)
        if_entry.dwAdminStatus = MIB_IF_ADMIN_STATUS_UP;
    else
        return TE_RC(TE_TA_WIN32, TE_EINVAL);

    if (SetIfEntry(&if_entry) != 0)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    return 0;
}


/**
 * Get promiscuous mode of the interface ("0" - disabled or "1" - enabled).
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value location
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
promisc_get(unsigned int gid, const char *oid, char *value,
            const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    /* TODO */
    *value = '0';
    value[1] = '\0';
    UNUSED(ifname);

    return 0;
}

/**
 * Change status of the interface. If virtual interface is put to down
 * state, it is de-installed and information about it is stored in the
 * list of down interfaces.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         new value pointer
 * @param ifname        name of the interface (like "eth0")
 *
 * @return status code
 */
static te_errno
promisc_set(unsigned int gid, const char *oid, const char *value,
            const char *ifname)
{
    UNUSED(gid);
    UNUSED(oid);

    /* TODO */
    if (strcmp(value, "0") == 0)
        return 0;
    UNUSED(ifname);

    return TE_RC(TE_TA_WIN32, TE_EOPNOTSUPP);
}


/** Find neighbour entry and return its parameters */
static te_errno
neigh_find(const char *oid, const char *ifname, const char *addr,
           char *mac)
{
    MIB_IPNETTABLE *table;
    DWORD           a;
    int             i;
    DWORD           type = strstr(oid, "dynamic") == NULL ? 
                           ARP_STATIC : ARP_DYNAMIC;
    DWORD           ifindex = ifname2ifindex(ifname);
    
    if (ifindex == 0)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    if ((a = inet_addr(addr)) == INADDR_NONE)
        return TE_RC(TE_TA_WIN32, TE_EINVAL);
        
    GET_TABLE(MIB_IPNETTABLE, GetIpNetTable);
    if (table == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        if (a == table->table[i].dwAddr && table->table[i].dwType == type)
        {
            uint8_t *ptr = table->table[i].bPhysAddr;
            
            if (table->table[i].dwIndex != ifindex)
                continue;

            if (table->table[i].dwPhysAddrLen != 6)
            {
                free(table);
                return TE_RC(TE_TA_WIN32, TE_ENOENT);
            }
            if (mac != NULL)
                snprintf(mac, RCF_MAX_VAL, "%02x:%02x:%02x:%02x:%02x:%02x",
                         ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
            free(table);
            return 0;
        }
    }

    free(table);

    return TE_RC(TE_TA_WIN32, TE_ENOENT);
}           

/**
 * Get neighbour entry state.
 *
 * @param gid            group identifier (unused)
 * @param oid            full object instence identifier (unused)
 * @param value          location for the value
 *                       (XX:XX:XX:XX:XX:XX is returned)
 * @param ifname         interface name
 * @param addr           IP address in human notation
 *
 * @return Status code
 */
te_errno
neigh_state_get(unsigned int gid, const char *oid, char *value,
                const char *ifname, const char *addr)
{
    int  rc;
    
    UNUSED(gid);
    
    if (strstr(oid, "dynamic") == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    
    if ((rc = neigh_find(oid, ifname, addr, NULL)) != 0)
        return rc;
        
    sprintf(value, "%u", CS_NEIGH_REACHABLE);
        
    return 0;
}

/**
 * Get neighbour entry value (hardware address corresponding to IP).
 *
 * @param gid            group identifier (unused)
 * @param oid            full object instence identifier (unused)
 * @param value          location for the value
 *                       (XX:XX:XX:XX:XX:XX is returned)
 * @param ifname         interface name
 * @param addr           IP address in human notation
 *
 * @return Status code
 */
static te_errno
neigh_get(unsigned int gid, const char *oid, char *value,
          const char *ifname, const char *addr)
{
    UNUSED(gid);
    
    return neigh_find(oid, ifname, addr, value);
}


/**
 * Change already existing neighbour entry.
 *
 * @param gid            group identifier
 * @param oid            full object instence identifier (unused)
 * @param value          new value pointer ("XX:XX:XX:XX:XX:XX")
 * @param ifname         interface name
 * @param addr           IP address in human notation
 *
 * @return Status code
 */
static te_errno
neigh_set(unsigned int gid, const char *oid, const char *value,
          const char *ifname, const char *addr)
{
    UNUSED(gid);
    
    if (neigh_find(oid, ifname, addr, NULL) != 0)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    return neigh_add(gid, oid, value, ifname, addr);
}

/**
 * Add a new neighbour entry.
 *
 * @param gid            group identifier (unused)
 * @param oid            full object instence identifier (unused)
 * @param value          new entry value pointer ("XX:XX:XX:XX:XX:XX")
 * @param ifname         interface name
 * @param addr           IP address in human notation
 *
 * @return Status code
 */
static te_errno
neigh_add(unsigned int gid, const char *oid, const char *value,
          const char *ifname, const char *addr)
{
    MIB_IPNETROW   entry;
    int            int_mac[6];
    int            rc;
    int            i;

    UNUSED(gid);
    UNUSED(oid);
    
    if (neigh_find(oid, ifname, addr, NULL) == 0)
        return TE_RC(TE_TA_WIN32, TE_EEXIST);

    memset(&entry, 0, sizeof(entry));
    
    entry.dwType = strstr(oid, "dynamic") != NULL ? ARP_DYNAMIC 
                                                  : ARP_STATIC;
                                                  
    if ((entry.dwIndex =  ifname2ifindex(ifname)) == 0)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    if (sscanf(value, "%2x:%2x:%2x:%2x:%2x:%2x%s", int_mac, int_mac + 1,
            int_mac + 2, int_mac + 3, int_mac + 4, int_mac + 5, buf) != 6)
    {
        return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }

    for (i = 0; i < 6; i++)
        entry.bPhysAddr[i] = (unsigned char)int_mac[i];

    entry.dwAddr = inet_addr(addr);
    entry.dwPhysAddrLen = 6;
    if ((rc = CreateIpNetEntry(&entry)) != NO_ERROR)
    {
        ERROR("CreateIpNetEntry() failed, error %d", rc);
        return TE_RC(TE_TA_WIN32, TE_EWIN);
    }
    return 0;
}

/**
 * Delete neighbour entry.
 *
 * @param gid            group identifier (unused)
 * @param oid            full object instence identifier (unused)
 * @param ifname         interface name
 * @param addr           IP address in human notation
 *
 * @return Status code
 */
static te_errno
neigh_del(unsigned int gid, const char *oid, const char *ifname, 
          const char *addr)
{
    MIB_IPNETTABLE *table;
    int             i;
    DWORD           a;
    int             rc;
    DWORD           type = strstr(oid, "dynamic") == NULL ? ARP_STATIC
                                                          : ARP_DYNAMIC;
    te_bool         found = FALSE;
    DWORD           ifindex = ifname2ifindex(ifname);
    
    UNUSED(gid);
    UNUSED(oid);

    if (ifindex == 0)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    if ((a = inet_addr(addr)) == INADDR_NONE)
        return TE_RC(TE_TA_WIN32, TE_EINVAL);

    GET_TABLE(MIB_IPNETTABLE, GetIpNetTable);
    if (table == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        if (table->table[i].dwAddr == a && table->table[i].dwType == type &&
            table->table[i].dwIndex == ifindex)
        {
            if ((rc = DeleteIpNetEntry(table->table + i)) != 0)
            {
                if (type == ARP_STATIC)
                {
                    ERROR("DeleteIpNetEntry() failed, error %d", rc);
                    free(table);
                    return TE_RC(TE_TA_WIN32, TE_EWIN);
                }
            }
            found = TRUE;
            /* Continue to delete entries on other interfaces */
        }
    }
    free(table);
    return (!found && type == ARP_STATIC) ? 
           TE_RC(TE_TA_WIN32, TE_ENOENT) : 0;
}

/**
 * Get instance list for object "agent/arp" and "agent/volatile/arp".
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier
 * @param list          location for the list pointer
 * @param ifname        interface name
 *
 * @return Status code
 */
static te_errno
neigh_list(unsigned int gid, const char *oid, char **list, 
           const char *ifname)
{
    MIB_IPNETTABLE *table;
    int             i;
    char           *s = buf;
    DWORD           type = strstr(oid, "dynamic") == NULL ? ARP_STATIC
                                                          : ARP_DYNAMIC;
    DWORD           ifindex = ifname2ifindex(ifname);
    
    UNUSED(gid);
    UNUSED(oid);

    if (ifindex == 0)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);

    GET_TABLE(MIB_IPNETTABLE, GetIpNetTable);
    if (table == NULL)
    {
        if ((*list = strdup(" ")) == NULL)
            return TE_RC(TE_TA_WIN32, TE_ENOMEM);
        else
            return 0;
    }

    buf[0] = 0;

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        if (table->table[i].dwPhysAddrLen != 6 ||
            table->table[i].dwType != type || 
            table->table[i].dwIndex != ifindex ||
            table->table[i].dwAddr == 0xFFFFFFFF /* FIXME */)
        {
            continue;
        }
        s += snprintf(s, sizeof(buf) - (s - buf), "%s ",
                      inet_ntoa(*(struct in_addr *)
                                    &(table->table[i].dwAddr)));
    }

    free(table);

    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);

    return 0;
}

/******************************************************************/
/* Implementation of /agent/route subtree                         */
/******************************************************************/

/** 
 * Convert system-independent route info data structure to
 * Win32-specific MIB_IPFORWARDROW data structure.
 *
 * @param rt_info TA portable route info
 * @param rt      Win32-specific data structure (OUT)
 *
 * @return Status code.
 */
static te_errno
rt_info2ipforw(const ta_rt_info_t *rt_info, MIB_IPFORWARDROW *rt)
{
    int rc;

    if ((rt_info->flags & TA_RT_INFO_FLG_GW) == 0 &&
        (rt_info->flags & TA_RT_INFO_FLG_IF) == 0)
    {
        ERROR("Incorrect flags %x for rt_info %x", rt_info->flags);
        return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }

    rt->dwForwardDest = SIN(&(rt_info->dst))->sin_addr.s_addr;
    rt->dwForwardNextHop = SIN(&(rt_info->gw))->sin_addr.s_addr;
    rt->dwForwardMask = PREFIX2MASK(rt_info->prefix);
    rt->dwForwardType =
        ((rt_info->flags & TA_RT_INFO_FLG_GW) != 0) ?
        FORW_TYPE_REMOTE : FORW_TYPE_LOCAL;

    rt->dwForwardMetric1 =
        ((rt_info->flags & TA_RT_INFO_FLG_METRIC) != 0) ?
        rt_info->metric : METRIC_DEFAULT;

    if (rt->dwForwardNextHop == 0)
        rt->dwForwardNextHop = rt->dwForwardDest;

    rt->dwForwardProto = 3;
    
    if ((rt_info->flags & TA_RT_INFO_FLG_IF) != 0)
    {
        rc = name2index(rt_info->ifname, &(rt->dwForwardIfIndex));
        rt->dwForwardNextHop = rt->dwForwardDest;
    }
    else
    {
        /* Use Next Hop address to define outgoing interface */
        rc = find_ifindex(rt->dwForwardNextHop, &rt->dwForwardIfIndex);
    } 
        
    return rc;
}

/**
 * Get route attributes.
 *
 * @param route    route instance name: see doc/cm_cm_base.xml
 *                 for the format
 * @param rt_info  route related information (OUT)
 * @param rt       route related information in win32 format (OUT)
 *
 * @return status code
 */
static int
route_find(const char *route, ta_rt_info_t *rt_info, MIB_IPFORWARDROW *rt)
{
    MIB_IPFORWARDTABLE *table;
    DWORD               route_addr;
    int                 rc;
    int                 i;

    if ((rc = ta_rt_parse_inst_name(route, rt_info)) != 0)
        return TE_RC(TE_TA_WIN32, rc);

    GET_TABLE(MIB_IPFORWARDTABLE, GetIpForwardTable);
    if (table == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOENT);
    route_addr = SIN(&(rt_info->dst))->sin_addr.s_addr;
    
    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        uint32_t p;

        if (table->table[i].dwForwardType != FORW_TYPE_LOCAL &&
            table->table[i].dwForwardType != FORW_TYPE_REMOTE)
        {
            continue;
        }

        MASK2PREFIX(table->table[i].dwForwardMask, p);
        if (table->table[i].dwForwardDest != route_addr ||
            (p != rt_info->prefix) ||
            ((rt_info->flags & TA_RT_INFO_FLG_METRIC) != 0 &&
             (table->table[i].dwForwardMetric1 != rt_info->metric)))
        {
            continue;
        }        
        
        if (table->table[i].dwForwardIfIndex != 0)
        {
            rt_info->flags |= TA_RT_INFO_FLG_IF;
            sprintf(rt_info->ifname, 
                    ifindex2ifname(table->table[i].dwForwardIfIndex));
        }
        if (table->table[i].dwForwardNextHop != 0)
        {
            rt_info->flags |= TA_RT_INFO_FLG_GW;
            SIN(&rt_info->gw)->sin_family = AF_INET;
            SIN(&rt_info->gw)->sin_addr.s_addr = 
                table->table[i].dwForwardNextHop;
        }        

        if (rt != NULL)   
            *rt = table->table[i];
        
        /*
         * win32 agent supports only a limited set of route attributes.
         */
        free(table);
        return 0;
    }

    free(table);

    return TE_RC(TE_TA_WIN32, TE_ENOENT);
}

/**
 * Load all route-specific attributes into route object.
 *
 * @param obj  Object to be uploaded
 *
 * @return 0 in case of success, or error code on failure.
 */
static int
route_load_attrs(ta_cfg_obj_t *obj)
{
    ta_rt_info_t rt_info;
    int          rc;
    char         val[128];

    if ((rc = route_find(obj->name, &rt_info, NULL)) != 0)
        return rc;

    snprintf(val, sizeof(val), "%s", rt_info.ifname);
    if ((rt_info.flags & TA_RT_INFO_FLG_IF) != 0 &&
        (rc = ta_obj_attr_set(obj , "dev", val)) != 0)
    {
        return rc;
    }

    return 0;
}

static te_errno
route_dev_get(unsigned int gid, const char *oid,
              char *value, const char *route)
{
    int          rc;
    ta_rt_info_t rt_info;

    UNUSED(gid);
    UNUSED(oid);

    if ((rc = route_find(route, &rt_info, NULL)) != 0)
        return rc;

    sprintf(value, "%s", rt_info.ifname);
    return 0;
}

static te_errno
route_dev_set(unsigned int gid, const char *oid,
              const char *value, const char *route)
{
    UNUSED(oid);
    
    return ta_obj_set(TA_OBJ_TYPE_ROUTE, route, "dev",
                      value, gid, route_load_attrs);
}

/**
 * Add a new route.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value string (unused)
 * @param route         route instance name: see doc/cm_cm_base.xml
 *                      for the format
 *
 * @return status code
 */
static te_errno
route_add(unsigned int gid, const char *oid, const char *value,
          const char *route)
{
    UNUSED(oid);

    return ta_obj_add(TA_OBJ_TYPE_ROUTE, route, value, gid, NULL, NULL);
}

/**
 * Delete a route.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param route         route instance name: see doc/cm_cm_base.xml
 *                      for the format
 *
 * @return status code
 */
static te_errno
route_del(unsigned int gid, const char *oid, const char *route)
{
    UNUSED(oid);

    return ta_obj_del(TA_OBJ_TYPE_ROUTE, route, NULL, gid, NULL);
}

/** Get the value of the route.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value string (unused)
 * @param route         route instance name: see doc/cm_cm_base.xml
 *                      for the format
 *
 * @return status code
 */
static te_errno
route_get(unsigned int gid, const char *oid, char *value,
          const char *route_name)
{
    ta_rt_info_t  attr;
    int           rc;
    char         *addr; 

    UNUSED(gid);
    UNUSED(oid);

    if ((rc = route_find(route_name, &attr, NULL)) != 0)
    {
        ERROR("Route %s cannot be found", route_name);
        return rc;
    }
                        
    if (attr.dst.ss_family == AF_INET)
    {
        addr = inet_ntoa(SIN(&attr.gw)->sin_addr);
        memcpy(value, addr, strlen(addr));
    }
    else
    {
        ERROR("Unexpected destination address family: %d",
               attr.dst.ss_family);
        return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }
    
    return 0;
}            

/** Set new value for the route.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         value string (unused)
 * @param route         route instance name: see doc/cm_cm_base.xml
 *                      for the format
 *
 * @return status code
 */
static te_errno
route_set(unsigned int gid, const char *oid, const char *value,
          const char *route_name)
{
    UNUSED(oid);

    return ta_obj_value_set(TA_OBJ_TYPE_ROUTE, route_name, value, gid);
}            

/**
 * Get instance list for object "agent/route".
 *
 * @param gid           group identifier (unused)
 * @param id            full identifier of the father instance
 * @param list          location for the list pointer
 *
 * @return status code
 * @retval 0                    success
 * @retval TE_ENOENT            no such instance
 * @retval TE_ENOMEM               cannot allocate memory
 */
static te_errno
route_list(unsigned int gid, const char *oid, char **list)
{
    MIB_IPFORWARDTABLE *table;
    int                 i;
    char               *end_ptr = buf + sizeof(buf);
    char               *ptr = buf;

    UNUSED(gid);
    UNUSED(oid);

    GET_TABLE(MIB_IPFORWARDTABLE, GetIpForwardTable);
    if (table == NULL)
    {
        if ((*list = strdup(" ")) == NULL)
            return TE_RC(TE_TA_WIN32, TE_ENOMEM);
        else
            return 0;
    }

    buf[0] = '\0';

    for (i = 0; i < (int)table->dwNumEntries; i++)
    {
        int prefix;

        if ((table->table[i].dwForwardType != FORW_TYPE_LOCAL &&
             table->table[i].dwForwardType != FORW_TYPE_REMOTE) ||
             table->table[i].dwForwardDest == 0xFFFFFFFF ||
             (table->table[i].dwForwardMask == 0xFFFFFFFF &&
              table->table[i].dwForwardDest != htonl(INADDR_LOOPBACK) &&
              table->table[i].dwForwardNextHop == htonl(INADDR_LOOPBACK)))
        {
            continue;
        }

        MASK2PREFIX(table->table[i].dwForwardMask, prefix);
        snprintf(ptr, end_ptr - ptr, "%s|%d",
                 inet_ntoa(*(struct in_addr *)
                     &(table->table[i].dwForwardDest)), prefix);
        ptr += strlen(ptr);
        if (table->table[i].dwForwardMetric1 != METRIC_DEFAULT)
        {
            snprintf(ptr, end_ptr - ptr, ",metric=%ld",
                     table->table[i].dwForwardMetric1);
        }

        ptr += strlen(ptr);
        snprintf(ptr, end_ptr - ptr, " ");
        ptr += strlen(ptr);
    }

    free(table);

    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);

    return 0;
}

/**
 * Commit changes made for the route.
 *
 * @param gid           group identifier (unused)
 * @param p_oid         object instence data structure
 *
 * @return status code
 */
static te_errno
route_commit(unsigned int gid, const cfg_oid *p_oid)
{
    const char       *route;
    ta_cfg_obj_t     *obj;
    ta_rt_info_t      rt_info;
    int               rc;
    MIB_IPFORWARDROW  rt;
    
    ta_cfg_obj_action_e obj_action;

    UNUSED(gid);
    ENTRY("%s", route);

    memset(&rt, 0, sizeof(rt));

    route = ((cfg_inst_subid *)(p_oid->ids))[p_oid->len - 1].name;
    
    if ((obj = ta_obj_find(TA_OBJ_TYPE_ROUTE, route)) == NULL)
    {
        WARN("Commit for %s route which has not been updated", route);
        return 0;
    }

    if ((rc = ta_rt_parse_obj(obj, &rt_info)) != 0)
    {
        ta_obj_free(obj);
        return rc;
    }

    if ((obj_action = obj->action) == TA_CFG_OBJ_DELETE ||
        obj_action == TA_CFG_OBJ_SET)
    {
        rc = route_find(obj->name, &rt_info, &rt);
    }
    ta_obj_free(obj);
    if (rc != 0)
        return rc;

    switch (obj_action)
    {
        case TA_CFG_OBJ_DELETE:
        case TA_CFG_OBJ_SET:
        {
            if ((rc = DeleteIpForwardEntry(&rt)) != 0)
            {
                ERROR("DeleteIpForwardEntry() failed, error %d", rc);
                return TE_RC(TE_TA_WIN32, TE_EWIN);
            }
            if (obj_action == TA_CFG_OBJ_DELETE)
                break;
            /* FALLTHROUGH */
        }

        case TA_CFG_OBJ_CREATE:
        {
            if ((rc = rt_info2ipforw(&rt_info, &rt)) != 0)
            {
                ERROR("Failed to convert route to "
                      "MIB_IPFORWARDROW data structure");
                return TE_RC(TE_TA_WIN32, TE_EWIN);
            }
                      
            /* Add or set operation */
            if ((rc = CreateIpForwardEntry(&rt)) != NO_ERROR)
            {
                ERROR("CreateIpForwardEntry() failed, error %d", rc);
                return TE_RC(TE_TA_WIN32, TE_EWIN);
            }
            break;
        }

        default:
            ERROR("Unknown object action specified %d", obj_action);
            return TE_RC(TE_TA_WIN32, TE_EINVAL);
    }
    
    return 0;
}

/**
 * Is Environment variable with such name hidden?
 *
 * @param name      Variable name
 * @param name_len  -1, if @a name is a NUL-terminated string;
 *                  >= 0, if length of the @a name is @a name_len
 */
static te_bool
env_is_hidden(const char *name, int name_len)
{
    unsigned int    i;

    for (i = 0; i < sizeof(env_hidden) / sizeof(env_hidden[0]); ++i)
    {
        if (memcmp(env_hidden[i], name,
                   (name_len < 0) ? strlen(name) : (size_t)name_len) == 0)
            return TRUE;
    }
    return FALSE;
}

/**
 * Get Environment variable value.
 *
 * @param gid       Request's group identifier (unused)
 * @param oid       Full object instance identifier (unused)
 * @param value     Location for the value (OUT)
 * @param name      Variable name
 *
 * @return status code
 */
static te_errno
env_get(unsigned int gid, const char *oid, char *value,
        const char *name)
{
    const char *tmp = getenv(name);

    UNUSED(gid);
    UNUSED(oid);
    
    if (tmp == NULL)
    {
        static char buf[RCF_MAX_VAL];
        
        *buf = 0;
        GetEnvironmentVariable(name, buf, sizeof(buf));
        if (*buf != 0)
            tmp = buf;
    }

    if (!env_is_hidden(name, -1) && (tmp != NULL))
    {
        if (strlen(tmp) >= RCF_MAX_VAL)
            WARN("Environment variable '%s' value truncated", name);
        snprintf(value, RCF_MAX_VAL, "%s", tmp);
        return 0;
    }
    else
    {
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    }
}

/**
 * Change already existing Environment variable.
 *
 * @param gid       Request's group identifier (unused)
 * @param oid       Full object instance identifier (unused)
 * @param value     New value to set
 * @param name      Variable name
 *
 * @return status code
 */
static te_errno
env_set(unsigned int gid, const char *oid, const char *value,
        const char *name)
{
    UNUSED(gid);
    UNUSED(oid);

    if (env_is_hidden(name, -1))
        return TE_RC(TE_TA_UNIX, TE_EPERM);

    setenv(name, value, 1);
    SetEnvironmentVariable(name, value);
    
    return 0;
}

/**
 * Add a new Environment variable.
 *
 * @param gid       Request's group identifier (unused)
 * @param oid       Full object instance identifier (unused)
 * @param value     Value
 * @param name      Variable name
 *
 * @return status code
 */
static te_errno
env_add(unsigned int gid, const char *oid, const char *value,
        const char *name)
{
    UNUSED(gid);
    UNUSED(oid);

    if (env_is_hidden(name, -1))
        return TE_RC(TE_TA_WIN32, TE_EPERM);

    setenv(name, value, 1);
    SetEnvironmentVariable(name, value);

    return 0;
}

/**
 * Delete Environment variable.
 *
 * @param gid       Request's group identifier (unused)
 * @param oid       Full object instance identifier (unused)
 * @param name      Variable name
 *
 * @return status code
 */
static te_errno
env_del(unsigned int gid, const char *oid, const char *name)
{
    te_errno rc;
    
    UNUSED(gid);
    UNUSED(oid);

    if ((rc = env_get(0, NULL, buf, name)) != 0)
        return rc;
    
    SetEnvironmentVariable(name, NULL);
    unsetenv(name);

    return 0;
}

/**
 * Get instance list for object "/agent/env".
 *
 * @param gid       Request's group identifier (unused)
 * @param oid       Full object instance identifier (unused)
 * @param list      Location for the list pointer
 *
 * @return status code
 */
static te_errno
env_list(unsigned int gid, const char *oid, char **list)
{
    char **env;

    char   *ptr = buf;
    char   *buf_end = buf + sizeof(buf);

    UNUSED(gid);
    UNUSED(oid);

    if (environ == NULL)
        return 0;

    *ptr = '\0';
    for (env = environ; *env != NULL; ++env)
    {
        char    *s = strchr(*env, '=');
        ssize_t  name_len;

        if (s == NULL)
        {
            ERROR("Invalid Environment entry format: %s", *env);
            return TE_RC(TE_TA_UNIX, TE_EFMT);
        }
        name_len = s - *env;
        if (env_is_hidden(*env, name_len))
            continue;

        if (ptr != buf)
            *ptr++ = ' ';
        if ((buf_end - ptr) <= name_len)
        {
            ERROR("Too small buffer for the list of Environment "
                  "variables");
            return TE_RC(TE_TA_UNIX, TE_ESMALLBUF);
        }
        memcpy(ptr, *env, name_len);
        ptr += name_len;
        *ptr = '\0';
    }

    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);

    return 0;
}

/*
 * win32 Test Agent network statistics configuration tree.
 */
 
#define STATS_IFTABLE_COUNTER_GET(_counter_, _field_) \
static te_errno net_if_stats_##_counter_##_get(unsigned int gid_,       \
                                               const char  *oid_,       \
                                               char        *value_,     \
                                               const char  *ifname)     \
{                                                                       \
    int        rc = 0;                                                  \
                                                                        \
    UNUSED(gid_);                                                       \
    UNUSED(oid_);                                                       \
                                                                        \
    GET_IF_ENTRY;                                                       \
    snprintf((value_), RCF_MAX_VAL, "%lu", if_entry. _field_);          \
                                                                        \
                                                                        \
    VERB("dev_counter_get(dev_name=%s, counter=%s) returns %s",         \
         ifname, #_counter_, value_);                                   \
                                                                        \
    return rc;                                                          \
}


STATS_IFTABLE_COUNTER_GET(in_octets, dwInOctets);
STATS_IFTABLE_COUNTER_GET(in_ucast_pkts, dwInUcastPkts);
STATS_IFTABLE_COUNTER_GET(in_nucast_pkts, dwInNUcastPkts);
STATS_IFTABLE_COUNTER_GET(in_discards, dwInDiscards);
STATS_IFTABLE_COUNTER_GET(in_errors, dwInErrors);
STATS_IFTABLE_COUNTER_GET(in_unknown_protos, dwInUnknownProtos);
STATS_IFTABLE_COUNTER_GET(out_octets, dwOutOctets);
STATS_IFTABLE_COUNTER_GET(out_ucast_pkts, dwOutUcastPkts);
STATS_IFTABLE_COUNTER_GET(out_nucast_pkts, dwOutNUcastPkts);
STATS_IFTABLE_COUNTER_GET(out_discards, dwOutDiscards);
STATS_IFTABLE_COUNTER_GET(out_errors, dwOutErrors);

#undef STATS_IFTABLE_COUNTER_GET

#define STATS_NET_SNMP_IPV4_COUNTER_GET(_counter_, _field_)     \
static te_errno                                                 \
net_snmp_ipv4_stats_##_counter_##_get(unsigned int gid_,        \
                                      const char  *oid_,        \
                                      char        *value_)      \
{                                                               \
    int         rc = 0;                                         \
    MIB_IPSTATS *table = NULL;                                  \
                                                                \
    UNUSED(gid_);                                               \
    UNUSED(oid_);                                               \
    if ((table = malloc(sizeof(MIB_IPSTATS))) == NULL   )       \
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);                   \
                                                                \
    if ((rc = GetIpStatistics(table)) != NO_ERROR)              \
    {                                                           \
        ERROR("GetIpStatistics failed, error %d", rc);          \
        free(table);                                            \
        return TE_RC(TE_TA_WIN32, TE_EWIN);                     \
    }                                                           \
                                                                \
    snprintf((value_), RCF_MAX_VAL, "%lu",                      \
             table->_field_);                                   \
                                                                \
    VERB("net_snmp_ipv4_counter_get(counter=%s) returns %s",    \
         #_counter_, value_);                                   \
                                                                \
    free(table);                                                \
    return 0;                                                   \
}

STATS_NET_SNMP_IPV4_COUNTER_GET(in_recvs, dwInReceives);
STATS_NET_SNMP_IPV4_COUNTER_GET(in_hdr_errs, dwInHdrErrors);
STATS_NET_SNMP_IPV4_COUNTER_GET(in_addr_errs, dwInAddrErrors);
STATS_NET_SNMP_IPV4_COUNTER_GET(forw_dgrams, dwForwDatagrams);
STATS_NET_SNMP_IPV4_COUNTER_GET(in_unknown_protos, dwInUnknownProtos);
STATS_NET_SNMP_IPV4_COUNTER_GET(in_discards, dwInDiscards);
STATS_NET_SNMP_IPV4_COUNTER_GET(in_delivers, dwInDelivers);
STATS_NET_SNMP_IPV4_COUNTER_GET(out_requests, dwOutRequests);
STATS_NET_SNMP_IPV4_COUNTER_GET(out_discards, dwOutDiscards);
STATS_NET_SNMP_IPV4_COUNTER_GET(out_no_routes, dwOutNoRoutes);
STATS_NET_SNMP_IPV4_COUNTER_GET(reasm_timeout, dwReasmTimeout);
STATS_NET_SNMP_IPV4_COUNTER_GET(reasm_reqds, dwReasmReqds);
STATS_NET_SNMP_IPV4_COUNTER_GET(reasm_oks, dwReasmOks);
STATS_NET_SNMP_IPV4_COUNTER_GET(reasm_fails, dwReasmFails);
STATS_NET_SNMP_IPV4_COUNTER_GET(frag_oks, dwFragOks);
STATS_NET_SNMP_IPV4_COUNTER_GET(frag_fails, dwFragFails);
STATS_NET_SNMP_IPV4_COUNTER_GET(frag_creates, dwFragCreates);

#undef STATS_NET_SNMP_IPV4_COUNTER_GET

#define STATS_NET_SNMP_ICMP_COUNTER_GET(_counter_, _field_) \
static te_errno                                                 \
net_snmp_icmp_stats_ ## _counter_ ## _get(unsigned int gid_,    \
                                          const char  *oid_,    \
                                          char        *value_)  \
{                                                               \
    int         rc = 0;                                         \
    MIB_ICMP *table = NULL;                                     \
                                                                \
    UNUSED(gid_);                                               \
    UNUSED(oid_);                                               \
    if ((table = malloc(sizeof(MIB_ICMP))) == NULL   )          \
        return TE_RC(TE_TA_WIN32, TE_ENOMEM);                   \
                                                                \
    if ((rc = GetIcmpStatistics(table)) != NO_ERROR)            \
    {                                                           \
        ERROR("GetIcmpStatistics failed, error %d", rc);        \
        free(table);                                            \
        return TE_RC(TE_TA_WIN32, TE_EWIN);                     \
    }                                                           \
                                                                \
    snprintf((value_), RCF_MAX_VAL, "%lu",                      \
             table->stats._field_);                             \
                                                                \
    VERB("net_snmp_icmp_counter_get(counter=%s) returns %s",    \
         #_counter_, value_);                                   \
                                                                \
    free(table);                                                \
    return 0;                                                   \
}

STATS_NET_SNMP_ICMP_COUNTER_GET(in_msgs, icmpInStats.dwMsgs);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_errs, icmpInStats.dwErrors);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_dest_unreachs, 
                                icmpInStats.dwDestUnreachs);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_time_excds, icmpInStats.dwTimeExcds);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_parm_probs, icmpInStats.dwParmProbs);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_src_quenchs, icmpInStats.dwSrcQuenchs);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_redirects, icmpInStats.dwRedirects);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_echos, icmpInStats.dwEchos);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_echo_reps, icmpInStats.dwEchoReps);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_timestamps, icmpInStats.dwTimestamps);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_timestamp_reps, 
                                icmpInStats.dwTimestampReps);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_addr_masks, icmpInStats.dwAddrMasks);
STATS_NET_SNMP_ICMP_COUNTER_GET(in_addr_mask_reps, 
                                icmpInStats.dwAddrMaskReps);

STATS_NET_SNMP_ICMP_COUNTER_GET(out_msgs, icmpOutStats.dwMsgs);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_errs, icmpOutStats.dwErrors);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_dest_unreachs, 
                                icmpOutStats.dwDestUnreachs);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_time_excds, icmpOutStats.dwTimeExcds);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_parm_probs, icmpOutStats.dwParmProbs);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_src_quenchs, icmpOutStats.dwSrcQuenchs);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_redirects, icmpOutStats.dwRedirects);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_echos, icmpOutStats.dwEchos);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_echo_reps, icmpOutStats.dwEchoReps);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_timestamps, icmpOutStats.dwTimestamps);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_timestamp_reps, 
                                icmpOutStats.dwTimestampReps);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_addr_masks, icmpOutStats.dwAddrMasks);
STATS_NET_SNMP_ICMP_COUNTER_GET(out_addr_mask_reps,
                                icmpOutStats.dwAddrMaskReps);

#undef STATS_NET_SNMP_ICMP_COUNTER_GET

/* counters from MIB_IFROW structure */

RCF_PCH_CFG_NODE_RO(node_stats_net_if_in_octets, "in_octets",
                    NULL, NULL, net_if_stats_in_octets_get);

#define STATS_NET_IF_ATTR(_name_, _next) \
    RCF_PCH_CFG_NODE_RO(node_stats_net_if_##_name_, #_name_, \
                        NULL, &node_stats_net_if_##_next,    \
                        net_if_stats_##_name_##_get);

STATS_NET_IF_ATTR(in_ucast_pkts, in_octets);
STATS_NET_IF_ATTR(in_nucast_pkts, in_ucast_pkts);
STATS_NET_IF_ATTR(in_discards, in_nucast_pkts);
STATS_NET_IF_ATTR(in_errors, in_discards);
STATS_NET_IF_ATTR(in_unknown_protos, in_errors);
STATS_NET_IF_ATTR(out_octets, in_unknown_protos);
STATS_NET_IF_ATTR(out_ucast_pkts, out_octets);
STATS_NET_IF_ATTR(out_nucast_pkts, out_ucast_pkts);
STATS_NET_IF_ATTR(out_discards, out_nucast_pkts);
STATS_NET_IF_ATTR(out_errors, out_discards);

#undef STATS_NET_IF_ATTR 
/* counters from MIB_IPSTATS structure */

RCF_PCH_CFG_NODE_RO(node_stats_net_snmp_ipv4_in_recvs, "ipv4_in_recvs",
                    NULL, NULL, net_snmp_ipv4_stats_in_recvs_get);

#define STATS_NET_SNMP_IPV4_ATTR(_name_, _next) \
    RCF_PCH_CFG_NODE_RO(node_stats_net_snmp_ipv4_##_name_,          \
                        "ipv4_" #_name_,                            \
                        NULL, &node_stats_net_snmp_ipv4_##_next,    \
                        net_snmp_ipv4_stats_##_name_##_get);

STATS_NET_SNMP_IPV4_ATTR(in_hdr_errs, in_recvs);
STATS_NET_SNMP_IPV4_ATTR(in_addr_errs, in_hdr_errs);
STATS_NET_SNMP_IPV4_ATTR(forw_dgrams, in_addr_errs);
STATS_NET_SNMP_IPV4_ATTR(in_unknown_protos, forw_dgrams);
STATS_NET_SNMP_IPV4_ATTR(in_discards, in_unknown_protos);
STATS_NET_SNMP_IPV4_ATTR(in_delivers, in_discards);
STATS_NET_SNMP_IPV4_ATTR(out_requests, in_delivers);
STATS_NET_SNMP_IPV4_ATTR(out_discards, out_requests);
STATS_NET_SNMP_IPV4_ATTR(out_no_routes, out_discards);
STATS_NET_SNMP_IPV4_ATTR(reasm_timeout, out_no_routes);
STATS_NET_SNMP_IPV4_ATTR(reasm_reqds, reasm_timeout);
STATS_NET_SNMP_IPV4_ATTR(reasm_oks, reasm_reqds);
STATS_NET_SNMP_IPV4_ATTR(reasm_fails, reasm_oks);
STATS_NET_SNMP_IPV4_ATTR(frag_oks, reasm_fails);
STATS_NET_SNMP_IPV4_ATTR(frag_fails, frag_oks);
STATS_NET_SNMP_IPV4_ATTR(frag_creates, frag_fails);


/* counters from MIB_ICMP structure */

RCF_PCH_CFG_NODE_RO(node_stats_net_snmp_icmp_in_msgs, "icmp_in_msgs",
                    NULL, &node_stats_net_snmp_ipv4_frag_creates,
                    net_snmp_icmp_stats_in_msgs_get);

#define STATS_NET_SNMP_ICMP_ATTR(_name_, _next) \
    RCF_PCH_CFG_NODE_RO(node_stats_net_snmp_icmp_##_name_,          \
                        "icmp_" #_name_,                            \
                        NULL, &node_stats_net_snmp_icmp_##_next,    \
                        net_snmp_icmp_stats_##_name_##_get);

STATS_NET_SNMP_ICMP_ATTR(in_errs, in_msgs);
STATS_NET_SNMP_ICMP_ATTR(in_dest_unreachs, in_errs);
STATS_NET_SNMP_ICMP_ATTR(in_time_excds, in_dest_unreachs);
STATS_NET_SNMP_ICMP_ATTR(in_parm_probs, in_time_excds);
STATS_NET_SNMP_ICMP_ATTR(in_src_quenchs, in_parm_probs);
STATS_NET_SNMP_ICMP_ATTR(in_redirects, in_src_quenchs);
STATS_NET_SNMP_ICMP_ATTR(in_echos, in_redirects);
STATS_NET_SNMP_ICMP_ATTR(in_echo_reps, in_echos);
STATS_NET_SNMP_ICMP_ATTR(in_timestamps, in_echo_reps);
STATS_NET_SNMP_ICMP_ATTR(in_timestamp_reps, in_timestamps);
STATS_NET_SNMP_ICMP_ATTR(in_addr_masks, in_timestamp_reps);
STATS_NET_SNMP_ICMP_ATTR(in_addr_mask_reps, in_addr_masks);

STATS_NET_SNMP_ICMP_ATTR(out_msgs, in_addr_mask_reps);
STATS_NET_SNMP_ICMP_ATTR(out_errs, out_msgs);
STATS_NET_SNMP_ICMP_ATTR(out_dest_unreachs, out_errs);
STATS_NET_SNMP_ICMP_ATTR(out_time_excds, out_dest_unreachs);
STATS_NET_SNMP_ICMP_ATTR(out_parm_probs, out_time_excds);
STATS_NET_SNMP_ICMP_ATTR(out_src_quenchs, out_parm_probs);
STATS_NET_SNMP_ICMP_ATTR(out_redirects, out_src_quenchs);
STATS_NET_SNMP_ICMP_ATTR(out_echos, out_redirects);
STATS_NET_SNMP_ICMP_ATTR(out_echo_reps, out_echos);
STATS_NET_SNMP_ICMP_ATTR(out_timestamps, out_echo_reps);
STATS_NET_SNMP_ICMP_ATTR(out_timestamp_reps, out_timestamps);
STATS_NET_SNMP_ICMP_ATTR(out_addr_masks, out_timestamp_reps);
STATS_NET_SNMP_ICMP_ATTR(out_addr_mask_reps, out_addr_masks);

RCF_PCH_CFG_NODE_NA(node_net_if_stats, "stats",
                    &node_stats_net_if_out_errors,
                    NULL);

RCF_PCH_CFG_NODE_NA(node_net_snmp_stats, "stats",
                    &node_stats_net_snmp_icmp_out_addr_mask_reps,
                    NULL);


te_errno
ta_win32_conf_net_snmp_stats_init(void)
{

    return rcf_pch_add_node("/agent", &node_net_snmp_stats);
}

te_errno
ta_win32_conf_net_if_stats_init(void)
{
    
    return rcf_pch_add_node("/agent/interface", &node_net_if_stats);
}


// Multicast

#define DRV_TYPE 40000
#define METHOD_BUFFERED                 0
#define FILE_ANY_ACCESS                 0

#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)


#define KRX_ADD_MULTICAST_ADDR \
    CTL_CODE( DRV_TYPE, 0x907, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define KRX_DEL_MULTICAST_ADDR \
    CTL_CODE( DRV_TYPE, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define KRX_GET_MULTICAST_LIST \
    CTL_CODE( DRV_TYPE, 0x909, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define WRAPPER_DEVICE_NAME  "\\\\.\\olwrapper"
#define WRAPPER_DEVFILE_NAME "\\\\.\\olwrapper"

static te_errno
mcast_link_addr_add(unsigned int gid, const char *oid,
                    const char *value, const char *ifname, const char *addr)
{

    HANDLE dev = CreateFile(WRAPPER_DEVFILE_NAME, GENERIC_READ | 
                            GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    int rc;
    DWORD bytes_returned = 0;
    unsigned char addr6[6];
    unsigned int itemp;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(ifname);
//    UNUSED(addr);

    if (INVALID_HANDLE_VALUE == dev)
    {
      return TE_RC(TE_TA_WIN32, TE_EOPNOTSUPP);
    }
    RING("add addr %s", addr);
    printf("IGORL: add addr %s\n", addr);
    sscanf(addr+0,"%x", &itemp);
    RING("itemp 0 = 0x%x", itemp);
    addr6[0] = (unsigned char)itemp;
    sscanf(addr+3,"%x", &itemp);
    RING("itemp 1 = 0x%x", itemp);
    addr6[1] = (unsigned char)itemp;
    sscanf(addr+6,"%x", &itemp);
    RING("itemp 2 = 0x%x", itemp);
    addr6[2] = (unsigned char)itemp;
    sscanf(addr+9,"%x", &itemp);
    addr6[3] = (unsigned char)itemp;
    sscanf(addr+12,"%x", &itemp);
    addr6[4] = (unsigned char)itemp;
    sscanf(addr+15,"%x", &itemp);
    addr6[5] = (unsigned char)itemp;

    RING("hex addr = 0x%x", *(unsigned int *)addr6);
    printf("IGORL: read addr  = %02x:%02x:%02x:%02x:%02x:%02x\n",
           addr6[0], addr6[1], addr6[2], addr6[3], addr6[4], addr6[5]);
    if (!DeviceIoControl(dev, KRX_ADD_MULTICAST_ADDR, addr6, 6, NULL, 0,
                         &bytes_returned, NULL))
    {
      rc = GetLastError();
      WARN("DeviceIoControl failed with errno=%d", GetLastError());
      return -2;
    }
    printf("IGORL: before closehandle\n");
    CloseHandle(dev);
    return 0;

/*
    te_errno rc;
    
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    rc = mcast_link_addr_change(ifname, addr, SIOCADDMULTI);
#ifndef __linux__
    if (rc == 0)
    {
        mma_list_el *p = (mma_list_el *)malloc(sizeof(mma_list_el));
        strncpy(p->ifname, ifname, IFNAMSIZ);
        strncpy(p->value, addr, sizeof(p->value));
        p->next = mcast_mac_addr_list;
        mcast_mac_addr_list = p->next;
    }
#endif
    return rc;
    */
}
    
static te_errno
mcast_link_addr_del(unsigned int gid, const char *oid, const char *ifname,
                    const char *addr)
{

    HANDLE dev = CreateFile(WRAPPER_DEVFILE_NAME, GENERIC_READ | 
                            GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    int rc;
    DWORD bytes_returned = 0;
    unsigned char addr6[6];
    unsigned int itemp;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(ifname);
//    UNUSED(addr);

    if (INVALID_HANDLE_VALUE == dev)
    {
      return TE_RC(TE_TA_WIN32, TE_EOPNOTSUPP);
    }
    RING("del addr %s", addr);
    
    sscanf(addr+0,"%x", &itemp);
    RING("itemp 0 = 0x%x", itemp);
    addr6[0] = (unsigned char)itemp;
    sscanf(addr+3,"%x", &itemp);
    RING("itemp 1 = 0x%x", itemp);
    addr6[1] = (unsigned char)itemp;
    sscanf(addr+6,"%x", &itemp);
    RING("itemp 2 = 0x%x", itemp);
    addr6[2] = (unsigned char)itemp;
    sscanf(addr+9,"%x", &itemp);
    addr6[3] = (unsigned char)itemp;
    sscanf(addr+12,"%x", &itemp);
    addr6[4] = (unsigned char)itemp;
    sscanf(addr+15,"%x", &itemp);
    addr6[5] = (unsigned char)itemp;

    RING("hex addr = 0x%x", *(unsigned int *)addr6);

    if (!DeviceIoControl(dev, KRX_DEL_MULTICAST_ADDR, addr6, 6, NULL, 0,
                         &bytes_returned, NULL))
    {
      rc = GetLastError();
      WARN("DeviceIoControl failed with errno=%d", GetLastError());
      return -2;
    }
    CloseHandle(dev);
    return 0;

/*    te_errno rc;
    
    UNUSED(gid);
    UNUSED(oid);
    rc = mcast_link_addr_change(ifname, addr, SIOCDELMULTI);
#ifndef __linux__ 
    if (rc == 0)
    {
        if (strcmp(mcast_mac_addr_list->value, addr) == 0 &&
            strcmp(mcast_mac_addr_list->ifname, ifname) == 0)
        {
            mma_list_el *p = mcast_mac_addr_list->next;
            free(mcast_mac_addr_list);
            mcast_mac_addr_list = p;
        }
        else
        {
            mma_list_el *p,
                        *pp;
            for (p = mcast_mac_addr_list;
                 p->next != NULL && 
                 (strcmp(p->next->value, addr) != 0 ||
                  strcmp(p->next->ifname, ifname) != 0);
                 p = p->next);
            pp = p->next->next;
            free(p->next);
            p->next = pp;
        }
    }
#endif
    return rc;*/

}

static te_errno
mcast_link_addr_list(unsigned int gid, const char *oid, char **list,
                     const char *ifname)
{

    HANDLE dev = CreateFile(WRAPPER_DEVFILE_NAME, GENERIC_READ | 
                            GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    int rc;
    DWORD bytes_returned = 0;
    unsigned int i;
    unsigned char *buf = malloc(1024 * sizeof(unsigned char));
    unsigned char *ret = malloc(1024 * sizeof(unsigned char));

    UNUSED(gid);
    UNUSED(oid);
//    UNUSED(list);
    UNUSED(ifname);


    if (INVALID_HANDLE_VALUE == dev)
    {
      return TE_RC(TE_TA_WIN32, TE_ENOENT);
    }
    if (!DeviceIoControl(dev, KRX_GET_MULTICAST_LIST, 
                        (void *)buf, 1024, (void *)buf, 1024,
                         &bytes_returned, NULL))
    {
      rc = GetLastError();
      WARN("DeviceIoControl failed with errno=%d", GetLastError());
      return -2;
    }
    else
   { 
      RING("bytes_returned=%d", bytes_returned);
   }
    CloseHandle(dev);
    for (i = 0; i < bytes_returned / 6; i++)
    {
      sprintf(&ret[i*18],"%02x:%02x:%02x:%02x:%02x:%02x ", buf[i*6], 
              buf[i*6+1], buf[i*6+2], buf[i*6+3], buf[i*6+4], buf[i*6+5]);
      RING("%d element of list: %s", i, &ret[i*18]);
      RING("begin og the buffer 0x%x", *(unsigned int*)buf);
    }
//    list[i][0] = 0;

    *list = ret;
    return 0;

#if 0
/*    char       *s = NULL;
    int         p = 0;
    int         buf_segs = 1;

#define MMAC_ADDR_BUF_SIZE 16384 
#ifndef __linux__
    mma_list_el *tmp;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(ifname);

    s = (char *)malloc(MMAC_ADDR_BUF_SIZE);
    *s = '\0';

    for (tmp = mcast_mac_addr_list; tmp != NULL; tmp = tmp->next)
    {
        if (strcmp(tmp->ifname, ifname) == 0)
        {
            if (p >= MMAC_ADDR_BUF_SIZE - ETHER_ADDR_LEN * 3)
            {
                s = realloc(s, (++buf_segs) * MMAC_ADDR_BUF_SIZE);
            }
            p += sprintf(&s[p], "%s ", tmp->value);
        }
    }
#else
    FILE       *fd;
    char        ifn[IFNAMSIZ];
    char        addrstr[ETHER_ADDR_LEN * 3];

    UNUSED(gid);
    UNUSED(oid);
    if ((fd = fopen("/proc/net/dev_mcast", "r")) == NULL)
        return TE_OS_RC(TE_TA_UNIX, errno);
    
    s = (char *)malloc(MMAC_ADDR_BUF_SIZE);
    *s = '\0';

    while (fscanf(fd, "%*d %s %*d %*d %s\n", ifn, addrstr) > 0)
    {
        /*
         * Read file and copy items with appropriate interface name
         * to the buffer, adding colons to MAC addresses.
         */
            
/*        if (strcmp(ifn, ifname) == 0)
        {
            int i;

            for (i = 0; i < 6; i++)
            {
                if (p >= MMAC_ADDR_BUF_SIZE - ETHER_ADDR_LEN * 3)
                {
                    s = realloc(s, (++buf_segs) * MMAC_ADDR_BUF_SIZE);
                }
                strncpy(&s[p], &addrstr[i * 2], 2);
                p += 2;
                s[p++] = (i < 5) ? ':' : ' ';
                s[p] = '\0';
            }
        }
    }
    fclose(fd);
#endif
    *list = s;
    return 0;
    */
#endif
}

// Multicast
