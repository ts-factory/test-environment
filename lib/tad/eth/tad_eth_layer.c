/** @file
 * @brief TAD Ethernet
 *
 * Traffic Application Domain Command Handler.
 * Ethernet CSAP layer-related callbacks.
 *
 * See IEEE 802.1d, 802.1q.
 *
 * Copyright (C) 2003-2006 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
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
 * @author Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "TAD Ethernet"

#include "te_config.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#if HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif
#if HAVE_NET_IF_ETHER_H
#include <net/if_ether.h>
#endif

#include "te_defs.h"
#include "logger_api.h"
#include "logger_ta_fast.h"

#include "tad_bps.h"
#include "tad_eth_impl.h"


/** 802.1Q Tag Protocol Type (IEEE Std 802.1Q-2003 9.3.1) */
#define TAD_802_1Q_TAG_TYPE     0x8100


/** Type of Ethernet frame */
typedef enum tad_eth_frame_type {
    TAD_ETH_FRAME_TYPE_ANY,
    TAD_ETH_FRAME_TYPE_UNTAGGED,
    TAD_ETH_FRAME_TYPE_TAGGED,
    TAD_ETH_FRAME_TYPE_SNAP,
} tad_eth_frame_type;


/**
 * Ethernet layer specific data
 */
typedef struct tad_eth_proto_data {
    tad_eth_frame_type      type;
    tad_bps_pkt_frag_def    eth;
    tad_bps_pkt_frag_def    len_type;
    tad_bps_pkt_frag_def    tpid;
    tad_bps_pkt_frag_def    tci;
    tad_bps_pkt_frag_def    e_rif;
    tad_bps_pkt_frag_def    snap;
} tad_eth_proto_data;

/**
 * Ethernet layer specific data for PDU processing (both send and
 * receive).
 */
typedef struct tad_eth_proto_pdu_data {
    tad_eth_frame_type      type;
    tad_bps_pkt_frag_data   eth;
    tad_bps_pkt_frag_data   len_type;
    tad_bps_pkt_frag_data   tpid;
    tad_bps_pkt_frag_data   tci;
    tad_bps_pkt_frag_data   e_rif;
    tad_bps_pkt_frag_data   snap;
} tad_eth_proto_pdu_data;


/**
 * Definition of 802.3 Ethernet header.
 */
static const tad_bps_pkt_frag tad_eth_addrs_bps_hdr[] =
{
    { "dst-addr", 48, NDN_TAG_ETH_DST,
      NDN_TAG_ETH_REMOTE, NDN_TAG_ETH_LOCAL, 0, TAD_DU_OCTS },
    { "src-addr", 48, NDN_TAG_ETH_SRC,
      NDN_TAG_ETH_LOCAL, NDN_TAG_ETH_REMOTE, 0, TAD_DU_OCTS },
};

/**
 * Definition of Length-Type field which can follow after sources MAC
 * address or TCI in the case of 802.1Q.
 */
static const tad_bps_pkt_frag tad_eth_length_type_bps_hdr[] =
{
    { "length-type", 16, BPS_FLD_SIMPLE(NDN_TAG_ETH_LENGTH_TYPE),
      TAD_DU_I32 },
};

/**
 * Definition of Tag Protocol Identifier (TPID) in the case of 802.1Q.
 */
static const tad_bps_pkt_frag tad_802_1q_tpid_bps_hdr[] =
{
    { "tpid", 16, BPS_FLD_CONST(TAD_802_1Q_TAG_TYPE), TAD_DU_I32 },
};

/**
 * Definition of 802.1q Tag Control Information.
 */
static const tad_bps_pkt_frag tad_802_1q_tci_bps_hdr[] =
{
    { "priority",  3, BPS_FLD_CONST_DEF(NDN_TAG_ETH_PRIO, 0),
      TAD_DU_I32 },
    { "cfi",       1, BPS_FLD_CONST_DEF(NDN_TAG_ETH_CFI, 0),
      TAD_DU_I32 },
    { "vlan-id",  12, BPS_FLD_CONST_DEF(NDN_TAG_ETH_VLAN_ID, 0),
      TAD_DU_I32 },
};

/**
 * Definition of 802.1q E-RIF.
 */
static const tad_bps_pkt_frag tad_802_1q_e_rif_bps_hdr[] =
{
    { "e-rif-rc-rt",   3, BPS_FLD_NO_DEF(NDN_TAG_ETH_ERIF_RC_RT),
      TAD_DU_I32 },
    { "e-rif-rc-lth",  5, BPS_FLD_NO_DEF(NDN_TAG_ETH_ERIF_RC_LTH),
      TAD_DU_I32 },
    { "e-rif-rc-d",    1, BPS_FLD_NO_DEF(NDN_TAG_ETH_ERIF_RC_D),
      TAD_DU_I32 },
    { "e-rif-rc-lf",   6, BPS_FLD_NO_DEF(NDN_TAG_ETH_ERIF_RC_LF),
      TAD_DU_I32 },
    { "e-rif-rc-ncfi", 1, BPS_FLD_NO_DEF(NDN_TAG_ETH_ERIF_RC_NCFI),
      TAD_DU_I32 },
    { "e-rif-rt",      0, BPS_FLD_NO_DEF(NDN_TAG_ETH_ERIF_RT),
      TAD_DU_I32 },
};

/**
 * Definition of 802 LLC/SNAP header.
 */
