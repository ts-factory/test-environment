/** @file
 * @brief ASN.1 library
 *
 * Declarations of context-specific NDN ASN.1 types
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * @author Konstantin Abramenko <konst@oktetlabs.ru>
 *
 * $Id$
 */ 

#include "asn_impl.h"
#include "ndn.h"

#ifdef WITH_FILE
#include "ndn_file.h"
#endif
#ifdef WITH_SNMP
#include "ndn_snmp.h"
#endif
#ifdef WITH_ETH
#include "ndn_eth.h"
#endif 

#ifdef WITH_IPSTACK
#include "ndn_ipstack.h"
#endif 

#ifdef WITH_CLI
#include "ndn_cli.h"
#endif

#ifdef WITH_BRIDGE
#include "ndn_bridge.h"
#endif

/* Add here declaraions of protocol-specific CSAP init params */ 
#ifdef WITH_FILE
extern asn_type ndn_file_csap_s;
#endif
#ifdef WITH_SNMP
extern asn_type ndn_snmp_csap_s;
#endif
#ifdef WITH_ETH
extern asn_type ndn_eth_csap_s;
#endif 

#ifdef WITH_IPSTACK
extern asn_type ndn_ip4_csap_s;
extern asn_type ndn_icmp4_csap_s;
extern asn_type ndn_udp_csap_s;
#endif 

#ifdef WITH_CLI
extern asn_type ndn_cli_csap_s;
#endif
#ifdef WITH_DHCP
extern asn_type ndn_dhcpv4_csap_s;
#endif

#ifdef WITH_BRIDGE
extern asn_type ndn_bridge_csap_s;
#endif 

static asn_named_entry_t _ndn_generic_csap_level_ne_array [] = 
{
/* Add here reference to protocol-specific CSAP init params */ 
#ifdef WITH_FILE
    { "file", &ndn_file_csap_s },
#endif
#ifdef WITH_SNMP
    { "snmp", &ndn_snmp_csap_s },
#endif
#ifdef WITH_ETH
    { "eth", &ndn_eth_csap_s },
#endif 

#ifdef WITH_IPSTACK
    { "ip4", &ndn_ip4_csap_s },
    { "icmp4", &ndn_icmp4_csap_s },
    { "udp", &ndn_udp_csap_s },
#endif 


#ifdef WITH_CLI
    { "cli", &ndn_cli_csap_s },
#endif
#ifdef WITH_DHCP
    { "dhcp", &ndn_dhcpv4_csap_s },
#endif

#ifdef WITH_BRIDGE
    { "bridge", &ndn_bridge_csap_s },
#endif
};

asn_type ndn_generic_csap_level_s =
{
    "Generic-CSAP-Level", {APPLICATION, 1}, CHOICE, 
    sizeof(_ndn_generic_csap_level_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_generic_csap_level_ne_array}
};

const asn_type * const ndn_generic_csap_level = &ndn_generic_csap_level_s;

/* Add here declaraions of protocol-specific PDU ASN type */ 
#ifdef WITH_FILE
extern asn_type ndn_file_message_s;
#endif
#ifdef WITH_SNMP
extern asn_type ndn_snmp_message_s;
#endif
#ifdef WITH_ETH
extern asn_type ndn_eth_header_s;
#endif 

#ifdef WITH_IPSTACK
extern asn_type ndn_ip4_header_s;
extern asn_type ndn_icmp4_message_s;
extern asn_type ndn_udp_header_s;
#endif 

#ifdef WITH_CLI
extern asn_type ndn_cli_message_s;
#endif
#ifdef WITH_DHCP
extern asn_type ndn_dhcpv4_message_s;
#endif

#ifdef WITH_BRIDGE
extern asn_type ndn_bridge_pdu_s;
#endif


static asn_named_entry_t _ndn_generic_pdu_ne_array [] = 
{
/* Add here reference of protocol-specific CSAP init params */ 
#ifdef WITH_FILE
    { "file", &ndn_file_message_s },
#endif
#ifdef WITH_SNMP
    { "snmp", &ndn_snmp_message_s },
#endif
#ifdef WITH_ETH
    { "eth", &ndn_eth_header_s },
#endif 

#ifdef WITH_IPSTACK
    { "ip4", &ndn_ip4_header_s },
    { "icmp4", &ndn_icmp4_message_s },
    { "udp", &ndn_udp_header_s },
#endif 

#ifdef WITH_CLI
    { "cli", &ndn_cli_message_s },
#endif
#ifdef WITH_DHCP
    { "dhcp", &ndn_dhcpv4_message_s },
#endif
#ifdef WITH_BRIDGE
    { "bridge", &ndn_bridge_pdu_s },
#endif
};

asn_type ndn_generic_pdu_s =
{
    "Generic-PDU", {APPLICATION, 2}, CHOICE, 
    sizeof(_ndn_generic_pdu_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_generic_pdu_ne_array}
};

const asn_type * const ndn_generic_pdu = &ndn_generic_pdu_s;


