/*
 * bserver.c - Back-end Server SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 18-06-2016
 *
 */

#include <siri/net/bserver.h>
#include <stdlib.h>
#include <logger/logger.h>
#include <siri/net/pkg.h>
#include <siri/net/socket.h>
#include <siri/net/protocol.h>
#include <siri/db/auth.h>
#include <assert.h>
#include <siri/db/query.h>
#include <siri/db/insert.h>
#include <siri/db/servers.h>

#define DEFAULT_BACKLOG 128

#define SERVER_CHECK_AUTHENTICATED(server)                                    \
siridb_server_t * server = ((sirinet_socket_t * ) client->data)->origin;      \
if (!(server->flags & SERVER_FLAG_AUTHENTICATED))                             \
{                                                                             \
    sirinet_pkg_t * package;                                                  \
    package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ERR_NOT_AUTHENTICATED, NULL); \
    sirinet_pkg_send((uv_stream_t *) client, package);                        \
    free(package);                                                            \
    return;                                                                   \
}

static void on_new_connection(uv_stream_t * server, int status);
static void on_data(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_auth_request(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_flags_update(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_log_level_update(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_repl_finished(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_query(uv_handle_t * client, sirinet_pkg_t * pkg, int flags);
static void on_insert_pool(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_insert_server(uv_handle_t * client, sirinet_pkg_t * pkg);

static uv_loop_t * loop = NULL;
static struct sockaddr_in server_addr;
static uv_tcp_t backend_server;

int sirinet_bserver_init(siri_t * siri)
{
#ifdef DEBUG
    assert (loop == NULL);
#endif

    int rc;

    /* bind loop to the given loop */
    loop = siri->loop;

    uv_tcp_init(loop, &backend_server);

    uv_ip4_addr(
            siri->cfg->listen_backend_address,
            siri->cfg->listen_backend_port,
            &server_addr);

    /* make sure data is set to NULL so we later on can check this value. */
    backend_server.data = NULL;

    uv_tcp_bind(&backend_server, (const struct sockaddr *) &server_addr, 0);

    rc = uv_listen(
            (uv_stream_t*) &backend_server,
            DEFAULT_BACKLOG,
            on_new_connection);

    if (rc)
    {
        log_error("Error listening back-end server: %s", uv_strerror(rc));
        return 1;
    }

    log_info("Start listening for back-end connections on '%s:%d'",
            siri->cfg->listen_backend_address,
            siri->cfg->listen_backend_port);

    return 0;
}

static void on_new_connection(uv_stream_t * server, int status)
{
    if (status < 0)
    {
        log_error("Back-end server connection error: %s", uv_strerror(status));
        return;
    }

    log_debug("Received a back-end server connection request.");

    uv_tcp_t * client =
            sirinet_socket_new(SOCKET_BACKEND, (on_data_cb_t) &on_data);

    uv_tcp_init(loop, client);

    if (uv_accept(server, (uv_stream_t *) client) == 0)
    {
        uv_read_start(
                (uv_stream_t *) client,
                sirinet_socket_alloc_buffer,
                sirinet_socket_on_data);
    }
    else
    {
        uv_close((uv_handle_t *) client, (uv_close_cb) sirinet_socket_free);
    }
}

static void on_data(uv_handle_t * client, sirinet_pkg_t * pkg)
{
#ifdef DEBUG
    log_debug("[Back-end server] Got data (pid: %d, len: %d, tp: %d)",
            pkg->pid, pkg->len, pkg->tp);
#endif

    switch ((bproto_client_t) pkg->tp)
    {
    case BPROTO_AUTH_REQUEST:
        on_auth_request(client, pkg);
        break;
    case BPROTO_FLAGS_UPDATE:
        on_flags_update(client, pkg);
        break;
    case BPROTO_LOG_LEVEL_UPDATE:
        on_log_level_update(client, pkg);
        break;
    case BPROTO_REPL_FINISHED:
        on_repl_finished(client, pkg);
        break;
    case BPROTO_QUERY_SERVER:
        on_query(client, pkg, 0);
        break;
    case BPROTO_QUERY_UPDATE:
        on_query(client, pkg, SIRIDB_QUERY_FLAG_UPDATE_REPLICA);
        break;
    case BPROTO_INSERT_POOL:
        on_insert_pool(client, pkg);
        break;
    case BPROTO_INSERT_SERVER:
        on_insert_server(client, pkg);
        break;
    case BPROTO_REGISTER_SERVER_UPDATE:
        break;
    case BPROTO_REGISTER_SERVER:
        break;
    }

}

static void on_auth_request(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    bproto_server_t rc;
    sirinet_pkg_t * package;
    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    qp_obj_t * qp_uuid = qp_object_new();
    qp_obj_t * qp_dbname = qp_object_new();
    qp_obj_t * qp_flags = qp_object_new();
    qp_obj_t * qp_version = qp_object_new();
    qp_obj_t * qp_min_version = qp_object_new();
    qp_obj_t * qp_dbpath = qp_object_new();
    qp_obj_t * qp_buffer_path = qp_object_new();
    qp_obj_t * qp_buffer_size = qp_object_new();
    qp_obj_t * qp_startup_time = qp_object_new();

    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_uuid) == QP_RAW &&
            qp_next(unpacker, qp_dbname) == QP_RAW &&
            qp_next(unpacker, qp_flags) == QP_INT64 &&
            qp_next(unpacker, qp_version) == QP_RAW &&
            qp_next(unpacker, qp_min_version) == QP_RAW &&
            qp_next(unpacker, qp_dbpath) == QP_RAW &&
            qp_next(unpacker, qp_buffer_path) == QP_RAW &&
            qp_next(unpacker, qp_buffer_size) == QP_INT64 &&
            qp_next(unpacker, qp_startup_time) == QP_INT64)
    {
        rc = siridb_auth_server_request(
                client,
                qp_uuid,
                qp_dbname,
                qp_version,
                qp_min_version);
        if (rc == BPROTO_AUTH_SUCCESS)
        {
            siridb_server_t * server =
                    ((sirinet_socket_t * ) client->data)->origin;

            /* update server flags */
            siridb_server_update_flags(server->flags, qp_flags->via->int64);

            /* update other server properties */
            server->dbpath = strdup(qp_dbpath->via->raw);
            server->buffer_path = strdup(qp_buffer_path->via->raw);
            server->buffer_size = qp_buffer_size->via->int64;
            server->startup_time = qp_startup_time->via->int64;

            log_info("Accepting back-end server connection: '%s'",
                    server->name);
        }
        else
        {
            log_warning("Refusing back-end connection (error code: %d)", rc);
        }

        package = sirinet_pkg_new(pkg->pid, 0, rc, NULL);

        /* ignore result code, signal can be raised */
        sirinet_pkg_send((uv_stream_t *) client, package);
        free(package);
    }
    else
    {
        log_error("Invalid back-end 'on_auth_request' received.");
    }
    qp_object_free(qp_uuid);
    qp_object_free(qp_dbname);
    qp_object_free(qp_flags);
    qp_object_free(qp_version);
    qp_object_free(qp_min_version);
    qp_object_free(qp_dbpath);
    qp_object_free(qp_buffer_path);
    qp_object_free(qp_buffer_size);
    qp_object_free(qp_startup_time);
    qp_unpacker_free(unpacker);
}

static void on_flags_update(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    qp_obj_t * qp_flags = qp_object_new();

    if (qp_next(unpacker, qp_flags) == QP_INT64)
    {
        /* update server flags */
        siridb_server_update_flags(server->flags, qp_flags->via->int64);

        /* if this is the replica see if we have anything to update */
        if (    siridb->replica == server &&
                siridb_replicate_is_idle(siridb->replicate))
        {
            siridb_replicate_start(siridb->replicate);
        }

        log_info("Status received from '%s' (status: %d)",
                server->name,
                server->flags);

        package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_FLAGS, NULL);

        /* ignore result code, signal can be raised */
        sirinet_pkg_send((uv_stream_t *) client, package);
        free(package);
    }
    else
    {
        log_error("Invalid back-end 'on_flags_update' received.");
    }
    qp_object_free(qp_flags);
    qp_unpacker_free(unpacker);
}

static void on_log_level_update(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    qp_obj_t * qp_log_level = qp_object_new();

    if (qp_next(unpacker, qp_log_level) == QP_INT64)
    {
        /* update log level */
        logger_set_level(qp_log_level->via->int64);

        log_info("Log level update received from '%s' (%s)",
                server->name,
                Logger.level_name);

        package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_LOG_LEVEL, NULL);
        if (package != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send((uv_stream_t *) client, package);
            free(package);
        }
    }
    else
    {
        log_error("Invalid back-end 'on_log_level_update' received.");
    }
    qp_object_free(qp_log_level);
    qp_unpacker_free(unpacker);
}

static void on_repl_finished(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    sirinet_pkg_t * package;

    if (siridb->server->flags & SERVER_FLAG_SYNCHRONIZING)
    {
        siridb->server->flags &= ~SERVER_FLAG_SYNCHRONIZING;

        siridb_servers_send_flags(siridb->servers);
    }

    package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_REPL_FINISHED, NULL);
    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send((uv_stream_t *) client, package);
        free(package);
    }
}

