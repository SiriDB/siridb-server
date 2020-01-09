/*
 * api.c
 */
#include <siri/api.h>
#include <assert.h>
#include <siri/siri.h>
#include <siri/db/users.h>
#include <qpjson/qpjson.h>
#include <siri/db/query.h>

#define API__HEADER_MAX_SZ 256
#define CONTENT_TYPE_JSON "application/json"

static uv_tcp_t api__uv_server;
static http_parser_settings api__settings;

#define API__ICMP_WITH(__s, __n, __w) \
    __n == strlen(__w) && strncasecmp(__s, __w, __n) == 0

static const char api__content_type[2][20] = {
        "text/plain",
        "application/json",
};

static const char api__html_header[8][32] = {
        "200 OK",
        "400 Bad Request",
        "401 Unauthorized",
        "403 Forbidden",
        "404 Not Found",
        "415 Unsupported Media Type",
        "500 Internal Server Error",
        "503 Service Unavailable",
};

static const char api__default_body[8][30] = {
        "OK\r\n",
        "BAD REQUEST\r\n",
        "UNAUTHORIZED\r\n",
        "FORBIDDEN\r\n",
        "NOT FOUND\r\n",
        "UNSUPPORTED MEDIA TYPE\r\n",
        "INTERNAL SERVER ERROR\r\n",
        "SERVICE UNAVAILABLE\r\n",
};

typedef enum
{
    E200_OK,
    E400_BAD_REQUEST,
    E401_UNAUTHORIZED,
    E403_FORBIDDEN,
    E404_NOT_FOUND,
    E415_UNSUPPORTED_MEDIA_TYPE,
    E500_INTERNAL_SERVER_ERROR,
    E503_SERVICE_UNAVAILABLE
} api__header_t;

inline static int api__header(
        char * ptr,
        const api__header_t ht,
        const siridb_api_content_t ct,
        size_t content_length)
{
    int len = sprintf(
        ptr,
        "HTTP/1.1 %s\r\n" \
        "Content-Type: %s\r\n" \
        "Content-Length: %zu\r\n" \
        "\r\n",
        api__html_header[ht],
        api__content_type[ct],
        content_length);
    return len;
}

static inline _Bool api__starts_with(
        const char ** str,
        size_t * strn,
        const char * with,
        size_t withn)
{
    if (*strn < withn || strncasecmp(*str, with, withn))
    {
        return false;
    }
    *str += withn;
    *strn -= withn;
    return true;
}

static void api__alloc_cb(
        uv_handle_t * UNUSED_handle __attribute__((unused)),
        size_t UNUSED_sugsz __attribute__((unused)),
        uv_buf_t * buf)
{
    buf->base = malloc(HTTP_MAX_HEADER_SIZE);
    buf->len = buf->base ? HTTP_MAX_HEADER_SIZE-1 : 0;
}

static void api__data_cb(
        uv_stream_t * uvstream,
        ssize_t n,
        const uv_buf_t * buf)
{
    size_t parsed;
    siri_api_request_t * ar = uvstream->data;

    if (!ar->ref)
        goto done;

    if (n < 0)
    {
        if (n != UV_EOF)
            log_error(uv_strerror(n));

        sirinet_stream_decref(ar);
        goto done;
    }

    buf->base[HTTP_MAX_HEADER_SIZE-1] = '\0';

    parsed = http_parser_execute(
            &ar->parser,
            &api__settings,
            buf->base, n);

    if (ar->parser.upgrade)
    {
        /* TODO: do we need to do something? */
        log_debug("upgrade to a new protocol");
    }
    else if (parsed != (size_t) n)
    {
        log_warning("error parsing HTTP API request");
        sirinet_stream_decref(ar);
    }

done:
     free(buf->base);
}

static int api__headers_complete_cb(http_parser * parser)
{
    siri_api_request_t * ar = parser->data;

    assert (!ar->buf);

    ar->buf = malloc(parser->content_length);
    if (ar->len)
    {
        ar->len = parser->content_length;
    }
    return 0;
}

static int api__url_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;

    if (api__starts_with(&at, &n, "/query/", strlen("/query/")))
    {
        ar->siridb = siridb_getn(siri.siridb_list, at, n);
        if (ar->siridb)
        {
            siridb_incref(ar->siridb);
        }
    }

    return 0;
}

