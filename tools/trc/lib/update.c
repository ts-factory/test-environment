/** @file
 * @brief Testing Results Comparator: report tool
 *
 * Auxiluary routines for TRC update tool.
 *
 *
 * Copyright (C) 2011 Test Environment authors (see file AUTHORS
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
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#include "te_config.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#include "te_alloc.h"
#include "te_queue.h"
#include "trc_update.h"

/* See the description in trc_update.h */
void
trc_update_free_test_iter_data(trc_update_test_iter_data *data)
{
    trc_exp_result             *q;
    trc_exp_result             *q_tvar;

    if (data == NULL)
        return;

    SLIST_FOREACH_SAFE(q, &data->new_results, links, q_tvar)
    {
        SLIST_REMOVE_HEAD(&data->new_results, links);
        trc_exp_result_free(q);
    }

    for (data->args_n = 0; data->args_n < data->args_max; data->args_n++)
    {
        free(data->args[data->args_n].name);
        free(data->args[data->args_n].value);
    }
    free(data->args);

    free(data);
}

/* See the description in trc_update.h */
void
trc_update_wilds_list_entry_free(trc_update_wilds_list_entry *entry)
{
    if (entry == NULL)
        return;

    trc_free_test_iter_args(entry->args);
}

/* See the description in trc_update.h */
void
trc_update_wilds_list_free(trc_update_wilds_list *list)
{
    trc_update_wilds_list_entry *entry;
    trc_update_wilds_list_entry *entry_aux;

    if (list == NULL)
        return;

    SLIST_FOREACH_SAFE(entry, list, links, entry_aux)
    {
        trc_update_wilds_list_entry_free(entry);
        free(entry);
    }
}

/* See the description in trc_update.h */
void
trc_update_rule_free(trc_update_rule *rule)
{
    if (rule == NULL)
        return;

    trc_exp_result_free(rule->def_res);
    trc_exp_results_free(rule->old_res);
    trc_exp_results_free(rule->new_res);
    trc_exp_results_free(rule->confl_res);
    trc_update_wilds_list_free(rule->wilds);
    if (rule->match_exprs != NULL)
        tq_strings_free(rule->match_exprs, free);
}

/* See the description in trc_update.h */
void
trc_update_rules_free(trc_update_rules *rules)
{
    trc_update_rule *p;

    if (rules == NULL)
        return;

    while ((p = TAILQ_FIRST(rules)) != NULL)
    {
        TAILQ_REMOVE(rules, p, links);
        trc_update_rule_free(p);
        free(p);
    }
}

/* See the description in trc_update.h */
void
trc_update_test_entry_free(trc_update_test_entry *test_entry)
{
    if (test_entry == NULL)
        return;

    trc_update_rules_free(test_entry->rules);
}

/* See the description in trc_update.h */
void
trc_update_test_entries_free(trc_update_test_entries *tests)
{
    trc_update_test_entry *p;

    if (tests == NULL)
        return;

    while ((p = TAILQ_FIRST(tests)) != NULL)
    {
        TAILQ_REMOVE(tests, p, links);
        trc_update_test_entry_free(p);
        free(p);
    }
}

/* See the description in trc_update.h */
void
trc_update_args_group_free(trc_update_args_group *args_group)
{
    trc_free_test_iter_args(args_group->args);
}

/* See the description in trc_update.h */
void
trc_update_args_groups_free(trc_update_args_groups *args_groups)
{
    trc_update_args_group   *args_group;

    while ((args_group = SLIST_FIRST(args_groups)) != NULL)
    {
        SLIST_REMOVE_HEAD(args_groups, links);
        trc_update_args_group_free(args_group);
        free(args_group);
    }
}