static void on_query(uv_handle_t * client, sirinet_pkg_t * pkg, int flags)
{
    SERVER_CHECK_AUTHENTICATED(server)

    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    if (unpacker == NULL)
    {
        return;  /* signal is raised */
    }
    qp_obj_t * qp_query = qp_object_new();
    if (qp_query == NULL)
    {
        qp_unpacker_free(unpacker);
        return;  /* signal is raised */
    }
    qp_obj_t * qp_time_precision = qp_object_new();
    if (qp_time_precision == NULL)
    {
        qp_object_free(qp_query);
        qp_unpacker_free(unpacker);
        return;  /* signal is raised */
    }

    if (flags & SIRIDB_QUERY_FLAG_UPDATE_REPLICA)
    {
        siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
        if (siridb->replica != NULL)
        {
            pkg->tp = BPROTO_QUERY_SERVER;
            siridb_fifo_append(siridb->fifo, pkg);
        }
    }

    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_query) == QP_RAW &&
            qp_next(unpacker, qp_time_precision) == QP_INT64)
    {
        siridb_query_run(
                pkg->pid,
                client,
                qp_query->via->raw,
                qp_query->len,
                (siridb_timep_t) qp_time_precision->via->int64,
                0);
    }
    else
    {
        log_error("Invalid back-end 'on_query_server' received.");
    }
    qp_object_free(qp_query);
    qp_object_free(qp_time_precision);
    qp_unpacker_free(unpacker);
}

