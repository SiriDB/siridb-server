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
#include <siri/db/servers.h>
#include <siri/net/promise.h>
#include <siri/siri.h>
#include <assert.h>
#include <siri/net/socket.h>
#include <string.h>
#include <siri/version.h>
#include <procinfo/procinfo.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1
#define SIRIDB_SERVER_FLAGS_TIMEOUT 5000    // 5 seconds

static int SERVER_update_name(siridb_server_t * server);
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

/*
 * In case of an error the return value is NULL and a SIGNAL is raised.
 */
siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool)
{
    siridb_server_t * server =
            (siridb_server_t *) malloc(sizeof(siridb_server_t));
    if (server == NULL)
    {
        ERR_ALLOC
        return NULL;
    }
    /* copy uuid */
    memcpy(server->uuid, uuid, 16);

    /* initialize with NULL, SERVER_update_name() sets the correct name */
    server->name = NULL;

    /* copy address */
    server->address = strndup(address, address_len);
    if (server->address == NULL)
    {
        ERR_ALLOC
        free(server);
        return NULL;
    }

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
    if (SERVER_update_name(server))
    {
        SERVER_free(server);
        server = NULL;  /* signal is raised */
    }

    return server;
}

/*
 * Returns < 0 if the uuid from server A is less than uuid from server B.
 * Returns > 0 if the uuid from server A is greater than uuid from server B.
 * Returns 0 when uuid server A and B are equal.
 */
inline int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb)
{
    return uuid_compare(sa->uuid, sb->uuid);
}

inline void siridb_server_incref(siridb_server_t * server)
{
    server->ref++;
}

/*
 * Decrement server reference counter and free the server when zero is reached.
 * When the server is destroyed, all remaining server->promises are cancelled
 * and each promise->cb() will be called.
 */
void siridb_server_decref(siridb_server_t * server)
{
    if (!--server->ref)
    {
        SERVER_free(server);
    }
}

/*
 * This function can return -1 and raise a SIGNAL which means the 'cb' function
 * will NOT be called. Usually this function should return 0 which means that
 * we try to send the package and the 'cb' function can be checked for the
 * result.
 *
 * Note that 'pkg->pid' will be overwritten with a new package id.
 *
 * (default timeout PROMISE_DEFAULT_TIMEOUT is used when timeout 0 is set)
 *
 */
int siridb_server_send_pkg(
        siridb_server_t * server,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data)
{
#ifdef DEBUG
    assert (server->socket != NULL);
    assert (server->promises != NULL);
    assert (cb != NULL);
#endif
    sirinet_promise_t * promise =
            (sirinet_promise_t *) malloc(sizeof(sirinet_promise_t));
    if (promise == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    promise->timer = (uv_timer_t *) malloc(sizeof(uv_timer_t));
    if (promise->timer == NULL)
    {
        ERR_ALLOC
        free(promise);
        return -1;
    }
    promise->timer->data = promise;
    promise->cb = cb;
    /*
     * we do not need to increment the server reference counter since promises
     * will be destroyed before the server is destroyed.
     */
    promise->server = server;
    pkg->pid = promise->pid = server->pid;
    promise->data = data;

    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));
    if (req == NULL)
    {
        ERR_ALLOC
        free(promise->timer);
        free(promise);
        return -1;
    }

    if (imap64_add(server->promises, promise->pid, promise))
    {
        free(promise->timer);
        free(promise);
        free(req);
        return -1;  /* signal is raised */
    }

    uv_timer_init(siri.loop, promise->timer);
    uv_timer_start(
            promise->timer,
            SERVER_timeout_pkg,
            (timeout) ? timeout : PROMISE_DEFAULT_TIMEOUT,
            0);

    log_debug("Sending (pid: %lu, len: %lu, tp: %u) to '%s'",
            pkg->pid, pkg->len, pkg->tp, server->name);

    req->data = promise;

    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            PKG_HEADER_SIZE + pkg->len);

    uv_write(
            req,
            (uv_stream_t *) server->socket,
            &wrbuf,
            1,
            SERVER_write_cb);

    server->pid++;

    return 0;
}

/*
 * Register and return a new server from qpack data.
 * The qpack data should contain: [uuid, address, port, pool]
 *
 * In case of an error NULL is returned.
 * (a SIGNAL might be raised in case of allocation errors)
 */
siridb_server_t * siridb_server_register(
        siridb_t * siridb,
        const char * data,
        size_t len)
{
    siridb_server_t * server = NULL;
    qp_unpacker_t * unpacker = qp_unpacker_new(data, len);
    if (unpacker != NULL)
    {
        qp_obj_t * qp_uuid = qp_object_new();
        qp_obj_t * qp_address = qp_object_new();
        qp_obj_t * qp_port = qp_object_new();
        qp_obj_t * qp_pool = qp_object_new();
        if (    qp_uuid == NULL || qp_address == NULL ||
                qp_port == NULL || qp_pool == NULL)
        {
            qp_object_free_safe(qp_uuid);
            qp_object_free_safe(qp_address);
            qp_object_free_safe(qp_port);
            qp_object_free_safe(qp_pool);
        }
        else
        {
            if (qp_is_array(qp_next(unpacker, NULL)) &&
                qp_next(unpacker, qp_uuid) == QP_RAW &&
                qp_next(unpacker, qp_address) == QP_RAW &&
                qp_next(unpacker, qp_port) == QP_INT64 &&
                qp_next(unpacker, qp_pool) == QP_INT64)
            {
                server = siridb_server_new(
                        qp_uuid->via->raw,
                        qp_address->via->raw,
                        qp_address->len,
                        qp_port->via->int64,
                        qp_pool->via->int64);


                if (server != NULL)
                {
                    if (    (server->promises = imap64_new()) == NULL ||
                            siridb_servers_register(siridb, server))
                    {
                        SERVER_free(server);
                        server = NULL;
                    }
                }
            }

            qp_object_free(qp_uuid);
            qp_object_free(qp_address);
            qp_object_free(qp_port);
            qp_object_free(qp_pool);
        }

        qp_unpacker_free(unpacker);
    }
    return server;
}

