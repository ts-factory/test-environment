/** @file
 * @brief RPC client API for DPDK Ethernet Device API
 *
 * RPC client API for DPDK Ethernet Device API functions.
 *
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 */

#include "te_config.h"

#include "log_bufs.h"
#include "tapi_mem.h"
#include "tapi_rpc_internal.h"
#include "tapi_test_log.h"

#include "tarpc.h"

#include "tapi_rpc_rte_ethdev.h"
#include "tapi_rpc_rte_mbuf.h"
#include "rpcc_dpdk.h"


static const char *
tarpc_rte_eth_stats2str(te_log_buf                 *tlbp,
                        struct tarpc_rte_eth_stats *stats)
{
    te_log_buf_append(tlbp, "{ ipackets = %llu, opackets = %llu, "
                      "ibytes = %llu, obytes = %llu, imissed = %llu, "
                      "ierrors = %llu, oerrors = %llu, rx_nombuf = %llu }",
                      stats->ipackets, stats->opackets, stats->ibytes,
                      stats->obytes, stats->imissed, stats->ierrors,
                      stats->oerrors, stats->rx_nombuf);

    return te_log_buf_get(tlbp);
}

int
rpc_rte_eth_stats_get(rcf_rpc_server             *rpcs,
                      uint8_t                     port_id,
                      struct tarpc_rte_eth_stats *stats)
{
    struct tarpc_rte_eth_stats_get_in   in;
    struct tarpc_rte_eth_stats_get_out  out;
    te_log_buf                         *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (stats == NULL)
        TEST_FAIL("Invalid %s() 'stats' argument", __func__);

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_stats_get", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_stats_get, out.retval);

    *stats = out.stats;

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_stats_get, "%hhu",
                 "stats = %s", in.port_id,
                 tarpc_rte_eth_stats2str(tlbp, stats));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_stats_get, out.retval);
}

static const char *
tarpc_rte_eth_rx_offloads2str(te_log_buf *tlbp, uint32_t rx_offloads)
{
    const struct te_log_buf_bit2str rx_offloads2str[] = {
#define TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(_bit) \
        { TARPC_RTE_DEV_RX_OFFLOAD_##_bit##_BIT, #_bit }
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(VLAN_STRIP),
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(IPV4_CKSUM),
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(UDP_CKSUM),
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(TCP_CKSUM),
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(TCP_LRO),
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(QINQ_STRIP),
        TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR(OUTER_IPV4_CKSUM),
#undef TARPC_RTE_DEV_RX_OFFLOAD_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, rx_offloads, rx_offloads2str);
}

static const char *
tarpc_rte_eth_tx_offloads2str(te_log_buf *tlbp, uint32_t tx_offloads)
{
    const struct te_log_buf_bit2str tx_offloads2str[] = {
#define TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(_bit) \
        { TARPC_RTE_DEV_TX_OFFLOAD_##_bit##_BIT, #_bit }
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(VLAN_INSERT),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(IPV4_CKSUM),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(UDP_CKSUM),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(TCP_CKSUM),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(SCTP_CKSUM),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(TCP_TSO),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(UDP_TSO),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(OUTER_IPV4_CKSUM),
        TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR(QINQ_INSERT),
#undef TARPC_RTE_DEV_TX_OFFLOAD_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, tx_offloads, tx_offloads2str);
}

static const char *
tarpc_rte_eth_dev_flow_types2str(te_log_buf *tlbp, uint64_t rss_flow_types)
{
    const struct te_log_buf_bit2str rss_flow_types2str[] = {
#define TARPC_RTE_ETH_FLOW_TYPE2STR(_bit) \
        { TARPC_RTE_ETH_FLOW_##_bit, #_bit }
        TARPC_RTE_ETH_FLOW_TYPE2STR(IPV4),
        TARPC_RTE_ETH_FLOW_TYPE2STR(FRAG_IPV4),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV4_TCP),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV4_UDP),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV4_SCTP),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV4_OTHER),
        TARPC_RTE_ETH_FLOW_TYPE2STR(IPV6),
        TARPC_RTE_ETH_FLOW_TYPE2STR(FRAG_IPV6),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV6_TCP),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV6_UDP),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV6_SCTP),
        TARPC_RTE_ETH_FLOW_TYPE2STR(NONFRAG_IPV6_OTHER),
        TARPC_RTE_ETH_FLOW_TYPE2STR(L2_PAYLOAD),
        TARPC_RTE_ETH_FLOW_TYPE2STR(IPV6_EX),
        TARPC_RTE_ETH_FLOW_TYPE2STR(IPV6_TCP_EX),
        TARPC_RTE_ETH_FLOW_TYPE2STR(IPV6_UDP_EX),
#undef TARPC_RTE_ETH_FLOW_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, rss_flow_types, rss_flow_types2str);
}

static const char *
tarpc_rte_eth_thresh2str(te_log_buf *tlbp,
                         const struct tarpc_rte_eth_thresh *thresh)
{
    te_log_buf_append(tlbp, "{ pthresh=%u, hthresh=%u, wthresh=%u }",
                      thresh->pthresh, thresh->hthresh, thresh->wthresh);
    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_rxconf2str(te_log_buf *tlbp,
                         const struct tarpc_rte_eth_rxconf *rxconf)
{
    if (rxconf == NULL)
    {
        te_log_buf_append(tlbp, "(null)");
        return te_log_buf_get(tlbp);
    }

    te_log_buf_append(tlbp, "{ rx_thresh=");
    tarpc_rte_eth_thresh2str(tlbp, &rxconf->rx_thresh);
    te_log_buf_append(tlbp, ", rx_free_thresh=%u, rx_drop_en=%u, "
                      "rx_deferred_start=%u }",
                      rxconf->rx_free_thresh, rxconf->rx_drop_en,
                      rxconf->rx_deferred_start);
    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_txq_flags2str(te_log_buf *tlbp, uint32_t txq_flags)
{
    const struct te_log_buf_bit2str txq_flags2str[] = {
#define TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(_bit) \
        { TARPC_RTE_ETH_TXQ_FLAGS_##_bit##_BIT, #_bit }
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOMULTSEGS),
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOREFCOUNT),
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOMULTMEMP),
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOVLANOFFL),
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOXSUMSCTP),
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOXSUMUDP),
        TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR(NOXSUMTCP),
#undef TARPC_RTE_ETH_TXQ_FLAGS_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, txq_flags, txq_flags2str);
}

