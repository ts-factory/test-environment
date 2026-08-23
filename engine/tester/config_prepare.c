/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Tester Subsystem
 *
 * Prepare configuration to be run.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.
 */

/** Logging user name to be used here */
#define TE_LGR_USER "Config Prepare"

#include "te_config.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "te_alloc.h"
#include "te_defs.h"
#include "te_errno.h"
#include "te_queue.h"
#include "te_string.h"
#include "te_str.h"
#include "te_kvpair.h"
#include "te_compound.h"
#include "te_expand.h"
#include "logger_api.h"

#include "tester_conf.h"
#include "tester_flags.h"
#include "type_lib.h"


/**
 * Maximum number of iterations. INT_MAX is used because of
 * Bublik implementation.
 */
#define TESTER_MAX_ITERS INT_MAX

/** Tester context */
typedef struct config_prepare_ctx {
    SLIST_ENTRY(config_prepare_ctx) links;  /**< List links */

    unsigned int        total_iters;

    unsigned int        inherit_flags;  /**< Inheritance flags */
/** Inherit exception handler for all descendant sessions */
#define PREPARE_INHERIT_EXCEPTION_ALL   (1 << 0)
/** Inherit keep-alive handler for all descendant sessions */
#define PREPARE_INHERIT_KEEPALIVE_ALL   (1 << 1)
/** Inherit 'track_conf' attribute */
#define PREPARE_INHERIT_TRACK_CONF      (1 << 2)
/** Inherit 'track_conf' attribute for all descendants */
#define PREPARE_INHERIT_TRACK_CONF_ALL  (1 << 3)

    run_item           *exception;  /**< Current exception handler */
    run_item           *keepalive;  /**< Current keep-alive handler */
    unsigned int        track_conf; /**< Current track_conf attribute */

    const char         *pkg_path;   /**< Package file the current session
                                         comes from */

} config_prepare_ctx;

/**
 * Opaque data for all configuration traverse callbacks.
 */
typedef struct config_prepare_data {

    SLIST_HEAD(, config_prepare_ctx) ctxs;   /**< Stack of contexts */

    te_errno                        rc;     /**< Status code */

    bool                            expand; /**< Expansion is enabled for
                                                 the configuration file
                                                 being prepared */
    const char                     *new_pkg_path; /**< Path of the package
                                                       file whose session is
                                                       about to be entered */

} config_prepare_data;

/**
 * Print error about too many iterations in test suite.
 */
static void
too_many_iters_log(void)
{
    ERROR("Test suite can have no more than %u test iterations",
          TESTER_MAX_ITERS);
}

/**
 * Check whether sum may result in overflowing iteration
 * counter.
 *
 * @param x: The first operand.
 * @param y: The second operand.
 *
 * @return True if there is an overflow, false otherwise.
 */
static bool
check_sum_overflow(unsigned int x, unsigned int y)
{
    if (x > TESTER_MAX_ITERS || TESTER_MAX_ITERS - x < y)
    {
        too_many_iters_log();
        return true;
    }

    return false;
}

/**
 * Check whether multiplication may result in overflowing iteration
 * counter.
 *
 * @param x: The first operand.
 * @param y: The second operand.
 *
 * @return True if there is an overflow, false otherwise.
 */
static bool
check_mul_overflow(unsigned int x, unsigned int y)
{
    if (x != 0 && TESTER_MAX_ITERS / x < y)
    {
        too_many_iters_log();
        return true;
    }

    return false;
}

/**
 * Clone the most recent (current) Tester context.
 *
 * @param gctx          Configuration prepare global context
 *
 * @return Allocated context.
 */
static config_prepare_ctx *
config_prepare_new_ctx(config_prepare_data *gctx)
{
    config_prepare_ctx *cur_ctx = SLIST_FIRST(&gctx->ctxs);
    config_prepare_ctx *new_ctx;

    new_ctx = TE_ALLOC(sizeof(*new_ctx));

    if (cur_ctx != NULL)
    {
        new_ctx->inherit_flags = cur_ctx->inherit_flags;
        new_ctx->exception = cur_ctx->exception;
        new_ctx->keepalive = cur_ctx->keepalive;
        new_ctx->track_conf = cur_ctx->track_conf;
        new_ctx->pkg_path = cur_ctx->pkg_path;
    }
    else
    {
        /* new_ctx->inherit_flags = 0; */
        /* new_ctx->exception = NULL; */
        /* new_ctx->keepalive = NULL; */
        new_ctx->track_conf = TESTER_TRACK_CONF_UNSPEC;
    }

    SLIST_INSERT_HEAD(&gctx->ctxs, new_ctx, links);

    return new_ctx;
}

