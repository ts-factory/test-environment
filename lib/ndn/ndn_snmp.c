/** @file
 * @brief Proteos, TAD file protocol, NDN.
 *
 * Definitions of ASN.1 types for NDN for file protocol. 
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asn_impl.h"
#include "ndn_internal.h"
#include "ndn_snmp.h"

#if HAVE_NET_SNMP_DEFINITIONS_H
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/definitions.h> 
#elif HAVE_UCD_SNMP_SNMP_IMPL_H
#include <sys/types.h>
#include <ucd-snmp/asn1.h>
#include<ucd-snmp/snmp_impl.h>
#else
#error This module cannot be compiled without UCD- or NET-SNMP library 
#endif




asn_type ndn_snmp_obj_syntax_integer_s = 
{ "INTEGER",           
  {APPLICATION, NDN_SNMP_OBJSYN_INT},        
  INTEGER, 1,{NULL}
};

asn_type ndn_snmp_obj_syntax_string_s = 
{ "OCTET STRING",      {APPLICATION, NDN_SNMP_OBJSYN_STR},        OCT_STRING, 0,{NULL}};

asn_type ndn_snmp_obj_syntax_objid_s = 
{ "OBJECT IDENTIFIER", {APPLICATION, NDN_SNMP_OBJSYN_OID},        OID, 0,{NULL}};

asn_type ndn_snmp_obj_syntax_ipaddr_s = 
{ "IpAddress",         {APPLICATION, NDN_SNMP_OBJSYN_IPADDR},     OCT_STRING, 4,{NULL}};

asn_type ndn_snmp_obj_syntax_counter_s = 
{ "Counter32",         {APPLICATION, NDN_SNMP_OBJSYN_COUNTER},    INTEGER, 0,{NULL}};

asn_type ndn_snmp_obj_syntax_timeticks_s = 
{ "TimeTicks",         {APPLICATION, NDN_SNMP_OBJSYN_TIMETICKS},  INTEGER, 0,{NULL}};

asn_type ndn_snmp_obj_syntax_arbitrary_s = 
{ "Opaque",            {APPLICATION, NDN_SNMP_OBJSYN_ARB},        OCT_STRING, 0,{NULL}};

asn_type ndn_snmp_obj_syntax_big_counter_s = 
{ "Counter64",         {APPLICATION, NDN_SNMP_OBJSYN_BIGCOUNTER}, LONG_INT, 64,{NULL}};

asn_type ndn_snmp_obj_syntax_unsigned_s = 
{ "Unsigned32",        {APPLICATION, NDN_SNMP_OBJSYN_UINT},       INTEGER , 0,{NULL}};


const asn_type * const ndn_snmp_obj_syntax_integer    = &ndn_snmp_obj_syntax_integer_s;
const asn_type * const ndn_snmp_obj_syntax_string     = &ndn_snmp_obj_syntax_string_s;
const asn_type * const ndn_snmp_obj_syntax_objid      = &ndn_snmp_obj_syntax_objid_s;
const asn_type * const ndn_snmp_obj_syntax_ipaddr     = &ndn_snmp_obj_syntax_ipaddr_s;
const asn_type * const ndn_snmp_obj_syntax_counter    = &ndn_snmp_obj_syntax_counter_s;
const asn_type * const ndn_snmp_obj_syntax_timeticks  = &ndn_snmp_obj_syntax_timeticks_s;
const asn_type * const ndn_snmp_obj_syntax_arbitrary  = &ndn_snmp_obj_syntax_arbitrary_s;
const asn_type * const ndn_snmp_obj_syntax_big_counter= &ndn_snmp_obj_syntax_big_counter_s;
const asn_type * const ndn_snmp_obj_syntax_unsigned   = &ndn_snmp_obj_syntax_unsigned_s;


asn_enum_entry_t _ndn_snmp_error_status_enum_entries[] = 
{
    {"noError", 0},
    {"tooBig", 1},
    {"noSuchName", 2}, 
    {"badValue", 3},  
    {"readOnly", 4}, 
    {"noAccess", 6},
    {"wrongType", 7},
    {"wrongLength", 8},
    {"wrongEncoding", 9},
    {"wrongValue", 10},
    {"noCreation", 11},
    {"inconsistentValue", 12},
    {"resourceUnavailable", 13},
    {"commitFailed", 14},
    {"undoFailed", 15},
    {"authorizationError", 16},
    {"notWritable", 17},
    {"inconsistentName", 18} 
};

asn_type ndn_snmp_error_status_s = {
    "SnmpErrorStatus",
    {UNIVERSAL, 10},
    ENUMERATED,
    sizeof(_ndn_snmp_error_status_enum_entries)/sizeof(asn_enum_entry_t), 
    {_ndn_snmp_error_status_enum_entries}
};

asn_type * ndn_snmp_error_status = &ndn_snmp_error_status_s;




asn_enum_entry_t _ndn_snmp_message_type_enum_entries[] = 
{
    {"get",     NDN_SNMP_MSG_GET},
    {"get-next",NDN_SNMP_MSG_GETNEXT},
    {"response",NDN_SNMP_MSG_RESPONSE},
    {"set",     NDN_SNMP_MSG_SET},
    {"trap1",   NDN_SNMP_MSG_TRAP1},
    {"trap2",   NDN_SNMP_MSG_TRAP2},
    {"get-bulk",NDN_SNMP_MSG_GETBULK}
};

asn_type ndn_snmp_message_type_s = {
    "SnmpErrorStatus",
    {UNIVERSAL, 10},
    ENUMERATED,
    sizeof(_ndn_snmp_message_type_enum_entries)/sizeof(asn_enum_entry_t), 
    {_ndn_snmp_message_type_enum_entries}
};

const asn_type * const ndn_snmp_message_type = &ndn_snmp_message_type_s;




static asn_named_entry_t _ndn_snmp_simple_ne_array [] = 
{
    { "integer-value",   &ndn_snmp_obj_syntax_integer_s },
    { "string-value",    &ndn_snmp_obj_syntax_string_s },
    { "objectID-value",  &ndn_snmp_obj_syntax_objid_s }
};

asn_type ndn_snmp_simple_s =
{
    "SimpleSyntax", {APPLICATION, 1}, CHOICE, 
    sizeof(_ndn_snmp_simple_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_snmp_simple_ne_array}
}; 

const asn_type * const ndn_snmp_simple = &ndn_snmp_simple_s;



static asn_named_entry_t _ndn_snmp_appl_ne_array [] = 
{
    { "ipAddress-value",  &ndn_snmp_obj_syntax_ipaddr_s },
    { "counter-value",    &ndn_snmp_obj_syntax_counter_s },
    { "timeticks-value",  &ndn_snmp_obj_syntax_timeticks_s },
    { "arbitrary-value",  &ndn_snmp_obj_syntax_arbitrary_s },
    { "big-counter-value",&ndn_snmp_obj_syntax_big_counter_s },
    { "unsigned-value",   &ndn_snmp_obj_syntax_unsigned_s }
};

asn_type ndn_snmp_appl_s =
{
    "ApplicationSyntax", {APPLICATION, 1}, CHOICE, 
    sizeof(_ndn_snmp_appl_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_snmp_appl_ne_array}
}; 

const asn_type * const ndn_snmp_appl = &ndn_snmp_appl_s;




static asn_named_entry_t _ndn_snmp_object_syntax_ne_array [] = 
{
    { "simple",            &ndn_snmp_simple_s },
    { "application-wide" , &ndn_snmp_appl_s }
};

asn_type ndn_snmp_object_syntax_s =
{
    "ObjectSyntax", {APPLICATION, 1}, CHOICE, 
    sizeof(_ndn_snmp_object_syntax_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_snmp_object_syntax_ne_array}
}; 

const asn_type * const ndn_snmp_object_syntax = &ndn_snmp_object_syntax_s;


NDN_DATA_UNIT_TYPE (object_syntax, ndn_snmp_object_syntax_s, ObjectSyntax)


asn_type snmp_no_such_object_s = 
{ "noSuchObject", {CONTEXT_SPECIFIC, 0}, PR_ASN_NULL, 0, {NULL} };

asn_type snmp_no_such_instance_s = 
{ "noSuchInstance", {CONTEXT_SPECIFIC, 1}, PR_ASN_NULL, 0, {NULL} }; 

asn_type snmp_end_of_mib_view_s = 
{ "endOfMibView", {CONTEXT_SPECIFIC, 2}, PR_ASN_NULL, 0, {NULL} };


/* This is very simple temporary specification of VarBind!!! */
static asn_named_entry_t _ndn_snmp_var_bind_ne_array [] = 
{
    { "name",           &ndn_data_unit_objid_s },
    { "value",          &ndn_data_unit_object_syntax_s },
    { "noSuchObject",   &snmp_no_such_object_s },
    { "noSuchInstance", &snmp_no_such_instance_s },
    { "endOfMibView",   &snmp_end_of_mib_view_s }
};

