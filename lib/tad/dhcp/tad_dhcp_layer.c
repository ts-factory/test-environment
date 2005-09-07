/** @file
 * @brief Test Environment: 
 *
 * Traffic Application Domain Command Handler
 * Ethernet CSAP layer-related callbacks.
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
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
 * Author: Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * @(#) $Id$
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "tad_dhcp_impl.h"

#define TE_LGR_USER     "TAD DHCP layer"

#include "logger_api.h"
#include "logger_ta.h"


/**
 * The first four octets of the 'options' field of the DHCP message
 * RFC 2131 section 3.
 */
static unsigned char magic_dhcp[] = { 99, 130, 83, 99 };

/**
 * Callback for read parameter value of ethernet CSAP.
 *
 * @param csap_descr    CSAP descriptor structure.
 * @param level         Index of level in CSAP stack, which param is wanted.
 * @param param         Protocol-specific name of parameter.
 *
 * @return 
 *     String with textual presentation of parameter value, or NULL 
 *     if error occured. User have to free memory at returned pointer.
 */ 
char* dhcp_get_param_cb (csap_p csap_descr, int level, const char *param)
{
    dhcp_csap_specific_data_t *   spec_data; 
    spec_data = (dhcp_csap_specific_data_t *)
        csap_descr->layers[level].specific_data; 

    if (strcmp (param, "ipaddr") == 0)
    { 
        return spec_data->ipaddr;
    }
    return NULL;
}

/**
 * Callback for confirm PDU with DHCP CSAP parameters and possibilities.
 *
 * @param csap_id       identifier of CSAP
 * @param layer         numeric index of layer in CSAP type to be processed.
 * @param tmpl_pdu      asn_value with PDU (IN/OUT)
 *
 * @return zero on success or error code.
 */ 
int 
dhcp_confirm_pdu_cb (int csap_id, int layer, asn_value *tmpl_pdu)
{ 
    int rc;
    int xid;
    size_t len = sizeof (xid);

    rc = asn_read_value_field(tmpl_pdu, &xid, &len, "xid.#plain");
    if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)
    {
        xid = random();
        rc = asn_write_int32(tmpl_pdu, xid, "xid.#plain");
        if (rc) 
            return rc;
    }
    
    UNUSED (csap_id);
    UNUSED (layer);
    UNUSED (tmpl_pdu);
    return 0;
}


/**
 * Calculate amount of data necessary for all options in DHCP message.
 *
 * @param options       asn_value with sequence of DHCPv4-Option
 *
 * @return number of octets or -1 if error occured.
 */
int
dhcp_calculate_options_data(asn_value_p options)
{
    asn_value_p sub_opts;
    int n_opts = asn_get_length(options, "");
    int i;
    int data_len = 0;
    char label_buf [10];

    for (i = 0; i < n_opts; i++)
    { 
        data_len += 2;  /* octets for type and len */
        snprintf (label_buf, sizeof(label_buf), "%d.options", i);
        if (asn_read_component_value(options, &sub_opts, label_buf) == 0)
        {
            data_len += dhcp_calculate_options_data(sub_opts);
            asn_free_value(sub_opts);
        }
        else
        {
            snprintf (label_buf, sizeof(label_buf), "%d.value", i);
            data_len += asn_get_length(options, label_buf);
        }
    } 
    return data_len;
}

static int
fill_dhcp_options(void *buf, asn_value_p options)
{
    asn_value_p opt;
    int         i;
    size_t      len;
    int         n_opts;
    int         rc = 0;
    uint8_t     opt_type;

    if (options == NULL)
        return 0;

    n_opts = asn_get_length(options, "");
    for (i = 0; i < n_opts && rc == 0; i++)
    { 
        opt = asn_read_indexed(options, i, "");
        len = 1;
        if ((rc = asn_read_value_field(opt, &opt_type, &len, "type.#plain")) != 0)
            return rc;
        memcpy(buf, &opt_type, len);
        buf += len;
        /* Options 255 and 0 don't have length and value parts */
        if (opt_type == 255 || opt_type == 0)
            continue;

        len = 1;
        if ((rc = asn_read_value_field(opt, buf, &len, "length.#plain")) != 0)
            return rc;
        buf += len;
        if (asn_get_length(opt, "options") > 0)
        {
            asn_value_p sub_opts;

            if ((rc = asn_read_component_value(opt, &sub_opts, "options")) != 0)
                return rc;
            rc = fill_dhcp_options(buf, sub_opts);
        }
        else
        {
            len = asn_get_length(opt, "value.#plain");
            if ((rc = asn_read_value_field(opt, buf, &len, "value.#plain")) != 0)
                return rc;
            buf += len;
        }
    }
    return rc;
}