static const tad_bps_pkt_frag tad_802_snap_bps_hdr[] =
{
    { "llc-addr", 24, BPS_FLD_CONST(0xaaaa03),              TAD_DU_I32 },
    { "snap-oui", 24, BPS_FLD_NO_DEF(NDN_TAG_ETH_SNAP_OUI), TAD_DU_I32 },
    { "snap-pid", 16, BPS_FLD_NO_DEF(NDN_TAG_ETH_SNAP_PID), TAD_DU_I32 },
};


/* See description in tad_eth_impl.h */
te_errno
tad_eth_init_cb(csap_p csap, unsigned int layer)
{
    te_errno            rc;
    tad_eth_proto_data *proto_data;
    const asn_value    *layer_nds;

    proto_data = calloc(1, sizeof(*proto_data));
    if (proto_data == NULL)
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);

    csap_set_proto_spec_data(csap, layer, proto_data);

    layer_nds = csap->layers[layer].nds;

    rc = tad_bps_pkt_frag_init(tad_eth_addrs_bps_hdr,
                               TE_ARRAY_LEN(tad_eth_addrs_bps_hdr),
                               layer_nds, &proto_data->eth);
    if (rc != 0)
        return rc;

    /* FIXME */
    if ((proto_data->eth.tx_def[1].du_type == TAD_DU_UNDEF) &&
        (csap_get_rw_layer(csap) == layer))
    {
        /* Source address is not specified, use local interface address */
        tad_eth_rw_data    *rw_data = csap_get_rw_data(csap);
        eth_interface_p     iface;
        tad_data_unit_t    *du = proto_data->eth.tx_def + 1;

        assert(rw_data != NULL);
        iface = rw_data->interface;
        assert(rw_data != NULL);

        du->val_data.len = sizeof(iface->local_addr);
        du->val_data.oct_str = malloc(du->val_data.len);
        if (du->val_data.oct_str == NULL)
            return TE_RC(TE_TAD_CSAP, TE_ENOMEM);

        memcpy(du->val_data.oct_str, iface->local_addr,
               sizeof(iface->local_addr));

        /* Successfully got */
        du->du_type = TAD_DU_OCTS;
    }

    rc = tad_bps_pkt_frag_init(tad_eth_length_type_bps_hdr,
                               TE_ARRAY_LEN(tad_eth_length_type_bps_hdr),
                               layer_nds, &proto_data->len_type);
    if (rc != 0)
        return rc;

    /* FIXME */
    if (layer > 0 &&
        proto_data->len_type.tx_def[0].du_type == TAD_DU_UNDEF &&
        proto_data->len_type.rx_def[0].du_type == TAD_DU_UNDEF)
    {
        uint16_t    eth_type;

        VERB("%s(): eth-type is not defined, try to guess", __FUNCTION__);
        switch (csap->layers[layer - 1].proto_tag)
        {
            case TE_PROTO_IP4:
                eth_type = ETHERTYPE_IP;
                break;

            case TE_PROTO_ARP:
                eth_type = ETHERTYPE_ARP;
                break;

            default:
                eth_type = 0;
                break;
        }
        if (eth_type != 0)
        {
            INFO("%s(): Guessed eth-type is 0x%x", __FUNCTION__, eth_type);
            proto_data->len_type.tx_def[0].du_type = TAD_DU_I32;
            proto_data->len_type.tx_def[0].val_i32 = eth_type;
            proto_data->len_type.rx_def[0].du_type = TAD_DU_I32;
            proto_data->len_type.rx_def[0].val_i32 = eth_type;
        }
    }

#if 0
    const asn_value    *frame_type;

    rc = asn_get_child_value(layer_nds, &frame_type,
                             PRIVATE, NDN_TAG_ETH_FRAME_TYPE);
    if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)
    {
        proto_data->type = TAD_ETH_FRAME_TYPE_ANY;
    }
    else if (rc != 0)
    {
        ERROR("%s(): Failed to get 'frame-type' from NDS: %r",
              __FUNCTION__, rc);
        return rc;
    }
    else
    {
        asn_tag_class   class;
        asn_tag_value   tag;

        rc = asn_get_choice_value(frame_type, (asn_value **)&frame_type,
                                  &class, &tag);
        if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)
        {
            proto_data->type = TAD_ETH_FRAME_TYPE_ANY;
        }
        else if (rc != 0)
        {
            ERROR("%s(): Failed to get 'frame-type' choice from NDS: %r",
                  __FUNCTION__, rc);
            return rc;
        }
        else if (class != PRIVATE)
        {
            ERROR("%s(): Invalid choice tag class in 'frame-type'",
                  __FUNCTION__);
            return TE_RC(TE_TAD_CSAP, TE_ETADWRONGNDS);
        }
        else if (tag == NDN_TAG_ETH_UNTAGGED)
        {
            proto_data->type = TAD_ETH_FRAME_TYPE_UNTAGGED;
        }
        else if (tag == NDN_TAG_ETH_TAGGED)
        {
            proto_data->type = TAD_ETH_FRAME_TYPE_TAGGED;

            /* FIXME: Update length-type in header */

            rc = tad_bps_pkt_frag_init(tad_802_1q_tci_bps_hdr,
                     TE_ARRAY_LEN(tad_802_1q_tci_bps_hdr),
                     frame_type, &proto_data->tci);
            if (rc != 0)
                return rc;

            rc = tad_bps_pkt_frag_init(tad_802_1q_e_rif_bps_hdr,
                     TE_ARRAY_LEN(tad_802_1q_e_rif_bps_hdr),
                     frame_type, &proto_data->e_rif);
            if (rc != 0)
                return rc;

            rc = tad_bps_pkt_frag_init(tad_eth_length_type_bps_hdr,
                     TE_ARRAY_LEN(tad_eth_length_type_bps_hdr),
                     layer_nds, &proto_data->len_type);
            if (rc != 0)
                return rc;
        }
        else if (tag == NDN_TAG_ETH_SNAP)
        {
            proto_data->type = TAD_ETH_FRAME_TYPE_SNAP;

            /* FIXME: Check length-type in header */

            rc = tad_bps_pkt_frag_init(tad_802_snap_bps_hdr,
                     TE_ARRAY_LEN(tad_802_snap_bps_hdr),
                     frame_type, &proto_data->snap);
            if (rc != 0)
                return rc;
        }
        else
        {
            ERROR("%s(): Invalid choice tag in 'frame-type'",
                  __FUNCTION__);
            return TE_RC(TE_TAD_CSAP, TE_ETADWRONGNDS);
        }
    }
