/** @file
 * @brief Tester subsystem internal definitions
 *
 * Definition of Tester subsystem internal interfaces.
 *
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights served.
 *
 * 
 *
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TE_TESTER_INTERNAL_H__
#define __TE_TESTER_INTERNAL_H__

/** Name of the environment variable with Tester IPC server name */
#define TESTER_IPC_SERVER_ENV   "TE_TESTER"

/** Type of IPC used by Tester IPC */
#define TESTER_IPC              (TRUE)  /** Connection-oriented */


/**
 * Representation of the verdict header passed from tests to Tester.
 * Verdict message string is passed just after the header.
 */
typedef struct tester_test_verdict_hdr {
        uint32_t    id;     /**< ID of the test */
} tester_test_verdict_hdr;

#endif /* !__TE_TESTER_INTERNAL_H__ */
