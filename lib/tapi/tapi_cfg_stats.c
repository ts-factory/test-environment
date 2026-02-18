/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Test API to access network statistics via configurator.
 *
 * Implementation of API to configure network statistics.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER     "TAPI CFG stats"

#include "te_config.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#endif

#include "te_defs.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_stats.h"


#ifdef U64_FMT
#undef U64_FMT
#endif

#ifdef _U64_FMT
#undef _U64_FMT
#endif

#ifdef I64_FMT
#undef I64_FMT
#endif

#ifdef _I64_FMT
#undef _I64_FMT
#endif

#define U64_FMT "%" TE_PRINTF_64 "u"
#define I64_FMT "%" TE_PRINTF_64 "u"

#define _U64_FMT " " U64_FMT
#define _I64_FMT " " I64_FMT


typedef struct tapi_cfg_if_irq_per_cpu_diff {
    unsigned int  cpu;
    bool          old;
    bool          new;
    uint64_t      old_num;
    uint64_t      new_num;
} tapi_cfg_if_irq_per_cpu_diff;

typedef struct tapi_cfg_if_irq_stats_diff {
    unsigned int   irq_num;
    char          *name;
    bool           old;
    bool           new;
    te_vec         diff_irq_per_cpu;
} tapi_cfg_if_irq_stats_diff;

