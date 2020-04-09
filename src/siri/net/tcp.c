/*
 * tcp.c - TCP support.
 */
#include <siri/net/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

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
        const char * s,
        char * addr,
        uint16_t * port)
{
    if (*option->val->string == '[')
    {
        /* an IPv6 address... */
        for (port = address = option->val->string + 1; *port; port++)
        {
            if (*port == ']')
            {
                *port = 0;
                port++;
                break;
            }
        }
    }
    else
    {
        port = address = option->val->string;
    }

    for (; *port; port++)
    {
        if (*port == ':')
        {
            *port = 0;
            port++;
            break;
        }
    }

    if (    !strlen(address) ||
            strlen(address) >= SIRI_CFG_MAX_LEN_ADDRESS ||
            !xstr_is_int(port) ||
            strcpy(address_pt, address) == NULL ||
            xstr_replace_str(
                    address_pt,
                    "%HOSTNAME",
                    hostname,
                    SIRI_CFG_MAX_LEN_ADDRESS))
    {
        log_critical(
                "Error reading '%s' in '%s': "
                "error: got an unexpected value '%s:%s'.",
                option_name,
                siri.args->config,
                address,
                port);
        exit(EXIT_FAILURE);
    }
    else
    {
        test_port = atoi(port);

        if (test_port < 1 || test_port > 65535)
        {
            log_critical(
                    "Error reading '%s' in '%s': "
                    "error: port should be between 1 and 65535, got '%d'.",
                    option_name,
                    siri.args->config,
                    test_port);
            exit(EXIT_FAILURE);
        }
        else
        {
            *port_pt = (uint16_t) test_port;
        }

        log_debug("Read '%s' from config: %s:%d",
                option_name,
                address_pt,
                *port_pt);
    }
}



