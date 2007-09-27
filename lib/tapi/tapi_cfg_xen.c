/** @file
 * @brief Test API to configure XEN.
 *
 * Implementation of API to configure XEN.
 *
 *
 * Copyright (C) 2005 Test Environment authors (see file AUTHORS
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
 * @author Edward Makarov <Edward.Makarov@oktetlabs.ru>
 *
 * $Id:
 */

#define TE_LGR_USER     "TAPI CFG XEN"

#include "te_config.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>

/* Ensure PATH_MAX is defined */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <limits.h>

/* Nevertheless PATH_MAX can still be undefined here yet */
#ifndef PATH_MAX
#define PATH_MAX 108
#endif

#endif /* STDC_HEADERS */

#include "te_defs.h"
#include "logger_api.h"
#include "tapi_sockaddr.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_xen.h"

#define DHCP_MAGIC_SIZE        4

#define SERVER_ID_OPTION       54
#define REQUEST_IP_ADDR_OPTION 50

/* Ethernet address length */
#define ETHER_ADDR_LEN 6

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_get_path(char const *ta, char *path)
{
    cfg_val_type type  = CVT_STRING;
    char const  *value;
    te_errno     rc    = cfg_get_instance_fmt(&type, &value,
                                              "/agent:%s/xen:", ta);

    if (rc == 0)
    {
        strcpy(path, value);
        free((void *)value);
    }
    else
        ERROR("Failed to get XEN path on %s", ta);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_set_path(char const *ta, char const *path)
{
    te_errno rc = cfg_set_instance_fmt(CFG_VAL(STRING, path),
                                       "/agent:%s/xen:",
                                       ta);

    if (rc != 0)
        ERROR("Failed to set XEN path to '%s' on %s", path, ta);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_create_dom_u(char const *ta, char const *dom_u)
{
    /* Create domU destroying old  directory/disk images in XEN storage */
    te_errno rc = cfg_add_instance_fmt(NULL, CFG_VAL(INTEGER, 0),
                                       "/agent:%s/xen:/dom_u:%s",
                                       ta, dom_u);

    if (rc != 0)
    {
        ERROR("Failed to create '%s' domU on %s destroying old "
              "directory and images in XEN storage", dom_u, ta);
    }

    if ((rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, 1),
                                   "/agent:%s/xen:/dom_u:%s",
                                   ta, dom_u)) != 0)
    {
        ERROR("Failed to create '%s' domU on %s creating new "
              "directory and images in XEN storage", dom_u, ta);
    }

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_destroy_dom_u(char const *ta, char const *dom_u)
{
    /* Ensure that domU is in "non-running" state */
    te_errno rc = cfg_set_instance_fmt(CFG_VAL(STRING, "non-running"),
                                       "/agent:%s/xen:/dom_u:%s/status:",
                                       ta, dom_u);

    if (rc != 0)
    {
        ERROR("Failed to shutdown '%s' domU on %s: %r", dom_u, ta, rc);
        goto cleanup0;
    }

    /* Remove directory/disk images of domU from XEN storage */
    if ((rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, 0),
                                   "/agent:%s/xen:/dom_u:%s",
                                   ta, dom_u)) != 0)
    {
        ERROR("Failed to remove directory/images of '%s' domU on %s",
              dom_u, ta);
        goto cleanup0;
    }

    /* Destroy domU */
    if ((rc = cfg_del_instance_fmt(FALSE,
                                   "/agent:%s/xen:/dom_u:%s",
                                   ta, dom_u)) != 0)
    {
        ERROR("Failed to destroy '%s' domU on %", dom_u, ta);
    }

