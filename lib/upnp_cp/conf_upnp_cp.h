/** @file
 * @brief Unix Test Agent UPnP Control Point support.
 *
 * Definition of unix TA UPnP Control Point configuring support.
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS
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
 *
 * @author Ivan Melnikov <Ivan.Melnikov@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __CONF_UPNP_CP_H__
#define __CONF_UPNP_CP_H__

#include "te_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the UPnP Control Point configuration subtrees and default
 * settings.
 *
 * @return Status code.
 */
extern te_errno ta_unix_conf_upnp_cp_init(void);

/**
 * Stop the UPnP Control Point process.
 *
 * @return Status code.
 */
extern te_errno ta_unix_conf_upnp_cp_release(void);

/**
 * Set the UPnP Control Point pathname for the UNIX socket which is need to
 * connect to the Control Point.
 *
 * @param ta_path       Path to the TA workspace.
 */
extern void ta_unix_conf_upnp_cp_set_socket_name(const char *ta_path);

/**
 * Get the UPnP Control Point pathname for the UNIX socket which is need to
 * connect to the Control Point.
 *
 * @return Pathname for the UNIX socket.
 */
extern const char * ta_unix_conf_upnp_cp_get_socket_name(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__CONF_UPNP_CP_H__ */