static void api__connection_cb(uv_stream_t * server, int status)
{
    int rc;
    siri_api_request_t * ar;

    if (status < 0)
    {
        log_error("HTTP API connection error: `%s`", uv_strerror(status));
        return;
    }

    log_debug("received a HTTP API call");

    ar = calloc(1, sizeof(siri_api_request_t));
    if (!ar)
    {
        ERR_ALLOC
        return;
    }

    ar->stream = malloc(sizeof(uv_tcp_t));
    if (!ar->stream)
    {
        free(ar);
        ERR_ALLOC
        return;
    }

    ar->tp = STREAM_API_CLIENT;
    ar->ref = 1;
    ar->on_state = NULL;

    (void) uv_tcp_init(siri.loop, (uv_tcp_t *) ar->stream);

    ar->stream->data = ar;
    ar->parser.data = ar;

    rc = uv_accept(server, ar->stream);
    if (rc)
    {
        log_error("cannot accept HTTP API request: `%s`", uv_strerror(rc));
        sirinet_stream_decref(ar);
        return;
    }

    http_parser_init(&ar->parser, HTTP_REQUEST);

    rc = uv_read_start(ar->stream, api__alloc_cb, api__data_cb);
    if (rc)
    {
        log_error("cannot read HTTP API request: `%s`", uv_strerror(rc));
        sirinet_stream_decref(ar);
        return;
    }
}

static int api__on_content_type(siri_api_request_t * ar, const char * at, size_t n)
{
    if (API__ICMP_WITH(at, n, CONTENT_TYPE_JSON))
    {
        ar->content_type = SIRIDB_API_CT_JSON;
        return 0;
    }

    if (API__ICMP_WITH(at, n, "text/json"))
    {
        ar->content_type = SIRIDB_API_CT_JSON;
        return 0;
    }

    /* invalid content type */
    log_debug("unsupported content-type: %.*s", (int) n, at);
    return 0;
}

static int api__on_authorization(siri_api_request_t * ar, const char * at, size_t n)
{
    if (api__starts_with(&at, &n, "token ", strlen("token ")))
    {
        log_debug("token authorization is not supported yet");
    }

    if (api__starts_with(&at, &n, "basic ", strlen("basic ")))
    {
        siridb_user_t * user;
        user = siridb_users_get_user_from_basic(ar->siridb, at, n);

        if (user)
        {
            siridb_user_incref(user);
            ar->origin = user;
        }
        return 0;
    }

    log_debug("invalid authorization type: %.*s", (int) n, at);
    return 0;
}
static int api__header_value_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;
    return ar->on_state ? ar->on_state(ar, at, n) : 0;
}

static int api__header_field_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;

    ar->on_state = API__ICMP_WITH(at, n, "content-type")
            ? api__on_content_type
            : API__ICMP_WITH(at, n, "authorization")
            ? api__on_authorization
            : NULL;
    return 0;
}

static int api__body_cb(http_parser * parser, const char * at, size_t n)
{
    size_t offset;
    siri_api_request_t * ar = parser->data;

    if (!n || !ar->len)
        return 0;

    offset = ar->len - (parser->content_length + n);
    assert (offset + n <= ar->len);
    memcpy(ar->buf + offset, at, n);

    return 0;
}

static void api__write_cb(uv_write_t * req, int status)
{
    if (status)
        log_error(
                "error writing HTTP API response: `%s`",
                uv_strerror(status));

    sirinet_stream_decref((siri_api_request_t *) req->handle->data);
}

static int api__plain_response(siri_api_request_t * ar, const api__header_t ht)
{
    const char * body = api__default_body[ht];
    char header[API__HEADER_MAX_SZ];
    size_t body_size;
    int header_size;

    body_size = strlen(body);
    header_size = api__header(header, ht, SIRIDB_API_CT_TEXT, body_size);

    if (header_size > 0)
    {
        uv_buf_t uvbufs[2] = {
                uv_buf_init(header, (size_t) header_size),
                uv_buf_init((char *) body, body_size),
        };

        (void) uv_write(&ar->req, ar->stream, uvbufs, 2, api__write_cb);
        return 0;
    }
    return -1;
}

