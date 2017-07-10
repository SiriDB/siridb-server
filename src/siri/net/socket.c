/*
 * socket.c - Handle TCP request.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/admin/client.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/net/socket.h>
#include <siri/siri.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALLOWED_PKG_SIZE 20971520      // 20 MB

#define QUIT_SOCKET                     \
    free(ssocket->buf);                    \
    ssocket->buf = NULL;                \
    ssocket->len = 0;                    \
    ssocket->size = 0;                    \
    ssocket->on_data = NULL;            \
    sirinet_socket_decref(client);        \
    return;

/* dns_req_family_map maps to IP_SUPPORT values defined in socket.h */
int dns_req_family_map[3] = {AF_UNSPEC, AF_INET, AF_INET6};

const char * sirinet_socket_ip_support_str(uint8_t ip_support)
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
 * This function can raise a SIGNAL.
 */
void sirinet_socket_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    sirinet_socket_t * ssocket = (sirinet_socket_t *) handle->data;

    if (!ssocket->len && ssocket->size > RESET_BUF_SIZE)
    {
        free(ssocket->buf);
        ssocket->buf = (char *) malloc(suggested_size);
        if (ssocket->buf == NULL)
        {
            ERR_ALLOC
            buf->len = 0;
            return;
        }
        ssocket->size = suggested_size;
        ssocket->len = 0;
    }
    buf->base = ssocket->buf + ssocket->len;
    buf->len = ssocket->size - ssocket->len;
}

/*
 * Buffer should have a size of ADDR_BUF_SZ
 *
 * Return 0 if successful or -1 in case of an error.
 */
int sirinet_addr_and_port(char * buffer, uv_stream_t * client)
{
    struct sockaddr_storage name;
    int namelen = sizeof(name);

    if (uv_tcp_getpeername(
            (uv_tcp_t *) client,
            (struct sockaddr *) &name,
            &namelen))
    {
        return -1;
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
            snprintf(
                    buffer,
                    ADDR_BUF_SZ,
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
            snprintf(
                    buffer,
                    ADDR_BUF_SZ,
                    "[%s]:%d",
                    addr,
                    ntohs(((struct sockaddr_in6 *) &name)->sin6_port));
        }
        break;

    default:
        return -1;
    }

    return 0;
}

/*
 * This function can raise a SIGNAL.
 */
void sirinet_socket_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf)
{
    sirinet_socket_t * ssocket = (sirinet_socket_t *) client->data;
    sirinet_pkg_t * pkg;
    size_t total_sz;

    /*
     * ssocket->on_data is NULL when 'sirinet_socket_decref' is called from
     * within this function. We should never call 'sirinet_socket_decref' twice
     * so the best thing is to log and and exit this function.
     */
    if (ssocket->on_data == NULL)
    {
        char addr_port[ADDR_BUF_SZ];
        if (sirinet_addr_and_port(addr_port, client) == 0)
        {
            log_error(
                    "Received data from '%s' but we ignore the data since the "
                    "connection will be closed in a few seconds...",
                addr_port);
        }
        return;
    }

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error: %s", uv_err_name(nread));
        }
        QUIT_SOCKET
    }

    ssocket->len += nread;

    if (ssocket->len < sizeof(sirinet_pkg_t))
    {
        return;
    }

    pkg = (sirinet_pkg_t *) ssocket->buf;
    if (    (pkg->tp ^ 255) != pkg->checkbit ||
            (ssocket->tp == SOCKET_CLIENT && pkg->len > MAX_ALLOWED_PKG_SIZE))
    {
        char addr_port[ADDR_BUF_SZ];
        if (sirinet_addr_and_port(addr_port, client) == 0)
        {
            log_error(
                "Got an illegal package or size too large from '%s', "
                "closing connection "
                "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %" PRIu8 ")",
                addr_port, pkg->pid, pkg->len, pkg->tp);
        }
        QUIT_SOCKET
    }

    total_sz = sizeof(sirinet_pkg_t) + pkg->len;
    if (ssocket->len < total_sz)
    {
        if (ssocket->size < total_sz)
        {
            char * tmp = realloc(ssocket->buf, total_sz);
            if (tmp == NULL)
            {
                log_critical(
                    "Cannot allocate size for package "
                    "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %" PRIu8 ")",
                    pkg->pid, pkg->len, pkg->tp);
                QUIT_SOCKET
            }
            ssocket->buf = tmp;
            ssocket->size = total_sz;
        }
        return;
    }

    /* call on-data function */
    (*ssocket->on_data)(client, pkg);

    ssocket->len -= total_sz;

    if (ssocket->len > 0)
    {
        /* move data and call sirinet_socket_on_data() function again */
        memmove(ssocket->buf, ssocket->buf + total_sz, ssocket->len);
        sirinet_socket_on_data(client, 0, buf);
    }
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Note: ((sirinet_socket_t *) socket->data)->ref is initially set to 1
 */
uv_tcp_t * sirinet_socket_new(sirinet_socket_tp_t tp, on_data_cb_t cb)
{
    sirinet_socket_t * ssocket =
            (sirinet_socket_t *) malloc(sizeof(sirinet_socket_t));

    if (ssocket == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    ssocket->tp = tp;
    ssocket->on_data = cb;
    ssocket->buf = NULL;
    ssocket->len = 0;
    ssocket->size = -1; /* this will force allocating on first request */
    ssocket->origin = NULL;
    ssocket->siridb = NULL;
    ssocket->ref = 1;
    ssocket->tcp.data = ssocket;

    return &ssocket->tcp;
}

/*
 * Never use this function but call sirinet_socket_decref.
 * Destroy socket. (parsing NULL is not allowed)
 *
 * We know three different socket types:
 *  - client: used for clients. a user object might be destroyed.
 *  - back-end: used to connect to other servers. a server might be destroyed.
 *  - server: user for severs connecting to here. a server might be destroyed.
 *
 *  In case a server is destroyed, remaining promises will be cancelled and
 *  the call-back functions will be called.
 */
void sirinet__socket_free(uv_stream_t * client)
{
    sirinet_socket_t * ssocket = client->data;

#ifdef DEBUG
    log_debug("Free socket type: %d", ssocket->tp);
#endif

    switch (ssocket->tp)
    {
    case SOCKET_CLIENT:  // listens to client connections
        if (ssocket->origin != NULL)
        {
            siridb_user_t * user = (siridb_user_t *) ssocket->origin;
            siridb_user_decref(user);
        }
        break;
    case SOCKET_BACKEND:  // listens to server connections
        if (ssocket->origin != NULL)
        {
            siridb_server_t * server = (siridb_server_t *) ssocket->origin;
            siridb_server_decref(server);
        }
        break;
    case SOCKET_SERVER:  // a server connection
        {
            siridb_server_t * server = (siridb_server_t *) ssocket->origin;
            server->socket = NULL;
            server->flags = 0;
            siridb_server_decref(server);
        }
        break;
    case SOCKET_MANAGE:  // a server manage connection
        siri_admin_client_free((siri_admin_client_t *) ssocket->origin);
        siri.socket = NULL;
        break;
    }
    free(ssocket->buf);
    free(ssocket);
}





