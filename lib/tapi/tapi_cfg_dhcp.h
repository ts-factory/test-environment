/** @file
 * @brief Test API to configure DHCP.
 *
 * Definition of API to configure DHCP.
 *
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights reserved.
 *
 * 
 *
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 */

#ifndef __TE_TAPI_CFG_DHCP_H__
#define __TE_TAPI_CFG_DHCP_H__

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include "conf_api.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup tapi_conf_dhcp_serv DHCP Server configuration
 * @ingroup tapi_conf
 * @{
 */

/**
 * Add subnet declaration in DHCP server on the Test Agent.
 *
 * @param ta            Test Agent
 * @param subnet        Subnet address
 * @param prefix_len    Subnet prefix length
 * @param handle        Location for handle of the created instance or
 *                      NULL
 *
 * @return Status code
 */
extern int tapi_cfg_dhcps_add_subnet(const char            *ta,
                                     const struct sockaddr *subnet,
                                     int                    prefix_len,
                                     cfg_handle            *handle);

/**
 * Add host definition in DHCP server on the Test Agent.
 *
 * @param ta            Test Agent
 * @param name          Name of the host for human or NULL (unique name
 *                      will be generated by TAPI)
 * @param group         Name of the group the host belongs to or NULL
 * @param chaddr        Hardware address
 * @param client_id     Client identifier or NULL
 * @param client_id_len Client identifier length if ID is in binary
 *                      format or -1 if ID is a string
 * @param fixed_ip      Fixed IP address or NULL
 * @param next_server   Next server or NULL
 * @param filename      File name or NULL
 * @param flags         Flags for Solaris 'dhcp' server lease type (bootp)
 * @param handle        Location for handle of the created instance or
 *                      NULL
 *
 * @return Status code
 *
 * @attention In the case of failure, configuration changes are not
 *            rolled back. It is assumed that test fails and
 *            configuration backup will be restored.
 *
 * @note Ideally the function must used local set operations and, then,
 *       commit. However, it does not work :(
 */
extern int tapi_cfg_dhcps_add_host(const char            *ta,
                                   const char            *name,
                                   const char            *group,
                                   const struct sockaddr *chaddr,
                                   const void           *client_id,
                                   int                   client_id_len,
                                   const struct sockaddr *fixed_ip,
                                   const char            *next_server,
                                   const char            *filename,
                                   const char            *flags,
                                   cfg_handle            *handle);

/**
 * Add host definition in DHCP server on the Test Agent,
 * host identifier and prefix delegation are supported.
 *
 * @param ta            Test Agent
 * @param name          Name of the host for human or NULL (unique name
 *                      will be generated by TAPI)
 * @param group         Name of the group the host belongs to or NULL
 * @param chaddr        Hardware address
 * @param client_id     Client identifier or NULL
 * @param client_id_len Client identifier length if ID is in binary
 *                      format or -1 if ID is a string
 * @param fixed_ip      Fixed IP address or NULL
 * @param next_server   Next server or NULL
 * @param filename      File name or NULL
 * @param flags         Flags for Solaris 'dhcp' server lease type (bootp)
 * @param host_id       DHCPv6 host identifier or NULL
 * @param host_id_len   Host identifier length if ID is in binary format
 *                      or -1 if ID is a string
 * @param prefix6       DHCPv6 prefix delegation in string format or NULL
 * @param handle        Location for handle of the created instance or
 *                      NULL
 *
 * @return Status code
 *
 * @attention In the case of failure, configuration changes are not
 *            rolled back. It is assumed that test fails and
 *            configuration backup will be restored.
 *
 * @note Ideally the function must used local set operations and, then,
 *       commit. However, it does not work :(
 */
extern int tapi_cfg_dhcps_add_host_gen(const char            *ta,
                                       const char            *name,
                                       const char            *group,
                                       const struct sockaddr *chaddr,
                                       const void           *client_id,
                                       int                   client_id_len,
                                       const struct sockaddr *fixed_ip,
                                       const char            *next_server,
                                       const char            *filename,
                                       const char            *flags,
                                       const void            *host_id,
                                       int                    host_id_len,
                                       const char            *prefix6,
                                       cfg_handle            *handle);

/**@} <!-- END tapi_conf_dhcp_serv --> */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_CFG_DHCP_H__ */
