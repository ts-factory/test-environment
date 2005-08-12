/** @file 
 * @brief Test Environment
 * Network Communication Library Tests - Test Agent side
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
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
 * Author: Pavel A. Bolokhov <Pavel.Bolokhov@oktetlabs.ru>
 * 
 * @(#) $Id$
 */
#include "config.h"

#include <stdio.h>
#include <pthread.h>

#include "synch.h"
#include "connection.h"
#include "workshop.h"
#include "debug.h"

#include "te_errno.h"
#include "comm_agent.h"

/* Defines whether to send or not initial random messages */
#define SEND_RANDOM             1

/* The default total size of input and output buffers */
#define TOTAL_BUFFER_LENGTH     1000

/* The default size of the command generated by this test */
#define COMMAND_SIZE            ((TOTAL_BUFFER_LENGTH) / 4)

/* The default size of the attachment generated by this test */
#define ATTACHMENT_SIZE         (((TOTAL_BUFFER_LENGTH) * 2) / 4)

/* The default size of the whole message */
#define MESSAGE_SIZE            ((COMMAND_SIZE) + (ATTACHMENT_SIZE))

/* The default declared size of the input buffer */
#define DECLARED_BUFFER_LENGTH  ((TOTAL_BUFFER_LENGTH) / 2)

#define TEST_BUFFER_SANITY()                                            \
    do {                                                          \
        if (DECLARED_BUFFER_LENGTH * 2 <= MESSAGE_SIZE ||                \
           MESSAGE_SIZE > TOTAL_BUFFER_LENGTH ||                       \
           COMMAND_SIZE >= DECLARED_BUFFER_LENGTH)                       \
        {                                                          \
               fprintf(stderr,                                            \
                  "TESTS_BUFFER_SANITY: check sizes of the buffers\n"); \
            exit(2);                                                   \
        }                                                          \
    } while (0)

/* thread handle of the remote station thread */
pthread_t remote_thread;

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

#if (SEND_RANDOM == 1)
    remote_presend_random();
#endif /* SEND_RANDOM == 1 */

    alloc_output_buffer(TOTAL_BUFFER_LENGTH, TOTAL_BUFFER_LENGTH);

    /* synchronize at this point */
    remote_synch(10);

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

#if (SEND_RANDOM == 1)
    local_receive_random();
#endif /* SEND_RANDOM == 1 */

    /* synchronize at this point */
    local_synch(10);

    alloc_input_buffer(TOTAL_BUFFER_LENGTH, DECLARED_BUFFER_LENGTH);

    /* now call the _wait() function two times */
    len = declared_input_buffer_length;
    ptr = input_buffer;
    rc = rcf_comm_agent_wait(handle, ptr, &len, NULL);
    if (len != MESSAGE_SIZE)
    {
       fprintf(stderr, "ERROR: rcf_comm_agent_wait() did not "
              "return the length of the whole message (%d) while "
              "returning TE_EPENDING\n", MESSAGE_SIZE);
       exit(3);
    }
    len = declared_input_buffer_length;
    if (TE_RC_GET_ERROR(rc) != TE_EPENDING)
    {
       fprintf(stderr, "ERROR: the first call of rcf_comm_agent_wait() "
              "returned %d instead of TE_EPENDING(%d)\n", rc, TE_EPENDING);
       exit(3);
    }
    ptr += len;
    len = total_input_buffer_length - len;
    rc = rcf_comm_agent_wait(handle, ptr, &len, NULL);
    DEBUG("got len = %d on the second call\n", len);
    if (rc != 0)
    {
       fprintf(stderr, "ERROR: the second call of rcf_comm_agent_wait() "
              "returned %d instead of zero\n", rc);
       exit(3);
    }
    declared_input_buffer_length = (ptr + len) - input_buffer;

    /* synchronize at this point */
    local_synch(20);
    ZERO_ADJUST_INPUT_BUFFER(input_buffer, declared_input_buffer_length);
    VERIFY_BUFFERS();

    /* close the connection */
    local_connection_close();
}

/** @page test_rcf_net_agent_wait09 test_rcf_net_agent_wait() double call, big attachment
 *
 * @descr The remote station issues a message consisting of a command and
 * a big attachment so that the input buffer of @b rcf_comm_agent_wait() 
 * function can room the whole command and only a part of the attachment,
 * but the total message must fit in two input buffers. The first call
 * of the function returns TE_EPENDING, the second call succeeds. The two
 * calls must make up the whole consistent message sent by the remote
 * station.
 *
 * @post Once successful, the test is repeated sending several messages 
 * with different combinations of sizes of the commands and their attachments
 * prior to doing the main check.
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