#define OPTIONS_IMPL 1
/**
 * Callback for generate binary data to be sent to media.
 *
 * @param csap_descr    CSAP instance
 * @param layer         numeric index of layer in CSAP type to be processed.
 * @param tmpl_pdu      asn_value with PDU. 
 * @param up_payload    pointer to data which is already generated for upper 
 *                      layers and is payload for this protocol level. 
 *                      May be zero.  Presented as list of packets. 
 *                      Almost always this list will contain only one element, 
 *                      but need in fragmentation sometimes may occur. 
 *                      Of cause, on up level only one PDU is passed, 
 *                      but upper layer (if any present) may perform 
 *                      fragmentation, and current layer may have possibility 
 *                      to de-fragment payload.
 * @param pkts          Callback have to fill this structure with list of 
 *                      generated packets. Almost always this list will 
 *                      contain only one element, but necessaty of 
 *                      fragmentation sometimes may occur. (OUT)
 *
 * @return zero on success or error code.
 */ 
int 
dhcp_gen_bin_cb(csap_p csap_descr, int layer, const asn_value *tmpl_pdu,
                const tad_tmpl_arg_t *args, size_t arg_num,
                const csap_pkts_p up_payload, csap_pkts_p pkts)
{
    int         rc;
    uint8_t    *p; 
#if OPTIONS_IMPL
    asn_value_p options = NULL;
#endif

    UNUSED(up_payload); /* DHCP has no payload */ 
    UNUSED(csap_descr); 
    UNUSED(args); 
    UNUSED(arg_num); 
    UNUSED(layer); 
    
    /* Total length of mandatory fields in DHCP message */
    pkts->len = 236;

#if OPTIONS_IMPL
    rc = asn_read_component_value(tmpl_pdu, &options, "options"); 
    pkts->len += (rc != 0) ? 0 :
        (sizeof(magic_dhcp) + dhcp_calculate_options_data(options));
#endif

    pkts->data = calloc(1, pkts->len);
    if (pkts->data == NULL)
    {
        ERROR("%s(): calloc(1, %u) failed", __FUNCTION__, pkts->len);
        return errno;
    }
    pkts->next = NULL;
    
    p = pkts->data;

#define PUT_DHCP_FIELD(_label, _defval, _type, _conv) \
    do {                                                               \
        _type  value;                                                  \
        size_t val_len = sizeof(value);                                \
                                                                       \
        rc = asn_read_value_field(tmpl_pdu, &value, &val_len, _label); \
        if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)                  \
        {                                                              \
            value = _conv(_defval);                                    \
        }                                                              \
        else if (rc != 0)                                              \
        {                                                              \
            free(pkts->data);                                          \
            pkts->data = NULL;                                         \
            pkts->len = 0;                                             \
            return rc;                                                 \
        }                                                              \
        *((_type *)p) = _conv(value);                                  \
        p += sizeof(_type);                                            \
    } while (0)

    PUT_DHCP_FIELD("op",     0, uint8_t, (uint8_t));
    PUT_DHCP_FIELD("htype",  0, uint8_t, (uint8_t));
    PUT_DHCP_FIELD("hlen",   0, uint8_t, (uint8_t));
    PUT_DHCP_FIELD("hops",   0, uint8_t, (uint8_t));
    PUT_DHCP_FIELD("xid",    0, uint32_t, htonl);
    PUT_DHCP_FIELD("secs",   0, uint16_t, htons);
    PUT_DHCP_FIELD("flags",  0, uint16_t, htons);
    PUT_DHCP_FIELD("ciaddr", 0, uint32_t, (uint32_t));
    PUT_DHCP_FIELD("yiaddr", 0, uint32_t, (uint32_t));
    PUT_DHCP_FIELD("siaddr", 0, uint32_t, (uint32_t));
    PUT_DHCP_FIELD("giaddr", 0, uint32_t, (uint32_t));

