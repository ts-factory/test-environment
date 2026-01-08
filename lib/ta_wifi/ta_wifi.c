/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library
 *
 * Basic WiFi agent tree implementation.
 */

#define TE_LGR_USER "TA WiFi"

#include "te_config.h"

#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"
#include "te_queue.h"
#include "tq_string.h"
#include "rcf_pch.h"
#include "ta_wifi.h"
#include "ta_wifi_internal.h"
#include "ta_wifi_uci.h"
#include "ta_wifi_wh.h"

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

static rcf_pch_cfg_object node_wifi;

static te_errno ta_unix_conf_wifi_apply(void);
static te_errno ta_unix_conf_wifi_cancel(void);

static te_errno
node_wifi_ssid_passphrase_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    snprintf(value, RCF_MAX_VAL, "%s",
        ssid->passphrase != NULL ? ssid->passphrase : "");
    return 0;
}

static te_errno
node_wifi_ssid_passphrase_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    free(ssid->passphrase);
    ssid->passphrase = TE_STRDUP(value);
    return 0;
}

static te_errno
node_wifi_ssid_mode_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    te_errno rc;
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%s",
             tapi_cfg_wifi_mode_from_value(ssid->mode));
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_ssid_mode_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;
    int mapped;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    mapped = tapi_cfg_wifi_mode_from_str(value);
    if (mapped < 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    ssid->mode = mapped;
    return 0;
}

static te_errno
node_wifi_ssid_security_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    te_errno rc;
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%s",
        tapi_cfg_wifi_security_from_value(ssid->security));
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_ssid_security_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;
    int mapped;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    mapped = tapi_cfg_wifi_security_from_str(value);
    if (mapped < 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    ssid->security = mapped;
    return 0;
}

static te_errno
node_wifi_ssid_protocol_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    te_errno rc;
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%s",
        tapi_cfg_wifi_protocol_from_value(ssid->protocol));
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_ssid_protocol_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;
    int mapped;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    mapped = tapi_cfg_wifi_protocol_from_str(value);
    if (mapped < 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    ssid->protocol = mapped;
    return 0;
}

static te_errno
node_wifi_ssid_name_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    snprintf(value, RCF_MAX_VAL, "%s",
        ssid->name != NULL ? ssid->name : "");
    return 0;
}

static te_errno
node_wifi_ssid_name_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    free(ssid->name);
    ssid->name = TE_STRDUP(value);
    return 0;
}

static te_errno
node_wifi_ssid_ifname_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    snprintf(value, RCF_MAX_VAL, "%s",
        ssid->ifname != NULL ? ssid->ifname : "");
    return 0;
}

static te_errno
node_wifi_ssid_ifname_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    free(ssid->ifname);
    ssid->ifname = TE_STRDUP(value);
    return 0;
}

static te_errno
node_wifi_ssid_enable_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    sprintf(value, "%d", ssid->enable ? 1 : 0);
    return 0;
}

