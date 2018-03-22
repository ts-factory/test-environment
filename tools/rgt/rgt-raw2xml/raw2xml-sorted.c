/** @file 
 * @brief Test Environment: RGT - log dumping utility
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights served.
 *
 * 
 *
 * @author Nikolai Kondrashov <Nikolai.Kondrashov@oktetlabs.ru>
 *
 * $Id$
 */

#include "te_config.h"

#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/sysinfo.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_raw_log.h"
#include "logger_defs.h"

#include "lua_rgt_msg.h"

#define ERROR(_fmt, _args...) fprintf(stderr, _fmt "\n", ##_args)

#define ERROR_CLEANUP(_fmt, _args...) \
    do {                                \
        ERROR(_fmt, ##_args);           \
        goto cleanup;                   \
    } while (0)

#define ERROR_USAGE_RETURN(_fmt, _args...) \
    do {                                                \
        ERROR(_fmt, ##_args);                           \
        usage(stderr, program_invocation_short_name);   \
        return 1;                                       \
    } while (0)

#define INPUT_BUF_SIZE  16384

#define SCRAP_MIN_SIZE  16384

static void    *scrap_buf   = NULL;
static size_t   scrap_size  = 0;

te_bool
scrap_grow(size_t size)
{
    size_t  new_scrap_size  = scrap_size;
    void   *new_scrap_buf;

    if (new_scrap_size < SCRAP_MIN_SIZE)
        new_scrap_size = SCRAP_MIN_SIZE;

    while (new_scrap_size < size)
        new_scrap_size += new_scrap_size / 2;

    new_scrap_buf = realloc(scrap_buf, new_scrap_size);
    if (new_scrap_buf == NULL)
        return FALSE;

    scrap_buf = new_scrap_buf;
    scrap_size = new_scrap_size;

    return TRUE;
}

#define SCRAP_CHECK_GROW(size) \
    (((size) <= scrap_size) ? TRUE : scrap_grow(size))

void
scrap_clear(void)
{
    free(scrap_buf);
    scrap_size = 0;
}


/** Message reading result code */
typedef enum read_message_rc {
    READ_MESSAGE_RC_ERR         = -1,   /**< A reading error occurred or
                                             unexpected EOF was reached */
    READ_MESSAGE_RC_EOF         = 0,    /**< The EOF was encountered instead
                                             of the message */
    READ_MESSAGE_RC_OK          = 1,    /**< The message was read
                                              successfully */
} read_message_rc;


/**
 * Read message variable length fields from a stream and place them into the
 * scrap buffer.
 *
 * @param input The stream to read from.
 * @param msg   The message to output field references to.
 *
 * @return Result code.
 */
static read_message_rc
read_message_flds(FILE *input, rgt_msg *msg)
{
    static size_t       entity_pos;
    static size_t       user_pos;
    static size_t       fmt_pos;
    static size_t       args_pos;
    static size_t      *fld_pos[]   = {&entity_pos, &user_pos,
                                       &fmt_pos, &args_pos};
    size_t              fld_idx     = 0;

    size_t              size        = 0;
    te_log_nfl          len;
    te_log_nfl          buf_len;
    size_t              fld_size;
    size_t              remainder;
    size_t              new_size;
    rgt_msg_fld        *fld;

    do {
        /* Read and convert the field length */
        if (fread(&len, sizeof(len), 1, input) != 1)
            return READ_MESSAGE_RC_ERR;
#if SIZEOF_TE_LOG_NFL == 4
        len = ntohl(len);
#elif SIZEOF_TE_LOG_NFL == 2
        len = ntohs(len);
#elif SIZEOF_TE_LOG_NFL != 1
#error Unexpected value of SIZEOF_TE_LOG_NFL
#endif

        /* If it is an optional field and it is the terminating length */
        if (fld_idx >= (sizeof(fld_pos) / sizeof(*fld_pos) - 1) &&
            len == TE_LOG_RAW_EOR_LEN)
            buf_len = 0;
        else
            buf_len = len;

        /* Calculate field size, with alignment */
        fld_size = sizeof(*fld) + buf_len;
        remainder = fld_size % sizeof(*fld);
        if (remainder != 0)
            fld_size += sizeof(*fld) - remainder;

        /* Calculate new size and grow the buffer */
        new_size = size + fld_size;
        if (!SCRAP_CHECK_GROW(new_size))
            return READ_MESSAGE_RC_ERR;

        /* If it is a directly referenced field */
        if (fld_idx < sizeof(fld_pos) / sizeof(*fld_pos))
            /* Output the field position */
            *fld_pos[fld_idx] = size;

        /* Calculate field pointer */
        fld = (rgt_msg_fld *)((char *)scrap_buf + size);

        /* Fill-in and read-in the field */
        fld->size = fld_size;
        fld->len = len;
        if (buf_len > 0 && fread(fld->buf, buf_len, 1, input) != 1)
            return READ_MESSAGE_RC_ERR;

        /* Seal the field */
        size = new_size;
    } while (
         /* While it is a mandatory field or not a terminating length */
         fld_idx++ < (sizeof(fld_pos) / sizeof(*fld_pos) - 1) ||
         len != TE_LOG_RAW_EOR_LEN);

    /*
     * Output field pointers
     */
    msg->entity = (rgt_msg_fld *)((char *)scrap_buf + entity_pos);
    msg->user   = (rgt_msg_fld *)((char *)scrap_buf + user_pos);
    msg->fmt    = (rgt_msg_fld *)((char *)scrap_buf + fmt_pos);
    msg->args   = (rgt_msg_fld *)((char *)scrap_buf + args_pos);

    return READ_MESSAGE_RC_OK;
}


/**
 * Read a message from a stream and place variable length fields into the
 * scrap buffer.
 *
 * @param input The stream to read from.
 * @param msg   The message to output to.
 *
 * @return Result code.
 */
static read_message_rc
read_message(FILE *input, rgt_msg *msg)
{
    te_log_version      version;
    te_log_ts_sec       ts_secs;
    te_log_ts_usec      ts_usecs;
    te_log_level        level;
    te_log_id           id;

    /* Read and verify log message version */
    if (fread(&version, sizeof(version), 1, input) != 1)
        return feof(input) ? READ_MESSAGE_RC_EOF : READ_MESSAGE_RC_ERR;
    if (version != TE_LOG_VERSION)
    {
        errno = EINVAL;
        ERROR("Unknown log message version %u", version);
        return READ_MESSAGE_RC_ERR;
    }

    /*
     * Transfer timestamp
     */
    /* Read the timestamp fields */
    if (fread(&ts_secs, sizeof(ts_secs), 1, input) != 1 ||
        fread(&ts_usecs, sizeof(ts_usecs), 1, input) != 1)
        return READ_MESSAGE_RC_ERR;
    msg->ts_secs = ntohl(ts_secs);
    msg->ts_usecs = ntohl(ts_usecs);

    /*
     * Transfer level
     */
    if (fread(&level, sizeof(level), 1, input) != 1)
        return READ_MESSAGE_RC_ERR;
#if SIZEOF_TE_LOG_LEVEL == 4
    msg->level = ntohl(level);
#elif SIZEOF_TE_LOG_LEVEL == 2
    msg->level = ntohs(level);
#elif SIZEOF_TE_LOG_LEVEL != 1
#error Unexpected value of SIZEOF_TE_LOG_LEVEL
#endif

    /*
     * Transfer ID
     */
    if (fread(&id, sizeof(id), 1, input) != 1)
        return READ_MESSAGE_RC_ERR;
#if SIZEOF_TE_LOG_ID == 4
    msg->id = ntohl(id);
#elif SIZEOF_TE_LOG_ID == 2
    msg->id = ntohs(id);
#elif SIZEOF_TE_LOG_ID != 1
#error Unexpected value of SIZEOF_TE_LOG_ID
#endif

    /*
     * Transfer variable-length fields
     */
    return read_message_flds(input, msg);
}


/* Taken from lua.c */
static int
l_traceback(lua_State *L)
{
    /* 'message' not a string? */
    if (!lua_isstring(L, 1))
        /* Keep it intact */
        return 1;

    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return 1;
    }

    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 2);
        return 1;
    }

    /* Pass error message */
    lua_pushvalue(L, 1);
    /* Skip this function and traceback */
    lua_pushinteger(L, 2);
    /* Call debug.traceback */
    lua_call(L, 2, 1);

    return 1;
}