/**
 * Destroy the most recent (current) context.
 *
 * @param gctx          Configuration prepare global context
 *
 * @return Status code.
 */
static te_errno
config_prepare_destroy_ctx(config_prepare_data *gctx)
{
    config_prepare_ctx *curr = SLIST_FIRST(&gctx->ctxs);
    config_prepare_ctx *prev;
    te_errno result = 0;

    assert(curr != NULL);
    prev = SLIST_NEXT(curr, links);
    if (prev != NULL)
    {
        if (check_sum_overflow(prev->total_iters, curr->total_iters))
            gctx->rc = result = TE_RC(TE_TESTER, TE_EOVERFLOW);
        else
            prev->total_iters += curr->total_iters;
    }

    SLIST_REMOVE(&gctx->ctxs, curr, config_prepare_ctx, links);
    free(curr);

    return result;
}


/**
 * Inherit executable and update inheritance settings for the next
 * descendant.
 *
 * @param child_exec    Location of the child executable
 * @param child_flags   Child flags to mark that executable is inherited
 * @param inherit_done  Flag to set in child flags when executable is
 *                      inherited
 * @param inherit_exec  Location of the executable to be inherited
 * @param inherit_flags Location of inheritance flags
 * @param inherit_do    Flag which requests executable inheritance
 */
static void
inherit_executable(run_item **child_exec, unsigned int *child_flags,
                   unsigned int inherit_done,
                   run_item **inherit_exec, unsigned int *inherit_flags,
                   unsigned int inherit_do)
{
    assert(child_exec != NULL);
    assert(child_flags != NULL);
    assert(inherit_exec != NULL);
    assert(inherit_flags != NULL);

    if (*child_exec != NULL)
    {
        /*
         * If the current session (referred to as "child" here) already has
         * a given executable set, we should update executable and its
         * inheritance flags in the current processing context. It will
         * allow descendant sessions to inherit it later from descendant
         * contexts (see also config_prepare_new_ctx()), if handdown
         * property of executable says that it should be inherited.
         */

        *inherit_flags &= ~inherit_do;
        *inherit_exec = NULL;
        switch ((*child_exec)->handdown)
        {
            case TESTER_HANDDOWN_NONE:
                break;

            case TESTER_HANDDOWN_DESCENDANTS:
                *inherit_flags |= inherit_do;
                /* FALLTHROUGH */

            case TESTER_HANDDOWN_CHILDREN:
                *inherit_exec = *child_exec;
                break;

            default:
                assert(false);
        }
    }
    else
    {
        /*
         * If the current session does not have a given executable set,
         * we should set it to a value stored in the current processing
         * context (which may have got it via the previous contexts from
         * some higher level session, see comments above).
         */

        /* Set flag even in the case of NULL executable - it is harmless */
        *child_flags |= inherit_done;
        *child_exec = *inherit_exec;

        /*
         * If context flags do not say that all descendants (not just
         * direct children) should inherit this executable, set it to
         * NULL in the current processing context, so that nothing will
         * be inherited from it by descendant contexts and therefore by
         * descendant sessions.
         */
        if (~(*inherit_flags) & inherit_do)
            *inherit_exec = NULL;
    }
}


/**
 * Function to be called for each singleton value of the run item
 * argument (explicit or inherited) to calculate total number of values.
 *
 * The function complies with test_entity_value_enum_cb prototype.
 */
static te_errno
prepare_arg_value_cb(const test_entity_value *value, void *opaque)
{
    unsigned int *num = opaque;

    UNUSED(value);

    *num += 1;

    return 0;
}

/** Data to be passed as opaque to prepare_arg_cb() function. */
typedef struct prepare_arg_cb_data {
    run_item       *ri;         /**< Run item context */
    unsigned int    n_args;     /**< Number of arguments */
    unsigned int    n_iters;    /**< Total number of iterations without
                                     lists */
} prepare_arg_cb_data;

