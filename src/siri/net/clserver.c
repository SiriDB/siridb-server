/*
 * clserver.c - TCP server for serving client requests.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#define _GNU_SOURCE
#include <siri/net/clserver.h>
#include <siri/net/protocol.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>
#include <msgpack.h>
#include <stdbool.h>
#include <stdio.h>
#include <siri/siri.h>
#include <siri/db/auth.h>
#include <siri/db/query.h>
#include <qpack/qpack.h>
#include <siri/db/insert.h>
#include <siri/net/socket.h>
#include <assert.h>
#include <siri/version.h>

#define DEFAULT_BACKLOG 128
#define CHECK_SIRIDB(ssocket)                                               \
sirinet_socket_t * ssocket = client->data;                                  \
if (ssocket->siridb == NULL)                                                \
{                                                                           \
    sirinet_pkg_t * package;                                                \
    package = sirinet_pkg_new(pkg->pid, 0, CPROTO_ERR_NOT_AUTHENTICATED, NULL); \
    if (package != NULL)                                                    \
    {                                                                       \
        sirinet_pkg_send((uv_stream_t *) client, package);                  \
        free(package);                                                      \
    }                                                                       \
    return;                                                                 \
}

static uv_loop_t * loop = NULL;
static struct sockaddr_in client_addr;
static uv_tcp_t client_server;

static void on_data(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_new_connection(uv_stream_t * server, int status);
static void on_auth_request(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_query(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_insert(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_ping(uv_handle_t * client, sirinet_pkg_t * pkg);
static void on_info(uv_handle_t * client, sirinet_pkg_t * pkg);
static void CLSERVER_send_server_error(
        siridb_t * siridb,
        uv_stream_t * stream,
        sirinet_pkg_t * pkg);
static void CLSERVER_send_pool_error(
        uv_stream_t * stream,
        sirinet_pkg_t * pkg);
static int CLSERVER_on_info_cb(siridb_t * siridb, qp_packer_t * packer);

#define POOL_ERR_MSG \
    "At least one pool has no server available to process the request"

#define POOL_ERR_LEN 64  // exact length of POOL_ERR_MSG


int sirinet_clserver_init(siri_t * siri)
{
#ifdef DEBUG
    assert (loop == NULL && siri->loop != NULL);
#endif

    int rc;

    /* bind loop to the given loop */
    loop = siri->loop;

    uv_tcp_init(loop, &client_server);

    uv_ip4_addr(
            siri->cfg->listen_client_address,
            siri->cfg->listen_client_port,
            &client_addr);

    /* make sure data is set to NULL so we later on can check this value. */
    client_server.data = NULL;

    uv_tcp_bind(&client_server, (const struct sockaddr *) &client_addr, 0);

    rc = uv_listen(
            (uv_stream_t*) &client_server,
            DEFAULT_BACKLOG,
            on_new_connection);

    if (rc)
    {
        log_error("Error listening client server: %s", uv_strerror(rc));
        return 1;
    }

    log_info("Start listening for client connections on '%s:%d'",
            siri->cfg->listen_client_address,
            siri->cfg->listen_client_port);

    return 0;
}

static void on_new_connection(uv_stream_t * server, int status)
{
    log_debug("Received a client connection request.");

    if (status < 0)
    {
        log_error("Client connection error: %s", uv_strerror(status));
        return;
    }
    uv_tcp_t * client =
            sirinet_socket_new(SOCKET_CLIENT, (on_data_cb_t) &on_data);

    uv_tcp_init(loop, client);

    if (uv_accept(server, (uv_stream_t*) client) == 0)
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
    log_debug("[Client server] Got data (pid: %d, len: %d, tp: %d)",
            pkg->pid, pkg->len, pkg->tp);
#endif

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
        case CPROTO_REQ_INFO:
            on_info(client, pkg);
            break;
        }
    }
    else
    {
        /* data->siridb can be NULL here, make sure we can handle this state */
        CLSERVER_send_server_error(
                ((sirinet_socket_t *) client->data)->siridb,
                (uv_stream_t *) client,
                pkg);
    }
}

static void on_auth_request(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    cproto_server_t rc;
    sirinet_pkg_t * package;
    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    qp_obj_t * qp_username = qp_object_new();
    qp_obj_t * qp_password = qp_object_new();
    qp_obj_t * qp_dbname = qp_object_new();


    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_username) == QP_RAW &&
            qp_next(unpacker, qp_password) == QP_RAW &&
            qp_next(unpacker, qp_dbname) == QP_RAW)
    {
        rc = siridb_auth_user_request(
                client,
                qp_username,
                qp_password,
                qp_dbname);
        package = sirinet_pkg_new(pkg->pid, 0, rc, NULL);

        /* ignore result code, signal can be raised */
        sirinet_pkg_send((uv_stream_t *) client, package);
        free(package);
    }
    else
    {
        log_error("Invalid 'on_auth_request' received.");
    }
    qp_object_free(qp_username);
    qp_object_free(qp_password);
    qp_object_free(qp_dbname);
    qp_unpacker_free(unpacker);
}

/*
 * A signal is raised in case an allocation error occurred.
 */
