#pragma once

#include <uv.h>
#include <siri/db/db.h>
#include <siri/net/pkg.h>
#include <xpath/xpath.h>

#define PIPE_NAME_SZ SIRI_PATH_MAX
#define RESET_BUF_SIZE 1048576  /*  1 MB        */

typedef enum sirinet_pipe_tp
{
    PIPE_CLIENT,
    PIPE_BACKEND
} sirinet_pipe_tp_t;

typedef struct siridb_s siridb_t;
typedef struct siridb_user_s siridb_user_t;

typedef void (* on_data_cb_t)(uv_stream_t * client, sirinet_pkg_t * pkg);
typedef void (* on_free_cb_t)(uv_stream_t * client);

typedef struct sirinet_pipe_s
{
    sirinet_pipe_tp_t tp;
    uint32_t ref;
    on_data_cb_t on_data;
    on_free_cb_t on_free;
    siridb_t * siridb;
    void * origin;  /* can be a user, server or NULL */
    char * buf;
    size_t len;
    size_t size;
    uv_pipe_t pipe;
} sirinet_pipe_t;

uv_pipe_t * sirinet_pipe_new(
        sirinet_pipe_tp_t tp,
        on_data_cb_t cb_data,
        on_free_cb_t cb_free);
void sirinet_pipe_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf);
int sirinet_pipe_name(char * buffer, uv_stream_t * client);
void sirinet_pipe_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf);
void sirinet__pipe_free(uv_stream_t * client);

#define sirinet_pipe_incref(client) \
    ((sirinet_pipe_t *) client->data)->ref++

#define sirinet_pipe_decref(client) \
    if (!--((sirinet_pipe_t *) client->data)->ref) \
        uv_close((uv_handle_t *) client, (uv_close_cb) sirinet__pipe_free)