#define LUA_PCALL(_nargs, _nresults) \
    do {                                                            \
        if (lua_pcall(L, _nargs, _nresults, traceback_idx) != 0)    \
            ERROR_CLEANUP("%s", lua_tostring(L, -1));               \
    } while (0)

#define LUA_REQUIRE(_mod) \
    do {                                \
        lua_getglobal(L, "require");    \
        lua_pushliteral(L, _mod);       \
        LUA_PCALL(1, 1);                \
    } while (0)


static te_bool
run_input_and_output(FILE *input,
                     lua_State *L, int traceback_idx,
                     int sink_idx, int sink_put_idx)
{
    te_bool             result      = FALSE;
    off_t               offset;
    off_t               msg_num     = 1;
    te_log_ts_sec       last_secs   = 0;
    te_log_ts_usec      last_usecs  = 0;
    rgt_msg             msg;
    int                 msg_idx;
    read_message_rc     read_rc;

    /* Create message reference userdata */
    lua_rgt_msg_wrap(L, &msg);
    msg_idx = lua_gettop(L);

    while (TRUE)
    {
        /* Retrieve current offset */
        offset = ftello(input);

        /* Read a message */
        read_rc = read_message(input, &msg);
        if (read_rc < READ_MESSAGE_RC_OK)
        {
            if (read_rc == READ_MESSAGE_RC_EOF)
                break;
            else
            {
                int read_errno = errno;

                ERROR_CLEANUP("Failed reading input message #%lld "
                              "(starting at %lld) at %lld: %s",
                              (long long int)msg_num,
                              (long long int)offset,
                              (long long int)ftello(input),
                              feof(input)
                                  ? "unexpected EOF"
                                  : strerror(read_errno));
            }
        }

        /* Check message order */
        if (msg.ts_secs < last_secs ||
            (msg.ts_secs == last_secs && msg.ts_usecs < last_usecs))
            ERROR_CLEANUP("Message #%lld (starting at %lld) "
                          "is out of order",
                          (long long int)msg_num,
                          (long long int)offset);
        last_secs = msg.ts_secs;
        last_usecs = msg.ts_usecs;

        /* Copy "put" sink instance method to the top */
        lua_pushvalue(L, sink_put_idx);
        /* Copy sink instance to the top */
        lua_pushvalue(L, sink_idx);
        /* Copy message to the top */
        lua_pushvalue(L, msg_idx);

        /* Call "put" sink instance method to feed the message */
        if (lua_pcall(L, 2, 0, traceback_idx) != 0)
            ERROR_CLEANUP("Failed to output message #%lld "
                          "starting at %lld:\n%s",
                          (long long int)msg_num,
                          (long long int)offset,
                          lua_tostring(L, -1));

        msg_num++;
    }

    result = TRUE;

cleanup:

    /* Remove the message reference userdata, since it becomes invalid */
    lua_remove(L, msg_idx);

    /* Free scrap buffer used to hold message variable-length fields */
    scrap_clear();

    return result;
}


