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
#include <assert.h>
#include <string.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static void SERVER_update_name(siridb_server_t * server);

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

    /* sets address:port to name property */
    SERVER_update_name(server);

    return server;
}

void siridb_server_free(siridb_server_t * server)
{
    free(server->name);
    free(server->address);
    free(server);
}

int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb)
{
    int i = 0;
    for (i = 0; i < 16; i++)
        if (sa->uuid[i] < sb->uuid[i])
            return -(i + 1);
        else if (sa->uuid[i] > sb->uuid[i])
            return i + 1;
    return 0;
}

static void SERVER_update_name(siridb_server_t * server)
{
    /* start len with 2, on for : and one for 0 terminator */
    size_t len = 2;
    uint16_t i = server->port;

    assert(server->port > 0);

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

