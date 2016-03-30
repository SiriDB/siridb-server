/*
 * handle.h - Handle TCP request.
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

#define SN_PKG_HEADER_SIZE 14

struct siridb_s;

enum {
    SN_HANDLE_SUCCESS,
    SN_HANDLE_ERR_MSGPACK_ERROR,
};

typedef struct sirinet_pkg_s
{
    uint64_t pid;
    uint32_t len;
    uint16_t tp;
    char data[];
} sirinet_pkg_t;

typedef void (* on_data_t)(uv_handle_t * client, const sirinet_pkg_t * pkg);

typedef struct sirinet_handle_s
{
    struct siridb_s * siridb;
    struct siridb_user_s * user;  /* can be NULL for back-end */
    on_data_t on_data;
} sirinet_handle_t;

void sirinet_handle_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf);

void sirinet_handle_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf);

/* do not forget to run free(pkg) when using sirinet_new_pkg */
sirinet_pkg_t * sirinet_new_pkg(
        uint64_t pid,
        uint32_t len,
        uint16_t tp,
        const char * data);

void sirinet_send_pkg(
        uv_handle_t * client,
        sirinet_pkg_t * pkg,
        uv_write_cb cb);

void sirinet_free_client(uv_handle_t * client);

void sirinet_free_async(uv_handle_t * handle);