#else
    rc = tad_bps_pkt_frag_init(tad_802_1q_tpid_bps_hdr,
             TE_ARRAY_LEN(tad_802_1q_tpid_bps_hdr),
             NULL, &proto_data->tpid);
    if (rc != 0)
        return rc;
    rc = tad_bps_pkt_frag_init(tad_802_1q_tci_bps_hdr,
             TE_ARRAY_LEN(tad_802_1q_tci_bps_hdr),
             NULL, &proto_data->tci);
    if (rc != 0)
        return rc;
    rc = tad_bps_pkt_frag_init(tad_802_1q_e_rif_bps_hdr,
             TE_ARRAY_LEN(tad_802_1q_e_rif_bps_hdr),
             NULL, &proto_data->e_rif);
    if (rc != 0)
        return rc;
    rc = tad_bps_pkt_frag_init(tad_802_snap_bps_hdr,
             TE_ARRAY_LEN(tad_802_snap_bps_hdr),
             NULL, &proto_data->snap);
    if (rc != 0)
        return rc;
#endif

    return rc;
}

/* See description in tad_eth_impl.h */
te_errno
tad_eth_destroy_cb(csap_p csap, unsigned int layer)
{
    tad_eth_proto_data *proto_data;

    proto_data = csap_get_proto_spec_data(csap, layer);
    csap_set_proto_spec_data(csap, layer, NULL);

    tad_bps_pkt_frag_free(&proto_data->eth);
    tad_bps_pkt_frag_free(&proto_data->len_type);
    tad_bps_pkt_frag_free(&proto_data->tpid);
    tad_bps_pkt_frag_free(&proto_data->tci);
    tad_bps_pkt_frag_free(&proto_data->e_rif);
    tad_bps_pkt_frag_free(&proto_data->snap);

    free(proto_data);

    return 0;
}


/**
 * Convert traffic template/pattern NDS to BPS internal data.
 *
 * @param csap          CSAP instance
 * @param proto_data    Protocol data prepared during CSAP creation
 * @param layer_pdu     Layer NDS
 * @param p_pdu_data    Location for PDU data pointer (updated in any
 *                      case and should be released by caller even in
 *                      the case of failure)
 *
 * @return Status code.
 */
