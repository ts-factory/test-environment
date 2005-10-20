/** @file
 * @brief Proteos, TAD CLI protocol, NDN.
 *
 * Declarations of ASN.1 types for NDN for ISCSI protocol. 
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
 * @author Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * $Id$
 */ 
#ifndef __TE_NDN_ISCSI_H__
#define __TE_NDN_ISCSI_H__

#include "asn_usr.h"
#include "ndn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ASN.1 tags for ISCSI CSAP NDN
 */
typedef enum {
    NDN_TAG_ISCSI_TYPE,
    NDN_TAG_ISCSI_ADDR,
    NDN_TAG_ISCSI_PORT,
    NDN_TAG_ISCSI_SOCKET,
    NDN_TAG_ISCSI_MESSAGE,
    NDN_TAG_ISCSI_LEN,
    NDN_TAG_ISCSI_PARAM,
} ndn_iscsi_tags_t;

/**
 * ASN.1 tags for ISCSI Segment Data fields access
 */
typedef enum {
    NDN_TAG_ISCSI_SD_INT_VALUE,
    NDN_TAG_ISCSI_SD_HEX_VALUE,
    NDN_TAG_ISCSI_SD_STR_VALUE,
    NDN_TAG_ISCSI_SD_KEY_VALUE,
    NDN_TAG_ISCSI_SD_KEY_VALUES,
    NDN_TAG_ISCSI_SD_KEY,
    NDN_TAG_ISCSI_SD_VALUES,
    NDN_TAG_ISCSI_SD_KEY_PAIR,
    NDN_TAG_ISCSI_SD_SEGMENT_DATA,
    NDN_TAG_ISCSI_SD,
} ndn_iscsi_sd_tags_t;


extern const asn_type *ndn_iscsi_message;
extern const asn_type *ndn_iscsi_csap;

typedef struct iscsi_target_params_t {
    int param;
} iscsi_target_params_t;

typedef int iscsi_key;
typedef int iscsi_key_value;
typedef asn_value *iscsi_segment_data;
typedef asn_value *iscsi_key_values;

typedef enum {
    iscsi_key_value_type_invalid = -1,
    iscsi_key_value_type_int = 0,
    iscsi_key_value_type_hex = 1,
    iscsi_key_value_type_string = 2,
} iscsi_key_value_type;    

extern const asn_type * const ndn_iscsi_segment_data;
extern const asn_type * const ndn_iscsi_key_pair;
extern const asn_type * const ndn_iscsi_key_values;
extern const asn_type * const ndn_iscsi_key_value;

extern int
asn2bin_data(asn_value *segment_data, 
             uint8_t *data, uint32_t *data_len);
             
extern int
bin_data2asn(uint8_t *data, uint32_t data_len, 
            asn_value_p *value);

extern int parse_key_value(char *str, asn_value *value);
 
/**
 * Flags:
 * the following assumption holds:
 * if parameter of the local Initiator structure 
 * was untouched than it should not be synchronized
 * with the Initiator. Than the Initiator uses the
 * default parameter and MAY NOT offer the parameter
 * during the negotiations.
 */
#define OFFER_MAX_CONNECTIONS                   (1 << 0)
#define OFFER_INITIAL_R2T                       (1 << 1)
#define OFFER_HEADER_DIGEST                     (1 << 2)
#define OFFER_DATA_DIGEST                       (1 << 3)
#define OFFER_IMMEDIATE_DATA                    (1 << 4)
#define OFFER_MAX_RECV_DATA_SEGMENT_LENGTH      (1 << 5)
#define OFFER_FIRST_BURST_LENGTH                (1 << 6)
#define OFFER_MAX_BURST_LENGTH                  (1 << 7)
#define OFFER_DEFAULT_TIME2WAIT                 (1 << 8)
#define OFFER_DEFAULT_TIME2RETAIN               (1 << 9)
#define OFFER_MAX_OUTSTANDING_R2T               (1 << 10)
#define OFFER_DATA_PDU_IN_ORDER                 (1 << 11)
#define OFFER_DATA_SEQUENCE_IN_ORDER            (1 << 12)
#define OFFER_ERROR_RECOVERY_LEVEL              (1 << 13)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __TE_NDN_ISCSI_H__ */
