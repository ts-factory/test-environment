/** @file
 * @brief RPC for RTE flow
 *
 * RPC for RTE flow functions
 *
 *
 * Copyright (C) 2017 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Roman Zhukov <Roman.Zhukov@oktetlabs.ru>
 */

#define TE_LGR_USER     "RPC rte_flow"

#include "te_config.h"
#ifdef HAVE_PACKAGE_H
#include "package.h"
#endif

#include "rte_config.h"
#include "rte_flow.h"

#include "asn_usr.h"
#include "asn_impl.h"
#include "ndn_rte_flow.h"

#include "logger_api.h"

#include "rpc_server.h"
#include "rpcs_dpdk.h"
#include "te_errno.h"
#include "te_defs.h"
#include "tad_common.h"

#define RTE_FLOW_BOOL_FILEDS_LEN 1

static te_errno
rte_flow_attr_from_ndn(const asn_value *ndn_attr,
                       struct rte_flow_attr **attr_out)
{
    int                     rc;
    struct rte_flow_attr   *attr;

    if (attr_out == NULL)
        return TE_EINVAL;

#define ASN_READ_ATTR_FIELD(_asn_val, _name, _data, _size) \
    do {                                                            \
        size_t __size = _size;                                      \
        unsigned int __val;                                         \
                                                                    \
        rc = asn_read_value_field(_asn_val, &__val,                 \
                                  &__size, #_name);                 \
        if (rc == 0)                                                \
            _data = __val;                                          \
        else if (rc != TE_EASNINCOMPLVAL)                           \
            goto out;                                               \
    } while (0)

    attr = calloc(1, sizeof(*attr));
    if (attr == NULL)
    {
        rc = TE_ENOMEM;
        goto out;
    }

    ASN_READ_ATTR_FIELD(ndn_attr, group, attr->group, sizeof(attr->group));
    ASN_READ_ATTR_FIELD(ndn_attr, priority, attr->priority, sizeof(attr->priority));
    ASN_READ_ATTR_FIELD(ndn_attr, ingress, attr->ingress, RTE_FLOW_BOOL_FILEDS_LEN);
    ASN_READ_ATTR_FIELD(ndn_attr, egress, attr->egress, RTE_FLOW_BOOL_FILEDS_LEN);
#undef ASN_READ_ATTR_FIELD

    *attr_out = attr;
    return 0;
out:
    free(attr);
    return rc;
}

static te_errno
rte_int_hton(uint32_t val, void *data, size_t size)
{
    switch (size)
    {
        case sizeof(uint8_t):
            *(uint8_t *)data = val;
            break;

        case sizeof(uint16_t):
            *(uint16_t *)data = rte_cpu_to_be_16(val);
            break;

        case sizeof(uint32_t):
            *(uint32_t *)data = rte_cpu_to_be_32(val);
            break;

        default:
            return TE_EINVAL;
    }

    return 0;
}

#define ASN_READ_INT_RANGE_FIELD(_asn_val, _name, _field, _size) \
    do {                                                                \
        size_t __size = _size;                                          \
        uint32_t __val;                                                 \
                                                                        \
        rc = asn_read_value_field(_asn_val, &__val,                     \
                                  &__size, #_name ".#range.first");     \
        if (rc == 0)                                                    \
            rc = rte_int_hton(__val, &spec->_field, _size);             \
        if (rc == 0 || rc == TE_EASNINCOMPLVAL)                         \
            rc = asn_read_value_field(_asn_val, &__val,                 \
                                      &__size, #_name ".#range.last");  \
        if (rc == 0)                                                    \
            rc = rte_int_hton(__val, &last->_field, _size);             \
        if (rc == 0 || rc == TE_EASNINCOMPLVAL)                         \
            rc = asn_read_value_field(_asn_val, &__val,                 \
                                      &__size, #_name ".#range.mask");  \
        if (rc == 0)                                                    \
            rc = rte_int_hton(__val, &mask->_field, _size);             \
        if (rc != 0 && rc != TE_EASNINCOMPLVAL)                         \
            goto out;                                                   \
    } while (0)

#define ASN_READ_MAC_ADDR_RANGE_FIELD(_asn_val, _field) \
    do {                                                                \
        size_t __size = ETHER_ADDR_LEN;                                 \
        struct ether_addr __addr;                                       \
                                                                        \
        rc = asn_read_value_field(_asn_val, __addr.addr_bytes, &__size, \
                                  #_field "-addr.#range.first");        \
        if (rc == 0)                                                    \
            memcpy(spec->_field.addr_bytes, __addr.addr_bytes, __size); \
        rc = asn_read_value_field(_asn_val, __addr.addr_bytes, &__size, \
                                  #_field "-addr.#range.last");         \
        if (rc == 0)                                                    \
            memcpy(last->_field.addr_bytes, __addr.addr_bytes, __size); \
        rc = asn_read_value_field(_asn_val, __addr.addr_bytes, &__size, \
                                  #_field "-addr.#range.mask");         \
        if (rc == 0)                                                    \
            memcpy(mask->_field.addr_bytes, __addr.addr_bytes, __size); \
        if (rc != 0 && rc != TE_EASNINCOMPLVAL)                         \
            goto out;                                                   \
    } while (0)

static te_errno
rte_alloc_mem_for_flow_item(void **spec_out, void **mask_out,
                            void **last_out, size_t size)
{
    uint8_t    *spec = NULL;
    uint8_t    *mask = NULL;
    uint8_t    *last = NULL;

    if (spec_out == NULL || mask_out == NULL || last_out == NULL)
        return TE_EINVAL;

    spec = calloc(1, size);
    if (spec == NULL)
        goto out_spec;

    mask = calloc(1, size);
    if (mask == NULL)
        goto out_mask;

    last = calloc(1, size);
    if (last == NULL)
        goto out_last;

    *spec_out = spec;
    *mask_out = mask;
    *last_out = last;

    return 0;

out_last:
    free(mask);
out_mask:
    free(spec);
out_spec:
    return TE_ENOMEM;
}

static te_errno
rte_flow_item_void(__rte_unused const asn_value *void_pdu,
                   struct rte_flow_item *item)
{
    if (item == NULL)
        return TE_EINVAL;

    item->type = RTE_FLOW_ITEM_TYPE_VOID;
    return 0;
}

static te_errno
rte_flow_item_eth_from_pdu(const asn_value *eth_pdu,
                           struct rte_flow_item *item)
{
    struct rte_flow_item_eth *spec = NULL;
    struct rte_flow_item_eth *mask = NULL;
    struct rte_flow_item_eth *last = NULL;
    int rc;

    if (item == NULL)
        return TE_EINVAL;

/*
 * If spec/mask/last is zero then the appropriate
 * item's field remains NULL
 */
#define FILL_FLOW_ITEM_ETH(_field) \
    do {                                            \
        if (!is_zero_ether_addr(&_field->dst) ||    \
            !is_zero_ether_addr(&_field->src) ||    \
            _field->type != 0)                      \
            item->_field = _field;                  \
        else                                        \
            free(_field);                           \
    } while(0)

    rc = rte_alloc_mem_for_flow_item((void **)&spec,
                                     (void **)&mask,
                                     (void **)&last,
                                     sizeof(struct rte_flow_item_eth));
    if (rc != 0)
        return rc;

    ASN_READ_MAC_ADDR_RANGE_FIELD(eth_pdu, dst);
    ASN_READ_MAC_ADDR_RANGE_FIELD(eth_pdu, src);
    ASN_READ_INT_RANGE_FIELD(eth_pdu, length-type, type, sizeof(spec->type));

    item->type = RTE_FLOW_ITEM_TYPE_ETH;
    FILL_FLOW_ITEM_ETH(spec);
    FILL_FLOW_ITEM_ETH(mask);
    FILL_FLOW_ITEM_ETH(last);
#undef FILL_FLOW_ITEM_ETH

    return 0;
out:
    free(spec);
    free(mask);
    free(last);
    return rc;
}

static te_errno
rte_flow_item_end(struct rte_flow_item *item)
{
    if (item == NULL)
        return TE_EINVAL;

    item->type = RTE_FLOW_ITEM_TYPE_END;
    return 0;
}

static void
rte_flow_free_pattern(struct rte_flow_item *pattern,
                      unsigned int len)
{
    unsigned int i;

    if (pattern != NULL)
    {
        for (i = 0; i < len; i++)
        {
            free((void *)pattern[i].spec);
            free((void *)pattern[i].mask);
            free((void *)pattern[i].last);
        }
        free(pattern);
    }
}

/*
 * Convert item/action ASN value to rte_flow structures and add
 * to the appropriate list using the map of conversion functions
 */
#define ASN_VAL_CONVERT(_asn_val, _tag, _map, _list) \
    do {                                                        \
        unsigned int i;                                         \
                                                                \
        for (i = 0; i < TE_ARRAY_LEN(_map); i++)                \
        {                                                       \
            if (_tag == _map[i].tag)                            \
            {                                                   \
                rc = _map[i].convert(_asn_val, &_list[i]);      \
                if (rc != 0)                                    \
                    goto out;                                   \
                                                                \
                break;                                          \
            }                                                   \
        }                                                       \
        if (i == TE_ARRAY_LEN(_map))                            \
        {                                                       \
            rc = TE_EINVAL;                                     \
            goto out;                                           \
        }                                                       \
    } while(0)

/* The mapping list of protocols tags and conversion functions */
static const struct rte_flow_item_tags_mapping {
    asn_tag_value   tag;
    te_errno      (*convert)(const asn_value *item_pdu,
                             struct rte_flow_item *item);
} rte_flow_item_tags_map[] = {
    { TE_PROTO_ETH,     rte_flow_item_eth_from_pdu },
    { 0,                rte_flow_item_void },
};

static te_errno
rte_flow_pattern_from_ndn(const asn_value *ndn_flow,
                          struct rte_flow_item **pattern_out,
                          unsigned int *pattern_len)
{
    unsigned int            i;
    int                     rc;
    asn_value              *gen_pdu;
    asn_value              *item_pdu;
    asn_tag_value           item_tag;
    struct rte_flow_item   *pattern = NULL;
    unsigned int            ndn_len;

    if (pattern_out == NULL || pattern_len == NULL)
        return TE_EINVAL;

    ndn_len = (unsigned int)asn_get_length(ndn_flow, "pattern");
    /*
     * Item END is not specified in the pattern NDN
     * and it should be the last item
     */
    *pattern_len = ndn_len + 1;

    pattern = calloc(*pattern_len, sizeof(*pattern));
    if (pattern == NULL)
        return TE_ENOMEM;

    for (i = 0; i < ndn_len; i++)
    {
        rc = asn_get_indexed(ndn_flow, &gen_pdu, i, "pattern");
        if (rc != 0)
            goto out;

        rc = asn_get_choice_value(gen_pdu, &item_pdu, NULL, &item_tag);
        if (rc != 0)
            goto out;

        ASN_VAL_CONVERT(item_pdu, item_tag, rte_flow_item_tags_map, pattern);
    }
    rte_flow_item_end(&pattern[ndn_len]);

    *pattern_out = pattern;
    return 0;
out:
    rte_flow_free_pattern(pattern, *pattern_len);
    return rc;
}

static te_errno
rte_flow_action_void(__rte_unused const asn_value *conf_pdu,
                     struct rte_flow_action *action)
{
    if (action == NULL)
        return TE_EINVAL;

    action->type = RTE_FLOW_ACTION_TYPE_VOID;
    return 0;
}

static te_errno
rte_flow_action_queue_from_pdu(const asn_value *conf_pdu,
                               struct rte_flow_action *action)
{
    struct rte_flow_action_queue   *conf = NULL;
    asn_tag_value                   tag;
    size_t                          len;
    int                             rc;
    unsigned int                    val;

    if (action == NULL)
        return TE_EINVAL;

    rc = asn_get_choice_value(conf_pdu, NULL, NULL, &tag);
    if (rc != 0)
        return rc;
    else if (tag != NDN_FLOW_ACTION_QID)
        return TE_EINVAL;

    conf = calloc(1, sizeof(*conf));
    if (conf == NULL)
        return TE_ENOMEM;

    len = sizeof(conf->index);
    rc = asn_read_value_field(conf_pdu, &val, &len, "#index");
    if (rc != 0)
    {
        free(conf);
        return rc;
    }

    conf->index = val;

    action->type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action->conf = conf;

    return 0;
}

static te_errno
rte_flow_action_end(struct rte_flow_action *action)
{
    if (action == NULL)
        return TE_EINVAL;

    action->type = RTE_FLOW_ACTION_TYPE_END;
    return 0;
}

static void
rte_flow_free_actions(struct rte_flow_action *actions,
                      unsigned int len)
{
    unsigned int i;

    if (actions != NULL)
    {
        for (i = 0; i < len; i++)
            free((void *)actions[i].conf);

        free(actions);
    }
}

/* The mapping list of action types and conversion functions */
static const struct rte_flow_action_types_mapping {
    uint8_t         tag;
    te_errno      (*convert)(const asn_value *conf_pdu,
                             struct rte_flow_action *action);
} rte_flow_action_types_map[] = {
    { NDN_FLOW_ACTION_TYPE_QUEUE,   rte_flow_action_queue_from_pdu },
    { NDN_FLOW_ACTION_TYPE_VOID,    rte_flow_action_void },
};

static te_errno
rte_flow_actions_from_ndn(const asn_value *ndn_flow,
                          struct rte_flow_action **actions_out)
{
    unsigned int                i;
    size_t                      size;
    int                         rc;
    asn_value                  *action;
    asn_value                  *conf;
    uint8_t                     type;
    struct rte_flow_action     *actions;
    unsigned int                actions_len;
    unsigned int                ndn_len;

    if (actions_out == NULL)
        return TE_EINVAL;

    ndn_len = (unsigned int)asn_get_length(ndn_flow, "actions");
    /*
     * Action END is not specified in the actions NDN
     * and it should be the last action
     */
    actions_len = ndn_len + 1;

    actions = calloc(actions_len, sizeof(*actions));
    if (actions == NULL)
        return TE_ENOMEM;

    for (i = 0; i < ndn_len; i++)
    {
        rc = asn_get_indexed(ndn_flow, &action, i, "actions");
        if (rc != 0)
            goto out;

        size = sizeof(type);
        rc = asn_read_value_field(action, &type, &size, "type");
        if (rc != 0)
            goto out;

        rc = asn_get_subvalue(action, &conf, "conf");
        if (rc != 0)
            goto out;

        ASN_VAL_CONVERT(conf, type, rte_flow_action_types_map, actions);
    }
    rte_flow_action_end(&actions[ndn_len]);

    *actions_out = actions;
    return 0;
out:
    rte_flow_free_actions(actions, actions_len);
    return rc;
}

static te_errno
rte_flow_from_ndn(const asn_value *ndn_flow,
                  struct rte_flow_attr **attr_out,
                  struct rte_flow_item **pattern_out,
                  struct rte_flow_action **actions_out)
{
    const asn_type *type;
    asn_value      *ndn_attr;
    unsigned int    pattern_len;
    int             rc;

    type = asn_get_type(ndn_flow);
    if (type != ndn_rte_flow_rule)
    {
        rc = TE_EINVAL;
        goto out;
    }

    rc = asn_get_subvalue(ndn_flow, &ndn_attr, "attr");
    if (rc != 0)
        goto out;

    rc = rte_flow_attr_from_ndn(ndn_attr, attr_out);
    if (rc != 0)
        goto out;

    rc = rte_flow_pattern_from_ndn(ndn_flow, pattern_out, &pattern_len);
    if (rc != 0)
        goto out_pattern;

    rc = rte_flow_actions_from_ndn(ndn_flow, actions_out);
    if (rc != 0)
        goto out_actions;

    return 0;

out_actions:
    rte_flow_free_pattern(*pattern_out, pattern_len);
out_pattern:
    free(*attr_out);
out:
    return rc;
}

static int
tarpc_rte_error_type2tarpc(const enum rte_flow_error_type rte,
                           enum tarpc_rte_flow_error_type *rpc)
{
#define CASE_RTE2TARPC(_rte) \
    case (_rte): *rpc = TARPC_##_rte; break

    switch (rte)
    {
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_NONE);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_UNSPECIFIED);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_HANDLE);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ATTR_GROUP);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ATTR_INGRESS);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ATTR_EGRESS);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ATTR);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ITEM_NUM);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ITEM);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ACTION_NUM);
        CASE_RTE2TARPC(RTE_FLOW_ERROR_TYPE_ACTION);
