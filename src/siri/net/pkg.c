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
#include <siri/err.h>
#include <qpack/qpack.h>
#include <assert.h>

static void PKG_write_cb(uv_write_t * req, int status);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (do not forget to run free(...) on the result. )
 */
sirinet_pkg_t * sirinet_pkg_new(
        uint64_t pid,
        uint32_t len,
        uint16_t tp,
        const char * data)
{
    sirinet_pkg_t * pkg =
            (sirinet_pkg_t *) malloc(sizeof(sirinet_pkg_t) + len);

    if (pkg == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        pkg->pid = pid;
        pkg->len = len;
        pkg->tp = tp;
        if (data != NULL)
        {
            memcpy(pkg->data, data, len);
        }
    }
    return pkg;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (do not forget to run free(...) on the result. )
 */
sirinet_pkg_t * sirinet_pkg_err(
        uint64_t pid,
        uint32_t len,
        uint16_t tp,
        const char * data)
{
#ifdef DEBUG
    assert (data != NULL);
#endif

    sirinet_pkg_t * pkg;
    qp_packer_t * packer = qp_packer_new(len + 20);
    if (packer == NULL)
    {
        pkg = NULL;  /* signal is raised */
    }
    else
    {

        qp_add_type(packer, QP_MAP_OPEN);
        qp_add_raw(packer, "error_msg", 9);
        qp_add_raw(packer, data, len);

        pkg = sirinet_pkg_new(pid, packer->len, tp, packer->buffer);

        qp_packer_free(packer);
    }
    return pkg;
}

/*
 * Returns 0 if successful or -1 when an error has occurred.
 * (signal is raised in case of an error)
 */
int sirinet_pkg_send(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));
    if (req == NULL)
    {
        ERR_ALLOC
        return -1;
    }
    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            PKG_HEADER_SIZE + pkg->len);
    uv_write(req, client, &wrbuf, 1, PKG_write_cb);
    return 0;
}

/*
 * Returns a copy of package allocated using malloc().
 * In case of an error, NULL is returned and a signal is raised.
 */
sirinet_pkg_t * sirinet_pkg_dup(sirinet_pkg_t * pkg)
{
    size_t size = PKG_HEADER_SIZE + pkg->len;
    sirinet_pkg_t * dup = (sirinet_pkg_t *) malloc(size);
    if (dup == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        memcpy(dup, pkg, size);
    }
    return dup;
}

static void PKG_write_cb(uv_write_t * req, int status)
{
    if (status)
    {
        log_error("Socket write error: %s", uv_strerror(status));
    }
    free(req);
}
