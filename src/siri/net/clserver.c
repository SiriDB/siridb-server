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

#include <siri/net/clserver.h>
#include <siri/net/protocol.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>
#include <siri/cfg/cfg.h>
#include <msgpack.h>
#include <stdbool.h>
#include <siri/net/handle.h>
#include <siri/db/auth.h>
#include <siri/db/query.h>
#include <qpack/qpack.h>
#include <siri/db/insert.h>

#define DEFAULT_BACKLOG 128
#define CHECK_SIRIDB                                                        \
if (((sirinet_handle_t *) client->data)->siridb == NULL)                    \
{                                                                           \
    sirinet_pkg_t * package;                                                \
    package = sirinet_new_pkg(pkg->pid, 0, SN_MSG_NOT_AUTHENTICATED, NULL); \
    sirinet_send_pkg(client, package, NULL);                                \
    free(package);                                                          \
    return;                                                                 \
}

static uv_loop_t * loop = NULL;
static struct sockaddr_in client_addr;
static uv_tcp_t client_server;

static void on_data(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg);

static void on_new_connection(
        uv_stream_t * server,
        int status);

static void on_auth_request(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg);

static void on_query(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg);

static void on_insert(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg);

static void on_ping(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg);

int sirinet_clserver_init(uv_loop_t * uv_loop)
{
    int rc;

    if (loop != NULL)
    {
        log_critical("The client server is already initialized!");
        return 1;
    }

    /* bind loop to the given loop */
    loop = uv_loop;

    uv_tcp_init(loop, &client_server);

    uv_ip4_addr(
            siri_cfg.listen_client_address,
            siri_cfg.listen_client_port,
            &client_addr);

    /* make sure data is set to NULL so we later on can check this value. */
    client_server.data = NULL;

    uv_tcp_bind(&client_server, (const struct sockaddr *) &client_addr, 0);

    rc = uv_listen(
            (uv_stream_t*) &client_server,
            DEFAULT_BACKLOG,
            on_new_connection);

    if (rc) {
        log_error("error listening client server: %s", uv_strerror(rc));
        return 1;
    }
    return 0;
}

static void on_new_connection(
        uv_stream_t * server,
        int status)
{
    log_debug("Got a new connection request.");
    if (status < 0)
    {
        log_error("New connection error: %s", uv_strerror(status));
        return;
    }
    uv_tcp_t * client = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));
    client->data =
            (sirinet_handle_t *) malloc(sizeof(sirinet_handle_t));
    ((sirinet_handle_t *) client->data)->on_data = &on_data;
    ((sirinet_handle_t *) client->data)->siridb = NULL;
    uv_tcp_init(loop, client);

    if (uv_accept(server, (uv_stream_t*) client) == 0)
    {
        uv_read_start(
                (uv_stream_t *) client,
                sirinet_handle_alloc_buffer,
                sirinet_handle_on_data);
    }
    else
    {
        uv_close((uv_handle_t *) client, sirinet_free_client);
    }
}

static void on_data(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg)
{
    log_debug("on_data, handle the following: pid: %d, len: %d, tp: %d",
            pkg->pid, pkg->len, pkg->tp);
    switch (pkg->tp)
    {
    case SN_MSG_AUTH_REQUEST:
        on_auth_request(client, pkg);
        break;
    case SN_MSG_QUERY:
        on_query(client, pkg);
        break;
    case SN_MSG_PING:
        on_ping(client, pkg);
        break;
    case SN_MSG_INSERT:
        on_insert(client, pkg);
        break;
    }
}

static void on_auth_request(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg)
{
    sirinet_msg_t rc;
    sirinet_pkg_t * package;
    qp_unpacker_t * unpacker = qp_new_unpacker(pkg->data, pkg->len);
    qp_obj_t * qp_username = qp_new_object();
    qp_obj_t * qp_password = qp_new_object();
    qp_obj_t * qp_dbname = qp_new_object();

    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_username) == QP_RAW &&
            qp_next(unpacker, qp_password) == QP_RAW &&
            qp_next(unpacker, qp_dbname) == QP_RAW)
    {
        rc = siridb_auth_request(
                client,
                qp_username,
                qp_password,
                qp_dbname);
        if (rc == SN_MSG_UNKNOWN_DATABASE)
            package = sirinet_new_pkg(
                    pkg->pid,
                    qp_dbname->len,
                    rc,
                    qp_dbname->via->raw);
        else
            package = sirinet_new_pkg(pkg->pid, 0, rc, NULL);

        sirinet_send_pkg(client, package, NULL);
        free(package);
    }
    else
        log_error("Invalid authentication request received.");
    qp_free_object(qp_username);
    qp_free_object(qp_password);
    qp_free_object(qp_dbname);
    qp_free_unpacker(unpacker);
}

static void on_query(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB
    qp_unpacker_t * unpacker = qp_new_unpacker(pkg->data, pkg->len);
    qp_obj_t * qp_query = qp_new_object();
    qp_obj_t * qp_time_precision = qp_new_object();
    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_query) == QP_RAW &&
            qp_next(unpacker, qp_time_precision))
    {
        siridb_async_query(
                pkg->pid,
                client,
                qp_query->via->raw,
                qp_query->len,
                (qp_time_precision->tp == QP_INT64) ?
                        (siridb_time_t) qp_time_precision->via->int64 :
                        SIRIDB_TIME_DEFAULT,
                SIRIDB_QUERY_FLAG_MASTER);
    }
    qp_free_object(qp_query);
    qp_free_object(qp_time_precision);
    qp_free_unpacker(unpacker);
}

static void on_insert(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg)
{
    CHECK_SIRIDB
    qp_unpacker_t * unpacker = qp_new_unpacker(pkg->data, pkg->len);
    siridb_t * siridb = ((sirinet_handle_t *) client->data)->siridb;
    qp_packer_t * packer[siridb->pools->size];
    int32_t rc;
    const char * err_msg;

    rc = siridb_insert_assign_pools(siridb, unpacker, packer);

    if (rc < 0)
    {
        /* something went wrong, get correct err message */
        err_msg = siridb_insert_err_msg(rc);

        /* create and send package */
        sirinet_pkg_t * package = sirinet_new_pkg(
                pkg->pid, strlen(err_msg), SN_MSG_INSERT_ERROR, err_msg);
        sirinet_send_pkg(client, package, NULL);

        /* free package*/
        free(package);

        /* free packer */
        for (size_t n = 0; n < siridb->pools->size; n++)
            qp_free_packer(packer[n]);

    }
    else
    {
        siridb_insert_points(
                pkg->pid,
                client,
                (size_t) rc,
                siridb->pools->size,
                packer);
    }

    /* free unpacker */
    qp_free_unpacker(unpacker);
}

static void on_ping(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg)
{
    sirinet_pkg_t * package;
    package = sirinet_new_pkg(pkg->pid, 0, SN_MSG_ACK, NULL);
    sirinet_send_pkg(client, package, NULL);
    free(package);
}