/*
 * This function can raise a SIGNAL.
 */
void siridb_server_send_flags(siridb_server_t * server)
{

#ifdef DEBUG
    assert (server->socket != NULL);
    assert (siridb_server_is_online(server));
#endif

    sirinet_socket_t * ssocket = server->socket->data;

#ifdef DEBUG
    assert (ssocket->siridb != NULL);
#endif

    int16_t n = ssocket->siridb->server->flags;
    QP_PACK_INT16(buffer, n)

    sirinet_pkg_t * pkg = sirinet_pkg_new(0, 3, BPROTO_FLAGS_UPDATE, buffer);
    if (pkg != NULL)
    {
        siridb_server_send_pkg(
                server,
                pkg,
                SIRIDB_SERVER_FLAGS_TIMEOUT,
                SERVER_on_flags_update_response,
                NULL);
        free(pkg);
    }
}

/*
 * Write call-back.
 */
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

/*
 * Timeout received.
 */
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

/*
 * This function can raise a SIGNAL.
 */
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
        sirinet_pkg_t * pkg;
        if (packer != NULL)
        {
            if (!(
                qp_add_type(packer, QP_ARRAY_OPEN) ||
                qp_add_raw(packer, (const char *) ssocket->siridb->server->uuid, 16) ||
                qp_add_string_term(packer, ssocket->siridb->dbname) ||
                qp_add_int16(packer, ssocket->siridb->server->flags) ||
                qp_add_string_term(packer, SIRIDB_VERSION) ||
                qp_add_string_term(packer, SIRIDB_MINIMAL_VERSION) ||
                qp_add_string_term(packer, ssocket->siridb->dbpath) ||
                qp_add_string_term(packer, ssocket->siridb->buffer_path) ||
                qp_add_int64(packer, (int64_t) abs(ssocket->siridb->buffer_size)) ||
                qp_add_int32(packer, (int32_t) abs(siri.startup_time))) &&
                    (pkg = sirinet_pkg_new(
                            0,
                            packer->len,
                            BPROTO_AUTH_REQUEST,
                            packer->buffer)) != NULL)
            {
                siridb_server_send_pkg(
                        server,
                        pkg,
                        0,
                        SERVER_on_auth_response,
                        NULL);
                free(pkg);
            }
            qp_packer_free(packer);
        }
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

/*
 * on-data call-back function.
 *In case the promise is found, promise->cb() will be called.
 */
static void SERVER_on_data(uv_stream_t * client, sirinet_pkg_t * pkg)
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

/*
 * Returns the current server status (flags) as string. the returned value
 * is created with malloc() so do not forget to free the result.
 *
 * NULL is returned in case malloc has failed.
 */
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

/*
 * Returns true when the given property (CLERI keyword) needs a remote query
 */
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

/*
 * Cancel promise. The promise->cb will be called.
 */
static void SERVER_cancel_promise(sirinet_promise_t * promise, void * args)
{
    if (!uv_is_closing((uv_handle_t *) promise->timer))
    {
        uv_timer_stop(promise->timer);
        uv_close((uv_handle_t *) promise->timer, (uv_close_cb) free);
    }
    promise->cb(promise, NULL, PROMISE_CANCELLED_ERROR);
}

/*
 * Free server. If the server has promises, each promise will be cancelled and
 * so each promise->cb() will be called.
 */
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

/*
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
static int SERVER_update_name(siridb_server_t * server)
{
    /* start len with 2, one for : and one for 0 terminator */
    size_t len = 2;
    uint16_t i = server->port;
    char * tmp;

#ifdef DEBUG
    assert(server->port > 0);
#endif

    /* append 'string' length for server->port */
    for (; i; i /= 10, len++);

    /* append 'address' length */
    len += strlen(server->address);

    /* allocate enough space */
    tmp = (char *) realloc(server->name, len);
    if (tmp == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    server->name = tmp;
    sprintf(server->name, "%s:%d", server->address, server->port);
    return 0;
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
    else if (pkg->tp == BPROTO_AUTH_SUCCESS)
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

    if ((status || pkg->tp != BPROTO_AUTH_SUCCESS) &&
            promise->server->socket != NULL)
    {
        uv_close(
                (uv_handle_t *) promise->server->socket,
                (uv_close_cb) sirinet_socket_free);
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
    else if (pkg->tp == BPROTO_ACK_FLAGS)
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