asn_type ndn_snmp_var_bind_s =
{
    "VarBind", {PRIVATE, 100}, SEQUENCE, 
    sizeof(_ndn_snmp_var_bind_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_snmp_var_bind_ne_array}
};

asn_type_p ndn_snmp_var_bind = &ndn_snmp_var_bind_s;


asn_type ndn_snmp_var_bind_seq_s = 
{ "SEQUENCE OF VarBind", {APPLICATION, 200},
   SEQUENCE_OF, 0, {&ndn_snmp_var_bind_s} 
};

asn_type * ndn_snmp_var_bind_seq = &ndn_snmp_var_bind_seq_s;





static asn_named_entry_t _ndn_snmp_message_ne_array [] = 
{
    { "type",       &ndn_snmp_message_type_s },
    { "community",  &ndn_data_unit_char_string_s },
    { "repeats",    &ndn_data_unit_int32_s },
    { "request-id", &ndn_data_unit_int32_s },
    { "err-status", &ndn_snmp_error_status_s },
    { "err-index",  &ndn_data_unit_int32_s },
    { "enterprise", &ndn_snmp_obj_syntax_objid_s },
    { "gen-trap",   &asn_base_integer_s },
    { "spec-trap",  &asn_base_integer_s },
    { "agent-addr", &ndn_ip_address_s },
    { "variable-bindings",  &ndn_snmp_var_bind_seq_s } 
};

