/** @file
 * @brief TAD RTE mbuf
 *
 * Traffic Application Domain Command Handler
 * RTE mbuf CSAP layer-related callbacks
 *
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS in
 * the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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

#define TE_LGR_USER     "TAD RTE mbuf"

#include "te_config.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "te_errno.h"
#include "logger_api.h"
#include "logger_ta_fast.h"
#include "asn_usr.h"

#include "tad_csap_support.h"
#include "tad_csap_inst.h"
#include "tad_bps.h"

#include "tad_rte_mbuf_impl.h"

te_errno
tad_rte_mbuf_gen_bin_cb(csap_p                  csap,
                        unsigned int            layer,
                        const asn_value        *tmpl_pdu,
                        void                   *opaque,
                        const tad_tmpl_arg_t   *args,
                        size_t                  arg_num,
                        tad_pkts               *sdus,
                        tad_pkts               *pdus)
{
    UNUSED(csap);
    UNUSED(layer);
    UNUSED(tmpl_pdu);
    UNUSED(opaque);
    UNUSED(args);
    UNUSED(arg_num);

    /* Move all SDUs to PDUs */
    tad_pkts_move(pdus, sdus);

    return 0;
}

te_errno
tad_rte_mbuf_match_post_cb(csap_p              csap,
                           unsigned int        layer,
                           tad_recv_pkt_layer *meta_pkt_layer)
{
    UNUSED(layer);

    if (~csap->state & CSAP_STATE_RESULTS)
        return 0;

    if ((meta_pkt_layer->nds = asn_init_value(ndn_rte_mbuf_pdu)) == NULL)
    {
        ERROR_ASN_INIT_VALUE(ndn_rte_mbuf_pdu);
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);
    }

    return 0;
}

te_errno
tad_rte_mbuf_match_do_cb(csap_p              csap,
                         unsigned int        layer,
                         const asn_value    *ptrn_pdu,
                         void               *ptrn_opaque,
                         tad_recv_pkt       *meta_pkt,
                         tad_pkt            *pdu,
                         tad_pkt            *sdu)
{
    te_errno    rc;

    UNUSED(layer);
    UNUSED(ptrn_pdu);
    UNUSED(ptrn_opaque);
    UNUSED(meta_pkt);

    rc = tad_pkt_get_frag(sdu, pdu, 0, tad_pkt_len(pdu),
                          TAD_PKT_GET_FRAG_ERROR);
    if (rc != 0)
    {
        ERROR(CSAP_LOG_FMT "Failed to prepare RTE mbuf SDU: %r",
              CSAP_LOG_ARGS(csap), rc);
        return rc;
    }

    EXIT(CSAP_LOG_FMT "OK", CSAP_LOG_ARGS(csap));

    return 0;
}