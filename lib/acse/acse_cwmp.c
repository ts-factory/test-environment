/** @file
 * @brief ACSE CWMP Dispatcher
 *
 * ACS Emulator support
 *
 *
 * Copyright (C) 2009-2010 Test Environment authors (see file AUTHORS
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
 *
 * @author Edward Makarov <Edward.Makarov@oktetlabs.ru>
 * @author Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * $Id$
 */

#include "te_config.h"

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <stddef.h>
#include<string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_queue.h"
#include "te_defs.h"
#include "logger_api.h"
#include "acse_internal.h"

#include "httpda.h"

#include "acse_soapH.h"

SOAP_NMAC struct Namespace namespaces[] =
{
    {"SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/",
                "http://www.w3.org/*/soap-envelope", NULL},
    {"SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/",
                "http://www.w3.org/*/soap-encoding", NULL},
    {"xsi", "http://www.w3.org/2001/XMLSchema-instance",
                "http://www.w3.org/*/XMLSchema-instance", NULL},
    {"xsd", "http://www.w3.org/2001/XMLSchema",
                "http://www.w3.org/*/XMLSchema", NULL},
    {"cwmp", "urn:dslforum-org:cwmp-1-1",
                "urn:dslforum-org:cwmp-1-*", NULL},
    {NULL, NULL, NULL, NULL}
};
/** Single REALM for Digest Auth. which we support. */
const char *authrealm = "tr-069";



SOAP_FMAC5 int SOAP_FMAC6
__cwmp__GetRPCMethods(struct soap *soap, 
            struct _cwmp__GetRPCMethods *cwmp__GetRPCMethods, 
            struct _cwmp__GetRPCMethodsResponse
                                  *cwmp__GetRPCMethodsResponse)
{
    UNUSED(soap);
    UNUSED(cwmp__GetRPCMethods);
    UNUSED(cwmp__GetRPCMethodsResponse);

    return 0;
}

SOAP_FMAC5 int SOAP_FMAC6 
__cwmp__Inform(struct soap *soap,
               struct _cwmp__Inform *cwmp__Inform,
               struct _cwmp__InformResponse *cwmp__InformResponse)
{
    cwmp_session_t *session = (cwmp_session_t *)soap->user;

    if(session == NULL)
    {
        ERROR("%s(): NULL user pointer in soap!", __FUNCTION__);
        /* TODO: correct processing */
        return 500; 
    }

    VERB("%s called. Header is %p, enc style is '%s', inform Dev is '%s'",
            __FUNCTION__, soap->header, soap->encodingStyle,
            cwmp__Inform->DeviceId->OUI);
    soap->keep_alive = 1; 

    if (soap->authrealm && soap->userid)
    {
        printf("%s(): Digest auth, authrealm: '%s', userid '%s'\n", 
                __FUNCTION__, soap->authrealm, soap->userid);
        /* TODO: lookup passwd by userid */
        if (!strcmp(soap->authrealm, authrealm) &&
            !strcmp(soap->userid, cpe_record->username))
        {

#if 0
          char *passwd = "z7cD7CTDA1DrQKUb";
#else
          char *passwd = "passwd";
#endif

          if (http_da_verify_post(soap, passwd))
          {
            printf("%s(): Digest auth failed RRRRRRRRRRRR\n", __FUNCTION__);
              soap_dealloc(soap, soap->authrealm);
              soap->authrealm = soap_strdup(soap, authrealm);
              soap->keep_alive = 1; 
            return 401;
          }
          printf("%s(): AUTH passed\n", __FUNCTION__);
        }
        printf("%s(): Should be fault\n", __FUNCTION__);
    }
    else
    {
printf("%s(): Digest auth failed 2\n", __FUNCTION__);
      soap->authrealm = soap_strdup(soap, authrealm);
      soap->keep_alive = 1; 
#if 1
        soap->error = SOAP_OK;
        soap_serializeheader(soap);
        soap_begin_count(soap);
        soap_end_count(soap);
        soap_response(soap, 401);
        // soap_envelope_end_out(soap);
        soap_end_send(soap);
      soap->keep_alive = 1; 
      return SOAP_STOP;
#else
      return 401;
#endif
    }


