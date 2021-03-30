/*
 * api.c
 */
#include <assert.h>
#include <math.h>
#include <siri/api.h>
#include <stdbool.h>
#include <string.h>
#include <siri/siri.h>
#include <siri/db/users.h>
#include <qpjson/qpjson.h>
#include <siri/db/query.h>
#include <siri/db/insert.h>
#include <siri/service/account.h>
#include <siri/net/tcp.h>

#define API__HEADER_MAX_SZ 256

static uv_tcp_t api__uv_server;
static http_parser_settings api__settings;

typedef struct
{
    char * query;
    size_t query_n;
    double factor;
} api__query_t;

#define API__ICMP_WITH(__s, __n, __w) \
    (__n == strlen(__w) && strncasecmp(__s, __w, __n) == 0)

#define API__CMP_WITH(__s, __n, __w) \
    (__n == strlen(__w) && strncmp(__s, __w, __n) == 0)

static const char api__content_type[3][20] = {
        "text/plain",
        "application/json",
        "application/qpack",
};

static const char api__html_header[10][32] = {
        "200 OK",
        "400 Bad Request",
        "401 Unauthorized",
        "403 Forbidden",
        "404 Not Found",
        "405 Method Not Allowed",
        "415 Unsupported Media Type",
        "422 Unprocessable Entity",
        "500 Internal Server Error",
        "503 Service Unavailable",
};

static const char api__default_body[10][30] = {
        "OK\r\n",
        "BAD REQUEST\r\n",
        "UNAUTHORIZED\r\n",
        "FORBIDDEN\r\n",
        "NOT FOUND\r\n",
        "METHOD NOT ALLOWED\r\n",
        "UNSUPPORTED MEDIA TYPE\r\n",
        "UNPROCESSABLE ENTITY\r\n",
        "INTERNAL SERVER ERROR\r\n",
        "SERVICE UNAVAILABLE\r\n",
};


