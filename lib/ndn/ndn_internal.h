/** @file
 * @brief ASN.1 library
 *
 * Definitions of internal useful macros for general NDN ASN.1 types
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

#ifndef __TE_NDN_INTERNAL_H__ 
#define __TE_NDN_INTERNAL_H__ 


extern asn_type asn_base_boolean_s;
extern asn_type asn_base_integer_s;
extern asn_type asn_base_bitstring_s;
extern asn_type asn_base_octstring_s;
extern asn_type asn_base_null_s;
extern asn_type asn_base_objid_s;
extern asn_type asn_base_real_s;
extern asn_type asn_base_enum_s;
extern asn_type asn_base_charstring_s;

extern asn_type ndn_data_unit_ints_s;
extern asn_type ndn_data_unit_enum_s;
extern asn_type ndn_data_unit_mask_s;
extern asn_type ndn_data_unit_env_s;

typedef enum { 
    NDN_DU_PLAIN = 1, 
    NDN_DU_SCRIPT, 
    NDN_DU_ENUM, 
    NDN_DU_MASK, 
    NDN_DU_INTERVALS, 
    NDN_DU_ENV, 
    NDN_DU_FUNC, 
} ndn_data_unit_tags_t;

#define NDN_DATA_UNIT_TYPE(id, asn_t, asn_bt_txt_id)                    \
    static asn_named_entry_t _ndn_data_unit_##id##_ne_array [] =        \
    {                                                                   \
      {"plain",    & asn_t ,              {PRIVATE, NDN_DU_PLAIN}},     \
      {"script",   &asn_base_charstring_s,{PRIVATE, NDN_DU_SCRIPT} },   \
      {"enum",     &ndn_data_unit_enum_s, {PRIVATE, NDN_DU_ENUM} },     \
      {"mask",     &ndn_data_unit_mask_s, {PRIVATE, NDN_DU_MASK} },     \
      {"intervals",&ndn_data_unit_ints_s, {PRIVATE, NDN_DU_INTERVALS}}, \
      {"env",      &ndn_data_unit_env_s,  {PRIVATE, NDN_DU_ENV} },      \
      {"function", &asn_base_charstring_s,{PRIVATE, NDN_DU_FUNC} },     \
    };                                                                  \
    asn_type ndn_data_unit_##id##_s =                                   \
    {                                                                   \
      "DATA-UNIT ( " #asn_bt_txt_id " )", {PRIVATE, 1},                 \
      CHOICE, 7, {_ndn_data_unit_##id##_ne_array}                       \
    };                                                                  \
    const asn_type * const ndn_data_unit_##id = &ndn_data_unit_##id##_s;


#endif /* __TE_NDN_INTERNAL_H__ */
