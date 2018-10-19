/*
 * stream.c - For handling streams.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/service/client.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/net/stream.h>
#include <siri/net/pipe.h>
#include <siri/net/tcp.h>
#include <siri/siri.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALLOWED_PKG_SIZE 20971520      /* 20 MB  */

#define QUIT_STREAM                     \
    free(client->buf);                  \
    client->buf = NULL;                 \
    client->len = 0;                    \
    client->size = 0;                   \
    client->on_data = NULL;             \
    sirinet_stream_decref(client);      \
    return;

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Note: object->ref is initially set to 1
 */
sirinet_stream_t * sirinet_stream_new(sirinet_stream_tp_t tp, on_data_cb_t cb)
{
    size_t stream_sz;
    sirinet_stream_t * client = malloc(sizeof(sirinet_stream_t));

    if (client == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    client->tp = tp;
    client->on_data = cb;
    client->buf = NULL;
    client->len = 0;
    client->size = -1; /* this will force allocating on first request */
    client->origin = NULL;
    client->siridb = NULL;
    client->ref = 1;

    switch(tp)
    {
    case STREAM_TCP_CLIENT:
    case STREAM_TCP_BACKEND:
    case STREAM_TCP_SERVER:
    case STREAM_TCP_MANAGE:
        stream_sz = sizeof(uv_tcp_t);
        break;
    case STREAM_PIPE_CLIENT:
        stream_sz = sizeof(uv_pipe_t);
        break;
    default:
        stream_sz = sizeof(uv_stream_t);
        assert(0);
    }

    client->stream = malloc(stream_sz);
    if (client->stream == NULL)
    {
        free(client);
        ERR_ALLOC
        return NULL;
    }

    client->stream->data = client;

    return client;
}

/*
 * Return a name for the connection if successful or NULL in case of a failure.
 *
 * The returned value is malloced and should be freed.
 */
char * sirinet_stream_name(sirinet_stream_t * client)
{
    switch (client->tp)
    {
    case STREAM_TCP_CLIENT:
    case STREAM_TCP_BACKEND:
    case STREAM_TCP_SERVER:
    case STREAM_TCP_MANAGE:
        return sirinet_tcp_name((uv_tcp_t *) client->stream);
    case STREAM_PIPE_CLIENT:
        return sirinet_pipe_name((uv_pipe_t *) client->stream);
    }
    return NULL;
}

/*
 * This function can raise a SIGNAL.
 */
void sirinet_stream_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    sirinet_stream_t * client = (sirinet_stream_t *) handle->data;

    if (!client->len && client->size > RESET_BUF_SIZE)
    {
        free(client->buf);
        client->buf = (char *) malloc(suggested_size);
        if (client->buf == NULL)
        {
            ERR_ALLOC
            buf->len = 0;
            return;
        }
        client->size = suggested_size;
        client->len = 0;
    }
    buf->base = client->buf + client->len;
    buf->len = client->size - client->len;
}

/*
 * This function can raise a SIGNAL.
 */
void sirinet_stream_on_data(
        uv_stream_t * uvclient,
        ssize_t nread,
        const uv_buf_t * buf)
{
    sirinet_stream_t * client = uvclient->data;
    sirinet_pkg_t * pkg;
    size_t total_sz;
    uint8_t check;

    /*
     * client->on_data is NULL when 'sirinet_stream_decref' is called from
     * within this function. We should never call 'sirinet_stream_decref' twice
     * so the best thing is to log and and exit this function.
     */
    if (client->on_data == NULL)
    {
        char * name = sirinet_stream_name(client);
        if (name != NULL)
        {
            log_error(
                    "Received data from '%s' but we ignore the data since the "
                    "connection will be closed in a few seconds...",
                name);
            free(name);
        }
        return;
    }

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error: %s", uv_err_name(nread));
        }
        QUIT_STREAM
    }

    client->len += nread;

    if (client->len < sizeof(sirinet_pkg_t))
    {
        return;
    }

    pkg = (sirinet_pkg_t *) client->buf;
    check = pkg->tp ^ 255;
    if (check != pkg->checkbit ||
            ((      client->tp == STREAM_TCP_CLIENT ||
                    client->tp == STREAM_PIPE_CLIENT) &&
                    pkg->len > MAX_ALLOWED_PKG_SIZE))
    {
        char * name = sirinet_stream_name(client);
        if (name != NULL)
        {
            log_error(
                "Got an illegal package or size too large from '%s', "
                "closing connection "
                "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %" PRIu8 ")",
                name, pkg->pid, pkg->len, pkg->tp);
            free(name);
        }
        QUIT_STREAM
    }

    total_sz = sizeof(sirinet_pkg_t) + pkg->len;
    if (client->len < total_sz)
    {
        if (client->size < total_sz)
        {
            char * tmp = realloc(client->buf, total_sz);
            if (tmp == NULL)
            {
                log_critical(
                    "Cannot allocate size for package "
                    "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %" PRIu8 ")",
                    pkg->pid, pkg->len, pkg->tp);
                QUIT_STREAM
            }
            client->buf = tmp;
            client->size = total_sz;
        }
        return;
    }

    /* call on-data function */
    (*client->on_data)(client, pkg);

    client->len -= total_sz;

    if (client->len > 0)
    {
        /* move data and call sirinet_stream_on_data() function again */
        memmove(client->buf, client->buf + total_sz, client->len);
        sirinet_stream_on_data(uvclient, 0, buf);
    }
}

/*
 * Never use this function but call sirinet_stream_decref.
 * Destroy stream. (parsing NULL is not allowed)
 *
 * We know three different stream types:
 *  - client: used for clients. a user object might be destroyed.
 *  - back-end: used to connect to other servers. a server might be destroyed.
 *  - server: user for severs connecting to here. a server might be destroyed.
 *
 *  In case a server is destroyed, remaining promises will be cancelled and
 *  the call-back functions will be called.
 */
void sirinet__stream_free(uv_stream_t * uvclient)
{
    sirinet_stream_t * client = uvclient->data;

    switch (client->tp)
    {
    case STREAM_PIPE_CLIENT:
    case STREAM_TCP_CLIENT:  /* listens to client connections  */
        if (client->origin != NULL)
        {
            siridb_user_t * user = client->origin;
            siridb_user_decref(user);
        }
        break;
    case STREAM_TCP_BACKEND:  /* listens to server connections  */
        if (client->origin != NULL)
        {
            siridb_server_t * server = (siridb_server_t *) client->origin;
            siridb_server_decref(server);
        }
        break;
    case STREAM_TCP_SERVER:  /* a server connection  */
        {
            siridb_server_t * server = client->origin;
            server->client = NULL;
            server->flags = 0;
            siridb_server_decref(server);
        }
        break;
    case STREAM_TCP_MANAGE:  /* a server manage connection  */
        siri_service_client_free((siri_service_client_t *) client->origin);
        siri.client = NULL;
        break;
    }
    free(client->buf);
    free(client);
    free(uvclient);
}






