/** @file
 * @brief RPC for Agent job control
 *
 * Definition of RPC structures and functions for Agent job control
 *
 * Copyright (C) 2019 OKTET Labs.
 *
 * @author Igor Romanov <Igor.Romanov@oktetlabs.ru>
 */

/* job_create() */
struct tarpc_job_create_in {
    struct tarpc_in_arg common;

    string spawner<>;
    string tool<>;
    tarpc_string argv<>;
    tarpc_string env<>;
};

struct tarpc_job_create_out {
    struct tarpc_out_arg common;

    tarpc_int               retval;
    tarpc_uint job_id;
};

/* job_start */
struct tarpc_job_start_in {
    struct tarpc_in_arg common;

    tarpc_uint job_id;
};

struct tarpc_job_start_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* job_allocate_channels */
struct tarpc_job_allocate_channels_in {
    struct tarpc_in_arg common;

    tarpc_uint job_id;
    tarpc_bool input_channels;
    tarpc_uint n_channels;
    tarpc_uint channels<>;
};

struct tarpc_job_allocate_channels_out {
    struct tarpc_out_arg common;

    tarpc_uint channels<>;
    tarpc_int retval;
};

/* job_attach_filter */
struct tarpc_job_attach_filter_in {
    struct tarpc_in_arg common;

    tarpc_uint channels<>;
    tarpc_bool readable;
    tarpc_uint log_level;
    string filter_name<>;
};

struct tarpc_job_attach_filter_out {
    struct tarpc_out_arg common;

    tarpc_uint filter;
    tarpc_int retval;
};

/* job_receive */
struct tarpc_job_receive_in {
    struct tarpc_in_arg common;

    tarpc_uint filters<>;
    tarpc_int timeout_ms;
};

struct tarpc_job_buffer {
    tarpc_uint channel;
    tarpc_uint filter;
    tarpc_bool eos;
    char data<>;
    tarpc_size_t dropped;
};

struct tarpc_job_receive_out {
    struct tarpc_out_arg common;

    tarpc_job_buffer buffer;
    tarpc_int retval;
};

/* job_poll */
struct tarpc_job_poll_in {
    struct tarpc_in_arg common;

    tarpc_uint channels<>;
    tarpc_int timeout_ms;
};

struct tarpc_job_poll_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* job_kill */
struct tarpc_job_kill_in {
    struct tarpc_in_arg common;

    tarpc_uint job_id;
    tarpc_signum signo;
};

struct tarpc_job_kill_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* job_wait */
struct tarpc_job_wait_in {
    struct tarpc_in_arg common;

    tarpc_uint job_id;
    tarpc_int timeout_ms;
};

enum tarpc_job_status_type {
    TARPC_JOB_STATUS_EXITED,
    TARPC_JOB_STATUS_SIGNALED,
    TARPC_JOB_STATUS_UNKNOWN
};

struct tarpc_job_status {
    tarpc_job_status_type type;
    tarpc_int value;
};

struct tarpc_job_wait_out {
    struct tarpc_out_arg common;

    tarpc_job_status status;
    tarpc_int retval;
};

/* job_destroy */
struct tarpc_job_destroy_in {
    struct tarpc_in_arg common;

    tarpc_uint job_id;
};

struct tarpc_job_destroy_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

program job
{
    version ver0
    {
        RPC_DEF(job_create)
        RPC_DEF(job_start)
        RPC_DEF(job_allocate_channels)
        RPC_DEF(job_attach_filter)
        RPC_DEF(job_receive)
        RPC_DEF(job_poll)
        RPC_DEF(job_kill)
        RPC_DEF(job_wait)
        RPC_DEF(job_destroy)
    } = 1;
} = 2;