static const char *
tarpc_rte_eth_txconf2str(te_log_buf *tlbp,
                         const struct tarpc_rte_eth_txconf *txconf)
{
    if (txconf == NULL)
    {
        te_log_buf_append(tlbp, "(null)");
        return te_log_buf_get(tlbp);
    }

    te_log_buf_append(tlbp, "{ tx_thresh=");
    tarpc_rte_eth_thresh2str(tlbp, &txconf->tx_thresh);
    te_log_buf_append(tlbp, ", tx_rs_thresh=%u, tx_free_thresh=%u",
                      txconf->tx_rs_thresh, txconf->tx_free_thresh);
    te_log_buf_append(tlbp, ", txq_flags=");
    tarpc_rte_eth_txq_flags2str(tlbp, txconf->txq_flags);
    te_log_buf_append(tlbp, ", tx_deferred_start=%u }",
                      txconf->tx_deferred_start);
    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_dev_desc_lim2str(te_log_buf *tlbp,
                               const struct tarpc_rte_eth_desc_lim *desc_lim)
{
    te_log_buf_append(tlbp, "{ nb_max=%u, nb_min=%u, nb_align=%u }",
                      desc_lim->nb_max, desc_lim->nb_min, desc_lim->nb_align);
    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_speeds2str(te_log_buf *tlbp, uint32_t speeds)
{
    const struct te_log_buf_bit2str speeds2str[] = {
#define TARPC_RTE_ETH_LINK_SPEED_BIT2STR(_bit) \
        { TARPC_RTE_ETH_LINK_SPEED_##_bit, #_bit }
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(FIXED),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(10M_HD),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(10M),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(100M_HD),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(100M),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(1G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(2_5G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(5G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(10G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(20G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(25G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(40G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(50G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(56G),
        TARPC_RTE_ETH_LINK_SPEED_BIT2STR(100G),
#undef TARPC_RTE_ETH_LINK_SPEED_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, speeds, speeds2str);
}

static const char *
tarpc_rte_eth_dev_info2str(te_log_buf *tlbp,
                           const struct tarpc_rte_eth_dev_info *dev_info)
{
    te_log_buf_append(tlbp, "{ ");

    te_log_buf_append(tlbp, "driver_name=%s, if_index=%u, "
                      "min_rx_bufsize=%u, max_rx_pktlen=%u, "
                      "max_rx_queues=%u, max_tx_queues=%u, "
                      "max_mac_addrs=%u, max_hash_mac_addrs=%u, "
                      "max_vfs=%u, max_vmdq_pools=%u",
                      dev_info->driver_name, dev_info->if_index,
                      dev_info->min_rx_bufsize, dev_info->max_rx_pktlen,
                      dev_info->max_rx_queues, dev_info->max_tx_queues,
                      dev_info->max_mac_addrs, dev_info->max_hash_mac_addrs,
                      dev_info->max_vfs, dev_info->max_vmdq_pools);

    te_log_buf_append(tlbp, ", rx_offload_capa=");
    tarpc_rte_eth_rx_offloads2str(tlbp, dev_info->rx_offload_capa);
    te_log_buf_append(tlbp, ", tx_offload_capa=");
    tarpc_rte_eth_tx_offloads2str(tlbp, dev_info->tx_offload_capa);

    te_log_buf_append(tlbp, ", reta_size=%u, hash_key_size=%u, "
                      "flow_type_rss_offloads=",
                      dev_info->reta_size, dev_info->hash_key_size);
    tarpc_rte_eth_dev_flow_types2str(tlbp, dev_info->flow_type_rss_offloads);

    te_log_buf_append(tlbp, ", default_rxconf=");
    tarpc_rte_eth_rxconf2str(tlbp, &dev_info->default_rxconf);
    te_log_buf_append(tlbp, ", default_txconf=");
    tarpc_rte_eth_txconf2str(tlbp, &dev_info->default_txconf);

    te_log_buf_append(tlbp, ", vmdq_queue_base=%u, vmdq_queue_num=%u, "
                      "vmdq_pool_base=%u",
                      dev_info->vmdq_queue_base, dev_info->vmdq_queue_num,
                      dev_info->vmdq_pool_base);

    te_log_buf_append(tlbp, ", rx_desc_lim=");
    tarpc_rte_eth_dev_desc_lim2str(tlbp, &dev_info->rx_desc_lim);
    te_log_buf_append(tlbp, ", tx_desc_lim=");
    tarpc_rte_eth_dev_desc_lim2str(tlbp, &dev_info->tx_desc_lim);

    te_log_buf_append(tlbp, ", speed_capa=");
    tarpc_rte_eth_speeds2str(tlbp, dev_info->speed_capa);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}

void
rpc_rte_eth_dev_info_get(rcf_rpc_server *rpcs, uint8_t port_id,
                         struct tarpc_rte_eth_dev_info *dev_info)
{
    tarpc_rte_eth_dev_info_get_in   in;
    tarpc_rte_eth_dev_info_get_out  out;
    te_log_buf                     *tlbp;

    if (dev_info == NULL)
        TEST_FAIL("Invalid %s() dev_info argument", __func__);

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_info_get", &in, &out);

    *dev_info = out.dev_info;
    dev_info->driver_name = tapi_strdup(dev_info->driver_name);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_info_get, "%u", "dev_info=%s",
                 in.port_id, tarpc_rte_eth_dev_info2str(tlbp, dev_info));
    te_log_buf_free(tlbp);

    RETVAL_VOID(rte_eth_dev_info_get);
}


static const char *
te_log_buf_append_octet_string(te_log_buf *tlbp,
                               const uint8_t *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i)
        te_log_buf_append(tlbp, "%.2x", buf[i]);

    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_rx_mq_mode2str(te_log_buf *tlbp,
                             enum tarpc_rte_eth_rx_mq_mode mq_mode)
{
    const char *mode;

    switch (mq_mode)
    {
        case TARPC_ETH_MQ_RX_NONE:
            mode = "NONE";
            break;
        case TARPC_ETH_MQ_RX_RSS:
            mode = "RSS";
            break;
        case TARPC_ETH_MQ_RX_DCB:
            mode = "DCB";
            break;
        case TARPC_ETH_MQ_RX_DCB_RSS:
            mode = "DBC+RSS";
            break;
        case TARPC_ETH_MQ_RX_VMDQ_ONLY:
            mode = "VMDQ";
            break;
        case TARPC_ETH_MQ_RX_VMDQ_RSS:
            mode = "VMDQ+RSS";
            break;
        case TARPC_ETH_MQ_RX_VMDQ_DCB:
            mode = "VMDQ+DCB";
            break;
        case TARPC_ETH_MQ_RX_VMDQ_DCB_RSS:
            mode = "VMDQ+DCB+RSS";
            break;
        default:
            mode = "<UNKNOWN>";
            break;
    }
    te_log_buf_append(tlbp, "mq_mode=%s", mode);

    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_rxmode_flags2str(te_log_buf *tlbp, uint16_t flags)
{
    const struct te_log_buf_bit2str rxmode_flags2str[] = {
#define TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(_bit) \
        { TARPC_RTE_ETH_RXMODE_##_bit##_BIT, #_bit }
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(HEADER_SPLIT),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(HW_IP_CHECKSUM),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(HW_VLAN_FILTER),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(HW_VLAN_STRIP),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(HW_VLAN_EXTEND),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(JUMBO_FRAME),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(HW_STRIP_CRC),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(ENABLE_SCATTER),
        TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR(ENABLE_LRO),
#undef TARPC_RTE_DEV_RXMODE_FLAG_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, flags, rxmode_flags2str);
}

static const char *
tarpc_rte_eth_rxmode2str(te_log_buf *tlbp,
                         const struct tarpc_rte_eth_rxmode *rxconf)
{
    te_log_buf_append(tlbp, "{ ");

    tarpc_rte_eth_rx_mq_mode2str(tlbp, rxconf->mq_mode);
    te_log_buf_append(tlbp, ", max_rx_pkt_len=%u", rxconf->max_rx_pkt_len);
    te_log_buf_append(tlbp, ", split_hdr_size=%u, flags=",
                      rxconf->split_hdr_size);
    tarpc_rte_eth_rxmode_flags2str(tlbp, rxconf->flags);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_tx_mq_mode2str(te_log_buf *tlbp,
                             enum tarpc_rte_eth_tx_mq_mode mq_mode)
{
    const char *mode;

    switch (mq_mode)
    {
        case TARPC_ETH_MQ_TX_NONE:
            mode = "NONE";
            break;
        case TARPC_ETH_MQ_TX_DCB:
            mode = "DCB";
            break;
        case TARPC_ETH_MQ_TX_VMDQ_DCB:
            mode = "VMDQ_DCB";
            break;
        case TARPC_ETH_MQ_TX_VMDQ_ONLY:
            mode = "VMDQ_ONLY";
            break;
        default:
            mode = "<UNKNOWN>";
            break;
    }
    te_log_buf_append(tlbp, "mq_mode=%s", mode);

    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_txmode_flags2str(te_log_buf *tlbp, uint16_t flags)
{
    const struct te_log_buf_bit2str txmode_flags2str[] = {
#define TARPC_RTE_DEV_TXMODE_FLAG_BIT2STR(_bit) \
        { TARPC_RTE_ETH_TXMODE_##_bit##_BIT, #_bit }
        TARPC_RTE_DEV_TXMODE_FLAG_BIT2STR(HW_VLAN_REJECT_TAGGED),
        TARPC_RTE_DEV_TXMODE_FLAG_BIT2STR(HW_VLAN_REJECT_UNTAGGED),
        TARPC_RTE_DEV_TXMODE_FLAG_BIT2STR(HW_VLAN_INSERT_PVID),
#undef TARPC_RTE_DEV_TXMODE_FLAG_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, flags, txmode_flags2str);
}

static const char *
tarpc_rte_eth_txmode2str(te_log_buf *tlbp,
                         const struct tarpc_rte_eth_txmode *txconf)
{
    te_log_buf_append(tlbp, "{ ");

    tarpc_rte_eth_tx_mq_mode2str(tlbp, txconf->mq_mode);
    te_log_buf_append(tlbp, ", pvid=%u, flags=", txconf->pvid);
    tarpc_rte_eth_txmode_flags2str(tlbp, txconf->flags);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rss_hash_protos2str(te_log_buf *tlbp, tarpc_rss_hash_protos_t protos)
{
    te_bool             added = FALSE;

#define TARPC_RSS_HASH_PROTO2STR(_proto)                                 \
    do {                                                                 \
        tarpc_rss_hash_protos_t proto = 1ULL << TARPC_ETH_RSS_##_proto;  \
                                                                         \
        if ((protos & proto) == proto)                                   \
        {                                                                \
            te_log_buf_append(tlbp, "%s%s", added ? "|" : "", #_proto);  \
            added = TRUE;                                                \
            protos &= ~(1ULL << TARPC_ETH_RSS_##_proto);                 \
        }                                                                \
    } while (0)
    TARPC_RSS_HASH_PROTO2STR(IP);
    TARPC_RSS_HASH_PROTO2STR(UDP);
    TARPC_RSS_HASH_PROTO2STR(TCP);
    TARPC_RSS_HASH_PROTO2STR(SCTP);
    TARPC_RSS_HASH_PROTO2STR(TUNNEL);
    TARPC_RSS_HASH_PROTO2STR(IPV4);
    TARPC_RSS_HASH_PROTO2STR(FRAG_IPV4);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV4_TCP);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV4_UDP);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV4_SCTP);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV4_OTHER);
    TARPC_RSS_HASH_PROTO2STR(IPV6);
    TARPC_RSS_HASH_PROTO2STR(FRAG_IPV6);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV6_TCP);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV6_UDP);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV6_SCTP);
    TARPC_RSS_HASH_PROTO2STR(NONFRAG_IPV6_OTHER);
    TARPC_RSS_HASH_PROTO2STR(L2_PAYLOAD);
    TARPC_RSS_HASH_PROTO2STR(IPV6_EX);
    TARPC_RSS_HASH_PROTO2STR(IPV6_TCP_EX);
    TARPC_RSS_HASH_PROTO2STR(IPV6_UDP_EX);
    TARPC_RSS_HASH_PROTO2STR(PORT);
    TARPC_RSS_HASH_PROTO2STR(VXLAN);
    TARPC_RSS_HASH_PROTO2STR(GENEVE);
    TARPC_RSS_HASH_PROTO2STR(NVGRE);
#undef TARPC_RSS_HASH_PROTO2STR

    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_rss_conf2str(te_log_buf *tlbp,
                           const struct tarpc_rte_eth_rss_conf *rss_conf)
{
    te_log_buf_append(tlbp, "{");

    te_log_buf_append(tlbp, "rss_key=");
    te_log_buf_append_octet_string(tlbp,
                                   rss_conf->rss_key.rss_key_val,
                                   rss_conf->rss_key.rss_key_len);
    te_log_buf_append(tlbp, ", rss_key_len=%u",
                     (unsigned long long)rss_conf->rss_key_len);
    te_log_buf_append(tlbp, ", rss_hf=");
    tarpc_rss_hash_protos2str(tlbp, rss_conf->rss_hf);

    te_log_buf_append(tlbp, "}");
    return te_log_buf_get(tlbp);
}

static const char *
taprc_rte_eth_rx_adv_conf2str(te_log_buf *tlbp,
    const struct tarpc_rte_eth_rx_adv_conf *rx_conf_adv)
{
    te_log_buf_append(tlbp, "{ rss_conf=");

    tarpc_rte_eth_rss_conf2str(tlbp, &rx_conf_adv->rss_conf);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}


static const char *
tarpc_rte_intr_conf2str(te_log_buf *tlbp,
                        const struct tarpc_rte_intr_conf *intr_conf)
{
    te_log_buf_append(tlbp, "{ lsc=%u, rxq=%u }",
                      intr_conf->lsc, intr_conf->rxq);

    return te_log_buf_get(tlbp);
}


static const char *
tarpc_rte_eth_conf2str(te_log_buf *tlbp,
                       const struct tarpc_rte_eth_conf *eth_conf)
{
    if (eth_conf == NULL)
    {
        te_log_buf_append(tlbp, "(null)");
        return te_log_buf_get(tlbp);
    }

    te_log_buf_append(tlbp, "{ ");

    te_log_buf_append(tlbp, "link_speeds=%#x, rxmode=", eth_conf->link_speeds);
    tarpc_rte_eth_rxmode2str(tlbp, &eth_conf->rxmode);
    te_log_buf_append(tlbp, ", txmode=");
    tarpc_rte_eth_txmode2str(tlbp, &eth_conf->txmode);
    te_log_buf_append(tlbp, ", lbpk_mode=%#x, rx_conf_adv=",
                      eth_conf->lpbk_mode);
    taprc_rte_eth_rx_adv_conf2str(tlbp, &eth_conf->rx_adv_conf);
    te_log_buf_append(tlbp, ", dcb_cap_en=%u, intr_conf=",
                      eth_conf->dcb_capability_en);
    tarpc_rte_intr_conf2str(tlbp, &eth_conf->intr_conf);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}

int
rpc_rte_eth_dev_configure(rcf_rpc_server *rpcs,
                          uint8_t port_id,
                          uint16_t nb_rx_queue,
                          uint16_t nb_tx_queue,
                          const struct tarpc_rte_eth_conf *eth_conf)
{
    tarpc_rte_eth_dev_configure_in      in;
    tarpc_rte_eth_dev_configure_out     out;
    te_log_buf                         *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.nb_rx_queue = nb_rx_queue;
    in.nb_tx_queue = nb_tx_queue;
    if (eth_conf != NULL)
    {
        in.eth_conf.eth_conf_val = tapi_memdup(eth_conf, sizeof(*eth_conf));
        in.eth_conf.eth_conf_len = 1;
    }

    rcf_rpc_call(rpcs, "rte_eth_dev_configure", &in, &out);

    free(in.eth_conf.eth_conf_val);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_configure, out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_configure, "%u, %u, %u, %s",
                 NEG_ERRNO_FMT,
                 in.port_id, in.nb_rx_queue, in.nb_tx_queue,
                 tarpc_rte_eth_conf2str(tlbp, eth_conf),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_INT(rte_eth_dev_configure, out.retval);
}

void
rpc_rte_eth_dev_close(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_close_in   in;
    tarpc_rte_eth_dev_close_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_close", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_close, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_dev_close);
}

int
rpc_rte_eth_dev_start(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_start_in   in;
    tarpc_rte_eth_dev_start_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_start", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_start, out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_start, "%hhu", NEG_ERRNO_FMT,
                 in.port_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_start, out.retval);
}

void
rpc_rte_eth_dev_stop(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_stop_in   in;
    tarpc_rte_eth_dev_stop_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_stop", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_stop, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_dev_stop);
}

int
rpc_rte_eth_tx_queue_setup(rcf_rpc_server *rpcs,
                           uint8_t port_id,
                           uint16_t tx_queue_id,
                           uint16_t nb_tx_desc,
                           unsigned int socket_id,
                           struct tarpc_rte_eth_txconf *tx_conf)
{
    tarpc_rte_eth_tx_queue_setup_in     in;
    tarpc_rte_eth_tx_queue_setup_out    out;
    te_log_buf                         *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.tx_queue_id = tx_queue_id;
    in.nb_tx_desc = nb_tx_desc;
    in.socket_id = socket_id;
    if (tx_conf != NULL)
    {
        in.tx_conf.tx_conf_val = tapi_memdup(tx_conf, sizeof(*tx_conf));
        in.tx_conf.tx_conf_len = 1;
    }

    rcf_rpc_call(rpcs, "rte_eth_tx_queue_setup", &in, &out);

    free(in.tx_conf.tx_conf_val);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_tx_queue_setup, out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_tx_queue_setup,
                 "%hhu, %hu, %hu, %u, %s", NEG_ERRNO_FMT,
                 in.port_id, in.tx_queue_id, in.nb_tx_desc, in.socket_id,
                 tarpc_rte_eth_txconf2str(tlbp, tx_conf),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_tx_queue_setup, out.retval);
}

int
rpc_rte_eth_rx_queue_setup(rcf_rpc_server *rpcs,
                           uint8_t port_id,
                           uint16_t rx_queue_id,
                           uint16_t nb_rx_desc,
                           unsigned int socket_id,
                           struct tarpc_rte_eth_rxconf *rx_conf,
                           rpc_rte_mempool_p mp)
{
    tarpc_rte_eth_rx_queue_setup_in     in;
    tarpc_rte_eth_rx_queue_setup_out    out;
    te_log_buf                         *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.rx_queue_id = rx_queue_id;
    in.nb_rx_desc = nb_rx_desc;
    in.socket_id = socket_id;
    in.mp = (tarpc_rte_mempool)mp;
    if (rx_conf != NULL)
    {
        in.rx_conf.rx_conf_val = tapi_memdup(rx_conf, sizeof(*rx_conf));
        in.rx_conf.rx_conf_len = 1;
    }

    rcf_rpc_call(rpcs, "rte_eth_rx_queue_setup", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_rx_queue_setup, out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_rx_queue_setup,
                 "%hhu, %hu, %hu, %u, %s, " RPC_PTR_FMT, NEG_ERRNO_FMT,
                 in.port_id, in.rx_queue_id, in.nb_rx_desc, in.socket_id,
                 tarpc_rte_eth_rxconf2str(tlbp, rx_conf), RPC_PTR_VAL(mp),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_rx_queue_setup, out.retval);
}

uint16_t
rpc_rte_eth_tx_burst(rcf_rpc_server *rpcs,
                     uint8_t  port_id,
                     uint16_t queue_id,
                     rpc_rte_mbuf_p *tx_pkts,
                     uint16_t nb_pkts)
{
    tarpc_rte_eth_tx_burst_in     in;
    tarpc_rte_eth_tx_burst_out    out;
    te_log_buf                   *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;
    if (tx_pkts != NULL)
    {
        in.tx_pkts.tx_pkts_len = nb_pkts;
        in.tx_pkts.tx_pkts_val = (tarpc_rte_mbuf *)tapi_memdup(tx_pkts, nb_pkts *
                                                               sizeof(*tx_pkts));
    }

    rcf_rpc_call(rpcs, "rte_eth_tx_burst", &in, &out);

    CHECK_RETVAL_VAR_ERR_COND(rte_eth_tx_burst, out.retval,
                              out.retval > in.tx_pkts.tx_pkts_len,
                              out.retval,
                              out.retval > in.tx_pkts.tx_pkts_len);

    free(in.tx_pkts.tx_pkts_val);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_tx_burst, "%hhu, %hu, %s, %hu", "%hu",
                 in.port_id, in.queue_id,
                 rpc_rte_mbufs2str(tlbp, tx_pkts, in.tx_pkts.tx_pkts_len,
                                   rpcs),
                 in.tx_pkts.tx_pkts_len, out.retval);
    te_log_buf_free(tlbp);

    TAPI_RPC_OUT(rte_eth_tx_burst, out.retval > in.tx_pkts.tx_pkts_len);

    return out.retval;
}

uint16_t
rpc_rte_eth_rx_burst(rcf_rpc_server *rpcs,
                     uint8_t  port_id,
                     uint16_t queue_id,
                     rpc_rte_mbuf_p *rx_pkts,
                     uint16_t nb_pkts)
{
    tarpc_rte_eth_rx_burst_in     in;
    tarpc_rte_eth_rx_burst_out    out;
    te_log_buf                   *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;
    in.nb_pkts = nb_pkts;

    rcf_rpc_call(rpcs, "rte_eth_rx_burst", &in, &out);

    CHECK_RETVAL_VAR_ERR_COND(rte_eth_rx_burst, out.rx_pkts.rx_pkts_len,
                              out.rx_pkts.rx_pkts_len > in.nb_pkts,
                              out.rx_pkts.rx_pkts_len,
                              out.rx_pkts.rx_pkts_len > in.nb_pkts);

    memcpy(rx_pkts, out.rx_pkts.rx_pkts_val,
           MIN(nb_pkts, out.rx_pkts.rx_pkts_len) * sizeof(*rx_pkts));

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_rx_burst, "%hhu, %hu, %hu", "%hu %s",
                 in.port_id, in.queue_id, in.nb_pkts, out.rx_pkts.rx_pkts_len,
                 rpc_rte_mbufs2str(tlbp, rx_pkts, out.rx_pkts.rx_pkts_len,
                                   rpcs));
    te_log_buf_free(tlbp);

    TAPI_RPC_OUT(rte_eth_rx_burst, out.rx_pkts.rx_pkts_len > in.nb_pkts);

    return out.rx_pkts.rx_pkts_len;
}

int
rpc_rte_eth_dev_set_link_up(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_set_link_up_in   in;
    tarpc_rte_eth_dev_set_link_up_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_link_up", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_link_up, out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_link_up, "%hhu", NEG_ERRNO_FMT,
                 in.port_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_set_link_up, out.retval);
}

int
rpc_rte_eth_dev_set_link_down(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_set_link_down_in   in;
    tarpc_rte_eth_dev_set_link_down_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_link_down", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_link_down,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_link_down, "%hhu", NEG_ERRNO_FMT,
                 in.port_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_set_link_down, out.retval);
}

void
rpc_rte_eth_promiscuous_enable(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_promiscuous_enable_in   in;
    tarpc_rte_eth_promiscuous_enable_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_promiscuous_enable", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_promiscuous_enable, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_promiscuous_enable);
}

void
rpc_rte_eth_promiscuous_disable(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_promiscuous_disable_in   in;
    tarpc_rte_eth_promiscuous_disable_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_promiscuous_disable", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_promiscuous_disable, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_promiscuous_disable);
}

int
rpc_rte_eth_promiscuous_get(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_promiscuous_get_in   in;
    tarpc_rte_eth_promiscuous_get_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_promiscuous_get", &in, &out);

    CHECK_RETVAL_VAR(rte_eth_promiscuous_get, out.retval, ((out.retval != -1)
                     && (out.retval != 0) && (out.retval != 1)), -1);

    TAPI_RPC_LOG(rpcs, rte_eth_promiscuous_get, "%hhu", "%d",
                 in.port_id, out.retval);

    TAPI_RPC_OUT(rte_eth_promiscuous_get, ((out.retval != 0) &&
                 (out.retval != 1)));

    return out.retval;
}

void
rpc_rte_eth_allmulticast_enable(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_allmulticast_enable_in   in;
    tarpc_rte_eth_allmulticast_enable_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_allmulticast_enable", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_allmulticast_enable, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_allmulticast_enable);
}

void
rpc_rte_eth_allmulticast_disable(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_allmulticast_disable_in   in;
    tarpc_rte_eth_allmulticast_disable_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_allmulticast_disable", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_allmulticast_disable, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_allmulticast_disable);
}

int
rpc_rte_eth_allmulticast_get(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_allmulticast_get_in   in;
    tarpc_rte_eth_allmulticast_get_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_allmulticast_get", &in, &out);

    CHECK_RETVAL_VAR(rte_eth_allmulticast_get, out.retval, ((out.retval != -1)
                     && (out.retval != 0) && (out.retval != 1)), -1);

    TAPI_RPC_LOG(rpcs, rte_eth_allmulticast_get, "%hhu", "%d",
                 in.port_id, out.retval);

    TAPI_RPC_OUT(rte_eth_allmulticast_get, ((out.retval != 0) &&
                 (out.retval != 1)));

    return out.retval;
}

int
rpc_rte_eth_dev_get_mtu(rcf_rpc_server *rpcs, uint8_t port_id, uint16_t *mtu)
{
    tarpc_rte_eth_dev_get_mtu_in   in;
    tarpc_rte_eth_dev_get_mtu_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    if (mtu != NULL)
    {
        in.mtu.mtu_len = 1;
        in.mtu.mtu_val = malloc(sizeof(*mtu));
    }

    rcf_rpc_call(rpcs, "rte_eth_dev_get_mtu", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_get_mtu,
                                          out.retval);

    if (RPC_IS_CALL_OK(rpcs))
        *mtu = out.mtu;

    TAPI_RPC_LOG(rpcs, rte_eth_dev_get_mtu, "%hhu, %p",
                 NEG_ERRNO_FMT " mtu=%hu", port_id, mtu,
                 NEG_ERRNO_ARGS(out.retval), out.mtu);

    RETVAL_ZERO_INT(rte_eth_dev_get_mtu, out.retval);
}

int
rpc_rte_eth_dev_set_mtu(rcf_rpc_server *rpcs, uint8_t port_id,
                        uint16_t mtu)
{
    tarpc_rte_eth_dev_set_mtu_in   in;
    tarpc_rte_eth_dev_set_mtu_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.mtu = mtu;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_mtu", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_mtu,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_mtu, "%hhu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.mtu, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_set_mtu, out.retval);
}

int
rpc_rte_eth_dev_vlan_filter(rcf_rpc_server *rpcs, uint8_t port_id,
                            uint16_t vlan_id, int on)
{
    tarpc_rte_eth_dev_vlan_filter_in   in;
    tarpc_rte_eth_dev_vlan_filter_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.vlan_id = vlan_id;
    in.on = on;

    rcf_rpc_call(rpcs, "rte_eth_dev_vlan_filter", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_vlan_filter,
                                         out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_vlan_filter,
                 "%hhu, %hu, %d", NEG_ERRNO_FMT,
                 in.port_id, in.vlan_id, in.on,
                 NEG_ERRNO_ARGS(out.retval));

    RETVAL_ZERO_INT(rte_eth_dev_vlan_filter, out.retval);
}

int
rpc_rte_eth_dev_set_vlan_strip_on_queue(rcf_rpc_server *rpcs, uint8_t port_id,
                                        uint16_t rx_queue_id, int on)
{
    tarpc_rte_eth_dev_set_vlan_strip_on_queue_in   in;
    tarpc_rte_eth_dev_set_vlan_strip_on_queue_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.rx_queue_id = rx_queue_id;
    in.on = on;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_vlan_strip_on_queue", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_vlan_strip_on_queue,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_vlan_strip_on_queue,
                 "%hhu, %hu, %d", NEG_ERRNO_FMT,
                 in.port_id, in.rx_queue_id, in.on,
                 NEG_ERRNO_ARGS(out.retval));

    RETVAL_ZERO_INT(rte_eth_dev_set_vlan_strip_on_queue, out.retval);
}

static const char *
tarpc_rte_vlan_type2str(enum tarpc_rte_vlan_type vlan_type)
{
    const char *type;

    switch (vlan_type)
    {
        case TARPC_ETH_VLAN_TYPE_UNKNOWN:
            type = "UNKNOWN";
            break;
        case TARPC_ETH_VLAN_TYPE_INNER:
            type = "INNER";
            break;
        case TARPC_ETH_VLAN_TYPE_OUTER:
            type = "OUTER";
            break;
        case TARPC_ETH_VLAN_TYPE_MAX:
            type = "MAX";
            break;
        default:
            type = "<UNKNOWN>";
            break;
    }

    return type;
}

int
rpc_rte_eth_dev_set_vlan_ether_type(rcf_rpc_server *rpcs, uint8_t port_id,
                                    enum tarpc_rte_vlan_type vlan_type,
                                    uint16_t tag_type)
{
    tarpc_rte_eth_dev_set_vlan_ether_type_in   in;
    tarpc_rte_eth_dev_set_vlan_ether_type_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.vlan_type = vlan_type;
    in.tag_type = tag_type;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_vlan_ether_type", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_vlan_strip_on_queue,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_vlan_ether_type,
                 "%hhu, %s, %hu", NEG_ERRNO_FMT,
                 in.port_id, tarpc_rte_vlan_type2str(in.vlan_type),
                 in.tag_type, NEG_ERRNO_ARGS(out.retval));

    RETVAL_ZERO_INT(rte_eth_dev_set_vlan_ether_type, out.retval);
}

static const char *
tarpc_rte_eth_vlan_offload_mask2str(te_log_buf *tlbp, uint16_t offload_mask)
{
    const struct te_log_buf_bit2str vlan_offload_mask2str[] = {
#define TARPC_RTE_DEV_VLAN_OFFLOAD_FLAG_BIT2STR(_bit) \
        { TARPC_ETH_VLAN_##_bit##_OFFLOAD_BIT, #_bit }
        TARPC_RTE_DEV_VLAN_OFFLOAD_FLAG_BIT2STR(STRIP),
        TARPC_RTE_DEV_VLAN_OFFLOAD_FLAG_BIT2STR(FILTER),
        TARPC_RTE_DEV_VLAN_OFFLOAD_FLAG_BIT2STR(EXTEND),
#undef TARPC_RTE_DEV_VLAN_OFFLOAD_FLAG_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, offload_mask, vlan_offload_mask2str);
}

int
rpc_rte_eth_dev_set_vlan_offload(rcf_rpc_server *rpcs, uint8_t port_id,
                                 tarpc_int offload_mask)
{
    tarpc_rte_eth_dev_set_vlan_offload_in   in;
    tarpc_rte_eth_dev_set_vlan_offload_out  out;
    te_log_buf                             *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.offload_mask = offload_mask;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_vlan_offload", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_vlan_offload,
                                          out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_vlan_offload,
                 "%hhu, %s", NEG_ERRNO_FMT,
                 in.port_id,
                 tarpc_rte_eth_vlan_offload_mask2str(tlbp, in.offload_mask),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_set_vlan_offload, out.retval);
}

static uint16_t
tarpc_rte_eth_vlan_offload_mask_valid(uint16_t offload_mask)
{
#define CHECK_TARPC_RTE_ETH_VLAN_OFFLOAD_BIT(_bit) \
    do {                                                            \
        uint16_t flag = (1 << TARPC_ETH_VLAN_##_bit##_OFFLOAD_BIT); \
        offload_mask &= ~flag;                                      \
    } while (0)
    CHECK_TARPC_RTE_ETH_VLAN_OFFLOAD_BIT(STRIP);
    CHECK_TARPC_RTE_ETH_VLAN_OFFLOAD_BIT(FILTER);
    CHECK_TARPC_RTE_ETH_VLAN_OFFLOAD_BIT(EXTEND);
#undef CHECK_TARPC_RTE_ETH_VLAN_OFFLOAD_BIT
    return offload_mask;
}

int
rpc_rte_eth_dev_get_vlan_offload(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_get_vlan_offload_in   in;
    tarpc_rte_eth_dev_get_vlan_offload_out  out;
    te_log_buf                             *tlbp;
    int                                     check_mask;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_get_vlan_offload", &in, &out);

    check_mask = tarpc_rte_eth_vlan_offload_mask_valid(out.retval);

    CHECK_RETVAL_VAR_ERR_COND(rte_eth_dev_get_vlan_offload, out.retval,
                              out.retval >= 0 && check_mask != 0,
                              RETVAL_ECORRUPTED, out.retval < 0);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_get_vlan_offload,
                 "%hhu", "%s",
                 in.port_id,
                 (out.retval < 0) ? neg_errno_rpc2str(out.retval) :
                     tarpc_rte_eth_vlan_offload_mask2str(tlbp, out.retval));
    te_log_buf_free(tlbp);

    RETVAL_INT(rte_eth_dev_get_vlan_offload, out.retval);
}

int
rpc_rte_eth_dev_set_vlan_pvid(rcf_rpc_server *rpcs, uint8_t port_id,
                              uint16_t pvid, int on)
{
    tarpc_rte_eth_dev_set_vlan_pvid_in   in;
    tarpc_rte_eth_dev_set_vlan_pvid_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.pvid = pvid;
    in.on = on;

    rcf_rpc_call(rpcs, "rte_eth_dev_set_vlan_pvid", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_set_vlan_pvid,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_set_vlan_pvid,
                 "%hhu, %hu, %d", NEG_ERRNO_FMT,
                 in.port_id, in.pvid, in.on, NEG_ERRNO_ARGS(out.retval));

    RETVAL_ZERO_INT(rte_eth_dev_set_vlan_pvid, out.retval);
}

int
rpc_rte_eth_rx_descriptor_done(rcf_rpc_server *rpcs, uint8_t port_id,
                               uint16_t queue_id, uint16_t offset)
{
    tarpc_rte_eth_rx_descriptor_done_in   in;
    tarpc_rte_eth_rx_descriptor_done_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;
    in.offset = offset;

    rcf_rpc_call(rpcs, "rte_eth_rx_descriptor_done", &in, &out);

    CHECK_RETVAL_VAR_ERR_COND(rte_eth_rx_descriptor_done, out.retval,
                              (out.retval > 1), RETVAL_ECORRUPTED,
                              (out.retval < 0));

    TAPI_RPC_LOG(rpcs, rte_eth_rx_descriptor_done,
                 "%hhu, %hu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.queue_id, in.offset,
                 NEG_ERRNO_ARGS(out.retval));

    RETVAL_INT(rte_eth_rx_descriptor_done, out.retval);
}

int
rpc_rte_eth_rx_queue_count(rcf_rpc_server *rpcs, uint8_t port_id,
                           uint16_t queue_id)
{
    tarpc_rte_eth_rx_queue_count_in   in;
    tarpc_rte_eth_rx_queue_count_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_rx_queue_count", &in, &out);

    /* Number of descriptors is 16bit */
    CHECK_RETVAL_VAR_ERR_COND(rte_eth_rx_queue_count, out.retval,
                              (out.retval > UINT16_MAX), RETVAL_ECORRUPTED,
                              (out.retval < 0));

    TAPI_RPC_LOG(rpcs, rte_eth_rx_queue_count,
                 "%hhu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.queue_id, NEG_ERRNO_ARGS(out.retval));

    RETVAL_INT(rte_eth_rx_queue_count, out.retval);
}

int
rpc_rte_eth_dev_socket_id(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_socket_id_in   in;
    tarpc_rte_eth_dev_socket_id_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_socket_id", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(rte_eth_dev_socket_id, out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_socket_id, "%hhu", "%d",
                 in.port_id, out.retval);

    TAPI_RPC_OUT(rte_eth_dev_socket_id, out.retval < -1);

    return (out.retval);
}

int
rpc_rte_eth_dev_is_valid_port(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_dev_is_valid_port_in   in;
    tarpc_rte_eth_dev_is_valid_port_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_is_valid_port", &in, &out);

    CHECK_RETVAL_VAR(rte_eth_dev_is_valid_port, out.retval, ((out.retval != 1)
                     && (out.retval != 0)), -1);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_is_valid_port, "%hhu", "%d",
                 in.port_id, out.retval);
    RETVAL_INT(rte_eth_dev_is_valid_port, out.retval);
}

int
rpc_rte_eth_dev_rx_queue_start(rcf_rpc_server *rpcs, uint8_t port_id,
                               uint16_t queue_id)
{
    tarpc_rte_eth_dev_rx_queue_start_in   in;
    tarpc_rte_eth_dev_rx_queue_start_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_rx_queue_start", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_rx_queue_start,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_rx_queue_start, "%hhu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.queue_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_rx_queue_start, out.retval);
}

int
rpc_rte_eth_dev_rx_queue_stop(rcf_rpc_server *rpcs, uint8_t port_id,
                              uint16_t queue_id)
{
    tarpc_rte_eth_dev_rx_queue_stop_in   in;
    tarpc_rte_eth_dev_rx_queue_stop_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_rx_queue_stop", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_rx_queue_stop,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_rx_queue_stop, "%hhu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.queue_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_rx_queue_stop, out.retval);
}

int
rpc_rte_eth_dev_tx_queue_start(rcf_rpc_server *rpcs, uint8_t port_id,
                               uint16_t queue_id)
{
    tarpc_rte_eth_dev_tx_queue_start_in   in;
    tarpc_rte_eth_dev_tx_queue_start_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_tx_queue_start", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_tx_queue_start,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_tx_queue_start, "%hhu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.queue_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_tx_queue_start, out.retval);
}

int
rpc_rte_eth_dev_tx_queue_stop(rcf_rpc_server *rpcs, uint8_t port_id,
                              uint16_t queue_id)
{
    tarpc_rte_eth_dev_tx_queue_stop_in   in;
    tarpc_rte_eth_dev_tx_queue_stop_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_tx_queue_stop", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_tx_queue_stop,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_tx_queue_stop, "%hhu, %hu", NEG_ERRNO_FMT,
                 in.port_id, in.queue_id, NEG_ERRNO_ARGS(out.retval));
    RETVAL_ZERO_INT(rte_eth_dev_tx_queue_stop, out.retval);
}

void
rpc_rte_eth_macaddr_get(rcf_rpc_server *rpcs, uint8_t port_id,
                        struct tarpc_ether_addr *mac_addr)
{
    tarpc_rte_eth_macaddr_get_in   in;
    tarpc_rte_eth_macaddr_get_out  out;
    te_log_buf                    *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    if (mac_addr != NULL)
    {
        in.mac_addr.mac_addr_len = 1;
        in.mac_addr.mac_addr_val = malloc(sizeof(*mac_addr));
    }

    rcf_rpc_call(rpcs, "rte_eth_macaddr_get", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && out.mac_addr.mac_addr_len != 0)
        memcpy(mac_addr->addr_bytes, out.mac_addr.mac_addr_val[0].addr_bytes,
               sizeof(out.mac_addr.mac_addr_val[0].addr_bytes));

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_macaddr_get, "%hhu, %p", "%s",
                 in.port_id, mac_addr, te_ether_addr2log_buf(
                    tlbp, (uint8_t *)&out.mac_addr.mac_addr_val[0].addr_bytes));
    te_log_buf_free(tlbp);

    RETVAL_VOID(rte_eth_macaddr_get);
}

int
rpc_rte_eth_dev_default_mac_addr_set(rcf_rpc_server *rpcs, uint8_t port_id,
                                     struct tarpc_ether_addr *mac_addr)
{
    tarpc_rte_eth_dev_default_mac_addr_set_in   in;
    tarpc_rte_eth_dev_default_mac_addr_set_out  out;
    te_log_buf                                 *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    if (mac_addr != NULL)
    {
        in.mac_addr.mac_addr_len = 1;
        in.mac_addr.mac_addr_val = tapi_memdup(mac_addr, sizeof(*mac_addr));
    }

    rcf_rpc_call(rpcs, "rte_eth_dev_default_mac_addr_set", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_default_mac_addr_set,
                                          out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_default_mac_addr_set, "%hhu, %s", NEG_ERRNO_FMT,
                 in.port_id, te_ether_addr2log_buf(
                    tlbp, (uint8_t *)&in.mac_addr.mac_addr_val[0].addr_bytes),
                    NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_default_mac_addr_set, out.retval);
}

static const char *
tarpc_rte_eth_rxq_info2str(te_log_buf *tlbp,
                           const struct tarpc_rte_eth_rxq_info *qinfo,
                           rcf_rpc_server *rpcs)
{
    te_log_buf_append(tlbp, "{ ");

    te_log_buf_append(tlbp, "mp=", RPC_PTR_FMT, RPC_PTR_VAL(qinfo->mp));

    te_log_buf_append(tlbp, ", conf=");
    tarpc_rte_eth_rxconf2str(tlbp, &qinfo->conf);

    te_log_buf_append(tlbp, ", scattered_rx=%hhu, nb_desc=%hu",
                      qinfo->scattered_rx, qinfo->nb_desc);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}

int
rpc_rte_eth_rx_queue_info_get(rcf_rpc_server *rpcs, uint8_t port_id,
                              uint16_t queue_id,
                              struct tarpc_rte_eth_rxq_info *qinfo)
{
    tarpc_rte_eth_rx_queue_info_get_in   in;
    tarpc_rte_eth_rx_queue_info_get_out  out;
    te_log_buf                          *tlbp;

    if (qinfo == NULL)
        TEST_FAIL("Invalid %s() qinfo argument", __func__);

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_rx_queue_info_get", &in, &out);

    *qinfo = out.qinfo;

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_rx_queue_info_get, "%hhu, %hu", "qinfo=%s, %s",
                 in.port_id, in.queue_id,
                 tarpc_rte_eth_rxq_info2str(tlbp, qinfo, rpcs),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_rx_queue_info_get, out.retval);
}

