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
#include <assert.h>
#include <logger/logger.h>
#include <siri/err.h>
#include <siri/net/pkg.h>
#include <siri/net/socket.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct pkg_send_s
{
    sirinet_pkg_t * pkg;
    uv_stream_t * client;
} pkg_send_t;

static void PKG_write_cb(uv_write_t * req, int status);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (do not forget to run free(...) on the result. )
 */
sirinet_pkg_t * sirinet_pkg_new(
        uint16_t pid,
        uint32_t len,
        uint8_t tp,
        const char * data)
{
    sirinet_pkg_t * pkg =
            (sirinet_pkg_t *) malloc(sizeof(sirinet_pkg_t) + len);

#ifdef DEBUG
    assert (sizeof(sirinet_pkg_t) == sizeof(sirinet_pkg_t));
#endif

    if (pkg == NULL)
    {
        ERR_ALLOC
    }
    else
    {

        pkg->len = len;
        pkg->pid = pid;
        pkg->tp = tp;
        pkg->checkbit = 0;  /* check bit will be set when send */

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
sirinet_pkg_t * sirinet_pkg_copy(sirinet_pkg_t * source)
{
    size_t size = sizeof(sirinet_pkg_t) + source->len;
    sirinet_pkg_t * pkg = (sirinet_pkg_t *) malloc(size);

    if (pkg == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        memcpy(pkg, source, size);
    }
    return pkg;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Use 'qp_packer_free' to destroy the returned value or use
 * 'sirinet_packer2pkg' to convert to 'sirinet_pkg_t'.
 */
qp_packer_t * sirinet_packer_new(size_t alloc_size)
{
#ifdef DEBUG
    assert (alloc_size >= sizeof(sirinet_pkg_t));
#endif

    qp_packer_t * packer = qp_packer_new(alloc_size);
    if (packer != NULL)
    {
        packer->len = sizeof(sirinet_pkg_t);
#ifdef DEBUG
        ((sirinet_pkg_t *) packer->buffer)->tp = PKG___QP_TP;
#endif
    }

    return packer;
}

/*
 * Returns a 'sirinet_pkg_t' from a packer created with 'sirinet_packer_new'
 *
 * Call 'free' to destroy the returned pkg and do not destroy the
 * packer anymore since this is handled here.
 */
sirinet_pkg_t * sirinet_packer2pkg(
        qp_packer_t * packer,
        uint16_t pid,
        uint8_t tp)
{
    sirinet_pkg_t * pkg = (sirinet_pkg_t *) packer->buffer;

#ifdef DEBUG
    assert (pkg->tp == PKG___QP_TP);
#endif


    pkg->pid = pid;
    pkg->tp = tp;
    pkg->len = packer->len - sizeof(sirinet_pkg_t);
    pkg->checkbit = 0;  /* check bit will be set when send */

    /* Free the packer, not the buffer */
    free(packer);

    return pkg;
}


/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (do not forget to run free(...) on the result. )
 */
sirinet_pkg_t * sirinet_pkg_err(
        uint16_t pid,
        uint32_t len,
        uint8_t tp,
        const char * data)
{
#ifdef DEBUG
    assert (data != NULL);
#endif

    sirinet_pkg_t * pkg;
    qp_packer_t * packer = sirinet_packer_new(len + 20 + sizeof(sirinet_pkg_t));
    if (packer == NULL)
    {
        pkg = NULL;  /* signal is raised */
    }
    else
    {
        qp_add_type(packer, QP_MAP_OPEN);
        qp_add_raw(packer, "error_msg", 9);
        qp_add_raw(packer, data, len);
        pkg = sirinet_packer2pkg(packer, pid, tp);
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
        free(pkg);
        return -1;
    }

    pkg_send_t * data = (pkg_send_t *) malloc(sizeof(pkg_send_t));

    if (data == NULL)
    {
        ERR_ALLOC
        free(pkg);
        free(req);
        return -1;
    }

    /* increment client reference counter */
    sirinet_socket_incref(client);

    data->client = client;
    data->pkg = pkg;
    req->data = data;

    /* set the correct check bit */
    pkg->checkbit = pkg->tp ^ 255;

    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            sizeof(sirinet_pkg_t) + pkg->len);

    uv_write(req, client, &wrbuf, 1, PKG_write_cb);

    return 0;
}

/*
 * Returns a copy of package allocated using malloc().
 * In case of an error, NULL is returned and a signal is raised.
 */
sirinet_pkg_t * sirinet_pkg_dup(sirinet_pkg_t * pkg)
{
    size_t size = sizeof(sirinet_pkg_t) + pkg->len;
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

    pkg_send_t * data = (pkg_send_t *) req->data;

    sirinet_socket_decref(data->client);

    free(data->pkg);
    free(data);
    free(req);
}
