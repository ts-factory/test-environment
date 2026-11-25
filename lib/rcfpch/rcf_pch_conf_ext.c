/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief RCF Portable Command Handler
 *
 * Registry of configuration tree extensions provided by external
 * libraries linked into a Test Agent.
 */

#define TE_LGR_USER     "RCF PCH CONF EXT"

#include "te_config.h"

#include <assert.h>

#include "te_errno.h"
#include "te_queue.h"
#include "logger_api.h"

#include "rcf_pch_conf_ext.h"

/*
 * Constructor functions register before main(), from one thread,
 * so the list needs no lock.
 */
static TAILQ_HEAD(, rcf_pch_conf_ext) conf_exts =
    TAILQ_HEAD_INITIALIZER(conf_exts);

/* See description in rcf_pch_conf_ext.h */
void
rcf_pch_conf_ext_register(rcf_pch_conf_ext *ext)
{
    assert(ext != NULL);
    assert(ext->name != NULL);
    assert(ext->init != NULL);

    TAILQ_INSERT_TAIL(&conf_exts, ext, links);
}

/* See description in rcf_pch_conf_ext.h */
te_errno
rcf_pch_conf_ext_init_all(void)
{
    rcf_pch_conf_ext *ext;
    unsigned int n = 0;

    TAILQ_FOREACH(ext, &conf_exts, links)
        n++;

    /*
     * The linker drops a library built without 'link_whole = true'
     * together with its constructor, and the agent then looks as
     * if the library was not linked at all, so log the count.
     */
    RING("%u configuration extension(s) registered", n);

    TAILQ_FOREACH(ext, &conf_exts, links)
    {
        te_errno rc;

        RING("Initializing configuration extension '%s'", ext->name);
        rc = ext->init();
        if (rc != 0)
        {
            ERROR("Configuration extension '%s' failed to initialize: %r",
                  ext->name, rc);
            return rc;
        }
    }

    return 0;
}
