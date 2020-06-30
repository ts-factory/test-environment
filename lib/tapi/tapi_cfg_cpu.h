/** @file
 * @brief Test API to configure CPUs.
 *
 * Definition of API to configure CPUs.
 *
 * Copyright (C) 2018-2019 OKTET Labs. All rights reserved.
 *
 * @author Igor Romanov<Igor.Romanov@oktetlabs.ru>
 */

#ifndef __TE_TAPI_CFG_CPU_H__
#define __TE_TAPI_CFG_CPU_H__

#include "conf_api.h"
#include "te_defs.h"
#include "te_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup tapi_conf_cpu CPU topology configuration of Test Agents
 * @ingroup tapi_conf
 * @{
 */

/** Identifier of a logical CPU (CPU thread) */
typedef struct tapi_cpu_index_t {
    unsigned long node_id;
    unsigned long package_id;
    unsigned long core_id;
    unsigned long thread_id;
} tapi_cpu_index_t;

/** CPU properties that can be requested when looking for a CPU */
typedef struct tapi_cpu_prop_t {
    te_bool isolated;
} tapi_cpu_prop_t;

/**
 * Grab a CPU on a test agent with requested index
 *
 * @param[in]  ta               Test Agent
 * @param[in]  cpu_id           CPU index
 *
 * @return Status code.
 */
te_errno tapi_cfg_cpu_grab_by_id(const char *ta,
                                 const tapi_cpu_index_t *cpu_id);

/**
 * Release a CPU on a test agent with requested index
 *
 * @param[in]  ta               Test Agent
 * @param[in]  cpu_id           CPU index
 *
 * @return Status code.
 */
te_errno tapi_cfg_cpu_release_by_id(const char *ta,
                                    const tapi_cpu_index_t *cpu_id);

/**
 * Grab a CPU on a test agent with requested properties (if specified)
 * as a resource and retrieve its index.
 *
 * @param[in]  ta               Test Agent
 * @param[in]  prop             CPU properties. May be @c NULL to grab
 *                              any available CPU
 * @param[out] cpu_id           Index of grabbed CPU
 *
 * @return Status code.
 */
te_errno tapi_cfg_cpu_grab_by_prop(const char *ta, const tapi_cpu_prop_t *prop,
                                   tapi_cpu_index_t *cpu_id);

/**
 * Get all available CPU threads indices on a test agents.
 *
 * @param[in]  ta               Test Agent
 * @param[out] size             Number of CPU threads (size of @p indices)
 * @param[out] indices          CPU thread indices (might be @c NULL)
 */
extern te_errno tapi_cfg_get_all_threads(const char *ta,  size_t *size,
                                         tapi_cpu_index_t **indices);

/**@} <!-- END tapi_conf_cpu --> */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_CFG_CPU_H__ */
