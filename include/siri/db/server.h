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


typedef struct siridb_s siridb_t;

typedef struct siridb_server_s
{
    uuid_t uuid;
    char * name; /* this is a format for address:port but we use it a lot */
    char * address;
    uint16_t port;
    uint16_t pool;
    uint8_t flags;
    uint8_t ref;
    imap64_t * promises;
    uv_tcp_t * socket;
} siridb_server_t;

siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool);

void siridb_server_free(siridb_server_t * server);

/*
 * returns < 0 if the uuid from server A is less than uuid from server B.
 * returns > 0 if the uuid from server A is greater than uuid from server B.
 * returns 0 when uuid server A and B are equal.
 */
int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb);

void siridb_server_incref(siridb_server_t * server);
void siridb_server_decref(siridb_server_t * server);
void siridb_server_connect(siridb_t * siridb, siridb_server_t * server);
