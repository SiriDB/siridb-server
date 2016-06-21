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
#include <siri/siri.h>
#include <logger/logger.h>
#include <siri/net/handle.h>

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
        log_critical("Client server is already initialized!");
        return 1;
    }

    /* bind loop to the given loop */
    loop = siri->loop;

    uv_tcp_init(loop, &backend_server);

    uv_ip4_addr(
            siri->cfg->listen_backend_address,
            siri->cfg->listen_client_port,
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
        log_error("Error listening client server: %s", uv_strerror(rc));
        return 1;
    }
    return 0;
}

static void on_new_connection(uv_stream_t * server, int status)
{
    log_debug("Got a server connection request.");
    if (status < 0)
    {
        log_error("Server connection error: %s", uv_strerror(status));
        return;
    }
    uv_tcp_t * client = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));
    client->data = sirinet_handle_new(&on_data);

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

static void on_data(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
    log_debug("[Back-end server] Got data (pid: %d, len: %d, tp: %d)",
            pkg->pid, pkg->len, pkg->tp);
    switch (pkg->tp)
    {

    }
}
