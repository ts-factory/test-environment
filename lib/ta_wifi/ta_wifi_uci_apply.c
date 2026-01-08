/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library - UCI support
 *
 * The library provides ability to write UCI configurations
 */

#define TE_LGR_USER "TA WiFi UCI Apply"

#include "ta_wifi.h"
#include "ta_wifi_internal.h"
#include "ta_wifi_uci.h"
#include "logger_api.h"
#include "agentlib.h"
#include "te_enum.h"
#include "te_str.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_SYS_STAT_H)
#include <sys/stat.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

/** UCI wireless configuration */
#define OPENWRT_CONFIG "/etc/config/wireless"

/** Size of ht_mode field in bytes */
#define HT_MODE_SIZE (3 + 3 + 1)

/** Configuration context */
typedef struct ta_wifi_cfg_context {
    FILE   *f;                      /**< File where to write data */
    int     radio_instance;         /**< Current radio instance */
    int     iface_instance;         /**< Current interface instance */
    ta_wifi_port      *port;        /**< Currently processed port */
    ta_wifi_tmpl_data *tmpl_data;   /**< Template data */
} ta_wifi_cfg_context;

/** Initialize configuration context */
#define TA_WIFI_CFG_CONTEXT_INIT() {        \
        .f = NULL,                          \
        .radio_instance = 0,                \
        .iface_instance = 0,                \
        .port = NULL,                       \
        .tmpl_data = NULL,                  \
    }

extern const te_enum_map wifi_htmode_mapping[];

/* AP modes */
static const char *wifi_mode2str[] = {
    [TAPI_CFG_WIFI_MODE_AP] = "ap",
    [TAPI_CFG_WIFI_MODE_STA] = "sta",
};

static void
infer_ht_mode(tapi_cfg_wifi_standard std, tapi_cfg_wifi_width width,
              char *htmode)
{
    switch (std)
    {
        default:
        case TAPI_CFG_WIFI_STANDARD_G:
        case TAPI_CFG_WIFI_STANDARD_N:
            strcpy(htmode, "HT");
            break;
        case TAPI_CFG_WIFI_STANDARD_AC:
            strcpy(htmode, "VHT");
            break;
        case TAPI_CFG_WIFI_STANDARD_AX:
            strcpy(htmode, "HE");
            break;
        case TAPI_CFG_WIFI_STANDARD_BE:
            strcpy(htmode, "EHT");
            break;
    }

    sprintf(htmode + strlen(htmode), "%d", width);
}

/* HW modes */
static const char *wifi_standard2hwmode[] = {
    [TAPI_CFG_WIFI_STANDARD_G] = "11g",
    [TAPI_CFG_WIFI_STANDARD_N] = "11n",
    [TAPI_CFG_WIFI_STANDARD_AC] = "11ac",
    [TAPI_CFG_WIFI_STANDARD_AX] = "11ax",
    [TAPI_CFG_WIFI_STANDARD_BE] = "11be",
};

/* Security modes */
static const char *wifi_security2enc[] = {
    [TAPI_CFG_WIFI_SECURITY_OPEN] = "none",
    [TAPI_CFG_WIFI_SECURITY_WEP]  = "wep",
    [TAPI_CFG_WIFI_SECURITY_WPA]  = "psk",
    [TAPI_CFG_WIFI_SECURITY_WPA2] = "psk2",
    [TAPI_CFG_WIFI_SECURITY_WPA3] = "sae-mixed",
};

/* fprintf that would propagate error to TE */
#define CHECKED_FPRINTF(__x...) do {                                        \
        int ___retval = fprintf(__x);                                       \
        if (___retval < 0)                                                  \
        {                                                                   \
            int __err = errno;                                              \
            ERROR("Failed to write configuration: %s", strerror(__err));    \
            ret = TE_OS_RC(TE_TA_UNIX, __err);                              \
            goto err;                                                       \
        }                                                                   \
    } while (0)

