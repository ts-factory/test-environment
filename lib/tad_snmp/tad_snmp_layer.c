/** @file
 * @brief Test Environment: 
 *
 * Traffic Application Domain Command Handler
 * Dummy FILE protocol implementaion, layer-related callbacks.
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
 * Author: Konstantin Abramenko <konst@oktetlabs.ru>
 *
 * @(#) $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#undef SNMPDEBUG

#define LGR_USER         "TAD SNMP"
#include "logger_ta.h"

#include <string.h>
#include "te_stdint.h"
#include "tad_snmp_impl.h"


/**
 * Callback for read parameter value of "snmp" CSAP.
 *
 * @param csap_id       identifier of CSAP.
 * @param level         Index of level in CSAP stack, which param is wanted.
 * @param param         Protocol-specific name of parameter.
 *
 * @return 
 *     String with textual presentation of parameter value, or NULL 
 *     if error occured. User have to free memory at returned pointer.
 */ 
char* snmp_get_param_cb (int csap_id, int level, const char *param)
{
    UNUSED(csap_id);
    UNUSED(level);
    UNUSED(param);
    return NULL;
}

/**
 * Callback for confirm PDU with 'snmp' CSAP parameters and possibilities.
 *
 * @param csap_id       identifier of CSAP
 * @param layer         numeric index of layer in CSAP type to be processed.
 * @param tmpl_pdu      asn_value with PDU (IN/OUT)
 *
 * @return zero on success or error code.
 */ 
int snmp_confirm_pdu_cb (int csap_id, int layer, asn_value_p tmpl_pdu)
{
    UNUSED(csap_id);
    UNUSED(layer);
    UNUSED(tmpl_pdu);
    VERB("%s, csap %d, layer %d", __FUNCTION__, csap_id, layer);
    return 0;
}

/**
 * Callback for generate binary data to be sent to media.
 *
 * @param csap_id       identifier of CSAP
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
 *                      contain only one element, but need 
 *                      in fragmentation sometimes may occur. (OUT)
 *
 * @return zero on success or error code.
 */ 
int snmp_gen_bin_cb(int csap_id, int layer, const asn_value *tmpl_pdu,
                    const tad_template_arg_t *args, size_t  arg_num, 
                    csap_pkts_p  up_payload, csap_pkts_p pkts)
{
    int rc; 
    int operation;
    int operation_len = sizeof(int);
    int ucd_snmp_op;
    int num_var_bind;
    int i;

    struct snmp_pdu *pdu;

    UNUSED(args);
    UNUSED(arg_num);
    UNUSED(csap_id);
    UNUSED(up_payload); 

    VERB("%s, layer %d", __FUNCTION__, layer);

    memset(pkts, 0, sizeof(*pkts));

    rc = asn_read_value_field(tmpl_pdu, &operation, &operation_len, "type");
    if (rc) return rc;

    VERB("%s, operation %d", __FUNCTION__, operation);
    switch (operation)
    {
        case NDN_SNMP_MSG_GET:     ucd_snmp_op = SNMP_MSG_GET;     break;
        case NDN_SNMP_MSG_GETNEXT: ucd_snmp_op = SNMP_MSG_GETNEXT; break;
        case NDN_SNMP_MSG_GETBULK: ucd_snmp_op = SNMP_MSG_GETBULK; break;
        case NDN_SNMP_MSG_SET:     ucd_snmp_op = SNMP_MSG_SET;     break;
        case NDN_SNMP_MSG_TRAP1:   ucd_snmp_op = SNMP_MSG_TRAP;    break; 
        case NDN_SNMP_MSG_TRAP2:   ucd_snmp_op = SNMP_MSG_TRAP2;   break; 
        default: 
            return ETADWRONGNDS;
    } 
    pdu = snmp_pdu_create(ucd_snmp_op);
    VERB("%s, snmp pdu created 0x%x", __FUNCTION__, pdu);


    if (operation == NDN_SNMP_MSG_GETBULK) 
    {
        int repeats, r_len = sizeof(repeats);
        rc = asn_read_value_field(tmpl_pdu, &repeats, &r_len, "repeats");
        if (rc) 
            pdu->max_repetitions = SNMP_CSAP_DEF_REPEATS;
        else 
            pdu->max_repetitions = repeats;
#ifdef SNMPDEBUG
        printf ("GETBULK on TA, repeats: %d\n", pdu->max_repetitions);
#endif
        pdu->non_repeaters = 0;
    }

    num_var_bind = asn_get_length(tmpl_pdu, "variable-bindings"); 
    for (i = 0; i < num_var_bind; i++)
    {
        asn_value_p var_bind = asn_read_indexed(tmpl_pdu, i, "variable-bindings");
        unsigned long oid [MAX_OID_LEN];
        int           oid_len = MAX_OID_LEN;
        rc = asn_read_value_field(var_bind, oid, &oid_len, "name.#plain");
        if (rc) break;

        switch (operation)
        {
            case NDN_SNMP_MSG_GET:     
            case NDN_SNMP_MSG_GETNEXT:
            case NDN_SNMP_MSG_GETBULK:
                snmp_add_null_var ( pdu, oid, oid_len ); 
                break;

            case NDN_SNMP_MSG_SET:   
            case NDN_SNMP_MSG_TRAP1: 
            case NDN_SNMP_MSG_TRAP2: 
            {
                const char* val_name;
                asn_value_p value;
                uint8_t buffer[1000];
                int d_len = sizeof(buffer);


                rc = asn_read_component_value(var_bind, &value, "value");
                if (rc) break; 

                rc = asn_read_value_field(value, buffer, &d_len, "");
                if (rc) break; 

                val_name = asn_get_name(value);
                if (val_name == NULL)
                { 
                    rc = EASNGENERAL;
                    break;
                } 
                snmp_pdu_add_variable ( pdu, oid, oid_len, 
                                        snmp_asn_syntaxes[asn_get_tag(value)], 
                                        buffer, d_len );
            }
        } 
        if (rc) break;
    }


    if (rc)
        snmp_free_pdu(pdu);
    else
    {
        pkts->next = 0;
        pkts->data = pdu;
        pkts->len  = sizeof(*pdu);
        pkts->free_data_cb = snmp_free_pdu;
    }
    VERB("%s rc %X", __FUNCTION__, rc);

    return rc;
}


