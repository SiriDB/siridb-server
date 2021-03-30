/*
 * api.h - SiriDB HTTP API.
 */
#ifndef SIRI_API_H_
#define SIRI_API_H_

#include <lib/http_parser.h>
#include <siri/db/db.h>
#include <siri/service/request.h>
#include <stdbool.h>
#include <uv.h>

typedef enum
{
    SIRI_API_CT_TEXT,
    SIRI_API_CT_JSON,
    SIRI_API_CT_QPACK,
} siri_api_content_t;

typedef enum
{
    SIRI_API_RT_NONE,
    SIRI_API_RT_QUERY,
    SIRI_API_RT_INSERT,
    SIRI_APT_RT_SERVICE,
} siri_api_req_t;

typedef enum
{
    E200_OK,
    E400_BAD_REQUEST,
    E401_UNAUTHORIZED,
    E403_FORBIDDEN,
    E404_NOT_FOUND,
    E405_METHOD_NOT_ALLOWED,
    E415_UNSUPPORTED_MEDIA_TYPE,
    E422_UNPROCESSABLE_ENTITY,
    E500_INTERNAL_SERVER_ERROR,
    E503_SERVICE_UNAVAILABLE
} siri_api_header_t;

typedef struct siri_api_request_s siri_api_request_t;

typedef int (*on_state_cb_t)(siri_api_request_t * ar, const char * at, size_t n);

int siri_api_init(void);
int siri_api_send(
        siri_api_request_t * ar,
        siri_api_header_t ht,
        unsigned char * src,
        size_t n);

struct siri_api_request_s
{
    uint32_t tp;        /* maps to siridb_tee_t flags for cleanup */
    uint32_t ref;
    on_state_cb_t on_state;
    siridb_t * siridb;
    void * origin;      /* can be a user, server or NULL */
    char * buf;
    size_t len;
    size_t size;
    uv_stream_t * stream;
    siri_api_content_t content_type;
    siri_api_req_t request_type;
    service_request_t service_type;
    bool service_authenticated;
    http_parser parser;
    uv_write_t req;
};

#endif /* SIRI_API_H_ */
