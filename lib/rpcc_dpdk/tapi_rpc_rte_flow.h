/** @file
 * @brief RPC client API for DPDK flow API
 *
 * RPC client API for DPDK flow API functions
 *
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights reserved.
 *
 * 
 *
 *
 * @author Roman Zhukov <Roman.Zhukov@oktetlabs.ru>
 */

#ifndef __TE_TAPI_RPC_RTE_FLOW_H__
#define __TE_TAPI_RPC_RTE_FLOW_H__

#include "rcf_rpc.h"
#include "te_rpc_types.h"
#include "tapi_rpc_rte.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup te_lib_rpc_rte_flow TAPI for RTE FLOW API remote calls
 * @ingroup te_lib_rpc_tapi
 * @{
 */

/**
 * rte_flow_validate() RPC.
 *
 * @param[in]  port_id     Port number
 * @param[in]  attr        RTE flow attr pointer
 * @param[in]  pattern     RTE flow item pointer to the array of items
 * @param[in]  actions     RTE flow action pointer to the array of actions
 * @param[out] error       Perform verbose error reporting if not @c NULL
 *
 * @return @c 0 on success; jumps out in case of failure
 */
extern int rpc_rte_flow_validate(rcf_rpc_server *rpcs,
                                 uint16_t port_id,
                                 rpc_rte_flow_attr_p attr,
                                 rpc_rte_flow_item_p pattern,
                                 rpc_rte_flow_action_p actions,
                                 tarpc_rte_flow_error *error);

/**
 * rte_flow_create() RPC.
 *
 * @param[in]  port_id     Port number
 * @param[in]  attr        RTE flow attr pointer
 * @param[in]  pattern     RTE flow item pointer to the array of items
 * @param[in]  actions     RTE flow action pointer to the array of actions
 * @param[out] error       Perform verbose error reporting if not @c NULL
 *
 * @return RTE flow pointer on success; jumps out when pointer is @c NULL
 */
extern rpc_rte_flow_p rpc_rte_flow_create(rcf_rpc_server *rpcs,
                                          uint16_t port_id,
                                          rpc_rte_flow_attr_p attr,
                                          rpc_rte_flow_item_p pattern,
                                          rpc_rte_flow_action_p actions,
                                          tarpc_rte_flow_error *error);

/**
 * rte_flow_destroy() RPC.
 *
 * @param[in]  port_id     Port number
 * @param[in]  flow        RTE flow pointer
 * @param[out] error       Perform verbose error reporting if not @c NULL
 *
 * @return @c 0 on success; jumps out in case of failure
 */
extern int rpc_rte_flow_destroy(rcf_rpc_server *rpcs,
                                uint16_t port_id,
                                rpc_rte_flow_p flow,
                                tarpc_rte_flow_error *error);

/**
 * rte_flow_flush() RPC.
 *
 * @param[in]  rpcs        RPC server handle
 * @param[in]  port_id     Port number
 * @param[out] error       Perform verbose error reporting if not NULL
 *
 * @return @c 0 on success; jumps out in case of failure
 */
extern int rpc_rte_flow_flush(rcf_rpc_server *rpcs,
                              uint16_t port_id,
                              tarpc_rte_flow_error *error);

/**
 * rte_flow_isolate() RPC
 * @param[in]  port_id Port number
 * @param[in]  set     Non-zero to enter isolated mode, @c 0 to leave it
 * @param[out] error   Perform verbose error reporting if not @c NULL
 *
 * @return @c 0 on success; jumps out in case of failure
 */
extern int rpc_rte_flow_isolate(rcf_rpc_server              *rpcs,
                                uint16_t                     port_id,
                                int                          set,
                                struct tarpc_rte_flow_error *error);

/**@} <!-- END te_lib_rpc_rte_flow --> */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_RPC_RTE_FLOW_H__ */
