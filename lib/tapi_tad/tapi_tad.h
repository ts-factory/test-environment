/** @file
 * @brief Test API
 *
 * Definition of Test API for common Traffic Application Domain
 * features.
 *
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
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
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TE_TAPI_TAD_H__
#define __TE_TAPI_TAD_H__

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <unistd.h>

#include "te_defs.h"
#include "te_stdint.h"
#include "tad_common.h"
#include "asn_usr.h"
#include "rcf_api.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Get status parameter of CSAP.
 *
 * @param ta_name   - name of the Test Agent
 * @param ta_sid    - session identfier to be used
 * @param csap_id   - CSAP handle
 * @param status    - location for status of csap (OUT)
 *
 * @return Status code.
 */
extern int tapi_csap_get_status(const char *ta_name, int ta_sid,
                                csap_handle_t csap_id,
                                tad_csap_status_t *status);

/**
 * Get total number of bytes parameter of CSAP.
 *
 * @param ta_name   - name of the Test Agent
 * @param ta_sid    - session identfier to be used
 * @param csap_id   - CSAP handle
 * @param p_bytes   - location for total number of bytes (OUT)
 *
 * @return Status code.
 */
extern int tapi_csap_get_total_bytes(const char *ta_name, int ta_sid,
                                     csap_handle_t csap_id,
                                     unsigned long long int *p_bytes);

/**
 * Get duration of the last traffic receiving session on TA CSAP.
 *
 * Returned value is calculated as difference between timestamp
 * of the last packet and timestamp of the first packet.
 *
 * @param ta_name   - name of the Test Agent
 * @param ta_sid    - session identfier to be used
 * @param csap_id   - CSAP handle
 * @param p_dur     - location for duration (OUT)
 *
 * @return Status code.
 */
extern int tapi_csap_get_duration(const char *ta_name, int ta_sid,
                                  csap_handle_t csap_id,
                                  struct timeval *p_dur);

/**
 * Get 'long long int' CSAP parameter from TA.
 *
 * @param ta_name       - name of the Test Agent
 * @param ta_sid        - session identfier to be used
 * @param csap_id       - CSAP handle
 * @param param_name    - parameter name
 * @param p_llint       - location for 'long long int'
 *
 * @return Status code.
 */
extern int tapi_csap_param_get_llint(const char *ta_name, int ta_sid,
                                     csap_handle_t csap_id,
                                     const char *param_name,
                                     int64_t *p_llint);

/**
 * Get timestamp CSAP parameter from TA in format "<sec>.<usec>".
 *
 * @param ta_name       - name of the Test Agent
 * @param ta_sid        - session identfier to be used
 * @param csap_id       - CSAP handle
 * @param param_name    - parameter name
 * @param p_timestamp   - location for timestamp
 *
 * @return Status code.
 */
extern int tapi_csap_param_get_timestamp(const char *ta_name, int ta_sid,
                                         csap_handle_t csap_id,
                                         const char *param_name,
                                         struct timeval *p_timestamp);

/**
 * Creates CSAP (communication service access point) on the Test Agent. 
 *
 * @param ta_name       Test Agent name                 
 * @param session       TA session or 0   
 * @param stack_id      protocol stack identifier
 * @param csap_spec     ASN value of type CSAP-spec
 * @param handle        location for unique CSAP handle
 *
 * @return zero on success or error code
 */
extern int tapi_tad_csap_create(const char *ta_name, int session,
                                const char *stack_id, 
                                const asn_value *csap_spec, int *handle);

/**
 * Destroys CSAP (communication service access point) on the Test Agent.
 *
 * @note In comparison with rcf_ta_csap_destroy() the function
 *       synchronizes /agent/csap subtree of the corresponding
 *       Test Agent.
 *
 * @param ta_name       Test Agent name
 * @param session       TA session or 0
 * @param handle        CSAP handle
 *
 * @return Status code.
 */
extern te_errno tapi_tad_csap_destroy(const char *ta_name, int session,
                                      csap_handle_t handle);

/**
 * This function is used to force sending of traffic via already created
 * CSAP.
 * Started sending process may be managed via standard function rcf_ta_*.
 *
 * @param ta_name       Test Agent name                 
 * @param session       TA session or 0   
 * @param csap          CSAP handle
 * @param templ         ASN value of type Traffic-Template
 * @param blk_mode      mode of the operation:
 *                      in blocking mode it suspends the caller
 *                      until sending of all the traffic finishes
 *
 * @return zero on success or error code
 */
extern int tapi_tad_trsend_start(const char *ta_name, int session, 
                                 csap_handle_t csap, const asn_value *templ,
                                 rcf_call_mode_t blk_mode);

/**
 * Start receiving of traffic via already created CSAP. 
 * Started receiving process may be managed via standard function rcf_ta_*.
 *
 * @param ta_name       Test Agent name
 * @param session       TA session or 0
 * @param handle        CSAP handle
 * @param pattern       ASN value of type Traffic-Pattern
 * @param timeout       Timeout for traffic receive operation. After this
 *                      time interval CSAP stops capturing any traffic on
 *                      the agent and will be waiting until
 *                      rcf_ta_trrecv_stop() or rcf_ta_trrecv_wait() are
 *                      called.
 * @param num           Number of packets that needs to be captured;
 *                      if it is zero, the number of received packets
 *                      is not limited.
 * @param mode          Count received packets only or store packets
 *                      to get to the test side later
 *
 * @return Zero on success or error code
 */
