/*
 * health.h - SiriDB Health Status.
 */
#ifndef SIRI_HEALTH_H_
#define SIRI_HEALTH_H_

#include <lib/http_parser.h>
#include <stdbool.h>
#include <uv.h>

#define SIRIDB_HEALTH_FLAG 1<<30

typedef struct siri_health_request_s siri_health_request_t;

int siri_health_init(void);
void siri_health_close(siri_health_request_t * web_request);
static inline bool siri_health_is_handle(uv_handle_t * handle);

struct siri_health_request_s
{
    uint32_t flags;  /* maps to sirnet_stream_t tp for cleanup */
    uint32_t pad0_;
    bool is_closed;
    uv_write_t req;
    uv_stream_t uvstream;
    http_parser parser;
    uv_buf_t * response;
};

static inline bool siri_health_is_handle(uv_handle_t * handle)
{
    return
        handle->type == UV_TCP &&
        handle->data &&
        (((siri_health_request_t *) handle->data)->flags & SIRIDB_HEALTH_FLAG);
}

#endif /* TI_HEALTH_H_ */
