/** @file
 * @brief Test GateWay network configuring API
 *
 * Implementation of functions for gateway configuration to be
 * used in tests. "Gateway" here is the third host which forwards
 * packets between two testing hosts not connected directly.
 *
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS
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
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

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

#include "tapi_sockaddr.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg.h"
#include "tapi_test.h"

#include "tapi_route_gw.h"

/* See description in tapi_route_gw.h */
void
tapi_update_arp(const char *ta_src,
                const struct if_nameindex *iface_src,
                const char *ta_dest,
                const struct if_nameindex *iface_dest,
                const struct sockaddr *addr_dest,
                const struct sockaddr *link_addr_dest,
                te_bool is_static)
{
    static struct sockaddr link_addr;

    CHECK_RC(tapi_cfg_del_neigh_entry(ta_src, iface_src->if_name,
                                      addr_dest));

    if (link_addr_dest != NULL)
        memcpy(&link_addr, link_addr_dest, sizeof(link_addr));
    else if (ta_dest != NULL && iface_dest != NULL)
        CHECK_RC(tapi_cfg_base_if_get_link_addr(ta_dest,
                                                iface_dest->if_name,
                                                &link_addr));
    else
        TEST_FAIL("Wrong options combination to change arp table");

    CHECK_RC(tapi_cfg_add_neigh_entry(ta_src, iface_src->if_name,
                                      addr_dest, link_addr.sa_data,
                                      is_static));
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_init(
                  tapi_route_gateway *gw,
                  const char *iut_ta,
                  const char *tst_ta,
                  const char *gw_ta,
                  const struct if_nameindex *iut_if,
                  const struct if_nameindex *tst_if,
                  const struct if_nameindex *gw_iut_if,
                  const struct if_nameindex *gw_tst_if,
                  const struct sockaddr *iut_addr,
                  const struct sockaddr *tst_addr,
                  const struct sockaddr *gw_iut_addr,
                  const struct sockaddr *gw_tst_addr,
                  const struct sockaddr *alien_link_addr)
{
    if (snprintf(gw->iut_ta, RCF_MAX_NAME, "%s", iut_ta) >= RCF_MAX_NAME ||
        snprintf(gw->tst_ta, RCF_MAX_NAME, "%s", tst_ta) >= RCF_MAX_NAME ||
        snprintf(gw->gw_ta, RCF_MAX_NAME, "%s", gw_ta) >= RCF_MAX_NAME)
        TEST_FAIL("%s(): TA name is too long", __FUNCTION__);

    gw->iut_if = iut_if;
    gw->tst_if = tst_if;
    gw->gw_iut_if = gw_iut_if;
    gw->gw_tst_if = gw_tst_if;
    gw->iut_addr = iut_addr;
    gw->tst_addr = tst_addr;
    gw->gw_iut_addr = gw_iut_addr;
    gw->gw_tst_addr = gw_tst_addr;
    gw->alien_link_addr = alien_link_addr;
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_configure(tapi_route_gateway *gw)
{
    CHECK_RC(tapi_cfg_add_route_via_gw(
               gw->iut_ta,
               gw->tst_addr->sa_family,
               te_sockaddr_get_netaddr(gw->tst_addr),
               te_netaddr_get_size(gw->tst_addr->sa_family) * 8,
               te_sockaddr_get_netaddr(gw->gw_iut_addr)));

    CHECK_RC(tapi_cfg_add_route_via_gw(
               gw->tst_ta,
               gw->iut_addr->sa_family,
               te_sockaddr_get_netaddr(gw->iut_addr),
               te_netaddr_get_size(gw->iut_addr->sa_family) * 8,
               te_sockaddr_get_netaddr(gw->gw_tst_addr)));

    tapi_route_gateway_set_forwarding(gw, TRUE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_set_forwarding(tapi_route_gateway *gw,
                                  te_bool enabled)
{
    CHECK_RC(tapi_cfg_base_ipv4_fw(gw->gw_ta, &enabled));
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_break_gw_iut(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->gw_ta, gw->gw_iut_if, NULL, NULL,
                    gw->iut_addr, gw->alien_link_addr, TRUE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_repair_gw_iut(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->gw_ta, gw->gw_iut_if, gw->iut_ta, gw->iut_if,
                    gw->iut_addr, NULL, FALSE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_break_gw_tst(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->gw_ta, gw->gw_tst_if, NULL, NULL,
                    gw->tst_addr, gw->alien_link_addr, TRUE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_repair_gw_tst(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->gw_ta, gw->gw_tst_if, gw->tst_ta, gw->tst_if,
                    gw->tst_addr, NULL, FALSE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_break_iut_gw(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->iut_ta, gw->iut_if, NULL, NULL,
                    gw->gw_iut_addr, gw->alien_link_addr, TRUE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_repair_iut_gw(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->iut_ta, gw->iut_if, gw->gw_ta, gw->gw_iut_if,
                    gw->gw_iut_addr, NULL, FALSE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_break_tst_gw(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->tst_ta, gw->tst_if, NULL, NULL,
                    gw->gw_tst_addr, gw->alien_link_addr, TRUE);
}

/* See description in tapi_route_gw.h */
void
tapi_route_gateway_repair_tst_gw(tapi_route_gateway *gw)
{
    tapi_update_arp(gw->tst_ta, gw->tst_if, gw->gw_ta, gw->gw_tst_if,
                    gw->gw_tst_addr, NULL, FALSE);
}