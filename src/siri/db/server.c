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
#include <siri/net/handle.h>
#include <siri/net/promise.h>
#include <siri/siri.h>
#include <assert.h>
#include <string.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static void SERVER_update_name(siridb_server_t * server);
static void SERVER_free(siridb_server_t * server);

siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool)
{
    siridb_server_t * server =
            (siridb_server_t *) malloc(sizeof(siridb_server_t));

    /* copy uuid */
    memcpy(server->uuid, uuid, 16);

    /* initialize with NULL, SERVER_update_name() sets the correct name */
    server->name = NULL;

    /* copy address */
    server->address = (char *) malloc(address_len + 1);
    memcpy(server->address, address, address_len);
    server->address[address_len] = 0;

    server->port = port;
    server->pool = pool;
    server->flags = 0;
    server->ref = 0;

    /* we set the promises later because we don't need one for self */
    server->promises = NULL;
    server->socket = NULL;

    /* sets address:port to name property */
    SERVER_update_name(server);

    return server;
}

inline int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb)
{
    return uuid_compare(sa->uuid, sa->uuid);
}

inline void siridb_server_incref(siridb_server_t * server)
{
    server->ref++;
}

void siridb_server_decref(siridb_server_t * server)
{
    if (!--server->ref)
    {
        SERVER_free(server);
    }
}

static void SERVER_on_connect(uv_connect_t * req, int status)
{
    siridb_server_t * server = ((sirinet_socket_t *)req->handle->data)->origin;
    if (status == 0)
    {
        log_debug("Connection made to back-end server: '%s'", server->name);

        uv_read_start(
                req->handle,
                sirinet_socket_alloc_buffer,
                sirinet_socket_on_data);

        sirinet_pkg_t * package;
        package = sirinet_pkg_new(1, 0, 1, NULL);
        sirinet_pkg_send(req->handle, package, NULL);
        free(package);
    }
    else
    {
        log_error("Connecting to back-end server '%s' failed. (error: %s)",
                server->name,
                uv_strerror(status));

        uv_close((uv_handle_t *) req->handle, (uv_close_cb) sirinet_socket_free);
    }
    free(req);
}

static void SERVER_on_data(uv_handle_t * client, const sirinet_pkg_t * pkg)
{
    log_warning("!!!!! HERE !!!!");
    siridb_server_t * server = ((sirinet_socket_t *) client->data)->origin;
    sirinet_promise_t * promise =
            imap64_pop(server->promises, pkg->pid);

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

void siridb_server_connect(siridb_t * siridb, siridb_server_t * server)
{
    if (server->socket != NULL)
        return;

    struct sockaddr_in dest;
    server->socket = sirinet_socket_new(SOCKET_SERVER, &SERVER_on_data);

    sirinet_socket_t * ssocket = server->socket->data;
    ssocket->origin = server;
    ssocket->siridb = siridb;

    uv_tcp_init(siri.loop, server->socket);

    uv_ip4_addr(server->address, server->port, &dest);

    uv_connect_t * req = (uv_connect_t *) malloc(sizeof(uv_connect_t));

    uv_tcp_connect(
            req,
            server->socket,
            (const struct sockaddr *) &dest,
            SERVER_on_connect);
}

static void SERVER_free(siridb_server_t * server)
{
    log_debug("FREE Server");
    if (server->promises != NULL)
    {
        imap64_free(server->promises);
    }
    free(server->name);
    free(server->address);
    free(server);
}

static void SERVER_update_name(siridb_server_t * server)
{
    /* start len with 2, on for : and one for 0 terminator */
    size_t len = 2;
    uint16_t i = server->port;

#ifdef DEBUG
    assert(server->port > 0);
#endif

    /* append 'string' length for server->port */
    for (; i; i /= 10, len++);

    /* append 'address' length */
    len += strlen(server->address);

    /* allocate enough space */
    server->name = (server->name == NULL) ?
            (char *) malloc(len) : (char *) realloc(server->name, len);

    /* set the name */
    sprintf(server->name, "%s:%d", server->address, server->port);
}

