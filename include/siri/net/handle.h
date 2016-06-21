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
#include <siri/net/pkg.h>

typedef struct siridb_s siridb_t;
typedef struct siridb_user_s siridb_user_t;

typedef void (* on_data_cb)(uv_handle_t * client, const sirinet_pkg_t * pkg);

typedef struct sirinet_handle_s
{
    on_data_cb on_data;
    siridb_t * siridb;
    void * origin;  /* can be a user, server or NULL */
    char * buf;
    size_t len;
} sirinet_handle_t;

sirinet_handle_t * sirinet_handle_new(on_data_cb cb);
void sirinew_handle_free(sirinet_handle_t * sn_handle);

void sirinet_handle_alloc_buffer(
        uv_handle_t * handle,
        size_t suggested_size,
        uv_buf_t * buf);

void sirinet_handle_on_data(
        uv_stream_t * client,
        ssize_t nread,
        const uv_buf_t * buf);


void sirinet_free_client(uv_handle_t * client);
void sirinet_free_async(uv_handle_t * handle);
