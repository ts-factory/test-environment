/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2025 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief WiFi agent library - WPA supplicant/hostapd support
 *
 * The library provides ability to write & apply WPA supplicant configuration
 */

#define TE_LGR_USER "TA WiFi WPAS/HA"

#include "ta_wifi.h"
#include "ta_wifi_internal.h"
#include "ta_wifi_uci.h"
#include "te_str.h"
#include "logger_api.h"
#include "agentlib.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_SYS_STAT_H)
#include <sys/stat.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

/** Path to wpa_supplicant */
#define WPA_SUPPLICANT_PATH     "/usr/sbin/wpa_supplicant"
/** WPA supplicant configuration format */
#define WPA_SUPPLICANT_CONF_FMT "/tmp/wpa_supplicant.%s.conf"
/** WPA supplicant pid file format */
#define WPA_SUPPLICANT_PID_FMT  "/var/run/wpa_supplicant.%s.pid"

/** Path to hostapd */
#define HOSTAPD_PATH            "/usr/sbin/hostapd"
/** hostapd configuration format */
#define HOSTAPD_CONF_FMT        "/tmp/hostapd.%s.conf"
/** hostapd pid file format */
#define HOSTAPD_PID_FMT         "/var/run/hostapd.%s.pid"

#define FILE_PATH_SIZE  (200)
#define CMD_SIZE        (500)

/** WPA supplicant/hostapd configuration generation context */
typedef struct ta_wifi_wh_context {
    FILE         *f;    /**< File where to put the context data */
    int           ssid_instance; /**< Index of the SSID within current port */
    ta_wifi_port *port; /**< WiFi port */
} ta_wifi_wh_context;

/* WPA supplicant/hostapd context initializer */
#define TA_WH_CONTEXT_INIT() { \
    NULL, NULL \
}

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

/*
 * Convert channel to an actual frequency
 *
 * @param standard Target WiFi standard
 * @param ch       Target channel
 *
 * @return The frequency, or @c -1 in case of unspecified conversion.
 */
static int
wifi_channel_to_freq(tapi_cfg_wifi_standard standard, unsigned ch)
{
    if (ch <= 0)
        return -1;

    switch (standard)
    {
        case TAPI_CFG_WIFI_STANDARD_G:
        case TAPI_CFG_WIFI_STANDARD_N:
        {
            if (ch >= 1 && ch <= 13)
                return 2412 + 5 * (ch - 1);
            if (ch == 14)
                return 2484;
            return -1;
        }

        case TAPI_CFG_WIFI_STANDARD_AC:
        case TAPI_CFG_WIFI_STANDARD_AX:
        {
            if (ch >= 7 && ch <= 196)
                return 5000 + 5 * ch;
            return -1;
        }
        case TAPI_CFG_WIFI_STANDARD_BE:
        {
            if (ch >= 1 && ch <= 233)
                return 5950 + 5 * ch;
            return -1;
        }
        default:
        {
            return -1;
        }
    }
}