#undef CASE_RTE2TARPC

        default:
            return -1;
    }
    return 0;
}

static int
tarpc_rte_error2tarpc(struct tarpc_rte_flow_error *tarpc_error,
                      struct rte_flow_error *error)
{
    enum tarpc_rte_flow_error_type  tarpc_error_type;

    if (tarpc_rte_error_type2tarpc(error->type, &tarpc_error_type) != 0)
        return -1;

    tarpc_error->type = tarpc_error_type;

    if (error->message != NULL)
        tarpc_error->message = strdup(error->message);
    else
        tarpc_error->message = strdup("");

    return 0;
}

static void
rte_free_flow_rule(struct rte_flow_attr *attr,
                   struct rte_flow_item *pattern,
                   struct rte_flow_action *actions)
{
    unsigned int i;

    free(attr);

    if (pattern != NULL)
    {
        for (i = 0; pattern[i].type != RTE_FLOW_ITEM_TYPE_END; i++)
        {
            free((void *)pattern[i].spec);
            free((void *)pattern[i].mask);
            free((void *)pattern[i].last);
        }
        free(pattern);
    }

    if (actions != NULL)
    {
        for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++)
            free((void *)actions[i].conf);
        free(actions);
    }
}

TARPC_FUNC_STATIC(rte_free_flow_rule, {},
{
    struct rte_flow_attr           *attr = NULL;
    struct rte_flow_item           *pattern = NULL;
    struct rte_flow_action         *actions = NULL;

   RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_FLOW, {
        attr = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->attr, ns);
        pattern = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->pattern, ns);
        actions = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->actions, ns);
    });

    MAKE_CALL(func(attr, pattern, actions));

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_FLOW, {
        RCF_PCH_MEM_INDEX_FREE(in->attr, ns);
        RCF_PCH_MEM_INDEX_FREE(in->pattern, ns);
        RCF_PCH_MEM_INDEX_FREE(in->actions, ns);
    });
})

