/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi configuration TAPI
 *
 * @defgroup tapi_cfg_wifi WiFi configuration TAPI
 * @ingroup ta_wifi
 * @{
 *
 * Helpers
 */

#ifndef __TAPI_CFG_WIFI_H__
#define __TAPI_CFG_WIFI_H__

#include "te_config.h"
#include "te_defs.h"
#include "te_enum.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Supported WiFi configurators */
typedef enum tapi_cfg_wifi_configurator {
    TAPI_CFG_WIFI_CFG_AUTO = 0,                 /**< Automatic detection of
                                                     configurator */
    TAPI_CFG_WIFI_CFG_HOSTAPD_WPA_SUPPLICANT,   /**< hostapd/wpa_supplicant
                                                     configurator */
    TAPI_CFG_WIFI_CFG_UCI,                      /**< UCI configurator */
} tapi_cfg_wifi_configurator;

/** Supported WiFi standards */
typedef enum tapi_cfg_wifi_standard {
    TAPI_CFG_WIFI_STANDARD_G = 0,       /**< G standard (2.4GHz) */
    TAPI_CFG_WIFI_STANDARD_N,           /**< N standard (2.4GHz) */
    TAPI_CFG_WIFI_STANDARD_AC,          /**< AC standard (5GHz) */
    TAPI_CFG_WIFI_STANDARD_AX,          /**< AX standard (2.4/5GHz) */
    TAPI_CFG_WIFI_STANDARD_BE,          /**< BE standard (2.4/5/6GHz) */
} tapi_cfg_wifi_standard;

/** Supported WiFi bandwidths */
typedef enum tapi_cfg_wifi_width {
    TAPI_CFG_WIFI_WIDTH_NOT_SET = 0,    /**< Not set */
    TAPI_CFG_WIFI_WIDTH_20 = 20,        /**< 20 MHz */
    TAPI_CFG_WIFI_WIDTH_40 = 40,        /**< 40 MHz */
    TAPI_CFG_WIFI_WIDTH_80 = 80,        /**< 80 MHz */
    TAPI_CFG_WIFI_WIDTH_160 = 160,      /**< 160 MHz */
    TAPI_CFG_WIFI_WIDTH_320 = 320,      /**< 320 MHz */
} tapi_cfg_wifi_width;

/** Supported WiFi modes */
typedef enum tapi_cfg_wifi_mode {
    TAPI_CFG_WIFI_MODE_AP = 0,          /**< Access point mode */
    TAPI_CFG_WIFI_MODE_STA,             /**< STA mode */
} tapi_cfg_wifi_mode;

/** Supported WiFi security */
typedef enum tapi_cfg_wifi_security {
    TAPI_CFG_WIFI_SECURITY_OPEN = 0,    /**< No security (no password) */
    TAPI_CFG_WIFI_SECURITY_WEP,         /**< WEP security */
    TAPI_CFG_WIFI_SECURITY_WPA,         /**< WPA security */
    TAPI_CFG_WIFI_SECURITY_WPA2,        /**< WPA2 security */
    TAPI_CFG_WIFI_SECURITY_WPA3,        /**< WPA3 security */
} tapi_cfg_wifi_security;

/** Supported WiFi protocols */
typedef enum tapi_cfg_wifi_protocol {
    TAPI_CFG_WIFI_PROTOCOL_CCMP = 0,    /**< CCMP protocol */
    TAPI_CFG_WIFI_PROTOCOL_TKIP,        /**< TKIP protocol */
} tapi_cfg_wifi_protocol;

/**
 * Get string representation of the configurator
 *
 * @param value     Numeric representation of the configurator
 *
 * @return The pointer to the string representation
 */
extern const char *tapi_cfg_wifi_configurator_from_value(
    tapi_cfg_wifi_configurator value);

/**
 * Convert string representation of the configurator to the numeric one
 *
 * @param str       Pointer to string representation
 *
 * @return The security mode of @p tapi_cfg_wifi_configurator enumeration
 */
extern tapi_cfg_wifi_configurator tapi_cfg_wifi_configurator_from_str(
    const char *str);

/**
 * Get string representation of the WiFi standard
 *
 * @param value     Numeric representation of the standard
 *
 * @return The pointer to the string representation
 */
extern const char *tapi_cfg_wifi_standard_from_value(
    tapi_cfg_wifi_standard value);

/**
 * Convert string representation of the standard to a numeric one
 *
 * @param str       Pointer to string representation
 *
 * @return The WiFi standard of @p tapi_cfg_wifi_standard enumeration
 */
extern tapi_cfg_wifi_standard tapi_cfg_wifi_standard_from_str(const char *str);

/**
 * Get string representation of the band width
 *
 * @param value     Numeric representation of the width
 *
 * @return The pointer to the string representation
 */
extern const char *tapi_cfg_wifi_width_from_value(tapi_cfg_wifi_width value);

/**
 * Convert string representation of the width to a numeric one
 *
 * @param str       Pointer to string representation
 *
 * @return The band width of @p tapi_cfg_wifi_width enumeration
 */
extern tapi_cfg_wifi_standard tapi_cfg_wifi_width_from_str(const char *str);

/**
 * Get string representation of the WiFi operation mode
 *
 * @param value     Numeric representation of the mode
 *
 * @return The pointer to the string representation
 */
extern const char *tapi_cfg_wifi_mode_from_value(tapi_cfg_wifi_mode value);

/**
 * Convert string representation of the operation mode to the numeric one
 *
 * @param str       Pointer to string representation
 *
 * @return The security mode of @p tapi_cfg_wifi_mode enumeration
 */
extern tapi_cfg_wifi_mode tapi_cfg_wifi_mode_from_str(const char *str);

/**
 * Get string representation of the WiFi security mode
 *
 * @param value     Numeric representation of the security mode
 *
 * @return The pointer to the string representation
 */
extern const char *tapi_cfg_wifi_security_from_value(
    tapi_cfg_wifi_security value);

/**
 * Convert string representation of the security mode to a numeric one
 *
 * @param str       Pointer to string representation
 *
 * @return The security mode of @p tapi_cfg_wifi_security enumeration
 */
extern tapi_cfg_wifi_security tapi_cfg_wifi_security_from_str(const char *str);

/**
 * Get string representation of the protocol
 *
 * @param value     Numeric representation of the protocol
 *
 * @return The pointer to the string representation
 */
extern const char *tapi_cfg_wifi_protocol_from_value(
    tapi_cfg_wifi_protocol value);

/**
 * Convert string representation of the protocol to the numeric one
 *
 * @param str       Pointer to string representation
 *
 * @return The security mode of @p tapi_cfg_wifi_protocol enumeration
 */
extern tapi_cfg_wifi_mode tapi_cfg_wifi_protocol_from_str(const char *str);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !__TAPI_CFG_WIFI_H__ */
