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
static char tee__buf[TEE__BUF_SZ];
static char tee__address[SIRI_CFG_MAX_LEN_ADDRESS+7];


static void tee__alloc_buffer(
    uv_handle_t * handle __attribute__((unused)),
    size_t suggsz __attribute__((unused)),
    uv_buf_t * buf)
{
    buf->base = tee__buf;
    buf->len = TEE__BUF_SZ;
}

static void tee__write_cb(uv_write_t * req, int status)
{
    sirinet_pkg_t * pkg = req->data;
    if (status)
    {
        log_error("Socket (tee) write error: %s", uv_strerror(status));
    }
    free(pkg);
    free(req);
}

static void tee__on_data(
    uv_tcp_t * tcp,
    ssize_t nread,
    const uv_buf_t * buf __attribute__((unused)))
{
    siridb_tee_t * tee = tcp->data;
    if (!tee)
    {
        return;
    }

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error on tee '%s:%u': '%s'",
                tee->address,
                tee->port,
                uv_err_name(nread));
        }

        log_info("Disconnected from tee `%s`", tee->address);

        uv_close((uv_handle_t *) tcp, (uv_close_cb) free);
        tee->tcp = NULL;
        return;
    }

    log_debug("Got %zd bytes on tee `%s:%u` which will be ignored",
            nread,
            tee->address,
            tee->port);
}

static void tee__do_write(siridb_tee_t * tee, sirinet_pkg_t * pkg)
{
    uv_write_t * req;
    uv_buf_t buf;

    pkg = sirinet_pkg_dup(pkg);
    if (!pkg)
    {
        log_error("Failed to create duplicate for tee");
        return;
    }

    req = malloc(sizeof(uv_write_t));
    buf = uv_buf_init((char *) pkg, sizeof(sirinet_pkg_t) + pkg->len);

    if (req)
    {
        int rc;
        req->data = pkg;
        rc = uv_write(req, (uv_stream_t *) tee->tcp, &buf, 1, tee__write_cb);
        if (rc == 0)
        {
            return;  /* success */
        }
        free(req);
    }
    free(pkg);
    log_error("Cannot write to tee");
    return;
}

static void tee__on_connect(uv_connect_t * req, int status)
{
    uv_tcp_t * tcp = (uv_tcp_t *) req->handle;
    siridb_tee_t * tee = tcp->data;

    if (status == 0)
    {
        if (uv_read_start(
                req->handle,
                tee__alloc_buffer,
                (uv_read_cb) tee__on_data))
        {
            /* Failed to start reading the tee connection */
            tee->err_code = SIRIDB_TEE_E_READ;
            log_error(
                    "Failed to open tee `%s:%u` for reading",
                    tee->address, tee->port);
            goto fail;
        }

        /* success */
        tee->tcp = tcp;
        log_info(
                "Connection created to tee: '%s:%u'", tee->address, tee->port);
        goto done;
    }

    /* failed */
    tee->err_code = SIRIDB_TEE_E_CONNECT;
    log_warning(
            "Cannot connect to tee '%s:%u' (%s)",
            tee->address,
            tee->port,
            uv_strerror(status));

fail:
    uv_close((uv_handle_t *) tcp, (uv_close_cb) free);
done:
    free(req);
    uv_mutex_unlock(&tee->lock_);
}

void tee__make_connection(siridb_tee_t * tee, const struct sockaddr * dest)
{
    uv_connect_t * req = malloc(sizeof(uv_connect_t));
    uv_tcp_t * tcp = malloc(sizeof(uv_tcp_t));
    if (tcp == NULL || req == NULL)
    {
        goto fail0;
    }
    tcp->data = tee;

    log_debug("Trying to connect to tee '%s:%u'...", tee->address, tee->port);

    (void) uv_tcp_init(siri.loop, tcp);
    (void) uv_tcp_connect(req, tcp, dest, tee__on_connect);

    return;

fail0:
    free(req);
    free(tcp);
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
    tee->tcp = NULL;
    tee->flags = SIRIDB_TEE_FLAG;
    tee->err_code = 0;
    uv_mutex_init(&tee->lock_);
    return tee;
}

void siridb_tee_close(siridb_tee_t * tee)
{
    uv_mutex_lock(&tee->lock_);
    if (tee->tcp && !uv_is_closing((uv_handle_t *) tee->tcp))
    {
        uv_close((uv_handle_t *) tee->tcp, (uv_close_cb) free);
        tee->tcp = NULL;
    }
    uv_mutex_unlock(&tee->lock_);
}

void siridb_tee_free(siridb_tee_t * tee)
{
    /* must be closed before free can be used */
    assert (tee->tcp == NULL);

    uv_mutex_destroy(&tee->lock_);
    free(tee->address);
    free(tee);
}

const char * siridb_tee_str(siridb_tee_t * tee)
{
    if (tee->err_code)
    {
        switch((enum siridb_tee_e_t) tee->err_code)
        {
        case SIRIDB_TEE_E_OK:       return "OK";
        case SIRIDB_TEE_E_ALLOC:    return "memory allocation error";
        case SIRIDB_TEE_E_READ:     return "failed to open socket for reading";
        case SIRIDB_TEE_E_CONNECT:  return "failed to make connection";
        }
    }
    if (tee->address)
    {
        (void) sprintf(tee__address, "%s:%u", tee->address, tee->port);
        return tee__address;
    }
    return "disabled";
}

void siridb_tee_write(siridb_tee_t * tee, sirinet_pkg_t * pkg)
{
    assert (tee->address);
    if (tee->tcp)
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

    tee->err_code = SIRIDB_TEE_E_OK;
    tee->address = address ? strdup(address) : NULL;
    tee->port = port;

    if (tee->tcp && !uv_is_closing((uv_handle_t *) tee->tcp))
    {
        uv_close((uv_handle_t *) tee->tcp, (uv_close_cb) free);
        tee->tcp = NULL;
    }

    uv_mutex_unlock(&tee->lock_);

    return address && !tee->address ? -1 : 0;
}