static te_bool
run_input(FILE *input, const char *output_name,
          const char *tmp_dir, unsigned long max_mem)
{
    te_bool     result          = FALSE;
    lua_State  *L               = NULL;
    int         traceback_idx;
    int         sink_idx;
    int         sink_put_idx;
#ifdef RGT_WITH_LUA_PROFILER
    int         profiler_idx;
#endif

    /*
     * Setup Lua
     */
    /* Create Lua instance */
    L = luaL_newstate();
    if (L == NULL)
        ERROR_CLEANUP("Failed to create Lua state");
    /* Load standard libraries */
    luaL_openlibs(L);

    /* Push traceback function */
    lua_pushcfunction(L, l_traceback);
    traceback_idx = lua_gettop(L);

    /* Require "strict" module */
    lua_getglobal(L, "require");
    lua_pushliteral(L, "strict");
    LUA_PCALL(1, 0);

#ifdef RGT_WITH_LUA_PROFILER
    /* Require "profiler" module */
    LUA_REQUIRE("profiler");
    profiler_idx = lua_gettop(L);
#endif

    /* Require rgt.msg */
    lua_getglobal(L, "require");
    lua_pushliteral(L, LUA_RGT_MSG_NAME);
    LUA_PCALL(1, 0);

    /*
     * Create sink instance
     */
    /* Call rgt.sink(max_mem) to create sink instance */
    LUA_REQUIRE("rgt.sink");
    lua_pushstring(L, tmp_dir);
    lua_pushnumber(L, (lua_Number)max_mem * 1024 * 1024);
    LUA_PCALL(2, 1);
    sink_idx = lua_gettop(L);

    /*
     * Supply sink instance with the output file
     */
    lua_getfield(L, sink_idx, "take_file");
    lua_pushvalue(L, sink_idx);

    /* Open/push output file */
    /* Get "io" table */
    lua_getglobal(L, "io");
    if (output_name[0] == '-' && output_name[1] == '\0')
        lua_getfield(L, -1, "stdout");
    else
    {
        lua_getfield(L, -1, "open");
        lua_pushstring(L, output_name);
        lua_pushliteral(L, "w");
        LUA_PCALL(2, 1);
    }
    /* Remove "io" table */
    lua_remove(L, -2);

    /* Call "take_file" instance method to supply the sink with the file */
    LUA_PCALL(2, 0);

#ifdef RGT_WITH_LUA_PROFILER
    /* Start profiling */
    lua_getfield(L, profiler_idx, "start");
    LUA_PCALL(0, 0);
#endif

    /*
     * Start sink output
     */
    lua_getfield(L, sink_idx, "start");
    lua_pushvalue(L, sink_idx);
    LUA_PCALL(1, 0);

    /*
     * Run with input file and Lua state
     */
    /* Retrieve sink instance "put" method */
    lua_getfield(L, sink_idx, "put");
    sink_put_idx = lua_gettop(L);

    if (!run_input_and_output(input, L, traceback_idx,
                              sink_idx, sink_put_idx))
        goto cleanup;

    /*
     * Finish sink output
     */
    lua_getfield(L, sink_idx, "finish");
    lua_pushvalue(L, sink_idx);
    LUA_PCALL(1, 0);

#ifdef RGT_WITH_LUA_PROFILER
    /* Stop profiling */
    lua_getfield(L, profiler_idx, "stop");
    LUA_PCALL(0, 0);
#endif

    /*
     * Take the output file from the sink
     */
    lua_getfield(L, sink_idx, "yield_file");
    lua_pushvalue(L, sink_idx);
    LUA_PCALL(1, 1);

    /*
     * Close the output
     */
    lua_getfield(L, -1, "close");
    lua_pushvalue(L, -2);
    LUA_PCALL(1, 0);

    result = TRUE;

cleanup:

    if (L != NULL)
        lua_close(L);

    return result;
}