static te_errno
tad_eth_nds_to_pdu_data(csap_p csap, tad_eth_proto_data *proto_data,
                        const asn_value *layer_pdu,
                        tad_eth_proto_pdu_data **p_pdu_data)
{
    te_errno                    rc;
    tad_eth_proto_pdu_data     *pdu_data;
    const asn_value            *frame_type;

    assert(proto_data != NULL);
    assert(layer_pdu != NULL);
    assert(p_pdu_data != NULL);

    *p_pdu_data = pdu_data = calloc(1, sizeof(*pdu_data));
    if (pdu_data == NULL)
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);

    rc = tad_bps_nds_to_data_units(&proto_data->eth, layer_pdu,
                                   &pdu_data->eth);
    if (rc != 0)
        return rc;

    rc = tad_bps_nds_to_data_units(&proto_data->len_type, layer_pdu,
                                   &pdu_data->len_type);
    if (rc != 0)
        return rc;

    rc = asn_get_child_value(layer_pdu, &frame_type,
                             PRIVATE, NDN_TAG_ETH_FRAME_TYPE);
    if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)
    {
        rc = 0;
        pdu_data->type = proto_data->type;
        F_VERB(CSAP_LOG_FMT "'frame-type' is not specified in PDU - "
               "inherit from CSAP parameters %u", CSAP_LOG_ARGS(csap),
               pdu_data->type);
    }
    else if (rc != 0)
    {
        ERROR("%s(): Failed to get 'frame-type' from NDS: %r",
              __FUNCTION__, rc);
        return rc;
    }
    else
    {
        asn_tag_class   class;
        asn_tag_value   tag;

        rc = asn_get_choice_value(frame_type, (asn_value **)&frame_type,
                                  &class, &tag);
        if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)
        {
            rc = 0;
            pdu_data->type = proto_data->type;
            F_VERB(CSAP_LOG_FMT "'frame-type' is not specified in PDU - "
                   "inherit from CSAP parameters %u", CSAP_LOG_ARGS(csap),
                   pdu_data->type);
        }
        else if (rc != 0)
        {
            ERROR("%s(): Failed to get 'frame-type' choice from NDS: %r",
                  __FUNCTION__, rc);
            return rc;
        }
        else if (class != PRIVATE)
        {
            ERROR("%s(): Invalid choice tag class in 'frame-type'",
                  __FUNCTION__);
            return TE_RC(TE_TAD_CSAP, TE_ETADWRONGNDS);
        }
        else if (tag == NDN_TAG_ETH_UNTAGGED)
        {
            pdu_data->type = TAD_ETH_FRAME_TYPE_UNTAGGED;
            F_VERB(CSAP_LOG_FMT "Untagged frame format",
                   CSAP_LOG_ARGS(csap));
        }
        else if (tag == NDN_TAG_ETH_TAGGED)
        {
            pdu_data->type = TAD_ETH_FRAME_TYPE_TAGGED;
            F_VERB(CSAP_LOG_FMT "Tagged frame format",
                   CSAP_LOG_ARGS(csap));

            rc = tad_bps_nds_to_data_units(&proto_data->tpid, frame_type,
                                           &pdu_data->tpid);
            if (rc != 0)
                return rc;

            rc = tad_bps_nds_to_data_units(&proto_data->tci, frame_type,
                                           &pdu_data->tci);
            if (rc != 0)
                return rc;

            rc = tad_bps_nds_to_data_units(&proto_data->e_rif, frame_type,
                                           &pdu_data->e_rif);
            if (rc != 0)
                return rc;
        }
        else if (tag == NDN_TAG_ETH_SNAP)
        {
            pdu_data->type = TAD_ETH_FRAME_TYPE_SNAP;
            F_VERB(CSAP_LOG_FMT "SNAP frame format",
                   CSAP_LOG_ARGS(csap));

            /* FIXME: Check length-type in header */

            rc = tad_bps_nds_to_data_units(&proto_data->snap, frame_type,
                                           &pdu_data->snap);
            if (rc != 0)
                return rc;
        }
        else
        {
            ERROR("%s(): Invalid choice tag in 'frame-type'",
                  __FUNCTION__);
            return TE_RC(TE_TAD_CSAP, TE_ETADWRONGNDS);
        }
    }

    return rc;
}

/* See description in tad_eth_impl.h */
void
tad_eth_release_pdu_cb(csap_p csap, unsigned int layer, void *opaque)
{
    tad_eth_proto_data     *proto_data;
    tad_eth_proto_pdu_data *pdu_data = opaque;

    proto_data = csap_get_proto_spec_data(csap, layer);
    assert(proto_data != NULL);

    if (pdu_data != NULL)
    {
        tad_bps_free_pkt_frag_data(&proto_data->eth, &pdu_data->eth);
        tad_bps_free_pkt_frag_data(&proto_data->len_type,
                                   &pdu_data->len_type);
        tad_bps_free_pkt_frag_data(&proto_data->tpid, &pdu_data->tpid);
        tad_bps_free_pkt_frag_data(&proto_data->tci, &pdu_data->tci);
        tad_bps_free_pkt_frag_data(&proto_data->e_rif, &pdu_data->e_rif);
        tad_bps_free_pkt_frag_data(&proto_data->snap, &pdu_data->snap);
        free(pdu_data);
    }
}


/* See description in tad_eth_impl.h */
te_errno
tad_eth_confirm_tmpl_cb(csap_p csap, unsigned int layer,
                        asn_value *layer_pdu, void **p_opaque)
{
    te_errno                    rc;
    tad_eth_proto_data         *proto_data;
    tad_eth_proto_pdu_data     *tmpl_data;

    proto_data = csap_get_proto_spec_data(csap, layer);

    rc = tad_eth_nds_to_pdu_data(csap, proto_data, layer_pdu, &tmpl_data);
    *p_opaque = tmpl_data;
    if (rc != 0)
        return rc;

    tmpl_data = *p_opaque;

    /* By default send untagged Ethernet frames */
    if (tmpl_data->type == TAD_ETH_FRAME_TYPE_ANY)
        tmpl_data->type = TAD_ETH_FRAME_TYPE_UNTAGGED;

    rc = tad_bps_confirm_send(&proto_data->eth, &tmpl_data->eth);
    if (rc != 0)
        return rc;

    rc = tad_bps_confirm_send(&proto_data->len_type, &tmpl_data->len_type);
    if (rc != 0)
        return rc;

    if (tmpl_data->type == TAD_ETH_FRAME_TYPE_UNTAGGED)
    {
        /* Nothing special to confirm in this case */
    }
    else if (tmpl_data->type == TAD_ETH_FRAME_TYPE_TAGGED)
    {
        /* Nothing to check for TPID - it is a constant */
        tad_data_unit_t *cfi_du =
            (tmpl_data->tci.dus[1].du_type == TAD_DU_UNDEF) ?
            (proto_data->tci.tx_def + 1) : (tmpl_data->tci.dus + 1);

        rc = tad_bps_confirm_send(&proto_data->tci, &tmpl_data->tci);
        if (rc != 0)
            return rc;

#if 1
        if (cfi_du->du_type != TAD_DU_I32)
        {
            ERROR("%s(): Not plain (%u) CFI is not supported yet",
                  __FUNCTION__, cfi_du->du_type);
            return TE_RC(TE_TAD_CSAP, TE_EOPNOTSUPP);
        }
#endif

        /* If CFI bit is set, confirm E-RIF */
        if (cfi_du->val_i32 == 1)
        {
            rc = tad_bps_confirm_send(&proto_data->e_rif, &tmpl_data->e_rif);
            if (rc != 0)
                return rc;
        }
    }
    else if (tmpl_data->type == TAD_ETH_FRAME_TYPE_SNAP)
    {
        rc = tad_bps_confirm_send(&proto_data->snap, &tmpl_data->snap);
        if (rc != 0)
            return rc;
    }
    else
    {
        /* Impossible */
        assert(FALSE);
    }

    return rc;
}


