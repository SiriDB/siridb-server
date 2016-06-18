/*
 * handle.c - Handle TCP request.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#include <siri/net/handle.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void sirinet_handle_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    sirinet_handle_t * sn_handle = (sirinet_handle_t *) handle->data;

    if (sn_handle->buf == NULL)
    {
        buf->base = (char *) malloc(suggested_size);
        buf->len = suggested_size;
    }
    else
    {
        if (sn_handle->len > SN_PKG_HEADER_SIZE)
        {
            suggested_size = ((sirinet_pkg_t *) sn_handle->buf)->len + SN_PKG_HEADER_SIZE;
        }

        buf->base = sn_handle->buf + sn_handle->len;
        buf->len = suggested_size - sn_handle->len;
    }
}

void sirinet_handle_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf)
{
    sirinet_handle_t * sn_handle = (sirinet_handle_t *) client->data;
    sirinet_pkg_t * pkg;

    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            log_error("Read error: %s", uv_err_name(nread));
        }

        uv_close((uv_handle_t *) client, sirinet_free_client);

        free((sn_handle->buf == NULL) ? buf->base : sn_handle->buf);

        return;
    }

    if (sn_handle->buf == NULL)
    {
        if (nread >= SN_PKG_HEADER_SIZE)
        {
            pkg = (sirinet_pkg_t *) buf->base;
            size_t total_sz = pkg->len + SN_PKG_HEADER_SIZE;

            if (nread == total_sz)
            {
                (*sn_handle->on_data)((uv_handle_t *) client, pkg);
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

            sn_handle->buf = (buf->len < total_sz) ?
                (char *) realloc(buf->base, total_sz) : buf->base;

        }
        else
        {
            sn_handle->buf = buf->base;
        }

        sn_handle->len = nread;

        return;
    }

    if (sn_handle->len < SN_PKG_HEADER_SIZE)
    {
        sn_handle->len += nread;

        if (sn_handle->len < SN_PKG_HEADER_SIZE)
        {
            return;
        }

        size_t total_sz =
                ((sirinet_pkg_t *) sn_handle->buf)->len + SN_PKG_HEADER_SIZE;

        if (buf->len < total_sz)
        {
            sn_handle->buf = (char *) realloc(sn_handle->buf, total_sz);
        }
    }
    else
    {
        sn_handle->len += nread;
    }

    pkg = (sirinet_pkg_t *) sn_handle->buf;

    if (sn_handle->len < pkg->len + SN_PKG_HEADER_SIZE)
    {
        return;
    }

    if (sn_handle->len == pkg->len + SN_PKG_HEADER_SIZE)
    {
        (*sn_handle->on_data)((uv_handle_t *) client, pkg);
    }
    else
    {
        log_error(
                "Got more bytes than expected, "
                "ignore package (pid: %d, len: %d, tp: %d)",
                pkg->pid, pkg->len, pkg->tp);
    }

    free(sn_handle->buf);
    sn_handle->buf = NULL;
}

void sirinet_free_client(uv_handle_t * client)
{
    log_debug("Free client...");
    free((sirinet_handle_t *) client->data);
    free((uv_tcp_t *) client);
}

void sirinet_free_async(uv_handle_t * handle)
{
    free((uv_async_t *) handle);
}