static const char *
tarpc_rte_eth_txq_info2str(te_log_buf *tlbp,
                           const struct tarpc_rte_eth_txq_info *qinfo)
{
    te_log_buf_append(tlbp, "{ ");

    te_log_buf_append(tlbp, ", conf=");
    tarpc_rte_eth_txconf2str(tlbp, &qinfo->conf);

    te_log_buf_append(tlbp, ", nb_desc=%hu", qinfo->nb_desc);

    te_log_buf_append(tlbp, " }");
    return te_log_buf_get(tlbp);
}

int
rpc_rte_eth_tx_queue_info_get(rcf_rpc_server *rpcs, uint8_t port_id,
                              uint16_t queue_id,
                              struct tarpc_rte_eth_txq_info *qinfo)
{
    tarpc_rte_eth_tx_queue_info_get_in   in;
    tarpc_rte_eth_tx_queue_info_get_out  out;
    te_log_buf                          *tlbp;

    if (qinfo == NULL)
        TEST_FAIL("Invalid %s() qinfo argument", __func__);

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.queue_id = queue_id;

    rcf_rpc_call(rpcs, "rte_eth_tx_queue_info_get", &in, &out);

    *qinfo = out.qinfo;

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_tx_queue_info_get, "%hhu, %hu", "qinfo=%s, %s",
                 in.port_id, in.queue_id,
                 tarpc_rte_eth_txq_info2str(tlbp, qinfo),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_tx_queue_info_get, out.retval);
}

