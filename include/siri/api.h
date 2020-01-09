/*
 * api.h - SiriDB HTTP API.
 */
#ifndef SIRI_API_H_
#define SIRI_API_H_

#include <lib/http_parser.h>
#include <uv.h>
#include <siri/db/db.h>

typedef enum
{
    SIRIDB_API_CT_TEXT,
    SIRIDB_API_CT_JSON,
} siridb_api_content_t;

typedef struct siri_api_request_s siri_api_request_t;

typedef int (*on_state_cb_t)(siri_api_request_t * ar, const char * at, size_t n);

int siri_api_init(void);
int siri_api_send(siri_api_request_t * ar, unsigned char * src, size_t n);

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
    siridb_api_content_t content_type;
    http_parser parser;
    uv_write_t req;
};

#endif /* SIRI_API_H_ */
