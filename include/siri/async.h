/*
 * async.h - SiriDB async wrapper
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 21-07-2016
 *
 */
#pragma once

#include <uv.h>
#include <inttypes.h>

typedef struct siri_async_s
{
    uv_close_cb free_cb;
    uint8_t ref;
} siri_async_t;

void siri_async_incref(uv_async_t * handle);
void siri_async_close(uv_handle_t * handle);
void siri_async_decref(uv_async_t ** handle);
