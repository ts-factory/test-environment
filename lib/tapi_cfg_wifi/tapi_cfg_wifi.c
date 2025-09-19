/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi configuration TAPI
 */

#define TE_LGR_USER "TAPI CFG WIFI"

#include "te_config.h"
#include "te_defs.h"
#include "te_enum.h"
#include "tapi_cfg_wifi.h"

/** Mapping of supported WiFi configurators */
static const te_enum_map wifi_configurator_mapping[] = {
    { .name = "auto", .value = TAPI_CFG_WIFI_CFG_AUTO },
    { .name = "hostapd_wpa_supplicant",
        .value = TAPI_CFG_WIFI_CFG_HOSTAPD_WPA_SUPPLICANT },
    { .name = "uci", .value = TAPI_CFG_WIFI_CFG_UCI },
    TE_ENUM_MAP_END
};

/** Mapping of supported WiFi standards */
static const te_enum_map wifi_standard_mapping[] = {
    { .name = "g", .value = TAPI_CFG_WIFI_STANDARD_G },
    { .name = "n", .value = TAPI_CFG_WIFI_STANDARD_N },
    { .name = "ac", .value = TAPI_CFG_WIFI_STANDARD_AC },
    { .name = "ax", .value = TAPI_CFG_WIFI_STANDARD_AX },
    TE_ENUM_MAP_END
};

/** Mapping of supported widths */
static const te_enum_map wifi_width_mapping[] = {
    { .name = "0", .value = TAPI_CFG_WIFI_WIDTH_NOT_SET },
    { .name = "20", .value = TAPI_CFG_WIFI_WIDTH_20 },
    { .name = "40", .value = TAPI_CFG_WIFI_WIDTH_40 },
    { .name = "80", .value = TAPI_CFG_WIFI_WIDTH_80 },
    { .name = "160", .value = TAPI_CFG_WIFI_WIDTH_160 },
    { .name = "320", .value = TAPI_CFG_WIFI_WIDTH_320 },
    TE_ENUM_MAP_END
};

/** Mapping of supported WiFi security */
static const te_enum_map wifi_security_mapping[] = {
    { .name = "open", .value = TAPI_CFG_WIFI_SECURITY_OPEN },
    { .name = "wep", .value = TAPI_CFG_WIFI_SECURITY_WEP },
    { .name = "wpa", .value = TAPI_CFG_WIFI_SECURITY_WPA },
    { .name = "wpa2", .value = TAPI_CFG_WIFI_SECURITY_WPA2 },
    { .name = "wpa3", .value = TAPI_CFG_WIFI_SECURITY_WPA3 },
    TE_ENUM_MAP_END
};

/** Mapping of supported WiFi modes */
static const te_enum_map wifi_mode_mapping[] = {
    { .name = "ap", .value = TAPI_CFG_WIFI_MODE_AP },
    { .name = "sta", .value = TAPI_CFG_WIFI_MODE_STA },
    TE_ENUM_MAP_END
};

/** Mapping of supported WiFi protocols */
static const te_enum_map wifi_protocol_mapping[] = {
    { .name = "ccmp", .value = TAPI_CFG_WIFI_PROTOCOL_CCMP },
    { .name = "tkip", .value = TAPI_CFG_WIFI_PROTOCOL_TKIP },
    TE_ENUM_MAP_END
};

/* See the description in tapi_cfg_wifi.h */
const char *
tapi_cfg_wifi_configurator_from_value(tapi_cfg_wifi_configurator value)
{
    return te_enum_map_from_value(wifi_configurator_mapping, value);
}

/* See the description in tapi_cfg_wifi.h */
tapi_cfg_wifi_configurator
tapi_cfg_wifi_configurator_from_str(const char *str)
{
    return te_enum_map_from_str(wifi_configurator_mapping, str, -1);
}

/* See the description in tapi_cfg_wifi.h */
const char *
tapi_cfg_wifi_standard_from_value(tapi_cfg_wifi_standard value)
{
    return te_enum_map_from_value(wifi_standard_mapping, value);
}

/* See the description in tapi_cfg_wifi.h */
tapi_cfg_wifi_standard
tapi_cfg_wifi_standard_from_str(const char *str)
{
    return te_enum_map_from_str(wifi_standard_mapping, str, -1);
}

/* See the description in tapi_cfg_wifi.h */
const char *
tapi_cfg_wifi_width_from_value(tapi_cfg_wifi_width value)
{
    return te_enum_map_from_value(wifi_width_mapping, value);
}

/* See the description in tapi_cfg_wifi.h */
tapi_cfg_wifi_standard
tapi_cfg_wifi_width_from_str(const char *str)
{
    return te_enum_map_from_str(wifi_width_mapping, str, -1);
}

/* See the description in tapi_cfg_wifi.h */
const char *
tapi_cfg_wifi_mode_from_value(tapi_cfg_wifi_mode value)
{
    return te_enum_map_from_value(wifi_mode_mapping, value);
}

/* See the description in tapi_cfg_wifi.h */
tapi_cfg_wifi_mode
tapi_cfg_wifi_mode_from_str(const char *str)
{
    return te_enum_map_from_str(wifi_mode_mapping, str, -1);
}

/* See the description in tapi_cfg_wifi.h */
const char *
tapi_cfg_wifi_security_from_value(tapi_cfg_wifi_security value)
{
    return te_enum_map_from_value(wifi_security_mapping, value);
}

/* See the description in tapi_cfg_wifi.h */
tapi_cfg_wifi_security
tapi_cfg_wifi_security_from_str(const char *str)
{
    return te_enum_map_from_str(wifi_security_mapping, str, -1);
}

/* See the description in tapi_cfg_wifi.h */
const char *
tapi_cfg_wifi_protocol_from_value(tapi_cfg_wifi_protocol value)
{
    return te_enum_map_from_value(wifi_protocol_mapping, value);
}

/* See the description in tapi_cfg_wifi.h */
tapi_cfg_wifi_mode
tapi_cfg_wifi_protocol_from_str(const char *str)
{
    return te_enum_map_from_str(wifi_protocol_mapping, str, -1);
}
