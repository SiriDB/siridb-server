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
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/auth.h>
#include <siri/db/groups.h>
#include <siri/db/insert.h>
#include <siri/db/query.h>
#include <siri/db/replicate.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/net/bserver.h>
#include <siri/net/pkg.h>
#include <siri/net/protocol.h>
#include <siri/net/socket.h>
#include <siri/optimize.h>
#include <siri/siri.h>
#include <stdlib.h>

#define DEFAULT_BACKLOG 128

#define SERVER_CHECK_AUTHENTICATED(server)                                    \
siridb_server_t * server = ((sirinet_socket_t * ) client->data)->origin;      \
if (!(server->flags & SERVER_FLAG_AUTHENTICATED))                             \
{                                                                             \
    sirinet_pkg_t * package;                                                  \
    package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ERR_NOT_AUTHENTICATED, NULL); \
    sirinet_pkg_send((uv_stream_t *) client, package);                        \
    return;                                                                   \
}

static void BSERVER_flags_update(
        siridb_t * siridb,
        siridb_server_t * server,
        int64_t flags);
static void on_new_connection(uv_stream_t * server, int status);
static void on_data(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_auth_request(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_flags_update(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_log_level_update(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_repl_finished(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_query(uv_stream_t * client, sirinet_pkg_t * pkg, int flags);
static void on_insert(uv_stream_t * client, sirinet_pkg_t * pkg, int flags);
static void on_register_server(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_drop_series(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_req_groups(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_enable_backup_mode(uv_stream_t * client, sirinet_pkg_t * pkg);
static void on_disable_backup_mode(uv_stream_t * client, sirinet_pkg_t * pkg);

static uv_loop_t * loop = NULL;
static struct sockaddr_storage server_addr;
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

    /* make sure data is set to NULL so we later on can check this value. */
    backend_server.data = NULL;

    if (siri->cfg->ip_support == IP_SUPPORT_IPV4ONLY)
    {
        uv_ip4_addr(
                "0.0.0.0",
                siri->cfg->listen_backend_port,
                (struct sockaddr_in *) &server_addr);
    }
    else
    {
		uv_ip6_addr(
				"::",
				siri->cfg->listen_backend_port,
				(struct sockaddr_in6 *) &server_addr);
    }

    uv_tcp_bind(
    		&backend_server,
			(const struct sockaddr *) &server_addr,
			(siri->cfg->ip_support == IP_SUPPORT_IPV6ONLY) ?
					UV_TCP_IPV6ONLY : 0);

    rc = uv_listen(
            (uv_stream_t*) &backend_server,
            DEFAULT_BACKLOG,
            on_new_connection);

    if (rc)
    {
        log_error("Error listening back-end server: %s", uv_strerror(rc));
        return 1;
    }

    log_info("Start listening for back-end connections on port %d",
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
    if (client != NULL)
    {
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
            sirinet_socket_decref(client);
        }
    }
}

static void BSERVER_flags_update(
        siridb_t * siridb,
        siridb_server_t * server,
        int64_t flags)
{
    /* update server flags */
    siridb_server_update_flags(server->flags, flags);

    if (server->socket == NULL)
    {
        /* connect in case we do not have a connection yet */
        siridb_server_connect(siridb, server);
    }

    /* if server is re-indexing, update status */
    if (    (siridb->flags & SIRIDB_FLAG_REINDEXING) &&
            (~siridb->server->flags & SERVER_FLAG_REINDEXING) &&
            (~server->flags & SERVER_FLAG_REINDEXING))
    {
        siridb_reindex_status_update(siridb);
    }

    /* if this is the replica see if we have anything to update */
    if (    siridb->replica == server &&
            siridb_replicate_is_idle(siridb->replicate))
    {
        siridb_replicate_start(siridb->replicate);
    }

    log_info("Status received from '%s' (status: %d)",
            server->name,
            server->flags);
}

static void on_data(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    if (Logger.level == LOGGER_DEBUG)
    {
    	char addr_port[ADDR_BUF_SZ];
    	if (sirinet_addr_and_port(addr_port, client) == 0)
    	{
    	    log_debug(
					"Package received from server '%s' "
					"(pid: %u, len: %lu, tp: %s)",
					addr_port,
    	            pkg->pid,
    	            pkg->len,
    	            sirinet_bproto_client_str(pkg->tp));
    	}
    }

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
        on_insert(client, pkg, INSERT_FLAG_POOL);
        break;
    case BPROTO_INSERT_SERVER:
        on_insert(client, pkg, 0);
        break;
    case BPROTO_INSERT_TEST_POOL:
        on_insert(client, pkg, INSERT_FLAG_POOL | INSERT_FLAG_TEST);
        break;
    case BPROTO_INSERT_TEST_SERVER:
        on_insert(client, pkg, INSERT_FLAG_TEST);
        break;
    case BPROTO_INSERT_TESTED_POOL:
        on_insert(client, pkg, INSERT_FLAG_POOL | INSERT_FLAG_TESTED);
        break;
    case BPROTO_INSERT_TESTED_SERVER:
        on_insert(client, pkg, INSERT_FLAG_TESTED);
        break;
    case BPROTO_REGISTER_SERVER:
        on_register_server(client, pkg);
        break;
    case BPROTO_DROP_SERIES:
        on_drop_series(client, pkg);
        break;
    case BPROTO_REQ_GROUPS:
        on_req_groups(client, pkg);
        break;
    case BPROTO_ENABLE_BACKUP_MODE:
        on_enable_backup_mode(client, pkg);
        break;
    case BPROTO_DISABLE_BACKUP_MODE:
        on_disable_backup_mode(client, pkg);
        break;
    }

}

static void on_auth_request(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    bproto_server_t rc;
    sirinet_pkg_t * package;
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_uuid;
    qp_obj_t qp_dbname;
    qp_obj_t qp_flags;
    qp_obj_t qp_version;
    qp_obj_t qp_min_version;
    qp_obj_t qp_ip_support;
    qp_obj_t qp_libuv;
    qp_obj_t qp_dbpath;
    qp_obj_t qp_buffer_path;
    qp_obj_t qp_buffer_size;
    qp_obj_t qp_startup_time;
    qp_obj_t qp_address;
    qp_obj_t qp_port;

    if (    qp_is_array(qp_next(&unpacker, NULL)) &&
            qp_next(&unpacker, &qp_uuid) == QP_RAW &&
            qp_next(&unpacker, &qp_dbname) == QP_RAW &&
            qp_is_raw_term(&qp_dbname) &&
            qp_next(&unpacker, &qp_flags) == QP_INT64 &&
            qp_next(&unpacker, &qp_version) == QP_RAW &&
            qp_is_raw_term(&qp_version) &&
            qp_next(&unpacker, &qp_min_version) == QP_RAW &&
            qp_is_raw_term(&qp_min_version) &&
			qp_next(&unpacker, &qp_ip_support) == QP_INT64 &&
            qp_next(&unpacker, &qp_libuv) == QP_RAW &&
            qp_is_raw_term(&qp_libuv) &&
            qp_next(&unpacker, &qp_dbpath) == QP_RAW &&
            qp_is_raw_term(&qp_dbpath) &&
            qp_next(&unpacker, &qp_buffer_path) == QP_RAW &&
            qp_is_raw_term(&qp_buffer_path) &&
            qp_next(&unpacker, &qp_buffer_size) == QP_INT64 &&
            qp_next(&unpacker, &qp_startup_time) == QP_INT64 &&
            qp_next(&unpacker, &qp_address) == QP_RAW &&
            qp_is_raw_term(&qp_address) &&
            qp_next(&unpacker, &qp_port) == QP_INT64)
    {
        rc = siridb_auth_server_request(
                client,
                &qp_uuid,
                &qp_dbname,
                &qp_version,
                &qp_min_version);
        if (rc == BPROTO_AUTH_SUCCESS)
        {
            siridb_server_t * server =
                    ((sirinet_socket_t * ) client->data)->origin;
            siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;

            /* check and update flags */
            BSERVER_flags_update(siridb, server, qp_flags.via.int64);

            /* update server address if needed */
            siridb_server_update_address(
                    siridb,
                    server,
                    qp_address.via.raw,
                    (uint16_t) qp_port.via.int64);

            /* update other server properties */
            free(server->dbpath);
            server->dbpath = strdup(qp_dbpath.via.raw);
            free(server->buffer_path);
            server->buffer_path = strdup(qp_buffer_path.via.raw);
            free(server->libuv);
            server->libuv = strdup(qp_libuv.via.raw);
            server->buffer_size = (size_t) qp_buffer_size.via.int64;
            server->startup_time = (uint32_t) qp_startup_time.via.int64;
            server->ip_support = (uint8_t) qp_ip_support.via.int64;

            log_info("Accepting back-end server connection: '%s'",
                    server->name);
        }
        else
        {
            log_warning("Refusing back-end connection (error code: %d)", rc);
        }

        package = sirinet_pkg_new(pkg->pid, 0, rc, NULL);

        /* ignore result code, signal can be raised */
        sirinet_pkg_send(client, package);
    }
    else
    {
        log_error("Invalid back-end 'on_auth_request' received.");
    }
}

static void on_flags_update(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_flags;

    if (qp_next(&unpacker, &qp_flags) == QP_INT64)
    {
        /* check and update flags */
        BSERVER_flags_update(siridb, server, qp_flags.via.int64);

        package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_FLAGS, NULL);

        /* ignore result code, signal can be raised */
        sirinet_pkg_send(client, package);
    }
    else
    {
        log_error("Invalid back-end 'on_flags_update' received.");
    }
}

static void on_log_level_update(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_log_level;

    if (qp_next(&unpacker, &qp_log_level) == QP_INT64)
    {
        /* update log level */
        logger_set_level(qp_log_level.via.int64);

        log_info("Log level update received from '%s' (%s)",
                server->name,
                Logger.level_name);

        package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_LOG_LEVEL, NULL);
        if (package != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send(client, package);
        }
    }
    else
    {
        log_error("Invalid back-end 'on_log_level_update' received.");
    }
}

static void on_repl_finished(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    sirinet_pkg_t * package;

    if (siridb->server->flags & SERVER_FLAG_SYNCHRONIZING)
    {
        /* remove synchronization flag */
        siridb->server->flags &= ~SERVER_FLAG_SYNCHRONIZING;

        /* continue optimize */
        siri_optimize_continue();

        /* start re-index task if needed */
        if (siridb->reindex != NULL)
        {
            siridb_reindex_start(siridb->reindex->timer);
        }

        siridb_servers_send_flags(siridb->servers);
    }

    package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ACK_REPL_FINISHED, NULL);
    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send(client, package);
    }
}

static void on_query(uv_stream_t * client, sirinet_pkg_t * pkg, int flags)
{
    SERVER_CHECK_AUTHENTICATED(server)

    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);

    qp_obj_t qp_query;
    qp_obj_t qp_time_precision;

    if (flags & SIRIDB_QUERY_FLAG_UPDATE_REPLICA)
    {
        siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
        if (siridb->replica != NULL)
        {
            pkg->tp = BPROTO_QUERY_SERVER;
            siridb_replicate_pkg(siridb, pkg);
        }
    }

    if (    qp_is_array(qp_next(&unpacker, NULL)) &&
            qp_next(&unpacker, &qp_query) == QP_RAW &&
            qp_next(&unpacker, &qp_time_precision) == QP_INT64)
    {
        siridb_query_run(
                pkg->pid,
                client,
                qp_query.via.raw,
                qp_query.len,
                (siridb_timep_t) qp_time_precision.via.int64,
                0);
    }
    else
    {
        log_error("Invalid back-end 'on_query_server' received.");
    }
}

static void on_insert(uv_stream_t * client, sirinet_pkg_t * pkg, int flags)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    if ((flags & INSERT_FLAG_POOL) && siridb->replica != NULL)
    {
#ifdef DEBUG
        assert (siridb->fifo != NULL);
#endif
        sirinet_pkg_t * repl_pkg;

        if ((siridb->replicate->initsync == NULL))
        {
            pkg->tp = (flags & INSERT_FLAG_TEST) ?
                    BPROTO_INSERT_TEST_SERVER : (flags & INSERT_FLAG_TESTED) ?
                    BPROTO_INSERT_TESTED_SERVER : BPROTO_INSERT_SERVER;
            repl_pkg = pkg;
        }
        else
        {
            repl_pkg = siridb_replicate_pkg_filter(
                    siridb,
                    pkg->data,
                    pkg->len,
                    flags);
        }

        if (repl_pkg == NULL || siridb_replicate_pkg(siridb, repl_pkg))
        {
            /* signal is raised */
            sirinet_pkg_t * package =
                    sirinet_pkg_new(pkg->pid, 0, BPROTO_ERR_INSERT, NULL);
            if (package != NULL)
            {
                sirinet_pkg_send(client, package);
            }
        }

        if (repl_pkg != pkg)
        {
            free(repl_pkg);
        }
    }

    if (!siri_err)
    {
        /*
         * We do not check here for backup mode since we want back-end inserts
         * to finish. We rather prevent servers from starting new insert tasks.
         */
        if (    (~siridb->server->flags & SERVER_FLAG_RUNNING) ||
                insert_init_backend_local(siridb, client, pkg, flags))
        {
            package = sirinet_pkg_new(pkg->pid, 0, BPROTO_ERR_INSERT, NULL);
            if (package != NULL)
            {
                sirinet_pkg_send(client, package);
            }
        }
    }
}