uint8_t
rpc_rte_eth_dev_count(rcf_rpc_server *rpcs)
{
    tarpc_rte_eth_dev_count_in   in;
    tarpc_rte_eth_dev_count_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "rte_eth_dev_count", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_count, "", "%hhu", out.retval);

    TAPI_RPC_OUT(rte_eth_dev_count, FALSE);

    return out.retval;
}

int
rpc_rte_eth_dev_detach(rcf_rpc_server *rpcs, uint8_t port_id,
                       char *devname)
{
    tarpc_rte_eth_dev_detach_in   in;
    tarpc_rte_eth_dev_detach_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_detach", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_detach, out.retval);

    if (out.retval == 0 && out.devname.devname_val != NULL)
        memcpy(devname, out.devname.devname_val, RPC_RTE_ETH_NAME_MAX_LEN);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_detach, "%hhu, %p", NEG_ERRNO_FMT ", %s",
                 in.port_id, devname, NEG_ERRNO_ARGS(out.retval), out.devname);

    RETVAL_ZERO_INT(rte_eth_dev_detach, out.retval);
}

static const char *
tarpc_rte_reta_conf2str(te_log_buf *tlbp,
                        const struct tarpc_rte_eth_rss_reta_entry64 *reta_conf,
                        uint16_t reta_size)
{
    unsigned cur_group, cur_entry;

    if (reta_conf == NULL)
    {
        te_log_buf_append(tlbp, "(null)");
        return te_log_buf_get(tlbp);
    }

    te_log_buf_append(tlbp, "reta_conf={");

    for (cur_group = 0; cur_group < reta_size / RPC_RTE_RETA_GROUP_SIZE; cur_group++)
    {
        te_log_buf_append(tlbp, " mask=%llx", reta_conf[cur_group].mask);
        te_log_buf_append(tlbp, ", reta=");

        for (cur_entry = 0; cur_entry < RPC_RTE_RETA_GROUP_SIZE; cur_entry++)
            te_log_buf_append(tlbp, " %hu", reta_conf[cur_group].reta[cur_entry]);

    }

    te_log_buf_append(tlbp, " }");

    return te_log_buf_get(tlbp);
}

