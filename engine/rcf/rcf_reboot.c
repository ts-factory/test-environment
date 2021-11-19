/** @file
 * @brief RCF routines for TA reboot
 *
 *
 * Copyright (C) 2021 OKTET Labs. All rights reserved.
 *
 * @author Artemii Morozov <Artemii.Morozov@oktetlabs.ru>
 */
#include "te_config.h"

#include "rcf.h"
#include "te_str.h"

#include "logger_api.h"
#include "logger_ten.h"

static te_bool
is_timed_out(time_t timestart, double timeout)
{
    time_t t = time(NULL);

    return difftime(t, timestart) > timeout;
}

void
rcf_ta_reboot_init_ctx(ta *agent)
{
    agent->reboot_ctx.state = TA_REBOOT_STATE_IDLE;
    agent->reboot_ctx.is_agent_reboot_msg_sent = FALSE;
    agent->reboot_ctx.is_answer_recv = FALSE;
}

static const char *
ta_reboot_type2str(ta_reboot_type type)
{
    switch (type)
    {
        case TA_REBOOT_TYPE_AGENT:
            return "restart the TA process";

        default:
            return "<unknown>";
    }
}

static void
log_reboot_state(ta *agent, ta_reboot_state state)
{
    switch (state)
    {
        case TA_REBOOT_STATE_IDLE:
            RING("Agent '%s' in normal state", agent->name);
            break;

        case TA_REBOOT_STATE_LOG_FLUSH:
            RING("%s: agent '%s' is waiting for the logs to be flushed",
                 ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            break;

        case TA_REBOOT_STATE_WAITING:
            RING("%s: sending a message requesting a reboot to TA '%s'",
                 ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            break;

        case TA_REBOOT_STATE_WAITING_ACK:
            RING("%s: waiting for a response to a message about restarting ",
                 ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            break;

        case TA_REBOOT_STATE_REBOOTING:
            RING("%s: waiting for a reboot TA '%s'",
                 ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            break;
    }
}

/* See description in rcf.h */
void
rcf_set_ta_reboot_state(ta *agent, ta_reboot_state state)
{
    log_reboot_state(agent, state);

    agent->reboot_ctx.state = state;
    agent->reboot_ctx.reboot_timestamp = time(NULL);
}

te_bool
rcf_ta_reboot_before_req(ta *agent, usrreq *req)
{
    /*
     * In the LOG_FLUSH state an agent can accept only one GET_LOG command
     */
    if (agent->reboot_ctx.state == TA_REBOOT_STATE_LOG_FLUSH &&
        req->message->opcode != RCFOP_GET_LOG)
    {
        WARN("The agent is waiting for reboot");
        return FALSE;
    }

    /*
     * If the agent in the REBOOTING state, it cannot accept requests.
     * If the agent in the WAITING state, then it can only
     * accept a reboot request.
     */
    if (agent->reboot_ctx.state == TA_REBOOT_STATE_REBOOTING ||
        (agent->reboot_ctx.state == TA_REBOOT_STATE_WAITING &&
         req->message->opcode != RCFOP_REBOOT))
    {
        if (!agent->reboot_ctx.is_agent_reboot_msg_sent)
        {
            ERROR("Agent `%s` in the reboot state", agent->name);
            agent->reboot_ctx.is_agent_reboot_msg_sent = TRUE;
        }

        return FALSE;
    }

    return TRUE;
}

te_bool
rcf_ta_reboot_on_req_reply(ta *agent, rcf_op_t opcode)
{
    if (agent->reboot_ctx.state == TA_REBOOT_STATE_LOG_FLUSH &&
        opcode == RCFOP_GET_LOG)
    {
        /*
         * If RCF is waiting for logs from an agent to reboot that agent,
         * there is no need to push the next waiting request and so on.
         * This will make the reboot state machine.
         */
        agent->reboot_ctx.is_answer_recv = TRUE;
        return TRUE;
    }

    return FALSE;
}

te_bool
rcf_ta_reboot_on_ta_dead(ta *agent, usrreq *req)
{
    if (agent->flags & TA_REBOOTABLE)
    {
        WARN("TA '%s' is dead, try to reboot...", agent->name);
        rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_LOG_FLUSH);
        agent->reboot_ctx.requested_type = TA_REBOOT_TYPE_AGENT;
        agent->reboot_ctx.current_type = TA_REBOOT_TYPE_AGENT;
        agent->reboot_ctx.req = req;
        return TRUE;
    }

    return FALSE;
}

/**
 * @c TA_REBOOT_STATE_LOG_FLUSH handler.
 *
 * If time to waiting the log flush is expired then force set the next
 * reboot state for the @p agent.
 * If the logs are flushed, respond to all pending and sending requests
 * and set the next reboot state for the @p agent.
 *
 * @param agent Test Agent structure
 *
 * @return Status code
 */
static te_errno
log_flush_state_handler(ta *agent)
{
    if (agent->reboot_ctx.is_answer_recv)
    {
        usrreq *req;

        rcf_answer_all_requests(&(agent->waiting), TE_ETAREBOOTING);
        rcf_answer_all_requests(&(agent->pending), TE_ETAREBOOTING);

        /*
         * If the request with id 0 was sent earlier
         * then we need to try to wait for a response.
         */
        req = rcf_find_user_request(&(agent->sent), 0);
        if (req != NULL)
            return 0;

        agent->conn_locked = FALSE;

        if (agent->reboot_ctx.current_type == TA_REBOOT_TYPE_AGENT)
            rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_REBOOTING);
        else
            rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_WAITING);

        agent->reboot_ctx.is_answer_recv = FALSE;
        return 0;
    }

    if (is_timed_out(agent->reboot_ctx.reboot_timestamp,
                     RCF_LOG_FLUSHED_TIMEOUT))
    {
        rcf_answer_all_requests(&(agent->waiting), TE_ETAREBOOTING);
        rcf_answer_all_requests(&(agent->pending), TE_ETAREBOOTING);
        rcf_answer_all_requests(&(agent->sent), TE_ETAREBOOTING);

        if (agent->reboot_ctx.current_type == TA_REBOOT_TYPE_AGENT)
            rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_REBOOTING);
        else
            rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_WAITING);

        agent->reboot_ctx.is_answer_recv = FALSE;
    }

    return 0;
}