static te_errno
node_wifi_ssid_enable_set(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    te_errno      ret;
    ta_wifi_ssid *ssid;
    bool          result;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ssid = ta_wifi_port_find_ssid(ta_wifi_find_port(port_name), ssid_name);
    if (ssid == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    ret = te_strtol_bool(value, &result);
    if (ret != 0)
        return ret;

    ssid->enable = result;

    return 0;
}

static te_errno
node_wifi_port_ssid_add(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if (ta_wifi_port_find_ssid(port, ssid_name) != NULL)
        return TE_RC(TE_TA_UNIX, TE_EEXIST);

    ssid = TE_ALLOC(sizeof(*ssid));

    ssid->instance_name = TE_STRDUP(ssid_name);
    ssid->security = TAPI_CFG_WIFI_SECURITY_WPA2;
    ssid->protocol = TAPI_CFG_WIFI_PROTOCOL_CCMP;

    SLIST_INSERT_HEAD(&port->ssids, ssid, links);
    return 0;
}

static te_errno
node_wifi_port_ssid_del(unsigned int gid, const char *oid,
    const char *empty, const char *port_name, const char *ssid_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((ssid = ta_wifi_port_find_ssid(port, ssid_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    SLIST_REMOVE(&port->ssids, ssid, ta_wifi_ssid, links);

    ta_wifi_ssid_free(ssid);
    return 0;
}

static te_errno
node_wifi_port_ssid_list(unsigned int gid, const char *oid, const char *sub_id,
    char **list, const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;
    te_string str = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    SLIST_FOREACH(ssid, &port->ssids, links)
    {
        te_string_append(&str, "%s%s",
                         (str.ptr != NULL) ? " " : "", ssid->instance_name);
    }

    *list = str.ptr;
    return 0;
}

/** Set custom option value */
static te_errno
node_wifi_port_option_value_set(unsigned int gid, const char *oid,
    const char *value, const char *empty, const char *port_name,
    const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((option = ta_wifi_port_find_option(port, option_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    free(option->value);
    option->value = TE_STRDUP(value);

    return 0;
}

/** Get custom option value */
static te_errno
node_wifi_port_option_value_get(unsigned int gid, const char *oid, char *value,
                      const char *empty,
                      const char *port_name,
                      const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((option = ta_wifi_port_find_option(port, option_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    strcpy(value, option->value);

    return 0;
}

static te_errno
node_wifi_port_option_add(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name, const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if (ta_wifi_port_find_option(port, option_name) != NULL)
        return TE_RC(TE_TA_UNIX, TE_EEXIST);

    option = TE_ALLOC(sizeof(*option));

    option->name = TE_STRDUP(option_name);
    option->value = TE_STRDUP(value);

    SLIST_INSERT_HEAD(&port->options, option, links);

    return 0;
}

static te_errno
node_wifi_port_option_del(unsigned int gid, const char *oid,
    const char *empty, const char *port_name, const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((option = ta_wifi_port_find_option(port, option_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    SLIST_REMOVE(&port->options, option, ta_wifi_option, links);

    ta_wifi_option_free(option);

    return 0;
}

static te_errno
node_wifi_port_option_list(unsigned int gid, const char *oid,
    const char *sub_id, char **list, const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    ta_wifi_option *option;
    te_string str = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    SLIST_FOREACH(option, &port->options, links)
    {
        te_string_append(&str, "%s%s",
                         (str.ptr != NULL) ? " " : "", option->name);
    }

    *list = str.ptr;
    return 0;
}


/** Set custom option value */
static te_errno
node_wifi_ssid_option_value_set(unsigned int gid, const char *oid,
    const char *value, const char *empty, const char *port_name,
    const char *ssid_name, const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((ssid = ta_wifi_port_find_ssid(port, ssid_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((option = ta_wifi_ssid_find_option(ssid, option_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    free(option->value);
    option->value = TE_STRDUP(value);

    return 0;
}

/** Get custom option value */
static te_errno
node_wifi_ssid_option_value_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name, const char *ssid_name,
    const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((ssid = ta_wifi_port_find_ssid(port, ssid_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((option = ta_wifi_ssid_find_option(ssid, option_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    strcpy(value, option->value);

    return 0;
}

static te_errno
node_wifi_ssid_option_add(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name, const char *ssid_name,
    const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((ssid = ta_wifi_port_find_ssid(port, ssid_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if (ta_wifi_ssid_find_option(ssid, option_name) != NULL)
        return TE_RC(TE_TA_UNIX, TE_EEXIST);

    option = TE_ALLOC(sizeof(*option));

    option->name = TE_STRDUP(option_name);
    option->value = TE_STRDUP(value);

    SLIST_INSERT_HEAD(&ssid->options, option, links);

    return 0;
}

static te_errno
node_wifi_ssid_option_del(unsigned int gid, const char *oid, const char *empty,
    const char *port_name, const char *ssid_name, const char *option_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;
    ta_wifi_option *option;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((ssid = ta_wifi_port_find_ssid(port, ssid_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((option = ta_wifi_ssid_find_option(ssid, option_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    SLIST_REMOVE(&ssid->options, option, ta_wifi_option, links);

    ta_wifi_option_free(option);

    return 0;
}

static te_errno
node_wifi_ssid_option_list(unsigned int gid, const char *oid,
    const char *sub_id, char **list, const char *empty, const char *port_name,
    const char *ssid_name)
{
    ta_wifi_port *port;
    ta_wifi_ssid *ssid;
    ta_wifi_option *option;
    te_string str = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);
    UNUSED(empty);

    if ((port = ta_wifi_find_port(port_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if ((ssid = ta_wifi_port_find_ssid(port, ssid_name)) == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    SLIST_FOREACH(option, &ssid->options, links)
    {
        te_string_append(&str, "%s%s",
                         (str.ptr != NULL) ? " " : "", option->name);
    }

    *list = str.ptr;
    return 0;
}

static te_errno
node_wifi_port_channel_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ENTRY("%s", port_name);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%u", (unsigned int)port->channel);
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_port_channel_set(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ENTRY("%s", port_name);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_strtou_size(value, 0, &port->channel, sizeof(port->channel));

    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_port_enable_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ENTRY("%s", port_name);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%u", port->enable ? 1U : 0U);
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_port_enable_set(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ENTRY("%s", port_name);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_strtou_size(value, 0, &port->enable, sizeof(port->enable));

    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}


static te_errno
node_wifi_configurator_get(unsigned int gid, const char *oid, char *value,
    const char *empty)
{
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    rc = te_snprintf(value, RCF_MAX_VAL, "%s",
        tapi_cfg_wifi_configurator_from_value(
            ta_wifi_get_node()->configurator));
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_configurator_set(unsigned int gid, const char *oid,
    const char *value, const char *empty)
{
    int mapped;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    mapped = tapi_cfg_wifi_configurator_from_str(value);
    if (mapped < 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    ta_wifi_get_node()->configurator = mapped;
    return 0;
}

static te_errno
node_wifi_enable_get(unsigned int gid, const char *oid, char *value,
    const char *empty)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    sprintf(value, "%d", ta_wifi_get_node()->enable ? 1 : 0);

    return 0;
}

static te_errno
node_wifi_enable_set(unsigned int gid, const char *oid,
    const char *value, const char *empty)
{
    te_errno ret;
    bool     result;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ret = te_strtol_bool(value, &result);
    if (ret != 0)
        return ret;

    ta_wifi_get_node()->enable = result;

    return 0;
}

static te_errno
node_wifi_status_get(unsigned int gid, const char *oid, char *value,
    const char *empty)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    sprintf(value, "%d", ta_wifi_get_node()->status ? 1 : 0);

    return 0;
}

static te_errno
node_wifi_port_ifname_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    if (port->ifname != NULL)
        strcpy(value, port->ifname);
    else
        strcpy(value, "");

    return 0;
}

static te_errno
node_wifi_port_ifname_set(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    free(port->ifname);
    port->ifname = TE_STRDUP(value);

    return 0;
}

static te_errno
node_wifi_port_standard_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%s",
        tapi_cfg_wifi_standard_from_value(port->standard));
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_port_standard_set(unsigned int gid, const char *oid,
    const char *value, const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    int mapped;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    mapped = tapi_cfg_wifi_standard_from_str(value);
    if (mapped < 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    port->standard = mapped;

    return 0;
}

static te_errno
node_wifi_port_width_get(unsigned int gid, const char *oid, char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    te_errno rc;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    rc = te_snprintf(value, RCF_MAX_VAL, "%s",
        tapi_cfg_wifi_width_from_value(port->width));
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);
}

static te_errno
node_wifi_port_width_set(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *port_name)
{
    ta_wifi_port *port;
    int mapped;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    port = ta_wifi_find_port(port_name);
    if (port == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);

    mapped = tapi_cfg_wifi_width_from_str(value);
    if (mapped < 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    port->width = mapped;

    return 0;
}

static te_errno
port_add(unsigned int gid, const char *oid, const char *value,
    const char *empty, const char *name)
{
    ta_wifi_port *port;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(empty);

    ENTRY("%s", name);

    port = ta_wifi_find_port(name);
    if (port != NULL)
    {
        ERROR("WiFi port with such name already exists: '%s'", name);
        return TE_RC(TE_TA_UNIX, TE_EEXIST);
    }

    port = TE_ALLOC(sizeof(*port));

    port->instance_name = TE_STRDUP(name);
    port->standard = TAPI_CFG_WIFI_STANDARD_G;
    SLIST_INIT(&port->ssids);

    SLIST_INSERT_HEAD(&ta_wifi_get_node()->ports, port, links);

    return 0;
}

static te_errno
port_del(unsigned int gid, const char *oid, const char *empty, const char *name)
{
    ta_wifi_port *port;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(empty);

    ENTRY("%s", name);

    port = ta_wifi_find_port(name);
    if (port == NULL)
    {
        ERROR("Instance with such name doesn't exist: '%s'", name);
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    }

    SLIST_REMOVE(&ta_wifi_get_node()->ports, port, ta_wifi_port, links);

    ta_wifi_port_free(port);
    return 0;
}

static te_errno
port_list(unsigned int gid, const char *oid, const char *sub_id, char **list)
{
    ta_wifi_port *port;
    te_string str = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);

    SLIST_FOREACH(port, &ta_wifi_get_node()->ports, links)
    {
        te_string_append(&str, "%s%s",
                         (str.ptr != NULL) ? " " : "", port->instance_name);
    }

    *list = str.ptr;
    return 0;
}


RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_option_value, "value",
                     NULL, NULL,
                     node_wifi_ssid_option_value_get,
                     node_wifi_ssid_option_value_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_COLLECTION(node_wifi_ssid_option, "option",
                            &node_wifi_ssid_option_value, NULL,
                            node_wifi_ssid_option_add,
                            node_wifi_ssid_option_del,
                            node_wifi_ssid_option_list, NULL);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_passphrase, "passphrase",
                     NULL, &node_wifi_ssid_option,
                     node_wifi_ssid_passphrase_get,
                     node_wifi_ssid_passphrase_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_protocol, "protocol",
                     NULL, &node_wifi_ssid_passphrase,
                     node_wifi_ssid_protocol_get, node_wifi_ssid_protocol_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_security, "security",
                     NULL, &node_wifi_ssid_protocol,
                     node_wifi_ssid_security_get, node_wifi_ssid_security_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_mode, "mode",
                     NULL, &node_wifi_ssid_security,
                     node_wifi_ssid_mode_get, node_wifi_ssid_mode_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_name, "name",
                     NULL, &node_wifi_ssid_mode,
                     node_wifi_ssid_name_get, node_wifi_ssid_name_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_ifname, "ifname",
                     NULL, &node_wifi_ssid_name,
                     node_wifi_ssid_ifname_get, node_wifi_ssid_ifname_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_ssid_enable, "enable",
                     NULL, &node_wifi_ssid_ifname,
                     node_wifi_ssid_enable_get, node_wifi_ssid_enable_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_COLLECTION(node_wifi_ssid, "ssid",
                            &node_wifi_ssid_enable, NULL,
                            node_wifi_port_ssid_add,
                            node_wifi_port_ssid_del,
                            node_wifi_port_ssid_list, NULL);

RCF_PCH_CFG_NODE_RWC(node_wifi_option_value, "value",
                     NULL, NULL,
                     node_wifi_port_option_value_get,
                     node_wifi_port_option_value_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_COLLECTION(node_wifi_port_option, "option",
                            &node_wifi_option_value, &node_wifi_ssid,
                            node_wifi_port_option_add,
                            node_wifi_port_option_del,
                            node_wifi_port_option_list, NULL);

#define UINT32_VAL_NODE(__name, __next)                             \
static te_errno                                                     \
node_wifi_port_ ##__name## _get(unsigned int gid, const char *oid,  \
    char *value, const char *empty, const char *port_name)          \
{                                                                   \
    ta_wifi_port *port;                                             \
    te_errno rc;                                                    \
                                                                    \
    UNUSED(gid);                                                    \
    UNUSED(oid);                                                    \
    UNUSED(empty);                                                  \
                                                                    \
    ENTRY("%s", port_name);                                         \
                                                                    \
    port = ta_wifi_find_port(port_name);                            \
    if (port == NULL)                                               \
        return TE_RC(TE_TA_UNIX, TE_ENOENT);                        \
                                                                    \
    rc = te_snprintf(value, RCF_MAX_VAL, "%u",                      \
             (unsigned int)port->__name);                           \
                                                                    \
    return TE_RC_UPSTREAM(TE_TA_UNIX, rc);                          \
}                                                                   \
                                                                    \
static te_errno                                                     \
node_wifi_port_ ##__name## _set(unsigned int gid, const char *oid,  \
    const char *value, const char *empty, const char *port_name)    \
{                                                                   \
    ta_wifi_port *port;                                             \
    te_errno rc;                                                    \
    uint16_t val;                                                   \
                                                                    \
    UNUSED(gid);                                                    \
    UNUSED(oid);                                                    \
    UNUSED(empty);                                                  \
                                                                    \
    ENTRY("%s", port_name);                                         \
                                                                    \
    port = ta_wifi_find_port(port_name);                            \
    if (port == NULL)                                               \
        return TE_RC(TE_TA_UNIX, TE_ENOENT);                        \
                                                                    \
    rc = te_strtou_size(value, 0, &val, sizeof(val));               \
    if (rc != 0)                                                    \
        return TE_RC(TE_TA_UNIX, rc);                               \
                                                                    \
    port->__name = val;                                             \
                                                                    \
    return 0;                                                       \
}                                                                   \
                                                                    \
RCF_PCH_CFG_NODE_RWC(node_wifi_port_ ## __name, #__name,            \
                     NULL, &node_wifi_port_ ##__next,               \
                     node_wifi_port_ ##__name## _get,               \
                     node_wifi_port_ ##__name## _set,               \
                     &node_wifi);


UINT32_VAL_NODE(max_nss, option);
UINT32_VAL_NODE(tx_power, max_nss);
UINT32_VAL_NODE(txop_limit, tx_power);
UINT32_VAL_NODE(contention_window_max, txop_limit);
UINT32_VAL_NODE(contention_window_min, contention_window_max);
UINT32_VAL_NODE(aifs, contention_window_min);
UINT32_VAL_NODE(guard_interval, aifs);
UINT32_VAL_NODE(long_retry_limit, guard_interval);
UINT32_VAL_NODE(short_retry_limit, long_retry_limit);
UINT32_VAL_NODE(rts_threshold, short_retry_limit);
UINT32_VAL_NODE(frag_threshold, rts_threshold);
UINT32_VAL_NODE(max_a_mpdu, frag_threshold);
UINT32_VAL_NODE(max_a_msdu, max_a_mpdu);

#undef UINT32_VAL_NODE

RCF_PCH_CFG_NODE_RWC(node_wifi_port_channel, "channel",
                     NULL, &node_wifi_port_max_a_msdu,
                     node_wifi_port_channel_get, node_wifi_port_channel_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_port_standard, "standard",
                     NULL, &node_wifi_port_channel,
                     node_wifi_port_standard_get, node_wifi_port_standard_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_port_width, "width",
                     NULL, &node_wifi_port_standard,
                     node_wifi_port_width_get, node_wifi_port_width_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_port_ifname, "ifname",
                     NULL, &node_wifi_port_width,
                     node_wifi_port_ifname_get, node_wifi_port_ifname_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_port_enable, "enable",
                     NULL, &node_wifi_port_ifname,
                     node_wifi_port_enable_get, node_wifi_port_enable_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_COLLECTION(node_wifi_port, "port",
                            &node_wifi_port_enable, NULL,
                            port_add, port_del, port_list, NULL);

RCF_PCH_CFG_NODE_RO(node_wifi_status, "status",
                    NULL, &node_wifi_port,
                    node_wifi_status_get);

RCF_PCH_CFG_NODE_RWC(node_wifi_enable, "enable",
                     NULL, &node_wifi_status,
                     node_wifi_enable_get, node_wifi_enable_set,
                     &node_wifi);

RCF_PCH_CFG_NODE_RWC(node_wifi_configurator, "configurator",
                     NULL, &node_wifi_enable,
                     node_wifi_configurator_get, node_wifi_configurator_set,
                     &node_wifi);

/* Commit WiFi node */
static te_errno
node_wifi_commit(unsigned int gid, const cfg_oid *p_oid)
{
    te_errno ret;

    ta_unix_conf_wifi_cancel();
    ta_wifi_get_node()->status = 0;

    if (ta_wifi_get_node()->enable)
    {
        ret = ta_unix_conf_wifi_apply();
        ta_wifi_get_node()->status = (ret == 0);
    }

    return 0;
}

RCF_PCH_CFG_NODE_NA_COMMIT(node_wifi, "wifi", &node_wifi_configurator, NULL,
                           node_wifi_commit);

/* Infer the configurator from the system */
static tapi_cfg_wifi_configurator
infer_configurator(tapi_cfg_wifi_configurator cfg)
{
    switch (cfg)
    {
        case TAPI_CFG_WIFI_CFG_AUTO:
        {
            struct stat st;

            if (stat("/sbin/uci", &st) == 0)
            {
                return TAPI_CFG_WIFI_CFG_UCI;
            }

            return TAPI_CFG_WIFI_CFG_HOSTAPD_WPA_SUPPLICANT;
        }
        default:
        {
            return cfg;
        }
    }
}

/* Apply WiFi configuration */
static te_errno
ta_unix_conf_wifi_apply(void)
{
    te_errno rc;

    rc = ta_unix_conf_wifi_cancel();
    if (rc != 0)
        return rc;

    if (ta_wifi_get_node()->enable)
    {
        switch (infer_configurator(ta_wifi_get_node()->configurator))
        {
            case TAPI_CFG_WIFI_CFG_UCI:
            {
                rc = ta_wifi_uci_apply(ta_wifi_get_node());
                if (rc != 0)
                    return rc;

                return 0;
            }
            case TAPI_CFG_WIFI_CFG_HOSTAPD_WPA_SUPPLICANT:
            {
                rc = ta_wifi_wh_apply(ta_wifi_get_node());
                if (rc != 0)
                    return rc;

                return 0;
            }
            default:
            {
                /* not implemented */
                return TE_ENOSYS;
            }
        }
    }

    return 0;
}

/* Cancel WiFi configuration */
static te_errno
ta_unix_conf_wifi_cancel(void)
{
    switch (infer_configurator(ta_wifi_get_node()->configurator))
    {
        case TAPI_CFG_WIFI_CFG_UCI:
            return ta_wifi_uci_cancel(ta_wifi_get_node());
        case TAPI_CFG_WIFI_CFG_HOSTAPD_WPA_SUPPLICANT:
            return ta_wifi_wh_cancel(ta_wifi_get_node());
        default:
            /* not implemented */
            return TE_ENOSYS;
    }
}

/* See the description in ta_wifi.h */
te_errno
ta_unix_conf_wifi_init(void)
{
    te_errno rc;

    rc = rcf_pch_rsrc_info("/agent/wifi",
                           rcf_pch_rsrc_grab_dummy,
                           rcf_pch_rsrc_release_dummy);
    if (rc != 0)
        return rc;

    rc = rcf_pch_add_node("/agent", &node_wifi);
    if (rc != 0)
        return rc;

    return 0;
}