/* Write out SSID to wpa supplicant configuration */
static te_errno
ta_wifi_wh_apply_wpas_ssid(ta_wifi_wh_context *ctx, ta_wifi_ssid *ssid)
{
    te_errno      ret;
    ta_wifi_port *port;
    int           freq;

    assert(ctx != NULL);
    assert(ctx->port != NULL);
    assert(ssid != NULL);
    assert(ssid->name != NULL);

    port = ctx->port;

    CHECKED_FPRINTF(ctx->f, "network={\n");
    CHECKED_FPRINTF(ctx->f, "ssid=\"%s\"\n", ssid->name);

    if (ssid->security == TAPI_CFG_WIFI_SECURITY_OPEN)
    {
        CHECKED_FPRINTF(ctx->f, "key_mgmt=NONE\n");
    }
    else if (ssid->security == TAPI_CFG_WIFI_SECURITY_WPA3 ||
             port->standard == TAPI_CFG_WIFI_STANDARD_BE)
    {
        CHECKED_FPRINTF(ctx->f, "key_mgmt=SAE\n");
        CHECKED_FPRINTF(ctx->f, "sae_password=\"%s\"\n", ssid->passphrase);
        CHECKED_FPRINTF(ctx->f, "ieee80211w=2\n");
        CHECKED_FPRINTF(ctx->f, "proto=RSN\n");
    }
    else if (ssid->security == TAPI_CFG_WIFI_SECURITY_WEP)
    {
        CHECKED_FPRINTF(ctx->f, "key_mgmt=NONE\n");
        CHECKED_FPRINTF(ctx->f, "wep_key0=\"%s\"\n", ssid->passphrase);
        CHECKED_FPRINTF(ctx->f, "wep_tx_keyidx=0\n");
    }
    else
    {
        /* PSK variants */
        CHECKED_FPRINTF(ctx->f, "key_mgmt=WPA-PSK\n");
        CHECKED_FPRINTF(ctx->f, "psk=\"%s\"\n", ssid->passphrase);
        if (ssid->security == TAPI_CFG_WIFI_SECURITY_WPA)
        {
            CHECKED_FPRINTF(ctx->f, "proto=WPA\n");
        }
        else if (ssid->security == TAPI_CFG_WIFI_SECURITY_WPA2)
        {
            CHECKED_FPRINTF(ctx->f, "proto=RSN\n");
        }
    }


    switch (ssid->protocol)
    {
        case TAPI_CFG_WIFI_PROTOCOL_TKIP:
        {
            if (ssid->security == TAPI_CFG_WIFI_SECURITY_WPA3 ||
                port->standard == TAPI_CFG_WIFI_STANDARD_BE)
            {
                ERROR("Invalid specification of protocol: TKIP can't be used "
                      "with WPA3");
                ret = -1;
                goto err;
            }

            CHECKED_FPRINTF(ctx->f, "pairwise=TKIP\n");
            CHECKED_FPRINTF(ctx->f, "group=TKIP\n");
            break;
        }
        case TAPI_CFG_WIFI_PROTOCOL_CCMP:
        {
            CHECKED_FPRINTF(ctx->f, "pairwise=CCMP\n");
            CHECKED_FPRINTF(ctx->f, "group=CCMP\n");
            break;
        }
    }

    CHECKED_FPRINTF(ctx->f, "scan_ssid=1\n");

    freq = wifi_channel_to_freq(port->standard, port->channel);
    if (freq != -1)
    {
        CHECKED_FPRINTF(ctx->f, "freq_list=%d\n", freq);
    }

    /* TODO: support HT modes */
    switch (port->standard)
    {
        case TAPI_CFG_WIFI_STANDARD_G:
        {
            if (freq == -1)
            {
                CHECKED_FPRINTF(ctx->f,
                    "freq_list=2412 2417 2422 2427 2432 2437 2442 "
                    "2447 2452 2457 2462 2467 2472\n");
            }
            CHECKED_FPRINTF(ctx->f, "disable_ht=1\n");
            CHECKED_FPRINTF(ctx->f, "disable_vht=1\n");
            break;
        }
        case TAPI_CFG_WIFI_STANDARD_N:
        {
            if (freq == -1)
            {
                CHECKED_FPRINTF(ctx->f,
                    "freq_list=2412 2417 2422 2427 2432 "
                    "2437 2442 2447 2452 2457 2462 2467 2472\n");
            }
            CHECKED_FPRINTF(ctx->f, "disable_ht=0\n");
            CHECKED_FPRINTF(ctx->f, "disable_vht=1\n");
            break;
        }
        case TAPI_CFG_WIFI_STANDARD_AC:
        {
            if (freq == -1)
            {
                CHECKED_FPRINTF(ctx->f, "freq_list=5180 5200 5220 5240 5260 "
                    "5280 5300 5320 5500 5520 5540 5560 5580 5600 5620 5640 "
                    "5660 5680 5700 5720 5745 5765 5785 5805 5825\n");
            }
            CHECKED_FPRINTF(ctx->f, "disable_ht=0\n");
            CHECKED_FPRINTF(ctx->f, "disable_vht=0\n");
            break;
        }
        case TAPI_CFG_WIFI_STANDARD_AX:
        {
            if (freq == -1)
            {
                CHECKED_FPRINTF(ctx->f, "freq_list=5180 5200 5220 5240 5260 "
                    "5280 5300 5320 5500 5520 5540 5560 5580 5600 5620 5640 "
                    "5660 5680 5700 5720 5745 5765 5785 5805 5825\n");
            }
            CHECKED_FPRINTF(ctx->f, "disable_ht=0\n");
            CHECKED_FPRINTF(ctx->f, "disable_vht=0\n");
            CHECKED_FPRINTF(ctx->f, "disable_he=0\n");
            CHECKED_FPRINTF(ctx->f, "disable_eht=1\n");
            break;
        }
        case TAPI_CFG_WIFI_STANDARD_BE:
        {
            if (freq == -1)
            {
                CHECKED_FPRINTF(ctx->f, "freq_list=5935 5955 5975 5995 "
                    "6015 6035 6055 6075 6095 6115 6135 6155 6175 6195 "
                    "6215 6235 6255 6275 6295 6315 6335 6355 6375 6395 "
                    "6415 6435 6455 6475 6495 6515 6535 6555 6575 6595 "
                    "6615 6635 6655 6675 6695 6715 6735 6755 6775 6795 "
                    "6815 6835 6855 6875 6895 6915 6935 6955 6975 6995 "
                    "7015 7035 7055 7075 7095 7115\n");
            }
            CHECKED_FPRINTF(ctx->f, "disable_ht=0\n");
            CHECKED_FPRINTF(ctx->f, "disable_vht=0\n");
            break;
        }
    }

    CHECKED_FPRINTF(ctx->f, "}\n");

    ret = 0;

err:
    return ret;
}