/**
 * Function to be called for each argument (explicit or inherited) to
 * calculate total number of iterations.
 *
 * The function complies with test_var_arg_enum_cb prototype.
 */
static te_errno
prepare_arg_cb(const test_var_arg *va, void *opaque)
{
    prepare_arg_cb_data    *data = opaque;
    unsigned int            n_values = 0;
    te_errno                rc;

    data->n_args++;

    rc = test_var_arg_enum_values(data->ri, va, prepare_arg_value_cb,
                                  &n_values, NULL, NULL);
    if (rc != 0)
    {
        ERROR("Enumeration of values of argument '%s' of the run item "
              "'%s' failed: %r", va->name, run_item_name(data->ri), rc);
        return rc;
    }

    if (va->list == NULL)
    {
        if (check_mul_overflow(data->n_iters, n_values))
        {
            ERROR("Enumeration of values of argument '%s' of the run item "
                  "'%s' failed: too many iterations", va->name,
                  run_item_name(data->ri));
            return TE_RC(TE_TESTER, TE_EOVERFLOW);
        }

        data->n_iters *= n_values;
        VERB("%s(): arg=%s: n_values=%u -> n_iters =%u",
             __FUNCTION__, va->name, n_values, data->n_iters);
    }
    else
    {
        test_var_arg_list  *p;

        for (p = SLIST_FIRST(&data->ri->lists);
             p != NULL && strcmp(p->name, va->list) != 0;
             p = SLIST_NEXT(p, links));

        if (p != NULL)
        {
            VERB("%s(): arg=%s: found list=%s len=%u n_values=%u",
                 __FUNCTION__, va->name, p->name, p->len, n_values);
            assert(data->n_iters % p->len == 0);
            data->n_iters /= p->len;
            p->len = MAX(p->len, n_values);

            if (check_mul_overflow(data->n_iters, p->len))
            {
                ERROR("Enumeration of values of argument '%s' of the run "
                      "item '%s' failed: too many iterations", va->name,
                      run_item_name(data->ri));
                return TE_RC(TE_TESTER, TE_EOVERFLOW);
            }

            data->n_iters *= p->len;
        }
        else
        {
            p = TE_ALLOC(sizeof(*p));

            p->name = va->list;
            p->len = n_values;
            p->n_iters = data->n_iters;
            SLIST_INSERT_HEAD(&data->ri->lists, p, links);

            if (check_mul_overflow(data->n_iters, n_values))
            {
                ERROR("Enumeration of values of argument '%s' of the run "
                      "item '%s' failed: too many iterations", va->name,
                      run_item_name(data->ri));
                return TE_RC(TE_TESTER, TE_EOVERFLOW);
            }

            data->n_iters *= n_values;

            VERB("%s(): arg=%s: new list=%s len=%u -> n_iters=%u",
                 __FUNCTION__, va->name, p->name, p->len, data->n_iters);
        }
    }

    return 0;
}

/**
 * Calculate number of iterations for specified run item.
 *
 * @param ri            Run item
 *
 * @return Number of iterations.
 */
static te_errno
prepare_calc_iters(run_item *ri)
{
    prepare_arg_cb_data     data;
    te_errno                rc;

    data.ri = ri;
    data.n_args = 0;
    data.n_iters = 1;

    rc = test_run_item_enum_args(ri, prepare_arg_cb, true, &data);
    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
        return rc;

    ri->n_args = data.n_args;
    ri->n_iters = data.n_iters;

    return 0;
}


static tester_cfg_walk_ctl
prepare_cfg_start(tester_cfg *cfg, unsigned int cfg_id_off, void *opaque)
{
    config_prepare_data    *gctx = opaque;
    config_prepare_ctx     *ctx;

    UNUSED(cfg_id_off);

    assert(gctx != NULL);
    ctx = SLIST_FIRST(&gctx->ctxs);
    assert(ctx != NULL);

    gctx->expand = (cfg->syntax_flags & TESTER_EXPAND_VARS) != 0;

    if (config_prepare_new_ctx(gctx) == NULL)
        return TESTER_CFG_WALK_FAULT;

    return TESTER_CFG_WALK_CONT;
}

