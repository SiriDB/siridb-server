/*
 * stream.h - For handling streams.
 */
#ifndef SIRINET_STREAM_H_
#define SIRINET_STREAM_H_

#define RESET_BUF_SIZE 1048576  /*  1 MB        */

typedef enum
{
    STREAM_TCP_CLIENT,
    STREAM_TCP_BACKEND,
    STREAM_TCP_SERVER,
    STREAM_TCP_MANAGE,
    STREAM_PIPE_CLIENT,
} sirinet_stream_tp_t;

typedef struct sirinet_stream_s sirinet_stream_t;

#include <uv.h>
#include <siri/db/db.h>
#include <siri/net/pkg.h>

typedef void (* on_data_cb_t)(sirinet_stream_t * stream, sirinet_pkg_t * pkg);

sirinet_stream_t * sirinet_stream_new(sirinet_stream_tp_t tp, on_data_cb_t cb);
char * sirinet_stream_name(sirinet_stream_t * client);
void sirinet_stream_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf);
void sirinet_stream_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf);
void sirinet__stream_free(uv_stream_t * uvclient);

#define sirinet_stream_incref(client) \
    (client)->ref++

#define sirinet_stream_decref(client)               \
    if (!--(client)->ref) uv_close(                 \
        (uv_handle_t *) (client)->stream,           \
        (uv_close_cb) sirinet__stream_free)

#define sirinet_stream_is_pipe(client)      \
    ((client)->tp == STREAM_PIPE_CLIENT)

struct sirinet_stream_s
{
    sirinet_stream_tp_t tp;
    uint32_t ref;
    on_data_cb_t on_data;
    siridb_t * siridb;
    void * origin;  /* can be a user, server or NULL */
    char * buf;
    size_t len;
    size_t size;
    uv_stream_t * stream;
};

#endif  /* SIRINET_STREAM_H_ */