/* Write ssid to wireless configuration */
static te_errno
ta_wifi_uci_apply_ssid(ta_wifi_cfg_context *ctx, ta_wifi_ssid *node)
{
    ta_wifi_tmpl_node *tmpl_node;
    ta_wifi_option    *option;
    te_errno           ret;

    assert(ctx != NULL);
    assert(ctx->f != NULL);
    assert(node != NULL);

    ctx->iface_instance++;

    tmpl_node = ta_wifi_tmpl_data_get_tmpl_by_device(ctx->tmpl_data,
        TA_WIFI_TMPL_TYPE_IFACE, ctx->port->ifname);

    if (tmpl_node != NULL)
        RING("Found template node for SSID '%s'", node->instance_name);
    else
        WARN("No template node for SSID '%s'", node->instance_name);

    CHECKED_FPRINTF(ctx->f, "config wifi-iface '%s'\n", node->instance_name);
    if (!ctx->port->enable || !node->enable)
        CHECKED_FPRINTF(ctx->f, "\toption disabled '1'\n");
    if (ctx->port->ifname != NULL)
        CHECKED_FPRINTF(ctx->f, "\toption device '%s'\n", ctx->port->ifname);
    if (node->ifname != NULL && strlen(node->ifname) != 0)
        CHECKED_FPRINTF(ctx->f, "\toption ifname '%s'\n", node->ifname);
    CHECKED_FPRINTF(ctx->f, "\toption mode '%s'\n", wifi_mode2str[node->mode]);
    CHECKED_FPRINTF(ctx->f, "\toption ssid '%s'\n", node->name);
    CHECKED_FPRINTF(ctx->f, "\toption encryption '%s'\n",
        wifi_security2enc[node->security]);
    if (!te_str_is_null_or_empty(node->passphrase))
        CHECKED_FPRINTF(ctx->f, "\toption key '%s'\n", node->passphrase);
    CHECKED_FPRINTF(ctx->f, "\toption wifi_iface_instance '%d'\n",
        ctx->iface_instance);

    SLIST_FOREACH(option, &node->options, links)
    {
        assert(option->name != NULL);
        assert(option->value != NULL);

        CHECKED_FPRINTF(ctx->f, "\toption %s '%s'\n", option->name,
            option->value);
    }

    if (tmpl_node != NULL)
    {
        /*
         * Go over template options and add them as long as they don't conflict
         * with our options.
         */
        SLIST_FOREACH(option, &tmpl_node->options, links)
        {
            ta_wifi_option *option2;
            bool            add = true;

            assert(option->name != NULL);
            assert(option->value != NULL);

            SLIST_FOREACH(option2, &node->options, links)
            {
                assert(option2->name != NULL);
                assert(option2->value != NULL);

                if (strcmp(option->name, option2->name) == 0)
                {
                    add = false;
                    break;
                }
            }
            if (add)
            {
                CHECKED_FPRINTF(ctx->f, "\toption %s '%s'\n", option->name,
                    option->value);
                RING("Adding option '%s' to SSID configuration", option->name);
            }
            else
            {
                RING("Template option '%s' is skipped because it is present "
                     "among node options", option->name);
            }
        }
    }

    ret = 0;
err:
    return ret;
}

/* Write port to wireless configuration */
static te_errno
ta_wifi_uci_apply_port(ta_wifi_cfg_context *ctx, ta_wifi_port *node)
{
    te_errno               ret;
    ta_wifi_ssid          *ssid;
    ta_wifi_option        *option;
    ta_wifi_tmpl_node     *tmpl_node;
    char                   htmode[HT_MODE_SIZE];

    assert(ctx != NULL);
    assert(ctx->f != NULL);
    assert(node != NULL);

    ctx->radio_instance++;
    ctx->port = node;

    tmpl_node = ta_wifi_tmpl_data_get_tmpl_by_device(ctx->tmpl_data,
        TA_WIFI_TMPL_TYPE_DEVICE, node->ifname);

    if (tmpl_node != NULL)
        RING("Found template node for port '%s'", node->ifname);
    else
        WARN("No template node for port '%s'", node->ifname);

    CHECKED_FPRINTF(ctx->f, "config wifi-device '%s'\n", node->ifname);
    if (!node->enable)
        CHECKED_FPRINTF(ctx->f, "\toption disabled '1'\n");
    if (node->channel == 0)
        CHECKED_FPRINTF(ctx->f, "\toption channel 'auto'\n");
    else
        CHECKED_FPRINTF(ctx->f, "\toption channel '%d'\n", node->channel);

    if (node->width != TAPI_CFG_WIFI_WIDTH_NOT_SET)
    {
        infer_ht_mode(node->standard, node->width, htmode);
        CHECKED_FPRINTF(ctx->f, "\toption htmode '%s'\n", htmode);
    }
    CHECKED_FPRINTF(ctx->f, "\toption hwmode '%s'\n",
        wifi_standard2hwmode[node->standard]);
    CHECKED_FPRINTF(ctx->f, "\toption wifi_radio_instance '%d'\n",
        ctx->radio_instance);
    if (node->tx_power != 0)
        CHECKED_FPRINTF(ctx->f, "\toption txpower '%u'\n",
            (unsigned)node->tx_power);
    if (node->frag_threshold != 0)
        CHECKED_FPRINTF(ctx->f, "\toption frag '%u'\n",
            (unsigned)node->frag_threshold);
    if (node->rts_threshold != 0)
        CHECKED_FPRINTF(ctx->f, "\toption rts '%u'\n",
            (unsigned)node->rts_threshold);

    SLIST_FOREACH(option, &node->options, links)
    {
        CHECKED_FPRINTF(ctx->f, "\toption %s '%s'\n", option->name,
            option->value);
    }

    if (tmpl_node != NULL)
    {
        /*
         * Go over template options and add them as long as they don't conflict
         * with our options.
         */
        SLIST_FOREACH(option, &tmpl_node->options, links)
        {
            ta_wifi_option *option2;
            bool            add = true;

            assert(option->name != NULL);
            assert(option->value != NULL);

            SLIST_FOREACH(option2, &node->options, links)
            {
                assert(option2->name != NULL);
                assert(option2->value != NULL);

                if (strcmp(option->name, option2->name) == 0)
                {
                    add = false;
                    break;
                }
            }
            if (add)
            {
                CHECKED_FPRINTF(ctx->f, "\toption %s '%s'\n", option->name,
                    option->value);
                RING("Adding option '%s' to port configuration", option->name);
            }
            else
            {
                RING("Template option '%s' is skipped because it is present "
                     "among node options", option->name);
            }
        }
    }

    SLIST_FOREACH(ssid, &node->ssids, links)
    {
        CHECKED_FPRINTF(ctx->f, "\n");
        ret = ta_wifi_uci_apply_ssid(ctx, ssid);
        if (ret != 0)
            goto err;
    }

    ret = 0;

err:
    return ret;
}

