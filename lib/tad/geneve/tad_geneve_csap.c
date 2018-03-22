/** @file
 * @brief TAD Geneve
 *
 * Traffic Application Domain Command Handler
 * Geneve CSAP support description structures
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights served.
 *
 * 
 *
 * @author Ivan Malov <Ivan.Malov@oktetlabs.ru>
 */

#define TE_LGR_USER "TAD Geneve"

#include "te_config.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "te_errno.h"
#include "tad_csap_support.h"

#include "tad_geneve_impl.h"

static csap_spt_type_t geneve_csap_spt =
{
    .proto               = "geneve",
    .unregister_cb       = NULL,

    .init_cb             = tad_geneve_init_cb,
    .destroy_cb          = tad_geneve_destroy_cb,
    .get_param_cb        = NULL,

    .confirm_tmpl_cb     = tad_geneve_confirm_tmpl_cb,
    .generate_pkts_cb    = tad_geneve_gen_bin_cb,
    .release_tmpl_cb     = tad_geneve_release_pdu_cb,

    .confirm_ptrn_cb     = tad_geneve_confirm_ptrn_cb,
    .match_pre_cb        = NULL,
    .match_do_cb         = tad_geneve_match_do_cb,
    .match_done_cb       = NULL,
    .match_post_cb       = tad_geneve_match_post_cb,
    .match_free_cb       = tad_geneve_release_pdu_cb,
    .release_ptrn_cb     = tad_geneve_release_pdu_cb,

    .generate_pattern_cb = NULL,

    CSAP_SUPPORT_NO_RW,
};

/**
 * Register Geneve CSAP callbacks and support structures in TAD CH
 *
 * @return Status code
 */
te_errno
csap_support_geneve_register(void)
{
    return csap_spt_add(&geneve_csap_spt);
}