/**
 * Check length of packet as Ethernet frame. If frame is too small,
 * add a new segment with data filled in by zeros.
 *
 * This function complies with tad_pkt_enum_cb prototype.
 */
static te_errno
tad_eth_check_frame_len(tad_pkt *pkt, void *opaque)
{
    ssize_t tailer_len = (ETHER_MIN_LEN - ETHER_CRC_LEN) -
                         tad_pkt_len(pkt);

    UNUSED(opaque);
    if (tailer_len > 0)
    {
        tad_pkt_seg *seg = tad_pkt_alloc_seg(NULL, tailer_len, NULL);

        if (seg == NULL)
        {
            ERROR("%s(): Failed to allocate a new segment",
                  __FUNCTION__);
            return TE_RC(TE_TAD_PKT, TE_ENOMEM);
        }
        memset(seg->data_ptr, 0, seg->data_len);
        tad_pkt_append_seg(pkt, seg);
    }
    return 0;
}

/* See description in tad_eth_impl.h */
te_errno
tad_eth_gen_bin_cb(csap_p csap, unsigned int layer,
                   const asn_value *tmpl_pdu, void *opaque,
                   const tad_tmpl_arg_t *args, size_t arg_num, 
                   tad_pkts *sdus, tad_pkts *pdus)
{
    tad_eth_proto_data     *proto_data;
    tad_eth_proto_pdu_data *tmpl_data = opaque;

    te_errno        rc;
    size_t          bitlen;
    unsigned int    bitoff;
    uint8_t        *data;
    size_t          len;


    assert(csap != NULL);
    F_ENTRY("(%d:%u) tmpl_pdu=%p args=%p arg_num=%u sdus=%p pdus=%p",
            csap->id, layer, (void *)tmpl_pdu, (void *)args,
            (unsigned)arg_num, sdus, pdus);

    proto_data = csap_get_proto_spec_data(csap, layer);

    bitlen = tad_bps_pkt_frag_data_bitlen(&proto_data->eth,
                                          &tmpl_data->eth) +
             tad_bps_pkt_frag_data_bitlen(&proto_data->len_type,
                                          &tmpl_data->len_type);
    if (tmpl_data->type == TAD_ETH_FRAME_TYPE_UNTAGGED)
    {
        /* Nothing special to add in this case */
    }
    else if (tmpl_data->type == TAD_ETH_FRAME_TYPE_TAGGED)
    {
        bitlen += tad_bps_pkt_frag_data_bitlen(&proto_data->tpid,
                                               &tmpl_data->tpid) +
                  tad_bps_pkt_frag_data_bitlen(&proto_data->tci,
                                               &tmpl_data->tci);

        /* If CFI bit is set, confirm E-RIF */
        if (tmpl_data->tci.dus[1].du_type == TAD_DU_I32 &&
            tmpl_data->tci.dus[1].val_i32 == 1)
        {
            bitlen += tad_bps_pkt_frag_data_bitlen(&proto_data->e_rif,
                                                   &tmpl_data->e_rif);
        }
    }
    else if (tmpl_data->type == TAD_ETH_FRAME_TYPE_SNAP)
    {
        bitlen += tad_bps_pkt_frag_data_bitlen(&proto_data->snap,
                                               &tmpl_data->snap);
    }
    else
    {
        /* Impossible */
        assert(FALSE);
    }

    assert((bitlen & 7) == 0);

    len = (bitlen >> 3);
    data = malloc(len);
    if (data == NULL)
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);

    bitoff = 0;

    rc = tad_bps_pkt_frag_gen_bin(&proto_data->eth, &tmpl_data->eth,
                                  args, arg_num, data, &bitoff, bitlen);
    if (rc != 0)
    {
        ERROR("%s(): tad_bps_pkt_frag_gen_bin failed for addresses: %r",
              __FUNCTION__, rc);
        free(data);
        return rc;
    }

    if (tmpl_data->type == TAD_ETH_FRAME_TYPE_TAGGED)
    {
        rc = tad_bps_pkt_frag_gen_bin(&proto_data->tpid, &tmpl_data->tpid,
                                      args, arg_num,
                                      data, &bitoff, bitlen);
        if (rc != 0)
        {
            ERROR("%s(): tad_bps_pkt_frag_gen_bin() failed for TPID: %r",
                  __FUNCTION__, rc);
            free(data);
            return rc;
        }

        rc = tad_bps_pkt_frag_gen_bin(&proto_data->tci, &tmpl_data->tci,
                                      args, arg_num,
                                      data, &bitoff, bitlen);
        if (rc != 0)
        {
            ERROR("%s(): tad_bps_pkt_frag_gen_bin() failed for TCI: %r",
                  __FUNCTION__, rc);
            free(data);
            return rc;
        }
    }

    rc = tad_bps_pkt_frag_gen_bin(&proto_data->len_type,
                                  &tmpl_data->len_type,
                                  args, arg_num,
                                  data, &bitoff, bitlen);
    if (rc != 0)
    {
        ERROR("%s(): tad_bps_pkt_frag_gen_bin() failed for "
              "Length/Type: %r", __FUNCTION__, rc);
        free(data);
        return rc;
    }

    if (tmpl_data->type == TAD_ETH_FRAME_TYPE_TAGGED &&
        tmpl_data->tci.dus[1].du_type == TAD_DU_I32 &&
        tmpl_data->tci.dus[1].val_i32 == 1)
    {
        /* CFI bit is set, confirm E-RIF */
        rc = tad_bps_pkt_frag_gen_bin(&proto_data->e_rif,
                                      &tmpl_data->e_rif,
                                      args, arg_num,
                                      data, &bitoff, bitlen);
        if (rc != 0)
        {
            ERROR("%s(): tad_bps_pkt_frag_gen_bin() failed for "
                  "E-RIF: %r", __FUNCTION__, rc);
            free(data);
            return rc;
        }
    }

    if (tmpl_data->type == TAD_ETH_FRAME_TYPE_SNAP)
    {
        rc = tad_bps_pkt_frag_gen_bin(&proto_data->snap, &tmpl_data->snap,
                                      args, arg_num,
                                      data, &bitoff, bitlen);
        if (rc != 0)
        {
            ERROR("%s(): tad_bps_pkt_frag_gen_bin() failed for SNAP: %r",
                  __FUNCTION__, rc);
            free(data);
            return rc;
        }
    }

    assert(bitoff == bitlen);

    /* Move all SDUs to PDUs */
    tad_pkts_move(pdus, sdus);
    /* 
     * Add header segment to each PDU. All segments refer to the same
     * memory. Free function is set for segment of the first packet only.
     */
    rc = tad_pkts_add_new_seg(pdus, TRUE, data, len,
                              tad_pkt_seg_data_free);
    if (rc != 0)
    {
        free(data);
        return rc;
    }

    rc = tad_pkt_enumerate(pdus, tad_eth_check_frame_len, NULL);
    if (rc != 0)
    {
        ERROR("Failed to check length of Ethernet frames to send: %r", rc);
        return rc;
    }

    return 0;
}


