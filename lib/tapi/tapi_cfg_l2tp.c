#include <arpa/inet.h>
#include <string.h>
#include "tapi_cfg_l2tp.h"
#include "te_config.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#endif

#include "te_defs.h"
#include "logger_api.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_aggr.h"
#include "conf_api.h"

#define TE_CFG_TA_L2TP_SERVER "/agent:%s/l2tp:"

/** Max length of the instance /ip_range: */
#define L2TP_IP_RANGE_INST 35

/* All descriptions are in tapi_cfg_l2tp.h */

te_errno
tapi_cfg_l2tp_listen_ip_set(const char *ta, const char *lns,
                            struct sockaddr_in *local)
{

    return cfg_set_instance_fmt(CFG_VAL(ADDRESS, local),
                                TE_CFG_TA_L2TP_SERVER "/listen:", ta);
}


te_errno
tapi_cfg_l2tp_ip_get(const char *ta, const char *lns,
                     struct sockaddr_in *local)
{
    return cfg_get_instance_fmt((cfg_val_type *) CVT_ADDRESS, local,
                                TE_CFG_TA_L2TP_SERVER "/listen:", ta);
}

te_errno
tapi_cfg_l2tp_tunnel_ip_set(const char *ta, const char *lns,
                            struct sockaddr_in *local)
{

    return cfg_set_instance_fmt(CFG_VAL(ADDRESS, local),
                                TE_CFG_TA_L2TP_SERVER "/lns:%s/local_ip:",
                                ta, lns);
}

te_errno
tapi_cfg_l2tp_tunnel_ip_get(const char *ta, const char *lns,
                            struct sockaddr_in *local)
{
    return cfg_get_instance_fmt((cfg_val_type *) CVT_ADDRESS, local,
                                TE_CFG_TA_L2TP_SERVER "/lns:%s/local_ip:",
                                ta, lns);
}

te_errno
tapi_cfg_l2tp_lns_range_add(const char *ta, const char *lns,
                            const l2tp_ipv4_range *iprange,
                            enum l2tp_iprange_class kind)
{
    cfg_handle  handle;
    char        buf[L2TP_IP_RANGE_INST] = {};
    char       *end = strdup(inet_ntoa(iprange->end->sin_addr));
    TE_SPRINTF(buf, "%s%s-%s",
               iprange->type == L2TP_POLICY_ALLOW ?
               "allow" : "deny",
               inet_ntoa(iprange->start->sin_addr), end);


    switch (kind)
    {
        case L2TP_IP_RANGE_CLASS_IP:
            return cfg_add_instance_fmt(&handle, CFG_VAL(NONE, NULL),
                                        TE_CFG_TA_L2TP_SERVER
                                        "/lns:%s/ip_range:%s",
                                        ta, lns, buf);
        case L2TP_IP_RANGE_CLASS_LAC:
            return cfg_add_instance_fmt(&handle, CFG_VAL(NONE, NULL),
                                        TE_CFG_TA_L2TP_SERVER
                                        "/lns:%s/lac_range:%s",
                                        ta, lns, buf);
        default:
            return TE_RC(TE_TAPI, TE_EINVAL);
    }
}


te_errno
tapi_cfg_l2tp_lns_range_del(const char *ta, const char *lns,
                            const l2tp_ipv4_range *iprange,
                            enum l2tp_iprange_class kind)
{
    char        buf[L2TP_IP_RANGE_INST] = {};
    char       *end = strdup(inet_ntoa(iprange->end->sin_addr));
    TE_SPRINTF(buf, "%s%s-%s",
               iprange->type == L2TP_POLICY_ALLOW ?
               "allow" : "deny",
               inet_ntoa(iprange->start->sin_addr), end);

    switch (kind)
    {
        case L2TP_IP_RANGE_CLASS_IP:
            return cfg_del_instance_fmt(FALSE, TE_CFG_TA_L2TP_SERVER
                                        "/lns:%s/ip_range:%s",
                                        ta, lns, buf);
        case L2TP_IP_RANGE_CLASS_LAC:
            return cfg_del_instance_fmt(FALSE, TE_CFG_TA_L2TP_SERVER
                                        "/lns:%s/lac_range:%s",
                                        ta, lns, buf);
        default:
            return TE_RC(TE_TAPI, TE_EINVAL);
    }
}

te_errno
tapi_cfg_l2tp_lns_bit_set(const char *ta, const char *lns,
                          enum l2tp_bit bit, bool selector)
{

}