/** @file
 * @brief TCP states API library
 *
 * TCP states API library - implementation of handlers used to change
 * TCP state, for the case when a socket is used on a peer.
 *
 * Copyright (C) 2011-2017 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
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
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#include "te_config.h"

#include "te_defs.h"
#include "logger_api.h"
#include "te_sleep.h"
#include "tapi_test.h"
#include "tapi_rpc_socket.h"
#include "tapi_rpc_unistd.h"

#include "tapi_tcp_states.h"
#include "tapi_tcp_states_internal.h"

/**
 * Action to be done when SYN must be sent from socket on the IUT side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
iut_syn_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;

    RPC_AWAIT_ERROR(ss->config.pco_iut);
    if (rpc_connect(ss->config.pco_iut, ss->state.iut_s,
                    ss->config.tst_addr) != 0 &&
        RPC_ERRNO(ss->config.pco_iut) != RPC_EINPROGRESS)
        return RPC_ERRNO(ss->config.pco_iut);

    ss->state.iut_wait_connect = TRUE;
    rc = tsa_update_cur_state(ss);
    if (rc != 0)
        return rc;

    if (tsa_state_cur(ss) == RPC_TCP_CLOSE)
    {
        rc = iut_wait_change_gen(ss, MAX_CHANGE_TIMEOUT);
        if (rc != 0)
            return rc;
    }

    return 0;
}

/**
 * Action to be done when SYN must be sent from socket on the TST side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
tst_syn_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;

    if (ss->state.sock.tst_s >= 0)
    {
        RPC_AWAIT_ERROR(ss->config.pco_tst);
        if (rpc_close(ss->config.pco_tst, ss->state.sock.tst_s) < 0)
            return RPC_ERRNO(ss->config.pco_tst);
    }

    ss->state.sock.tst_s = -1;
    rc = tsa_sock_create(ss, TSA_TST);
    if (rc != 0)
        return rc;

    ss->config.pco_tst->op = RCF_RPC_CALL;
    RPC_AWAIT_ERROR(ss->config.pco_tst);
    if (rpc_connect(ss->config.pco_tst, ss->state.sock.tst_s,
                    ss->config.iut_addr) < 0)
        return RPC_ERRNO(ss->config.pco_tst);

    ss->state.tst_wait_connect = TRUE;
    return 0;
}

/**
 * Action to be done when SYN-ACK must be sent from socket on the IUT side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
iut_syn_ack_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;
    te_errno rc_aux = 0;

    rc = tsa_repair_tst_iut_conn(ss);
    if (rc != 0)
        return rc;

    wait_connectivity_changes(ss);

    /* Listener socket does not change its status when it sends SYN-ACK. */
    if (!(tsa_state_from(ss) == RPC_TCP_LISTEN &&
          tsa_state_to(ss) == RPC_TCP_SYN_RECV &&
          tsa_state_cur(ss) == RPC_TCP_LISTEN))
    {
        rc = iut_wait_change_gen(ss, MAX_CHANGE_TIMEOUT);
    }

    rc_aux = tsa_break_tst_iut_conn(ss);
    if (rc_aux != 0)
        return rc_aux;
    wait_connectivity_changes(ss);

    return rc;
}

