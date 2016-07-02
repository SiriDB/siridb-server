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
#include <siri/version.h>
#include <procinfo/procinfo.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static void SERVER_update_name(siridb_server_t * server);
static void SERVER_free(siridb_server_t * server);
static void SERVER_timeout_pkg(uv_timer_t * handle);
static void SERVER_write_cb(uv_write_t * req, int status);
static void SERVER_on_auth_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);
static void SERVER_on_flags_update_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
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
    server->version = NULL;
    server->dbpath = NULL;
    server->buffer_path = NULL;
    server->buffer_size = 0;
    server->startup_time = 0;
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
        sirinet_promise_cb_t cb,
        void * data)
{
#ifdef DEBUG
    assert (server->socket != NULL);
    assert (server->promises != NULL);
    assert (cb != NULL);
#endif
    sirinet_promise_t * promise =
            (sirinet_promise_t *) malloc(sizeof(sirinet_promise_t));

    promise->timer = (uv_timer_t *) malloc(sizeof(uv_timer_t));
    promise->timer->data = promise;
    promise->cb = cb;
    /*
     * we do not need to increment the server reference counter since promises
     * will be destroyed before the server is destroyed.
     */
    promise->server = server;
    promise->pid = server->pid;
    promise->data = data;

    uv_timer_init(siri.loop, promise->timer);
    uv_timer_start(
            promise->timer,
            SERVER_timeout_pkg,
            (timeout) ? timeout : PROMISE_DEFAULT_TIMEOUT,
            0);

    imap64_add(server->promises, promise->pid, promise);

    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));

    req->data = promise;

    sirinet_pkg_t * pkg = sirinet_pkg_new(
            promise->pid,
            len,
            tp,
            content);

    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            PKG_HEADER_SIZE + pkg->len);

    uv_write(req, (uv_stream_t *) server->socket, &wrbuf, 1, SERVER_write_cb);

    free(pkg);

    server->pid++;
}

void siridb_server_send_flags(siridb_server_t * server)
{

#ifdef DEBUG
    assert (server->socket != NULL);
    assert (siridb_server_is_available(server));
#endif

    sirinet_socket_t * ssocket = server->socket->data;

#ifdef DEBUG
    assert (ssocket->siridb != NULL);
#endif

    int16_t n = ssocket->siridb->server->flags;
    QP_PACK_INT16(buffer, n)

    siridb_server_send_pkg(
            server,
            3,
            BP_FLAGS_UPDATE,
            buffer,
            0,
            SERVER_on_flags_update_response,
            NULL);
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

        uv_timer_stop(promise->timer);
        uv_close((uv_handle_t *) promise->timer, (uv_close_cb) free);

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
    /* the timer is stopped but we still need to close the timer */
    uv_close((uv_handle_t *) promise->timer, (uv_close_cb) free);

    promise->cb(promise, NULL, PROMISE_TIMEOUT_ERROR);
}

