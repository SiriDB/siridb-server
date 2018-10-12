/*
 * async.h - SiriDB async wrapper.
 */
#ifndef SIRI_ASYC_H_
#define SIRI_ASYC_H_

typedef struct siri_async_s siri_async_t;

#include <uv.h>
#include <inttypes.h>

void siri_async_close(uv_handle_t * handle);
void siri_async_decref(uv_async_t ** handle);

#define siri_async_incref(HANDLE__) ((siri_async_t *) HANDLE__->data)->ref++

struct siri_async_s
{
    uv_close_cb free_cb;
    uint8_t ref;
};

#endif  /* SIRI_ASYC_H_ */