/**
 * Callback for parse received packet and match it with pattern. 
 *
 * @param csap_id       identifier of CSAP
 * @param layer         numeric index of layer in CSAP type to be processed.
 * @param pattern_pdu   pattern NDS 
 * @param pkt           recevied packet
 * @param payload       rest upper layer payload, if any exists. (OUT)
 * @param parsed_packet caller of method should pass here empty asn_value 
 *                      instance of ASN type 'Generic-PDU'. Callback 
 *                      have to fill this instance with values from 
 *                      parsed and matched packet
 *
 * @return zero on success or error code.
 */
int snmp_match_bin_cb (int csap_id, int layer, const asn_value_p pattern_pdu,
                       const csap_pkts *  pkt, csap_pkts * payload,
                       asn_value_p  parsed_packet )
{ 
    int type;
    struct snmp_pdu * pdu = (struct snmp_pdu *)pkt->data;
    int rc;
    struct variable_list *vars;
    asn_value_p           vb_seq = asn_init_value(ndn_snmp_var_bind_seq);

    UNUSED(csap_id);
    UNUSED(pattern_pdu);

    memset (payload, 0, sizeof (csap_pkts)); /* never use buffer for upper payload. */

    VERB("%s, layer %d, pdu 0x%x, pdu command: <%d>", 
            __FUNCTION__, layer, pdu, pdu->command);

    switch (pdu->command)
    {
        case SNMP_MSG_GET:     type = NDN_SNMP_MSG_GET;     break;
        case SNMP_MSG_GETNEXT: type = NDN_SNMP_MSG_GETNEXT; break;
        case SNMP_MSG_RESPONSE:type = NDN_SNMP_MSG_RESPONSE;break;
        case SNMP_MSG_SET:     type = NDN_SNMP_MSG_SET;     break;
        case SNMP_MSG_TRAP:    type = NDN_SNMP_MSG_TRAP1;   break; 
        case SNMP_MSG_TRAP2:   type = NDN_SNMP_MSG_TRAP2;   break; 
        case SNMP_MSG_GETBULK: type = NDN_SNMP_MSG_GETBULK; break;
        default: 
            return ETADNOTMATCH;
    } 
 
    rc = asn_write_value_field(parsed_packet, &type, sizeof(type), 
                                "#snmp.type.#plain"); 
    if (rc) return rc;
    asn_write_value_field(parsed_packet, pdu->community,
                          pdu->community_len + 1, "#snmp.community.#plain"); 

    asn_write_value_field(parsed_packet, &pdu->reqid,
                          sizeof(pdu->reqid), "#snmp.request-id.#plain"); 

    asn_write_value_field(parsed_packet, &pdu->errstat,
                          sizeof(pdu->errstat), "#snmp.err-status.#plain"); 

    asn_write_value_field(parsed_packet, &pdu->errindex,
                          sizeof(pdu->errindex), "#snmp.err-index.#plain"); 

    if (pdu->errstat || pdu->errindex)
        RING("in %s, errstat %d, errindex %d",
                __FUNCTION__, pdu->errstat, pdu->errindex);

    if (type == NDN_SNMP_MSG_TRAP1)
    {
        asn_write_value_field(parsed_packet, pdu->enterprise, 
                              pdu->enterprise_length, "enterprise");

        asn_write_value_field(parsed_packet, &pdu->trap_type,
                              sizeof(pdu->trap_type), "gen-trap");

        asn_write_value_field(parsed_packet, &pdu->specific_type,
                              sizeof(pdu->specific_type), "spec-trap");

        asn_write_value_field(parsed_packet, pdu->agent_addr,
                              sizeof(pdu->agent_addr), "agent-addr");
    }

    for (vars = pdu->variables; vars; vars = vars->next_variable)
    {
        asn_value_p var_bind = asn_init_value(ndn_snmp_var_bind);
        char        os_choice[100]; 

        asn_write_value_field (var_bind, vars->name, vars->name_length, 
                                "name.#plain");

#ifdef SNMPDEBUG
        printf (" SNMP MATCH: add variable ");
        print_objid (vars->name, vars->name_length);

        printf (" SNMP MATCH: vars type: %d\n", vars->type);
#endif
        strcpy (os_choice, "value.#plain.");
        switch (vars->type)
        {
            case ASN_INTEGER:   
                strcat (os_choice, "#simple.#integer-value");
                break;
            case ASN_OCTET_STR: 
                strcat (os_choice, "#simple.#string-value");
                break;
            case ASN_OBJECT_ID: 
                strcat (os_choice, "#simple.#objectID-value");
                break;

            case ASN_IPADDRESS: 
                strcat (os_choice, "#application-wide.#ipAddress-value");
                break;
            case ASN_COUNTER:  
                strcat (os_choice, "#application-wide.#counter-value");
                break;
            case ASN_UNSIGNED:  
                strcat (os_choice, "#application-wide.#unsigned-value");
                break;
            case ASN_TIMETICKS: 
                strcat (os_choice, "#application-wide.#timeticks-value");
                break;
#if 0
            case ASN_OCTET_STR: 
                strcat (os_choice, "#application-wide.#arbitrary-value");
                break;
#ifdef OPAQUE_SPECIAL_TYPES 
            case ASN_OPAQUE_U64: 
#else
            case ASN_OCTET_STR:
#endif
                                
                strcat (os_choice, "#application-wide.#big-counter-value");
                break;
            case ASN_UNSIGNED:  
                strcat (os_choice, "#application-wide.#unsigned-value");
                break;
#endif
            case SNMP_NOSUCHOBJECT: 
                strcpy (os_choice, "noSuchObject"); /* overwrite "value..." */
                break;
            case SNMP_NOSUCHINSTANCE: 
                strcpy (os_choice, "noSuchInstance"); /* overwrite "value..." */
                break;
            case SNMP_ENDOFMIBVIEW: 
                strcpy (os_choice, "endOfMibView"); /* overwrite "value..." */
                break;
            default:
                asn_free_value(var_bind);
                return 1;
        }
#ifdef SNMPDEBUG
        printf ("in SNMP MATCH, rc before varbind value write: %x\n", rc);
        printf ("in SNMP MATCH, try to write for label: <%s>\n", os_choice);
#endif
        rc = asn_write_value_field(var_bind, vars->val.string, 
                              vars->val_len, os_choice    );
#ifdef SNMPDEBUG
        printf ("in SNMP MATCH, rc from varbind value write: %x\n", rc);
#endif
        if (rc == 0)
            rc = asn_insert_indexed(vb_seq, var_bind, -1, "");
#ifdef SNMPDEBUG
        printf ("in SNMP MATCH, rc from varbind insert: %x\n", rc);
#endif

        asn_free_value(var_bind);

        if (rc)
            break;
    }

    if (rc == 0)
        rc = asn_write_component_value(parsed_packet, vb_seq, "#snmp.variable-bindings");

#ifdef SNMPDEBUG
    printf ("in SNMP MATCH, rc from vb_seq insert: %x\n", rc);
#endif

    asn_free_value(vb_seq);

    return rc;
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
int snmp_gen_pattern_cb (int csap_id, int layer, const asn_value_p tmpl_pdu, 
                                         asn_value_p   *pattern_pdu)
{ 
    UNUSED(tmpl_pdu);

    *pattern_pdu = asn_init_value (ndn_snmp_message);
    VERB("%s callback, CSAP # %d, layer %d", __FUNCTION__, csap_id, layer); 
    return 0;
}


