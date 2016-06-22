/*
 * server.c - SiriDB Server.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 17-03-2016
 *
 */
#include <siri/db/server.h>
#include <logger/logger.h>
#include <siri/db/query.h>
#include <siri/net/promise.h>
#include <siri/siri.h>
#include <assert.h>
#include <siri/net/socket.h>
#include <string.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static void SERVER_update_name(siridb_server_t * server);
static void SERVER_free(siridb_server_t * server);
static void SERVER_timeout_pkg(uv_timer_t * handle);
static void SERVER_write_cb(uv_write_t * req, int status);
static void SERVER_on_auth_response(
        sirinet_promise_t * promise,
        const sirinet_pkg_t * pkg,
        int status);

siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool)
{
    siridb_server_t * server =
            (siridb_server_t *) malloc(sizeof(siridb_server_t));

    /* copy uuid */
    memcpy(server->uuid, uuid, 16);

    /* initialize with NULL, SERVER_update_name() sets the correct name */
    server->name = NULL;

    /* copy address */
    server->address = (char *) malloc(address_len + 1);
    memcpy(server->address, address, address_len);
    server->address[address_len] = 0;

    server->port = port;
    server->pool = pool;
    server->flags = 0;
    server->ref = 0;
    server->pid = 0;

    /* we set the promises later because we don't need one for self */
    server->promises = NULL;
    server->socket = NULL;

    /* sets address:port to name property */
    SERVER_update_name(server);

    return server;
}

inline int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb)
{
    return uuid_compare(sa->uuid, sa->uuid);
}

inline void siridb_server_incref(siridb_server_t * server)
{
    server->ref++;
}

void siridb_server_decref(siridb_server_t * server)
{
    if (!--server->ref)
    {
        SERVER_free(server);
    }
}

void siridb_server_send_pkg(
        siridb_server_t * server,
        uint32_t len,
        uint16_t tp,
        const char * content,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data)
{
#ifdef DEBUG
    assert (server->promises != NULL);
#endif
    sirinet_promise_t * promise = (sirinet_promise_t *) malloc(sizeof(sirinet_promise_t));

    promise->timer.data = promise;
    promise->cb = cb;
    promise->server = server;
    promise->pid = server->pid;
    promise->data = data;

    uv_timer_init(siri.loop, &promise->timer);
    uv_timer_start(
            &promise->timer,
            SERVER_timeout_pkg,
            (timeout) ? timeout : PROMISE_DEFAULT_TIMEOUT,
            0);

    imap64_add(server->promises, promise->pid, promise);

    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));

    req->data = promise;

    sirinet_pkg_t * pkg = sirinet_pkg_new(
            promise->pid,
            len,
            BP_AUTH_REQUEST,
            content);

    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            SN_PKG_HEADER_SIZE + pkg->len);

    uv_write(req, (uv_stream_t *) server->socket, &wrbuf, 1, SERVER_write_cb);

    free(pkg);

    server->pid++;
}

static void SERVER_write_cb(uv_write_t * req, int status)
{
    if (status)
    {
        sirinet_promise_t * promise = req->data;

        log_error("Socket write error to server '%s' (pid: %lu, error: %s)",
                promise->server->name,
                promise->pid,
                uv_strerror(status));

        if (imap64_pop(promise->server->promises, promise->pid) == NULL)
        {
            log_critical("Got a socket error but the promise is not found!!");
        }

        uv_timer_stop(&promise->timer);
        uv_close((uv_handle_t *) &promise->timer, NULL);
        promise->cb(promise, NULL, PROMISE_WRITE_ERROR);
    }
    free(req);
}

static void SERVER_timeout_pkg(uv_timer_t * handle)
{
    sirinet_promise_t * promise = handle->data;

    if (imap64_pop(promise->server->promises, promise->pid) == NULL)
    {
        log_critical(
                "Timeout task is called on package (PID %lu) for server '%s' "
                "but we cannot find this promise!!",
                promise->pid,
                promise->server->name);
    }
    else
    {
        log_warning("Timeout on package (PID %lu) for server '%s'",
                promise->pid,
                promise->server->name);
    }

    promise->cb(promise, NULL, PROMISE_TMEOUT_ERROR);
}