cleanup0:
    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_get_status(char const *ta, char const *dom_u,
                              char *status)
{
    cfg_val_type type  = CVT_STRING;
    char const  *value;

    te_errno rc = cfg_get_instance_fmt(&type, &value,
                                       "/agent:%s/xen:/dom_u:%s/status:",
                                       ta, dom_u);

    if (rc == 0)
    {
        strcpy(status, value);
        free((void *)value);
    }
    else
        ERROR("Failed to get status for '%s' domU on %s", dom_u, ta);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_set_status(char const *ta, char const *dom_u,
                              char const *status)
{
    te_errno rc = cfg_set_instance_fmt(CFG_VAL(STRING, status),
                                       "/agent:%s/xen:/dom_u:%s/status:",
                                       ta, dom_u);

    if (rc != 0)
        ERROR("Failed to set \"%s\" status for '%s' domU on %s: %r",
              status, dom_u, ta, rc);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_get_ip_addr(char const *ta, char const *dom_u,
                               struct sockaddr *ip_addr)
{
    cfg_val_type           type  = CVT_ADDRESS;
    struct sockaddr const *value;

    te_errno rc = cfg_get_instance_fmt(&type, &value,
                                       "/agent:%s/xen:/dom_u:%s/ip_addr:",
                                       ta, dom_u);

    if (rc == 0)
    {
        memcpy(ip_addr, value, sizeof(*value));
        free((void *)value);
    }
    else
        ERROR("Failed to get IP address for '%s' domU on %s", dom_u, ta);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_set_ip_addr(char const *ta, char const *dom_u,
                               struct sockaddr const *ip_addr)
{
    te_errno rc = cfg_set_instance_fmt(CFG_VAL(ADDRESS, ip_addr),
                                       "/agent:%s/xen:/dom_u:%s/ip_addr:",
                                       ta, dom_u);

    if (rc != 0)
        ERROR("Failed to set IP address for '%s' domU on %s", dom_u, ta);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_get_mac_addr(char const *ta, char const *dom_u, uint8_t *mac)
{
    cfg_val_type     type = CVT_ADDRESS;
    struct sockaddr *addr;

    te_errno rc = cfg_get_instance_fmt(&type, &addr,
                                       "/agent:%s/xen:/dom_u:%s/mac_addr:",
                                       ta, dom_u);

    if (rc == 0)
    {
        memcpy(mac, addr->sa_data, ETHER_ADDR_LEN);
        free(addr);
    }
    else
        ERROR("Failed to get MAC address of '%s' domU on %s", dom_u, ta);

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_set_mac_addr(char const *ta, char const *dom_u, uint8_t const *mac)
{
    struct sockaddr addr = { .sa_family = AF_LOCAL };
    te_errno        rc;

    memcpy(addr.sa_data, mac, ETHER_ADDR_LEN);

    if ((rc = cfg_set_instance_fmt(CFG_VAL(ADDRESS, &addr),
                                   "/agent:%s/xen:/dom_u:%s/mac_addr:",
                                   ta, dom_u)) != 0)
    {
        ERROR("Failed to set MAC address of '%s' domU on %s", dom_u, ta);
    }

    return rc;
}

/* See description in tapi_cfg_xen.h */
te_errno
tapi_cfg_xen_dom_u_migrate(char const *from_ta, char const *to_ta,
                           char const *dom_u, char const *host,
                           te_bool live)
{
    te_bool running;
    te_bool saved;

    char xen_path1[PATH_MAX];
    char xen_path2[PATH_MAX];
    char status[PATH_MAX];

    uint8_t         mac[ETHER_ADDR_LEN];
    struct sockaddr ip;

    te_errno rc = tapi_cfg_xen_dom_u_get_status(from_ta, dom_u, status);

    if (rc != 0)
        goto cleanup0;

    running = strcmp(status, "running") == 0 ||
              strcmp(status, "migrated-running") == 0;
    saved   = strcmp(status, "saved") == 0 ||
              strcmp(status, "migrated-saved") == 0;

    /* Check the status of domU on 'from_ta' agent */
    if (!running && !saved)
    {
        ERROR("Failed to migrate since '%s' domU is in \"%s\" status "
              "(neither in \"running\" nor in \"saved\" one)",
              dom_u, status);
        rc = TE_RC(TE_TA_UNIX, TE_EINVAL);
        goto cleanup0;
    }

    /* Cannot migrate to itself */
    if (strcmp(from_ta, to_ta) == 0)
    {
        ERROR("Failed to migrate from %s to itself", from_ta);
        rc = TE_RC(TE_TA_UNIX, TE_EINVAL);
        goto cleanup0;
    }

    /* Check that XEN paths are identical for both dom0 agents */
    if ((rc = tapi_cfg_xen_get_path(from_ta, xen_path1)) != 0 ||
        (rc = tapi_cfg_xen_get_path(  to_ta, xen_path2)) != 0)
    {
        goto cleanup0;
    }

    if (strcmp(xen_path1, xen_path2) != 0)
    {
        ERROR("XEN path differs between %s and %s", from_ta, to_ta);
        rc = TE_RC(TE_TA_UNIX, TE_EINVAL);
        goto cleanup0;
    }

    /* Save MAC and IP addresses */
    if ((rc = tapi_cfg_xen_dom_u_get_mac_addr(from_ta, dom_u, mac)) != 0 ||
        (rc = tapi_cfg_xen_dom_u_get_ip_addr( from_ta, dom_u, &ip)) != 0)
    {
        goto cleanup0;
    }

    if (running)
    {
        /* Set kind of migration (live/non-live) */
        if ((rc = cfg_set_instance_fmt(CFG_VAL(INTEGER, live),
                                       "/agent:%s/xen:/dom_u:%s/"
                                       "migrate:/kind:",
                                       from_ta, dom_u)) != 0)
        {
            ERROR("Failed to set migration kind for '%s' domU on %s",
                  dom_u, from_ta);
            goto cleanup0;
        }

        /* Perform migration */
        if ((rc = cfg_set_instance_fmt(CFG_VAL(STRING, host),
                                       "/agent:%s/xen:/dom_u:%s/migrate:",
                                       from_ta, dom_u)) != 0)
        {
            ERROR("Failed to perform migration itself");
            goto cleanup0;
        }
    }

    /* Delete domU item from the source agent configurator tree */
    if ((rc = cfg_del_instance_fmt(FALSE, "/agent:%s/xen:/dom_u:%s",
                                   from_ta, dom_u)) != 0)
    {
        ERROR("Failed to destroy '%s' domU on %s", dom_u, from_ta);
        goto cleanup0;
    }

    /* Create domU on target agent (domU will have "non-running" state) */
    if ((rc = cfg_add_instance_fmt(NULL, CFG_VAL(INTEGER, 1),
                                   "/agent:%s/xen:/dom_u:%s",
                                   to_ta, dom_u)) != 0)
    {
        ERROR("Failed to accept '%s' domU just migrated to %s",
              dom_u, to_ta);
        goto cleanup0;
    }

    /* Set MAC and IP addresses saved previously */
    if ((rc = tapi_cfg_xen_dom_u_set_mac_addr(to_ta, dom_u, mac)) != 0 ||
        (rc = tapi_cfg_xen_dom_u_set_ip_addr( to_ta, dom_u, &ip)) != 0)
    {
        goto cleanup0;
    }

    /* Set "migrated-running" or "migrated-saved" status */
    if ((rc = cfg_set_instance_fmt(CFG_VAL(STRING,
                                           running ? "migrated-running" :
                                                      "migrated-saved"),
                                   "/agent:%s/xen:/dom_u:%s/status:",
                                   to_ta, dom_u)) != 0)
    {
        ERROR("Failed to set migrated %s status for '%s' domU on %s",
              running ? "running" : "saved", dom_u, to_ta);
    }

cleanup0:
    if (rc != 0)
    {
        ERROR("Failed to migrate '%s' domU from %s to %s (to host '%s')",
              dom_u, from_ta, to_ta, host);
    }

    return rc;
}