int
rpc_rte_eth_dev_rss_reta_query(rcf_rpc_server *rpcs, uint8_t port_id,
                               struct tarpc_rte_eth_rss_reta_entry64 *reta_conf,
                               uint16_t reta_size)
{
    tarpc_rte_eth_dev_rss_reta_query_in   in;
    tarpc_rte_eth_dev_rss_reta_query_out  out;
    te_log_buf                           *tlbp;
    unsigned                              cur_group;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (reta_conf != NULL)
    {
        in.reta_conf.reta_conf_len = reta_size / RPC_RTE_RETA_GROUP_SIZE;
        in.reta_conf.reta_conf_val = calloc(in.reta_conf.reta_conf_len,
                                            sizeof(*reta_conf));
        for (cur_group = 0; cur_group < in.reta_conf.reta_conf_len; cur_group++)
            in.reta_conf.reta_conf_val[cur_group].mask = reta_conf[cur_group].mask;
    }

    in.port_id = port_id;
    in.reta_size = reta_size;

    rcf_rpc_call(rpcs, "rte_eth_dev_rss_reta_query", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_rss_reta_query,
                                          out.retval);

    if (out.retval == 0)
        for (cur_group = 0; cur_group < out.reta_conf.reta_conf_len; cur_group ++)
            memcpy(&reta_conf[cur_group], &out.reta_conf.reta_conf_val[cur_group],
                   sizeof(*reta_conf));

    tlbp = te_log_buf_alloc();

    TAPI_RPC_LOG(rpcs, rte_eth_dev_rss_reta_query,
                 "%hhu, %p, %hu", NEG_ERRNO_FMT ", %s",
                 in.port_id, reta_conf, reta_size,
                 NEG_ERRNO_ARGS(out.retval),
                 tarpc_rte_reta_conf2str(tlbp,
                     (out.reta_conf.reta_conf_len == 0) ? NULL :
                      out.reta_conf.reta_conf_val, reta_size));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_rss_reta_query, out.retval);
}

