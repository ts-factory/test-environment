/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief TAD Ethernet
 *
 * Traffic Application Domain Command Handler.
 * Ethernet high-throughput special sender.
 *
 * Copyright (C) 2026 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER "TAD ETH flood"

#include "te_config.h"

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "te_defs.h"
#include "te_str.h"
#include "logger_ta_fast.h"
#include "tad_eth_impl.h"

static te_errno
tad_eth_flood_count_parse(const char *usr_param, uint64_t *count)
{
    uint64_t parsed;
    te_errno rc;

    if (count == NULL)
        return TE_RC(TE_TAD_CH, TE_EINVAL);

    *count = 1;
    if (usr_param == NULL || *usr_param == '\0')
        return 0;

    rc = te_str_to_uint64(usr_param, 0, &parsed);
    if (rc != 0 || parsed == 0)
        return TE_RC(TE_TAD_CH, TE_EINVAL);

    *count = parsed;
    return 0;
}

static void
tad_eth_flood_stats_update(csap_p csap, uint64_t sent)
{
    struct timeval now;

    if (sent == 0 || csap == NULL)
        return;

    gettimeofday(&now, NULL);
    csap->last_pkt = now;

    if (sent >= UINT_MAX - csap->sender.sent_pkts)
        csap->sender.sent_pkts = UINT_MAX;
    else
        csap->sender.sent_pkts += sent;
}

/**
 * Method to send a large number of Ethernet frames from prepared packets.
 *
 * The function is intended to be resolved via "send-func" by name.
 *
 * User param should be a positive packet count.
 */
tad_special_send_pkt_cb tad_eth_flood;
te_errno
tad_eth_flood(csap_p csap, const char *usr_param, tad_pkts *pkts)
{
    csap_write_cb_t write_cb;
    csap_spt_type_t *spt;
    uint64_t sent = 0;
    uint64_t pkt_num;
    bool first_sent;
    tad_pkt *pkt;
    te_errno rc;

    if (csap == NULL || pkts == NULL || CIRCLEQ_EMPTY(&pkts->pkts))
        return TE_RC(TE_TAD_CH, TE_EINVAL);
    first_sent = (csap->sender.sent_pkts != 0);

    rc = tad_eth_flood_count_parse(usr_param, &pkt_num);
    if (rc != 0)
    {
        ERROR("%s(): invalid send-func user param '%s': %r",
              __FUNCTION__, (usr_param == NULL) ? "" : usr_param, rc);
        return rc;
    }

    spt = csap_get_proto_support(csap, csap_get_rw_layer(csap));
    if (spt == NULL || spt->write_cb == NULL)
        return TE_RC(TE_TAD_CH, TE_EINVAL);
    write_cb = spt->write_cb;

    while (sent < pkt_num)
    {
        if (csap->state & CSAP_STATE_STOP)
        {
            rc = TE_RC(TE_TAD_CH, TE_EINTR);
            goto out;
        }

        CIRCLEQ_FOREACH(pkt, &pkts->pkts, links)
        {
            if (sent >= pkt_num)
                break;

            rc = write_cb(csap, pkt);
            if (rc != 0)
                goto out;

            if (!first_sent)
            {
                gettimeofday(&csap->first_pkt, NULL);
                first_sent = true;
            }

            sent++;
        }
    }

out:
    tad_eth_flood_stats_update(csap, sent);

    return rc;
}