static int
run(const char *input_name, const char *output_name,
    const char *tmp_dir, unsigned long max_mem)
{
    int                 result          = 1;
    FILE               *input           = NULL;
    void               *input_buf       = NULL;
    te_log_version      version;

    /*
     * Setup input
     */
    /* Open input */
    if (input_name[0] == '-' && input_name[1] == '\0')
        input = stdin;
    else
    {
        input = fopen(input_name, "r");
        if (input == NULL)
            ERROR_CLEANUP("Failed to open \"%s\": %s",
                          input_name, strerror(errno));
    }

    /* Set input buffer */
    input_buf = malloc(INPUT_BUF_SIZE);
    setvbuf(input, input_buf, _IOFBF, INPUT_BUF_SIZE);

    /* Read and verify log file version */
    if (fread(&version, sizeof(version), 1, input) != 1)
        ERROR_CLEANUP("Failed to read log file version: %s",
                      feof(input) ? "unexpected EOF" : strerror(errno));
    if (version != 1)
        ERROR_CLEANUP("Unsupported log file version %hhu", version);

    /*
     * Run with input file
     */
    if (!run_input(input, output_name, tmp_dir, max_mem))
        goto cleanup;

    /*
     * Close the input
     */
    fclose(input);
    input = NULL;

    /* Success */
    result = 0;

cleanup:

    if (input != NULL)
        fclose(input);
    free(input_buf);

    return result;
}