/* See the description in trc_update.h */
trc_test_iter_args *
trc_update_args_wild_dup(trc_test_iter_args *args)
{
    trc_test_iter_args  *dup;
    trc_test_iter_arg   *arg;
    trc_test_iter_arg   *arg_dup;

    dup = TE_ALLOC(sizeof(*dup));
    
    TAILQ_INIT(&dup->head);

    TAILQ_FOREACH(arg, &args->head, links)
    {
        arg_dup = TE_ALLOC(sizeof(*arg_dup));
        if (arg->name != NULL)
            arg_dup->name = strdup(arg->name);
        arg_dup->value = TE_ALLOC(1);
        arg_dup->value[0] = '\0';
        TAILQ_INSERT_TAIL(&dup->head, arg_dup, links);
    }

    return dup;
}

/* See the description in trc_update.h */
int
te_test_result_cmp(te_test_result *p, te_test_result *q)
{
    int              rc;
    te_test_verdict *p_v;
    te_test_verdict *q_v;

    rc = p->status - q->status;
    if (rc != 0)
        return rc;
    else
    {
        p_v = TAILQ_FIRST(&p->verdicts); 
        q_v = TAILQ_FIRST(&q->verdicts);
        
        while (p_v != NULL && q_v != NULL)
        {
            rc = strcmp(p_v->str, q_v->str);
            if (rc != 0)
                return rc;

            p_v = TAILQ_NEXT(p_v, links);
            q_v = TAILQ_NEXT(q_v, links);
        }

        if (p_v == NULL && q_v == NULL)
            return 0;
        else if (p_v != NULL)
            return 1;
        else
            return -1;
    }
}


/* See the description in trc_update.h */
te_bool
trc_update_is_to_save(void *data, te_bool is_iter)
{
    if (data == NULL)
        return FALSE;

    if (is_iter)
    {
        trc_update_test_iter_data *iter_d =
            (trc_update_test_iter_data *)data;

        return iter_d->to_save;
    }
    else
    {
        trc_update_test_data *test_d =
            (trc_update_test_data *)data;
    
        return test_d->to_save;
    }
}

/* See the description in trc_update.h */
char *
trc_update_set_user_attr(void *data, te_bool is_iter)
{
    char *attr_value;
    int   val_len = 20;

    if (data == NULL)
        return FALSE;

    if (is_iter)
    {
        trc_update_test_iter_data *iter_d =
            (trc_update_test_iter_data *)data;

        if (iter_d->rule_id == 0)
            return NULL;

        attr_value = TE_ALLOC(val_len);
        snprintf(attr_value, val_len, "rule_%d", iter_d->rule_id);

        return attr_value;
    }
    else
        return NULL;
}

/* See the description in trc_update.h */
void
trc_update_init_ctx(trc_update_ctx *ctx_p)
{
    memset(ctx_p, 0, sizeof(ctx_p));
    TAILQ_INIT(&ctx_p->test_names);
    TAILQ_INIT(&ctx_p->tags_logs);
}

/* See the description in trc_update.h */
void
trc_update_free_ctx(trc_update_ctx *ctx)
{
    if (ctx == NULL)
        return;

    trc_update_tags_logs_free(&ctx->tags_logs);
    tq_strings_free(&ctx->test_names, free);
    free(ctx->fake_log);
    free(ctx->rules_load_from);
    free(ctx->rules_save_to);
    free(ctx->cmd);
}

/* See the description in trc_update.h */
void
tag_logs_init(trc_update_tag_logs *tag_logs)
{
    memset(tag_logs, 0, sizeof(tag_logs));
    TAILQ_INIT(&tag_logs->logs);
}

/* See the description in trc_update.h */
void
trc_update_tag_logs_free(trc_update_tag_logs *tag_logs)
{
    if (tag_logs == NULL)
        return;

    free(tag_logs->tags_str);
    logic_expr_free(tag_logs->tags_expr);
    tq_strings_free(&tag_logs->logs, free);
}

/* See the description in trc_update.h */
void
trc_update_tags_logs_free(trc_update_tags_logs *tags_logs)
{
    trc_update_tag_logs     *tl = NULL;

    if (tags_logs == NULL)
        return;

    while ((tl = TAILQ_FIRST(tags_logs)) != NULL)
    {
        TAILQ_REMOVE(tags_logs, tl, links);
        trc_update_tag_logs_free(tl);
        free(tl);
    }
}