static tester_cfg_walk_ctl
prepare_cfg_end(tester_cfg *cfg, unsigned int cfg_id_off, void *opaque)
{
    config_prepare_data    *gctx = opaque;
    config_prepare_ctx     *ctx;

    UNUSED(cfg_id_off);

    assert(gctx != NULL);
    ctx = SLIST_FIRST(&gctx->ctxs);
    assert(ctx != NULL);

    cfg->total_iters = ctx->total_iters;

    if (config_prepare_destroy_ctx(gctx) != 0)
        return TESTER_CFG_WALK_FAULT;

    return TESTER_CFG_WALK_CONT;
}

static tester_cfg_walk_ctl
prepare_pkg_start(run_item *ri, test_package *pkg, unsigned int cfg_id_off,
                  void *opaque)
{
    config_prepare_data *gctx = opaque;

    UNUSED(ri);
    UNUSED(cfg_id_off);

    assert(gctx != NULL);
    gctx->new_pkg_path = pkg->path;

    return TESTER_CFG_WALK_CONT;
}

static tester_cfg_walk_ctl
prepare_session_start(run_item *ri, test_session *session,
                      unsigned int cfg_id_off, void *opaque)
{
    config_prepare_data    *gctx = opaque;
    config_prepare_ctx     *ctx;

    UNUSED(ri);
    UNUSED(cfg_id_off);

    assert(gctx != NULL);

    if ((ctx =  config_prepare_new_ctx(gctx)) == NULL)
        return TESTER_CFG_WALK_FAULT;

    /*
     * pkg_start() is called just before the session of the package is
     * entered, so the announced path belongs to this very session.
     */
    if (gctx->new_pkg_path != NULL)
    {
        ctx->pkg_path = gctx->new_pkg_path;
        gctx->new_pkg_path = NULL;
    }

    /* Service executables inheritance */
    inherit_executable(&session->exception, &session->flags,
                       TEST_INHERITED_EXCEPTION, &ctx->exception,
                       &ctx->inherit_flags, PREPARE_INHERIT_EXCEPTION_ALL);
    inherit_executable(&session->keepalive, &session->flags,
                       TEST_INHERITED_KEEPALIVE, &ctx->keepalive,
                       &ctx->inherit_flags, PREPARE_INHERIT_KEEPALIVE_ALL);

    /* 'track_conf' attribute inheritance */

    if (session->attrs.track_conf != TESTER_TRACK_CONF_UNSPEC)
    {
        /*
         * If track_conf attribute was specified for the current
         * session, reset inheritance settings.
         */
        ctx->track_conf = session->attrs.track_conf;
        if (session->attrs.track_conf_hd != TESTER_HANDDOWN_NONE)
        {
            ctx->inherit_flags |= PREPARE_INHERIT_TRACK_CONF;
            if (session->attrs.track_conf_hd == TESTER_HANDDOWN_DESCENDANTS)
                ctx->inherit_flags |= PREPARE_INHERIT_TRACK_CONF_ALL;
            else
                ctx->inherit_flags &= ~PREPARE_INHERIT_TRACK_CONF_ALL;
        }
        else
        {
            ctx->inherit_flags &= ~PREPARE_INHERIT_TRACK_CONF;
            ctx->inherit_flags &= ~PREPARE_INHERIT_TRACK_CONF_ALL;
        }
    }
    else
    {
        /*
         * Inherit track_conf attribute from parent if it is not specified
         * in the current session and if inheritance settings allow.
         */

        if (ctx->inherit_flags & PREPARE_INHERIT_TRACK_CONF)
            session->attrs.track_conf = ctx->track_conf;
        else
            session->attrs.track_conf = TESTER_TRACK_CONF_DEF;

        /*
         * If track_conf_handdown was not set to "descendants" when
         * setting currently inherited track_conf value, disable
         * passing inherited value to children.
         */
        if (~ctx->inherit_flags & PREPARE_INHERIT_TRACK_CONF_ALL)
            ctx->inherit_flags &= ~PREPARE_INHERIT_TRACK_CONF;
    }

    return TESTER_CFG_WALK_CONT;
}

