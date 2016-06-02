/** @file
 * @brief RPC for DPDK MBUF
 *
 * RPC routines implementation to call DPDK (rte_mbuf_* and rte_pktmbuf_*)
 * functions
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * @author Ivan Malov <Ivan.Malov@oktetlabs.ru>
 */

#define TE_LGR_USER     "RPC DPDK MBUF"

#include "te_config.h"
#ifdef HAVE_PACKAGE_H
#include "package.h"
#endif

#include "rte_config.h"
#include "rte_mbuf.h"

#include "logger_api.h"

#include "unix_internal.h"
#include "tarpc_server.h"
#include "rpcs_dpdk.h"

TARPC_FUNC(rte_pktmbuf_pool_create, {},
{
    struct rte_mempool *mp;

    MAKE_CALL(mp = func(in->name, in->n, in->cache_size, in->priv_size,
                        in->data_room_size, in->socket_id));

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MEMPOOL, {
        out->retval = RCF_PCH_MEM_INDEX_ALLOC(mp, ns);
    });
}
)

TARPC_FUNC_STATIC(rte_pktmbuf_alloc, {},
{
    struct rte_mempool *mp = NULL;
    struct rte_mbuf *m;

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MEMPOOL, {
        mp = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->mp, ns);
    });

    MAKE_CALL(m = func(mp));

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MBUF, {
        out->retval = RCF_PCH_MEM_INDEX_ALLOC(m, ns);
    });
}
)

TARPC_FUNC_STATIC(rte_pktmbuf_free, {},
{
    struct rte_mbuf *m = NULL;

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MBUF, {
        m = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->m, ns);
    });

    MAKE_CALL(func(m));

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MBUF, {
        RCF_PCH_MEM_INDEX_FREE(in->m, ns);
    });
}
)

TARPC_FUNC_STANDALONE(rte_pktmbuf_append_data, {},
{
    struct rte_mbuf *m = NULL;
    uint8_t *dst;
    te_errno err = 0;

    if ((in->buf.buf_len != 0) && (in->buf.buf_val == NULL))
    {
        ERROR("Incorrect input data");
        err =  TE_RC(TE_RPCS, TE_EINVAL);
        goto finish;
    }

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MBUF, {
        m = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->m, ns);
    });

    MAKE_CALL(dst = (uint8_t *)rte_pktmbuf_append(m, in->buf.buf_len));

    if (dst == NULL)
    {
        ERROR("Not enough tailroom space in the last segment of the mbuf");
        err = TE_RC(TE_RPCS, TE_ENOSPC);
        goto finish;
    }

    memcpy(dst, in->buf.buf_val, in->buf.buf_len);

finish:
    out->retval = -err;
}
)

TARPC_FUNC_STANDALONE(rte_pktmbuf_read_data,
{
    COPY_ARG(buf);
},
{
    struct rte_mbuf *m = NULL;
    te_errno err = 0;
    ssize_t bytes_read = 0;
    size_t cur_offset = in->offset;

    if (in->buf.buf_val == NULL)
    {
        ERROR("Incorrect buffer");
        err =  TE_RC(TE_RPCS, TE_EINVAL);
        goto finish;
    }

    if (in->len > in->buf.buf_len)
    {
        ERROR("Not enough room in the specified buffer");
        err = TE_RC(TE_RPCS, TE_ENOSPC);
        goto finish;
    }

    RPC_PCH_MEM_WITH_NAMESPACE(ns, RPC_TYPE_NS_RTE_MBUF, {
        m = RCF_PCH_MEM_INDEX_MEM_TO_PTR(in->m, ns);
    });

    if (m == NULL)
    {
        ERROR("NULL mbuf pointer isn't valid for 'read' operation");
        err = TE_RC(TE_RPCS, TE_EINVAL);
        goto finish;
    }

    do {
        if (cur_offset < m->data_len)
        {
            size_t bytes_to_copy = MIN(m->data_len - cur_offset,
                                       (in->len - bytes_read));

            memcpy(out->buf.buf_val + bytes_read,
                   rte_pktmbuf_mtod_offset(m, uint8_t *, cur_offset),
                   bytes_to_copy);

            bytes_read += bytes_to_copy;
            cur_offset = 0;
        }
        else
        {
            cur_offset -= m->data_len;
        }
    } while ((in->len - bytes_read) != 0 && (m = m->next) != NULL);

finish:
    out->retval = (err != 0) ? -err : bytes_read;
}
)
