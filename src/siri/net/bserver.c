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

#define DEFAULT_BACKLOG 128

#define SERVER_CHECK_AUTHENTICATED(server)                                    \
siridb_server_t * server = ((sirinet_socket_t * ) client->data)->origin;      \
if (!(server->flags & SERVER_FLAG_AUTHENTICATED))                             \
{                                                                             \
    sirinet_pkg_t * package;                                                  \
    package = sirinet_pkg_new(pkg->pid, 0, BP_ERROR_NOT_AUTHENTICATED, NULL); \
    sirinet_pkg_send((uv_stream_t *) client, package, NULL, NULL);            \
    free(package);                                                            \
    return;                                                                   \
}

static void on_new_connection(uv_stream_t * server, int status);
static void on_data(uv_handle_t * client, const sirinet_pkg_t * pkg);
static void on_auth_request(uv_handle_t * client, const sirinet_pkg_t * pkg);
static void on_flags_update(uv_handle_t * client, const sirinet_pkg_t * pkg);

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

    uv_tcp_t * client = sirinet_socket_new(SOCKET_BACKEND, &on_data);

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

static void on_data(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
#ifdef DEBUG
    log_debug("[Back-end server] Got data (pid: %d, len: %d, tp: %d)",
            pkg->pid, pkg->len, pkg->tp);
#endif

    switch ((bp_client_t) pkg->tp)
    {
    case BP_AUTH_REQUEST:
        on_auth_request(client, pkg);
        break;
    case BP_FLAGS_UPDATE:
        on_flags_update(client, pkg);
        break;
    }

}

static void on_auth_request(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
    bp_server_t rc;
    sirinet_pkg_t * package;
    qp_unpacker_t * unpacker = qp_new_unpacker(pkg->data, pkg->len);
    qp_obj_t * qp_uuid = qp_new_object();
    qp_obj_t * qp_dbname = qp_new_object();
    qp_obj_t * qp_flags = qp_new_object();
    qp_obj_t * qp_version = qp_new_object();
    qp_obj_t * qp_min_version = qp_new_object();

    if (    qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_uuid) == QP_RAW &&
            qp_next(unpacker, qp_dbname) == QP_RAW &&
            qp_next(unpacker, qp_flags) == QP_INT64 &&
            qp_next(unpacker, qp_version) == QP_RAW &&
            qp_next(unpacker, qp_min_version) == QP_RAW)
    {
        rc = siridb_auth_server_request(
                client,
                qp_uuid,
                qp_dbname,
                qp_version,
                qp_min_version);
        if (rc == BP_AUTH_SUCCESS)
        {
            siridb_server_t * server =
                    ((sirinet_socket_t * ) client->data)->origin;

            /* update server flags */
            siridb_server_update_flags(server->flags, qp_flags->via->int64);

            log_info("Accepting back-end server connection: '%s'",
                    server->name);
        }
        else
        {
            log_warning("Refusing back-end connection (error code: %d)", rc);
        }

        package = sirinet_pkg_new(pkg->pid, 0, rc, NULL);
        sirinet_pkg_send((uv_stream_t *) client, package, NULL, NULL);
        free(package);
    }
    else
    {
        log_error("Invalid back-end 'on_auth_request' received.");
    }
    qp_free_object(qp_uuid);
    qp_free_object(qp_dbname);
    qp_free_object(qp_flags);
    qp_free_object(qp_version);
    qp_free_object(qp_min_version);
    qp_free_unpacker(unpacker);
}

static void on_flags_update(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
    SERVER_CHECK_AUTHENTICATED(server)

    sirinet_pkg_t * package;
    qp_unpacker_t * unpacker = qp_new_unpacker(pkg->data, pkg->len);
    qp_obj_t * qp_flags = qp_new_object();

    if (qp_next(unpacker, qp_flags) == QP_INT64)
    {
        /* update server flags */
        siridb_server_update_flags(server->flags, qp_flags->via->int64);

        log_info("Status received from '%s' (status: %d)",
                server->name,
                server->flags);

        package = sirinet_pkg_new(pkg->pid, 0, BP_FLAGS_ACK, NULL);
        sirinet_pkg_send((uv_stream_t *) client, package, NULL, NULL);
        free(package);
    }
    else
    {
        log_error("Invalid back-end 'on_flags_update' received.");
    }
    qp_free_object(qp_flags);
    qp_free_unpacker(unpacker);
}
