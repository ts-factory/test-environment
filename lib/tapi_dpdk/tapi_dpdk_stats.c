/** @file
 * @brief DPDK statistics helper functions TAPI
 *
 * TAPI to handle DPDK-related operations with statistics (implementation)
 *
 * Copyright (C) 2019 OKTET Labs. All rights reserved.
 *
 * @author Igor Romanov <Igor.Romanov@oktetlabs.ru>
 */

#include "te_config.h"

#include "te_ethernet.h"
#include "tapi_dpdk_stats.h"
#include "tapi_test_log.h"

static const char *
empty_string_if_null(const char *string)
{
    return string == NULL ? "" : string;
}

void
tapi_dpdk_stats_pps_artifact(uint64_t pps, const char *prefix)
{
    TEST_ARTIFACT("%sPPS: %lu", empty_string_if_null(prefix), pps);
}

uint64_t
tapi_dpdk_stats_calculate_l1_bitrate(uint64_t pps, unsigned int packet_size)
{
    /*
     * Assume the overhead size: 20 (Preamble + SOF + IPG) + 4 (FCS) = 24 bytes
     * per packet. IPG could be different, but the information is not present.
     */
    unsigned int overhead_size = 24;

    /* Packet is padded to the min ethernet frame not including CRC */
    packet_size = MAX(packet_size, ETHER_MIN_LEN - ETHER_CRC_LEN);

    return (packet_size + overhead_size) * 8U * pps;
}

void
tapi_dpdk_stats_l1_bitrate_artifact(uint64_t l1_bitrate, const char *prefix)
{
    TEST_ARTIFACT("%sL1 bit rate: %lu bit/s", empty_string_if_null(prefix),
                  l1_bitrate);
}

te_errno
tapi_dpdk_stats_calculate_l1_link_usage(uint64_t l1_bitrate,
                                        unsigned int link_speed,
                                        double *l1_link_usage)
{
    if (link_speed == 0)
    {
        ERROR("Link usage cannot be calculated when link speed is zero");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    *l1_link_usage = (double)l1_bitrate /
                     (double)((uint64_t)link_speed * 1000000ULL);

    return 0;
}

void
tapi_dpdk_stats_l1_link_usage_artifact(double l1_link_usage, const char *prefix)
{
    TEST_ARTIFACT("%sL1 rate percent: %.3f", empty_string_if_null(prefix),
                  l1_link_usage * 100.0);
}

void
tapi_dpdk_stats_log_rates(uint64_t pps, unsigned int packet_size,
                          unsigned int link_speed, const char *prefix)
{
    uint64_t l1_bitrate;

    l1_bitrate = tapi_dpdk_stats_calculate_l1_bitrate(pps, packet_size);

    tapi_dpdk_stats_pps_artifact(pps, prefix);
    tapi_dpdk_stats_l1_bitrate_artifact(l1_bitrate, prefix);

    if (link_speed == 0)
    {
        WARN_VERDICT("%sLink speed is zero: link usage report is skipped",
                     empty_string_if_null(prefix));
    }
    else
    {
        double l1_link_usage;

        tapi_dpdk_stats_calculate_l1_link_usage(l1_bitrate, link_speed,
                                                &l1_link_usage);
        tapi_dpdk_stats_l1_link_usage_artifact(l1_link_usage, prefix);
    }
}