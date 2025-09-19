/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library
 *
 * Internal WiFi agent library functions and helpers.
 */

#ifndef __TA_WIFI_INTERNAL_H__
#define __TA_WIFI_INTERNAL_H__

#include "te_config.h"
#include "te_defs.h"
#include "config.h"
#include "rcf_common.h"

#include "tapi_cfg_wifi.h"

#if HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** List of SSIDs */
typedef SLIST_HEAD(wifi_ssids, ta_wifi_ssid) wifi_ssids;

/** Extra option */
typedef struct ta_wifi_option {
    SLIST_ENTRY(ta_wifi_option) links;  /**< Single-linked list connector */
    char *name;                         /**< Name of the option */
    char *value;                        /**< Value of the option */
} ta_wifi_option;

/** WiFi SSID */
typedef struct ta_wifi_ssid {
    SLIST_ENTRY(ta_wifi_ssid) links;    /**< Single-linked list connector */
    bool             enable;            /**< Enable state */
    char            *instance_name;     /**< Instance name of the SSID */
    char            *name;              /**< Name of the SSID */
    char            *ifname;            /**< Interface name */
    tapi_cfg_wifi_mode     mode;        /**< WiFi mode */
    tapi_cfg_wifi_security security;    /**< WiFi security */
    tapi_cfg_wifi_protocol protocol;    /**< WiFi protocol */
    char            *passphrase;        /**< Passphrase */
    SLIST_HEAD(, ta_wifi_option) options;   /**< Embedded options */
} ta_wifi_ssid;

/** WiFi port */
typedef struct ta_wifi_port {
    SLIST_ENTRY(ta_wifi_port) links;    /**< Single-linked list connector */
    bool             enable;            /**< Enable state */
    char            *instance_name;     /**< Internal port name */
    char            *ifname;            /**< Device name */
    tapi_cfg_wifi_standard standard;    /**< WiFi standard */
    uint8_t          channel;           /**< Channel of the port */
    tapi_cfg_wifi_width    width;       /**< Bandwidth */

    uint32_t         max_a_msdu;        /**< Max A-MSDU */
    uint32_t         max_a_mpdu;        /**< Max A-MPDU */
    uint32_t         frag_threshold;    /**< Fragment threshold */
    uint32_t         rts_threshold;     /**< RTS threshold */
    uint32_t         short_retry_limit; /**< Short retry limit */
    uint32_t         long_retry_limit;  /**< Long retry limit */
    uint32_t         guard_interval;    /**< Guard interval */
    uint32_t         aifs;              /**< Arbitration Inter-Frame Space */
    uint32_t         contention_window_min; /**< Contention window (min) */
    uint32_t         contention_window_max; /**< Contention window (max) */
    uint32_t         txop_limit;        /**< TXOP limit */
    uint32_t         tx_power;          /**< TX Power */
    uint32_t         max_nss;           /**< Max number of spatial streams */

    wifi_ssids       ssids;             /**< SSID list */
    SLIST_HEAD(, ta_wifi_option) options;   /**< Embedded port options */
} ta_wifi_port;

/** Main wifi object */
typedef struct ta_wifi {
    SLIST_HEAD(, ta_wifi_port) ports;   /**< Port list */
    bool                 enable;        /**< Enable configurator or not */
    bool                 status;        /**< Current status */
    tapi_cfg_wifi_configurator configurator;  /**< Configurator selection */
} ta_wifi;

/**
 * Free up the WiFi option
 *
 * @param      option  The option to free up
 */
extern void ta_wifi_option_free(ta_wifi_option *option);

/**
 * Free up the SSID
 *
 * @param      ssid  The ssid to free up
 */
extern void ta_wifi_ssid_free(ta_wifi_ssid *ssid);

/**
 * Free up the port
 *
 * @param      port  The port to free up
 */
extern void ta_wifi_port_free(ta_wifi_port *port);

/**
 * Find the option among SSID options
 *
 * @param[in]  ssid  The SSID to find option in
 * @param[in]  name  The name of the option
 *
 * @return The found option pointer in case of success, @c NULL in case of
 *         failure
 */
extern ta_wifi_option *ta_wifi_ssid_find_option(const ta_wifi_ssid *ssid,
                                                const char *name);

/**
 * Find the option among port options
 *
 * @param[in]  port  The port to find option in
 * @param[in]  name  The name of the option
 *
 * @return The found option pointer in case of success, @c NULL in case of
 *         failure
 */
extern ta_wifi_option *ta_wifi_port_find_option(const ta_wifi_port *port,
                                                const char *name);

/**
 * Find the SSID among port SSIDs
 *
 * @param[in]  port       The port
 * @param[in]  ssid_name  The SSID name
 *
 * @return The found SSID pointer in case of success, @C NULL in case of failure
 */
extern ta_wifi_ssid *ta_wifi_port_find_ssid(const ta_wifi_port *port,
                                            const char *ssid_name);

/**
 * Find the port among the global list of ports
 *
 * @param[in]  name  The port name
 *
 * @return Pointer to WiFi port, or @c NULL in case of failure.
 */
extern ta_wifi_port *ta_wifi_find_port(const char *port_name);

/**
 * Get main WiFi node
 *
 * @return Pointer to WiFi node (always non-null)
 */
extern ta_wifi *ta_wifi_get_node(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_WIFI_INTERNAL_H__ */
