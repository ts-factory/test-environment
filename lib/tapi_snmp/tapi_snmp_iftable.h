/** @file
 * @brief Test Environment: 
 *
 * Traffic Application Domain Command Handler
 * SNMP protocol implementaion internal declarations.
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
#ifndef __TE__TAPI_SNMP_IFTABLE_H__
#define __TE__TAPI_SNMP_IFTABLE_H__ 

typedef struct tapi_snmp_if_table_row_t {
    int     *ifIndex;                /* InterfaceIndex,*/
    tapi_snmp_oct_string_t *ifDescr; /* DisplayString,*/
    int     *ifType;                 /* IANAifType,*/
    int     *ifMtu;                  /* Integer32,*/
    int     *ifSpeed;                /* Gauge32,*/
    tapi_snmp_oct_string_t *ifPhysAddress;       /* PhysAddress,*/
    int     *ifAdminStatus;          /* INTEGER,*/
    int     *ifOperStatus;           /* INTEGER,*/
    int     *ifLastChange;           /* TimeTicks,*/
    int     *ifInOctets;             /* Counter32,*/
    int     *ifInUcastPkts;          /* Counter32,*/
    int     *ifInNUcastPkts;         /* Counter32,  -- deprecated*/
    int     *ifInDiscards;           /* Counter32,*/
    int     *ifInErrors;             /* Counter32,*/
    int     *ifInUnknownProtos;      /* Counter32,*/
    int     *ifOutOctets;            /* Counter32,*/
    int     *ifOutUcastPkts;         /* Counter32,*/
    int     *ifOutNUcastPkts;        /* Counter32,  -- deprecated*/
    int     *ifOutDiscards;          /* Counter32,*/
    int     *ifOutErrors;            /* Counter32,*/
    int     *ifOutQLen;              /* Gauge32,    -- deprecated*/
    tapi_snmp_oid_t *ifSpecific;     /*  OBJECT IDENTIFIER -- deprecated*/

} tapi_snmp_if_table_row_t;


#endif /* __TE__TAPI_SNMP_IFTABLE_H__ */