/**
 * Action to be done when SYN-ACK must be sent from socket on the TST side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
tst_syn_ack_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;
    te_errno rc_aux = 0;

    rc = tsa_repair_iut_tst_conn(ss);
    if (rc != 0)
        return rc;

    rc = tsa_repair_tst_iut_conn(ss);
    if (rc != 0)
        return rc;

    wait_connectivity_changes(ss);

    RPC_AWAIT_ERROR(ss->config.pco_iut);
    if (rpc_connect(ss->config.pco_iut, ss->state.iut_s,
                    ss->config.tst_addr) != 0 &&
        RPC_ERRNO(ss->config.pco_iut) != RPC_EALREADY &&
        RPC_ERRNO(ss->config.pco_iut) != RPC_EINPROGRESS)
        rc = RPC_ERRNO(ss->config.pco_iut);

    ss->state.iut_wait_connect = FALSE;

    if (rc == 0)
    {
        ss->state.sock.tst_s_aux = ss->state.sock.tst_s;
        RPC_AWAIT_ERROR(ss->config.pco_tst);
        ss->state.sock.tst_s = rpc_accept(ss->config.pco_tst,
                                          ss->state.sock.tst_s,
                                          NULL, NULL);
        if (ss->state.sock.tst_s < 0)
            rc = RPC_ERRNO(ss->config.pco_tst);
    }

    rc_aux = tsa_break_iut_tst_conn(ss);
    if (rc_aux != 0)
        return rc_aux;

    rc_aux = tsa_break_tst_iut_conn(ss);
    if (rc_aux != 0)
        return rc_aux;

    wait_connectivity_changes(ss);

    return rc;
}

/**
 * Action to be done when ACK must be sent from socket on the IUT side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
iut_ack_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;
    te_errno rc_aux = 0;

    rc_aux = tsa_repair_tst_iut_conn(ss);
    if (rc_aux != 0)
        return rc_aux;
    wait_connectivity_changes(ss);

    rc = iut_wait_change_gen(ss, MAX_CHANGE_TIMEOUT);

    rc_aux = tsa_break_tst_iut_conn(ss);
    if (rc_aux != 0)
        return rc_aux;
    wait_connectivity_changes(ss);

    return rc;
}

/**
 * Action to be done when ACK must be sent from socket on the TST side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
tst_ack_sock_handler(tsa_session *ss)
{
    te_errno  rc = 0;
    te_errno  rc_aux = 0;
    int       fdflags = 0;

    rc_aux = tsa_repair_iut_tst_conn(ss);
    if (rc_aux != 0)
        return rc_aux;

    rc_aux = tsa_repair_tst_iut_conn(ss);
    if (rc_aux != 0)
        return rc_aux;

    wait_connectivity_changes(ss);

    if (ss->state.tst_wait_connect)
    {
        if (tsa_state_cur(ss) != RPC_TCP_LISTEN)
        {
            RING("Waiting for connect() call termination on "
                 "IUT side");
            RPC_AWAIT_ERROR(ss->config.pco_iut);
            if (rpc_connect(ss->config.pco_iut, ss->state.iut_s,
                            ss->config.tst_addr) != 0 &&
                RPC_ERRNO(ss->config.pco_iut) != RPC_EALREADY)
                rc = RPC_ERRNO(ss->config.pco_iut);
            ss->state.iut_wait_connect = FALSE;

            if (rc != 0)
            {
                RING("connect() call on IUT side failed: "
                     "restarting TESTER RPC server to "
                     "prevent timeout on TESTER connect() call");
                ss->state.sock.tst_s = -1;
                ss->state.sock.tst_s_aux = -1;
                ss->state.tst_wait_connect = FALSE;
                rcf_rpc_server_restart(ss->config.pco_tst);
            }
        }

        if (rc == 0)
        {
            RING("Waiting for connect() call termination on "
                 "TESTER side");
            ss->config.pco_tst->op = RCF_RPC_WAIT;
            RPC_AWAIT_ERROR(ss->config.pco_tst);
            if (rpc_connect(ss->config.pco_tst, ss->state.sock.tst_s,
                            ss->config.iut_addr) < 0)
                rc = RPC_ERRNO(ss->config.pco_tst);
            ss->state.tst_wait_connect = FALSE;

            if (tsa_state_cur(ss) == RPC_TCP_LISTEN && rc == 0)
            {
                ss->state.iut_s_aux = ss->state.iut_s;
                INFINITE_LOOP_BEGIN(loop);
                while (TRUE)
                {
                    RPC_AWAIT_ERROR(ss->config.pco_iut);
                    ss->state.iut_s =
                            rpc_accept(ss->config.pco_iut,
                                       ss->state.iut_s_aux,
                                       NULL, NULL);
                    if (ss->state.iut_s >= 0 ||
                        RPC_ERRNO(ss->config.pco_iut) !=
                                                    RPC_EAGAIN)
                        break;
                    MSLEEP(10);

                    INFINITE_LOOP_BREAK(loop, MAX_CHANGE_TIMEOUT);
                }

                if (ss->state.iut_s < 0)
                {
                    rc = RPC_ERRNO(ss->config.pco_iut);
                    if (rc == RPC_EAGAIN)
                        rc = TE_RC(TE_TAPI, TE_ETIMEDOUT);
                }
                else
                {
                    RPC_AWAIT_ERROR(ss->config.pco_iut);
                    fdflags = rpc_fcntl(ss->config.pco_iut,
                                        ss->state.iut_s,
                                        RPC_F_GETFL,
                                        0);
                    if (fdflags < 0)
                    {
                        rc = RPC_ERRNO(ss->config.pco_iut);
                    }
                    else
                    {
                        fdflags |= RPC_O_NONBLOCK;
                        RPC_AWAIT_ERROR(ss->config.pco_iut);
                        if (rpc_fcntl(ss->config.pco_iut,
                                      ss->state.iut_s,
                                      RPC_F_SETFL,
                                      fdflags) < 0)
                            rc = RPC_ERRNO(ss->config.pco_iut);

                    }

                    if (ss->state.close_listener)
                    {
                        RPC_AWAIT_ERROR(ss->config.pco_iut);
                        if (rpc_close(ss->config.pco_iut,
                                      ss->state.iut_s_aux) < 0)
                            rc = RPC_ERRNO(ss->config.pco_iut);
                        else
                            ss->state.iut_s_aux = -1;
                    }
                }
            }
        }
    }

    if (!(ss->config.flags & TSA_NO_CONNECTIVITY_CHANGE))
        MSLEEP(SLEEP_MSEC);

    rc_aux = tsa_break_iut_tst_conn(ss);
    if (rc_aux != 0)
        return rc_aux;

    rc_aux = tsa_break_tst_iut_conn(ss);
    if (rc_aux != 0)
        return rc_aux;

    wait_connectivity_changes(ss);

    if (ss->config.flags & TSA_NO_CONNECTIVITY_CHANGE ||
        ss->state.tst_type != TSA_TST_SOCKET)
    {
        rc_aux = iut_wait_change_gen(ss, MAX_CHANGE_TIMEOUT);
        if (rc_aux != 0)
            return rc_aux;
    }

    return rc;
}

/**
 * Action to be done when FIN must be sent from socket on the IUT side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
iut_fin_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;

    RPC_AWAIT_ERROR(ss->config.pco_iut);
    if (rpc_shutdown(ss->config.pco_iut, ss->state.iut_s,
                     RPC_SHUT_WR) < 0)
        rc = RPC_ERRNO(ss->config.pco_iut);

    return rc;
}

/**
 * Action to be done when FIN must be sent from socket on the TST side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
tst_fin_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;

    RPC_AWAIT_ERROR(ss->config.pco_tst);
    if (rpc_shutdown(ss->config.pco_tst, ss->state.sock.tst_s,
                     RPC_SHUT_WR) < 0)
        rc = RPC_ERRNO(ss->config.pco_tst);

    return rc;
}

/**
 * Action to be done when FIN-ACK (i.e. packet with FIN flag and
 * ACK on previously received FIN simultaneously)
 * must be sent from socket on the TST side according to TCP
 * specification. This can be performed by CSAP only.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
tst_fin_ack_sock_handler(tsa_session *ss)
{
    /* Using linux socket for this task is incorrect:
     * after repairing traffic it do not try
     * to merge previously blocked ACK and FIN
     * in one packet */

    UNUSED(ss);
    return TE_RC(TE_TAPI, TE_EFAIL);
}

