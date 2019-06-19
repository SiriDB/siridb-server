/*
 * health.h
 */
#include <siri/health.h>
#include <siri/siri.h>
#include <logger/logger.h>

#define OK_RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 3\r\n" \
    "\r\n" \
    "OK\n"

#define NOK_RESPONSE \
    "HTTP/1.1 503 Service Unavailable\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 4\r\n" \
    "\r\n" \
    "NOK\n"

#define NFOUND_RESPONSE \
    "HTTP/1.1 404 Not Found\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 10\r\n" \
    "\r\n" \
    "NOT FOUND\n"

#define SYNC_RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 14\r\n" \
    "\r\n" \
    "SYNCHRONIZING\n"

#define REIDX_RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 11\r\n" \
    "\r\n" \
    "REINDEXING\n"

#define READY_RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 6\r\n" \
    "\r\n" \
    "READY\n"

/* static response buffers */
static uv_buf_t web__uv_ok_buf;
static uv_buf_t web__uv_nok_buf;
static uv_buf_t web__uv_nfound_buf;
static uv_buf_t web__uv_sync_buf;
static uv_buf_t web__uv_reidx_buf;
static uv_buf_t web__uv_ready_buf;

static uv_tcp_t web__uv_server;
static http_parser_settings web__settings;

static void web__close_cb(uv_handle_t * handle)
{
    siri_health_request_t * web_request = handle->data;
    free(web_request);
}

static void web__alloc_cb(
        uv_handle_t * handle __attribute__((unused)),
        size_t sugsz __attribute__((unused)),
        uv_buf_t * buf)
{
    buf->base = malloc(HTTP_MAX_HEADER_SIZE);
    buf->len = buf->base ? HTTP_MAX_HEADER_SIZE : 0;
}

static void web__data_cb(
        uv_stream_t * uvstream,
        ssize_t n,
        const uv_buf_t * buf)
{
    size_t parsed;
    siri_health_request_t * web_request = uvstream->data;

    if (web_request->is_closed)
        goto done;

    if (n < 0)
    {
        if (n != UV_EOF)
        {
            log_error(uv_strerror(n));
        }
        siri_health_close(web_request);
        goto done;
    }

    buf->base[HTTP_MAX_HEADER_SIZE-1] = '\0';

    parsed = http_parser_execute(
            &web_request->parser,
            &web__settings,
            buf->base, n);

    if (web_request->parser.upgrade)
    {
        /* TODO: do we need to do something? */
        log_debug("upgrade to a new protocol");
    }
    else if (parsed != (size_t) n)
    {
        log_warning("error parsing HTTP request");
        siri_health_close(web_request);
    }

done:
     free(buf->base);
}

static uv_buf_t * web__get_status_response(void)
{
    siridb_t * siridb;
    uint8_t flags = 0;
    llist_node_t * siridb_node;

    siridb_node = siri.siridb_list->first;
    while (siridb_node != NULL)
    {
        siridb = (siridb_t *) siridb_node->data;
        flags |= siridb->server->flags;
        siridb_node = siridb_node->next;
    }

    if (flags & SERVER_FLAG_SYNCHRONIZING)
    {
        return &web__uv_sync_buf;
    }
    if (flags & SERVER_FLAG_REINDEXING)
    {
        return &web__uv_sync_buf;
    }
    return &web__uv_nok_buf;
}

static uv_buf_t * web__get_ready_response(void)
{
    siridb_t * siridb;
    uint8_t flags = 0;
    llist_node_t * siridb_node;

    siridb_node = siri.siridb_list->first;
    while (siridb_node != NULL)
    {
        siridb = (siridb_t *) siridb_node->data;
        flags |= siridb->server->flags;
        siridb_node = siridb_node->next;
    }

    return flags == SERVER_FLAG_RUNNING ? &web__uv_ok_buf : &web__uv_nok_buf;
}