int
rpc_rte_eth_dev_rss_hash_conf_get(rcf_rpc_server *rpcs, uint8_t port_id,
                                  struct tarpc_rte_eth_rss_conf *rss_conf)
{
    tarpc_rte_eth_dev_rss_hash_conf_get_in   in;
    tarpc_rte_eth_dev_rss_hash_conf_get_out  out;
    te_log_buf                              *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_rss_hash_conf_get", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_rss_hash_conf_get,
                                          out.retval);

    if (out.retval == 0)
    {
        rss_conf->rss_key_len = out.rss_conf.rss_key_len;
        rss_conf->rss_key.rss_key_len = rss_conf->rss_key_len;

        memcpy(rss_conf->rss_key.rss_key_val,
               out.rss_conf.rss_key.rss_key_val,
               rss_conf->rss_key.rss_key_len);

        rss_conf->rss_hf = out.rss_conf.rss_hf;
    }

    tlbp = te_log_buf_alloc();

    TAPI_RPC_LOG(rpcs, rte_eth_dev_rss_hash_conf_get,
                 "%hhu, %p", NEG_ERRNO_FMT ", %s",
                 in.port_id, rss_conf, NEG_ERRNO_ARGS(out.retval),
                 (rss_conf == NULL) ? "NULL" :
                 tarpc_rte_eth_rss_conf2str(tlbp, rss_conf));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_rss_hash_conf_get, out.retval);
}

static const char *
tarpc_rte_eth_fc_mode2str(te_log_buf *tlbp,
                          enum tarpc_rte_eth_fc_mode fc_mode)
{
    const char *mode;

    switch (fc_mode)
    {
        case TARPC_RTE_FC_NONE:
            mode = "NONE";
            break;
        case TARPC_RTE_FC_RX_PAUSE:
            mode = "RX_PAUSE";
            break;
        case TARPC_RTE_FC_TX_PAUSE:
            mode = "TX_PAUSE";
            break;
        case TARPC_RTE_FC_FULL:
            mode = "FULL";
            break;
        default:
            mode = "<UNKNOWN>";
            break;
    }

    te_log_buf_append(tlbp, "mode=%s", mode);

    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_eth_fc_conf2str(te_log_buf *tlbp,
                          const struct tarpc_rte_eth_fc_conf *fc_conf)
{
    te_log_buf_append(tlbp, "{");

    te_log_buf_append(tlbp, "high_water=%lu, low_water=%lu"
                      ", pause_time=%hu, send_xon=%hu, ",
                      fc_conf->high_water, fc_conf->low_water,
                      fc_conf->pause_time, fc_conf->send_xon);

    tarpc_rte_eth_fc_mode2str(tlbp, fc_conf->mode);

    te_log_buf_append(tlbp, ", mac_ctrl_frame_fwd=%hhu, autoneg=%hhu",
                      fc_conf->mac_ctrl_frame_fwd, fc_conf->autoneg);
    te_log_buf_append(tlbp, "}");

    return te_log_buf_get(tlbp);
}

int rpc_rte_eth_dev_flow_ctrl_get(rcf_rpc_server *rpcs, uint8_t port_id,
                                  struct tarpc_rte_eth_fc_conf *fc_conf)
{
    tarpc_rte_eth_dev_flow_ctrl_get_in   in;
    tarpc_rte_eth_dev_flow_ctrl_get_out  out;
    te_log_buf                          *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_flow_ctrl_get", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_flow_ctrl_get,
                                          out.retval);

    if (out.retval == 0)
        *fc_conf = out.fc_conf;

    tlbp = te_log_buf_alloc();

    TAPI_RPC_LOG(rpcs, rte_eth_dev_flow_ctrl_get,
                 "%hhu, %p", NEG_ERRNO_FMT ", %s",
                 in.port_id, fc_conf, NEG_ERRNO_ARGS(out.retval),
                 (fc_conf == NULL) ? "<NULL>" :
                 tarpc_rte_eth_fc_conf2str(tlbp, fc_conf));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_flow_ctrl_get, out.retval);
}

const char *
tapi_rpc_rte_filter_type2str(enum tarpc_rte_filter_type filter_type)
{
    const char *type;

    switch (filter_type)
    {
        case TARPC_RTE_ETH_FILTER_NONE:
            type = "NONE";
            break;
        case TARPC_RTE_ETH_FILTER_MACVLAN:
            type = "MACVLAN";
            break;
        case TARPC_RTE_ETH_FILTER_ETHERTYPE:
            type = "ETHERTYPE";
            break;
        case TARPC_RTE_ETH_FILTER_FLEXIBLE:
            type = "FLEXIBLE";
            break;
        case TARPC_RTE_ETH_FILTER_SYN:
            type = "SYN";
            break;
        case TARPC_RTE_ETH_FILTER_NTUPLE:
            type = "NTUPLE";
            break;
        case TARPC_RTE_ETH_FILTER_TUNNEL:
            type = "TUNNEL";
            break;
        case TARPC_RTE_ETH_FILTER_FDIR:
            type = "FDIR";
            break;
        case TARPC_RTE_ETH_FILTER_HASH:
            type = "HASH";
            break;
        case TARPC_RTE_ETH_FILTER_L2_TUNNEL:
            type = "L2_TUNNEL";
            break;
        case TARPC_RTE_ETH_FILTER_MAX:
            type = "MAX";
            break;
        default:
            type = "<UNKNOWN>";
            break;
    }

    return type;
}

int rpc_rte_eth_dev_filter_supported(rcf_rpc_server *rpcs, uint8_t port_id,
                                     enum tarpc_rte_filter_type filter_type)
{
    tarpc_rte_eth_dev_filter_supported_in   in;
    tarpc_rte_eth_dev_filter_supported_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.filter_type = filter_type;

    rcf_rpc_call(rpcs, "rte_eth_dev_filter_supported", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_filter_supported,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, rte_eth_dev_filter_supported,
                 "%hhu, %s", NEG_ERRNO_FMT,
                 in.port_id, tapi_rpc_rte_filter_type2str(filter_type),
                 NEG_ERRNO_ARGS(out.retval));

    RETVAL_ZERO_INT(rte_eth_dev_filter_supported, out.retval);
}

int
rpc_rte_eth_xstats_get_names(rcf_rpc_server *rpcs, uint8_t port_id,
                             struct tarpc_rte_eth_xstat_name *xstats_names,
                             unsigned int size)
{
    tarpc_rte_eth_xstats_get_names_in   in;
    tarpc_rte_eth_xstats_get_names_out  out;
    te_log_buf                         *tlbp;
    int                                 i;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (size != 0 && xstats_names == NULL)
    {
        ERROR("%s(): No array of xstats names, but size is greater than 0",
              __FUNCTION__);
        RETVAL_ZERO_INT(rte_eth_xstats_get_names, -1);
    }

    in.port_id = port_id;
    in.size = size;

    rcf_rpc_call(rpcs, "rte_eth_xstats_get_names", &in, &out);

    CHECK_RETVAL_VAR_ERR_COND(rte_eth_xstats_get_names,
                              out.retval, FALSE,
                              -TE_RC(TE_TAPI, TE_ECORRUPTED),
                              (out.retval < 0));

    tlbp = te_log_buf_alloc();
    te_log_buf_append(tlbp, "{");
    if (xstats_names != NULL && out.retval > 0 &&
        (unsigned int)out.retval <= size)
    {
        for (i = 0; i < out.retval; i++)
        {
            strncpy(xstats_names[i].name,
                    out.xstats_names.xstats_names_val[i].name,
                    TARPC_RTE_ETH_XSTATS_NAME_SIZE);

            te_log_buf_append(tlbp, "%s%d=%s", (i == 0) ? "" : ", ", i,
                              xstats_names[i].name);
        }
    }
    te_log_buf_append(tlbp, "}");

    TAPI_RPC_LOG(rpcs, rte_eth_xstats_get_names,
                 "%hhu, %u", NEG_ERRNO_FMT " xstats_names=%s", in.port_id,
                 size, NEG_ERRNO_ARGS(out.retval), te_log_buf_get(tlbp));
    te_log_buf_free(tlbp);

    RETVAL_INT(rte_eth_xstats_get_names, out.retval);
}

int
rpc_rte_eth_xstats_get(rcf_rpc_server *rpcs, uint8_t port_id,
                       struct tarpc_rte_eth_xstat *xstats,
                       unsigned int n)
{
    tarpc_rte_eth_xstats_get_in     in;
    tarpc_rte_eth_xstats_get_out    out;
    int                             i;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (n != 0 && xstats == NULL)
    {
        ERROR("%s(): No array of xstats, but size is greater than 0",
              __FUNCTION__);
        RETVAL_ZERO_INT(rte_eth_xstats_get, -1);
    }

    in.port_id = port_id;
    in.n = n;

    rcf_rpc_call(rpcs, "rte_eth_xstats_get", &in, &out);

    CHECK_RETVAL_VAR_ERR_COND(rpc_rte_eth_xstats_get,
                              out.retval, FALSE,
                              -TE_RC(TE_TAPI, TE_ECORRUPTED),
                              (out.retval < 0));

    if (xstats != NULL && out.retval > 0)
    {
        for (i = 0; i < out.retval; i++)
            xstats[i] = out.xstats.xstats_val[i];
    }

    TAPI_RPC_LOG(rpcs, rte_eth_xstats_get,
                 "%hhu, %u", NEG_ERRNO_FMT, in.port_id, n,
                 NEG_ERRNO_ARGS(out.retval));

    RETVAL_INT(rte_eth_xstats_get, out.retval);
}

void
rpc_rte_eth_xstats_reset(rcf_rpc_server *rpcs, uint8_t port_id)
{
    tarpc_rte_eth_xstats_reset_in   in;
    tarpc_rte_eth_xstats_reset_out  out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_xstats_reset", &in, &out);

    TAPI_RPC_LOG(rpcs, rte_eth_xstats_reset, "%hhu", "", in.port_id);
    RETVAL_VOID(rte_eth_xstats_reset);
}

static const char *
tarpc_rte_filter_op2str(enum tarpc_rte_filter_op filter_op)
{
    const char *op;

    switch (filter_op)
    {
        case TARPC_RTE_ETH_FILTER_NOP:
            op = "NOP";
            break;
        case TARPC_RTE_ETH_FILTER_ADD:
            op = "ADD";
            break;
        case TARPC_RTE_ETH_FILTER_UPDATE:
            op = "UPDATE";
            break;
        case TARPC_RTE_ETH_FILTER_DELETE:
            op = "DELETE";
            break;
        case TARPC_RTE_ETH_FILTER_FLUSH:
            op = "FLUSH";
            break;
        case TARPC_RTE_ETH_FILTER_GET:
            op = "GET";
            break;
        case TARPC_RTE_ETH_FILTER_SET:
            op = "SET";
            break;
        case TARPC_RTE_ETH_FILTER_INFO:
            op = "INFO";
            break;
        case TARPC_RTE_ETH_FILTER_STATS:
            op = "STATS";
            break;
        case TARPC_RTE_ETH_FILTER_OP_MAX:
            op = "OP_MAX";
            break;
        default:
            op = "<UNKNOWN>";
            break;
    }

    return op;
}

