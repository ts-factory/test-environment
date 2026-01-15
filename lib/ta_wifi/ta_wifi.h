/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library
 *
 * @defgroup ta_wifi_agent WiFi control library agent side (ta_wifi)
 * @ingroup ta_wifi
 * @{
 *
 * Agent-side library to use to control WiFi on hosts.
 */

#ifndef __TA_WIFI_H__
#define __TA_WIFI_H__

#include "te_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize WiFi configuration.
 *
 * @return Status code.
 */
extern te_errno ta_unix_conf_wifi_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_WIFI_H__ */

/**@} <!-- END ta_wifi --> */
