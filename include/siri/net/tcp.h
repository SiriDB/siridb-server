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

int dns_req_family_map[3];
const char * sirinet_tcp_ip_support_str(uint8_t ip_support);
char * sirinet_tcp_name(uv_tcp_t * client);

#endif  /* SIRINET_TCP_H_ */
