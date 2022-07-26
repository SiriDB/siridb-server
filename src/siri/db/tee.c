/*
 * tee.c - To tee the data for a SiriDB database.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <siri/db/tee.h>
#include <siri/siri.h>
#include <siri/net/tcp.h>
#include <logger/logger.h>

#define TEE__BUF_SZ 512
static char tee__address[SIRI_CFG_MAX_LEN_ADDRESS+7];


static void tee__do_write(siridb_tee_t * tee, sirinet_pkg_t * pkg)
{
    int rc;
    uv_buf_t buf;

    buf = uv_buf_init((char *) pkg, sizeof(sirinet_pkg_t) + pkg->len);
    rc = uv_udp_try_send(tee->udp, &buf, 1, NULL);
    if (rc != 0)
    {
        log_error("Cannot write to tee");
    }
}

void tee__make_connection(siridb_tee_t * tee, const struct sockaddr * dest)
{
    uv_udp_t * udp = malloc(sizeof(uv_udp_t));
    if (udp == NULL)
    {
        free(udp);
        uv_mutex_unlock(&tee->lock_);
        return;
    }
    udp->data = tee;

    log_debug("Trying to connect to tee '%s:%u'...", tee->address, tee->port);

    if (uv_udp_init(siri.loop, udp) || uv_udp_connect(udp, dest))
    {
        free(udp);
        uv_mutex_unlock(&tee->lock_);
        return;
    }

    tee->udp = udp;
    uv_mutex_unlock(&tee->lock_);
}

static void tee__on_resolved(
        uv_getaddrinfo_t * resolver,
        int status,
        struct addrinfo * res)
{
    siridb_tee_t * tee = resolver->data;
    free(resolver);

    if (status < 0)
    {
        log_error("Cannot resolve ip address for tee '%s' (error: %s)",
                tee->address,
                uv_err_name(status));
        uv_mutex_unlock(&tee->lock_);
        goto final;
    }

    if (Logger.level == LOGGER_DEBUG)
    {
        char addr[47] = {'\0'};  /* enough for both ipv4 and ipv6 */

        switch (res->ai_family)
        {
        case AF_INET:
            uv_ip4_name((struct sockaddr_in *) res->ai_addr, addr, 16);
            break;

        case AF_INET6:
            uv_ip6_name((struct sockaddr_in6 *) res->ai_addr, addr, 46);
            break;

        default:
            sprintf(addr, "unsupported family");
        }

        log_debug(
                "Resolved ip address '%s' for tee '%s', "
                "trying to connect...",
                addr, tee->address);
    }

    tee__make_connection(tee, (const struct sockaddr *) res->ai_addr);
final:
    uv_freeaddrinfo(res);
}

static int tee__resolve_dns(siridb_tee_t * tee, int ai_family)
{
    int result;
    struct addrinfo hints;
    char port[6]= {0};
    uv_getaddrinfo_t * resolver = malloc(sizeof(uv_getaddrinfo_t));

    hints.ai_family = ai_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;

    if (resolver == NULL)
    {
        return -1;
    }

    resolver->data = tee;

    sprintf(port, "%u", tee->port);

    result = uv_getaddrinfo(
            siri.loop,
            resolver,
            tee__on_resolved,
            tee->address,
            port,
            &hints);

    if (result)
    {
        log_error("getaddrinfo call error %s", uv_err_name(result));
        free(resolver);
    }

    return result;
}

void tee__connect(siridb_tee_t * tee)
{
    struct in_addr sa;
    struct in6_addr sa6;

    if (inet_pton(AF_INET, tee->address, &sa))
    {
        /* IPv4 */
        struct sockaddr_in dest;
        (void) uv_ip4_addr(tee->address, tee->port, &dest);
        tee__make_connection(tee, (const struct sockaddr *) &dest);
        return;
    }

    if (inet_pton(AF_INET6, tee->address, &sa6))
    {
        /* IPv6 */
        struct sockaddr_in6 dest6;
        (void) uv_ip6_addr(tee->address, tee->port, &dest6);
        tee__make_connection(tee, (const struct sockaddr *) &dest6);
        return;
    }

    /* Try DNS */
    if (tee__resolve_dns(tee, dns_req_family_map(siri.cfg->ip_support)))
    {
        uv_mutex_unlock(&tee->lock_);
    }
}

siridb_tee_t * siridb_tee_new(void)
{
    siridb_tee_t * tee = malloc(sizeof(siridb_tee_t));
    if (tee == NULL)
    {
        return NULL;
    }
    tee->address = NULL;
    tee->udp = NULL;
    tee->flags = SIRIDB_TEE_FLAG;
    uv_mutex_init(&tee->lock_);
    return tee;
}

void siridb_tee_close(siridb_tee_t * tee)
{
    uv_mutex_lock(&tee->lock_);
    if (tee->udp && !uv_is_closing((uv_handle_t *) tee->udp))
    {
        uv_close((uv_handle_t *) tee->udp, (uv_close_cb) free);
        tee->udp = NULL;
    }
    uv_mutex_unlock(&tee->lock_);
}

void siridb_tee_free(siridb_tee_t * tee)
{
    /* must be closed before free can be used */
    assert (tee->udp == NULL);

    uv_mutex_destroy(&tee->lock_);
    free(tee->address);
    free(tee);
}

const char * siridb_tee_str(siridb_tee_t * tee)
{
    if (tee->address)
    {
        const char * const connected = "%s:%u (connected)";
        const char * const disconnected = "%s:%u (disconnected)";

        (void) sprintf(
                tee__address,
                tee->udp ? connected : disconnected,
                tee->address, tee->port);
        return tee__address;
    }
    return "disabled";
}

void siridb_tee_write(siridb_tee_t * tee, sirinet_pkg_t * pkg)
{
    assert (tee->address);
    if (tee->udp)
    {
        tee__do_write(tee, pkg);
    }
    else
    {
        log_debug("No tee connection (yet)...");

        if (uv_mutex_trylock(&tee->lock_))
        {
            return;
        }

        tee__connect(tee);
    }
}

int siridb_tee_set_address_port(
        siridb_tee_t * tee,
        const char * address,
        uint16_t port)
{
    uv_mutex_lock(&tee->lock_);

    free(tee->address);

    tee->address = address ? strdup(address) : NULL;
    tee->port = port;

    if (tee->udp && !uv_is_closing((uv_handle_t *) tee->udp))
    {
        uv_close((uv_handle_t *) tee->udp, (uv_close_cb) free);
        tee->udp = NULL;
    }

    uv_mutex_unlock(&tee->lock_);

    return address && !tee->address ? -1 : 0;
}
