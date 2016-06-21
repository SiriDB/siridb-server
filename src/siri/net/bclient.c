/*
 * bserver.c - Back-end Client SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 20-06-2016
 *
 */

#include <siri/net/bclient.h>
#include <stddef.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <siri/db/server.h>
#include <siri/net/handle.h>
#include <siri/siri.h>

static void on_connect(uv_connect_t * req, int status);
static void on_data(uv_handle_t * client, const sirinet_pkg_t * pkg);

sirinet_bclient_t * sirinet_bclient_new(void)
{
    sirinet_bclient_t * bclient =
            (sirinet_bclient_t *) malloc(sizeof(sirinet_bclient_t));
    bclient->socket = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));
    bclient->socket->data = NULL;
    bclient->connect = (uv_connect_t *) malloc(sizeof(uv_connect_t));
    log_debug("Connect pointer: %p", bclient->connect);
    bclient->promises = imap64_new();
    bclient->pid = 0;
    return bclient;
}

void sirinet_bclient_init(sirinet_bclient_t * bclient)
{
    log_debug("Init.....");
    uv_tcp_init(siri.loop, bclient->socket);

    log_debug("Oke.......");

    struct sockaddr_in dest;
    uv_ip4_addr("127.0.0.1", 80, &dest);

    log_debug("Here...");

    uv_tcp_connect(
            bclient->connect,
            bclient->socket,
            (const struct sockaddr *) &dest,
            on_connect);
}

void sirinet_bclient_free(sirinet_bclient_t * bclient)
{
//    imap32_walk();
    log_debug("Free BACKEND Client");
    imap64_free(bclient->promises);
    free(bclient->connect);
    free(bclient->socket);
    free(bclient);
}

static void on_connect(uv_connect_t * req, int status)
{
    log_debug("ON CONNECT: %d", status);
//    req->handle->data

    /* req = bclient->connect pointer
     * TODO: we might wan to free the connect pointer and do not store this
     * to the bclient object.
     */
    log_debug("Request pointer: %p", req);
//    sirinet_pkg_t * package;
//    package = sirinet_pkg_new(1, 1, 1, NULL);
//    sirinet_pkg_send(req->handle, package, NULL);
//    free(package);
//    free(req);
}

static void on_data(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
    siridb_server_t * server = ((sirinet_handle_t *) client->data)->origin;
    sirinet_promise_t * promise =
            imap64_pop(server->bclient->promises, pkg->pid);

    if (promise == NULL)
    {
        log_warning("Cannot find promise for package id %lu", pkg->pid);
    }
    else
    {
        uv_timer_stop(&promise->timer);
        promise->cb(client, pkg, promise->data);

    }
}