    if (soap->header)
        printf("hold_request in Header: %d\n",
                (int)soap->header->cwmp__HoldRequests.__item);
    else
    {
        soap->header = soap_malloc(soap, sizeof(struct SOAP_ENV__Header));
    }

    if (soap->encodingStyle)
    {
        soap->encodingStyle = NULL;
    }

    

    cwmp__InformResponse->MaxEnvelopes = 255;
    soap->header->cwmp__HoldRequests.__item = 1;
    soap->header->cwmp__HoldRequests.SOAP_ENV__mustUnderstand = "1";
    soap->keep_alive = 1; 

    return SOAP_OK;
}


SOAP_FMAC5 int SOAP_FMAC6 
__cwmp__TransferComplete(struct soap *soap, 
                    struct _cwmp__TransferComplete *cwmp__TransferComplete,
                    struct _cwmp__TransferCompleteResponse
                            *cwmp__TransferCompleteResponse)
{
    UNUSED(soap);
    UNUSED(cwmp__TransferComplete);
    UNUSED(cwmp__TransferCompleteResponse);

    return 0;
}

SOAP_FMAC5 int SOAP_FMAC6 
__cwmp__AutonomousTransferComplete(struct soap *soap, 
            struct _cwmp__AutonomousTransferComplete
                            *cwmp__AutonomousTransferComplete,
            struct _cwmp__AutonomousTransferCompleteResponse 
                            *cwmp__AutonomousTransferCompleteResponse)
{
    UNUSED(soap);
    UNUSED(cwmp__AutonomousTransferComplete);
    UNUSED(cwmp__AutonomousTransferCompleteResponse);

    return 0;
}

SOAP_FMAC5 int SOAP_FMAC6
__cwmp__RequestDownload(struct soap *soap, 
        struct _cwmp__RequestDownload *cwmp__RequestDownload, 
        struct _cwmp__RequestDownloadResponse
                                     *cwmp__RequestDownloadResponse)
{
    UNUSED(soap);
    UNUSED(cwmp__RequestDownload);
    UNUSED(cwmp__RequestDownloadResponse);

    return 0;
}

SOAP_FMAC5 int SOAP_FMAC6 
__cwmp__Kicked(struct soap *soap, 
                struct _cwmp__Kicked *cwmp__Kicked, 
                struct _cwmp__KickedResponse *cwmp__KickedResponse)
{
    UNUSED(soap);
    UNUSED(cwmp__Kicked);
    UNUSED(cwmp__KickedResponse);

    return 0;
}



int
cwmp_SendConnectionRequest(const char *endpoint,
                           const char *username, 
                           const char *password)
{
    struct soap *soap = soap_new();
    struct http_da_info info;

    soap_register_plugin(soap, http_da);

    soap_begin(soap);
    soap_begin_count(soap);
    soap_end_count(soap);

    if (soap_connect_command(soap, SOAP_GET, endpoint, "")
        || soap_end_send(soap))
    {
        soap_print_fault(soap, stderr); 
        return 1;
    } 
    if (soap_begin_recv(soap))
    {
        if (soap->error == 401)
        {
            char local_realm[100];
            // soap_end_recv(soap);
            printf("UUUUUUUU first attempt failed, AUTH, realm: '%s'\n",
                  soap->authrealm);
            strcpy(local_realm, soap->authrealm);
            /* save userid and passwd for basic or digest authentication */
            http_da_save(soap, &info, local_realm, username, password);
#if 1
            soap_begin_count(soap);
            soap_end_count(soap);
#endif
            info.qop = soap_strdup(soap, "auth");
            http_da_restore(soap, &info);
            soap_connect_command(soap, SOAP_GET, endpoint, "");
            soap_end_send(soap);
            soap_begin_recv(soap);
            soap_end_recv(soap);
            printf("UUUUUUUUU second attempt result %d\n", soap->error);
            if (soap->error != SOAP_OK)
                soap_print_fault(soap, stderr);
        }
        else 
            soap_print_fault(soap, stderr); 
        return 1;
    } 

    soap_done(soap);
    soap_end(soap); 
    return 0;
}

