
#ifndef __TE_TAPI_IPSTACK_H__
#define __TE_TAPI_IPSTACK_H__

#include <assert.h>
#include <netinet/in.h>

#include "te_stdint.h"
#include "tad_common.h"
#include "asn_usr.h"


/** Structure of UDP/IPv4 datagram */
typedef struct udp4_datagram {
    struct in_addr      src_addr;
    struct in_addr      dst_addr;
    uint16_t            src_port;
    uint16_t            dst_port;
    uint16_t            payload_len;
    uint8_t            *payload;
} udp4_datagram;

/** 
 * Callback function for the tapi_snmp_walk() routine, it is called
 * for each variable in a walk subtree.
 *
 * @param pkt           Received UDP datagram. After return from this
 *                      callback memory block under this datagram will
 *                      be freed. 
 * @param userdata      Parameter, provided by the caller of
 *                      tapi_snmp_walk().
 */
typedef void (*udp4_callback)(const udp4_datagram *pkt, void *userdata);



/**
 * Convert UDP.IPv4 datagram ASN.1 value to plain C structure.
 *
 * @param pkt           ASN.1 value of type Raw-Packet.
 * @param udp_dgram     converted structure (OUT).
 *
 * @return zero on success or error code.
 */
extern int ndn_udp4_dgram_to_plain(asn_value_p pkt, 
                                   struct udp4_datagram **udp_dgram);



/**
 * Creates usual 'data.udp.ip4' CSAP on specified Test Agent and got its
 * handle.
 * 
 * @param ta            Test Agent name
 * @param sid           RCF SID
 * @param loc_addr_str  Character string with local IP address (or NULL)
 * @param rem_addr_str  Character string with remote IP address (or NULL)
 * @param loc_port      Local UDP port (may be zero)
 * @param rem_port      Remote UDP port (may be zero)
 * @param csap_id       Identifier of an SNMP CSAP (OUT)
 * 
 * @return Zero on success or error code.
 */
extern int tapi_udp4_csap_create(const char *ta_name, int sid,
                                 const char *loc_addr_str,
                                 const char *rem_addr_str,
                                 uint16_t loc_port, uint16_t rem_port,
                                 csap_handle_t *udp_csap);


/**
 * Send UDP datagram via 'data.udp.ip4' CSAP.
 * 
 * @param ta            Test Agent name.
 * @param sid           RCF SID
 * @param csap          identifier of an SNMP CSAP (OUT).
 * @param udp_dgram     UDP datagram to be sent.
 * 
 * @return zero on success or error code.
 */
extern int tapi_udp4_dgram_send(const char *ta_name, int sid,
            csap_handle_t csap, const udp4_datagram *udp_dgram);



/**
 * Start receiving of UDP datagrams via 'data.udp.ip4' CSAP, non-block
 * method.
 * 
 * @param ta            Test Agent name
 * @param sid           RCF SID
 * @param csap          Identifier of an SNMP CSAP (OUT)
 * @param udp_dgram     UDP datagram with pattern for filter
 * @param callback      Callback function, which will be call for each 
 *                      received packet
 * @param userdata      Opaque data to be passed into the callback function
 * 
 * @return Zero on success or error code.
 */
extern int tapi_udp4_dgram_start_recv(const char *ta_name,  int sid,
            csap_handle_t csap, const  udp4_datagram *udp_dgram, 
            udp4_callback callback, void *user_data);

/**
 * Receive some number of UDP datagrams via 'data.udp.ip4' CSAP, block
 * method.
 * 
 * @param ta            Test Agent name
 * @param sid           RCF SID
 * @param csap          Identifier of an SNMP CSAP (OUT)
 * @param number        Number of dgrams to be received
 * @param timeout       Total timeout for receive, in seconds
 * @param udp_dgram     UDP datagram with pattern for filter 
 * @param callback      Callback function, which will be call for each 
 *                      received packet
 * @param userdata      Opaque data to be passed into the callback function
 * 
 * @return Zero on success or error code.
 */
extern int tapi_udp4_dgram_recv(const char *ta_name, int sid,
                                csap_handle_t csap,
                                int number, int timeout,
                                const udp4_datagram *udp_dgram, 
                                udp4_callback callback, void *user_data);

/**
 * Send UDP datagram via 'data.udp.ip4' CSAP and receive response to it
 * (that is first received UDP datagram with reverse to the sent 
 * source/destination addresses and ports).
 * 
 * @param ta            Test Agent name
 * @param sid           RCF SID
 * @param csap          Identifier of an SNMP CSAP (OUT)
 * @param timeout       Timeout for receive, in seconds
 * @param dgram_sent    UDP datagram to be sent
 * @param dgram_recv    Location for received UDP datagram; memory for
 *                      payload will be allocated by TAPI and should be
 *                      freed by user
 * 
 * @return Zero on success or error code.
 */