static tester_cfg_walk_ctl
prepare_session_end(run_item *ri, test_session *session,
                    unsigned int cfg_id_off, void *opaque)
{
    config_prepare_data    *gctx = opaque;
    config_prepare_ctx     *ctx;

    UNUSED(session);
    UNUSED(cfg_id_off);

    assert(gctx != NULL);
    ctx = SLIST_FIRST(&gctx->ctxs);
    assert(ctx != NULL);

    ri->weight = ctx->total_iters;
    ctx->total_iters = 0;

    if (config_prepare_destroy_ctx(gctx) != 0)
        return TESTER_CFG_WALK_FAULT;

    return TESTER_CFG_WALK_CONT;
}

/** Data to be passed as opaque to expand_check_* callbacks. */
typedef struct expand_check_data {
    const run_item     *ri;         /**< Run item being checked */
    const test_var_arg *va;         /**< Argument being enumerated */
    te_kvpair_h        *kvpairs;    /**< Names available for expansion */
    const char         *pkg_path;   /**< Package file for diagnostics */
    te_errno            rc;         /**< Status of the check */
} expand_check_data;

/**
 * Bind one value of a compound argument in the validation context.
 *
 * The function complies with te_compound_iter_fn prototype.
 */
static te_errno
expand_check_field_cb(char *key, size_t idx, char *value, bool has_more,
                      void *user)
{
    expand_check_data *data = user;
    te_string name = TE_STRING_INIT;

    UNUSED(value);
    UNUSED(has_more);

    te_compound_build_name(&name, data->va->name, key, idx);
    te_kvpair_push(data->kvpairs, te_string_value(&name), "%s", "");
    te_string_free(&name);

    return 0;
}

/**
 * Bind the names provided by a single declared value.
 *
 * The function complies with test_entity_value_enum_cb prototype.
 */
static te_errno
expand_check_value_cb(const test_entity_value *value, void *opaque)
{
    expand_check_data *data = opaque;

    if (value->plain != NULL)
        te_compound_iterate_str(value->plain, expand_check_field_cb, data);

    return 0;
}

/**
 * Bind the name of an argument and of every field of its values.
 *
 * The function complies with test_var_arg_enum_cb prototype.
 */
static te_errno
expand_check_arg_cb(const test_var_arg *va, void *opaque)
{
    expand_check_data *data = opaque;

    data->va = va;
    te_kvpair_push(data->kvpairs, va->name, "%s", "");
    (void)test_var_arg_enum_values(data->ri, va, expand_check_value_cb,
                                   data, NULL, NULL);
    data->va = NULL;

    return 0;
}

/**
 * Check that every variable reference in @p src can be resolved.
 *
 * Only an unknown name is fatal.  Every known name is bound to an
 * empty placeholder here, so a filter may fail on a value it would
 * never see at run time; such a failure says nothing about the
 * reference itself and must not stop the run.
 *
 * @param data  Validation context.
 * @param what  Human readable name of the string being checked.
 * @param src   String to check (may be @c NULL).
 */
static void
expand_check_string(expand_check_data *data, const char *what,
                    const char *src)
{
    te_string out = TE_STRING_INIT;
    te_errno rc;

    if (src == NULL || strstr(src, "${") == NULL)
        return;

    rc = te_string_expand_kvpairs_strict(src, NULL, data->kvpairs, &out);
    if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
    {
        ERROR("Cannot expand %s of the run item '%s' in '%s': %r; "
              "the text is '%s'", what, run_item_name(data->ri),
              te_str_empty_if_null(data->pkg_path), rc, src);
        data->rc = TE_RC(TE_TESTER, TE_ENOENT);
    }
    else if (rc != 0)
    {
        INFO("Cannot check %s of the run item '%s' in '%s': %r; "
             "the text is '%s'", what, run_item_name(data->ri),
             te_str_empty_if_null(data->pkg_path), rc, src);
    }
    te_string_free(&out);
}

/**
 * Check the objectives of every declared value of an argument.
 *
 * The function complies with test_entity_value_enum_cb prototype.
 */
static te_errno
expand_check_value_objective_cb(const test_entity_value *value, void *opaque)
{
    expand_check_data *data = opaque;

    expand_check_string(data, "a value objective", value->objective);

    return 0;
}

/**
 * Check the objectives of every argument of the run item.
 *
 * The function complies with test_var_arg_enum_cb prototype.
 */
