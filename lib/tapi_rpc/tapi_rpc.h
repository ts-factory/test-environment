/** @file
 * @brief Test API - RPC
 *
 * Definition of TAPI for all remote calls.
 *
 * Most functions have the same prototype and semantics of parameters
 * and return code as Linux implementation of Berkeley Socket API
 * functions.
 *
 * The only exception is the first parameter - RPC server handle created
 * using RCF RPC library calls.
 *
 * If NULL pointer is provided to API call, no data are read/write
 * from/to.  The pointer is just passed to RPC interface to call
 * remote function with NULL argument.
 *
 * *_gen functions provide more generic interface for test purposes.
 * Additional arguments specify real length of the provided buffers.
 * Such functionality is required to verify that called function
 * writes nothing across provided buffer boundaries.
 *
 *
 * Copyright (C) 2005 Test Environment authors (see file AUTHORS in
 * the root directory of the distribution).
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

#ifndef __TE_TAPI_RPC_H__
#define __TE_TAPI_RPC_H__

#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_NET_IF_ARP_H
#include <net/if_arp.h>
#endif

#include "tapi_rpc_socket.h"
#include "tapi_rpc_unistd.h"
#include "tapi_rpc_signal.h"
#include "tapi_rpc_stdio.h"
#include "tapi_rpc_ifnameindex.h"
#include "tapi_rpc_netdb.h"
#include "tapi_rpc_aio.h"
#include "tapi_rpc_winsock2.h"
#include "tapi_rpc_misc.h"

#endif /* !__TE_TAPI_RPC_H__ */
