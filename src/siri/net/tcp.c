/*
 * tcp.c - TCP support.
 */
#include <siri/net/tcp.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <uv.h>
#include <xstr/xstr.h>
#include <siri/cfg/cfg.h>
#include <logger/logger.h>

#define TCP_NAME_BUF_SZ 54

/* dns_req_family_map maps to IP_SUPPORT values defined in socket.h */
int dns_req_family_map[3] = {AF_UNSPEC, AF_INET, AF_INET6};

const char * sirinet_tcp_ip_support_str(uint8_t ip_support)
{
    switch (ip_support)
    {
    case IP_SUPPORT_ALL: return "ALL";
    case IP_SUPPORT_IPV4ONLY: return "IPV4ONLY";
    case IP_SUPPORT_IPV6ONLY: return "IPV6ONLY";
    default: return "UNKNOWN";
    }
}

/*
 * Return a name for the connection if successful or NULL in case of a failure.
 *
 * The returned value is malloced and should be freed.
 */
char * sirinet_tcp_name(uv_tcp_t * client)
{
    char * buffer = malloc(TCP_NAME_BUF_SZ);
    struct sockaddr_storage name;
    int namelen = sizeof(name);

    if (buffer == NULL ||
        uv_tcp_getpeername(client, (struct sockaddr *) &name, &namelen))
    {
        goto failed;
    }

    switch (name.ss_family)
    {
    case AF_INET:
        {
            char addr[INET_ADDRSTRLEN];
            uv_inet_ntop(
                    AF_INET,
                    &((struct sockaddr_in *) &name)->sin_addr,
                    addr,
                    sizeof(addr));
            sprintf(
                    buffer,
                    "%s:%d",
                    addr,
                    ntohs(((struct sockaddr_in *) &name)->sin_port));
        }
        break;

    case AF_INET6:
        {
            char addr[INET6_ADDRSTRLEN];
            uv_inet_ntop(
                    AF_INET6,
                    &((struct sockaddr_in6 *) &name)->sin6_addr,
                    addr,
                    sizeof(addr));
            sprintf(
                    buffer,
                    "[%s]:%d",
                    addr,
                    ntohs(((struct sockaddr_in6 *) &name)->sin6_port));
        }
        break;

    default:
        goto failed;
    }

    return buffer;

failed:
    free(buffer);
    return NULL;
}

int sirinet_extract_addr_port(
        char * s,
        char * addr,
        uint16_t * addrport)
{
    int test_port;
    char * strport;
    char * address;
    char hostname[SIRI_CFG_MAX_LEN_ADDRESS];
    if (gethostname(hostname, SIRI_CFG_MAX_LEN_ADDRESS))
    {
        log_debug(
                "Unable to read the systems host name. Since its only purpose "
                "is to apply this in the configuration file this might not be "
                "any problem. (using 'localhost' as fallback)");
        strcpy(hostname, "localhost");
    }


    if (*s == '[')
    {
        /* an IPv6 address... */
        for (strport = address = s + 1; *strport; strport++)
        {
            if (*strport == ']')
            {
                *strport = 0;
                strport++;
                break;
            }
        }
    }
    else
    {
        strport = address = s;
    }

    for (; *strport; strport++)
    {
        if (*strport == ':')
        {
            *strport = 0;
            strport++;
            break;
        }
    }

    if (    !strlen(address) ||
            strlen(address) >= SIRI_CFG_MAX_LEN_ADDRESS ||
            !xstr_is_int(strport) ||
            strcpy(addr, address) == NULL ||
            xstr_replace_str(
                    addr,
                    "%HOSTNAME",
                    hostname,
                    SIRI_CFG_MAX_LEN_ADDRESS))
    {
        log_critical(
                "error: got an unexpected value '%s:%s'.",
                address,
                strport);
        return -1;
    }

    test_port = atoi(strport);

    if (test_port < 1 || test_port > 65535)
    {
        log_critical(
                "error: port should be between 1 and 65535, got '%d'.",
                test_port);
        return -1;
    }

    *addrport = (uint16_t) test_port;

    log_debug("Read '%s': %s:%d",
            s,
            addr,
            *addrport);
    return 0;
}



