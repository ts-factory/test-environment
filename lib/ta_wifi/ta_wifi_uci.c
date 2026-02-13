/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library - UCI generic
 *
 * The library provides basics of the UCI support in WiFi agent library
 */

#define TE_LGR_USER "TA WiFi UCI"

#include "te_config.h"

#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "ta_wifi.h"
#include "ta_wifi_internal.h"
#include "ta_wifi_uci.h"

#include "logger_api.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_queue.h"

/* See the description in ta_wifi_uci_internal.h */
void
ta_wifi_tmpl_node_init(ta_wifi_tmpl_node *node)
{
    memset(node, 0, sizeof(ta_wifi_tmpl_node));
    SLIST_INIT(&node->options);
}

/* See the description in ta_wifi_uci_internal.h */
void
ta_wifi_tmpl_node_free(ta_wifi_tmpl_node *node)
{
    ta_wifi_option *opt;
    ta_wifi_option *opt_tmp;

    if (node == NULL)
        return;

    free(node->raw_type);
    free(node->raw_value);
    free(node->device);

    SLIST_FOREACH_SAFE(opt, &node->options, links, opt_tmp)
    {
        SLIST_REMOVE(&node->options, opt, ta_wifi_option, links);
        ta_wifi_option_free(opt);
    }

    free(node);
}

/* See the description in ta_wifi_uci_internal.h */
void
ta_wifi_tmpl_data_init(ta_wifi_tmpl_data *data)
{
    memset(data, 0, sizeof(*data));
}

/* See the description in ta_wifi_uci_internal.h */
void
ta_wifi_tmpl_data_fini(ta_wifi_tmpl_data *data)
{
    ta_wifi_tmpl_node *node;
    ta_wifi_tmpl_node *node_tmp;

    if (data == NULL)
        return;

    SLIST_FOREACH_SAFE(node, &data->templates, links, node_tmp)
    {
        SLIST_REMOVE(&data->templates, node, ta_wifi_tmpl_node, links);
        ta_wifi_tmpl_node_free(node);
    }
}

/* See the description in ta_wifi_uci_parser.h */
ta_wifi_tmpl_node *
ta_wifi_tmpl_data_get_tmpl_by_device(ta_wifi_tmpl_data *data,
    ta_wifi_tmpl_type type,
    const char *device)
{
    struct ta_wifi_tmpl_node *entry;

    if (data == NULL || device == NULL)
        return NULL;

    SLIST_FOREACH(entry, &data->templates, links)
    {
        if (entry->type == type &&
            entry->device != NULL &&
            strcmp(entry->device, device) == 0)
        {
            return entry;
        }
    }

    return NULL;
}
