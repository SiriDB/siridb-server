/*
 * clserver.c - Listen for client requests.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <lock/lock.h>
#include <math.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/siri.h>
#include <siri/service/account.h>
#include <siri/service/request.h>
#include <siri/db/access.h>
#include <siri/db/auth.h>
#include <siri/db/insert.h>
#include <siri/db/query.h>
#include <siri/db/replicate.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/db/users.h>
#include <siri/err.h>
#include <siri/net/clserver.h>
#include <siri/net/promises.h>
#include <siri/net/protocol.h>
#include <siri/net/tcp.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const unsigned long int WARNING_PKG_SIZE = RESET_BUF_SIZE;

/*
 * note: this size is chosen to be 65535 but is not restricted to 16bit and
 *          can be larger if required.
 */
#define MAX_QUERY_PKG_SIZE 65535

#define DEFAULT_BACKLOG 128
#define CHECK_SIRIDB(client__, siridb__)                                \
siridb_t * siridb__ = (client__)->siridb;                               \
if (siridb__ == NULL)                                                   \
{                                                                       \
    sirinet_pkg_t * package;                                            \
    package = sirinet_pkg_new(                                          \
        pkg->pid, 0, CPROTO_ERR_NOT_AUTHENTICATED, NULL);               \
    if (package != NULL)                                                \
    {                                                                   \
        sirinet_pkg_send(client, package);                              \
    }                                                                   \
    return;                                                             \
}

static const int SERVER_RUNNING_REINDEXING =
        SERVER_FLAG_RUNNING + SERVER_FLAG_REINDEXING;

static uv_loop_t * loop = NULL;
static struct sockaddr_storage client_addr;
static uv_tcp_t client_server_tcp;
static uv_pipe_t client_server_pipe;