extern te_errno tapi_tad_trrecv_start(const char      *ta_name,
                                      int              session,
                                      csap_handle_t    handle,
                                      const asn_value *pattern,
                                      unsigned int     timeout,
                                      unsigned int     num,
                                      rcf_trrecv_mode  mode);


/**
 * Type for callback which will receive catched packets.
 *
 * @param packet        ASN value with received packet
 * @param user_data     Pointer to opaque data, specified by user for his
 *                      callback,
 *
 * @return none 
 */
typedef void (*tapi_tad_trrecv_cb)(asn_value *packet,
                                   void      *user_data);

/**
 * Structure for with parameters for receiving packets
 */
typedef struct tapi_tad_trrecv_cb_data {
    tapi_tad_trrecv_cb  callback;   /**< Callback */
    void               *user_data;  /**< Pointer to user data for it */
} tapi_tad_trrecv_cb_data;

/**
 * Standard method to make struct with parameters for receiving packet.
 *
 * @param callback      User callback
 * @param user_data     Pointer to user data for it
 *
 * @return pointer to new instance of structure.
 */
extern tapi_tad_trrecv_cb_data *tapi_tad_trrecv_make_cb_data(
                                    tapi_tad_trrecv_cb  callback,
                                    void               *user_data);

/**
 * Continue already started receiving process on CSAP. 
 * Blocks until receive will be finished. 
 *
 * @param ta_name       Test Agent name
 * @param session       TA session or 0
 * @param handle        CSAP handle
 * @param cb_data       Struct with user-specified data for 
 *                      catching packets
 * @param num           Location for number of received packets
 *
 * @return status code
 */
extern te_errno tapi_tad_trrecv_wait(const char              *ta_name,
                                     int                      session,
                                     csap_handle_t            handle,
                                     tapi_tad_trrecv_cb_data *cb_data,
                                     unsigned int            *num);

/**
 * Stops already started receiving process on CSAP. 
 *
 * @param ta_name       Test Agent name
 * @param session       TA session or 0
 * @param handle        CSAP handle
 * @param cb_data       Struct with user-specified data for 
 *                      catching packets
 * @param num           Location for number of received packets
 *
 * @return status code
 */
extern te_errno tapi_tad_trrecv_stop(const char              *ta_name,
                                     int                      session,
                                     csap_handle_t            handle,
                                     tapi_tad_trrecv_cb_data *cb_data,
                                     unsigned int            *num);

/**
 * Get received packets from already started receiving process on CSAP. 
 * Dont blocks, dont stop receiving process.
 * Got packets are removed from cache on CSAP, and will not be received
 * again, during call of _wait, _stop or _get method.
 *
 * @param ta_name       Test Agent name
 * @param session       TA session or 0
 * @param handle        CSAP handle
 * @param cb_data       Struct with user-specified data for 
 *                      catching packets
 * @param num           Location for number of received packets
 *
 * @return status code
 */
extern te_errno tapi_tad_trrecv_get(const char              *ta_name,
                                    int                      session,
                                    csap_handle_t            handle,
                                    tapi_tad_trrecv_cb_data *cb_data,
                                    unsigned int            *num);



/**
 * Insert arithmetical progression iterator argument into Traffic-Template
 * ASN value, at the end of iterator list. 
 * New iterator became most internal. 
 * 
 * @param templ         ASN value of Traffic-Template type 
 * @param begin         start of arithmetic progression
 * @param end           end of arithmetic progression
 * @param step          step of arithmetic progression
 *
 * @return status code
 */
extern int tapi_tad_add_iterator_for(asn_value *templ, int begin, int end,
                                     int step);

/**
 * Insert 'enumeration' iterator argument into Traffic-Template ASN value,
 * at the end of iterator list. 
 * New iterator became most internal. 
 * 
 * @param templ         ASN value of Traffic-Template type 
 * @param array         pointer to array with enumerated values
 * @param length        length of array
 *
 * @return status code
 */
extern int tapi_tad_add_iterator_ints(asn_value *templ, int *array,
                                      size_t length);



/**
 * Receive all data which currently are waiting for receive in 
 * specified CSAP and forward them into another CSAP, without 
 * passing via RCF to test. 
 *
 * @param ta_name       TA name
 * @param session       RCF session id
 * @param csap_rcv      identifier of recieve CSAP
 * @param csap_fwd      identifier of CSAP which should obtain data
 * @param pattern       traffic Pattern to receive data
 * @param timeout       timeout to wait data, in milliseconds
 * @param forwarded     number of forwarded PDUs (OUT)
 *
 * @return status code
 */
extern int tapi_tad_forward_all(const char *ta_name, int session,
                                csap_handle_t csap_rcv,
                                csap_handle_t csap_fwd,
                                asn_value *pattern,
                                unsigned int timeout, int *forwarded);

/**
 * Add socket layer over existing file descriptor in CSAP specification.
 *
 * @param csap_spec     Location of CSAP specification pointer.
 * @param fd            File descriptor to read/write data
 *
 * @retval Status code.
 */
extern te_errno tapi_tad_socket_add_csap_layer(asn_value **csap_spec,
                                               int         fd);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_TAD_H__ */
