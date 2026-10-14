/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief RCF Portable Command Handler
 *
 * Registry of configuration tree extensions provided by external
 * libraries linked into a Test Agent.
 *
 * An external library registers its configuration subtree
 * initializer from a constructor function, without any change to
 * the Test Agent sources:
 *
 * @code
 * static te_errno
 * my_conf_init(void)
 * {
 *     return rcf_pch_add_node("/agent", &node_my_subtree);
 * }
 *
 * TE_RCF_PCH_CONF_EXT(my_conf_init);
 * @endcode
 *
 * The library must set 'link_whole = true' in its meson.build,
 * otherwise the linker may drop the object file with the
 * constructor.
 *
 * TE_RCF_PCH_CONF_EXT() is defined only when the compiler supports
 * constructor functions (TE_CONSTRUCTOR_AVAILABLE). Without them
 * the library has to call rcf_pch_conf_ext_register() itself before
 * the agent initializes its configuration tree.
 *
 * @note The initializers run in the order of the constructors, and
 *       that order depends on the linker and on the way the
 *       libraries are linked. Extensions must not depend on each
 *       other or on running first or last. The agent sets up the
 *       built-in subtrees before it initializes any extension.
 */

#ifndef __TE_RCF_PCH_CONF_EXT_H__
#define __TE_RCF_PCH_CONF_EXT_H__

#include "te_compiler.h"
#include "te_errno.h"
#include "te_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Type of a configuration tree extension initializer. */
typedef te_errno (*rcf_pch_conf_ext_init_fn)(void);

/**
 * Registration entry of a configuration tree extension.
 *
 * TE_RCF_PCH_CONF_EXT() defines it as a static object that lives
 * as long as the program, so registration allocates nothing and
 * cannot fail. Do not fill it in or read it directly.
 */
typedef struct rcf_pch_conf_ext {
    /** Links in the registry of extensions. */
    TAILQ_ENTRY(rcf_pch_conf_ext) links;
    /** Extension name, used in logs. */
    const char *name;
    /** Initializer to call. */
    rcf_pch_conf_ext_init_fn init;
} rcf_pch_conf_ext;

/**
 * Register a configuration tree extension.
 *
 * A constructor function calls it before main(), see
 * TE_RCF_PCH_CONF_EXT(). The agent calls the initializer from its
 * configuration initialization, after the built-in subtrees are
 * set up.
 *
 * @param ext   Registration entry with static storage; the registry
 *              links it in as is, so a local or a freed object is
 *              not allowed.
 */
extern void rcf_pch_conf_ext_register(rcf_pch_conf_ext *ext);

/**
 * Call all registered extension initializers.
 *
 * The Test Agent calls it at the end of its configuration
 * initialization. Extensions must not call it.
 *
 * @return Status code (first failed initializer's one).
 */
extern te_errno rcf_pch_conf_ext_init_all(void);

#if TE_CONSTRUCTOR_AVAILABLE
/**
 * Register a function as a configuration tree extension initializer
 * at program startup.
 *
 * @param fn_   Initializer of the type rcf_pch_conf_ext_init_fn:
 *              a function of no arguments returning te_errno. The
 *              macro names the registration entry after it, so it
 *              must be an identifier, not an expression.
 */
#define TE_RCF_PCH_CONF_EXT(fn_)                                    \
    static rcf_pch_conf_ext te_rcf_pch_conf_ext_##fn_ = {           \
        .name = #fn_,                                               \
        .init = fn_,                                                \
    };                                                              \
                                                                    \
    static TE_CONSTRUCTOR void                                      \
    te_rcf_pch_conf_ext_ctor_##fn_(void)                            \
    {                                                               \
        rcf_pch_conf_ext_register(&te_rcf_pch_conf_ext_##fn_);      \
    }
#endif /* TE_CONSTRUCTOR_AVAILABLE */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !__TE_RCF_PCH_CONF_EXT_H__ */