te_errno
cwmp_before_poll(void *data, struct pollfd *pfd)
{
    cwmp_session_t *cwmp_sess = data;

    if (cwmp_sess == NULL ||
        cwmp_sess->state == CWMP_NOP)
    {
        return TE_EINVAL;
    }

    pfd->fd = cwmp_sess->m_soap.socket;
    pfd->events = POLLIN;
    pfd->revents = 0;

    return 0;
}

te_errno
cwmp_after_poll(void *data, struct pollfd *pfd)
{
    cwmp_session_t *cwmp_sess = data;

    if (!(pfd->revents & POLLIN))
        return 0;

    switch(cwmp_sess->state)
    {
        case CWMP_NOP:
            ERROR("CWMP after poll, unexpected state %d\n",
                    (int)cwmp_sess->state);
            return TE_EINVAL;

        case CWMP_LISTEN:
        case CWMP_WAIT_AUTH:
        case CWMP_SERVE:
            /* Now, after poll() on soap socket, it should not block */
            soap_serve(&cwmp_sess->m_soap);
            break;

        case CWMP_WAIT_RESPONSE:
            /* TODO */
            break;
    }

    return 0;
}

te_errno
cwmp_destroy(void *data)
{
    cwmp_session_t *cwmp_sess = data;

    return cwmp_close_session(cwmp_sess);
}


/**
 * Check wheather accepted TCP connection is related to 
 * particular ACS; if it is, start processing of CWMP session.
 * This procedure does not wait any data in TCP connection and 
 * does not perform regular read from it. 
 *
 * @return      0 if connection accepted by this ACS;
 *              TE_ECONNREFUSED if connection NOT accepted by this ACS;
 *              other error status on some error.
 */
te_errno
cwmp_accept_cpe_connection(acs_t *acs, int socket)
{
    channel_t *channel;
    cpe_t *cpe_item;
    cwmp_session_t *cwmp_sess;

    /* TODO: real check, now accept all, if any CPE registered. */

    if (LIST_EMPTY(&acs->cpe_list))
    {
        RING("%s: conn refused: no CPE for this ACS.", __FUNCTION__);
        return TE_ECONNREFUSED;
    }


    cwmp_sess = cwmp_new_session(socket);

    cwmp_sess->acs_owner = acs;
    cwmp_sess->state = CWMP_LISTEN; /* Auth not started at all. */
    acse_add_channel(cwmp_sess->channel);

    RING("%s: success", __FUNCTION__);

    return 0;
}


cwmp_session_t *
cwmp_new_session(int socket)
{
    cwmp_session_t *new_sess = malloc(sizeof(*new_sess));
    channel_t      *channel  = malloc(sizeof(*channel));

    if (new_sess == NULL || channel == NULL)
        return NULL;

    soap_init(&(new_sess->m_soap));

    new_sess->state = CWMP_NOP;
    new_sess->acs_owner = NULL;
    new_sess->cpe_owner = NULL;
    new_sess->channel = channel;
    new_sess->m_soap.user = new_sess;
    new_sess->m_soap.socket = socket;

    soap_imode(&new_sess->m_soap, SOAP_IO_KEEPALIVE);
    soap_omode(&new_sess->m_soap, SOAP_IO_KEEPALIVE);

    soap_register_plugin(&new_sess->m_soap, http_da); 

    new_sess->m_soap.max_keep_alive = 10;

    channel->data = new_sess;
    channel->before_poll = cwmp_before_poll;
    channel->after_poll = cwmp_after_poll;
    channel->destroy = cwmp_destroy;

    return new_sess;
}

te_errno 
cwmp_close_session(cwmp_session_t *sess)
{
    /* TODO: investigate, what else should be closed.. */
    soap_destroy(&(sess->m_soap));
    soap_end(&(sess->m_soap));
    soap_done(&(sess->m_soap));

    free(sess);

    return 0;
}


te_errno
acse_enable_acs(acs_t *acs)
{
    struct sockaddr_in *sin;

    if (acs == NULL || acs->port == 0)
        return TE_EINVAL;
    sin = malloc(sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = INADDR_ANY;
    sin->sin_port = htons(acs->port);

    acs->addr_listen = SA(sin);
    acs->addr_len = sizeof(struct sockaddr_in);

    conn_register_acs(acs);
}



