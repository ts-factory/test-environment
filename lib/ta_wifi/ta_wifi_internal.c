/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library
 *
 * Internal WiFi agent library functions and helpers
 */

#define TE_LGR_USER "TA WiFi"

#include "te_config.h"

#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"
#include "te_queue.h"
#include "tq_string.h"
#include "te_enum.h"
#include "rcf_pch.h"
#include "ta_wifi.h"
#include "ta_wifi_internal.h"

/* Main WiFi agent's node */
static ta_wifi wifi_node = {
    .ports = SLIST_HEAD_INITIALIZER(ports),
    .configurator = TAPI_CFG_WIFI_CFG_AUTO,
};

/* See the description in ta_wifi_internal.h */
void
ta_wifi_option_free(ta_wifi_option *option)
{
    if (option != NULL)
    {
        free(option->name);
        free(option->value);
    }
    free(option);
}

/* See the description in ta_wifi_internal.h */
void
ta_wifi_ssid_free(ta_wifi_ssid *ssid)
{
    if (ssid != NULL)
    {
        free(ssid->instance_name);
        free(ssid->name);
        free(ssid->ifname);
        free(ssid->passphrase);
    }
    free(ssid);
}

/* See the description in ta_wifi_internal.h */
void
ta_wifi_port_free(ta_wifi_port *port)
{
    if (port != NULL)
    {
        ta_wifi_ssid *ssid;
        ta_wifi_ssid *ssid_tmp;
        ta_wifi_option *option;
        ta_wifi_option *option_tmp;

        SLIST_FOREACH_SAFE(ssid, &port->ssids, links, ssid_tmp)
        {
            SLIST_REMOVE(&port->ssids, ssid, ta_wifi_ssid, links);
            ta_wifi_ssid_free(ssid);
        }

        SLIST_FOREACH_SAFE(option, &port->options, links, option_tmp)
        {
            SLIST_REMOVE(&port->options, option, ta_wifi_option, links);
            ta_wifi_option_free(option);
        }

        free(port->instance_name);
        free(port->ifname);
    }

    free(port);
}

/* See the description in ta_wifi_internal.h */
ta_wifi_option *
ta_wifi_ssid_find_option(const ta_wifi_ssid *ssid, const char *name)
{
    ta_wifi_option *option;

    if (ssid == NULL)
        return NULL;

    assert(name != NULL);

    SLIST_FOREACH(option, &ssid->options, links)
    {
        assert(option->name != NULL);

        if (strcmp(name, option->name) == 0)
            return option;
    }

    return NULL;
}

/* See the description in ta_wifi_internal.h */
ta_wifi_option *
ta_wifi_port_find_option(const ta_wifi_port *port, const char *name)
{
    ta_wifi_option *option;

    if (port == NULL)
        return NULL;

    assert(name != NULL);

    SLIST_FOREACH(option, &port->options, links)
    {
        assert(option->name != NULL);

        if (strcmp(name, option->name) == 0)
            return option;
    }

    return NULL;
}

/* See the description in ta_wifi_internal.h */
ta_wifi_ssid *
ta_wifi_port_find_ssid(const ta_wifi_port *port, const char *name)
{
    ta_wifi_ssid *ssid;

    if (port == NULL)
        return NULL;

    assert(name != NULL);

    SLIST_FOREACH(ssid, &port->ssids, links)
    {
        assert(ssid->instance_name != NULL);

        if (strcmp(name, ssid->instance_name) == 0)
            return ssid;
    }

    return NULL;
}

/* See the description in ta_wifi_internal.h */
ta_wifi *
ta_wifi_get_node(void)
{
    return &wifi_node;
}

/* See the description in ta_wifi_internal.h */
ta_wifi_port *
ta_wifi_find_port(const char *name)
{
    ta_wifi_port *port;

    assert(name != NULL);

    SLIST_FOREACH(port, &wifi_node.ports, links)
    {
        assert(port->instance_name != NULL);

        if (strcmp(name, port->instance_name) == 0)
            return port;
    }

    return NULL;
}