static te_errno
expand_check_arg_objectives_cb(const test_var_arg *va, void *opaque)
{
    expand_check_data *data = opaque;

    data->va = va;
    (void)test_var_arg_enum_values(data->ri, va,
                                   expand_check_value_objective_cb,
                                   data, NULL, NULL);
    data->va = NULL;

    return 0;
}

/**
 * Check every string of a run item that is subject to expansion.
 *
 * @param ri        Run item.
 * @param pkg_path  Package file the run item comes from.
 *
 * @return Status code.
 */
static te_errno
expand_check_run_item(const run_item *ri, const char *pkg_path)
{
    te_kvpair_h kvpairs;
    expand_check_data data = {
        .ri = ri,
        .va = NULL,
        .kvpairs = &kvpairs,
        .pkg_path = pkg_path,
        .rc = 0,
    };

    te_kvpair_init(&kvpairs);
    (void)test_run_item_enum_args(ri, expand_check_arg_cb, false, &data);

    expand_check_string(&data, "the objective", ri->objective);
    expand_check_string(&data, "the page reference", ri->page);

    switch (ri->type)
    {
        case RUN_ITEM_SCRIPT:
            expand_check_string(&data, "the objective",
                                ri->u.script.objective);
            expand_check_string(&data, "the page reference",
                                ri->u.script.page);
            break;

        case RUN_ITEM_SESSION:
            expand_check_string(&data, "the objective",
                                ri->u.session.objective);
            break;

        case RUN_ITEM_PACKAGE:
            /*
             * Unlike the strings above, the description of a package is
             * written in the package file itself, not in the file the
             * run item comes from.
             */
            data.pkg_path = ri->u.package->path;
            expand_check_string(&data, "the objective",
                                ri->u.package->objective);
            data.pkg_path = pkg_path;
            break;

        default:
            break;
    }

    (void)test_run_item_enum_args(ri, expand_check_arg_objectives_cb,
                                  false, &data);

    te_kvpair_fini(&kvpairs);

    return data.rc;
}

static tester_cfg_walk_ctl
prepare_test_start(run_item *ri, unsigned int cfg_id_off,
                   unsigned int flags, void *opaque)
{
    config_prepare_data    *gctx = opaque;
    config_prepare_ctx     *ctx;
    test_attrs             *attrs = test_get_attrs(ri);

    UNUSED(cfg_id_off);
    UNUSED(flags);

    assert(gctx != NULL);
    ctx = SLIST_FIRST(&gctx->ctxs);
    assert(ctx != NULL);

    /*
     * 'track_conf' attribute inheritance.
     * Note: this handler is actually run_start(), so it is called for
     * every <run>, including <run> enclosing <session>. However <run>
     * does not have its own attributes - attrs will point to those
     * of <session> in such case. As session attributes inheritance is
     * handled in prepare_session_start(), here we should only process
     * scripts (for a package attrs would point to main <session>).
     */

    if (ri->type == RUN_ITEM_SCRIPT &&
        attrs->track_conf == TESTER_TRACK_CONF_UNSPEC)
    {
        if (ctx->inherit_flags & PREPARE_INHERIT_TRACK_CONF)
            attrs->track_conf = ctx->track_conf;
        else
            attrs->track_conf = TESTER_TRACK_CONF_DEF;
    }

    if (gctx->expand)
    {
        gctx->rc = expand_check_run_item(ri, ctx->pkg_path);
        if (gctx->rc != 0)
            return TESTER_CFG_WALK_FAULT;
    }

    gctx->rc = prepare_calc_iters(ri);
    if (gctx->rc != 0)
        return TESTER_CFG_WALK_FAULT;

    VERB("%s(): run-item=%s n_iters=%u", __FUNCTION__,
         run_item_name(ri), ri->n_iters);

    return TESTER_CFG_WALK_CONT;
}