/**
 * @c TA_REBOOT_STATE_WAITING handler.
 *
 * @note When restarting the TA process @c TA_REBOOT_STATE_WAITING in
 *       this case is empty.
 *
 * @param agent Test Agent structure
 *
 * @return Status code
 */
static te_errno
waiting_state_handler(ta *agent)
{
    te_errno rc = 0;

    switch (agent->reboot_ctx.current_type)
    {
        case TA_REBOOT_TYPE_AGENT:
            break;

        default:
            ERROR("Unsupported reboot type %d",
                  agent->reboot_ctx.current_type);
            return TE_EINVAL;
    }

    return rc;
}

/**
 * @c TA_REBOOT_STATE_WAITING_ACK handeler.
 *
 * @note When restarting the TA process @c TA_REBOOT_STATE_WAITING_ACK in
 *       this case is empty.
 *
 * @param agent Test Agent structure
 *
 * @return Status code
 */
static te_errno
waiting_ack_state_handler(ta *agent)
{
    te_errno rc = 0;

    switch (agent->reboot_ctx.current_type)
    {
        case TA_REBOOT_TYPE_AGENT:
            break;

        default:
            ERROR("Unsupported reboot type %d",
                  agent->reboot_ctx.current_type);
            return TE_EINVAL;
    }

    return rc;
}

/**
 * Try to soft shutdown the agent
 *
 * @param agent Test Agent
 */
static void
try_soft_shutdown(ta *agent)
{
    char cmd[RCF_MAX_LEN];
    te_errno rc;
    time_t t = time(NULL);

    if (agent->flags & TA_DEAD)
    {
        WARN("Agent is '%s' is dead. Soft shutdown failed", agent->name);
        return;
    }

    TE_SPRINTF(cmd, "SID %d %s", ++agent->sid, "shutdown");
    rc = (agent->m.transmit)(agent->handle,
                             cmd, strlen(cmd) + 1);
    if (rc != 0)
    {
        WARN("Soft shutdown of TA '%s' failed", agent->name);
        agent->flags |= TA_DEAD;
        return;
    }

    /* TODO: This should be moved to a separate function */
    while (!is_timed_out(t, RCF_SHUTDOWN_TIMEOUT))
    {
        struct timeval tv  = tv0;
        fd_set         set = set0;

        select(FD_SETSIZE, &set, NULL, NULL, &tv);

        if ((agent->m.is_ready)(agent->handle))
        {
            char    answer[16];
            char   *ba;
            size_t  len = sizeof(cmd);

            if ((agent->m.receive)(agent->handle, cmd,
                                &len, &ba) != 0)
            {
                continue;
            }

            TE_SPRINTF(answer, "SID %d 0", agent->sid);

            if (strcmp(cmd, answer) != 0)
                continue;

            INFO("Test Agent '%s' is down", agent->name);
            agent->flags |= TA_DOWN;
            (agent->m.close)(agent->handle, &set0);
            break;
        }
    }

    if (~agent->flags & TA_DOWN)
    {
        WARN("Soft shutdown of TA '%s' failed", agent->name);
        agent->flags |= TA_DEAD;
    }
}

