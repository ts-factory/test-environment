/** @file
 * @brief RCF Engine - TA-specific library interface
 *
 * Definition of RCF TA-specific library interface.
 * 
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS
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
 *
 * @author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
 * 
 * $Id$
 */

#ifndef __TE_RCF_METHODS_H__
#define __TE_RCF_METHODS_H__

#ifdef __cplusplus
extern "C" {
#endif

    
/** Handle returned by the "start" method and used to control the TA */
typedef void * rcf_talib_handle;


/** @name Test Agent flags */
#define TA_LOCAL    1   /**< Test Agent runs on the same station with 
                             TEN, so it's prohibited to reboot it 
                             if it is not proxy */
#define TA_PROXY    2   /**< Test Agent is proxy agent, so reboot should
                             not lead to loss of the connectivity */
/*@}*/


/**
 * Start the Test Agent. Note that it's not necessary
 * to restart the proxy Test Agents after rebooting of
 * the NUT, which it serves.
 *
 * @param ta_name       Test Agent name
 * @param ta_type       Test Agent type (Test Agent executable is equal
 *                      to ta_type and is located in TE_INSTALL/agents/bin)
 * @param conf_str      TA-specific configuration string
 * @param handle        location for TA handle
 * @param flags         location of TA flags
 *
 * @return error code 
 */
typedef int (* rcf_talib_start)(char *ta_name, char *ta_type, 
                                char *conf_str, rcf_talib_handle *handle,
                                int *flags);

/**
 * Reboot Test Agent station or NUT served by it. The method is called
 * after sending of "reboot" command to the TA. After that rcf_talib_start
 * and rcf_talib_connect are called.
 * For the case of local Test Agents this routine should kill the Test Agent,
 * but return an error.
 *
 * @param handle        TA handle
 * @param parms         parameter string passed to the TA in "reboot" command
 *                      or NULL
 *
 * @return error code 
 */
typedef int (* rcf_talib_reboot)(rcf_talib_handle handle, char *parms);

/**
 * Establish connection with the Test Agent. Note that it's not necessary
 * to perform real reconnect to proxy Test Agents after rebooting of
 * the NUT, which it serves.
 *
 * @param handle        TA handle
 * @param select_set    FD_SET to be updated with the TA connection file 
 *                      descriptor (for Test Agents supporting listening mode)
 *                      (IN/OUT)
 *
 * @param select_tm     timeout value for the select to be updated with
 *                      TA polling interval (for Test Agents supporting
 *                      polling mode only)
 *                      (IN/OUT)
 *
 * @return error code 
 */
typedef int (* rcf_talib_connect)(rcf_talib_handle handle, 
                                  fd_set *select_set, 
                                  struct timeval *select_tm);

/**
 * Transmit data to the Test Agent.
 *
 * @param handle        TA handle
 * @param data          data to be transmitted
 * @param len           data length
 *
 * @return error code 
 */
typedef int (* rcf_talib_transmit)(rcf_talib_handle handle, 
                                   char *data, size_t len);
 
/**
 * Check pending data on the Test Agent connection.
 *
 * @param handle        TA handle
 *
 * @return TRUE, if data are pending; FALSE otherwise
 */
typedef int (* rcf_talib_is_ready)(rcf_talib_handle handle);

/**
 * Receive one command (possibly with attachment) from the Test Agent 
 * or its part.
 *
 * @param handle        TA handle
 * @param buf           location for received data
 * @param len           should be filled by the caller to length of the buffer;
 *                      is filled by the routine to length of received data
 * @param pba           location for address of first byte after answer 
 *                      end marker (is set only if binary attachment presents)
 *
 * @return error code
 * @retval 0            success
 *
 * @retval ETESMALLBUF  Buffer is too small for the command. The part
 *                      of the command is written to the buffer. Other
 *                      part(s) of the message can be read by the subsequent
 *                      routine calls. ETSMALLBUF is returned until last 
 *                      part of the message is read.
 *
 * @retval ETEPENDING   Attachment is too big to fit into the buffer.
 *                      The command and a part of the attachment is written 
 *                      to the buffer. Other part(s) can be read by the 
 *                      subsequent routine calls. ETEPENDING is returned until 
 *                      last part of the message is read.
 *
 * @retval other        OS errno
 */
typedef int (* rcf_talib_receive)(rcf_talib_handle handle, 
                                  char *buf, size_t *len, char **pba);

/**
 * Close interactions with TA.
 *
 * @param handle        TA handle
 * @param select_set    FD_SET to be updated with the TA connection file 
 *                      descriptor (for Test Agents supporting listening mode)
 *                      (IN/OUT)
 *
 * @return error code 
 */
typedef int (* rcf_talib_close)(rcf_talib_handle handle, fd_set *select_set);

#ifdef __cplusplus
}
#endif
#endif /* !__TE_RCF_METHODS_H__ */