/**
 * Action to be done when RST must be sent from socket on the TST side
 * according to TCP specification.
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
tst_rst_sock_handler(tsa_session *ss)
{
    rpc_tcp_state st;

    te_bool   tst_conn_repaired = FALSE;
    te_bool   iut_conn_repaired = FALSE;
    te_errno  rc = 0;
    te_errno  rc_aux = 0;


    /* We send packet with RST set having correct
     * sequence number and acknowledging all received
     * by TESTER previously */

    ss->state.state_from = tsa_state_cur(ss);

    if (tsa_state_cur(ss) != RPC_TCP_TIME_WAIT &&
        tsa_state_cur(ss) != RPC_TCP_CLOSE)
    {
        rc = tsa_repair_tst_iut_conn(ss);
        if (rc != 0)
            return rc;
        wait_connectivity_changes(ss);
        tst_conn_repaired = TRUE;

        /* If we have tst_s socket with SO_LINGER set to 0,
         * close() on it will not try to finish TCP connection
         * in a standard way but merely send RST to peer.
         * This doesn't not work if you didn't specify using of
         * SO_LINGER option for tst_s socket.
         */

         RING("Closing TESTER socket or restarting TESTER RPC: "
              "if SO_LINGER was set to 0, it should result in "
              "sending RST to previously connected IUT socket");

        if (ss->state.tst_wait_connect)
        {
            rc = rcf_rpc_server_restart(ss->config.pco_tst);
            if (rc != 0)
                goto cleanup;
        }
        else
        {
            RPC_AWAIT_ERROR(ss->config.pco_tst);
            if (rpc_close(ss->config.pco_tst,
                          ss->state.sock.tst_s) < 0)
            {
                rc = RPC_ERRNO(ss->config.pco_tst);
                goto cleanup;
            }
        }

        ss->state.sock.tst_s = -1;
        ss->state.sock.tst_s_aux = -1;
        ss->state.tst_wait_connect = FALSE;

        rc = tsa_repair_iut_tst_conn(ss);
        if (rc != 0)
            goto cleanup;
        wait_connectivity_changes(ss);
        iut_conn_repaired = TRUE;

        rc = iut_wait_change_gen(ss, MAX_CHANGE_TIMEOUT);
    }
    else
    {
        /* Unfortunately linux doesn't send RST so that it will
         * be received in TCP_TIME_WAIT state, so CSAP is used.
         */
        rc = tapi_tcp_reset_hack_catch(
                      ss->config.pco_tst->ta,
                      ss->state.sock.sid,
                      &ss->state.sock.rst_hack_c);
        if (rc != 0)
            goto cleanup;

        rc = tsa_repair_tst_iut_conn(ss);
        if (rc != 0)
            goto cleanup;
        wait_connectivity_changes(ss);
        tst_conn_repaired = TRUE;

        /*
         * We suppose that it is TCP_TIME_WAIT really,
         * and that TCP SEQN is 2 due to SIN and FIN from
         * IUT side.
         */
        rc = tapi_tcp_reset_hack_send(ss->config.pco_tst->ta,
                                      ss->state.sock.sid,
                                      &ss->state.sock.rst_hack_c,
                                      0, 2);
        if (rc != 0)
            goto cleanup;

        st = ss->state.state_to;
        /*
         * This is done because we don't want to stop waiting
         * quickly discovering that our state is TCP_CLOSE as
         * expected already as TCP_TIME_WAIT is not observable
         */
        ss->state.state_to = RPC_TCP_UNKNOWN;
        rc = iut_wait_change_gen(ss, MAX_CHANGE_TIMEOUT);
        ss->state.state_to = st;
    }