static tester_cfg_walk_ctl
prepare_test_end(run_item *ri, unsigned int cfg_id_off, unsigned int flags,
                 void *opaque)
{
    config_prepare_data    *gctx = opaque;
    config_prepare_ctx     *ctx;

    UNUSED(cfg_id_off);

    assert(gctx != NULL);
    ctx = SLIST_FIRST(&gctx->ctxs);
    assert(ctx != NULL);

    VERB("%s(): run-item=%s n_iters=%u weight=%u", __FUNCTION__,
         run_item_name(ri), ri->n_iters, ri->weight);

    if (gctx->rc == 0)
    {
        assert(ri->n_iters > 0);
        /* Empty package/session may have zero weight */
        assert(ri->weight > 0 || ri->type != RUN_ITEM_SCRIPT);
        if (~flags & TESTER_CFG_WALK_SERVICE)
        {
            unsigned int ri_tot_iters;

            if (check_mul_overflow(ri->weight, ri->n_iters))
            {
                ERROR("%s(): too many iterations for run item %s",
                      __FUNCTION__, run_item_name(ri));
                gctx->rc = TE_RC(TE_TESTER, TE_EOVERFLOW);
                return TESTER_CFG_WALK_FAULT;
            }

            ri_tot_iters = ri->n_iters * ri->weight;
            if (check_sum_overflow(ri_tot_iters, ctx->total_iters))
            {
                ERROR("%s(): too many iterations for run item %s",
                      __FUNCTION__, run_item_name(ri));
                gctx->rc = TE_RC(TE_TESTER, TE_EOVERFLOW);
                return TESTER_CFG_WALK_FAULT;
            }

            ctx->total_iters += ri_tot_iters;
        }
    }

    return TESTER_CFG_WALK_CONT;
}

static tester_cfg_walk_ctl
prepare_iter_start(run_item *ri, unsigned int cfg_id_off,
                   unsigned int flags, unsigned int iter, void *opaque)
{
    UNUSED(ri);
    UNUSED(cfg_id_off);
    UNUSED(flags);
    UNUSED(opaque);

    /*
     * All iterations are equal from prepare point of view.
     * Moreover, it is assumed that each run item processed only once.
     */
    if (iter == 0)
        return TESTER_CFG_WALK_CONT;
    else
        return TESTER_CFG_WALK_SKIP;
}

static tester_cfg_walk_ctl
prepare_script(run_item *ri, test_script *script,
               unsigned int cfg_id_off, void *opaque)
{
    UNUSED(script);
    UNUSED(cfg_id_off);
    UNUSED(opaque);

    ri->weight = 1;

    return TESTER_CFG_WALK_CONT;
}


/* See the description in tester_conf.h */
te_errno
tester_prepare_configs(tester_cfgs *cfgs)
{
    config_prepare_data     gctx;
    const tester_cfg_walk   cbs = {
        prepare_cfg_start,
        prepare_cfg_end,
        prepare_pkg_start,
        NULL, /* pkg_end */
        prepare_session_start,
        prepare_session_end,
        NULL, /* prologue_start */
        NULL, /* prologue_end */
        NULL, /* epilogue_start */
        NULL, /* epilogue_end */
        NULL, /* keepalive_start */
        NULL, /* keepalive_end */
        NULL, /* exception_start */
        NULL, /* exception_end */
        prepare_test_start,
        prepare_test_end,
        prepare_iter_start,
        NULL, /* iter_end */
        NULL, /* repeat_start */
        NULL, /* repeat_end */
        prepare_script,
        NULL, /* skip_start */
        NULL, /* skip_end */
    };

    te_errno rc;

    ENTRY();

    gctx.rc = 0;
    gctx.expand = false;
    gctx.new_pkg_path = NULL;
    SLIST_INIT(&gctx.ctxs);
    if (config_prepare_new_ctx(&gctx) == NULL)
    {
        EXIT("ENOMEM");
        return TE_RC(TE_TESTER, TE_ENOMEM);
    }

    if (tester_configs_walk(cfgs, &cbs, TESTER_CFG_WALK_FORCE_EXCEPTION,
                            &gctx) == TESTER_CFG_WALK_CONT)
    {
        assert(!SLIST_EMPTY(&gctx.ctxs));

        cfgs->total_iters = SLIST_FIRST(&gctx.ctxs)->total_iters;

        rc = config_prepare_destroy_ctx(&gctx);
        assert(SLIST_EMPTY(&gctx.ctxs));
        if (rc != 0)
        {
            EXIT("%r", rc);
            return rc;
        }

        EXIT("0 - total_iters=%u", cfgs->total_iters);
        return 0;
    }
    else
    {
        while (!SLIST_EMPTY(&gctx.ctxs))
            config_prepare_destroy_ctx(&gctx);

        return gctx.rc;
    }
}
