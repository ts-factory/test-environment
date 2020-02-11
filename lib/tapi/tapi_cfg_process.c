/** @file
 * @brief Test API to configure processes.
 *
 * @defgroup tapi_conf_process Processes configuration
 * @ingroup tapi_conf
 * @{
 *
 * Definition of TAPI to configure processes.
 *
 * Copyright (C) 2020 OKTET Labs. All rights reserved.
 *
 * @author Dilshod Urazov <Dilshod.Urazov@oktetlabs.ru>
 */

#include "te_config.h"

#include "te_defs.h"
#include "te_errno.h"
#include "logger_ten.h"
#include "conf_api.h"

#include "tapi_cfg_process.h"


#define TE_CFG_TA_PS    "/agent:%s/process:%s"


/* See descriptions in tapi_cfg_process.h */
te_errno
tapi_cfg_ps_add(const char *ta, const char *ps_name,
                  const char *exe, te_bool start)
{
    te_errno rc;

    rc = cfg_add_instance_fmt(NULL, CVT_NONE, NULL, TE_CFG_TA_PS, ta, ps_name);
    if (rc != 0)
    {
        ERROR("Cannot add process '%s' to TA '%s': %r", ps_name, ta, rc);
        return rc;
    }

    rc  = cfg_set_instance_fmt(CFG_VAL(STRING, exe),
                               TE_CFG_TA_PS "/exe:", ta, ps_name);
    if (rc != 0)
    {
        ERROR("Cannot set exe '%s' in process '%s': %r", exe, ps_name, rc);
        return tapi_cfg_ps_del(ta, ps_name);
    }

    return start ? tapi_cfg_ps_start(ta, ps_name) : 0;
}

/* See descriptions in tapi_cfg_process.h */
te_errno
tapi_cfg_ps_del(const char *ta, const char *ps_name)
{
    te_errno rc;

    rc = cfg_del_instance_fmt(FALSE, TE_CFG_TA_PS, ta, ps_name);
    if (rc != 0)
        ERROR("Cannot delete process '%s' from TA '%s': %r", ps_name, ta, rc);

    return rc;
}

/* See descriptions in tapi_cfg_process.h */
te_errno
tapi_cfg_ps_start(const char *ta, const char *ps_name)
{
    te_errno rc;

    rc  = cfg_set_instance_fmt(CFG_VAL(INTEGER, 1),
                               TE_CFG_TA_PS "/status:", ta, ps_name);
    if (rc != 0)
        ERROR("Cannot start process '%s' on TA '%s': %r", ps_name, ta, rc);

    return rc;
}

/* See descriptions in tapi_cfg_process.h */
te_errno
tapi_cfg_ps_stop(const char *ta, const char *ps_name)
{
    te_errno rc;

    rc  = cfg_set_instance_fmt(CFG_VAL(INTEGER, 0),
                               TE_CFG_TA_PS "/status:", ta, ps_name);
    if (rc != 0)
        ERROR("Cannot stop process '%s' on TA '%s': %r", ps_name, ta, rc);

    return rc;
}