#define PUT_DHCP_LONG_FIELD(_label, _defval, _length) \
    do {                                                            \
        size_t val_len = (_length);                                 \
                                                                    \
        rc = asn_read_value_field(tmpl_pdu, p, &val_len, _label);   \
        if (TE_RC_GET_ERROR(rc) == TE_EASNINCOMPLVAL)               \
        {                                                           \
            memset(p, (_defval), (_length));                        \
        }                                                           \
        else if (rc != 0)                                           \
        {                                                           \
            free(pkts->data);                                       \
            pkts->data = NULL;                                      \
            pkts->len = 0;                                          \
            return rc;                                              \
        }                                                           \
        p += (_length);                                             \
    } while (0) 

    PUT_DHCP_LONG_FIELD("chaddr", 0,  16);
    PUT_DHCP_LONG_FIELD("sname",  0,  64);
    PUT_DHCP_LONG_FIELD("file",   0, 128); 

    if (options != NULL)
    {
        memcpy(p, magic_dhcp, sizeof(magic_dhcp));
        p += sizeof(magic_dhcp);

        if ((rc = fill_dhcp_options(p, options)) != 0)
        {
            free(pkts->data);
            pkts->data = NULL;
            pkts->len = 0;
            return rc;
        }
    }
    return 0;
}


/**
 * Callback for parse received packet and match it with pattern. 
 *
 * @param csap_id       identifier of CSAP
 * @param layer         numeric index of layer in CSAP type to be processed.
 * @param pattern_pdu   pattern NDS 
 * @param pkt           recevied packet
 * @param payload       rest upper layer payload, if any exists. (OUT)
 * @param parsed_packet caller of mdhcpod should pass here empty asn_value 
 *                      instance of ASN type 'Generic-PDU'. Callback 
 *                      have to fill this instance with values from 
 *                      parsed and matched packet
 *
 * @return zero on success or error code.
 */
