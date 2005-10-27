/** @file
 * @brief PCAP TAD
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
 * @author Konstantin Abramenko <konst@oktetlabs.ru>
 * @author Alexander Kukuta <Alexander.Kukuta@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "TAD Ethernet-PCAP"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "te_defs.h"
#include "te_printf.h"

#include "logger_api.h"
#include "logger_ta_fast.h"

#include "tad_pcap_impl.h"


/* See description in tad_pcap_impl.h */
char *
tad_pcap_get_param_cb(csap_p csap_descr, unsigned int layer,
                      const char *param)
{
    char   *param_buf;            
    pcap_csap_specific_data_p spec_data;

    VERB("%s() started", __FUNCTION__);

    if (csap_descr == NULL)
    {
        VERB("error in pcap_get_param %s: wrong csap descr passed\n", param);
        return NULL;
    }

    spec_data =
        (pcap_csap_specific_data_p) csap_descr->layers[layer].specific_data; 

    if (strcmp (param, "total_bytes") == 0)
    {
        param_buf  = malloc(20);
        sprintf(param_buf, "%" TE_PRINTF_SIZE_T "u",
                spec_data->total_bytes);
        return param_buf;
    }
    else if (strcmp (param, "total_packets") == 0)
    {
        param_buf  = malloc(20);
        sprintf(param_buf, "%" TE_PRINTF_SIZE_T "u",
                spec_data->total_packets);
        return param_buf;
    }
    else if (strcmp (param, "filtered_packets") == 0)
    {
        param_buf  = malloc(20);
        sprintf(param_buf, "%" TE_PRINTF_SIZE_T "u",
                spec_data->filtered_packets);
        return param_buf;
    }

    VERB("error in pcap_get_param %s: not supported parameter\n", param);

    return NULL;
}


/* See description in tad_pcap_impl.h */
te_errno
tad_pcap_confirm_pdu_cb(int csap_id, unsigned int layer,
                        asn_value_p tmpl_pdu)
{
    pcap_csap_specific_data_p   spec_data; 
    csap_p                      csap_descr;

    size_t                      val_len;
    char                       *pcap_str;
    int                         rc; 

    struct bpf_program         *bpf_program;
    
    VERB("%s() started", __FUNCTION__);
    
    if ((csap_descr = csap_find(csap_id)) == NULL)
    {
        ERROR("null csap_descr for csap id %d", csap_id);
        return TE_ETADCSAPNOTEX;
    }
    
    spec_data = (pcap_csap_specific_data_p)
        csap_descr->layers[layer].specific_data; 

    rc = asn_get_length(tmpl_pdu, "filter");
    if (rc < 0)
    {
        ERROR("%s(): asn_get_length() failed, rc=%r", __FUNCTION__, rc);
        return rc;
    }
    
    val_len = rc;

    pcap_str = (char *)malloc(val_len + 1);
    if (pcap_str == NULL)
    {
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);
    }
    
    rc = asn_read_value_field(tmpl_pdu, pcap_str, &val_len, "filter");
    if (rc < 0)
    {
        ERROR("%s(): asn_read_value_field() failed, rc=%r", __FUNCTION__, rc);
        return rc;
    }

    VERB("%s: Try to compile filter string \"%s\"", __FUNCTION__, pcap_str);    

    bpf_program = (struct bpf_program *)malloc(sizeof(struct bpf_program));
    if (bpf_program == NULL)
    {
        return TE_RC(TE_TAD_CSAP, TE_ENOMEM);
    }

    rc = pcap_compile_nopcap(TAD_PCAP_SNAPLEN, spec_data->iftype,
                             bpf_program, pcap_str, TRUE, 0);
    if (rc != 0)
    {
        ERROR("%s(): pcap_compile_nopcap() failed, rc=%d", __FUNCTION__, rc);
        return TE_RC(TE_TAD_CSAP, TE_EINVAL);
    }
    VERB("%s: pcap_compile_nopcap() returns %d", __FUNCTION__, rc);

    spec_data->bpfs[++spec_data->bpf_count] = bpf_program;

    val_len = sizeof(int);
    rc = asn_write_value_field(tmpl_pdu, &spec_data->bpf_count,
                               val_len, "bpf-id");
    if (rc < 0)
    {
        ERROR("%s(): asn_write_value_field() failed, rc=%r",
              __FUNCTION__, rc);
        return rc;
    }

    VERB("%s: filter string compiled, bpf-id %d", __FUNCTION__,
         spec_data->bpf_count);

    VERB("exit, return 0");
    
    return 0;
}