asn_type ndn_snmp_message_s =
{
    "SNMP-Message", {PRIVATE, 100}, SEQUENCE, 
    sizeof(_ndn_snmp_message_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_snmp_message_ne_array}
};

asn_type_p ndn_snmp_message = &ndn_snmp_message_s;








static asn_named_entry_t _ndn_snmp_csap_ne_array [] = 
{
    { "version",        &ndn_data_unit_int8_s },
    { "remote-port",    &ndn_data_unit_int16_s },
    { "local-port",     &ndn_data_unit_int16_s },
    { "community",      &ndn_data_unit_char_string_s },
    { "timeout",        &ndn_data_unit_int32_s },
    { "snmp-agent",     &ndn_data_unit_char_string_s }
};

asn_type ndn_snmp_csap_s =
{
    "SNMP-CSAP", {PRIVATE, 101}, SEQUENCE, 
    sizeof(_ndn_snmp_csap_ne_array)/sizeof(asn_named_entry_t),
    {_ndn_snmp_csap_ne_array}
};

asn_type_p ndn_snmp_csap = &ndn_snmp_csap_s;

int snmp_asn_syntaxes[] = 
{ 
    ASN_INTEGER,
    ASN_OCTET_STR,
    ASN_OBJECT_ID,
    ASN_IPADDRESS,
    ASN_UNSIGNED,
    ASN_TIMETICKS,
    ASN_OCTET_STR,
#ifdef OPAQUE_SPECIAL_TYPES
    ASN_OPAQUE_U64,
#else
    ASN_OCTET_STR,
#endif
    ASN_UNSIGNED
};