/* Write out SSID to hostapd configuration */
static te_errno
ta_wifi_wh_apply_ha_ssid(ta_wifi_wh_context *ctx, ta_wifi_ssid *ssid)
{
    te_errno      ret;
    ta_wifi_port *port;
    int           freq;

    assert(ctx != NULL);
    assert(ctx->port != NULL);
    assert(ssid != NULL);
    assert(ssid->name != NULL);

    port = ctx->port;

    if (ctx->ssid_instance != 0)
    {
        CHECKED_FPRINTF(ctx->f, "bss=%s_%d\n", port->ifname,
                        ctx->ssid_instance);
    }
    CHECKED_FPRINTF(ctx->f, "ssid=%s\n", ssid->name);

    switch (ssid->security)
    {
        case TAPI_CFG_WIFI_SECURITY_OPEN:
        {
            CHECKED_FPRINTF(ctx->f, "auth_algs=1\n");
            break;
        }
        case TAPI_CFG_WIFI_SECURITY_WEP:
        {
            CHECKED_FPRINTF(ctx->f, "auth_algs=1\n");
            if (ssid->passphrase && strlen(ssid->passphrase) > 0)
            {
                CHECKED_FPRINTF(ctx->f, "wep_default_key=0\n");
                CHECKED_FPRINTF(ctx->f, "wep_key0=%s\n", ssid->passphrase);
            }
            break;
        }
        case TAPI_CFG_WIFI_SECURITY_WPA:
        {
            CHECKED_FPRINTF(ctx->f, "wpa=1\n");
            CHECKED_FPRINTF(ctx->f, "wpa_key_mgmt=WPA-PSK\n");
            if (ssid->protocol == TAPI_CFG_WIFI_PROTOCOL_TKIP)
            {
                CHECKED_FPRINTF(ctx->f, "wpa_pairwise=TKIP\n");
            }
            else
            {
                CHECKED_FPRINTF(ctx->f, "wpa_pairwise=CCMP\n");
            }
            if (ssid->passphrase && strlen(ssid->passphrase) > 0)
                CHECKED_FPRINTF(ctx->f, "wpa_passphrase=%s\n",
                                ssid->passphrase);

            break;
        }
        case TAPI_CFG_WIFI_SECURITY_WPA2:
        {
            CHECKED_FPRINTF(ctx->f, "wpa=2\n");
            CHECKED_FPRINTF(ctx->f, "wpa_key_mgmt=WPA-PSK\n");
            if (ssid->protocol == TAPI_CFG_WIFI_PROTOCOL_TKIP)
            {
                CHECKED_FPRINTF(ctx->f, "rsn_pairwise=TKIP\n");
            }
            else
            {
                CHECKED_FPRINTF(ctx->f, "rsn_pairwise=CCMP\n");
            }
            if (ssid->passphrase && strlen(ssid->passphrase) > 0)
                CHECKED_FPRINTF(ctx->f, "wpa_passphrase=%s\n",
                                ssid->passphrase);
            break;
        }
        case TAPI_CFG_WIFI_SECURITY_WPA3:
        {
            CHECKED_FPRINTF(ctx->f, "wpa=2\n");
            CHECKED_FPRINTF(ctx->f, "wpa_key_mgmt=SAE\n");
            CHECKED_FPRINTF(ctx->f, "ieee80211w=2\n");
            CHECKED_FPRINTF(ctx->f, "rsn_pairwise=CCMP\n");
            if (ssid->passphrase && strlen(ssid->passphrase) > 0)
                CHECKED_FPRINTF(ctx->f, "sae_password=%s\n", ssid->passphrase);
            break;
        }
        default:
        {
            break;
        }
    }

    ret = 0;
err:
    return ret;
}

