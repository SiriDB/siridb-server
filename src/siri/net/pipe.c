#include <assert.h>
#include <logger/logger.h>
#include <siri/admin/client.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/net/pipe.h>
#include <siri/siri.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALLOWED_PKG_SIZE 20971520      /* 20 MB  */

#define QUIT_PIPE                     \
    free(spipe->buf);                 \
    spipe->buf = NULL;                \
    spipe->len = 0;                   \
    spipe->size = 0;                  \
    spipe->on_data = NULL;            \
    sirinet_pipe_decref(client);      \
    return;

/*
 * This function can raise a SIGNAL.
 */
void sirinet_pipe_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    sirinet_pipe_t * spipe = (sirinet_pipe_t *) handle->data;

    if (!spipe->len && spipe->size > RESET_BUF_SIZE)
    {
        free(spipe->buf);
        spipe->buf = (char *) malloc(suggested_size);
        if (spipe->buf == NULL)
        {
            ERR_ALLOC
            buf->len = 0;
            return;
        }
        spipe->size = suggested_size;
        spipe->len = 0;
    }
    buf->base = spipe->buf + spipe->len;
    buf->len = spipe->size - spipe->len;
}

/*
 * Buffer should have a size of PIPE_NAME_SZ
 *
 * Return 0 if successful or -1 in case of an error.
 */
int sirinet_pipe_name(char * buffer, uv_stream_t * client)
{
    size_t len = PIPE_NAME_SZ - 1;

    if (uv_pipe_getsockname(
            (uv_pipe_t *) client,
            buffer,
            &len))
    {
        return -1;
    }

    buffer[len] = 0;
    return 0;
}

/*
 * This function can raise a SIGNAL.
 */
void sirinet_pipe_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf)
{
    sirinet_pipe_t * spipe = (sirinet_pipe_t *) client->data;
    sirinet_pkg_t * pkg;
    size_t total_sz;
    uint8_t check;

    /*
     * spipe->on_data is NULL when 'sirinet_pipe_decref' is called from
     * within this function. We should never call 'sirinet_pipe_decref' twice
     * so the best thing is to log and and exit this function.
     */
    if (spipe->on_data == NULL)
    {
        char pipe_name[PIPE_NAME_SZ];
        if (sirinet_pipe_name(pipe_name, client) == 0)
        {
            log_error(
                    "Received data from '%s' but we ignore the data since the "
                    "connection will be closed in a few seconds...",
                pipe_name);
        }
        return;
    }

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error: %s", uv_err_name(nread));
        }
        QUIT_PIPE
    }

    spipe->len += nread;

    if (spipe->len < sizeof(sirinet_pkg_t))
    {
        return;
    }

    pkg = (sirinet_pkg_t *) spipe->buf;
    check = pkg->tp ^ 255;
    if (    check != pkg->checkbit ||
            (spipe->tp == PIPE_CLIENT && pkg->len > MAX_ALLOWED_PKG_SIZE))
    {
        char pipe_name[PIPE_NAME_SZ];
        if (sirinet_pipe_name(pipe_name, client) == 0)
        {
            log_error(
                "Got an illegal package or size too large from '%s', "
                "closing connection "
                "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %" PRIu8 ")",
                pipe_name, pkg->pid, pkg->len, pkg->tp);
        }
        QUIT_PIPE
    }

    total_sz = sizeof(sirinet_pkg_t) + pkg->len;
    if (spipe->len < total_sz)
    {
        if (spipe->size < total_sz)
        {
            char * tmp = realloc(spipe->buf, total_sz);
            if (tmp == NULL)
            {
                log_critical(
                    "Cannot allocate size for package "
                    "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %" PRIu8 ")",
                    pkg->pid, pkg->len, pkg->tp);
                QUIT_PIPE
            }
            spipe->buf = tmp;
            spipe->size = total_sz;
        }
        return;
    }

    /* call on-data function */
    (*spipe->on_data)(client, pkg);

    spipe->len -= total_sz;

    if (spipe->len > 0)
    {
        /* move data and call sirinet_pipe_on_data() function again */
        memmove(spipe->buf, spipe->buf + total_sz, spipe->len);
        sirinet_pipe_on_data(client, 0, buf);
    }
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Note: ((sirinet_pipe_t *) pipe->data)->ref is initially set to 1
 */
uv_pipe_t * sirinet_pipe_new(
        sirinet_pipe_tp_t tp,
        on_data_cb_t cb_data,
        on_free_cb_t cb_free)
{
    sirinet_pipe_t * spipe =
            (sirinet_pipe_t *) malloc(sizeof(sirinet_pipe_t));

    if (spipe == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    spipe->tp = tp;
    spipe->on_data = cb_data;
    spipe->on_free = cb_free;
    spipe->buf = NULL;
    spipe->len = 0;
    spipe->size = -1; /* this will force allocating on first request */
    spipe->origin = NULL;
    spipe->siridb = NULL;
    spipe->ref = 1;
    spipe->pipe.data = spipe;

    return &spipe->pipe;
}

/*
 * Never use this function but call sirinet_pipe_decref.
 * Destroy pipe. (parsing NULL is not allowed)
 *
 * We know three different pipe types:
 *  - client: used for clients. a user object might be destroyed.
 *  - back-end: used to connect to other servers. a server might be destroyed.
 *  - server: user for severs connecting to here. a server might be destroyed.
 *
 *  In case a server is destroyed, remaining promises will be cancelled and
 *  the call-back functions will be called.
 */
void sirinet__pipe_free(uv_stream_t * client)
{
    sirinet_pipe_t * spipe = client->data;

#if DEBUG
    log_debug("Free pipe type: %d", spipe->tp);
#endif

    switch (spipe->tp)
    {
    case PIPE_CLIENT:  /* listens to client connections  */
        if (spipe->origin != NULL)
        {
            siridb_user_t * user = (siridb_user_t *) spipe->origin;
            siridb_user_decref(user);
        }
        break;
    case PIPE_BACKEND:  /* listens to server connections  */
        if (spipe->origin != NULL)
        {
            siridb_server_t * server = (siridb_server_t *) spipe->origin;
            siridb_server_decref(server);
        }
        break;
    }

    if (spipe->on_free != NULL)
    {
        spipe->on_free(client);
    }

    free(spipe->buf);
    free(spipe);
}