int 
dhcp_match_bin_cb(int csap_id, int layer, const asn_value *pattern_pdu,
                  const csap_pkts *pkt, csap_pkts *payload, 
                  asn_value *parsed_packet )
{ 
    uint8_t    *data = pkt->data; 
    asn_value  *opt_list;
    asn_value  *dhcp_message_pdu = NULL;
    int         rc;

    UNUSED(payload); 

    if (pattern_pdu == NULL || pkt == NULL)
        return TE_EWRONGPTR;

    ENTRY("%s: CSAP %d, layer %d, pkt len: %d", 
          __FUNCTION__, csap_id, layer, pkt->len);

    VERB("DHCP match callback called: %tm", data, pkt->len);

    if (parsed_packet)
        dhcp_message_pdu = asn_init_value(ndn_dhcpv4_message);

#define FILL_DHCP_HEADER_FIELD(_asn_label, _size) \
    do {                                                        \
        rc = ndn_match_data_units(pattern_pdu, dhcp_message_pdu,\
                                  data, _size, _asn_label);     \
        if (rc)                                                 \
        {                                                       \
            F_VERB("%s: field %s not match, rc %r",             \
                    __FUNCTION__, _asn_label, rc);              \
            return rc;                                          \
        }                                                       \
        data += _size;                                          \
    } while(0)

    FILL_DHCP_HEADER_FIELD("op",      1);
    FILL_DHCP_HEADER_FIELD("htype",   1);
    FILL_DHCP_HEADER_FIELD("hlen",    1);
    FILL_DHCP_HEADER_FIELD("hops",    1);
    FILL_DHCP_HEADER_FIELD("xid",     4);
    FILL_DHCP_HEADER_FIELD("secs",    2);
    FILL_DHCP_HEADER_FIELD("flags",   2);
    FILL_DHCP_HEADER_FIELD("ciaddr",  4);
    FILL_DHCP_HEADER_FIELD("yiaddr",  4);
    FILL_DHCP_HEADER_FIELD("siaddr",  4);
    FILL_DHCP_HEADER_FIELD("giaddr",  4); 
    FILL_DHCP_HEADER_FIELD("chaddr", 16);
    FILL_DHCP_HEADER_FIELD("sname",  64);
    FILL_DHCP_HEADER_FIELD("file",  128); 

#undef FILL_DHCP_HEADER_FIELD

    /* check for magic DHCP cookie, see RFC2131, section 3. */
    if ((((void *)data) + sizeof(magic_dhcp)) > (pkt->data + pkt->len) || 
        memcmp(magic_dhcp, data, sizeof(magic_dhcp)) != 0)
    {
        VERB("DHCP magic does not match: "
             "it is pure BOOTP message without options");
    }
    else
    { 
        data += sizeof(magic_dhcp); 

        opt_list = asn_init_value(ndn_dhcpv4_options); 

        while (((void *)data) < (pkt->data + pkt->len))
        {
            asn_value_p opt = asn_init_value(ndn_dhcpv4_option);
            uint8_t     opt_len;
            uint8_t     opt_type;
        
#define FILL_DHCP_OPT_FIELD(_obj, _label, _size) \
            do {                                                \
                rc = asn_write_value_field(_obj, data, _size,   \
                                           _label ".#plain");   \
                data += _size;                                  \
            } while(0);

            opt_type = *data;
            FILL_DHCP_OPT_FIELD(opt, "type",  1);
            if (opt_type == 255 || opt_type == 0)
            {
                /* END and PAD options don't have length and value */
                rc = asn_insert_indexed(opt_list, opt, -1, "");
                asn_free_value(opt);
                continue;
            }

            opt_len = *data;
            FILL_DHCP_OPT_FIELD(opt, "length", 1);
            FILL_DHCP_OPT_FIELD(opt, "value", opt_len);

            /* possible suboptions.  */
            switch (opt_type)
            {
                case 43:
                {
                    asn_value_p  sub_opt_list;
                    uint8_t      sub_opt_len;
                    asn_value_p  sub_opt;
                    void        *start_opt_value;

                    /* Set pointer to the beginning of the Option data */
                    data -= opt_len;
                    start_opt_value = data;
                    sub_opt_list = asn_init_value(ndn_dhcpv4_options);
                    while (((void *)data) < (start_opt_value + opt_len))
                    {
                        sub_opt = asn_init_value(ndn_dhcpv4_option);

                        
                        FILL_DHCP_OPT_FIELD(sub_opt, "type",  1);
                        sub_opt_len = *data;
                        FILL_DHCP_OPT_FIELD(sub_opt, "length", 1);
                        FILL_DHCP_OPT_FIELD(sub_opt, "value", sub_opt_len);

                        asn_insert_indexed(sub_opt_list, sub_opt, -1, "");
                        asn_free_value(sub_opt);
                    }
                    rc = asn_write_component_value(opt, sub_opt_list,
                                                   "options");
                    break;
                }
            }
            rc = asn_insert_indexed(opt_list, opt, -1, "");
            asn_free_value(opt);
        }
        asn_write_component_value(parsed_packet, opt_list, "#dhcp.options");
        asn_free_value(opt_list);
    }

    if (parsed_packet)
    {
        rc = asn_write_component_value(parsed_packet, dhcp_message_pdu,
                                       "#dhcp"); 
        if (rc)
            ERROR("%s, write DHCP message to packet fails %r", 
                  __FUNCTION__, rc);
    } 

    asn_free_value(dhcp_message_pdu); 

    memset(payload, 0 , sizeof(*payload));

    VERB("MATCH CALLBACK OK\n");

    return 0;
}

/**
 * Callback for generating pattern to filter 
 * just one response to the packet which will be sent by this CSAP 
 * according to this template. 
 *
 * @param csap_id       identifier of CSAP
 * @param layer         numeric index of layer in CSAP type to be processed.
 * @param tmpl_pdu      ASN value with template PDU.
 * @param pattern_pdu   OUT: ASN value with pattern PDU, generated according 
 *                      to passed template PDU and CSAP parameters. 
 *
 * @return zero on success or error code.
 */
int 
dhcp_gen_pattern_cb(int csap_id, int layer, const asn_value *tmpl_pdu, 
                    asn_value **pattern_pdu)
{
    int rc;
    int xid;
    size_t len = sizeof (xid);

    *pattern_pdu = asn_init_value(ndn_dhcpv4_message); 
    rc = asn_read_value_field(tmpl_pdu, &xid, &len, "xid.#plain");
    if (rc == 0)
    {
        rc = asn_write_int32(*pattern_pdu, xid, "xid.#plain");
    }
    /* TODO: DHCP options to be inserted into pattern */
    UNUSED(csap_id);
    UNUSED(layer);
    UNUSED(tmpl_pdu);


    return rc;
}


