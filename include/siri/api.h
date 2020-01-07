/*
 * api.h - SiriDB HTTP API.
 */
#ifndef SIRI_API_H_
#define SIRI_API_H_

#include <lib/http_parser.h>
#include <uv.h>


typedef enum
{
    SIRIDB_API_STATE_NONE,
    SIRIDB_API_STATE_CONTENT_TYPE,
    SIRIDB_API_STATE_AUTHORIZATION,
} siridb_api_state_t;

typedef enum
{
    SIRIDB_API_CT_TEXT,
    SIRIDB_API_CT_JSON,
} siridb_api_content_t;

typedef enum
{
    SIRIDB_API_FLAG_IS_CLOSED       =1<<0,
    SIRIDB_API_FLAG_IN_USE          =1<<1,
    SIRIDB_API_FLAG_JSON_BEAUTY     =1<<2,
    SIRIDB_API_FLAG_JSON_UTF8       =1<<3,
} siridb_api_flags_t;

typedef struct siri_api_request_s siri_api_request_t;

int siri_api_init(void);
void siri_ali_close(siri_api_request_t * web_request);
static inline _Bool siri_api_is_handle(uv_handle_t * handle);

struct siri_api_request_s
{
    uint32_t tp;        /* maps to siridb_tee_t flags for cleanup */
    uint32_t ref;
    on_data_cb_t on_data;
    siridb_t * siridb;
    void * origin;      /* can be a user, server or NULL */
    char * buf;
    size_t len;
    size_t size;
    uv_stream_t * stream;
    siridb_api_flags_t flags;
    siridb_api_state_t state;
    siridb_api_content_t content_type;
    http_parser parser;
};

static inline _Bool siri_api_is_handle(uv_handle_t * handle)
{
    return
        handle->type == UV_TCP &&
        handle->data &&
        (((siri_api_request_t *) handle->data)->ref_ & SIRIDB_API_FLAG);
}

#endif /* SIRI_API_H_ */
