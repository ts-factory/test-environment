/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library - UCI parsing support
 *
 * The library parses UCI network configuration.
 */

#define TE_LGR_USER "TA WiFi UCI Parser"

#include "te_config.h"

#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "te_vector.h"

#include "ta_wifi.h"
#include "ta_wifi_internal.h"
#include "ta_wifi_uci.h"

#include "logger_api.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_queue.h"

#define MAX_LINE (1024)

/** Parsing context */
typedef struct ta_wifi_tmpl_parse_context {
    ta_wifi_tmpl_node *current_node; /**< Current node (shouldn't be freed) */
} ta_wifi_tmpl_parse_context;

/* Initialize the parsing context */
static void
ta_wifi_tmpl_parse_context_init(ta_wifi_tmpl_parse_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

/*
 * Test if the specific option should be ignored
 * for the given node type
 */
static bool
should_ignore(ta_wifi_tmpl_type type, const char *opt)
{

#define MATCH(__a) (strcmp(opt, __a) == 0)
    switch (type)
    {
        case TA_WIFI_TMPL_TYPE_DEVICE:
        {
            return MATCH("channel") ||
                   MATCH("htmode") ||
                   MATCH("hwmode") ||
                   MATCH("wifi_radio_instance") ||
                   MATCH("disabled") ||
                   MATCH("txpower") ||
                   MATCH("frag") ||
                   MATCH("rts");
        }
        case TA_WIFI_TMPL_TYPE_IFACE:
        {
            return MATCH("device") ||
                   MATCH("ifname") ||
                   MATCH("mode") ||
                   MATCH("ssid") ||
                   MATCH("encryption") ||
                   MATCH("key") ||
                   MATCH("wifi_iface_instance") ||
                   MATCH("disabled");
        }
        default:
        {
            break;
        }
    }
#undef MATCH

    return false;
}

/*
 * Process three tokens of the UCI configuration and fill the
 * template data
 */
static te_errno
ta_wifi_process_tokens(ta_wifi_tmpl_parse_context *ctx,
                       ta_wifi_tmpl_data *data,
                       const char *type,
                       const char *opt,
                       const char *value)
{
    if (strcmp(type, "config") == 0)
    {
        /* Create a new template object */
        ctx->current_node = TE_ALLOC(sizeof(ta_wifi_tmpl_node));

        ta_wifi_tmpl_node_init(ctx->current_node);

        ctx->current_node->raw_type = TE_STRDUP(opt);
        ctx->current_node->raw_value = TE_STRDUP(value);

        SLIST_INSERT_HEAD(&data->templates, ctx->current_node, links);
        if (strcmp(opt, "wifi-device") == 0)
        {
            /* This is the wifi-device - remember device from value */
            ctx->current_node->type = TA_WIFI_TMPL_TYPE_DEVICE;
            ctx->current_node->device = TE_STRDUP(value);
            RING("Added WiFi device '%s'", value);
        }
        else if (strcmp(opt, "wifi-iface") == 0)
        {
            /* This is the interface - we'll remember device later */
            ctx->current_node->type = TA_WIFI_TMPL_TYPE_IFACE;
            RING("Added WiFi interface '%s'", value);
        }
        else
        {
            /* This is the other entry - remember options standalone */
            ctx->current_node->type = TA_WIFI_TMPL_TYPE_OTHER;
            RING("Added other section '%s'", value);
        }
    }
    else if (strcmp(type, "option") == 0)
    {
        /* Create a new option */

        if (ctx->current_node == NULL)
        {
            ERROR("Option in an unknown context: '%s'='%s'", opt, value);
            return TE_RC(TE_TA_UNIX, TE_EINVAL);
        }

        if (strcmp(opt, "device") == 0 &&
            ctx->current_node->type == TA_WIFI_TMPL_TYPE_IFACE)
        {
            /* This is the interface, therefore we need to remember device */
            free(ctx->current_node->device);
            ctx->current_node->device = strdup(value);
            RING("Template node device updated: '%s'", value);
        }
        else
        {
            ta_wifi_option *option;

            /* This is the option of the other type - just remember it */
            if (should_ignore(ctx->current_node->type, opt))
            {
                RING("Template option '%s' ignored", opt);
                return 0;
            }

            option = TE_ALLOC(sizeof(ta_wifi_option));
            option->name = TE_STRDUP(opt);
            option->value = TE_STRDUP(value);

            SLIST_INSERT_HEAD(&ctx->current_node->options, option, links);

            RING("Template option '%s'='%s' added", opt, value);
        }
    }

    return 0;
}

/* See the description in ta_wifi_uci.h */
te_errno
ta_wifi_uci_parser_parse(const char *path, ta_wifi_tmpl_data *data)
{
    te_errno ret;
    FILE    *f;
    char     line[MAX_LINE];
    ta_wifi_tmpl_parse_context ctx;

    ta_wifi_tmpl_parse_context_init(&ctx);

    f = fopen(path, "r");
    if (f == NULL)
    {
        int saved_errno = errno;

        ERROR("Failed to open UCI configuration '%s': %s", path,
            strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, saved_errno);
    }

    while (fgets(line, sizeof(line), f))
    {
        int    n = 0;
        char  *tok[3] = {0};
        te_vec linevec = TE_VEC_INIT(char *);
        const char * const *p;

        line[sizeof(line) - 1] = '\0';

        if (te_vec_tokenize_string(line, &linevec, " \t\r\n'") != 0)
            continue;

        if (te_vec_size(&linevec) == 3)
        {
            TE_VEC_FOREACH(&linevec, p)
            {
                tok[n] = (char *)*p;
                n++;
                if (n == 3)
                    break;
            }

            ret = ta_wifi_process_tokens(&ctx, data, tok[0], tok[1], tok[2]);
            if (ret != 0)
                goto err;
        }

        te_vec_deep_free(&linevec);
    }

    ret = 0;
err:
    if (f != NULL)
        fclose(f);
    return ret;
}