TARPC_FUNC_STANDALONE(rte_mk_flow_rule_from_str, {},
{
    te_errno                        rc = 0;
    int                             num_symbols_parsed;
    asn_value                      *flow_rule = NULL;
    struct rte_flow_attr           *attr = NULL;
    struct rte_flow_item           *pattern = NULL;
    struct rte_flow_action         *actions = NULL;

    rc = asn_parse_value_text(in->flow_rule, ndn_rte_flow_rule,
                              &flow_rule, &num_symbols_parsed);
    if (rc != 0)
        goto out;

    rc = rte_flow_from_ndn(flow_rule, &attr, &pattern, &actions);
    if (rc != 0)
        goto out;

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_FLOW, {
        out->attr = RCF_PCH_MEM_INDEX_ALLOC(attr, ns);
        out->pattern = RCF_PCH_MEM_INDEX_ALLOC(pattern, ns);
        out->actions = RCF_PCH_MEM_INDEX_ALLOC(actions, ns);
        rc = 0;
    });

out:
    out->retval = -TE_RC(TE_RPCS, rc);
    asn_free_value(flow_rule);
})

TARPC_FUNC(rte_flow_validate, {},
{
    struct rte_flow_attr           *attr = NULL;
    struct rte_flow_item           *pattern = NULL;
    struct rte_flow_action         *actions = NULL;
    struct rte_flow_error           error;

    memset(&error, 0, sizeof(error));

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_FLOW, {
        attr = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->attr, ns);
        pattern = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->pattern, ns);
        actions = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->actions, ns);
    });

    MAKE_CALL(out->retval = func(in->port_id, attr, pattern, actions, &error));
    neg_errno_h2rpc(&out->retval);

    if (tarpc_rte_error2tarpc(&out->error, &error) != 0)
        out->retval = -TE_RC(TE_RPCS, TE_EINVAL);
})