static int api__query(siri_api_request_t * ar)
{
    const char q[100] = "select * from 'aggr'";

    siridb_query_run(
                    0,
                    (sirinet_stream_t *) ar,
                    q, strlen(q),
                    0.0,
                    SIRIDB_QUERY_FLAG_MASTER);
    return 0;
}

static int api__message_complete_cb(http_parser * parser)
{
    siri_api_request_t * ar = parser->data;

    if (!ar->siridb)
        return api__plain_response(ar, E404_NOT_FOUND);

    if (!ar->origin)
        return api__plain_response(ar, E401_UNAUTHORIZED);

    switch (ar->content_type)
    {
    case SIRIDB_API_CT_TEXT:
        /* Or, shall we allow text and return we some sort of CSV format? */
        return api__plain_response(ar, E400_BAD_REQUEST);
    case SIRIDB_API_CT_JSON:
        return api__query(ar);
    }

    return api__plain_response(ar, E500_INTERNAL_SERVER_ERROR);
}

static int api__chunk_header_cb(http_parser * parser)
{
    LOGC("Chunk header\n Content-Length: %zu", parser->content_length);
    return 0;
}

static int api__chunk_complete_cb(http_parser * parser)
{
    LOGC("Chunk complete\n Content-Length: %zu", parser->content_length);
    return 0;
}

static void api__write_free_cb(uv_write_t * req, int status)
{
    free(req->data);
    api__write_cb(req, status);
}

static int api__close_resp(siri_api_request_t * ar, void * data, size_t size)
{
    char header[API__HEADER_MAX_SZ];
    int header_size = 0;

    header_size = api__header(header, E200_OK, ar->content_type, size);

    uv_buf_t uvbufs[2] = {
            uv_buf_init(header, (unsigned int) header_size),
            uv_buf_init(data, size),
    };

    /* bind response to request to we can free in the callback */
    ar->req.data = data;

    uv_write(&ar->req, ar->stream, uvbufs, 2, api__write_free_cb);
    return 0;
}

int siri_api_init(void)
{
    int rc;
    struct sockaddr_storage addr = {0};
    uint16_t port = siri.cfg->http_api_port;

    if (port == 0)
        return 0;

    (void) uv_ip6_addr("::", (int) port, (struct sockaddr_in6 *) &addr);

    api__settings.on_url = api__url_cb;
    api__settings.on_header_field = api__header_field_cb;
    api__settings.on_header_value = api__header_value_cb;
    api__settings.on_message_complete = api__message_complete_cb;
    api__settings.on_body = api__body_cb;
    api__settings.on_chunk_header = api__chunk_header_cb;
    api__settings.on_chunk_complete = api__chunk_complete_cb;
    api__settings.on_headers_complete = api__headers_complete_cb;

    if (
        (rc = uv_tcp_init(siri.loop, &api__uv_server)) ||
        (rc = uv_tcp_bind(
                &api__uv_server,
                (const struct sockaddr *) &addr,
                0)) ||
        (rc = uv_listen(
                (uv_stream_t *) &api__uv_server,
                128,
                api__connection_cb)))
    {
        log_error("error initializing HTTP API on port %u: `%s`",
                port,
                uv_strerror(rc));
        return -1;
    }

    log_info("start listening for HTTP API requests on TCP port %u", port);
    return 0;
}

int siri_api_send(siri_api_request_t * ar, unsigned char * src, size_t n)
{
    unsigned char * data;
    if (ar->content_type == SIRIDB_API_CT_JSON)
    {
        size_t tmp_sz;
        yajl_gen_status stat = qpjson_qp_to_json(
                src,
                n,
                &data,
                &tmp_sz,
                0);
        if (stat)
        {
            // api__set_yajl_gen_status_error(&ar->e, stat);
            // return ti_api_close_with_err(ar, &ar->e);
            // TODO : return error
            LOGC("HERE: %d", stat);
        }
        n = tmp_sz;
    }
    else
    {
        data = malloc(n);
        if (data == NULL)
        {
            // TODO : return error
        }
        memcpy(data, src, n);
    }
    return api__close_resp(ar, data, n);
}