inline static int api__header(
        char * ptr,
        const siri_api_header_t ht,
        const siri_api_content_t ct,
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

static inline bool api__istarts_with(
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

static inline bool api__starts_with(
        const char ** str,
        size_t * strn,
        const char * with,
        size_t withn)
{
    if (*strn < withn || strncmp(*str, with, withn))
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

static void api__reset(siri_api_request_t * ar)
{
    /* Reset buffer in case multiple HTTP requests are used */
    free (ar->buf);

    if (ar->siridb)
    {
        siridb_decref(ar->siridb);
        ar->siridb = NULL;
    }

    if (ar->origin)
    {
        siridb_user_decref((siridb_user_t *) ar->origin);
        ar->origin = NULL;
    }

    ar->buf = NULL;
    ar->len = 0;
    ar->size = 0;
    ar->on_state = NULL;
    ar->service_authenticated = 0;
    ar->request_type = SIRI_API_RT_NONE;
    ar->content_type = SIRI_API_CT_TEXT;
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

    if (parser->content_length != ULLONG_MAX)
    {
        ar->buf = malloc(parser->content_length);
        if (ar->buf)
        {
            ar->len = parser->content_length;
        }
    }

    return 0;
}

static void api__get_siridb(siri_api_request_t * ar, const char * at, size_t n)
{
    const char * pt = at;
    size_t nn = 0;

    while (n && *pt != '?')
    {
        ++pt;
        --n;
        ++nn;
    }
    ar->siridb = siridb_getn(siri.siridb_list, at, nn);
    if (ar->siridb)
    {
        siridb_incref(ar->siridb);
    }
}

static int api__url_cb(http_parser * parser, const char * at, size_t n)
{
    siri_api_request_t * ar = parser->data;

    if (api__starts_with(&at, &n, "/query/", strlen("/query/")))
    {
        ar->request_type = SIRI_API_RT_QUERY;
        api__get_siridb(ar, at, n);
    }
    else if (api__starts_with(&at, &n, "/insert/", strlen("/insert/")))
    {
        ar->request_type = SIRI_API_RT_INSERT;
        api__get_siridb(ar, at, n);
    }
    else if (API__CMP_WITH(at, n, "/new-account"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_NEW_ACCOUNT;
    }
    else if (API__CMP_WITH(at, n, "/change-password"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_CHANGE_PASSWORD;
    }
    else if (API__CMP_WITH(at, n, "/drop-account"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_DROP_ACCOUNT;
    }
    else if (API__CMP_WITH(at, n, "/new-database"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_NEW_DATABASE;
    }
    else if (API__CMP_WITH(at, n, "/new-pool"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_NEW_POOL;
    }
    else if (API__CMP_WITH(at, n, "/new-replica"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_NEW_REPLICA;
    }
    else if (API__CMP_WITH(at, n, "/drop-database"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_DROP_DATABASE;
    }
    else if (API__CMP_WITH(at, n, "/get-version"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_GET_VERSION;
    }
    else if (API__CMP_WITH(at, n, "/get-accounts"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_GET_ACCOUNTS;
    }
    else if (API__CMP_WITH(at, n, "/get-databases"))
    {
        ar->request_type = SIRI_APT_RT_SERVICE;
        ar->service_type = SERVICE_GET_DATABASES;
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
    if (API__ICMP_WITH(at, n, api__content_type[SIRI_API_CT_JSON]) ||
        API__ICMP_WITH(at, n, "text/json"))
    {
        ar->content_type = SIRI_API_CT_JSON;
        return 0;
    }

    if (API__ICMP_WITH(at, n, api__content_type[SIRI_API_CT_QPACK]) ||
        API__ICMP_WITH(at, n, "application/x-qpack"))
    {
        ar->content_type = SIRI_API_CT_QPACK;
        return 0;
    }

    /* invalid content type */
    log_debug("unsupported content-type: %.*s", (int) n, at);
    return 0;
}

static int api__on_authorization(siri_api_request_t * ar, const char * at, size_t n)
{
    if (api__istarts_with(&at, &n, "bearer ", strlen("bearer ")))
    {
        log_debug("token authorization is not supported yet");
    }

    if (api__istarts_with(&at, &n, "basic ", strlen("basic ")))
    {
        if (ar->request_type == SIRI_APT_RT_SERVICE)
        {
            ar->service_authenticated = \
                    siri_service_account_check_basic(&siri, at, n);
            return 0;
        }
        siridb_user_t * user;
        user = ar->siridb
                ? siridb_users_get_user_from_basic(ar->siridb, at, n)
                : NULL;

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
    siri_api_request_t * ar = req->handle->data;

    if (status)
        log_error(
                "error writing HTTP API response: `%s`",
                uv_strerror(status));

    /* reset the API to support multiple request on the same connection */
    api__reset(ar);

    /* Resume parsing */
    http_parser_pause(&ar->parser, 0);

    sirinet_stream_decref(ar);
}

static int api__plain_response(
        siri_api_request_t * ar,
        const siri_api_header_t ht)
{
    const char * body = api__default_body[ht];
    char header[API__HEADER_MAX_SZ];
    size_t body_size;
    int header_size;

    body_size = strlen(body);
    header_size = api__header(header, ht, SIRI_API_CT_TEXT, body_size);

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

static int api__query(siri_api_request_t * ar, api__query_t * q)
{
    sirinet_stream_incref(ar);

    siridb_query_run(
                    0,
                    (sirinet_stream_t *) ar,
                    q->query,
                    q->query_n,
                    q->factor,
                    SIRIDB_QUERY_FLAG_MASTER);
    return 0;
}

static int api__read_factor(
        siridb_timep_t precision,
        qp_unpacker_t * up,
        api__query_t * q)
{
    qp_obj_t obj;

    if (!qp_is_raw(qp_next(up, &obj)))
        return -1;

     if (obj.len == 1 && obj.via.raw[0] == 's')
     {
         q->factor = pow(1000.0, SIRIDB_TIME_SECONDS - precision);
         return 0;
     }

     if (obj.len == 2 && obj.via.raw[1] == 's')
     {
         switch (obj.via.raw[0])
         {
         case 'm':
             q->factor = pow(1000.0, SIRIDB_TIME_MILLISECONDS - precision);
             return 0;
         case 'u':
             q->factor = pow(1000.0, SIRIDB_TIME_MICROSECONDS - precision);
             return 0;
         case 'n':
             q->factor = pow(1000.0, SIRIDB_TIME_NANOSECONDS - precision);
             return 0;
         }
     }
     return -1;
}

static int api__query_from_qp(api__query_t * q, siri_api_request_t * ar)
{
    qp_obj_t obj;
    qp_unpacker_t up;
    qp_unpacker_init(&up, (unsigned char *) ar->buf, ar->len);

    q->factor = 0.0;
    q->query = NULL;

    if (!qp_is_map(qp_next(&up, &obj)))
    {
        return api__plain_response(ar, E400_BAD_REQUEST);
    }

    while (qp_next(&up, &obj) && !qp_is_close(obj.tp))
    {
        if (!qp_is_raw(obj.tp) || obj.len < 1)
        {
            return api__plain_response(ar, E400_BAD_REQUEST);
        }

        switch(*obj.via.str)
        {
        case 't':
            if (api__read_factor(ar->siridb->time->precision, &up, q))
            {
                return api__plain_response(ar, E400_BAD_REQUEST);
            }
            break;
        case 'q':
            if (!qp_is_raw(qp_next(&up, &obj)))
            {
                return api__plain_response(ar, E400_BAD_REQUEST);
            }
            q->query = obj.via.str;
            q->query_n = obj.len;
            break;
        default:
            return api__plain_response(ar, E400_BAD_REQUEST);
        }
    }

    return q->query == NULL
            ? api__plain_response(ar, E400_BAD_REQUEST)
            : api__query(ar, q);
}

static int api__insert_from_qp(siri_api_request_t * ar)
{
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, (unsigned char *) ar->buf, ar->len);

    siridb_insert_t * insert = siridb_insert_new(
            ar->siridb,
            0,
            (sirinet_stream_t *) ar);

    if (insert == NULL)
    {
        return api__plain_response(ar, E500_INTERNAL_SERVER_ERROR);
    }

    ssize_t rc = siridb_insert_assign_pools(
            ar->siridb,
            &unpacker,
            insert->packer);

    switch ((siridb_insert_err_t) rc)
    {
    case ERR_EXPECTING_ARRAY:
    case ERR_EXPECTING_SERIES_NAME:
    case ERR_EXPECTING_MAP_OR_ARRAY:
    case ERR_EXPECTING_INTEGER_TS:
    case ERR_TIMESTAMP_OUT_OF_RANGE:
    case ERR_UNSUPPORTED_VALUE:
    case ERR_EXPECTING_AT_LEAST_ONE_POINT:
    case ERR_EXPECTING_NAME_AND_POINTS:
    case ERR_INCOMPATIBLE_SERVER_VERSION:
    case ERR_MEM_ALLOC:
        {
            /* something went wrong, get correct err message */
            const char * err_msg = siridb_insert_err_msg(rc);

            log_error("Insert error: '%s' at position %lu",
                    err_msg, unpacker.pt -  (unsigned char *) ar->buf);

            /* create and send package */
            sirinet_pkg_t * package = sirinet_pkg_err(
                    0,
                    strlen(err_msg),
                    CPROTO_ERR_INSERT,
                    err_msg);

            if (package != NULL)
            {
                /* ignore result code, signal can be raised */
                sirinet_pkg_send((sirinet_stream_t *) ar, package);
            }
        }

        /* error, free insert */
        siridb_insert_free(insert);
        break;

    default:
        if (siridb_insert_points_to_pools(insert, (size_t) rc))
        {
            siridb_insert_free(insert);  /* signal is raised */
        }
        else
        {
            /* extra increment for the insert task */
            sirinet_stream_incref(ar);
        }
        break;
    }
    return 0;
}

static int api__insert_cb(http_parser * parser)
{
    siri_api_request_t * ar = parser->data;

    if (parser->method != HTTP_POST)
        return api__plain_response(ar, E405_METHOD_NOT_ALLOWED);

    if (!ar->siridb)
        return api__plain_response(ar, E404_NOT_FOUND);

    if (!ar->origin)
        return api__plain_response(ar, E401_UNAUTHORIZED);


    if (!(((siridb_user_t *) ar->origin)->access_bit & SIRIDB_ACCESS_INSERT))
        return api__plain_response(ar, E403_FORBIDDEN);

    if ((
            ar->siridb->server->flags != SERVER_FLAG_RUNNING &&
            ar->siridb->server->flags != SERVER_FLAG_RUNNING + SERVER_FLAG_REINDEXING
        ) ||
        !siridb_pools_accessible(ar->siridb))
        return api__plain_response(ar, E503_SERVICE_UNAVAILABLE);

    switch (ar->content_type)
    {
    case SIRI_API_CT_TEXT:
        /* Or, shall we allow text and return we some sort of CSV format? */
        break;
    case SIRI_API_CT_JSON:
    {
        char * dst;
        size_t dst_n;
        if (qpjson_json_to_qp(ar->buf, ar->len, &dst, &dst_n))
            return api__plain_response(ar, E400_BAD_REQUEST);
        free(ar->buf);
        ar->buf = dst;
        ar->len = dst_n;
    }
    /* fall through */
    case SIRI_API_CT_QPACK:
        return api__insert_from_qp(ar);
    }

    return api__plain_response(ar, E415_UNSUPPORTED_MEDIA_TYPE);
}

static int api__query_cb(http_parser * parser)
{
    api__query_t q;
    siri_api_request_t * ar = parser->data;

    if (parser->method != HTTP_POST)
        return api__plain_response(ar, E405_METHOD_NOT_ALLOWED);

    if (!ar->siridb)
        return api__plain_response(ar, E404_NOT_FOUND);

    if (!ar->origin)
        return api__plain_response(ar, E401_UNAUTHORIZED);

    switch (ar->content_type)
    {
    case SIRI_API_CT_TEXT:
        /* Or, shall we allow text and return we some sort of CSV format? */
        break;
    case SIRI_API_CT_JSON:
    {
        char * dst;
        size_t dst_n;
        if (qpjson_json_to_qp(ar->buf, ar->len, &dst, &dst_n))
            return api__plain_response(ar, E400_BAD_REQUEST);
        free(ar->buf);
        ar->buf = dst;
        ar->len = dst_n;
    }
    /* fall through */
    case SIRI_API_CT_QPACK:
        return api__query_from_qp(&q, ar);
    }

    return api__plain_response(ar, E415_UNSUPPORTED_MEDIA_TYPE);
}

static int api__service_cb(http_parser * parser)
{
    qp_unpacker_t up;
    cproto_server_t res;
    siri_api_request_t * ar = parser->data;
    qp_packer_t * packer = NULL;
    sirinet_pkg_t * package;
    char err_msg[SIRI_MAX_SIZE_ERR_MSG];

    switch ((service_request_t) ar->service_type)
    {
    case SERVICE_NEW_ACCOUNT:
    case SERVICE_CHANGE_PASSWORD:
    case SERVICE_DROP_ACCOUNT:
    case SERVICE_NEW_DATABASE:
    case SERVICE_NEW_POOL:
    case SERVICE_NEW_REPLICA:
    case SERVICE_DROP_DATABASE:
        if (parser->method != HTTP_POST)
            return api__plain_response(ar, E405_METHOD_NOT_ALLOWED);
        break;

    case SERVICE_GET_VERSION:
    case SERVICE_GET_ACCOUNTS:
    case SERVICE_GET_DATABASES:
        if (parser->method != HTTP_GET)
            return api__plain_response(ar, E405_METHOD_NOT_ALLOWED);
        break;
    }

    if (!ar->service_authenticated)
        return api__plain_response(ar, E401_UNAUTHORIZED);

    switch (ar->content_type)
    {
    case SIRI_API_CT_TEXT:
        if (ar->len)
            return api__plain_response(ar, E415_UNSUPPORTED_MEDIA_TYPE);
        ar->content_type = SIRI_API_CT_JSON;
        break;
    case SIRI_API_CT_JSON:
        if (ar->len)
        {
            char * dst;
            size_t dst_n;
            if (qpjson_json_to_qp(ar->buf, ar->len, &dst, &dst_n))
                return api__plain_response(ar, E400_BAD_REQUEST);
            free(ar->buf);
            ar->buf = dst;
            ar->len = dst_n;
        }
        break;
    case SIRI_API_CT_QPACK:
        break;
    }

    qp_unpacker_init(&up, (unsigned char *) ar->buf, ar->len);

    res = siri_service_request(
            ar->service_type,
            &up,
            &packer,
            0,
            (sirinet_stream_t *) ar,
            err_msg);

    if (res == CPROTO_DEFERRED)
    {
        sirinet_stream_incref(ar);
    }

    package =
            (res == CPROTO_DEFERRED) ? NULL :
            (res == CPROTO_ERR_SERVICE) ? sirinet_pkg_err(
                    0,
                    strlen(err_msg),
                    res,
                    err_msg) :
            (res == CPROTO_ACK_SERVICE_DATA) ? sirinet_packer2pkg(
                    packer,
                    0,
                    res) : sirinet_pkg_new(0, 0, res, NULL);

    return package ? sirinet_pkg_send((sirinet_stream_t *) ar, package) : 0;
}

static int api__message_complete_cb(http_parser * parser)
{
    siri_api_request_t * ar = parser->data;

    /* Pause the HTTP parser;
     * This is required since SiriDB will handle queries and inserts
     * asynchronously and SiriDB must be sure that the request does not
     * change during this time. It is also important to write the responses
     * in order and this solves both issues. */
    http_parser_pause(&ar->parser, 1);

    switch(ar->request_type)
    {
    case SIRI_API_RT_NONE:
        return api__plain_response(ar, E404_NOT_FOUND);
    case SIRI_API_RT_QUERY:
        return api__query_cb(parser);
    case SIRI_API_RT_INSERT:
        return api__insert_cb(parser);
    case SIRI_APT_RT_SERVICE:
        return api__service_cb(parser);
    }

    return api__plain_response(ar, E500_INTERNAL_SERVER_ERROR);
}

static void api__write_free_cb(uv_write_t * req, int status)
{
    free(req->data);
    api__write_cb(req, status);
}

static int api__close_resp(
        siri_api_request_t * ar,
        const siri_api_header_t ht,
        void * data,
        size_t size)
{
    char header[API__HEADER_MAX_SZ];
    int header_size = 0;

    header_size = api__header(header, ht, ar->content_type, size);

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

    if (siri.cfg->ip_support == IP_SUPPORT_IPV4ONLY)
    {
        (void) uv_ip4_addr("0.0.0.0", (int) port, (struct sockaddr_in *) &addr);
    } else
    {
        (void) uv_ip6_addr("::", (int) port, (struct sockaddr_in6 *) &addr);
    }

    api__settings.on_url = api__url_cb;
    api__settings.on_header_field = api__header_field_cb;
    api__settings.on_header_value = api__header_value_cb;
    api__settings.on_message_complete = api__message_complete_cb;
    api__settings.on_body = api__body_cb;
    api__settings.on_headers_complete = api__headers_complete_cb;

    if (
        (rc = uv_tcp_init(siri.loop, &api__uv_server)) ||
        (rc = uv_tcp_bind(
                &api__uv_server,
                (const struct sockaddr *) &addr,
                (siri.cfg->ip_support == IP_SUPPORT_IPV6ONLY) ? UV_TCP_IPV6ONLY : 0)) ||
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

#define API__DUP(__data, __n, __s) \
    do { \
        __n = strlen("{\"error_msg\":\""__s"\"}"); \
        __data = strndup("{\"error_msg\":\""__s"\"}", __n); \
        if (!__data) __n = 0; \
    } while(0)

static void api__yajl_parse_error(
        char ** str,
        size_t * size,
        siri_api_header_t * ht,
        yajl_gen_status stat)
{
    switch (stat)
    {
    case yajl_gen_status_ok:
        return;
    case yajl_gen_keys_must_be_strings:
        *ht = E400_BAD_REQUEST;
        API__DUP(*str, *size, "JSON keys must be strings");
        return;
    case yajl_max_depth_exceeded:
        *ht = E400_BAD_REQUEST;
        API__DUP(*str, *size, "JSON max depth exceeded");
        return;
    case yajl_gen_in_error_state:
        *ht = E500_INTERNAL_SERVER_ERROR;
        API__DUP(*str, *size, "JSON general error");
        return;
    case yajl_gen_generation_complete:
        *ht = E500_INTERNAL_SERVER_ERROR;
        API__DUP(*str, *size, "JSON completed");
        return;
    case yajl_gen_invalid_number:
        *ht = E400_BAD_REQUEST;
        API__DUP(*str, *size, "JSON invalid number");
        return;
    case yajl_gen_no_buf:
        *ht = E500_INTERNAL_SERVER_ERROR;
        API__DUP(*str, *size, "JSON no buffer has been set");
        return;
    case yajl_gen_invalid_string:
        *ht = E400_BAD_REQUEST;
        API__DUP(*str, *size, "JSON only accepts valid UTF8 strings");
        return;
    }
    *ht = E500_INTERNAL_SERVER_ERROR;
    API__DUP(*str, *size, "JSON unexpected error");
}

int siri_api_send(
        siri_api_request_t * ar,
        siri_api_header_t ht,
        unsigned char * src,
        size_t n)
{
    unsigned char * data;
    if (n == 0)
    {
        if (ht != E200_OK)
        {
            return api__plain_response(ar, ht);
        }
        src = (unsigned char *) "\x82OK";
        n = 3;
    }

    if (ar->content_type == SIRI_API_CT_JSON)
    {
        size_t tmp_sz;
        yajl_gen_status stat = qpjson_qp_to_json(
                src,
                n,
                &data,
                &tmp_sz,
                0);
        if (stat)
            api__yajl_parse_error((char **) &data, &tmp_sz, &ht, stat);

        n = tmp_sz;
    }
    else
    {
        assert (ar->content_type == SIRI_API_CT_QPACK);
        data = malloc(n);
        if (data == NULL)
        {
            ht = E500_INTERNAL_SERVER_ERROR;
            n = 0;
        }
        else
        {
            memcpy(data, src, n);
        }
    }

    return api__close_resp(ar, ht, data, n);
}