static void on_tcp_new_connection(uv_stream_t * server, int status);
static void on_pipe_new_connection(uv_stream_t * server, int status);
static void on_data(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void on_stream_data(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void on_auth_request(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void on_query(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void on_insert(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void on_ping(sirinet_stream_t * client, sirinet_pkg_t * pkg);

static void on_reqfile(
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg,
        sirinet_clserver_getfile getfile);
static void on_register_server(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void on_req_service(sirinet_stream_t * client, sirinet_pkg_t * pkg);
static void CLSERVER_send_server_error(
        siridb_t * siridb,
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg);
static void CLSERVER_send_pool_error(
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg);
static void CLSERVER_on_register_server_response(
        vec_t * promises,
        siridb_server_async_t * server_reg);

#define POOL_ERR_MSG \
    "At least one pool has no server available to process the request"

#define POOL_ERR_LEN 64  /* exact length of POOL_ERR_MSG  */


int sirinet_clserver_init(siri_t * siri)
{
    assert (loop == NULL && siri->loop != NULL);

    int rc;
    int ip_v6 = 0;  /* false */
    char * ip;

    /* bind loop to the given loop */
    loop = siri->loop;

    uv_tcp_init(loop, &client_server_tcp);
    uv_pipe_init(loop, &client_server_pipe, 0);

    /* make sure data is set to NULL so we later on can check this value. */
    client_server_tcp.data = NULL;
    client_server_pipe.data = NULL;

    if (siri->cfg->bind_client_addr != NULL)
    {
        struct in6_addr sa6;
        if (inet_pton(AF_INET6, siri->cfg->bind_client_addr, &sa6))
        {
            ip_v6 = 1;  /* true */
        }
        ip = siri->cfg->bind_client_addr;
    }
    else if (siri->cfg->ip_support == IP_SUPPORT_IPV4ONLY)
    {
        ip = "0.0.0.0";
    }
    else
    {
        ip = "::";
        ip_v6 = 1;  /* true */
    }

    if (ip_v6)
    {
        uv_ip6_addr(
                ip,
                siri->cfg->listen_client_port,
                (struct sockaddr_in6 *) &client_addr);
    }
    else
    {
        uv_ip4_addr(
                ip,
                siri->cfg->listen_client_port,
                (struct sockaddr_in *) &client_addr);
    }

    rc = uv_tcp_bind(
            &client_server_tcp,
            (const struct sockaddr *) &client_addr,
            (siri->cfg->ip_support == IP_SUPPORT_IPV6ONLY) ?
                    UV_TCP_IPV6ONLY : 0);

    if (rc)
    {
        log_error("Error binding TCP client server: %s", uv_strerror(rc));
        return 1;
    }

    rc = uv_listen(
            (uv_stream_t *) &client_server_tcp,
            DEFAULT_BACKLOG,
            on_tcp_new_connection);

    if (rc)
    {
        log_error("Error listening TCP client server: %s", uv_strerror(rc));
        return 1;
    }

    log_info("Start listening for TCP client connections on port %d",
            siri->cfg->listen_client_port);

    if (siri->cfg->pipe_support)
    {
        char * pipe_name = siri->cfg->pipe_client_name;

        rc = uv_pipe_bind(&client_server_pipe, pipe_name);

        if (rc)
        {
            log_error("Error binding pipe client server: %s", uv_strerror(rc));
            return 1;
        }

        rc = uv_listen(
                (uv_stream_t *) &client_server_pipe,
                DEFAULT_BACKLOG,
                on_pipe_new_connection);

        if (rc)
        {
            log_error(
                    "Error listening pipe client server: %s", uv_strerror(rc));
            return 1;
        }

        log_info("Start listening for pipe client connections on '%s'",
                pipe_name);
    }

    return 0;
}

static void on_tcp_new_connection(uv_stream_t * server, int status)
{
    log_debug("Received a TCP client connection request.");

    if (status < 0)
    {
        log_error("TCP client connection error: %s", uv_strerror(status));
        return;
    }
    sirinet_stream_t * client = sirinet_stream_new(
            STREAM_TCP_CLIENT, (on_data_cb_t) &on_stream_data);

    if (client != NULL)
    {
        uv_tcp_init(loop, (uv_tcp_t *) client->stream);

        if (uv_accept(server, client->stream) == 0)
        {
            uv_read_start(
                    client->stream,
                    sirinet_stream_alloc_buffer,
                    sirinet_stream_on_data);
        }
        else
        {
            sirinet_stream_decref(client);
        }
    }
}

static void on_pipe_new_connection(uv_stream_t * server, int status)
{
    log_debug("Received a pipe client connection request.");

    if (status < 0)
    {
        log_error("Pipe client connection error: %s", uv_strerror(status));
        return;
    }
    sirinet_stream_t * client = sirinet_stream_new(
            STREAM_PIPE_CLIENT, (on_data_cb_t) &on_stream_data);

    if (client != NULL)
    {
        uv_pipe_init(loop, (uv_pipe_t *) client->stream, 0);

        if (uv_accept(server,client->stream) == 0)
        {
            uv_read_start(
                    client->stream,
                    sirinet_stream_alloc_buffer,
                    sirinet_stream_on_data);
        }
        else
        {
            sirinet_stream_decref(client);
        }
    }
}

static void on_data(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    /* in case the online flag is not set, we cannot perform any request */
    if (siri.status == SIRI_STATUS_RUNNING)
    {
        switch ((cproto_client_t) pkg->tp)
        {
        case CPROTO_REQ_QUERY:
            on_query(client, pkg);
            break;
        case CPROTO_REQ_INSERT:
            on_insert(client, pkg);
            break;
        case CPROTO_REQ_AUTH:
            on_auth_request(client, pkg);
            break;
        case CPROTO_REQ_PING:
            on_ping(client, pkg);
            break;
        case CPROTO_REQ_REGISTER_SERVER:
            on_register_server(client, pkg);
            break;
        case CPROTO_REQ_FILE_SERVERS:
            on_reqfile(client, pkg, siridb_servers_get_file);
            break;
        case CPROTO_REQ_FILE_USERS:
            on_reqfile(client, pkg, siridb_users_get_file);
            break;
        case CPROTO_REQ_FILE_GROUPS:
            on_reqfile(client, pkg, siridb_groups_get_file);
            break;
        case CPROTO_REQ_FILE_DATABASE:
            on_reqfile(client, pkg, siridb_get_file);
            break;
        case CPROTO_REQ_SERVICE:
            on_req_service(client, pkg);
            break;
        }
    }
    else
    {
        /* siridb can be NULL here, make sure we can handle this state */
        CLSERVER_send_server_error(client->siridb, client, pkg);
    }
}

static void on_stream_data(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    if (Logger.level == LOGGER_DEBUG)
    {
        char * name = sirinet_stream_name(client);
        if (name != NULL)
        {
            log_debug(
                    "Package received from client '%s' "
                    "(pid: %" PRIu16 ", len: %" PRIu32 ", tp: %s)",
                    name,
                    pkg->pid,
                    pkg->len,
                    sirinet_cproto_client_str(pkg->tp));
            free(name);
        }
    }
    else if (pkg->len >= WARNING_PKG_SIZE)
    {
        char * name = sirinet_stream_name(client);
        if (name != NULL)
        {
            log_warning(
                    "Got a large package from '%s' (pid: %d, len: %d, tp: %s)."
                    " A package size smaller than 1MB is recommended!",
                    name,
                    pkg->pid,
                    pkg->len,
                    sirinet_cproto_client_str(pkg->tp));
            free(name);
        }
    }

    on_data(client, pkg);
}

static void on_auth_request(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    cproto_server_t rc;
    sirinet_pkg_t * package;
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_username;
    qp_obj_t qp_password;
    qp_obj_t qp_dbname;

    if (    qp_is_array(qp_next(&unpacker, NULL)) &&
            qp_next(&unpacker, &qp_username) == QP_RAW &&
            qp_next(&unpacker, &qp_password) == QP_RAW &&
            qp_next(&unpacker, &qp_dbname) == QP_RAW)
    {
        rc = siridb_auth_user_request(
                client,
                &qp_username,
                &qp_password,
                &qp_dbname);
        package = sirinet_pkg_new(pkg->pid, 0, rc, NULL);
        if (package != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send(client, package);
        }
    }
    else
    {
        log_error("Invalid 'on_auth_request' received.");
    }
}

/*
 * A signal is raised in case an allocation error occurred.
 */
static void CLSERVER_send_server_error(
        siridb_t * siridb,
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg)
{
    /* WARNING: siridb can be NULL here */

    char * err_msg;
    int len = (siridb == NULL) ?
            asprintf(
            &err_msg,
            "Not accepting the request because the sever is not running")
            :
            asprintf(
            &err_msg,
            "Server '%s' is not accepting the request because of having "
            "status: %u",
            siridb->server->name,
            siridb->server->flags);

    if (len < 0)
    {
        ERR_ALLOC
    }
    else
    {
        log_debug(err_msg);

        sirinet_pkg_t * package = sirinet_pkg_err(
                pkg->pid,
                len,
                CPROTO_ERR_SERVER,
                err_msg);
        if (package != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send(client, package);
        }
        free(err_msg);
    }
}

/*
 * A signal is raised in case an allocation error occurred.
 */
static void CLSERVER_send_pool_error(
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg)
{
    log_debug(POOL_ERR_MSG);

    sirinet_pkg_t * package = sirinet_pkg_err(
            pkg->pid,
            POOL_ERR_LEN,
            CPROTO_ERR_POOL,
            POOL_ERR_MSG);

    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send(client, package);

    }
}

static void on_query(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB(client, siridb)

    if (pkg->len > MAX_QUERY_PKG_SIZE)
    {
        sirinet_pkg_t * package;

        log_error(
                "Received a query exceeding the maximum size. "
                "(%" PRIu32 ", max size: %u)",
                pkg->len,
                MAX_QUERY_PKG_SIZE);

        package = sirinet_pkg_err(
                pkg->pid,
                15,
                CPROTO_ERR_QUERY,
                "Query too long.");

        if (package != NULL)
        {
            sirinet_pkg_send(client, package);
        }

        return;
    }

    qp_unpacker_t unpacker;
    qp_obj_t qp_query;
    qp_obj_t qp_time_precision;
    float factor;
    siridb_timep_t tp = SIRIDB_TIME_DEFAULT;

    qp_unpacker_init(&unpacker, pkg->data, pkg->len);

    if (    qp_is_array(qp_next(&unpacker, NULL)) &&
            qp_next(&unpacker, &qp_query) == QP_RAW)
    {
        qp_next(&unpacker, &qp_time_precision);

        if (qp_time_precision.tp == QP_INT64 &&
                (tp = (siridb_timep_t) qp_time_precision.via.int64) !=
                siridb->time->precision)
        {
            tp %= SIRIDB_TIME_END;
        }
        factor = (tp == SIRIDB_TIME_DEFAULT) ? 0.0 :
                pow(1000.0, tp - siridb->time->precision);

        siridb_query_run(
                pkg->pid,
                client,
                (const char *) qp_query.via.raw,
                qp_query.len,
                factor,
                SIRIDB_QUERY_FLAG_MASTER);
    }
    else
    {
        log_error("Incorrect package received: 'on_query'");
    }
}

static void on_insert(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB(client, siridb)

    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

    if (!siridb_user_check_access(
                (siridb_user_t *) client->origin,
                SIRIDB_ACCESS_INSERT,
                err_msg))
    {
        log_warning("(%s) %s",
                sirinet_cproto_server_str(CPROTO_ERR_USER_ACCESS),
                err_msg);
        sirinet_pkg_t * package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_USER_ACCESS,
                err_msg);

        if (package != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send(client, package);
        }

        return;
    }

    /* only when when the flag is EXACTLY running or
     * running + re-indexing we can continue */
    if (    siridb->server->flags != SERVER_FLAG_RUNNING &&
            siridb->server->flags != SERVER_RUNNING_REINDEXING)
    {
        CLSERVER_send_server_error(siridb, client, pkg);
        return;
    }

    if (!siridb_pools_accessible(siridb))
    {
        CLSERVER_send_pool_error(client, pkg);
        return;
    }

    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);

    siridb_insert_t * insert = siridb_insert_new(
            siridb,
            pkg->pid,
            client);

    if (insert != NULL)
    {
        ssize_t rc = siridb_insert_assign_pools(
                siridb,
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
                        err_msg, unpacker.pt - pkg->data);

                /* create and send package */
                sirinet_pkg_t * package = sirinet_pkg_err(
                        pkg->pid,
                        strlen(err_msg),
                        CPROTO_ERR_INSERT,
                        err_msg);

                if (package != NULL)
                {
                    /* ignore result code, signal can be raised */
                    sirinet_pkg_send(client, package);
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
            break;
        }
    }
}

static void on_ping(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    sirinet_pkg_t * package;
    package = sirinet_pkg_new(pkg->pid, 0, CPROTO_RES_ACK, NULL);

    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send(client, package);
    }
}

/*
 * This function can raise a SIGNAL.
 */
static void on_reqfile(
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg,
        sirinet_clserver_getfile getfile)
{
    CHECK_SIRIDB(client, siridb)
    siridb_user_t * user = client->origin;
    sirinet_pkg_t * package = NULL;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

    if (siridb->server->flags != SERVER_FLAG_RUNNING)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Error getting file because server '%s' having status %d",
                siridb->server->name,
                siridb->server->flags);
        package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_SERVER,
                err_msg);
    }
    else if (!siridb_user_check_access(
            user,
            SIRIDB_ACCESS_PROFILE_FULL,
            err_msg))
    {
        package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_USER_ACCESS,
                err_msg);
    }
    else
    {
        char * content = NULL;
        ssize_t size = getfile(&content, siridb);
        package = (content == NULL) ?
            sirinet_pkg_new(
                    pkg->pid,
                    0,
                    CPROTO_ERR_FILE,
                    NULL) :
            sirinet_pkg_new(
                    pkg->pid,
                    size,
                    CPROTO_RES_FILE,
                    (const unsigned char *) content);
        free(content);
    }

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }
}

