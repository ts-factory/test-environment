/** @file
 * @brief Test API to configure DHCP.
 *
 * Definition of API to configure DHCP.
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
 *
 * $Id$
 */

#ifndef __TE_TAPI_CFG_DHCP_H__
#define __TE_TAPI_CFG_DHCP_H__

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include "conf_api.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add host definition in DHCP server on the Test Agent.
 *
 * @param ta            Test Agent
 * @param name          Name of the host for human or NULL (unique name
 *                      will be generated by TAPI)
 * @param group         Name of the group the host belongs to or NULL
 * @param chaddr        Hardware address
 * @param client_id     Client identifier or NULL
 * @param client_id_len Client identifier length if ID is in binary
 *                      format or -1 if ID is a string 
 * @param fixed_ip      Fixed IP address or NULL
 * @param next_server   Next server or NULL
 * @param filename      File name or NULL
 * @param handle        Location for handle of the created instance or
 *                      NULL
 *
 * @return Status code
 */
extern int tapi_cfg_dhcps_add_host(const char            *ta,
                                   const char            *name,
                                   const char            *group,
                                   const struct sockaddr *chaddr,
                                   const void           *client_id,
                                   int                   client_id_len,
                                   const struct sockaddr *fixed_ip,
                                   const char            *next_server,
                                   const char            *filename,
                                   cfg_handle            *handle);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_CFG_DHCP_H__ */
