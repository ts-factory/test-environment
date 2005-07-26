/** @file
 * @brief Test Environment: 
 *
 * Traffic Application Domain Command Handler
 * Ethernet CSAP, stack-related callbacks.
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
 * Author: Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * @(#) $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/ioctl.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "tad_iscsi_impl.h"

#define TE_LGR_USER     "TAD iSCSI stack"
#include "logger_ta.h"

 
/**
 * Callback for read data from media of iSCSI CSAP. 
 *
 * @param csap_id       identifier of CSAP.
 * @param timeout       timeout of waiting for data in microseconds.
 * @param buf           buffer for read data.
 * @param buf_len       length of available buffer.
 *
 * @return 
 *      quantity of read octets, or -1 if error occured, 0 if timeout expired. 
 */ 
int 
iscsi_read_cb (csap_p csap_descr, int timeout, char *buf, size_t buf_len)
{
    iscsi_csap_specific_data_t *spec_data;
    
    UNUSED(timeout);
    UNUSED(buf);
    UNUSED(buf_len);

    if (csap_descr == NULL)
    {
        return -1;
    }
    
    spec_data = (iscsi_csap_specific_data_t *)
        csap_descr->layers[0].specific_data; 

    return 0;
}

/**
 * Callback for write data to media of iSCSI CSAP. 
 *
 * @param csap_id       identifier of CSAP.
 * @param buf           buffer with data to be written.
 * @param buf_len       length of data in buffer.
 *
 * @return 
 *      quantity of written octets, or -1 if error occured. 
 */ 
int 
iscsi_write_cb (csap_p csap_descr, char *buf, size_t buf_len)
{
    iscsi_csap_specific_data_t * spec_data;
    int layer;    
    int rc = 0;
    
    if (csap_descr == NULL)
    {
        return -1;
    }
    
    spec_data = (iscsi_csap_specific_data_t *)
        csap_descr->layers[layer].specific_data; 

    UNUSED(buf);
    UNUSED(buf_len);
    return rc;
}

/**
 * Callback for write data to media of iSCSI CSAP and read
 *  data from media just after write, to get answer to sent request. 
 *
 * @param csap_id       identifier of CSAP.
 * @param timeout       timeout of waiting for data.
 * @param w_buf         buffer with data to be written.
 * @param w_buf_len     length of data in w_buf.
 * @param r_buf         buffer for data to be read.
 * @param r_buf_len     available length r_buf.
 *
 * @return 
 *      quantity of read octets, or -1 if error occured, 0 if timeout expired. 
 */ 
int 
iscsi_write_read_cb (csap_p csap_descr, int timeout,
                   char *w_buf, size_t w_buf_len,
                   char *r_buf, size_t r_buf_len)
{
    int rc; 
    
    rc = iscsi_write_cb(csap_descr, w_buf, w_buf_len);
    
    if (rc == -1)  
        return rc;
    else 
        return iscsi_read_cb(csap_descr, timeout, r_buf, r_buf_len);;
}

/**
 * Callback for init iscsi CSAP layer  if single in stack.
 *
 * @param csap_id       identifier of CSAP.
 * @param csap_nds      asn_value with CSAP init parameters
 * @param layer         numeric index of layer in CSAP type to be processed. 
 *                      Layers are counted from zero, from up to down.
 *
 * @return zero on success or error code.
 */ 
int 
iscsi_single_init_cb(int csap_id, const asn_value *csap_nds, int layer)
{
    csap_p   csap_descr;          /**< csap description        */

    iscsi_csap_specific_data_t *   iscsi_spec_data; 

    int             mode, rc;
    size_t          len;

    if (csap_nds == NULL)
        return ETEWRONGPTR;

    if ((csap_descr = csap_find (csap_id)) == NULL)
        return ETADCSAPNOTEX;


    len = sizeof(mode);
    rc = asn_read_value_field(csap_nds, &mode, &len, "0.mode");
    if (rc)
        return rc; /* If this field is not set, then CSAP cannot process */ 

    iscsi_spec_data = malloc (sizeof(iscsi_csap_specific_data_t));
    
    if (iscsi_spec_data == NULL)
    {
        return ENOMEM;
    }
    


    csap_descr->layers[layer].specific_data = iscsi_spec_data;
    csap_descr->layers[layer].get_param_cb = iscsi_get_param_cb;

    csap_descr->read_cb         = iscsi_read_cb;
    csap_descr->write_cb        = iscsi_write_cb;
    csap_descr->write_read_cb   = iscsi_write_read_cb;
    csap_descr->read_write_layer = layer; 
    csap_descr->timeout          = 500000;
    
    return 0;
}

/**
 * Callback for destroy iSCSI CSAP layer  if single in stack.
 *      This callback should free all undeground media resources used by 
 *      this layer and all memory used for layer-specific data and pointed 
 *      in respective structure in 'layer-data' in CSAP instance struct. 
 *
 * @param csap_id       identifier of CSAP.
 * @param layer         numeric index of layer in CSAP type to be processed. 
 *                      Layers are counted from zero, from up to down.
 *
 * @return zero on success or error code.
 */ 
int 
iscsi_single_destroy_cb (int csap_id, int layer)
{
    csap_p csap_descr = csap_find(csap_id);

    iscsi_csap_specific_data_t * spec_data = 
        (iscsi_csap_specific_data_t *)
        csap_descr->layers[layer].specific_data; 
     
    UNUSED(spec_data);

    return 0;
}


