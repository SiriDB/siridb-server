/*
 * server.h - SiriDB Server.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 17-03-2016
 *
 */
#pragma once

#include <uuid/uuid.h>
#include <siri/db/db.h>

struct siridb_s;

typedef struct siridb_server_s
{
    uuid_t uuid;
    char * name; /* this is a format for address:port but we use it a lot */
    char * address;
    uint16_t port;
    uint16_t pool;
} siridb_server_t;

typedef struct siridb_servers_s
{
    siridb_server_t * server;
    struct siridb_servers_s * next;
} siridb_servers_t;

int siridb_load_servers(struct siridb_s * siridb);
void siridb_free_servers(siridb_servers_t * servers);

/*
 * returns < 0 if the uuid from server A is less than uuid from server B.
 * returns > 0 if the uuid from server A is greater than uuid from server B.
 * returns 0 when uuid server A and B are equal.
 */
int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb);
