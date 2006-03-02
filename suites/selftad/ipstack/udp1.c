/** @file
 * @brief Test Environment
 *
 * Simple UDP CSAP test
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
 * @author Konstantin Abramenko <konst@oktetlabs.ru>
 * 
 * $Id$
 */

#define TE_TEST_NAME    "ipstack/udp1"

#define TE_LOG_LEVEL 0xff

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


#include "te_stdint.h"
#include "te_errno.h"
#include "rcf_api.h"
#include "logger_api.h"

#include "tapi_test.h"
#include "tapi_ip.h"
#include "tapi_udp.h"
#include "tapi_tcp.h"

#include "ndn_eth.h"
#include "ndn_ipstack.h"

void
udp_handler(char *fn, void *p)
{ 
    int rc, s_parsed;
    asn_value *packet, *eth_header;

    UNUSED(p);

    VERB("ETH handler, file: %s\n", fn);

    rc = asn_parse_dvalue_in_file(fn, ndn_raw_packet, &packet, &s_parsed);

    if (rc == 0)
    {
        ndn_eth_header_plain eh;
        VERB("parse file OK!\n");

        eth_header = asn_read_indexed(packet, 0, "pdus");
        rc = ndn_eth_packet_to_plain(eth_header, &eh);
        if (rc)
            VERB("eth_packet to plain fail: %x\n", rc);
        else
        {
            int i; 
            VERB("dst - %02x", eh.dst_addr[0]);
            for (i = 1; i < 6; i++)
                VERB(":%02x", eh.dst_addr[i]);

            VERB("\nsrc - %02x", eh.src_addr[0]);
            for (i = 1; i < 6; i++)
                VERB (":%02x", eh.src_addr[i]);
            VERB("\ntype - %04x\n", eh.len_type);
        } 
    }
    else
        VERB("parse file failed, rc = %x, symbol %d\n", rc, s_parsed); 

}

int
main(int argc, char *argv[])
{
    int  sid;
    char ta[32];
    char *agt_a = ta;
    char *agt_b;
    size_t  len = sizeof(ta);

    char path[1000];
    int  path_prefix;

    char *pattern_file;


    TEST_START; 
    TEST_GET_STRING_PARAM(pattern_file);
    
    if ((rc = rcf_get_ta_list(ta, &len)) != 0)
        TEST_FAIL("rcf_get_ta_list failed: %r", rc);

    INFO("Found first TA: %s; len %d", ta, len);

    agt_a = ta;
    if (strlen(ta) + 1 >= len) 
        TEST_FAIL("There is no second Test Agent");

    agt_b = ta + strlen(ta) + 1;

    INFO("Found second TA: %s", agt_b, len);

    /* Session */
    {
        if (rcf_ta_create_session(ta, &sid) != 0)
        {
            ERROR("rcf_ta_create_session failed\n");
            return 1;
        }
        INFO("Test: Created session: %d\n", sid); 
    }

    do {
        int csap;
        int num;
        int rc_mod;

        char *te_suites = getenv("TE_INSTALL_SUITE");

        if (te_suites)
            INFO("te_suites: %s\n", te_suites);

        INFO("let's create UDP data csap \n"); 
        rc = tapi_udp4_csap_create(ta, sid, NULL, "0.0.0.0", 
                                    5678, 0, &csap); 
        INFO("csap_create rc: %d, csap id %d\n", rc, csap); 
        if ((rc_mod = TE_RC_GET_MODULE(rc)) != 0)
        {
            INFO ("rc from module %d is %r\n", 
                        rc_mod, TE_RC_GET_ERROR(rc));
        } 
        if (rc) break;

        strcpy(path, te_suites);
        strcat(path, "/selftad/ipstack/");
        strcat(path, pattern_file);

        INFO("prepared path: %s", path); 

#if 1
        rc = rcf_ta_trrecv_start(ta, sid, csap, path, 0, 0,
                                 RCF_TRRECV_PACKETS);
        INFO("trrecv_start: %r \n", rc);
        if (rc) break;

#if 1
        sleep(1);
        INFO ("try to get\n");
        rc = rcf_ta_trrecv_get(ta, sid, csap, udp_handler, NULL, &num);
        INFO("trrecv_get: %r num: %d\n", rc, num);
        if (rc) break;

#endif
        num = 1;
        INFO ("sleep %d secs before stop\n", num);
        sleep (num);

        INFO ("try to stop\n");
        rc = rcf_ta_trrecv_stop(ta, sid, csap, udp_handler, NULL, &num);
        INFO("trrecv_stop: %r num: %d\n", rc, num);

#endif
        rc = rcf_ta_csap_destroy(ta, sid, csap);
        INFO("csap %d destroy: %r ", csap, rc); 

    } while(0);

    if (rc)
        TEST_FAIL("Failed, rc %X", rc);

    TEST_SUCCESS;

cleanup:
    TEST_END;
}