static int
usage(FILE *stream, const char *progname)
{
    return 
        fprintf(
            stream, 
            "Usage: %s [OPTION]... [INPUT_RAW [OUTPUT_XML]]\n"
            "Convert a sorted raw TE log to XML.\n"
            "\n"
            "With no INPUT_RAW, or when INPUT_RAW is -, "
            "read standard input.\n"
            "With no OUTPUT_XML, or when OUTPUT_XML is -, "
            "write standard output.\n"
            "\n"
            "Options:\n"
            "  -h, --help           this help message\n"
            "  -t, --tmp-dir=DIR    directory for temporary files\n"
            "                       (default: /tmp)\n"
            "  -m, --max-mem=MB     maximum memory to use for output (MB)\n"
            "                       (default: RAM / 4, maximum: 4096)\n"
            "\n",
            progname);
}


typedef enum opt_val {
    OPT_VAL_HELP        = 'h',
    OPT_VAL_TMP_DIR     = 't',
    OPT_VAL_MAX_MEM     = 'm',
} opt_val;


int
main(int argc, char * const argv[])
{
    static const struct option  long_opt_list[] = {
        {.name      = "help",
         .has_arg   = no_argument,
         .flag      = NULL,
         .val       = OPT_VAL_HELP},
        {.name      = "tmp-dir",
         .has_arg   = required_argument,
         .flag      = NULL,
         .val       = OPT_VAL_TMP_DIR},
        {.name      = "max-mem",
         .has_arg   = required_argument,
         .flag      = NULL,
         .val       = OPT_VAL_MAX_MEM},
        {.name      = NULL,
         .has_arg   = 0,
         .flag      = NULL,
         .val       = 0}
    };
    static const char          *short_opt_list = "ht:m:";

    int             c;
    const char     *input_name  = "-";
    const char     *output_name = "-";
    struct sysinfo  si;
    const char     *tmp_dir     = "/tmp";
    unsigned long   max_mem;

    /*
     * Set default max_mem
     */
    sysinfo(&si);
    max_mem = (unsigned long)
                (((uint64_t)(si.totalram) * si.mem_unit) / (4*1024*1024));
    if (max_mem > 4096)
        max_mem = 4096;

    /*
     * Read command line arguments
     */
    while ((c = getopt_long(argc, argv,
                            short_opt_list, long_opt_list, NULL)) >= 0)
    {
        switch (c)
        {
            case OPT_VAL_HELP:
                usage(stdout, program_invocation_short_name);
                return 0;
                break;
            case OPT_VAL_TMP_DIR:
                tmp_dir = optarg;
                break;
            case OPT_VAL_MAX_MEM:
                {
                    const char *end;

                    errno = 0;
                    max_mem = strtoul(optarg, (char **)&end, 0);
                    for (; isspace(*end); end++);
                    if (errno != 0 || *end != '\0' || max_mem > 4096)
                        ERROR_USAGE_RETURN("Invalid maximum memory option value");
                }
                break;
            case '?':
                usage(stderr, program_invocation_short_name);
                return 1;
                break;
        }
    }

    if (optind < argc)
    {
        input_name  = argv[optind++];
        if (optind < argc)
        {
            output_name = argv[optind++];
            if (optind < argc)
                ERROR_USAGE_RETURN("Too many arguments");
        }
    }

    /*
     * Verify command line arguments
     */
    if (*tmp_dir == '\0')
        ERROR_USAGE_RETURN("Empty temporary directory path");
    if (*input_name == '\0')
        ERROR_USAGE_RETURN("Empty input file name");
    if (*output_name == '\0')
        ERROR_USAGE_RETURN("Empty output file name");

    /*
     * Run
     */
    return run(input_name, output_name, tmp_dir, max_mem);
}


