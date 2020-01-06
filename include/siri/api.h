/*
 * api.h - SiriDB HTTP API.
 */
#ifndef SIRI_API_H_
#define SIRI_API_H_

#include <lib/http_parser.h>
#include <uv.h>

#define SIRIDB_API_FLAG 1<<29

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
    uint32_t ref_;  /* maps to sirnet_stream_t for cleanup */
    siridb_api_flags_t flags;
    siridb_api_state_t state;
    siridb_api_content_t content_type;
    size_t content_n;
    uv_write_t req;
    uv_stream_t uvstream;
    http_parser parser;
    char * content;
    siridb_t * siridb;
    siridb_user_t * user;
};

static inline _Bool siri_api_is_handle(uv_handle_t * handle)
{
    return
        handle->type == UV_TCP &&
        handle->data &&
        (((siri_api_request_t *) handle->data)->ref_ & SIRIDB_API_FLAG);
}

#endif /* SIRI_API_H_ */