/* Write other sections to wireless configuration */
static te_errno
ta_wifi_uci_apply_other(ta_wifi_cfg_context *ctx)
{
    te_errno               ret = 0;
    ta_wifi_tmpl_node     *tmpl_node;
    ta_wifi_option        *option;

    assert(ctx != NULL);
    assert(ctx->f != NULL);

    if (ctx->tmpl_data == NULL)
        return 0;

    SLIST_FOREACH(tmpl_node, &ctx->tmpl_data->templates, links)
    {
        if (tmpl_node->type != TA_WIFI_TMPL_TYPE_OTHER)
            continue;

        CHECKED_FPRINTF(ctx->f, "config %s '%s'\n",
            tmpl_node->raw_type,
            tmpl_node->raw_value);

        SLIST_FOREACH(option, &tmpl_node->options, links)
        {
            CHECKED_FPRINTF(ctx->f, "\toption %s '%s'\n", option->name,
                option->value);
        }
    }

    ret = 0;

err:
    return ret;
}

/* See the description in ta_wifi_uci.h */
te_errno
ta_wifi_uci_apply(ta_wifi *node)
{
    te_errno             ret;
    ta_wifi_port        *port;
    ta_wifi_cfg_context  ctx = TA_WIFI_CFG_CONTEXT_INIT();
    ta_wifi_tmpl_data    tmpl_data;

    assert(node != NULL);

    ta_wifi_tmpl_data_init(&tmpl_data);
    ret = ta_wifi_uci_parser_parse(OPENWRT_CONFIG,
        &tmpl_data);
    if (ret != 0)
    {
        ERROR("Failed to parse UCI configuration");
        goto err;
    }

    ctx.tmpl_data = &tmpl_data;

    ctx.f = fopen(OPENWRT_CONFIG, "w");
    if (ctx.f == NULL)
        return TE_OS_RC(TE_TA_UNIX, errno);

    /* Apply ports */
    SLIST_FOREACH(port, &node->ports, links)
    {
        ret = ta_wifi_uci_apply_port(&ctx, port);
        if (ret != 0)
            goto err;
    }

    /* Apply other sections */
    ret = ta_wifi_uci_apply_other(&ctx);
    if (ret != 0)
        goto err;

    fclose(ctx.f);
    ctx.f = NULL;

    /* Restart WiFi */
    if (ta_system("wifi up") != 0)
    {
        ERROR("Failed to restart WiFi");
        ret = TE_RC(TE_TA_UNIX, TE_ESHCMD);
        goto err;
    }
    else
    {
        RING("WiFi is restarted successfully");
    }

    ret = 0;

err:
    ta_wifi_tmpl_data_fini(&tmpl_data);
    if (ctx.f != NULL)
        fclose(ctx.f);

    return ret;
}

/* See the description in ta_wifi_uci.h */
te_errno
ta_wifi_uci_cancel(ta_wifi *node)
{
    UNUSED(node);

    ta_system("wifi down");

    return 0;
}