static void on_register_server(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    siridb_server_t * new_server;

    if (    siridb->server->flags != SERVER_FLAG_RUNNING ||
            (siridb->flags & SIRIDB_FLAG_REINDEXING))
    {
        log_error("Cannot register new server because of having status %d",
                siridb->server->flags);

        package = sirinet_pkg_new(
                pkg->pid,
                0,
                BPROTO_ERR_REGISTER_SERVER,
                NULL);
    }
    else
    {
        new_server = siridb_server_register(siridb, pkg->data, pkg->len);
        package = (new_server == NULL) ?
                /* a signal might be raised */
                sirinet_pkg_new(
                        pkg->pid,
                        0,
                        BPROTO_ERR_REGISTER_SERVER,
                        NULL) :
                sirinet_pkg_new(
                        pkg->pid,
                        0,
                        BPROTO_ACK_REGISTER_SERVER,
                        NULL);
    }

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }
}

static void on_drop_series(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package = NULL;
    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;

    if (    (~siridb->server->flags & SERVER_FLAG_RUNNING) ||
            (siridb->server->flags & SERVER_FLAG_BACKUP_MODE))
    {
        log_error("Cannot drop series because of having status %d",
                siridb->server->flags);

        package = sirinet_pkg_new(
                pkg->pid,
                0,
                BPROTO_ERR_DROP_SERIES,
                NULL);
    }
    else
    {
        qp_obj_t qp_series_name;

        qp_unpacker_t unpacker;
        qp_unpacker_init(&unpacker, pkg->data, pkg->len);
        qp_next(&unpacker, &qp_series_name);
        if (qp_is_raw_term(&qp_series_name))
        {
            siridb_series_t * series;
            series = ct_get(siridb->series, qp_series_name.via.raw);
            if (series != NULL)
            {
                uv_mutex_lock(&siridb->series_mutex);

                siridb_series_drop(siridb, series);

                uv_mutex_unlock(&siridb->series_mutex);

                siridb_series_flush_dropped(siridb);
            }
            else
            {
                log_warning(
                        "Received a request to drop series '%s' but "
                        "the series is not found (already dropped?)",
                        qp_series_name.via.raw);
            }
            package = sirinet_pkg_new(
                    pkg->pid,
                    0,
                    BPROTO_ACK_DROP_SERIES,
                    NULL);
        }
        else
        {
            log_error(
                    "Illegal back-end dropped series package "
                    "received, probably the series name was not "
                    "terminated?");
        }
    }

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }
}