static int web__url_cb(http_parser * parser, const char * at, size_t length)
{
    siri_health_request_t * web_request = parser->data;

    web_request->response

        /* status response */
        = ((length == 1 && *at == '/') ||
           (length == 7 && memcmp(at, "/status", 7) == 0))
        ? web__get_status_response()

        /* ready response */
        : (length == 6 && memcmp(at, "/ready", 6) == 0)
        ? web__get_ready_response()

        /* healthy response */
        : (length == 8 && memcmp(at, "/healthy", 8) == 0)
        ? &web__uv_ok_buf

        /* everything else */
        : &web__uv_nfound_buf;

    return 0;
}

static void web__write_cb(uv_write_t * req, int status)
{
    if (status)
        log_error("error writing HTTP response: `%s`", uv_strerror(status));

    siri_health_close((siri_health_request_t *) req->handle->data);
}

static int web__message_complete_cb(http_parser * parser)
{
    siri_health_request_t * web_request = parser->data;

    if (!web_request->response)
        web_request->response = &web__uv_nfound_buf;

    (void) uv_write(
            &web_request->req,
            &web_request->uvstream,
            web_request->response, 1,
            web__write_cb);

    return 0;
}

static void web__connection_cb(uv_stream_t * server, int status)
{
    int rc;
    siri_health_request_t * web_request;

    if (status < 0)
    {
        log_error("HTTP connection error: `%s`", uv_strerror(status));
        return;
    }

    log_debug("received a HTTP status connection request");

    web_request = malloc(sizeof(siri_health_request_t));
    if (!web_request)
    {
        ERR_ALLOC
        return;
    }

    (void) uv_tcp_init(siri.loop, (uv_tcp_t *) &web_request->uvstream);

    web_request->flags = SIRIDB_HEALTH_FLAG;
    web_request->is_closed = false;
    web_request->uvstream.data = web_request;
    web_request->parser.data = web_request;

    rc = uv_accept(server, &web_request->uvstream);
    if (rc)
    {
        log_error("cannot accept HTTP request: `%s`", uv_strerror(rc));
        siri_health_close(web_request);
        return;
    }

    http_parser_init(&web_request->parser, HTTP_REQUEST);

    rc = uv_read_start(&web_request->uvstream, web__alloc_cb, web__data_cb);
    if (rc)
    {
        log_error("cannot read HTTP request: `%s`", uv_strerror(rc));
        siri_health_close(web_request);
        return;
    }
}

int siri_health_init(void)
{
    int rc;
    struct sockaddr_storage addr = {0};
    uint16_t port = siri.cfg->http_status_port;

    (void) uv_ip6_addr("::", (int) port, (struct sockaddr_in6 *) &addr);

    web__uv_ok_buf =
            uv_buf_init(OK_RESPONSE, strlen(OK_RESPONSE));
    web__uv_nok_buf =
            uv_buf_init(NOK_RESPONSE, strlen(NOK_RESPONSE));
    web__uv_nfound_buf =
            uv_buf_init(NFOUND_RESPONSE, strlen(NFOUND_RESPONSE));
    web__uv_sync_buf =
            uv_buf_init(SYNC_RESPONSE, strlen(SYNC_RESPONSE));
    web__uv_reidx_buf =
            uv_buf_init(REIDX_RESPONSE, strlen(REIDX_RESPONSE));
    web__uv_ready_buf =
            uv_buf_init(READY_RESPONSE, strlen(READY_RESPONSE));

    web__settings.on_url = web__url_cb;
    web__settings.on_message_complete = web__message_complete_cb;

    if (
        (rc = uv_tcp_init(siri.loop, &web__uv_server)) ||
        (rc = uv_tcp_bind(
                &web__uv_server,
                (const struct sockaddr *) &addr,
                0)) ||
        (rc = uv_listen(
                (uv_stream_t *) &web__uv_server,
                128,
                web__connection_cb)))
    {
        log_error("error initializing HTTP status server on port %u: `%s`",
                port,
                uv_strerror(rc));
        return -1;
    }

    log_info("start listening for HTTP status requests on TCP port %u", port);
    return 0;
}

void siri_health_close(siri_health_request_t * web_request)
{
    if (!web_request || web_request->is_closed)
        return;
    web_request->is_closed = true;
    uv_close((uv_handle_t *) &web_request->uvstream, web__close_cb);
}
