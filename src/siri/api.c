/*
 * api.c
 */
#include <siri/api.h>
#include <siri/siri.h>

#define API__HEADER_MAX_SZ 256
#define CONTENT_TYPE_JSON "application/json"

static uv_tcp_t api__uv_server;
static http_parser_settings api__settings;

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

static inline _Bool api__starts_with(
        const char ** str,
        size_t * strn,
        const char * with,
        size_t withn)
{
    if (*strn < withn || strncasecmp(*str, with, withn))
        return false;
    *str += withn;
    *strn -= withn;
    return true;
}

static void api__close_cb(uv_handle_t * handle)
{
    siri_api_request_t * ar = handle->data;
    if (ar->siridb)
        siridb_decref(ar->siridb);
    if (ar->user)
        siridb_user_decref(ar->user);
    free(ar->content);
    free(ar);
}

static void api__alloc_cb(
        uv_handle_t * UNUSED(handle),
        size_t UNUSED(sugsz),
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

    if (ar->flags & SIRIDB_API_FLAG_IS_CLOSED)
        goto done;

    if (n < 0)
    {
        if (n != UV_EOF)
            log_error(uv_strerror(n));

        ti_api_close(ar);
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
        ti_api_close(ar);
    }

done:
     free(buf->base);
}

static int api__headers_complete_cb(http_parser * parser)
{
    siri_api_request_t * ar = parser->data;

    assert (!ar->content);

    ar->content = malloc(parser->content_length);
    if (ar->content)
        ar->content_n = parser->content_length;

    return 0;
}

static int api__url_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;



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

    (void) uv_tcp_init(siri.loop, (uv_tcp_t *) &ar->uvstream);

    ar->flags |= SIRIDB_API_FLAG;
    ar->uvstream.data = ar;
    ar->parser.data = ar;

    rc = uv_accept(server, &ar->uvstream);
    if (rc)
    {
        log_error("cannot accept HTTP API request: `%s`", uv_strerror(rc));
        ti_api_close(ar);
        return;
    }

    http_parser_init(&ar->parser, HTTP_REQUEST);

    rc = uv_read_start(&ar->uvstream, api__alloc_cb, api__data_cb);
    if (rc)
    {
        log_error("cannot read HTTP API request: `%s`", uv_strerror(rc));
        ti_api_close(ar);
        return;
    }
}

static int api__header_field_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;

    if (API__ICMP_WITH(at, n, "content-type"))
    {
        ar->state = SIRIDB_API_STATE_CONTENT_TYPE;
        return 0;
    }

    if (API__ICMP_WITH(at, n, "authorization"))
    {
        ar->state = SIRIDB_API_STATE_AUTHORIZATION;
        return 0;
    }

    ar->state = SIRIDB_API_STATE_NONE;
    return 0;
}

static int api__header_value_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;

    switch (ar->state)
    {
    case SIRIDB_API_STATE_NONE:
        break;

    case SIRIDB_API_STATE_CONTENT_TYPE:
        if (API__ICMP_WITH(at, n, CONTENT_TYPE_JSON))
        {
            ar->content_type = SIRIDB_API_CT_JSON;
            break;
        }
        if (API__ICMP_WITH(at, n, "text/json"))
        {
            ar->content_type = SIRIDB_API_CT_JSON;
            break;
        }

        /* invalid content type */
        log_debug("unsupported content-type: %.*s", (int) n, at);
        break;

    case SIRIDB_API_STATE_AUTHORIZATION:
        if (api__starts_with(&at, &n, "token ", strlen("token ")))
        {
            log_debug("token authorization is not supported yet");
        }

        if (api__starts_with(&at, &n, "basic ", strlen("basic ")))
        {
            ar->user = siridb_users_get_user_from_basic(ar->siridb, at, n);

            if (ar->user)
                siridb_user_incref(ar->user);

            break;
        }

        log_debug("invalid authorization type: %.*s", (int) n, at);
        break;
    }
    return 0;
}

static int api__body_cb(http_parser * parser, const char * at, size_t n)
{
    size_t offset;
    siri_api_request_t * ar = parser->data;

    if (!n || !ar->content_n)
        return 0;

    offset = ar->content_n - (parser->content_length + n);
    assert (offset + n <= ar->content_n);
    memcpy(ar->content + offset, at, n);

    return 0;
}

static void api__write_cb(uv_write_t * req, int status)
{
    if (status)
        log_error(
                "error writing HTTP API response: `%s`",
                uv_strerror(status));

    ti_api_close((siri_api_request_t *) req->handle->data);
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

        (void) uv_write(
                &ar->req,
                &ar->uvstream,
                uvbufs, 2,
                api__write_cb);
        return 0;
    }
    return -1;
}

static int api__message_complete_cb(http_parser * parser)
{
    siri_api_request_t * ar = parser->data;

    if (!ar->user)
        return api__plain_response(ar, E401_UNAUTHORIZED);

    switch (ar->content_type)
    {
    case SIRIDB_API_CT_JSON:
    {
        char * data;
        size_t size;
        if (qpjson_json_to_qp(ar->content, ar->content_n, &data, &size))
            return api__plain_response(ar, E400_BAD_REQUEST);

        free(ar->content);
        ar->content = data;
        ar->content_n = size;
    }
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

siri_api_request_t * siri_api_acquire(siri_api_request_t * ar)
{
    ar->flags |= SIRIDB_API_FLAG_IN_USE;
    return ar;
}

void ti_api_release(siri_api_request_t * ar)
{
    ar->flags &= ~SIRIDB_API_FLAG_IN_USE;

    if (ar->flags & SIRIDB_API_FLAG_IS_CLOSED)
        uv_close((uv_handle_t *) &ar->uvstream, api__close_cb);
}

void ti_api_close(siri_api_request_t * ar)
{
    if (!ar || (ar->flags & SIRIDB_API_FLAG_IS_CLOSED))
        return;

    ar->flags |= SIRIDB_API_FLAG_IS_CLOSED;

    if (ar->flags & SIRIDB_API_FLAG_IN_USE)
        return;

    uv_close((uv_handle_t *) &ar->uvstream, api__close_cb);
}
