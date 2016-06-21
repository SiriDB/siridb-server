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
#include <siri/net/handle.h>
#include <siri/net/pkg.h>

#define DEFAULT_BACKLOG 128

static void on_new_connection(uv_stream_t * server, int status);
static void on_data(uv_handle_t * client, const sirinet_pkg_t * pkg);

static uv_loop_t * loop = NULL;
static struct sockaddr_in server_addr;
static uv_tcp_t backend_server;

int sirinet_bserver_init(siri_t * siri)
{
    int rc;

    if (loop != NULL)
    {
        log_critical("Back-end server is already initialized!");
        return 1;
    }

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
    return 0;
}

static void on_new_connection(uv_stream_t * server, int status)
{
    log_debug("Received a back-end server connection request.");

    if (status < 0)
    {
        log_error("Back-end server connection error: %s", uv_strerror(status));
        return;
    }

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
    log_debug("[Back-end server] Got data (pid: %d, len: %d, tp: %d)",
            pkg->pid, pkg->len, pkg->tp);
    sirinet_pkg_t * package;
    package = sirinet_pkg_new(pkg->pid, 0, 25, NULL);
    sirinet_pkg_send((uv_stream_t *) client, package, NULL);
    free(package);
}