/*
 * This function can raise a SIGNAL.
 */
static void on_register_server(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB(client, siridb)
    siridb_user_t * user = client->origin;

    sirinet_pkg_t * package = NULL;
    siridb_server_t * new_server = NULL;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    vec_t * servers = NULL;

    if (siridb->server->flags != SERVER_FLAG_RUNNING)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot register new server because '%s' is having status %d",
                siridb->server->name,
                siridb->server->flags);
        package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_SERVER,
                err_msg);
    }
    if (siridb->flags & SIRIDB_FLAG_REINDEXING)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot register new server because the database has not "
                "finished re-indexing");
        package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_MSG,
                err_msg);
    }
    else if (!siridb_user_check_access(
            user,
            SIRIDB_ACCESS_PROFILE_FULL,
            err_msg))
    {
        package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_USER_ACCESS,
                err_msg);
    }
    else if (!siridb_servers_available(siridb))
    {
        sprintf(err_msg, "At least one server is not online or busy");
        package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_SERVER,
                err_msg);
    }
    else
    {
        /* make a copy of the current servers */
        servers = siridb_servers_other2vec(siridb);
        if (servers == NULL)
        {
            sprintf(err_msg, "Memory allocation error.");
            package = sirinet_pkg_err(
                    pkg->pid,
                    strlen(err_msg),
                    CPROTO_ERR_SERVER,
                    err_msg);
        }
        else
        {
            log_info("Register a new server");
            new_server = siridb_server_register(
                    siridb,
                    (unsigned char *) pkg->data,
                    pkg->len);

            pkg->tp = BPROTO_REGISTER_SERVER;

            if (new_server == NULL)
            {
                /* a signal might be raised */
                package = sirinet_pkg_new(pkg->pid, 0, CPROTO_ERR, NULL);
            }
        }
    }

    if (package != NULL)
    {
        sirinet_pkg_send(client, package);
    }
    else if (new_server != NULL)
    {
        siridb_server_async_t * server_reg = malloc(
                sizeof(siridb_server_async_t));

        if (server_reg == NULL)
        {
            ERR_ALLOC
        }
        else
        {
            /* save the client PID so we can respond to the client */
            server_reg->pid = pkg->pid;
            server_reg->client = client;

            pkg->tp = BPROTO_REGISTER_SERVER;

            if (servers != NULL && (package = sirinet_pkg_dup(pkg)) != NULL)
            {
                /* make sure to decrement the client in the callback */
                sirinet_stream_incref(client);

                siridb_servers_send_pkg(
                        servers,
                        package,
                        0,
                        (sirinet_promises_cb)
                                CLSERVER_on_register_server_response,
                        server_reg);
            }
        }
    }

    /* free the servers or NULL */
    vec_free(servers);
}

