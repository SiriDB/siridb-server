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

static void sirinet_handle_write_cb(uv_write_t * req, int status);

void sirinet_handle_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf)
{
    buf->base = (char *) malloc(suggested_size);
    buf->len = suggested_size;
}

void sirinet_handle_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            log_error("Read error: %s", uv_err_name(nread));
        log_debug("Free client...");
        uv_close((uv_handle_t *) client, sirinet_free_client);
    }
    else if (nread >= SN_PKG_HEADER_SIZE)
    {
        (*((sirinet_handle_t *) client->data)->on_data)(
                (uv_handle_t *) client,
                (sirinet_pkg_t *) buf->base);
    }
    else if (nread > 0)
    {
        log_debug("Hmm, lets see what to do now...", nread);
    } else if (nread == 0)
    {
        log_debug("Ok, now I got 0 nread...");
    }

    free(buf->base); /* was prefixed with if (buf->base)... */
}

void sirinet_send_pkg(
        uv_handle_t * client,
        sirinet_pkg_t * pkg,
        uv_write_cb cb)
{
    if (cb == NULL)
        cb = (uv_write_cb) (&sirinet_handle_write_cb);

    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));
    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            SN_PKG_HEADER_SIZE + pkg->len);
    uv_write(req,
            (uv_stream_t *) client,
            &wrbuf,
            1,
            *cb);
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

static void sirinet_handle_write_cb(uv_write_t * req, int status)
{
    if (status)
        log_error("Write error %s", uv_strerror(status));
    free(req);
}

sirinet_pkg_t * sirinet_new_pkg(
        uint64_t pid,
        uint32_t len,
        uint16_t tp,
        const char * data)
{
    /*
     * do not forget to run free(pkg) when using sirinet_new_pkg
     */
    sirinet_pkg_t * pkg =
            (sirinet_pkg_t *) malloc(sizeof(sirinet_pkg_t) + len);
    pkg->pid = pid;
    pkg->len = len;
    pkg->tp = tp;
    if (data != NULL)
        memcpy(pkg->data, data, len);

    return pkg;
}

