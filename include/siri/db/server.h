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
#include <stdint.h>
#include <siri/db/db.h>
#include <imap64/imap64.h>
#include <uv.h>

#define SERVER_FLAG_ONLINE 1
#define SERVER_FLAG_PAUSED 2
#define SERVER_FLAG_SYNCHRONIZING 4
#define SERVER_FLAG_EXPANDING 8
#define SERVER_FLAG_AUTHENTICATED 64

#define SERVER_IS_AVAILABLE 65  // SERVER_FLAG_ONLINE + SERVER_FLAG_AUTHENTICATED

#define siridb_server_is_available(server) \
    (server->flags & SERVER_IS_AVAILABLE) == SERVER_IS_AVAILABLE

typedef struct siridb_s siridb_t;

typedef struct siridb_server_s
{
    uuid_t uuid;
    char * name; /* this is a format for address:port but we use it a lot */
    char * address;
    char * version;
    uint16_t port;
    uint16_t pool;
    uint16_t ref;
    uint16_t flags; /* do not use flags above 16384 */
    imap64_t * promises;
    uv_tcp_t * socket;
    uint64_t pid;
} siridb_server_t;

siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool);

/*
 * returns < 0 if the uuid from server A is less than uuid from server B.
 * returns > 0 if the uuid from server A is greater than uuid from server B.
 * returns 0 when uuid server A and B are equal.
 */
int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb);

void siridb_server_incref(siridb_server_t * server);
void siridb_server_decref(siridb_server_t * server);
void siridb_server_connect(siridb_t * siridb, siridb_server_t * server);
void siridb_server_send_flags(siridb_server_t * server);

#define siridb_server_update_flags(org, new) \
    org = new | (org & SERVER_FLAG_AUTHENTICATED)
