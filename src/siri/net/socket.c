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
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <siri/net/socket.h>

static sirinet_socket_t * SOCKET_new(int tp, on_data_cb cb);

void sirinet_socket_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    sirinet_socket_t * ssocket = (sirinet_socket_t *) handle->data;

    if (ssocket->buf == NULL)
    {
        buf->base = (char *) malloc(suggested_size);
        buf->len = suggested_size;
    }
    else
    {
        if (ssocket->len > SN_PKG_HEADER_SIZE)
        {
            suggested_size =
                    ((sirinet_pkg_t *) ssocket->buf)->len + SN_PKG_HEADER_SIZE;
        }

        buf->base = ssocket->buf + ssocket->len;
        buf->len = suggested_size - ssocket->len;
    }
}

void sirinet_socket_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf)
{
    sirinet_socket_t * ssocket = (sirinet_socket_t *) client->data;
    sirinet_pkg_t * pkg;

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error: %s", uv_err_name(nread));
        }

        /* the buffer must be destroyed too
         * (sirinet_free_client will free ssocket->buf if needed)
         */
        if (ssocket->buf == NULL)
        {
            free(buf->base);
        }

        uv_close((uv_handle_t *) client, (uv_close_cb) sirinet_socket_free);

        return;
    }

    if (ssocket->buf == NULL)
    {
        if (nread >= SN_PKG_HEADER_SIZE)
        {
            pkg = (sirinet_pkg_t *) buf->base;
            size_t total_sz = pkg->len + SN_PKG_HEADER_SIZE;

            if (nread == total_sz)
            {
                (*ssocket->on_data)((uv_handle_t *) client, pkg);
                free(buf->base);
                return;
            }

            if (nread > total_sz)
            {
                log_error(
                        "Got more bytes than expected, "
                        "ignore package (pid: %d, len: %d, tp: %d)",
                        pkg->pid, pkg->len, pkg->tp);
                free(buf->base);
                return;
            }

            ssocket->buf = (buf->len < total_sz) ?
                (char *) realloc(buf->base, total_sz) : buf->base;

        }
        else
        {
            ssocket->buf = buf->base;
        }

        ssocket->len = nread;

        return;
    }

    if (ssocket->len < SN_PKG_HEADER_SIZE)
    {
        ssocket->len += nread;

        if (ssocket->len < SN_PKG_HEADER_SIZE)
        {
            return;
        }

        size_t total_sz =
                ((sirinet_pkg_t *) ssocket->buf)->len + SN_PKG_HEADER_SIZE;

        if (buf->len < total_sz)
        {
            ssocket->buf = (char *) realloc(ssocket->buf, total_sz);
        }
    }
    else
    {
        ssocket->len += nread;
    }

    pkg = (sirinet_pkg_t *) ssocket->buf;

    if (ssocket->len < pkg->len + SN_PKG_HEADER_SIZE)
    {
        return;
    }

    if (ssocket->len == pkg->len + SN_PKG_HEADER_SIZE)
    {
        (*ssocket->on_data)((uv_handle_t *) client, pkg);
    }
    else
    {
        log_error(
                "Got more bytes than expected, "
                "ignore package (pid: %d, len: %d, tp: %d)",
                pkg->pid, pkg->len, pkg->tp);
    }

    free(ssocket->buf);
    ssocket->buf = NULL;
}

uv_tcp_t * sirinet_socket_new(int tp, on_data_cb cb)
{
    uv_tcp_t * socket = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));
    socket->data = SOCKET_new(tp, cb);
    return socket;
}

void sirinet_socket_free(uv_tcp_t * client)
{
    sirinet_socket_t * ssocket = client->data;
    log_debug("Free socket (type: %d)", ssocket->tp);

    switch (ssocket->tp)
    {
    case SOCKET_CLIENT:
        if (ssocket->origin != NULL)
        {
            siridb_user_decref(ssocket->origin);
        }
        break;
    case SOCKET_BACKEND:
        if (ssocket->origin != NULL)
        {
            siridb_server_decref(ssocket->origin);
        }
        break;
    case SOCKET_SERVER:
        {
            siridb_server_t * server = ssocket->origin;
            server->socket = NULL;
            siridb_server_decref(server);
        }
    }
    free(ssocket->buf);
    free(ssocket);
    free(client);
}



static sirinet_socket_t * SOCKET_new(int tp, on_data_cb cb)
{
    sirinet_socket_t * ssocket =
            (sirinet_socket_t *) malloc(sizeof(sirinet_socket_t));

    ssocket->tp = tp;
    ssocket->on_data = cb;
    ssocket->buf = NULL;
    ssocket->len = 0;
    ssocket->origin = NULL;
    ssocket->siridb = NULL;

    return ssocket;
}