static void SERVER_on_connect(uv_connect_t * req, int status)
{
    sirinet_socket_t * ssocket = req->handle->data;
    siridb_server_t * server = ssocket->origin;

    if (status == 0)
    {
        log_debug(
                "Connection created to back-end server: '%s', "
                "sending authentication request", server->name);

        uv_read_start(
                req->handle,
                sirinet_socket_alloc_buffer,
                sirinet_socket_on_data);

        qp_packer_t * packer = qp_packer_new(512);

        qp_add_type(packer, QP_ARRAY_OPEN);

        qp_add_raw(packer, (const char *) ssocket->siridb->server->uuid, 16);
        qp_add_string_term(packer, ssocket->siridb->dbname);
        qp_add_int16(packer, ssocket->siridb->server->flags);
        qp_add_string_term(packer, SIRIDB_VERSION);
        qp_add_string_term(packer, SIRIDB_MINIMAL_VERSION);
        qp_add_string_term(packer, ssocket->siridb->dbpath);
        qp_add_string_term(packer, ssocket->siridb->buffer_path);
        qp_add_int64(packer, (int64_t) abs(ssocket->siridb->buffer_size));
        qp_add_int32(packer, (int32_t) abs(siri.startup_time));

        siridb_server_send_pkg(
                server,
                packer->len,
                BP_AUTH_REQUEST,
                packer->buffer,
                0,
                SERVER_on_auth_response,
                NULL);

        qp_packer_free(packer);
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

static void SERVER_on_data(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    sirinet_socket_t * ssocket = client->data;
    siridb_server_t * server = ssocket->origin;
    sirinet_promise_t * promise = imap64_pop(server->promises, pkg->pid);

    if (promise == NULL)
    {
        log_warning(
                "Received a package (PID %lu) from server '%s' which has "
                "probably timed-out earlier.", pkg->pid, server->name);
    }
    else
    {
        uv_timer_stop(promise->timer);
        uv_close((uv_handle_t *) promise->timer, (uv_close_cb) free);
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

char * siridb_server_str_status(siridb_server_t * server)
{
    /* we must initialize the buffer according to the longest possible value */
    char buffer[48] = {};
    int n = 0;
    for (int i = 1; i < SERVER_FLAG_AUTHENTICATED; i *= 2)
    {
        if (server->flags & i)
        {
            switch (i)
            {
            case SERVER_FLAG_RUNNING:
                strcat(buffer, (!n) ? "running" : " | " "running");
                break;
            case SERVER_FLAG_PAUSED:
                strcat(buffer, (!n) ? "paused" : " | " "paused");
                break;
            case SERVER_FLAG_SYNCHRONIZING:
                strcat(buffer, (!n) ? "synchronizing" : " | " "synchronizing");
                break;
            case SERVER_FLAG_REINDEXING:
                strcat(buffer, (!n) ? "re-indexing" : " | " "re-indexing");
                break;
            }
            n = 1;
        }
    }
    return strdup((n) ? buffer : "offline");
}

int siridb_server_is_remote_prop(uint32_t prop)
{
    switch (prop)
    {
    case CLERI_GID_K_ADDRESS:
    case CLERI_GID_K_BUFFER_PATH:
    case CLERI_GID_K_BUFFER_SIZE:
    case CLERI_GID_K_DBPATH:
    case CLERI_GID_K_NAME:
    case CLERI_GID_K_ONLINE:
    case CLERI_GID_K_POOL:
    case CLERI_GID_K_PORT:
    case CLERI_GID_K_STARTUP_TIME:
    case CLERI_GID_K_STATUS:
    case CLERI_GID_K_UUID:
    case CLERI_GID_K_VERSION:
        return 0;
    }
    return 1;
}

int siridb_server_cexpr_cb(
        siridb_server_walker_t * wserver,
        cexpr_condition_t * cond)
{
    switch (cond->prop)
    {
    case CLERI_GID_K_ADDRESS:
        return cexpr_str_cmp(
                cond->operator,
                wserver->server->address,
                cond->str);

    case CLERI_GID_K_BUFFER_PATH:
        return cexpr_str_cmp(
                cond->operator,
                (wserver->siridb->server == wserver->server) ?
                        wserver->siridb->buffer_path :
                        (wserver->server->buffer_path != NULL) ?
                                wserver->server->buffer_path : "",
                cond->str);

    case CLERI_GID_K_BUFFER_SIZE:
        return cexpr_int_cmp(
                cond->operator,
                (wserver->siridb->server == wserver->server) ?
                        wserver->siridb->buffer_size :
                        wserver->server->buffer_size,
                cond->int64);

    case CLERI_GID_K_DBPATH:
        return cexpr_str_cmp(
                cond->operator,
                (wserver->siridb->server == wserver->server) ?
                        wserver->siridb->dbpath :
                        (wserver->server->dbpath != NULL) ?
                                wserver->server->dbpath : "",
                cond->str);

    case CLERI_GID_K_NAME:
        return cexpr_str_cmp(
                cond->operator,
                wserver->server->name,
                cond->str);

    case CLERI_GID_K_ONLINE:
        return cexpr_bool_cmp(
                cond->operator,
                (   wserver->siridb->server == wserver->server ||
                    wserver->server->socket != NULL),
                cond->int64);

    case CLERI_GID_K_POOL:
        return cexpr_int_cmp(
                cond->operator,
                wserver->server->pool,
                cond->int64);

    case CLERI_GID_K_PORT:
        return cexpr_int_cmp(
                cond->operator,
                wserver->server->port,
                cond->int64);

    case CLERI_GID_K_STARTUP_TIME:
        return cexpr_int_cmp(
                cond->operator,
                (wserver->siridb->server == wserver->server) ?
                        siri.startup_time :
                        wserver->server->startup_time,
                cond->int64);

    case CLERI_GID_K_STATUS:
        {
            char * status = siridb_server_str_status(wserver->server);
            int rc = cexpr_str_cmp(cond->operator, status, cond->str);
            free(status);
            return rc;
        }

    case CLERI_GID_K_UUID:
        {
            char uuid[37];
            uuid_unparse_lower(wserver->server->uuid, uuid);
            return cexpr_str_cmp(cond->operator, uuid, cond->str);
        }

    case CLERI_GID_K_VERSION:
        return cexpr_str_cmp(
                cond->operator,
                (wserver->siridb->server == wserver->server) ?
                        SIRIDB_VERSION :
                        (wserver->server->version != NULL) ?
                                wserver->server->version : "",
                cond->str);

    /* all properties below are 'remote properties'. if a remote property
     * is detected we should perform the query on each server and only for
     * that specific server.
     */
    case CLERI_GID_K_LOG_LEVEL:
#ifdef DEBUG
        assert (wserver->siridb->server == wserver->server);
#endif
        return cexpr_int_cmp(
                cond->operator,
                Logger.level,
                cond->int64);

    case CLERI_GID_K_MAX_OPEN_FILES:
        return cexpr_int_cmp(
                cond->operator,
                siri.cfg->max_open_files,
                cond->int64);

    case CLERI_GID_K_MEM_USAGE:
        return cexpr_int_cmp(
                cond->operator,
                (int64_t) (procinfo_total_physical_memory() / 1024),
                cond->int64);

    case CLERI_GID_K_OPEN_FILES:
        return cexpr_int_cmp(
                cond->operator,
                siridb_open_files(wserver->siridb),
                cond->int64);

    case CLERI_GID_K_RECEIVED_POINTS:
        return cexpr_int_cmp(
                cond->operator,
                wserver->siridb->received_points,
                cond->int64);

    case CLERI_GID_K_UPTIME:
        return cexpr_int_cmp(
                cond->operator,
                (int64_t) (time(NULL) - wserver->siridb->start_ts),
                cond->int64);
    }
    /* we must NEVER get here */
    log_critical("Unexpected server property received: %d", cond->prop);
    assert (0);
    return -1;
}

static void SERVER_cancel_promise(sirinet_promise_t * promise, void * args)
{
    if (!uv_is_closing((uv_handle_t *) promise->timer))
    {
        uv_timer_stop(promise->timer);
        uv_close((uv_handle_t *) promise->timer, (uv_close_cb) free);
    }
    promise->cb(promise, NULL, PROMISE_CANCELLED_ERROR);
}

static void SERVER_free(siridb_server_t * server)
{
#ifdef DEBUG
    log_debug("Free server: %s", server->name);
#endif
    /* we MUST first free the promises because each promise has a reference to
     * this server and the promise callback might depend on this.
     */
    if (server->promises != NULL)
    {
        imap64_walk(server->promises, (imap64_cb_t) SERVER_cancel_promise, NULL);
        imap64_free(server->promises);
    }
    free(server->name);
    free(server->address);
    free(server->version);
    free(server->dbpath);
    free(server->buffer_path);
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

static void SERVER_on_auth_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug(
                "Error while sending authentication request to '%s' (%s)",
                promise->server->name,
                sirinet_promise_strstatus(status));
    }
    else if (pkg->tp == BP_AUTH_SUCCESS)
    {
        log_info("Successful authenticated to server '%s'",
                promise->server->name);

        promise->server->flags |= SERVER_FLAG_AUTHENTICATED;
    }
    else
    {
        log_error("Authentication with server '%s' failed, error code: %d",
                promise->server->name,
                pkg->tp);
    }

    /* we must free the promise */
    free(promise);
}

static void SERVER_on_flags_update_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug(
                "Error while sending flags update to '%s' (%s)",
                promise->server->name,
                sirinet_promise_strstatus(status));
    }
    else if (pkg->tp == BP_FLAGS_ACK)
    {
        log_debug("Flags ACK received from '%s'", promise->server->name);
    }
    else
    {
        log_critical("Unexpected package type received from '%s' (type: %u)",
                promise->server->name,
                pkg->tp);
    }

    /* we must free the promise */
    free(promise);
}