struct tapi_cfg_if_xstat_diff {
    char          name[TAPI_CFG_MAX_XSTAT_NAME];
    bool          old;
    bool          new;
    uint64_t      old_value;
    uint64_t      new_value;
};

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_if_stats_get(const char          *ta,
                            const char          *ifname,
                            tapi_cfg_if_stats   *stats)
{
    te_errno            rc;

    VERB("%s(ta=%s, ifname=%s, stats=%x) started",
         __FUNCTION__, ta, ifname, stats);

    if (stats == NULL)
    {
        ERROR("%s(): stats=NULL!", __FUNCTION__);
        return TE_OS_RC(TE_TAPI, EINVAL);
    }

    memset(stats, 0, sizeof(tapi_cfg_if_stats));

    VERB("%s(): stats=%x", __FUNCTION__, stats);

    /*
     * Synchronize configuration trees and get assigned interfaces
     */

    VERB("Try to sync stats");

    rc = cfg_synchronize_fmt(true, "/agent:%s/interface:%s/stats:",
                             ta, ifname);
    if (rc != 0)
        return rc;

    VERB("Get stats counters");

#define TAPI_CFG_STATS_IF_COUNTER_GET(_counter_) \
    do                                                      \
    {                                                       \
        VERB("IF_COUNTER_GET(%s)", #_counter_);             \
        rc = cfg_get_instance_uint64_fmt(&stats->_counter_, \
                 "/agent:%s/interface:%s/stats:/%s:",       \
                 ta, ifname, #_counter_);                   \
        if (rc != 0)                                        \
        {                                                   \
            ERROR("Failed to get %s counter for "           \
                  "interface %s on %s Test Agent: %r",      \
                  #_counter_, ifname, ta, rc);              \
            return rc;                                      \
        }                                                   \
    } while (0)

    TAPI_CFG_STATS_IF_COUNTER_GET(in_octets);
    TAPI_CFG_STATS_IF_COUNTER_GET(in_ucast_pkts);
    TAPI_CFG_STATS_IF_COUNTER_GET(in_nucast_pkts);
    TAPI_CFG_STATS_IF_COUNTER_GET(in_discards);
    TAPI_CFG_STATS_IF_COUNTER_GET(in_errors);
    TAPI_CFG_STATS_IF_COUNTER_GET(in_unknown_protos);
    TAPI_CFG_STATS_IF_COUNTER_GET(out_octets);
    TAPI_CFG_STATS_IF_COUNTER_GET(out_ucast_pkts);
    TAPI_CFG_STATS_IF_COUNTER_GET(out_nucast_pkts);
    TAPI_CFG_STATS_IF_COUNTER_GET(out_discards);
    TAPI_CFG_STATS_IF_COUNTER_GET(out_errors);

#undef TAPI_CFG_STATS_IF_COUNTER_GET

    return 0;
}

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_if_xstats_get(const char         *ta,
                             const char         *ifname,
                             tapi_cfg_if_xstats *xstats)
{
    te_errno            rc;
    unsigned int        n_xstats;
    cfg_handle         *xstats_handles;
    size_t              req_size;
    tapi_cfg_if_xstat  *result;
    unsigned int        i;

    VERB("%s(ta=%s, ifname=%s, xstats=%x) started",
         __FUNCTION__, ta, ifname, xstats);

    if (xstats == NULL || ifname == NULL || ta == NULL)
    {
        ERROR("%s(): ta, ifname or xstats is NULL!", __FUNCTION__);
        return TE_OS_RC(TE_TAPI, EINVAL);
    }

    VERB("%s(): xstats=%x", __FUNCTION__, xstats);

    /*
     * Synchronize configuration trees and get assigned interfaces
     */

    VERB("Try to sync xstats");

    rc = cfg_synchronize_fmt(true, "/agent:%s/interface:%s/xstats:",
                             ta, ifname);
    if (rc != 0)
        return rc;

    VERB("Get xstats counters");
    rc = cfg_find_pattern_fmt(&n_xstats, &xstats_handles,
                              "/agent:%s/interface:%s/xstats:/xstat:*",
                              ta, ifname);

    if ((rc != 0 && TE_RC_GET_ERROR(rc) == TE_ENOENT) ||
        (rc == 0 && n_xstats == 0))
    {
        xstats->num = 0;
        return 0;
    }
    if (rc != 0)
    {
        ERROR("cfg_find_pattern_fmt(/agent/interface/xstats/xstat) failed %r",
              rc);
        return rc;
    }

    req_size = sizeof(tapi_cfg_if_xstat) * n_xstats;
    result = TE_ALLOC(req_size);

    for (i = 0; i < n_xstats; i++)
    {
        uint64_t  xstat_val;
        char     *xstat_oid = NULL;
        char     *xstat_name;

        /* Get net OID as string */
        rc = cfg_get_oid_str(xstats_handles[i], &xstat_oid);
        if (rc != 0)
        {
            ERROR("%s(): cfg_get_oid_str() failed %r", __FUNCTION__, rc);
            break;
        }

        rc = cfg_get_uint64(&xstat_val, "%s", xstat_oid);
        if (rc != 0)
        {
            ERROR("%s(): failed to get value for %s", __FUNCTION__, xstat_oid);
            free(xstat_oid);
            break;
        }

        xstat_name = cfg_oid_str_get_inst_name(xstat_oid, -1);
        if (xstat_name == NULL)
        {
            ERROR("%s(): Failed to get the last instance name from OID '%s'",
                  __FUNCTION__, xstat_oid);
            free(xstat_oid);
            rc = TE_RC(TE_TAPI, TE_EFAULT);
            break;
        }

        te_strlcpy(result[i].name, xstat_name, TAPI_CFG_MAX_XSTAT_NAME);
        result[i].value = xstat_val;
        free(xstat_oid);
        free(xstat_name);
    }

    free(xstats_handles);
    if (rc != 0)
    {
        free(result);
    }
    else
    {
        xstats->xstats = result;
        xstats->num = n_xstats;
    }

    return rc;
}

void
tapi_cfg_stats_free_irqs_vec(const void *data)
{
    const te_vec *vec = data;
    const tapi_cfg_if_irq_stats *irq_stats_obj;
    const te_dbuf *dbuf = &vec->data;

    TE_VEC_FOREACH(vec, irq_stats_obj)
    {
        dbuf = &irq_stats_obj->irq_per_cpu.data;
        te_vec_item_free_ptr(dbuf);
        free(irq_stats_obj->name);
    }
    dbuf = &vec->data;
    te_vec_item_free_ptr(dbuf);
}

static void
tapi_cfg_stats_free_irqs_diff(te_vec *vec)
{
    tapi_cfg_if_irq_stats_diff *irq_diff_obj;

    TE_VEC_FOREACH(vec, irq_diff_obj)
    {
        te_vec_free(&irq_diff_obj->diff_irq_per_cpu);
    }
    te_vec_free(vec);
}

static int
compare_irqs(const void *obj1, const void *obj2)
{
    const tapi_cfg_if_irq_stats *r1 = obj1;
    const tapi_cfg_if_irq_stats *r2 = obj2;

    return (int)r1->irq_num - (int)r2->irq_num;
}

static int
compare_ipc(const void *obj1, const void *obj2)
{
    const tapi_cfg_if_irq_per_cpu *r1 = obj1;
    const tapi_cfg_if_irq_per_cpu *r2 = obj2;

    return (int)r1->cpu - (int)r2->cpu;
}

static void
tapi_cfg_stats_sort_irqs_vec(te_vec *irqs_vec)
{
    tapi_cfg_if_irq_stats *irq_stats_obj;

    te_vec_sort(irqs_vec, &compare_irqs);
    TE_VEC_FOREACH(irqs_vec, irq_stats_obj)
    {
        te_vec_sort(&irq_stats_obj->irq_per_cpu, &compare_ipc);
    }
}

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_if_irq_stats_get(const char *ta,
                                const char *ifname,
                                te_vec     *irq_stats)
{
    te_errno rc;
    unsigned int n_irqs;
    cfg_handle *irq_handles = NULL;
    cfg_handle *cpu_handles = NULL;
    tapi_cfg_if_irq_stats irq_stat = { .name = NULL };
    int i;

    VERB("%s(ta=%s, ifname=%s, irq_stats=%x) started",
         __FUNCTION__, ta, ifname, irq_stats);

    if (irq_stats == NULL || ifname == NULL || ta == NULL)
    {
        ERROR("%s(): ta, ifname or irq_stats is NULL!", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    VERB("%s(): irq_stats=%x", __FUNCTION__, irq_stats);

    *irq_stats = TE_VEC_INIT(tapi_cfg_if_irq_stats);
    irq_stat.irq_per_cpu = TE_VEC_INIT(tapi_cfg_if_irq_per_cpu);

    /*
     * Synchronize configuration trees and get assigned interfaces
     */

    VERB("Try to sync irq subtree.");
    rc = cfg_synchronize_fmt(true, "/agent:%s/interface:%s/irq:",
                             ta, ifname);
    if (rc != 0)
        goto cleanup;

    VERB("Get irq stats counters");
    rc = cfg_find_pattern_fmt(&n_irqs, &irq_handles,
                              "/agent:%s/interface:%s/irq:*",
                              ta, ifname);
    if ((rc != 0 && TE_RC_GET_ERROR(rc) == TE_ENOENT) ||
        (rc == 0 && n_irqs == 0))
        return 0;
    if (rc != 0)
    {
        ERROR("cfg_find_pattern_fmt(/agent/interface/irq/) failed %r", rc);
        goto cleanup;
    }

    for (i = 0; i < n_irqs; i++)
    {
        char *irq_oid;
        unsigned int n_cpus;
        char *irq_num_str;
        int j;

        /* Get net OID as string */
        rc = cfg_get_oid_str(irq_handles[i], &irq_oid);
        if (rc != 0)
        {
            ERROR("%s(): cfg_get_oid_str() failed %r", __FUNCTION__, rc);
            break;
        }

        rc = cfg_get_string(&irq_stat.name, "%s/name:", irq_oid);
        if (rc != 0)
        {
            ERROR("%s(): failed to get value for %s/name", __FUNCTION__,
                  irq_oid);
            free(irq_oid);
            break;
        }

        irq_num_str = cfg_oid_str_get_inst_name(irq_oid, -1);
        if (irq_num_str == NULL)
        {
            ERROR("%s(): Failed to get the last instance name from OID '%s'",
                  __FUNCTION__, irq_oid);
            free(irq_oid);
            rc = TE_RC(TE_TAPI, TE_EFAULT);
            break;
        }

        rc = te_strtoui(irq_num_str, 10, &irq_stat.irq_num);
        free(irq_num_str);
        if (rc != 0)
        {
            ERROR("%s(): Failed to convert the last instance name for OID '%s'",
                  __FUNCTION__, irq_oid);
            free(irq_oid);
            rc = TE_RC(TE_TAPI, rc);
            break;
        }

        rc = cfg_find_pattern_fmt(&n_cpus, &cpu_handles, "%s/cpu:*", irq_oid);
        free(irq_oid);
        if ((rc != 0 && TE_RC_GET_ERROR(rc) == TE_ENOENT) ||
            (rc == 0 && n_cpus == 0))
            continue;

        if (rc != 0)
        {
            ERROR("cfg_find_pattern_fmt(/agent/interface/irq/cpu) failed %r",
                  rc);
            break;
        }

        for (j = 0; j < n_cpus; j++)
        {
            char *cpu_oid;
            char *cpu_num_str;
            tapi_cfg_if_irq_per_cpu irq_per_cpu;

            /* Get net OID as string */
            rc = cfg_get_oid_str(cpu_handles[j], &cpu_oid);
            if (rc != 0)
            {
                ERROR("%s(): cfg_get_oid_str() failed %r", __FUNCTION__, rc);
                break;
            }

            rc = cfg_get_uint64(&irq_per_cpu.num, "%s", cpu_oid);
            if (rc != 0)
            {
                ERROR("%s(): failed to get value for '%s'", __FUNCTION__,
                      cpu_oid);
                free(cpu_oid);
                goto cleanup;
            }

            cpu_num_str = cfg_oid_str_get_inst_name(cpu_oid, -1);
            if (cpu_num_str == NULL)
            {
                ERROR("%s(): Failed to get the last instance name from OID "
                      "'%s'", __FUNCTION__, cpu_oid);
                free(cpu_oid);
                rc = TE_RC(TE_TAPI, TE_EFAULT);
                goto cleanup;
            }

            rc = te_strtoui(cpu_num_str, 10, &irq_per_cpu.cpu);
            free(cpu_num_str);
            free(cpu_oid);
            if (rc != 0)
            {
                ERROR("%s(): Failed to convert the last instance name for OID "
                      "'%s'", __FUNCTION__, cpu_oid);
                rc = TE_RC(TE_TAPI, rc);
                goto cleanup;
            }
            rc = TE_VEC_APPEND(&irq_stat.irq_per_cpu, irq_per_cpu);
            if (rc != 0)
            {
                ERROR("%s(): Failed to add irq per cpu stats to te_vec.",
                      __FUNCTION__);
                rc = TE_RC(TE_TAPI, rc);
                goto cleanup;
            }
        }

        rc = TE_VEC_APPEND(irq_stats, irq_stat);
        if (rc != 0)
        {
            ERROR("%s(): Failed to add irq stats instance to te_vec.",
                  __FUNCTION__);
            rc = TE_RC(TE_TAPI, rc);
            break;
        }
        /**
         * This pointer now in 'irq_per_cpu' vector, so we don't need it to
         * delete it at cleanup.
         */
        irq_stat.name = NULL;
        irq_stat.irq_per_cpu = TE_VEC_INIT(tapi_cfg_if_irq_per_cpu);
        free(cpu_handles);
        cpu_handles = NULL;
    }

    tapi_cfg_stats_sort_irqs_vec(irq_stats);

cleanup:
    free(irq_handles);
    free(cpu_handles);
    free(irq_stat.name);
    te_vec_free(&irq_stat.irq_per_cpu);
    if (rc != 0)
        tapi_cfg_stats_free_irqs_vec(irq_stats);

    return rc;
}


/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_net_stats_get(const char          *ta,
                             tapi_cfg_net_stats  *stats)
{
    te_errno            rc;

    VERB("%s(ta=%s) started", __FUNCTION__, ta);

    if (stats == NULL)
    {
        ERROR("%s(): stats=NULL!", __FUNCTION__);
        return TE_OS_RC(TE_TAPI, EINVAL);
    }

    memset(stats, 0, sizeof(tapi_cfg_net_stats));

    /*
     * Synchronize configuration trees and get assigned interfaces
     */

    VERB("Try to sync stats");

    rc = cfg_synchronize_fmt(true, "/agent:%s/stats:", ta);
    if (rc != 0)
        return rc;

    VERB("Get stats counters");

#define TAPI_CFG_STATS_IPV4_COUNTER_GET(_counter_) \
    do                                                          \
    {                                                           \
        VERB("IPV4_COUNTER_GET(%s)", #_counter_);               \
        rc = cfg_get_instance_uint64_fmt(                       \
                                  &stats->ipv4._counter_,       \
                                  "/agent:%s/stats:/ipv4_%s:",  \
                                  ta, #_counter_);              \
        if (rc != 0)                                            \
        {                                                       \
            ERROR("Failed to get ipv4_%s counter from %s "      \
                  "Test Agent: %r", #_counter_, ta, rc);        \
            return rc;                                          \
        }                                                       \
    } while (0)

    TAPI_CFG_STATS_IPV4_COUNTER_GET(in_recvs);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(in_hdr_errs);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(in_addr_errs);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(forw_dgrams);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(in_unknown_protos);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(in_discards);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(in_delivers);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(out_requests);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(out_discards);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(out_no_routes);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(reasm_timeout);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(reasm_reqds);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(reasm_oks);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(reasm_fails);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(frag_oks);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(frag_fails);
    TAPI_CFG_STATS_IPV4_COUNTER_GET(frag_creates);

#undef TAPI_CFG_STATS_IPV4_COUNTER_GET

#define TAPI_CFG_STATS_ICMP_COUNTER_GET(_counter_) \
    do                                                          \
    {                                                           \
        VERB("ICMP_COUNTER_GET(%s)", #_counter_);               \
        rc = cfg_get_instance_uint64_fmt(                       \
                                  &stats->icmp._counter_,       \
                                  "/agent:%s/stats:/icmp_%s:",  \
                                  ta, #_counter_);              \
        if (rc != 0)                                            \
        {                                                       \
            ERROR("Failed to get icmp_%s counter from %s "      \
                  "Test Agent: %r", #_counter_, ta, rc);        \
            return rc;                                          \
        }                                                       \
    } while (0)

    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_msgs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_errs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_dest_unreachs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_time_excds);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_parm_probs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_src_quenchs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_redirects);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_echos);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_echo_reps);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_timestamps);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_timestamp_reps);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_addr_masks);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(in_addr_mask_reps);

    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_msgs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_errs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_dest_unreachs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_time_excds);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_parm_probs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_src_quenchs);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_redirects);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_echos);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_echo_reps);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_timestamps);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_timestamp_reps);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_addr_masks);
    TAPI_CFG_STATS_ICMP_COUNTER_GET(out_addr_mask_reps);

