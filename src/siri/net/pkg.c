/*
 * pkg.h - SiriDB Package type.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-06-2016
 *
 */
#include <siri/net/pkg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <logger/logger.h>

static void PKG_write_cb(uv_write_t * req, int status);


sirinet_pkg_t * sirinet_pkg_new(
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
    {
        memcpy(pkg->data, data, len);
    }
    return pkg;
}

void sirinet_pkg_send(
        uv_stream_t * client,
        sirinet_pkg_t * pkg,
        uv_write_cb cb,
        void * data)
{
    if (cb == NULL)
    {
        cb = (uv_write_cb) (&PKG_write_cb);
    }

    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));

    req->data = data;

    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            SN_PKG_HEADER_SIZE + pkg->len);

    uv_write(req, client, &wrbuf, 1, *cb);
}

static void PKG_write_cb(uv_write_t * req, int status)
{
    if (status)
    {
        log_error("Socket write error: %s", uv_strerror(status));
    }
    free(req);
}