static void on_req_groups(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    sirinet_pkg_t * package = siridb_groups_pkg(siridb->groups, pkg->pid);

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }
}

static void on_enable_backup_mode(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    sirinet_pkg_t * package;

    if (    (~siridb->server->flags & SERVER_FLAG_RUNNING) ||
            (siridb->flags & SIRIDB_FLAG_REINDEXING) ||
            (siridb->server->flags & SERVER_FLAG_BACKUP_MODE))
    {
        log_error(
                "Cannot set server '%s' in backup mode because of "
                "having status %d",
                siridb->server->name,
                siridb->server->flags);

        package = sirinet_pkg_new(
                pkg->pid,
                0,
                BPROTO_ERR_ENABLE_BACKUP_MODE,
                NULL);
    }
    else
    {
        package = sirinet_pkg_new(
                pkg->pid,
                0,
                (siri_backup_enable(&siri, siridb)) ?
                        BPROTO_ERR_ENABLE_BACKUP_MODE :
                        BPROTO_ACK_ENABLE_BACKUP_MODE,
                NULL);
    }

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }

}

static void on_disable_backup_mode(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    siridb_t * siridb = ((sirinet_socket_t * ) client->data)->siridb;
    sirinet_pkg_t * package = NULL;

    if (    (~siridb->server->flags & SERVER_FLAG_RUNNING) ||
            (siridb->flags & SIRIDB_FLAG_REINDEXING) ||
            (~siridb->server->flags & SERVER_FLAG_BACKUP_MODE))
    {
        log_error(
                "Cannot clear server '%s' from backup mode because of "
                "having status %d",
                siridb->server->name,
                siridb->server->flags);

        package = sirinet_pkg_new(
                pkg->pid,
                0,
                BPROTO_ERR_DISABLE_BACKUP_MODE,
                NULL);
    }
    else
    {
        package = sirinet_pkg_new(
                pkg->pid,
                0,
                (siri_backup_disable(&siri, siridb)) ?
                        BPROTO_ERR_DISABLE_BACKUP_MODE :
                        BPROTO_ACK_DISABLE_BACKUP_MODE,
                NULL);
    }

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }
}