/* See description in tad_eth_impl.h */
te_errno
tad_eth_confirm_ptrn_cb(csap_p csap, unsigned int layer,
                        asn_value *layer_pdu, void **p_opaque)
{
    te_errno                rc;
    tad_eth_proto_data     *proto_data;
    tad_eth_proto_pdu_data *ptrn_data;

    F_ENTRY("(%d:%u) layer_pdu=%p", csap->id, layer,
            (void *)layer_pdu);

    proto_data = csap_get_proto_spec_data(csap, layer);

    rc = tad_eth_nds_to_pdu_data(csap, proto_data, layer_pdu, &ptrn_data);
    *p_opaque = ptrn_data;

    if (ptrn_data->type == TAD_ETH_FRAME_TYPE_ANY)
    {
        rc = tad_bps_nds_to_data_units(&proto_data->tpid, NULL,
                                       &ptrn_data->tpid);
        if (rc != 0)
            return rc;

        rc = tad_bps_nds_to_data_units(&proto_data->tci, NULL,
                                       &ptrn_data->tci);
        if (rc != 0)
            return rc;

        rc = tad_bps_nds_to_data_units(&proto_data->e_rif, NULL,
                                       &ptrn_data->e_rif);
        if (rc != 0)
            return rc;

        rc = tad_bps_nds_to_data_units(&proto_data->snap, NULL,
                                       &ptrn_data->snap);
        if (rc != 0)
            return rc;
    }

    return rc;
}


/* See description in tad_eth_impl.h */
te_errno
tad_eth_match_pre_cb(csap_p              csap,
                     unsigned int        layer,
                     tad_recv_pkt_layer *meta_pkt_layer)
{
    tad_eth_proto_data     *proto_data;
    tad_eth_proto_pdu_data *pkt_data;
    te_errno                rc;

    proto_data = csap_get_proto_spec_data(csap, layer);

    pkt_data = malloc(sizeof(*pkt_data));
    if (pkt_data == NULL)
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);
    meta_pkt_layer->opaque = pkt_data;

    rc = tad_bps_pkt_frag_match_pre(&proto_data->eth, &pkt_data->eth);
    if (rc == 0)
        rc = tad_bps_pkt_frag_match_pre(&proto_data->len_type,
                                        &pkt_data->len_type);
    if (rc == 0)
        rc = tad_bps_pkt_frag_match_pre(&proto_data->tpid,
                                        &pkt_data->tpid);
    if (rc == 0)
        rc = tad_bps_pkt_frag_match_pre(&proto_data->tci,
                                        &pkt_data->tci);
    if (rc == 0)
        rc = tad_bps_pkt_frag_match_pre(&proto_data->e_rif,
                                        &pkt_data->e_rif);
    if (rc == 0)
        rc = tad_bps_pkt_frag_match_pre(&proto_data->snap,
                                        &pkt_data->snap);

    return rc;
}