cleanup:

    if (tst_conn_repaired)
    {
        rc_aux = tsa_break_tst_iut_conn(ss);
        if (rc_aux != 0)
            return rc_aux;
    }

    if (iut_conn_repaired)
    {
        rc_aux = tsa_break_iut_tst_conn(ss);
        if (rc_aux != 0)
            return rc_aux;
    }

    if (tst_conn_repaired || iut_conn_repaired)
        wait_connectivity_changes(ss);

    return rc;
}

/**
 * Action to be done when socket on the IUT side should become listening
 *
 * @param ss    Pointer to TSA session structure
 *
 * @return Status code.
 */
static te_errno
iut_listen_sock_handler(tsa_session *ss)
{
    te_errno rc = 0;

    RPC_AWAIT_ERROR(ss->config.pco_iut);
    if (rpc_listen(ss->config.pco_iut, ss->state.iut_s,
                   TSA_BACKLOG_DEF) < 0)
        rc = RPC_ERRNO(ss->config.pco_iut);

    return rc;
}

/* See description in tapi_tcp_states_internal.h */
void
tsa_set_sock_handlers(tsa_handlers *handlers)
{
    memset(handlers, 0, sizeof(*handlers));

    handlers->iut_syn = &iut_syn_sock_handler;
    handlers->tst_syn = &tst_syn_sock_handler;
    handlers->iut_syn_ack = &iut_syn_ack_sock_handler;
    handlers->tst_syn_ack = &tst_syn_ack_sock_handler;
    handlers->iut_ack = &iut_ack_sock_handler;
    handlers->tst_ack = &tst_ack_sock_handler;
    handlers->iut_fin = &iut_fin_sock_handler;
    handlers->tst_fin = &tst_fin_sock_handler;
    handlers->tst_fin_ack = &tst_fin_ack_sock_handler;
    handlers->tst_rst = &tst_rst_sock_handler;
    handlers->iut_listen = &iut_listen_sock_handler;
}
