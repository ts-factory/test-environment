/** @file
 * @brief RPC for DPDK EAL
 *
 * RPC routines implementation to call DPDK (rte_eal_*) functions.
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights reserved.
 *
 * 
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 */

#define TE_LGR_USER     "RPC DPDK EAL"

#include "te_config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "rte_config.h"
#include "rte_version.h"
#include "rte_eal.h"

#include "logger_api.h"

#include "rpc_server.h"
#include "rpcs_dpdk.h"


TARPC_FUNC(rte_eal_init, {},
{
    char **argv;
    int i;

    if (in->argc > 0)
    {
        argv = calloc(in->argc, sizeof(*argv));
        if (argv == NULL)
        {
            out->common._errno = TE_RC(TE_RPCS, TE_ENOMEM);
            out->retval = -out->common._errno;
            goto done;
        }
        for (i = 0; i < in->argc; ++i)
            argv[i] = in->argv.argv_val[i].str;
    }
    else
    {
        argv = NULL;
    }


    MAKE_CALL(out->retval = func(in->argc, argv););
    neg_errno_h2rpc(&out->retval);

    free(argv);

done:
    ;
})

TARPC_FUNC(rte_eal_process_type, {},
{
    enum rte_proc_type_t retval;

    MAKE_CALL(retval = func());

    switch (retval)
    {
        case RTE_PROC_AUTO:
            out->retval = TARPC_RTE_PROC_AUTO;
            break;

        case RTE_PROC_PRIMARY:
            out->retval = TARPC_RTE_PROC_PRIMARY;
            break;

        case RTE_PROC_SECONDARY:
            out->retval = TARPC_RTE_PROC_SECONDARY;
            break;

        case RTE_PROC_INVALID:
            out->retval = TARPC_RTE_PROC_INVALID;
            break;

        default:
            out->retval = TARPC_RTE_PROC__UNKNOWN;
            break;
    }
})

TARPC_FUNC_STANDALONE(dpdk_get_version, {},
{
    out->year = RTE_VER_YEAR;
    out->month = RTE_VER_MONTH;
    out->minor = RTE_VER_MINOR;
    out->release = RTE_VER_RELEASE;
})

TARPC_FUNC(rte_eal_hotplug_add, {},
{
    MAKE_CALL(out->retval = func(in->busname, in->devname, in->devargs));
    neg_errno_h2rpc(&out->retval);
})