/* See description in tad_eth_impl.h */
te_errno
tad_eth_match_post_cb(csap_p              csap,
                      unsigned int        layer,
                      tad_recv_pkt_layer *meta_pkt_layer)
{
    tad_eth_proto_data     *proto_data;
    tad_eth_proto_pdu_data *pkt_data = meta_pkt_layer->opaque;
    tad_pkt                *pkt;
    asn_value              *frame_type = NULL;
    te_errno                rc;
    unsigned int            bitoff = 0;

    if (~csap->state & CSAP_STATE_RESULTS)
        return 0;

    if ((meta_pkt_layer->nds = asn_init_value(ndn_eth_header)) == NULL)
    {
        ERROR_ASN_INIT_VALUE(ndn_eth_header);
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);
    }

    proto_data = csap_get_proto_spec_data(csap, layer);
    pkt =  tad_pkts_first_pkt(&meta_pkt_layer->pkts);

    rc = tad_bps_pkt_frag_match_post(&proto_data->eth, &pkt_data->eth,
                                     pkt, &bitoff, meta_pkt_layer->nds);
    if (rc != 0)
        return rc;

    if (pkt_data->type == TAD_ETH_FRAME_TYPE_UNTAGGED)
    {
#if 0
        if (asn_retrieve_descendant(meta_pkt_layer->nds, &rc,
                                    "frame-type.#untagged") == NULL)
        {
            ERROR(CSAP_LOG_FMT "Failed to retrieve "
                  "frame-type.#untagged: %r", CSAP_LOG_ARGS(csap), rc);
            return TE_RC(TE_TAD_CSAP, rc);
        }
#endif
        /* Nothing to add here */
    }
    else if (pkt_data->type == TAD_ETH_FRAME_TYPE_TAGGED)
    {
#if 0
        frame_type = asn_retrieve_descendant(meta_pkt_layer->nds, &rc,
                                             "frame-type.#tagged");
        if (frame_type == NULL)
        {
            ERROR(CSAP_LOG_FMT "Failed to retrieve "
                  "frame-type.#tagged: %r", CSAP_LOG_ARGS(csap), rc);
            return TE_RC(TE_TAD_CSAP, rc);
        }
#else
        {
            asn_value *frame_type_gen;

            frame_type_gen = asn_init_value(ndn_eth_frame_type);
            asn_put_child_value(meta_pkt_layer->nds, frame_type_gen,
                                PRIVATE, NDN_TAG_ETH_FRAME_TYPE);
            frame_type = asn_init_value(ndn_eth_tagged);
            asn_put_child_value(frame_type_gen, frame_type,
                                PRIVATE, NDN_TAG_ETH_TAGGED);
        }
#endif
        /* Skip TPID */
        bitoff += tad_bps_pkt_frag_data_bitlen(&proto_data->tpid,
                                               &pkt_data->tpid);
        /* Fill in TCI */
        rc = tad_bps_pkt_frag_match_post(&proto_data->tci, &pkt_data->tci,
                                         pkt, &bitoff, frame_type);
        if (rc != 0)
            return rc;
    }

    rc = tad_bps_pkt_frag_match_post(&proto_data->len_type,
                                     &pkt_data->len_type,
                                     pkt, &bitoff, meta_pkt_layer->nds);
    if (rc != 0)
        return rc;

    if (pkt_data->type == TAD_ETH_FRAME_TYPE_TAGGED &&
        pkt_data->tci.dus[1].val_i32 == 1)
    {
        assert(pkt_data->tci.dus[1].du_type == TAD_DU_I32);

        rc = tad_bps_pkt_frag_match_post(&proto_data->e_rif,
                                         &pkt_data->e_rif,
                                         pkt, &bitoff, frame_type);
        if (rc != 0)
            return rc;
    }

    if (pkt_data->type == TAD_ETH_FRAME_TYPE_SNAP)
    {
#if 0
        frame_type = asn_retrieve_descendant(meta_pkt_layer->nds, &rc,
                                             "frame-type.#snap");
        if (frame_type == NULL)
        {
            ERROR(CSAP_LOG_FMT "Failed to retrieve "
                  "frame-type.#snap: %r", CSAP_LOG_ARGS(csap), rc);
            return TE_RC(TE_TAD_CSAP, rc);
        }
#else
        {
            asn_value *frame_type_gen;

            frame_type_gen = asn_init_value(ndn_eth_frame_type);
            asn_put_child_value(meta_pkt_layer->nds, frame_type_gen,
                                PRIVATE, NDN_TAG_ETH_FRAME_TYPE);
            frame_type = asn_init_value(ndn_eth_snap);
            asn_put_child_value(frame_type_gen, frame_type,
                                PRIVATE, NDN_TAG_ETH_SNAP);
        }
#endif

        rc = tad_bps_pkt_frag_match_post(&proto_data->len_type,
                                         &pkt_data->len_type,
                                         pkt, &bitoff, frame_type);
        if (rc != 0)
            return rc;
    }

    return 0;
}

