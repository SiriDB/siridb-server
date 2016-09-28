/*
 * socket.h - Handle TCP request.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/db/db.h>
#include <siri/net/pkg.h>

typedef enum sirinet_socket_tp
{
    SOCKET_CLIENT,
    SOCKET_BACKEND,
    SOCKET_SERVER
} sirinet_socket_tp_t;

typedef struct siridb_s siridb_t;
typedef struct siridb_user_s siridb_user_t;

typedef void (* on_data_cb_t)(uv_stream_t * client, sirinet_pkg_t * pkg);

typedef struct sirinet_socket_s
{
    sirinet_socket_tp_t tp;
    uint64_t ref;
    on_data_cb_t on_data;
    siridb_t * siridb;
    void * origin;  /* can be a user, server or NULL */
    char * buf;
    size_t len;
} sirinet_socket_t;

uv_tcp_t * sirinet_socket_new(int tp, on_data_cb_t cb);
void sirinet_socket_incref(uv_stream_t * client);
void sirinet_socket_decref(uv_stream_t * client);
void sirinet_socket_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf);

void sirinet_socket_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf);

