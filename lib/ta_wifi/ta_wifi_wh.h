/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library
 *
 * The library provides internal WiFi agent library definitions
 * related to WPA Supplicant and hostapd
 */

#ifndef __TA_WIFI_WH_H__
#define __TA_WIFI_WH_H__

#include "te_config.h"
#include "te_defs.h"
#include "config.h"
#include "rcf_common.h"
#include "ta_wifi_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Apply WiFi configuration to WPA Supplicant and hostpad
 *
 * @param node  The WiFi node
 *
 * @return Status code
 */
extern te_errno ta_wifi_wh_apply(ta_wifi *node);

/**
 * Stop WPA Supplicant/hostapd
 *
 * @param node  The WiFi node
 *
 * @return Status code
 */
extern te_errno ta_wifi_wh_cancel(ta_wifi *node);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_WIFI_WH_H__ */