static void on_req_service(sirinet_stream_t * client, sirinet_pkg_t * pkg)
{
    qp_unpacker_t unpacker;
    qp_packer_t * packer = NULL;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_username;
    qp_obj_t qp_password;
    qp_obj_t qp_request;
    sirinet_pkg_t * package = NULL;
    cproto_server_t tp;
    char err_msg[SIRI_MAX_SIZE_ERR_MSG];

    if (    qp_is_array(qp_next(&unpacker, NULL)) &&
            qp_next(&unpacker, &qp_username) == QP_RAW &&
            qp_next(&unpacker, &qp_password) == QP_RAW &&
            qp_next(&unpacker, &qp_request) == QP_INT64)
    {
        tp = (siri_service_account_check(
                &siri,
                &qp_username,
                &qp_password,
                err_msg)) ?
            CPROTO_ERR_SERVICE
            :
            siri_service_request(
                    qp_request.via.int64,
                    &unpacker,
                    &packer,
                    pkg->pid,
                    client,
                    err_msg);

        package =
                (tp == CPROTO_DEFERRED) ? NULL :
                (tp == CPROTO_ERR_SERVICE) ? sirinet_pkg_err(
                        pkg->pid,
                        strlen(err_msg),
                        tp,
                        err_msg) :
                (tp == CPROTO_ACK_SERVICE_DATA) ? sirinet_packer2pkg(
                        packer,
                        pkg->pid,
                        tp) : sirinet_pkg_new(pkg->pid, 0, tp, NULL);
    }
    else
    {
        log_error("Invalid service request received.");
        package = sirinet_pkg_new(
                pkg->pid,
                0,
                CPROTO_ERR_SERVICE_INVALID_REQUEST,
                NULL);
    }
    if (package != NULL)
    {
        switch (package->tp)
        {
        case CPROTO_ERR_SERVICE_INVALID_REQUEST:
            log_warning("Received an invalid manage request.");
            break;
        case CPROTO_ERR_SERVICE:
            log_warning("Error handling manage request: %s", err_msg);
            break;
        }

        /* ignore result code, signal can be raised */
        sirinet_pkg_send(client, package);
    }
}

