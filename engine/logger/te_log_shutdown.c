/** @file
 * @brief TE project. Logger subsystem.
 *
 * Separate Logger task for shutdowning main Logger process.
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights served.
 *
 * 
 *
 *
 * @author Igor B. Vasiliev <Igor.Vasiliev@oktetlabs.ru>
 *
 * $Id$
 */

#include "te_config.h"

#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "te_stdint.h"
#include "te_raw_log.h"
#include "logger_int.h"
#include "ipc_client.h"
#include "logger_internal.h"
#include "logger_ten.h"

/**
 * This is an entry point of te_log_shutdown task.
 */
int
main(void)
{
    int                 result = EXIT_SUCCESS;
    te_log_nfl          nfl = strlen(LGR_SHUTDOWN);
    te_log_nfl          nfl_net = htons(nfl);
    uint8_t             mess[sizeof(nfl) + nfl];
    struct ipc_client  *log_client = NULL;
    te_errno            rc;


    /* Prepare message with entity name LGR_SHUTDOWN */
    memcpy(mess, &nfl_net, sizeof(nfl_net));
    memcpy(mess + sizeof(nfl_net), LGR_SHUTDOWN, nfl);

    rc = ipc_init_client("LOGGER_SHUTDOWN_CLIENT", LOGGER_IPC, &log_client);
    if (rc != 0)
    {
        fprintf(stderr, "ipc_init_client() failed: 0x%X\n", rc);
        return EXIT_FAILURE;
    }
    assert(log_client != NULL);
    if (ipc_send_message(log_client, LGR_SRV_NAME, mess, sizeof(mess)) != 0)
    {
        fprintf(stderr, "ipc_send_message() failed\n");
        result = EXIT_FAILURE;
    }
    if (ipc_close_client(log_client) != 0)
    {
        fprintf(stderr, "ipc_close_client() failed\n");
        result = EXIT_FAILURE;
    }

    return result;
}