static const char *
tarpc_rte_eth_none_filter_arg2str(te_log_buf *tlbp, const void *filter_arg)
{
    UNUSED(tlbp);
    UNUSED(filter_arg);

    return ("<NULL>");
}

static const char *
tarpc_rte_eth_unsupported_filter_arg2str(te_log_buf *tlbp, const void *filter_arg)
{
    UNUSED(tlbp);
    UNUSED(filter_arg);

    return ("<UNSUPPORTED_FILTER>");
}

static const char *
tarpc_rte_ethtype_flags2str(te_log_buf *tlbp, uint16_t ethtype_flags)
{
    const struct te_log_buf_bit2str ethtype_flags2str[] = {
#define TARPC_RTE_ETHTYPE_FLAGS_BIT2STR(_bit) \
        { TARPC_RTE_ETHTYPE_FLAGS_##_bit##_BIT, #_bit }
        TARPC_RTE_ETHTYPE_FLAGS_BIT2STR(MAC),
        TARPC_RTE_ETHTYPE_FLAGS_BIT2STR(DROP),
#undef TARPC_RTE_ETHTYPE_FLAGS_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, ethtype_flags, ethtype_flags2str);
}

static const char *
tarpc_rte_eth_ethertype_filter_arg2str(te_log_buf *tlbp, const void *filter_arg)
{
    struct tarpc_rte_eth_ethertype_filter *filter =
        (struct tarpc_rte_eth_ethertype_filter *)filter_arg;

    te_log_buf_append(tlbp, "{");

    te_log_buf_append(tlbp, "mac_addr=");
    te_ether_addr2log_buf(tlbp,
        (uint8_t *)&filter->mac_addr.addr_bytes);

    te_log_buf_append(tlbp, ", ether_type=%hu", filter->ether_type);

    te_log_buf_append(tlbp, ", flags=");
    tarpc_rte_ethtype_flags2str(tlbp, filter->flags);

    te_log_buf_append(tlbp, ", queue=%hu", filter->queue);
    te_log_buf_append(tlbp, "}");

    return te_log_buf_get(tlbp);
}

static const char *
tarpc_rte_ntuple_flags2str(te_log_buf *tlbp, uint16_t ntuple_flags)
{
    const struct te_log_buf_bit2str ntuple_flags2str[] = {
#define TARPC_RTE_NTUPLE_FLAGS_BIT2STR(_bit) \
        { TARPC_RTE_NTUPLE_FLAGS_##_bit##_BIT, #_bit }
        TARPC_RTE_NTUPLE_FLAGS_BIT2STR(DST_IP),
        TARPC_RTE_NTUPLE_FLAGS_BIT2STR(SRC_IP),
        TARPC_RTE_NTUPLE_FLAGS_BIT2STR(DST_PORT),
        TARPC_RTE_NTUPLE_FLAGS_BIT2STR(SRC_PORT),
        TARPC_RTE_NTUPLE_FLAGS_BIT2STR(PROTO),
        TARPC_RTE_NTUPLE_FLAGS_BIT2STR(TCP_FLAG),
#undef TARPC_RTE_NTUPLE_FLAGS_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, ntuple_flags, ntuple_flags2str);
}

static const char *
tarpc_rte_tcp_flags2str(te_log_buf *tlbp, uint32_t tcp_flags)
{
    const struct te_log_buf_bit2str tcp_flags2str[] = {
#define TARPC_RTE_TCP_FLAGS_BIT2STR(_bit) \
        { TARPC_RTE_TCP_##_bit##_BIT, #_bit }
        TARPC_RTE_TCP_FLAGS_BIT2STR(URG_FLAG),
        TARPC_RTE_TCP_FLAGS_BIT2STR(ACK_FLAG),
        TARPC_RTE_TCP_FLAGS_BIT2STR(PSH_FLAG),
        TARPC_RTE_TCP_FLAGS_BIT2STR(RST_FLAG),
        TARPC_RTE_TCP_FLAGS_BIT2STR(SYN_FLAG),
        TARPC_RTE_TCP_FLAGS_BIT2STR(FIN_FLAG),
        TARPC_RTE_TCP_FLAGS_BIT2STR(FLAG_ALL),
#undef TARPC_RTE_TCP_FLAGS_BIT2STR
        { 0, NULL }
    };

    return te_bit_mask2log_buf(tlbp, tcp_flags, tcp_flags2str);
}

static const char *
tarpc_rte_eth_ntuple_filter_arg2str(te_log_buf *tlbp, const void *filter_arg)
{
    struct tarpc_rte_eth_ntuple_filter *filter;

    filter = (struct tarpc_rte_eth_ntuple_filter *)filter_arg;

    te_log_buf_append(tlbp, "{");
    te_log_buf_append(tlbp, "flags=");
    tarpc_rte_ntuple_flags2str(tlbp, filter->flags);
    te_log_buf_append(tlbp, ", dst_ip=");
    te_ip_addr2log_buf(tlbp, (const void *)&filter->dst_ip, INET_ADDRSTRLEN);
    te_log_buf_append(tlbp, ", dst_ip_mask=");
    te_ip_addr2log_buf(tlbp, (const void *)&filter->dst_ip_mask,
                       INET_ADDRSTRLEN);
    te_log_buf_append(tlbp, ", src_ip=");
    te_ip_addr2log_buf(tlbp, (const void *)&filter->src_ip, INET_ADDRSTRLEN);
    te_log_buf_append(tlbp, ", src_ip_mask=");
    te_ip_addr2log_buf(tlbp, (const void *)&filter->src_ip_mask,
                       INET_ADDRSTRLEN);

    te_log_buf_append(tlbp, ", dst_port=%hu, dst_port_mask=%hu",
                      ntohs(filter->dst_port), filter->dst_port_mask);
    te_log_buf_append(tlbp, ", src_port=%hu, src_port_mask=%hu",
                      ntohs(filter->dst_port), filter->dst_port_mask);
    te_log_buf_append(tlbp,  ", proto=%hhu, proto_mask=%hhu",
                      filter->proto, filter->proto_mask);

    te_log_buf_append(tlbp, ", tcp_flags=");
    tarpc_rte_tcp_flags2str(tlbp, filter->tcp_flags);
    te_log_buf_append(tlbp, ", priority=%hhu, queue=%hu",
                      filter->priority, filter->queue);
    te_log_buf_append(tlbp, "}");

    return te_log_buf_get(tlbp);
}

typedef const char * (*tarpc_rte_filter_arg2str)(te_log_buf *tlbp,
                                                 const void *filter_arg);

tarpc_rte_filter_arg2str tarpc_filter_arg2str[] = {
#define TARPC_FILTER2STR(_type) tarpc_rte_eth_##_type##_filter_arg2str
    TARPC_FILTER2STR(none),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(ethertype),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(ntuple),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(unsupported),
    TARPC_FILTER2STR(unsupported),
    NULL
#undef TARPC_FILTER2STR
};

int rpc_rte_eth_dev_filter_ctrl(rcf_rpc_server *rpcs, uint8_t port_id,
                                enum tarpc_rte_filter_type filter_type,
                                enum tarpc_rte_filter_op filter_op,
                                void *arg)
{
    tarpc_rte_eth_dev_filter_ctrl_in   in;
    tarpc_rte_eth_dev_filter_ctrl_out  out;
    te_log_buf                        *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.port_id = port_id;
    in.filter_type = filter_type;
    in.filter_op = filter_op;

    switch (filter_type)
    {
        case TARPC_RTE_ETH_FILTER_MACVLAN:
            in.arg.arg_len = 0;
            break;
        case TARPC_RTE_ETH_FILTER_ETHERTYPE:
            if (arg != NULL)
            {
                in.arg.arg_len = sizeof(struct tarpc_rte_eth_ethertype_filter);
                in.arg.arg_val = malloc(in.arg.arg_len);
                if (in.arg.arg_val == NULL)
                {
                    rpcs->_errno = TE_RC(TE_RPC, TE_ENOMEM);
                    RETVAL_INT(rte_eth_dev_filter_ctrl, -rpcs->_errno);
                }
                memcpy(in.arg.arg_val, arg, in.arg.arg_len);
            }
            else
            {
                in.arg.arg_len = 0;
            }

            break;
        case TARPC_RTE_ETH_FILTER_FLEXIBLE:
            in.arg.arg_len = 0;
            break;
        case TARPC_RTE_ETH_FILTER_SYN:
            in.arg.arg_len = 0;
            break;
        case TARPC_RTE_ETH_FILTER_NTUPLE:
            if (arg != NULL)
            {
                in.arg.arg_len = sizeof(struct tarpc_rte_eth_ntuple_filter);
                in.arg.arg_val = malloc(in.arg.arg_len);
                if (in.arg.arg_val == NULL)
                {
                    rpcs->_errno = TE_RC(TE_RPC, TE_ENOMEM);
                    RETVAL_INT(rte_eth_dev_filter_ctrl, -rpcs->_errno);
                }
                memcpy(in.arg.arg_val, arg, in.arg.arg_len);
            }
            else
            {
                in.arg.arg_len = 0;
            }

            break;
        case TARPC_RTE_ETH_FILTER_TUNNEL:
        case TARPC_RTE_ETH_FILTER_HASH:
        case TARPC_RTE_ETH_FILTER_L2_TUNNEL:
        default:
            in.arg.arg_len = 0;
            break;
    }

    rcf_rpc_call(rpcs, "rte_eth_dev_filter_ctrl", &in, &out);
    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_filter_ctrl,
                                          out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_filter_ctrl,
                 "%hhu, %s, %s, filter_arg=%s", NEG_ERRNO_FMT,
                 in.port_id, tapi_rpc_rte_filter_type2str(filter_type),
                 tarpc_rte_filter_op2str(filter_op),
                 (tarpc_filter_arg2str[filter_type] == NULL) ? "<NULL>" :
                 tarpc_filter_arg2str[filter_type](tlbp, arg),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_filter_ctrl, out.retval);
}

int
rpc_rte_eth_dev_rss_hash_update(rcf_rpc_server *rpcs, uint8_t port_id,
                                struct tarpc_rte_eth_rss_conf *rss_conf)
{
    tarpc_rte_eth_dev_rss_hash_update_in   in;
    tarpc_rte_eth_dev_rss_hash_update_out  out;
    te_log_buf                            *tlbp;
    te_errno                               rc = 0;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (rss_conf == NULL)
    {
        rc = TE_RC(TE_RPC, TE_EINVAL);
        goto check_ret;
    }

    if (rss_conf->rss_key_len != 0)
    {
        in.rss_conf.rss_key.rss_key_val = tapi_memdup(
            rss_conf->rss_key.rss_key_val, rss_conf->rss_key_len);

        in.rss_conf.rss_key.rss_key_len = rss_conf->rss_key_len;
    }

    in.rss_conf.rss_key_len = rss_conf->rss_key_len;
    in.rss_conf.rss_hf = rss_conf->rss_hf;
    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_dev_rss_hash_update", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_rss_hash_update,
                                          out.retval);
    tlbp = te_log_buf_alloc();

    TAPI_RPC_LOG(rpcs, rte_eth_dev_rss_hash_update,
                 "%hhu, %s", NEG_ERRNO_FMT,
                 in.port_id, (rss_conf == NULL) ? "NULL" :
                 tarpc_rte_eth_rss_conf2str(tlbp, rss_conf),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

check_ret:
    RETVAL_ZERO_INT(rte_eth_dev_rss_hash_update,
                    (rc) ? -rc : out.retval);
}

int
rpc_rte_eth_dev_rss_reta_update(rcf_rpc_server *rpcs, uint8_t port_id,
                               struct tarpc_rte_eth_rss_reta_entry64 *reta_conf,
                               uint16_t reta_size)
{
    tarpc_rte_eth_dev_rss_reta_update_in   in;
    tarpc_rte_eth_dev_rss_reta_update_out  out;
    te_log_buf                            *tlbp;
    te_errno                               rc = 0;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (reta_conf != NULL)
    {
        in.reta_conf.reta_conf_len = reta_size / RPC_RTE_RETA_GROUP_SIZE;
        in.reta_conf.reta_conf_val = calloc(in.reta_conf.reta_conf_len,
                                            sizeof(*reta_conf));
        if (in.reta_conf.reta_conf_val == NULL)
        {
            rc = TE_RC(TE_RPC, TE_ENOMEM);

            goto check_ret;
        }

        memcpy(in.reta_conf.reta_conf_val, reta_conf,
               in.reta_conf.reta_conf_len * sizeof(*reta_conf));
    }

    in.port_id = port_id;
    in.reta_size = reta_size;

    rcf_rpc_call(rpcs, "rte_eth_dev_rss_reta_update", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_rss_reta_update,
                                          out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_rss_reta_update,
                 "%hhu, %p, %hu", NEG_ERRNO_FMT,
                 in.port_id, reta_conf, reta_size,
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

check_ret:
    RETVAL_ZERO_INT(rte_eth_dev_rss_reta_update,
                    (rc) ? -rc : out.retval);
}

static const char *
tarpc_rte_eth_link2str(te_log_buf                 *tlbp,
                       struct tarpc_rte_eth_link *eth_link)
{
    te_log_buf_append(
        tlbp, "{ link_speed = %llu, link_duplex = %s, "
        "link_autoneg = %s, link_status = %s }", eth_link->link_speed,
        (eth_link->link_duplex == 0) ? "HALF_DUPLEX" : "FULL_DUPLEX",
        (eth_link->link_autoneg == 0) ? "FIXED" : "AUTONEG",
        (eth_link->link_status == 0) ? "DOWN" : "UP");

    return te_log_buf_get(tlbp);
}

void
rpc_rte_eth_link_get_nowait(rcf_rpc_server *rpcs, uint8_t port_id,
                            struct tarpc_rte_eth_link *eth_link)
{
    tarpc_rte_eth_link_get_nowait_in   in;
    tarpc_rte_eth_link_get_nowait_out  out;
    te_log_buf                        *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (eth_link == NULL)
        TEST_FAIL("Invalid %s() 'eth_link' argument", __func__);

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_link_get_nowait", &in, &out);

    *eth_link = out.eth_link;

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_link_get_nowait, "%hhu",
                 "eth_link = %s", in.port_id,
                 tarpc_rte_eth_link2str(tlbp, eth_link));
    te_log_buf_free(tlbp);

    RETVAL_VOID(rte_eth_link_get_nowait);
}

void
rpc_rte_eth_link_get(rcf_rpc_server *rpcs,
                     uint8_t port_id,
                     struct tarpc_rte_eth_link *eth_link)
{
    tarpc_rte_eth_link_get_in   in;
    tarpc_rte_eth_link_get_out  out;
    te_log_buf                 *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (eth_link == NULL)
        TEST_FAIL("Invalid %s() 'eth_link' argument", __func__);

    in.port_id = port_id;

    rcf_rpc_call(rpcs, "rte_eth_link_get", &in, &out);

    *eth_link = out.eth_link;

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_link_get, "%hhu",
                 "eth_link = %s", in.port_id,
                 tarpc_rte_eth_link2str(tlbp, eth_link));
    te_log_buf_free(tlbp);

    RETVAL_VOID(rte_eth_link_get);
}

int rpc_rte_eth_dev_flow_ctrl_set(rcf_rpc_server *rpcs, uint8_t port_id,
                                  struct tarpc_rte_eth_fc_conf *fc_conf)
{
    tarpc_rte_eth_dev_flow_ctrl_set_in   in;
    tarpc_rte_eth_dev_flow_ctrl_set_out  out;
    te_log_buf                          *tlbp;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if (fc_conf == NULL)
    {
        ERROR("%s(): No flow control parameters", __FUNCTION__);
        RETVAL_ZERO_INT(rte_eth_dev_flow_ctrl_set, -1);
    }

    in.port_id = port_id;
    in.fc_conf = *fc_conf;

    rcf_rpc_call(rpcs, "rte_eth_dev_flow_ctrl_set", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_NEG_ERRNO(rte_eth_dev_flow_ctrl_set,
                                          out.retval);

    tlbp = te_log_buf_alloc();
    TAPI_RPC_LOG(rpcs, rte_eth_dev_flow_ctrl_set, "%hhu, %s",
                 NEG_ERRNO_FMT, in.port_id,
                 tarpc_rte_eth_fc_conf2str(tlbp, fc_conf),
                 NEG_ERRNO_ARGS(out.retval));
    te_log_buf_free(tlbp);

    RETVAL_ZERO_INT(rte_eth_dev_flow_ctrl_set, out.retval);
}

static void
tapi_rpc_rte_packet_type_mask2str(te_log_buf *tlbp, uint32_t ptype_mask)
{
#define CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(_layer, _type) \
    case TARPC_RTE_PTYPE_##_layer##_##_type << TARPC_RTE_PTYPE_##_layer##_OFFSET: \
        te_log_buf_append(tlbp, "%s_%s", #_layer, #_type);                        \
        break

    switch (ptype_mask) {
        case 0:
            te_log_buf_append(tlbp, "NONE");
            break;
        case TARPC_RTE_PTYPE_L2_MASK:
            te_log_buf_append(tlbp, "L2_ALL");
            break;
        case TARPC_RTE_PTYPE_L3_MASK:
            te_log_buf_append(tlbp, "L3_ALL");
            break;
        case TARPC_RTE_PTYPE_L4_MASK:
            te_log_buf_append(tlbp, "L4_ALL");
            break;
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L2, ETHER);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L2, ETHER_TIMESYNC);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L2, ETHER_ARP);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L2, ETHER_LLDP);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L3, IPV4);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L3, IPV4_EXT);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L3, IPV4_EXT_UNKNOWN);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L3, IPV6);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L3, IPV6_EXT);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L3, IPV6_EXT_UNKNOWN);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L4, TCP);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L4, UDP);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L4, FRAG);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L4, SCTP);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L4, ICMP);
        CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR(L4, NONFRAG);
        default:
            te_log_buf_append(tlbp, "UNKNOWN_TYPE");
            break;
    }