#undef TAPI_CFG_STATS_ICMP_COUNTER_GET

    return 0;
}

static void
tapi_cfg_stats_if_stats_print_with_descr_va(const tapi_cfg_if_stats *stats,
                                            bool print_zeros,
                                            const char *descr_fmt, va_list ap)
{
    te_string buf = TE_STRING_INIT;

    te_string_append_va(&buf, descr_fmt, ap);

#define PRINT_STAT(_name) \
    do {                                                    \
        if (print_zeros || stats->_name != 0)                \
            te_string_append(&buf, "\n  %s : " U64_FMT,     \
                             #_name, stats->_name);         \
    } while (0)

    PRINT_STAT(in_octets);
    PRINT_STAT(in_ucast_pkts);
    PRINT_STAT(in_nucast_pkts);
    PRINT_STAT(in_discards);
    PRINT_STAT(in_errors);
    PRINT_STAT(in_unknown_protos);
    PRINT_STAT(out_octets);
    PRINT_STAT(out_ucast_pkts);
    PRINT_STAT(out_nucast_pkts);
    PRINT_STAT(out_discards);
    PRINT_STAT(out_errors);

#undef PRINT_STAT

    RING("%s", te_string_value(&buf));

    te_string_free(&buf);
}

static void
tapi_cfg_stats_if_stats_print_with_descr(const tapi_cfg_if_stats *stats,
                                          const char *descr_fmt, ...)
{
    va_list ap;

    va_start(ap, descr_fmt);
    tapi_cfg_stats_if_stats_print_with_descr_va(stats, true, descr_fmt, ap);
    va_end(ap);
}

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_if_stats_print(const char          *ta,
                              const char          *ifname,
                              tapi_cfg_if_stats   *stats)
{
    if (stats == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    tapi_cfg_stats_if_stats_print_with_descr(stats,
        "Network statistics for interface %s on Test Agent %s:", ifname, ta);

    return 0;
}

static void
tapi_cfg_stats_if_xstats_print_with_descr(const tapi_cfg_if_xstats *xstats,
                                          const char *descr_fmt, ...)
{
    int i;
    va_list ap;
    te_string buf = TE_STRING_INIT;

    va_start(ap, descr_fmt);
    te_string_append_va(&buf, descr_fmt, ap);
    va_end(ap);

    for (i = 0; i < xstats->num; i++)
    {
        tapi_cfg_if_xstat xstat = xstats->xstats[i];
        te_string_append(&buf, "\n  %s : " U64_FMT,
                         xstat.name, xstat.value);
    }

    RING("%s", te_string_value(&buf));

    te_string_free(&buf);
}

static void
tapi_cfg_stats_if_xstats_print_diff_va(struct tapi_cfg_if_xstat_diff *diff,
                                       size_t diff_num, const char *descr_fmt,
                                       va_list ap)
{
    int i;
    te_string buf = TE_STRING_INIT;

    te_string_append_va(&buf, descr_fmt, ap);

    for (i = 0; i < diff_num; i++)
    {
        if (diff[i].new && diff[i].old)
        {
            uint64_t diff_val;

            diff_val = diff[i].new_value - diff[i].old_value;
            if (diff_val != 0)
                te_string_append(&buf, "\n  %s : " U64_FMT,
                                 diff[i].name, diff_val);
        }
        else if (diff[i].old)
        {
            te_string_append(&buf, "\n  %s : " U64_FMT "(old only)",
                                 diff[i].name, diff[i].old_value);
        }
        else if (diff[i].new)
        {
            te_string_append(&buf, "\n  %s : " U64_FMT "(new only)",
                                 diff[i].name, diff[i].new_value);
        }

    }

    RING("%s", te_string_value(&buf));

    te_string_free(&buf);
}

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_if_xstats_print(const char          *ta,
                               const char          *ifname,
                               tapi_cfg_if_xstats  *xstats)
{
    if (xstats == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    tapi_cfg_stats_if_xstats_print_with_descr(xstats,
        "Network extended statistics for interface %s on Test Agent %s:",
        ifname, ta);

    return 0;
}

static void
tapi_cfg_stats_if_irq_print_with_descr(const te_vec *irq_stats,
                                       const char *descr_fmt, ...)
{
    va_list ap;
    te_string buf = TE_STRING_INIT;
    const tapi_cfg_if_irq_stats *irq_stats_obj;
    const tapi_cfg_if_irq_per_cpu *irq_per_cpu;

    va_start(ap, descr_fmt);
    te_string_append_va(&buf, descr_fmt, ap);
    va_end(ap);

    TE_VEC_FOREACH(irq_stats, irq_stats_obj)
    {
        TE_VEC_FOREACH(&irq_stats_obj->irq_per_cpu, irq_per_cpu)
        {
            te_string_append(&buf, "\n  IRQ:%u(%s) CPU:%u : " U64_FMT,
                             irq_stats_obj->irq_num, irq_stats_obj->name,
                             irq_per_cpu->cpu, irq_per_cpu->num);
        }
    }

    RING("%s", te_string_value(&buf));

    te_string_free(&buf);
}

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_if_irq_print(const char *ta,
                            const char *ifname,
                            te_vec     *irq_stats)
{
    if (irq_stats == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    tapi_cfg_stats_if_irq_print_with_descr(irq_stats,
        "Network extended statistics for interface %s on Test Agent %s:",
        ifname, ta);

    return 0;
}

static void
tapi_cfg_if_stats_diff(tapi_cfg_if_stats *diff,
                       const tapi_cfg_if_stats *stats,
                       const tapi_cfg_if_stats *prev)
{
    /* It is a bit dirty, but I have no time to restructure stats to be array */
    const size_t n_stats = sizeof(*diff) / sizeof(uint64_t);
    const uint64_t *stats_u64 = (const uint64_t *)stats;
    const uint64_t *prev_u64 = (const uint64_t *)prev;
    uint64_t *diff_u64 = (uint64_t *)diff;
    size_t i;

    for (i = 0; i < n_stats; ++i)
        diff_u64[i] = stats_u64[i] - prev_u64[i];
}

te_errno
tapi_cfg_stats_if_stats_print_diff(const tapi_cfg_if_stats *stats,
                                   const tapi_cfg_if_stats *prev,
                                   const char *descr_fmt, ...)
{
    const tapi_cfg_if_stats *diff = stats;
    tapi_cfg_if_stats diff_buf;
    va_list ap;

    if (stats == NULL || descr_fmt == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    if (prev != NULL)
    {
        tapi_cfg_if_stats_diff(&diff_buf, stats, prev);
        diff = &diff_buf;
    }

    va_start(ap, descr_fmt);
    tapi_cfg_stats_if_stats_print_with_descr_va(diff, false, descr_fmt, ap);
    va_end(ap);

    return 0;
}

static tapi_cfg_if_xstat *
tapi_cfg_if_xstat_find_by_name(const char *xstat_name,
                               const tapi_cfg_if_xstats *xstats)
{
    size_t i;

    if (xstats == NULL)
        return NULL;

    for (i = 0; i < xstats->num; i++)
    {
        if (strcmp(xstats->xstats[i].name, xstat_name) == 0)
            return &(xstats->xstats[i]);
    }
    return NULL;
}

static size_t
tapi_cfg_if_xstats_diff(const tapi_cfg_if_xstats *new_xstats,
                        const tapi_cfg_if_xstats *old_xstats,
                        struct tapi_cfg_if_xstat_diff *diff)
{
    const tapi_cfg_if_xstat *xstat;
    size_t i;
    size_t cnt = 0;

    for (i = 0; new_xstats != NULL && i < new_xstats->num; i++)
    {
        const tapi_cfg_if_xstat *new_i = &(new_xstats->xstats[i]);

        te_strlcpy(diff[cnt].name, new_i->name, TAPI_CFG_MAX_XSTAT_NAME);
        diff[cnt].new = true;
        diff[cnt].new_value = new_i->value;
        xstat = tapi_cfg_if_xstat_find_by_name(new_i->name, old_xstats);
        if (xstat != NULL)
        {
            diff[cnt].old = true;
            diff[cnt].old_value = xstat->value;
        }
        else
        {
            diff[cnt].old = false;
        }
        cnt++;
    }

    for (i = 0; old_xstats != NULL && i < old_xstats->num; i++)
    {
        const tapi_cfg_if_xstat *old_i = &(old_xstats->xstats[i]);

        xstat = tapi_cfg_if_xstat_find_by_name(old_i->name, new_xstats);
        if (xstat != NULL)
            continue;
        te_strlcpy(diff[cnt].name, old_i->name, TAPI_CFG_MAX_XSTAT_NAME);
        diff[cnt].old = true;
        diff[cnt].old_value = old_i->value;
        diff[cnt].new = false;
        cnt++;
    }
    return cnt;
}

te_errno
tapi_cfg_stats_if_xstats_print_diff(const tapi_cfg_if_xstats *stats,
                                   const tapi_cfg_if_xstats *prev,
                                   const char *descr_fmt, ...)
{
    struct tapi_cfg_if_xstat_diff *diff;
    va_list ap;
    size_t diff_num;
    size_t diff_max_len;

    if (stats == NULL || descr_fmt == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    diff_max_len = stats->num;
    diff_max_len += (prev == NULL) ? 0 : prev->num;
    diff = TE_ALLOC(diff_max_len * sizeof(struct tapi_cfg_if_xstat_diff));
    diff_num = tapi_cfg_if_xstats_diff(stats, prev, diff);

    va_start(ap, descr_fmt);
    tapi_cfg_stats_if_xstats_print_diff_va(diff, diff_num, descr_fmt, ap);
    va_end(ap);

    free(diff);
    return 0;
}

static void
tapi_cfg_if_irq_diff_per_cpu(const te_vec *new_ipc_vec,
                             const te_vec *old_ipc_vec,
                             te_vec *diff_ipc_vec)
{

    const tapi_cfg_if_irq_per_cpu *ipc;
    const tapi_cfg_if_irq_per_cpu *aux_ipc;
    tapi_cfg_if_irq_per_cpu_diff ipc_diff_val;

    *diff_ipc_vec = TE_VEC_INIT(tapi_cfg_if_irq_per_cpu_diff);

    if (new_ipc_vec != NULL)
    {
        TE_VEC_FOREACH(new_ipc_vec, ipc)
        {
            unsigned int ipc_pos;
            bool ipc_found = false;

            if (old_ipc_vec != NULL)
                ipc_found = te_vec_search(old_ipc_vec, ipc,
                                          &compare_ipc, &ipc_pos, NULL);
            ipc_diff_val.cpu = ipc->cpu;
            ipc_diff_val.new = true;
            ipc_diff_val.old = ipc_found;
            ipc_diff_val.new_num = ipc->num;
            if (ipc_found)
            {
                aux_ipc = te_vec_get(old_ipc_vec, ipc_pos);
                ipc_diff_val.old_num = aux_ipc->num;
            }
            TE_VEC_APPEND(diff_ipc_vec, ipc_diff_val);
        }
    }
    if (old_ipc_vec == NULL)
        return;

    TE_VEC_FOREACH(old_ipc_vec, ipc)
    {
        unsigned int ipc_pos;
        bool ipc_found = false;

        if (new_ipc_vec != NULL)
        {
            ipc_found = te_vec_search(new_ipc_vec, ipc,
                                      &compare_ipc, &ipc_pos, NULL);
            if (ipc_found)
                continue;
        }
        ipc_diff_val.cpu = ipc->cpu;
        ipc_diff_val.new = false;
        ipc_diff_val.old = true;
        ipc_diff_val.old_num = ipc->num;
        TE_VEC_APPEND(diff_ipc_vec, ipc_diff_val);
    }
}

static void
tapi_cfg_if_irq_diff(const te_vec *new_irq_stats,
                     const te_vec *old_irq_stats,
                     te_vec *diff)
{
    const tapi_cfg_if_irq_stats *irq_stats_obj;
    const tapi_cfg_if_irq_stats *aux_irq_stats_obj;
    tapi_cfg_if_irq_stats_diff diff_val;

    TE_VEC_FOREACH(new_irq_stats, irq_stats_obj)
    {
        unsigned int irq_pos;
        bool irq_found = false;

        if (old_irq_stats != NULL)
            irq_found = te_vec_search(old_irq_stats, irq_stats_obj,
                                      &compare_irqs, &irq_pos, NULL);
        diff_val.irq_num = irq_stats_obj->irq_num;
        diff_val.name = irq_stats_obj->name;
        diff_val.new = true;
        diff_val.old = irq_found;
        if (irq_found)
        {
            aux_irq_stats_obj = te_vec_get(old_irq_stats, irq_pos);
            tapi_cfg_if_irq_diff_per_cpu(&irq_stats_obj->irq_per_cpu,
                                         &aux_irq_stats_obj->irq_per_cpu,
                                         &diff_val.diff_irq_per_cpu);
        }
        else
        {
            tapi_cfg_if_irq_diff_per_cpu(&irq_stats_obj->irq_per_cpu, NULL,
                                         &diff_val.diff_irq_per_cpu);
        }
        TE_VEC_APPEND(diff, diff_val);
    }

    if (old_irq_stats == NULL)
        return;

    TE_VEC_FOREACH(old_irq_stats, irq_stats_obj)
    {
        unsigned int irq_pos;
        bool irq_found;

        irq_found = te_vec_search(new_irq_stats, irq_stats_obj,
                                  &compare_irqs, &irq_pos, NULL);
        if (irq_found)
            continue;
        diff_val.irq_num = irq_stats_obj->irq_num;
        diff_val.name = irq_stats_obj->name;
        diff_val.new = false;
        diff_val.old = true;
        tapi_cfg_if_irq_diff_per_cpu(NULL, &irq_stats_obj->irq_per_cpu,
                                     &diff_val.diff_irq_per_cpu);
        TE_VEC_APPEND(diff, diff_val);
    }
}

static void
tapi_cfg_stats_if_irq_print_diff_va(te_vec *diff, const char *descr_fmt,
                                    va_list ap)
{
    tapi_cfg_if_irq_stats_diff *diff_irq;
    tapi_cfg_if_irq_per_cpu_diff *diff_ipc;
    te_string buf = TE_STRING_INIT;

    te_string_append_va(&buf, descr_fmt, ap);

    TE_VEC_FOREACH(diff, diff_irq)
    {
        TE_VEC_FOREACH(&diff_irq->diff_irq_per_cpu, diff_ipc)
        {
            if (diff_irq->new && diff_irq->old)
            {
                if (diff_ipc->new && diff_ipc->old)
                {
                    uint64_t diff_val;

                    diff_val = diff_ipc->new_num - diff_ipc->old_num;
                    if (diff_val == 0)
                        continue;

                    te_string_append(&buf, "\n  IRQ:%u(%s) CPU:%u : " U64_FMT,
                                    diff_irq->irq_num, diff_irq->name,
                                    diff_ipc->cpu, diff_val);
                }
                else if (diff_ipc->new)
                {
                    te_string_append(&buf,
                                     "\n  IRQ:%u(%s) CPU(new):%u : " U64_FMT,
                                     diff_irq->irq_num, diff_irq->name,
                                     diff_ipc->cpu, diff_ipc->new_num);
                }
                else if (diff_ipc->old)
                {
                    te_string_append(&buf,
                                     "\n  IRQ:%u(%s) CPU(old):%u : " U64_FMT,
                                     diff_irq->irq_num, diff_irq->name,
                                     diff_ipc->cpu, diff_ipc->old_num);
                }
            }
            else if(diff_irq->new)
            {
                te_string_append(&buf, "\n  IRQ(new):%u(%s) CPU:%u : " U64_FMT,
                                 diff_irq->irq_num, diff_irq->name,
                                 diff_ipc->cpu, diff_ipc->new_num);
            }
            else
            {
                te_string_append(&buf, "\n  IRQ(old):%u(%s) CPU:%u : " U64_FMT,
                                 diff_irq->irq_num, diff_irq->name,
                                 diff_ipc->cpu, diff_ipc->old_num);
            }
        }
    }

    RING("%s", te_string_value(&buf));

    te_string_free(&buf);
}


te_errno
tapi_cfg_stats_if_irq_print_diff(const te_vec *stats,
                                 const te_vec *prev,
                                 const char *descr_fmt, ...)
{
    te_vec diff;
    va_list ap;

    if (stats == NULL || descr_fmt == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    diff = TE_VEC_INIT(tapi_cfg_if_irq_stats_diff);
    tapi_cfg_if_irq_diff(stats, prev, &diff);

    va_start(ap, descr_fmt);
    tapi_cfg_stats_if_irq_print_diff_va(&diff, descr_fmt, ap);
    va_end(ap);
    tapi_cfg_stats_free_irqs_diff(&diff);

    return 0;
}


static void
tapi_cfg_stats_net_stats_print_with_descr_va(const tapi_cfg_net_stats *stats,
                                             bool print_zeros,
                                             const char *descr_fmt, va_list ap)
{
    te_string buf = TE_STRING_INIT;

    te_string_append_va(&buf, descr_fmt, ap);

    te_string_append(&buf, "\nIPv4:");

#define PRINT_STAT(_name) \
    do {                                                    \
        if (print_zeros || stats->ipv4._name != 0)           \
            te_string_append(&buf, "\n  %s : " U64_FMT,     \
                             #_name, stats->ipv4._name);    \
    } while (0)

    PRINT_STAT(in_recvs);
    PRINT_STAT(in_hdr_errs);
    PRINT_STAT(in_addr_errs);
    PRINT_STAT(forw_dgrams);
    PRINT_STAT(in_unknown_protos);
    PRINT_STAT(in_discards);
    PRINT_STAT(in_delivers);
    PRINT_STAT(out_requests);
    PRINT_STAT(out_discards);
    PRINT_STAT(out_no_routes);
    PRINT_STAT(reasm_timeout);
    PRINT_STAT(reasm_reqds);
    PRINT_STAT(reasm_oks);
    PRINT_STAT(reasm_fails);
    PRINT_STAT(frag_oks);
    PRINT_STAT(frag_fails);
    PRINT_STAT(frag_creates);

#undef PRINT_STAT

    te_string_append(&buf, "\nICMP:");

#define PRINT_STAT(_name) \
    do {                                                    \
        if (print_zeros || stats->icmp._name != 0)           \
            te_string_append(&buf, "\n  %s : " U64_FMT,     \
                             #_name, stats->icmp._name);    \
    } while (0)

    PRINT_STAT(in_msgs);
    PRINT_STAT(in_errs);
    PRINT_STAT(in_dest_unreachs);
    PRINT_STAT(in_time_excds);
    PRINT_STAT(in_parm_probs);
    PRINT_STAT(in_src_quenchs);
    PRINT_STAT(in_redirects);
    PRINT_STAT(in_echos);
    PRINT_STAT(in_echo_reps);
    PRINT_STAT(in_timestamps);
    PRINT_STAT(in_timestamp_reps);
    PRINT_STAT(in_addr_masks);
    PRINT_STAT(in_addr_mask_reps);
    PRINT_STAT(out_msgs);
    PRINT_STAT(out_errs);
    PRINT_STAT(out_dest_unreachs);
    PRINT_STAT(out_time_excds);
    PRINT_STAT(out_parm_probs);
    PRINT_STAT(out_src_quenchs);
    PRINT_STAT(out_redirects);
    PRINT_STAT(out_echos);
    PRINT_STAT(out_echo_reps);
    PRINT_STAT(out_timestamps);
    PRINT_STAT(out_timestamp_reps);
    PRINT_STAT(out_addr_masks);
    PRINT_STAT(out_addr_mask_reps);

#undef PRINT_STAT

    RING("%s", te_string_value(&buf));

    te_string_free(&buf);
}

static void
tapi_cfg_stats_net_stats_print_with_descr(const tapi_cfg_net_stats *stats,
                                          const char *descr_fmt, ...)
{
    va_list ap;

    va_start(ap, descr_fmt);
    tapi_cfg_stats_net_stats_print_with_descr_va(stats, true, descr_fmt, ap);
    va_end(ap);
}

/* See description in tapi_cfg_stats.h */
te_errno
tapi_cfg_stats_net_stats_print(const char *ta, tapi_cfg_net_stats *stats)
{
    if (stats == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    tapi_cfg_stats_net_stats_print_with_descr(stats,
        "Network statistics for Test Agent %s:", ta);

    return 0;
}

static void
tapi_cfg_net_stats_diff(tapi_cfg_net_stats *diff,
                        const tapi_cfg_net_stats *stats,
                        const tapi_cfg_net_stats *prev)
{
    /* It is a bit dirty, but I have no time to restructure stats to be array */
    const size_t n_stats = sizeof(*diff) / sizeof(uint64_t);
    const uint64_t *stats_u64 = (const uint64_t *)stats;
    const uint64_t *prev_u64 = (const uint64_t *)prev;
    uint64_t *diff_u64 = (uint64_t *)diff;
    size_t i;

    for (i = 0; i < n_stats; ++i)
        diff_u64[i] = stats_u64[i] - prev_u64[i];
}

te_errno
tapi_cfg_stats_net_stats_print_diff(const tapi_cfg_net_stats *stats,
                                    const tapi_cfg_net_stats *prev,
                                    const char *descr_fmt, ...)
{
    const tapi_cfg_net_stats *diff = stats;
    tapi_cfg_net_stats diff_buf;
    va_list ap;

    if (stats == NULL || descr_fmt == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    if (prev != NULL)
    {
        tapi_cfg_net_stats_diff(&diff_buf, stats, prev);
        diff = &diff_buf;
    }

    va_start(ap, descr_fmt);
    tapi_cfg_stats_net_stats_print_with_descr_va(diff, false, descr_fmt, ap);
    va_end(ap);

    return 0;
}
