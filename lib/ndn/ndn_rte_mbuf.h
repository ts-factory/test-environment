/** @file
 * @brief Proteos, TAD RTE mbuf, NDN
 *
 * Declarations of ASN.1 types for NDN of RTE mbuf pseudo-protocol
 *
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS in
 * the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
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
 * @author Ivan Malov <Ivan.Malov@oktetlabs.ru>
 */

#ifndef __TE_NDN_RTE_MBUF_H__
#define __TE_NDN_RTE_MBUF_H__

#include "te_stdint.h"
#include "te_ethernet.h"
#include "asn_usr.h"
#include "ndn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These values must match the NDN for RTE mbuf */
typedef enum {
    NDN_TAG_RTE_MBUF_RING = 0,
    NDN_TAG_RTE_MBUF_POOL,
} ndn_rte_mbuf_tags_t;

extern const asn_type * const ndn_rte_mbuf_pdu;
extern const asn_type * const ndn_rte_mbuf_csap;

extern asn_type ndn_rte_mbuf_pdu_s;
extern asn_type ndn_rte_mbuf_csap_s;

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* __TE_NDN_RTE_MBUF_H__ */