/* See description in tad_eth_impl.h */
te_errno
tad_eth_match_do_cb(csap_p           csap,
                    unsigned int     layer,
                    const asn_value *ptrn_pdu,
                    void            *ptrn_opaque,
                    tad_recv_pkt    *meta_pkt,
                    tad_pkt         *pdu,
                    tad_pkt         *sdu)
{
    tad_eth_proto_data     *proto_data;
    tad_eth_proto_pdu_data *ptrn_data = ptrn_opaque;
    tad_eth_proto_pdu_data *pkt_data = meta_pkt->layers[layer].opaque;
    te_errno                rc;
    unsigned int            bitoff = 0;

    UNUSED(ptrn_pdu);

    if (tad_pkt_len(pdu) < ETHER_HDR_LEN)
    {
        F_VERB(CSAP_LOG_FMT "PDU is too small to be Ethernet frame",
               CSAP_LOG_ARGS(csap));
        return TE_RC(TE_TAD_CSAP, TE_ETADNOTMATCH);
    }
  
    proto_data = csap_get_proto_spec_data(csap, layer);

    assert(proto_data != NULL);
    assert(ptrn_data != NULL);
    assert(pkt_data != NULL);

    pkt_data->type = TAD_ETH_FRAME_TYPE_UNTAGGED;

    rc = tad_bps_pkt_frag_match_do(&proto_data->eth, &ptrn_data->eth,
                                   &pkt_data->eth, pdu, &bitoff);
    if (rc != 0)
    {
        F_VERB(CSAP_LOG_FMT "Match PDU vs Ethernet header failed on bit "
               "offset %u: %r", CSAP_LOG_ARGS(csap), (unsigned)bitoff, rc);
        return rc;
    }

    if (ptrn_data->type == TAD_ETH_FRAME_TYPE_TAGGED ||
        ptrn_data->type == TAD_ETH_FRAME_TYPE_ANY)
    {
        rc = tad_bps_pkt_frag_match_do(&proto_data->tpid, &ptrn_data->tpid,
                                       &pkt_data->tpid, pdu, &bitoff);
        if (rc != 0)
        {
            /* Frame is not tagged */
            F_VERB(CSAP_LOG_FMT "Match PDU vs TPID failed on bit offset "
                   "%u: %r", CSAP_LOG_ARGS(csap), (unsigned)bitoff, rc);
            if (ptrn_data->type == TAD_ETH_FRAME_TYPE_TAGGED)
            {
                /* Tagged frames are required only */
                return rc;
            }
        }
        else
        {
            /* Frame is tagged, match TCI */
            pkt_data->type = TAD_ETH_FRAME_TYPE_TAGGED;

            rc = tad_bps_pkt_frag_match_do(&proto_data->tci,
                                           &ptrn_data->tci,
                                           &pkt_data->tci,
                                           pdu, &bitoff);
            if (rc != 0)
            {
                F_VERB(CSAP_LOG_FMT "Match PDU vs TCI failed on bit "
                       "offset %u: %r", CSAP_LOG_ARGS(csap),
                       (unsigned)bitoff, rc);
                return rc;
            }
        }
    }

    rc = tad_bps_pkt_frag_match_do(&proto_data->len_type,
                                   &ptrn_data->len_type,
                                   &pkt_data->len_type, pdu, &bitoff);
    if (rc != 0)
    {
        F_VERB(CSAP_LOG_FMT "Match PDU vs Length/Type failed on bit "
               "offset %u: %r", CSAP_LOG_ARGS(csap), (unsigned)bitoff,
               rc);
        return rc;
    }

    if (ptrn_data->type == TAD_ETH_FRAME_TYPE_TAGGED &&
        ptrn_data->tci.dus[1].du_type == TAD_DU_I32 &&
        ptrn_data->tci.dus[1].val_i32 == 1)
    {
        /* CFI bit is set, match E-RIF */
        rc = tad_bps_pkt_frag_match_do(&proto_data->e_rif,
                                       &ptrn_data->e_rif,
                                       &pkt_data->e_rif,
                                       pdu, &bitoff);
        if (rc != 0)
        {
            F_VERB(CSAP_LOG_FMT "Match PDU vs E-RIF failed on bit "
                   "offset %u: %r", CSAP_LOG_ARGS(csap),
                   (unsigned)bitoff, rc);
            return rc;
        }
    }

    if (ptrn_data->type == TAD_ETH_FRAME_TYPE_SNAP)
    {
        rc = tad_bps_pkt_frag_match_do(&proto_data->snap, &ptrn_data->snap,
                                       &pkt_data->snap, pdu, &bitoff);
        if (rc != 0)
        {
            F_VERB(CSAP_LOG_FMT "Match PDU vs SNAP failed on bit "
                   "offset %u: %r", CSAP_LOG_ARGS(csap),
                   (unsigned)bitoff, rc);
            return rc;
        }
        if (pkt_data->type == TAD_ETH_FRAME_TYPE_TAGGED)
        {
            ERROR(CSAP_LOG_FMT "Tagged SNAP frames are not supported yet",
                  CSAP_LOG_ARGS(csap));
            return TE_RC(TE_TAD_CSAP, TE_EOPNOTSUPP);
        }
        pkt_data->type = TAD_ETH_FRAME_TYPE_SNAP;
    }

    rc = tad_pkt_get_frag(sdu, pdu, bitoff >> 3,
                          tad_pkt_len(pdu) - (bitoff >> 3),
                          TAD_PKT_GET_FRAG_ERROR);
    if (rc != 0)
    {
        ERROR(CSAP_LOG_FMT "Failed to prepare Ethernet SDU: %r",
              CSAP_LOG_ARGS(csap), rc);
        return rc;
    }

    EXIT(CSAP_LOG_FMT "OK", CSAP_LOG_ARGS(csap));

    return 0;
}