/* See description in tad_pcap_impl.h */
te_errno
tad_pcap_match_bin_cb(int csap_id, unsigned int layer,
                      const asn_value *pattern_pdu,
                      const csap_pkts *pkt, csap_pkts *payload, 
                      asn_value_p parsed_packet)
{
    csap_p                      csap_descr;
    pcap_csap_specific_data_p   spec_data;
    int                         rc;
    uint8_t                    *data;
    int                         bpf_id;
    struct bpf_program         *bpf_program;
    struct bpf_insn            *bpf_code;
    size_t                      tmp_len;

    VERB("%s() started", __FUNCTION__);

    if ((csap_descr = csap_find(csap_id)) == NULL)
    {
        ERROR("null csap_descr for csap id %d", csap_id);
        return TE_ETADCSAPNOTEX;
    }

    spec_data = (pcap_csap_specific_data_p)
        csap_descr->layers[layer].specific_data;
    data = pkt->data; 

    if (pattern_pdu == NULL)
    {
        VERB("pattern pdu is NULL, packet matches");
    }

    tmp_len = sizeof(int);
    rc = asn_read_value_field(pattern_pdu, &bpf_id, &tmp_len, "bpf-id");
    if (rc != 0)
    {
        ERROR("%s(): Cannot read \"bpf-id\" field from PDU pattern",
              __FUNCTION__);
        return rc;
    }

    /* bpf_id == 0 means that filter string is not compiled yet */
    if ((bpf_id <= 0) || (bpf_id > spec_data->bpf_count))
    {
        ERROR("%s(): Invalid bpf_id value in PDU pattern", __FUNCTION__);
        return TE_RC(TE_TAD_CSAP, TE_EINVAL);
    }

    bpf_program = spec_data->bpfs[bpf_id];
    if (bpf_program == NULL)
    {
        ERROR("%s(): Invalid bpf_id value in PDU pattern", __FUNCTION__);
        return TE_RC(TE_TAD_CSAP, TE_EINVAL);
    }
    
    bpf_code = bpf_program->bf_insns;
    
    rc = bpf_filter(bpf_code, pkt->data, pkt->len, pkt->len);
    VERB("bpf_filter() returns 0x%x (%d)", rc, rc);
    if (rc <= 0)
    {
        return TE_ETADNOTMATCH;
    }

#if 1
    do {
        int filter_id;
        size_t filter_len;
        char *filter = NULL;

        VERB("Packet matches, try to get filter string");

        filter_len = sizeof(int);
        
        if (asn_read_value_field(pattern_pdu, &filter_id, 
                                 &filter_len, "filter-id") < 0)
        {
            ERROR("Cannot get filter-id");
            filter_id = -1;
        }
        
        rc = asn_get_length(pattern_pdu, "filter");
        if (rc < 0)
        {
            ERROR("Cannot get length of filter string");
            break;
        }
        filter_len = rc;
        
        filter = (char *) malloc(filter_len + 1);
        if (filter == NULL)
        {
            ERROR("Cannot allocate memory for filter string");
            break;
        }
        
        if (asn_read_value_field(pattern_pdu, filter,
                                 &filter_len, "filter") < 0)
        {
            ERROR("Cannot get filter string");
            free(filter);
            break;
        }
        
        filter[filter_len] = '\0';
        
        VERB("Received packet matches to filter: \"%s\", filter-id=%d",
             filter, filter_id);
        
        free(filter);
    } while (0);
#endif

    /* Fill parsed packet value */
    if (parsed_packet)
    {
        rc = asn_write_component_value(parsed_packet, pattern_pdu, "#pcap"); 
        if (rc)
            ERROR("write pcap filter to packet rc %r", rc);
    }

    VERB("Try to copy payload of %u bytes", (unsigned)(pkt->len));

    /* passing payload to upper layer */
    memset(payload, 0 , sizeof(*payload));
    payload->len = pkt->len;

    payload->data = malloc(payload->len);
    memcpy(payload->data, pkt->data, payload->len);

    F_VERB("PCAP csap N %d, packet matches, pkt len %ld, pld len %ld", 
           csap_id, pkt->len, payload->len);
    
    return 0;
}