#undef CASE_TARPC_RTE_PKTMBUF_PTYPE_MASK2STR
}

static void
tarpc_rte_packet_type_mask_arg2str(te_log_buf  *tlbp, uint32_t ptype_mask)
{
    tapi_rpc_rte_packet_type_mask2str(tlbp, ptype_mask & TARPC_RTE_PTYPE_L2_MASK);
    te_log_buf_append(tlbp, " | ");
    tapi_rpc_rte_packet_type_mask2str(tlbp, ptype_mask & TARPC_RTE_PTYPE_L3_MASK);
    te_log_buf_append(tlbp, " | ");
    tapi_rpc_rte_packet_type_mask2str(tlbp, ptype_mask & TARPC_RTE_PTYPE_L4_MASK);
}

static void
tarpc_rte_supported_ptypes2str(te_log_buf *tlbp, uint32_t *ptypes, int num)
{
    int i;

    te_log_buf_append(tlbp, ": ");
    tapi_rpc_rte_packet_type_mask2str(tlbp, ptypes[0]);
    for (i = 1; i < num; i++)
    {
        te_log_buf_append(tlbp, " | ");
        tapi_rpc_rte_packet_type_mask2str(tlbp, ptypes[i]);
    }
}

int
rpc_rte_eth_dev_get_supported_ptypes(rcf_rpc_server *rpcs, uint8_t port_id,
                                     uint32_t ptype_mask, uint32_t *ptypes,
                                     int num)
{
    tarpc_rte_eth_dev_get_supported_ptypes_in   in;
    tarpc_rte_eth_dev_get_supported_ptypes_out  out;
    te_log_buf                                 *tlbp_arg;
    te_log_buf                                 *tlbp_ret;
    int                                         i;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    if ((num != 0) && (ptypes == NULL))
    {
        ERROR("%s(): No array of ptypes, but num is greater than 0", __FUNCTION__);
        RETVAL_ZERO_INT(rte_eth_dev_get_supported_ptypes, -1);
    }

    in.port_id = port_id;
    in.ptype_mask = ptype_mask;
    in.num = num;

    rcf_rpc_call(rpcs, "rte_eth_dev_get_supported_ptypes", &in, &out);

    CHECK_RETVAL_VAR_ERR_COND(rte_eth_dev_get_supported_ptypes,
                              out.retval, FALSE,
                              -TE_RC(TE_TAPI, TE_ECORRUPTED),
                              (out.retval < 0));
    tlbp_arg = te_log_buf_alloc();
    tlbp_ret = te_log_buf_alloc();

    tarpc_rte_packet_type_mask_arg2str(tlbp_arg, ptype_mask);

    if (ptypes != NULL && out.retval > 0)
    {
        for (i = 0; i < MIN(num, out.retval); i++)
            ptypes[i] = out.ptypes.ptypes_val[i];

        tarpc_rte_supported_ptypes2str(tlbp_ret, ptypes, MIN(num, out.retval));
    }

    TAPI_RPC_LOG(rpcs, rpc_rte_eth_dev_get_supported_ptypes,
                 "%hhu, %s", "%s%s", in.port_id, te_log_buf_get(tlbp_arg),
                 NEG_ERRNO_ARGS(out.retval), te_log_buf_get(tlbp_ret));
    te_log_buf_free(tlbp_arg);
    te_log_buf_free(tlbp_ret);

    RETVAL_INT(rte_eth_dev_get_supported_ptypes, out.retval);
}
