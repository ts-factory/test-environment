/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library
 *
 * The library provides internal WiFi agent library definitions
 * related to UCI parser.
 */

#ifndef __TA_WIFI_UCI_H__
#define __TA_WIFI_UCI_H__

#include "te_config.h"
#include "te_defs.h"
#include "config.h"
#include "rcf_common.h"
#include "ta_wifi_internal.h"

#if HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Template type, used for easy access to UCI templates */
typedef enum ta_wifi_tmpl_type {
    TA_WIFI_TMPL_TYPE_NONE,     /**< Unknown template type */
    TA_WIFI_TMPL_TYPE_DEVICE,   /**< The template is for the device */
    TA_WIFI_TMPL_TYPE_IFACE,    /**< The template is for the interface */
    TA_WIFI_TMPL_TYPE_OTHER,    /**< The template is for other undefined
                                     entity that should be indexed by raw
                                     type and raw value */
} ta_wifi_tmpl_type;

/**
 * Template node. It might contain the list of options and a few parsed
 * options.
 */
typedef struct ta_wifi_tmpl_node {
    SLIST_ENTRY(ta_wifi_tmpl_node) links;   /**< Single-linked list connector */
    ta_wifi_tmpl_type type;     /**< Parsed template type */
    char             *raw_type; /**< Raw template type as written in UCI */
    char             *raw_value;/**< Raw value as written in UCI */
    char             *device;   /**< Parsed device. This field is
                                     propagated for
                                     @c TA_WIFI_TEMPLATE_TYPE_DEVICE and
                                     @c TA_WIFI_TEMPLATE_TYPE_IFACE */
    SLIST_HEAD(, ta_wifi_option) options;   /**< Options from UCI file */
} ta_wifi_tmpl_node;

/**
 * Template container
 */
typedef struct ta_wifi_tmpl_data {
    SLIST_HEAD(, ta_wifi_tmpl_node) templates;  /**< Templates parsed from
                                                     UCI file */
} ta_wifi_tmpl_data;


/* Generic functions */

/**
 * Initialize template node
 *
 * @param      node  The pointer to the uninitialized template node
 */
extern void ta_wifi_tmpl_node_init(ta_wifi_tmpl_node *node);

/**
 * Free the template node completely and all resources used by it.
 *
 * @param      node  The pointer to the template node
 */
extern void ta_wifi_tmpl_node_free(ta_wifi_tmpl_node *node);


/**
 * Initialize the template data
 *
 * @param      data  The pointer to the data to initialize.
 */
extern void ta_wifi_tmpl_data_init(ta_wifi_tmpl_data *data);

/**
 * Release all resources used by template data
 *
 * @param      data  The pointer to the data to free up.
 */
extern void ta_wifi_tmpl_data_fini(ta_wifi_tmpl_data *data);

/**
 * Get node template by device
 *
 * @param data    The template data from which to get data
 * @param type    The type of the target template
 * @param device  The device associated with the target template
 *
 * @return Template with @c type type and @c device device name,
 *         or @c NULL if not found.
 */
extern ta_wifi_tmpl_node *ta_wifi_tmpl_data_get_tmpl_by_device(
    ta_wifi_tmpl_data *data,
    ta_wifi_tmpl_type type,
    const char *device);

/* Apply functions */

/**
 * Apply WiFi configuration to UCI
 *
 * @param node  The WiFi node
 *
 * @return Status code
 */
extern te_errno ta_wifi_uci_apply(ta_wifi *node);

/**
 * Cancel/remove applied WiFi UCI configuration
 *
 * @param node  The WiFi node
 *
 * @return Status code
 */
extern te_errno ta_wifi_uci_cancel(ta_wifi *node);

/* Parsing functions */

/**
 * Parse UCI configuration and fill data with it.
 *
 * @param[in]  uci_conf_path    The path to UCI configuration
 * @param[out] data             The fetched data. It must be non-initialized.
 *
 * @return Status code.
 */
extern te_errno ta_wifi_uci_parser_parse(const char *uci_conf_path,
                                         ta_wifi_tmpl_data *data);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_WIFI_UCI_H__ */
