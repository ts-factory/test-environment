/** @file
 * @brief TAD ATM
 *
 * Traffic Application Domain Command Handler.
 * ATM CSAP support description structures. 
 *
 * Copyright (C) 2006 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "TAD ATM"

#include "te_config.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "te_errno.h"
#include "tad_csap_support.h"
#include "tad_utils.h"

#include "tad_atm_impl.h"


static csap_spt_type_t atm_csap_spt = 
{
    .proto               = "atm",
    .unregister_cb       = NULL,

    .init_cb             = tad_atm_init_cb,
    .destroy_cb          = tad_atm_destroy_cb,
    .get_param_cb        = NULL,

    .confirm_tmpl_cb     = tad_atm_confirm_tmpl_cb,
    .generate_pkts_cb    = tad_atm_gen_bin_cb,
    .release_tmpl_cb     = tad_atm_release_pdu_cb,

    .confirm_ptrn_cb     = tad_atm_confirm_ptrn_cb,
    .match_pre_cb        = tad_atm_match_pre_cb,
    .match_do_cb         = tad_atm_match_do_cb,
    .match_done_cb       = NULL,
    .match_post_cb       = tad_atm_match_post_cb,
    .match_free_cb       = tad_atm_release_pdu_cb,
    .release_ptrn_cb     = tad_atm_release_pdu_cb,

    .generate_pattern_cb = NULL,

    .rw_init_cb          = tad_atm_rw_init_cb,
    .rw_destroy_cb       = tad_atm_rw_destroy_cb,

    .prepare_send_cb     = tad_atm_prepare_send,
    .write_cb            = tad_atm_write_cb,
    .shutdown_send_cb    = tad_atm_shutdown_send,
    
    .prepare_recv_cb     = tad_atm_prepare_recv,
    .read_cb             = tad_atm_read_cb,
    .shutdown_recv_cb    = tad_atm_shutdown_recv,

    .write_read_cb       = tad_common_write_read_cb,
};


static csap_spt_type_t aal5_csap_spt = 
{
    .proto               = "aal5",
    .unregister_cb       = NULL,

    .init_cb             = tad_aal5_init_cb,
    .destroy_cb          = tad_aal5_destroy_cb,
    .get_param_cb        = NULL,

    .confirm_tmpl_cb     = tad_aal5_confirm_tmpl_cb,
    .generate_pkts_cb    = tad_aal5_gen_bin_cb,
    .release_tmpl_cb     = NULL,

    .confirm_ptrn_cb     = tad_aal5_confirm_ptrn_cb,
    .match_pre_cb        = NULL,
    .match_do_cb         = tad_aal5_match_bin_cb,
    .match_done_cb       = NULL,
    .match_post_cb       = NULL,
    .match_free_cb       = NULL,
    .release_ptrn_cb     = NULL,

    .generate_pattern_cb = NULL,

    CSAP_SUPPORT_NO_RW,
};


/**
 * Register ATM CSAP callbacks and support structures in TAD
 * Command Handler.
 *
 * @return Status code.
 */ 
te_errno
csap_support_atm_register(void)
{ 
    te_errno    rc;

    rc = csap_spt_add(&aal5_csap_spt);
    if (rc != 0)
        return rc;

    return csap_spt_add(&atm_csap_spt);
}
