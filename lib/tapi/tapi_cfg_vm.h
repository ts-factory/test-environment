/** @file
 * @brief Test API to configure virtual machines.
 *
 * @defgroup tapi_conf_vm Virtual machines configuration
 * @ingroup tapi_conf
 * @{
 *
 * Definition of TAPI to configure virtual machines.
 *
 * Copyright (C) 2019 OKTET Labs. All rights reserved.
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 */

#ifndef __TE_TAPI_CFG_VM_H__
#define __TE_TAPI_CFG_VM_H__

#include "te_defs.h"
#include "te_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add virtual machine.
 *
 * @param ta            Test Agent.
 * @param vm_name       Virtual machine name.
 * @param tmpl          @c NULL or virtual machine configuration template.
 * @param start         Start it just after addition and template apply
 *
 * @return Status code
 */
extern te_errno tapi_cfg_vm_add(const char *ta, const char *vm_name,
                                const char *tmpl, te_bool start);

/**
 * Delete virtual machine.
 *
 * @param ta            Test Agent.
 * @param vm_name       Virtual machine name.
 *
 * @return Status code
 */
extern te_errno tapi_cfg_vm_del(const char *ta, const char *vm_name);

/**
 * Start virtual machine.
 *
 * @param ta            Test Agent.
 * @param vm_name       Virtual machine name.
 *
 * @return Status code
 */
extern te_errno tapi_cfg_vm_start(const char *ta, const char *vm_name);

/**
 * Stop virtual machine.
 *
 * @param ta            Test Agent.
 * @param vm_name       Virtual machine name.
 *
 * @return Status code
 */
extern te_errno tapi_cfg_vm_stop(const char *ta, const char *vm_name);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_CFG_VM_H__ */

/**@} <!-- END tapi_conf_vm --> */