static void CLSERVER_send_server_error(
        siridb_t * siridb,
        uv_stream_t * stream,
        sirinet_pkg_t * pkg)
{
    /* WARNING: siridb can be NULL here */

    char * err_msg;
    int len = (siridb == NULL) ?
            asprintf(
            &err_msg,
            "error, not accepting the request because the sever is not running")
            :
            asprintf(
            &err_msg,
            "error, '%s' is not accepting then request because of having "
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
            sirinet_pkg_send(stream, package);

            /* free package and err_msg*/
            free(package);
        }
        free(err_msg);
    }
}

/*
 * A signal is raised in case an allocation error occurred.
 */
static void CLSERVER_send_pool_error(
        uv_stream_t * stream,
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
        sirinet_pkg_send(stream, package);

        /* free package and err_msg*/
        free(package);
    }
}

static void on_query(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB(ssocket)

    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    qp_obj_t * qp_query = qp_object_new();
    qp_obj_t * qp_time_precision = qp_object_new();
    siridb_timep_t tp = SIRIDB_TIME_DEFAULT;

    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_query) == QP_RAW &&
            qp_next(unpacker, qp_time_precision))
    {
        if (qp_time_precision->tp == QP_INT64 &&
                (tp = (siridb_timep_t) qp_time_precision->via->int64) !=
                ssocket->siridb->time->precision)
        {
            tp %= SIRIDB_TIME_END;
        }

        siridb_query_run(
                pkg->pid,
                client,
                qp_query->via->raw,
                qp_query->len,
                tp,
                SIRIDB_QUERY_FLAG_MASTER);
    }
    qp_object_free(qp_query);
    qp_object_free(qp_time_precision);
    qp_unpacker_free(unpacker);
}

static void on_insert(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB(ssocket)

    siridb_t * siridb = ssocket->siridb;

    /* only when when the flag is EXACTLY running we can continue */
    if (siridb->server->flags != SERVER_FLAG_RUNNING)
    {
        CLSERVER_send_server_error(siridb, (uv_stream_t *) client, pkg);
        return;
    }

    if (!siridb_pools_available(siridb))
    {
        CLSERVER_send_pool_error((uv_stream_t *) client, pkg);
        return;
    }

    qp_unpacker_t * unpacker = qp_unpacker_new(pkg->data, pkg->len);
    if (unpacker == NULL)
    {
        return;  /* signal is raised */
    }
    qp_obj_t * qp_obj = qp_object_new();
    if (qp_obj == NULL)
    {
        free(unpacker);
        return;
    }
    qp_packer_t * packer[siridb->pools->len];
    siridb_insert_err_t rc;

    const char * err_msg;

    rc = siridb_insert_assign_pools(siridb, unpacker, qp_obj, packer);

    switch (rc)
    {
    case ERR_EXPECTING_ARRAY:
    case ERR_EXPECTING_SERIES_NAME:
    case ERR_EXPECTING_MAP_OR_ARRAY:
    case ERR_EXPECTING_INTEGER_TS:
    case ERR_TIMESTAMP_OUT_OF_RANGE:
    case ERR_UNSUPPORTED_VALUE:
    case ERR_EXPECTING_AT_LEAST_ONE_POINT:
    case ERR_EXPECTING_NAME_AND_POINTS:
        /* something went wrong, get correct err message */
        err_msg = siridb_insert_err_msg(rc);

        /* create and send package */
        sirinet_pkg_t * package = sirinet_pkg_err(
                pkg->pid,
                strlen(err_msg),
                CPROTO_ERR_INSERT,
                err_msg);

        if (package != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send((uv_stream_t *) client, package);

            /* free package*/
            free(package);
        }

        /* free packer */
        for (size_t n = 0; n < siridb->pools->len; n++)
        {
            qp_packer_free(packer[n]);
        }
        break;
    case ERR_MEM_ALLOC:
        break;
    default:
        siridb_insert_init(
                pkg->pid,
                client,
                (size_t) rc,
                siridb->pools->len,
                packer);
        break;
    }

    /* free qp_object */
    qp_object_free(qp_obj);

    /* free unpacker */
    qp_unpacker_free(unpacker);
}

static void on_ping(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    sirinet_pkg_t * package;
    package = sirinet_pkg_new(pkg->pid, 0, CPROTO_RES_ACK, NULL);

    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send((uv_stream_t *) client, package);
        free(package);
    }
}

static void on_info(uv_handle_t * client, sirinet_pkg_t * pkg)
{
    qp_packer_t * packer = qp_packer_new(128);
    if (packer != NULL)
    {
        qp_add_type(packer, QP_ARRAY_OPEN);
        qp_add_string(packer, SIRIDB_VERSION);
        qp_add_type(packer, QP_ARRAY_OPEN);

        if (!llist_walk(
                siri.siridb_list,
                (llist_cb) CLSERVER_on_info_cb,
                packer))
        {
            sirinet_pkg_t * package;
            package = sirinet_pkg_new(
                    pkg->pid,
                    packer->len,
                    CPROTO_RES_INFO,
                    packer->buffer);

            /* ignore result code, signal can be raised */
            sirinet_pkg_send((uv_stream_t *) client, package);

            free(package);
        }
        qp_packer_free(packer);
    }
}

/*
 * Typedef: llist_cb
 * Returns 0 if successful or -1 and a SIGNAL is raised if not.
 */
static int CLSERVER_on_info_cb(siridb_t * siridb, qp_packer_t * packer)
{
    return qp_add_string(packer, siridb->dbname);
}