/* Kill the pid by given pid file */
static te_errno
try_kill_pid(const char *pid_path)
{
    FILE *f;
    int   pid;
    int   ret;

    assert(pid_path != NULL);

    f = fopen(pid_path, "r");
    if (f == NULL)
    {
        /* No pid file - no issue */
        return 0;
    }

    if (fscanf(f, "%d", &pid) != 1)
    {
        fclose(f);
        return TE_RC(TE_TA_UNIX, TE_EIO);
    }

    fclose(f);

    ret = kill(pid, SIGTERM);
    if (ret != 0)
    {
        ret = kill(pid, SIGKILL);
        if (ret != 0)
        {
            return TE_OS_RC(TE_TA_UNIX, errno);
        }
    }

    unlink(pid_path);

    return 0;
}

/* Apply port's configuration and run necessary wpa_supplicant or hostapd */
static te_errno
ta_wifi_wh_apply_port(ta_wifi_wh_context *ctx, ta_wifi_port *port)
{
    te_errno      ret;
    char          conf_file[FILE_PATH_SIZE] = { 0 };
    char          pid_file[FILE_PATH_SIZE] = { 0 };
    char          cmd[CMD_SIZE] = { 0 };
    bool          found_wpas = false;
    bool          found_ha = false;
    ta_wifi_ssid *ssid;

    assert(port != NULL);

    if (port->ifname == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    SLIST_FOREACH(ssid, &port->ssids, links)
    {
        assert(ssid != NULL);

        if (!ssid->enable)
            continue;

        if (ssid->mode == TAPI_CFG_WIFI_MODE_STA)
        {
            found_wpas = true;
        }
        else if (ssid->mode == TAPI_CFG_WIFI_MODE_AP)
        {
            found_ha = true;
        }
    }

    if (!found_wpas && !found_ha)
        return TE_RC(TE_TA_UNIX, TE_ESKIP);

    if (found_wpas)
    {
        /* WPA supplicant configuration */
        ret = te_snprintf(conf_file,
                          sizeof(conf_file),
                          WPA_SUPPLICANT_CONF_FMT,
                          port->ifname);
        if (ret != 0)
            goto err;

        ret = te_snprintf(pid_file,
                          sizeof(pid_file),
                          WPA_SUPPLICANT_PID_FMT,
                          port->ifname);
        if (ret != 0)
            goto err;

        ctx->f = fopen(conf_file, "w");
        if (ctx->f == NULL)
            return TE_OS_RC(TE_TA_UNIX, errno);

        CHECKED_FPRINTF(ctx->f, "ctrl_interface=/var/run/wpa_supplicant\n");
        CHECKED_FPRINTF(ctx->f, "ap_scan=1\n");

        if (port->standard == TAPI_CFG_WIFI_STANDARD_BE)
        {
            CHECKED_FPRINTF(ctx->f, "sae_pwe=2\n");
        }

        ctx->port = port;

        SLIST_FOREACH(ssid, &port->ssids, links)
        {
            assert(ssid != NULL);

            if (!ssid->enable)
                continue;

            CHECKED_FPRINTF(ctx->f, "\n");
            if (ssid->mode == TAPI_CFG_WIFI_MODE_STA)
            {
                ret = ta_wifi_wh_apply_wpas_ssid(ctx, ssid);
                if (ret != 0)
                    goto err;
            }
        }

        if (ctx->f != NULL)
            fclose(ctx->f);
        ctx->f = NULL;

        /* Kill existing instance */
        try_kill_pid(pid_file);

        /* Run a new wpa_supplicant instance */
        ret = te_snprintf(cmd, sizeof(cmd),
            "%s -i %s -B -c %s -P %s",
            WPA_SUPPLICANT_PATH, port->ifname, conf_file, pid_file);
        if (ret != 0)
            goto err;

        if (ta_system(cmd) != 0)
        {
            ERROR("Failed to start WPA Supplicant");
            ret = TE_RC(TE_TA_UNIX, TE_ESHCMD);
            goto err;
        }
    }

    if (found_ha)
    {
        /* hostapd configuration */
        ret = te_snprintf(conf_file,
                          sizeof(conf_file),
                          HOSTAPD_CONF_FMT,
                          port->ifname);
        if (ret != 0)
            goto err;

        ret = te_snprintf(pid_file,
                          sizeof(pid_file),
                          HOSTAPD_PID_FMT,
                          port->ifname);
        if (ret != 0)
            goto err;

        ctx->f = fopen(conf_file, "w");
        if (ctx->f == NULL)
            return TE_OS_RC(TE_TA_UNIX, errno);

        CHECKED_FPRINTF(ctx->f, "ctrl_interface=/var/run/hostapd\n");
        CHECKED_FPRINTF(ctx->f, "ctrl_interface_group=0\n");

        ctx->port = port;

        CHECKED_FPRINTF(ctx->f, "interface=%s\n", port->ifname);
        /* FIXME: not sure how to handle different drivers */
        CHECKED_FPRINTF(ctx->f, "driver=nl80211\n");
        CHECKED_FPRINTF(ctx->f, "channel=%d\n", port->channel);

        if (port->tx_power > 0)
            CHECKED_FPRINTF(ctx->f, "tx_power=%u\n", port->tx_power);

        /* Add standard-specific options */
        switch (port->standard)
        {
            case TAPI_CFG_WIFI_STANDARD_G:
            case TAPI_CFG_WIFI_STANDARD_N:
            {
                CHECKED_FPRINTF(ctx->f, "hw_mode=g\n");
                if (port->standard == TAPI_CFG_WIFI_STANDARD_N)
                {
                    CHECKED_FPRINTF(ctx->f, "ieee80211n=1\n");
                }
                break;
            }

            case TAPI_CFG_WIFI_STANDARD_AC:
            {
                CHECKED_FPRINTF(ctx->f, "hw_mode=a\n");
                CHECKED_FPRINTF(ctx->f, "ieee80211ac=1\n");
                break;
            }
            case TAPI_CFG_WIFI_STANDARD_AX:
            case TAPI_CFG_WIFI_STANDARD_BE:
            {
                CHECKED_FPRINTF(ctx->f, "hw_mode=a\n");
                CHECKED_FPRINTF(ctx->f, "ieee80211ax=1\n");
                if (port->standard == TAPI_CFG_WIFI_STANDARD_BE)
                {
                    CHECKED_FPRINTF(ctx->f, "ieee80211be=1\n");
                }
                break;
            }
            default:
            {
                break;
            }
        }

        /* Convert port width */
        switch (port->width)
        {
            case TAPI_CFG_WIFI_WIDTH_20:
            {
                break;
            }
            case TAPI_CFG_WIFI_WIDTH_40:
            {
                CHECKED_FPRINTF(ctx->f, "ht_capab=[HT40+]\n");
                break;
            }
            case TAPI_CFG_WIFI_WIDTH_80:
            {
                CHECKED_FPRINTF(ctx->f, "vht_oper_chwidth=1\n");
                break;
            }
            case TAPI_CFG_WIFI_WIDTH_160:
            {
                CHECKED_FPRINTF(ctx->f, "vht_oper_chwidth=2\n");
                break;
            }
            case TAPI_CFG_WIFI_WIDTH_320:
            {
                CHECKED_FPRINTF(ctx->f, "ieee80211be=1\n");
                CHECKED_FPRINTF(ctx->f, "eht_oper_chwidth=3\n");
                break;
            }
            default:
            {
                break;
            }
        }

        /* Transfer SSIDs */
        ctx->ssid_instance = 0;
        SLIST_FOREACH(ssid, &port->ssids, links)
        {
            assert(ssid != NULL);

            if (!ssid->enable)
                continue;

            CHECKED_FPRINTF(ctx->f, "\n");
            if (ssid->mode == TAPI_CFG_WIFI_MODE_AP)
            {
                ret = ta_wifi_wh_apply_ha_ssid(ctx, ssid);
                if (ret != 0)
                    goto err;
                ctx->ssid_instance++;
            }
        }

        if (ctx->f != NULL)
            fclose(ctx->f);
        ctx->f = NULL;

        /* Kill existing instance */
        try_kill_pid(pid_file);

        /* Run a new hostapd instance */
        ret = te_snprintf(cmd, sizeof(cmd),
            "%s -P %s %s",
            HOSTAPD_PATH, pid_file, conf_file);
        if (ret != 0)
            goto err;

        if (ta_system(cmd) != 0)
        {
            ERROR("Failed to start hostapd");
            ret = TE_RC(TE_TA_UNIX, TE_ESHCMD);
            goto err;
        }
    }

err:
    if (ctx->f != NULL)
        fclose(ctx->f);
    ctx->f = NULL;

    return ret;
}

/* See the description in ta_wifi_wh.h */
te_errno
ta_wifi_wh_apply(ta_wifi *node)
{
    te_errno             ret;
    ta_wifi_wh_context   ctx = TA_WH_CONTEXT_INIT();
    ta_wifi_port        *port;

    assert(node != NULL);

    /* Apply ports */
    SLIST_FOREACH(port, &node->ports, links)
    {
        assert(port != NULL);

        if (!port->enable)
            continue;

        ret = ta_wifi_wh_apply_port(&ctx, port);
        if (TE_RC_GET_ERROR(ret) == TE_ESKIP)
        {
            ret = 0;
            continue;
        }
        else if (ret != 0)
        {
            goto err;
        }
    }

    ret = 0;

err:

    return ret;
}

/* See the description in ta_wifi_wh.h */
te_errno
ta_wifi_wh_cancel(ta_wifi *node)
{
    int  ret;
    char pid_file[FILE_PATH_SIZE];
    ta_wifi_port *port;

    /* Kill all supplicants */

    SLIST_FOREACH(port, &node->ports, links)
    {
        RING("Cancel WiFi port %s",
             port->ifname != NULL ? port->ifname : "none");
        if (port->ifname == NULL)
            continue;

        ret = te_snprintf(pid_file,
                          sizeof(pid_file),
                          WPA_SUPPLICANT_PID_FMT,
                          port->ifname);
        if (ret != 0)
        {
            ERROR("Couldn't build pid file path (wpa_supplicant)");
            return ret;
        }

        try_kill_pid(pid_file);

        ret = te_snprintf(pid_file,
                          sizeof(pid_file),
                          HOSTAPD_PID_FMT,
                          port->ifname);
        if (ret != 0)
        {
            ERROR("Couldn't build pid file path (hostapd)");
            return ret;
        }

        try_kill_pid(pid_file);
    }

    return 0;
}