/**
 * @c TA_REBOOT_STATE_REBOOTING for @c TA_REBOOT_TYPE_AGENT
 *
 * Restart TA process.
 *
 * @param agent Test Agent structure
 *
 * @return Status code
 */
static te_errno
rebooting_state_agent_reboot_handler(ta *agent)
{
    te_errno rc;

    try_soft_shutdown(agent);

    rc = (agent->m.finish)(agent->handle, NULL);
    if (rc != 0)
    {
        WARN("Cannot reboot TA '%s': finish failed %r", agent->name, rc);
        agent->reboot_ctx.error = rc;
        return 0;
    }

    agent->handle = NULL;

    RING("Test Agent '%s' is stopped", agent->name);
    rc = rcf_init_agent(agent);
    if (rc != 0)
    {
        ERROR("Cannot reboot TA '%s'", agent->name);
        agent->reboot_ctx.error = rc;
        return 0;
    }

    rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_IDLE);
    return 0;
}

/**
 * @c TA_REBOOT_STATE_REBOOTING handler.
 *
 * For @c TA_REBOOT_TYPE_AGENT:
 *     Call the appropriate handler
 *
 * @param agent Test Agent structure
 *
 * @return Status code
 */
static te_errno
rebooting_state_handler(ta *agent)
{
    te_errno rc = 0;

    switch (agent->reboot_ctx.current_type)
    {
        case TA_REBOOT_TYPE_AGENT:
            rc = rebooting_state_agent_reboot_handler(agent);
            break;

        default:
            ERROR("Unsupported reboot type %d",
                  agent->reboot_ctx.current_type);
            return TE_EINVAL;
    }

    return rc;
}

/* See description in rcf.h */
void
rcf_ta_reboot_state_handler(ta *agent)
{
    te_errno error = 0;

    switch (agent->reboot_ctx.state)
    {
        case TA_REBOOT_STATE_IDLE:
            /* Do nothing if the agent in the normal state */
            return;

        case TA_REBOOT_STATE_LOG_FLUSH:
            error = log_flush_state_handler(agent);
            break;

        case TA_REBOOT_STATE_WAITING:
            error = waiting_state_handler(agent);
            break;

        case TA_REBOOT_STATE_WAITING_ACK:
            error = waiting_ack_state_handler(agent);
            break;

        case TA_REBOOT_STATE_REBOOTING:
            error = rebooting_state_handler(agent);
            break;

        default:
            ERROR("Unsupported reboot type %d",
                  agent->reboot_ctx.current_type);
            error = TE_EINVAL;
    }

    if (error != 0)
    {
        ERROR("%s for '%s' is failed",
              ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
        agent->reboot_ctx.req->message->error = TE_RC(TE_RCF, TE_EFAIL);
        rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_IDLE);
        rcf_answer_user_request(agent->reboot_ctx.req);
        ta_checker.req = NULL;
        rcf_set_ta_unrecoverable(agent);
        agent->handle = NULL;
        return;
    }

    if (agent->reboot_ctx.error != 0)
    {
        if (agent->reboot_ctx.requested_type > agent->reboot_ctx.current_type)
        {
            WARN("%s for '%s' is failed",
                 ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            RING("Use %s instead of %s for '%s'",
                 ta_reboot_type2str(agent->reboot_ctx.current_type + 1),
                 ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            agent->reboot_ctx.current_type++;
            rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_WAITING);
            agent->reboot_ctx.error = 0;
            agent->reboot_ctx.is_agent_reboot_msg_sent = FALSE;
        }
        else
        {
            ERROR("%s for '%s' is failed",
              ta_reboot_type2str(agent->reboot_ctx.current_type), agent->name);
            agent->reboot_ctx.req->message->error = TE_RC(TE_RCF, TE_EFAIL);
            rcf_set_ta_reboot_state(agent, TA_REBOOT_STATE_IDLE);
            rcf_answer_user_request(agent->reboot_ctx.req);
            rcf_set_ta_unrecoverable(agent);
            agent->handle = NULL;
            ta_checker.req = NULL;
        }
    }
    else if (agent->reboot_ctx.state == TA_REBOOT_STATE_IDLE)
    {
        RING("TA '%s' has successfully rebooted", agent->name);
        rcf_answer_user_request(agent->reboot_ctx.req);
        ta_checker.req = NULL;
    }

    return;
}