static void on_insert_pool(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;

    if (siridb->replica != NULL)
    {
#ifdef DEBUG
        assert (siridb->fifo != NULL);
#endif
        pkg->tp = BPROTO_INSERT_SERVER;
        if (siridb_fifo_append(siridb->fifo, pkg))
        {
            /* signal is raised */
            sirinet_pkg_t * package =
                    sirinet_pkg_new(pkg->pid, 0, BPROTO_ERR_INSERT, NULL);
            if (package != NULL)
            {
                sirinet_pkg_send((uv_stream_t *) client, package);
                free(package);
            }
        }
    }
    if (!siri_err)
    {
        on_insert_server(client, pkg);
    }
}

static void on_insert_server(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);

    if (unpacker != NULL)
    {
        package = ( !(siridb->server->flags & SERVER_FLAG_RUNNING) ||
                    (siridb->server->flags & SERVER_FLAG_PAUSED) ||
                    siridb_insert_local(siridb, unpacker)) ?
                sirinet_pkg_new(pkg->pid, 0, BPROTO_ERR_INSERT, NULL) :
                sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_INSERT, NULL);
        if (package != NULL)
        {
            sirinet_pkg_send((uv_stream_t *) client, package);
            free(package);
        }
        qp_unpacker_free(unpacker);
    }
}
