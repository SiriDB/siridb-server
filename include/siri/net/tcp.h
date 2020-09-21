/*
 * tcp.h - TCP support.
 */
#ifndef SIRINET_TCP_H_
#define SIRINET_TCP_H_

#include <uv.h>

/* Warning: do not change the order! (maps to dns_req_family_map) */
enum
{
    IP_SUPPORT_ALL,
    IP_SUPPORT_IPV4ONLY,
    IP_SUPPORT_IPV6ONLY
};

/* dns_req_family_map maps to IP_SUPPORT values defined in socket.h */
static int DNS_REQ_FAMILY_MAP[3] = {AF_UNSPEC, AF_INET, AF_INET6};

const char * sirinet_tcp_ip_support_str(uint8_t ip_support);
char * sirinet_tcp_name(uv_tcp_t * client);
int sirinet_extract_addr_port(
        char * s,
        char * addr,
        uint16_t * port);

static inline int dns_req_family_map(int i)
{
    return DNS_REQ_FAMILY_MAP[i];
}

#endif  /* SIRINET_TCP_H_ */