static void SERVER_on_connect(uv_connect_t * req, int status)
{
    sirinet_socket_t * ssocket = req->handle->data;
    siridb_server_t * server = ssocket->origin;

#ifdef DEBUG
    /* make sure this server does not have the online flag set */
    assert (!(server->flags & SERVER_FLAG_ONLINE));
#endif

    if (status == 0)
    {
        log_debug(
                "Connection created to back-end server: '%s', "
                "sending authentication request", server->name);

        uv_read_start(
                req->handle,
                sirinet_socket_alloc_buffer,
                sirinet_socket_on_data);

        ;
        qp_packer_t * packer = qp_new_packer(1024);
        qp_add_type(packer, QP_ARRAY_OPEN);
        qp_add_raw(packer, (const char *) server->uuid, 16);
        qp_add_string(packer, ssocket->siridb->dbname);

        siridb_server_send_pkg(
                server,
                packer->len,
                BP_AUTH_REQUEST,
                packer->buffer,
                0,
                SERVER_on_auth_response,
                NULL);

        qp_free_packer(packer);
    }
    else
    {
        log_error("Connecting to back-end server '%s' failed (error: %s)",
                server->name,
                uv_strerror(status));

        uv_close((uv_handle_t *) req->handle, (uv_close_cb) sirinet_socket_free);
    }
    free(req);
}

static void SERVER_on_auth_response(
        sirinet_promise_t * promise,
        const sirinet_pkg_t * pkg,
        int status)
{
    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug("Error while sending authentication request: %d", status);
    }
    else
    {
        if (pkg->tp == BP_AUTH_SUCCESS)
        {
            log_info("Successful authenticated to server '%s'",
                    promise->server->name);

            qp_unpacker_t * unpacker = qp_new_unpacker(pkg->data, pkg->len);
            qp_obj_t * qp_flags = qp_new_object();

            if (qp_next(unpacker, qp_flags) == QP_INT64)
            {
                promise->server->flags = qp_flags->via->int64;
            }

            qp_free_object(qp_flags);
            qp_free_unpacker(unpacker);
        }
        else
        {
            log_error("Authentication with server '%s' failed, error code: %d",
                    promise->server->name,
                    pkg->tp);
        }

    }

    /* we must free the promise */
    free(promise);
}

static void SERVER_on_data(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
    sirinet_socket_t * ssocket = client->data;
    siridb_server_t * server = ssocket->origin;
    sirinet_promise_t * promise =
            imap64_pop(server->promises, pkg->pid);

    if (promise == NULL)
    {
        log_warning(
                "Received a package (PID %lu) from server '%s' which is "
                "probably timed-out earlier.", pkg->pid, server->name);
    }
    else
    {
        uv_timer_stop(&promise->timer);
        uv_close((uv_handle_t *) &promise->timer, NULL);
        promise->cb(promise, pkg, PROMISE_SUCCESS);
    }
}

void siridb_server_connect(siridb_t * siridb, siridb_server_t * server)
{
#ifdef DEBUG
    /* server->socket must be NULL at this point */
    assert (server->socket == NULL);
#endif

    struct sockaddr_in dest;
    server->socket = sirinet_socket_new(SOCKET_SERVER, &SERVER_on_data);

    sirinet_socket_t * ssocket = server->socket->data;
    ssocket->origin = server;
    ssocket->siridb = siridb;
    siridb_server_incref(server);

    uv_tcp_init(siri.loop, server->socket);

    uv_ip4_addr(server->address, server->port, &dest);

    uv_connect_t * req = (uv_connect_t *) malloc(sizeof(uv_connect_t));

    uv_tcp_connect(
            req,
            server->socket,
            (const struct sockaddr *) &dest,
            SERVER_on_connect);
}

static void SERVER_cancel_promise(sirinet_promise_t * promise, void * args)
{
    if (!uv_is_closing((uv_handle_t *) &promise->timer))
    {
        uv_timer_stop(&promise->timer);
        uv_close((uv_handle_t *) &promise->timer, NULL);
    }
    promise->cb(promise, NULL, PROMISE_CANCELLED_ERROR);
}

static void SERVER_free(siridb_server_t * server)
{
    log_debug("FREE Server");
    if (server->promises != NULL)
    {
        imap64_walk(server->promises, (imap64_cb_t) SERVER_cancel_promise, NULL);
        imap64_free(server->promises);
    }
    free(server->name);
    free(server->address);
    free(server);
}

static void SERVER_update_name(siridb_server_t * server)
{
    /* start len with 2, one for : and one for 0 terminator */
    size_t len = 2;
    uint16_t i = server->port;

#ifdef DEBUG
    assert(server->port > 0);
#endif

    /* append 'string' length for server->port */
    for (; i; i /= 10, len++);

    /* append 'address' length */
    len += strlen(server->address);

    /* allocate enough space */
    server->name = (server->name == NULL) ?
            (char *) malloc(len) : (char *) realloc(server->name, len);

    /* set the name */
    sprintf(server->name, "%s:%d", server->address, server->port);
}

