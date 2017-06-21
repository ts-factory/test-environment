/** @file
 * @brief TAD Overlay Auxiliary Tools
 *
 * Traffic Application Domain Command Handler
 * Overlay Auxiliary Tools implementation
 *
 * Copyright (C) 2017 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
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
 * @author Ivan Malov <Ivan.Malov@oktetlabs.ru>
 */

#define TE_LGR_USER "TAD Overlay Auxiliary Tools"

#include "te_config.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "te_errno.h"
#include "tad_csap_support.h"
#include "tad_bps.h"

/*
 * TAD protocol numbers to ethertype decimal numbers mapping
 * used to fill in 'protocol' field of Geneve or GRE headers
 *
 * https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xml
 */
static struct {
    te_tad_protocols_t proto_tag;
    uint16_t           ethertype;
} const tad_overlay_proto_ethertype[] = {
    { TE_PROTO_ETH, 25944 } /* Trans Ether Bridging */
};

/* See description in 'tad_overlay_tools.h' */
te_errno
tad_overlay_guess_def_protocol(csap_p                csap,
                               unsigned int          layer_idx,
                               tad_bps_pkt_frag_def *def,
                               unsigned int          du_idx)
{
    unsigned int i;

    if ((csap == NULL ) || (csap->layers == NULL) ||
        (csap->depth <= layer_idx) ||
        (def == NULL) || (du_idx >= def->fields) ||
        (def->tx_def == NULL) || (def->rx_def == NULL))
        return TE_RC(TE_TAD_CSAP, TE_EINVAL);

    if ((layer_idx == 0) ||
        (def->tx_def[du_idx].du_type != TAD_DU_UNDEF) ||
        (def->rx_def[du_idx].du_type != TAD_DU_UNDEF))
        return 0;

    for (i = 0; i < TE_ARRAY_LEN(tad_overlay_proto_ethertype); ++i)
    {
        if (csap->layers[layer_idx - 1].proto_tag !=
            tad_overlay_proto_ethertype[i].proto_tag)
            continue;

        def->tx_def[du_idx].du_type = TAD_DU_I32;
        def->tx_def[du_idx].val_i32 = tad_overlay_proto_ethertype[i].ethertype;
        def->rx_def[du_idx].du_type = TAD_DU_I32;
        def->rx_def[du_idx].val_i32 = tad_overlay_proto_ethertype[i].ethertype;

        break;
    }

    return 0;
}