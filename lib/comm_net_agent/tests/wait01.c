/** @file 
 * @brief Test Environment
 * Network Communication Library Tests - Test Agent side
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights served.
 *
 * 
 *
 * Author: Pavel A. Bolokhov <Pavel.Bolokhov@oktetlabs.ru>
 * 
 * @(#) $Id$
 */
#include "config.h"

#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "synch.h"
#include "connection.h"
#include "workshop.h"
#include "debug.h"

#include "te_errno.h"
#include "comm_agent.h"

/* The default total size of input and output buffers */
#define TOTAL_BUFFER_LENGTH     1000

/* The default size of the command generated by this test */
#define COMMAND_SIZE            ((TOTAL_BUFFER_LENGTH / 4) * 2)

/* The default size of the attachment generated by this test */
#define ATTACHMENT_SIZE         (TOTAL_BUFFER_LENGTH / 4)

/* The default size of the whole message */
#define MESSAGE_SIZE            ((COMMAND_SIZE) + (ATTACHMENT_SIZE))

/* The default declared size of the input buffer */
#define DECLARED_BUFFER_LENGTH  TOTAL_BUFFER_LENGTH

/* 
 * The delay (in SECONDS) that the remote station will hold in order to 
 * assure that the rcf_comm_agent_wait() does not return until a message
 * is sent
 */
#define REMOTE_SEND_DELAY       5 /* seconds */

#define TEST_BUFFER_SANITY()                                            \
    do {                                                          \
        if (MESSAGE_SIZE > TOTAL_BUFFER_LENGTH)                              \
        {                                                          \
               fprintf(stderr,                                            \
                  "TESTS_BUFFER_SANITY: check sizes of the buffers\n"); \
            exit(2);                                                   \
        }                                                          \
    } while (0)

/* thread handle of the remote station thread */
pthread_t remote_thread;

int       sleep_over = 0;  /* indicates that the remote station has
                           already passed its sleep period */

/**
 * The main routine of the remote station thread.
 *
 * @return   n/a
 */
void *
remote_station_proc(void *arg)
{
    DEBUG("\t\t\tRemote Station Thread started\n");

    /* initialize the connection */
    remote_connection_init();

    alloc_output_buffer(TOTAL_BUFFER_LENGTH, TOTAL_BUFFER_LENGTH);

    /* synchronize at this point */
    remote_synch(10);

    /* then we sleep */
    fprintf(stderr, "\t\t\tremote_station_proc: sleeping %d seconds...\n",
           REMOTE_SEND_DELAY);
#if (HAVE_NANOSLEEP == 1)
    {
       struct timespec t;
       
       t.tv_nsec = 0;
       t.tv_sec = REMOTE_SEND_DELAY;
       if (nanosleep(&t, NULL) < 0)
       {
           char err_buf[BUFSIZ];

           strerror_r(errno, err_buf, sizeof(err_buf));
           fprintf(stderr, "remote_station_proc: nanosleep() failed: %s\n", 
                  err_buf);
           exit(1);
       }
    }
#else
    sleep(REMOTE_SEND_DELAY);
#endif /* HAVE_NANOSLEEP == 1 */    

    sleep_over = 1;    /* raise the flag to let the local station know
                       that we're thru with our *sleep() */

    /* now generate a command */
    generate_command(output_buffer, COMMAND_SIZE, ATTACHMENT_SIZE);
    DEBUG("\t\t\tremote_station_proc: generated a message of %d bytes "
         "command and %d bytes attachment\n", COMMAND_SIZE, ATTACHMENT_SIZE);
    declared_output_buffer_length = MESSAGE_SIZE;
#if 0
    DEBUG("\t\t\tremote_station_proc: generated a message of %d bytes\n",
         MESSAGE_SIZE);
    DEBUG("\t\t\tHere follows the command:\n");
    DEBUG("\t\t\t%s\n", output_buffer);
    DEBUG("\t\t\tEnd of command\n");
#endif

    /* transport the message to the local station */
    remote_transfer_buffer();

    /* synchronize after the transfer */
    remote_synch(20);

    /* close the connection */
    remote_connection_close();
}

/**
 * The main routine of the local station thread.
 *
 * @return   n/a
 */
void *
local_station_proc(void *arg)
{
    int   rc;
    int   len;
    char *ptr;

    DEBUG("Local Station Thread started\n");

    /* initialize the connection */
    local_connection_init();

    alloc_input_buffer(TOTAL_BUFFER_LENGTH, DECLARED_BUFFER_LENGTH);

    /* synchronize at this point */
    local_synch(10);

    /* now call the _wait() function one time */
    len = declared_input_buffer_length;
    ptr = input_buffer;
    rc = rcf_comm_agent_wait(handle, ptr, &len, NULL);
    
    /* check that the remote station really has sent us something */
    if (!sleep_over)
    {
       fprintf(stderr, "ERROR: the call of rcf_comm_agent_wait() returned "
              "before the remote station had sent it anything\n");
       exit(3);
    }
    if (rc != 0)
    {
       fprintf(stderr, "ERROR: the call of rcf_comm_agent_wait() "
              "returned %d instead of zero\n", rc);
       exit(3);
    }

    /* synchronize at this point */
    local_synch(20);

    /* close the connection */
    local_connection_close();
}

/** @page test_rcf_net_agent_wait01 rcf_comm_agent_wait() message expectation check
 *
 * @descr The local station calls @b rcf_comm_agent_wait() and waits for 
 * some period of time. Then the remote station issues a incoming message. 
 * The function must not return before that moment. No message integrity 
 * check is supposed in this test.
 * 
 * @author Pavel A. Bolokhov <Pavel.Bolokhov@oktetlabs.ru>
 *
 * @return Test result
 * @retval 0            Test succeeded
 * @retval positive     Test failed
 *
 */
int 
main(int argc, char *argv[])
{
    int rc;

    /* initialize thread synchronization features */
    barrier_init();

    /* check test precondition sanity */
    TEST_BUFFER_SANITY();

    /* launch the remote station thread */
    rc = pthread_create(&remote_thread, /* attr */ NULL, 
                     remote_station_proc, /* arg */ NULL);
    if (rc != 0)
    {           
       char err_buf[BUFSIZ];

       strerror_r(errno, err_buf, sizeof(err_buf));
       fprintf(stderr, "main: pthread_create() failed: %s\n", err_buf);
       exit(1);
    }

    /* launch the local station in the current thread */
    local_station_proc(NULL);

    /* indicate that the test has passed successfully */
    PRINT_TEST_OK();

    /* shutdown thread synchronization features */
    barrier_close();

    return 0;
}