extern int tapi_udp4_dgram_send_recv(const char *ta_name, int sid, 
                                     csap_handle_t csap,
                                     unsigned int timeout,
                                     const udp4_datagram *dgram_send,
                                     udp4_datagram *dgram_recv);



/**
 * Creates 'ip4.eth' CSAP
 *
 * @param ta_name       Test Agent name
 * @param sid           RCF SID
 * @param loc_addr      Local IP address in network order (or NULL)
 * @param rem_addr      Remote IP address in network order (or NULL)
 * @param ip4_csap      Location for the IPv4 CSAP handle (OUT)
 *
 * @return  Status of the operation
 */
extern int tapi_ip4_eth_csap_create(const char *ta_name, int sid, 
                         const char *eth_dev,
                         const uint8_t *loc_addr, const uint8_t *rem_addr,
                         csap_handle_t *ip4_csap);


/**
 * Start receiving of IPv4 packets 'ip4.eth' CSAP, non-block
 * method. It cannot report received packets, only count them.
 *
 * Started receiving process may be controlled by rcf_ta_trrecv_get, 
 * rcf_ta_trrecv_wait, and rcf_ta_trrecv_stop methods.
 * 
 * @param ta            Test Agent name
 * @param sid           RCF SID
 * @param csap          Identifier of CSAP
 * @param src_addr      Source IP address in network order (or NULL)
 * @param dst_addr      Destination IP address in network order (or NULL)
 * @param timeout       Timeout of operation (in milliseconds, 
 *                      zero for infinitive)
 * @param num           nubmer of packets to be caugth
 * 
 * @return Zero on success or error code.
 */
extern int tapi_ip4_eth_recv_start(const char *ta_name, int sid, 
                        csap_handle_t csap,
                        const uint8_t *src_addr, const uint8_t *dst_addr,
                        unsigned int timeout, int num);






/**
 * Creates 'tcp.ip4.eth' CSAP
 *
 * @param ta_name       Test Agent name
 * @param sid           RCF SID
 * @param loc_addr      Local IP address in network order (or NULL)
 * @param rem_addr      Remote IP address in network order (or NULL)
 * @param loc_port      Local TCP port in HOST byte order 
 * @param rem_port      Remote TCP port in HOST byte order 
 * @param tcp_csap      Location for the IPv4 CSAP handle (OUT)
 *
 * @return  Status of the operation
 */
extern int tapi_tcp_ip4_eth_csap_create(const char *ta_name, int sid, 
                         const char *eth_dev,
                         const uint8_t *loc_addr, const uint8_t *rem_addr,
                         uint16_t loc_port, uint16_t rem_port,
                         csap_handle_t *tcp_csap);


/**
 * Start receiving of IPv4 packets 'tcp.ip4.eth' CSAP, non-block
 * method. It cannot report received packets, only count them.
 *
 * Started receiving process may be controlled by rcf_ta_trrecv_get, 
 * rcf_ta_trrecv_wait, and rcf_ta_trrecv_stop methods.
 * 
 * @param ta            Test Agent name
 * @param sid           RCF SID
 * @param csap          Identifier of CSAP
 * @param src_addr      Source IP address in network order (or NULL)
 * @param dst_addr      Destination IP address in network order (or NULL)
 * @param loc_port      Local TCP port in HOST byte order 
 * @param rem_port      Remote TCP port in HOST byte order 
 * @param timeout       Timeout of operation (in milliseconds, 
 *                      zero for infinitive)
 * @param num           nubmer of packets to be caugth
 * 
 * @return Zero on success or error code.
 */
extern int tapi_tcp_ip4_eth_recv_start(const char *ta_name, int sid, 
                        csap_handle_t csap,
                        const uint8_t *src_addr, const uint8_t *dst_addr,
                        uint16_t loc_port, uint16_t rem_port,
                        unsigned int timeout, int num);


/**
 * Prepare ASN Pattern-Unit value for 'tcp.ip4.eth' CSAP.
 * 
 * @param src_addr      Source IP address in network order (or NULL)
 * @param dst_addr      Destination IP address in network order (or NULL)
 * @param src_port      Source TCP port in HOST byte order 
 * @param dst_port      Destination TCP port in HOST byte order 
 * @param result_value  Location for pointer to new ASN value
 * 
 * @return Zero on success or error code.
 */
extern int tapi_tcp_ip4_pattern_unit(
                        const uint8_t *src_addr, const uint8_t *dst_addr,
                        uint16_t src_port, uint16_t dst_port,
                        asn_value **result_value);

#endif /* !__TE_TAPI_IPSTACK_H__ */