/*
 * Typedef: sirinet_promises_cb
 */
static void CLSERVER_on_register_server_response(
        vec_t * promises,
        siridb_server_async_t * server_reg)
{
    if (promises != NULL)
    {
        sirinet_pkg_t * pkg;
        sirinet_promise_t * promise;
        size_t err_count = 0;
        char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
        size_t i;

        for (i = 0; i < promises->len; i++)
        {
            promise = promises->data[i];
            if (promise == NULL)
            {
                err_count++;
                snprintf(err_msg,
                        SIRIDB_MAX_SIZE_ERR_MSG,
                        "Error occurred while registering the new server on "
                        "at least one server");
            }
            else
            {
                pkg = promise->data;

                if (pkg == NULL || pkg->tp != BPROTO_ACK_REGISTER_SERVER)
                {
                    err_count++;
                    snprintf(err_msg,
                            SIRIDB_MAX_SIZE_ERR_MSG,
                            "Error occurred while registering the new server "
                            "on at least '%s'", promise->server->name);
                }
                /* make sure we free the promise and data */
                free(promise->data);
                sirinet_promise_decref(promise);
            }
        }

        sirinet_pkg_t * package = (err_count) ?
                sirinet_pkg_err(
                        server_reg->pid,
                        strlen(err_msg),
                        CPROTO_ERR_MSG,
                        err_msg) :
                sirinet_pkg_new(
                        server_reg->pid,
                        0,
                        CPROTO_RES_ACK,
                        NULL);

        if (package != NULL)
        {
            sirinet_pkg_send(server_reg->client, package);
        }
    }

    /* decref the client */
    sirinet_stream_decref(server_reg->client);

    /* free server register object */
    free(server_reg);
}
