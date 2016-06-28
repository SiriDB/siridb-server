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
#include <cexpr/cexpr.h>
#include <uv.h>
#include <siri/net/promise.h>

#define SERVER_FLAG_RUNNING 1
#define SERVER_FLAG_PAUSED 2
#define SERVER_FLAG_SYNCHRONIZING 4
#define SERVER_FLAG_REINDEXING 8
#define SERVER_FLAG_AUTHENTICATED 16  // must be the last (we depend on this)

#define SERVER_IS_AVAILABLE 17  // RUNNING + AUTHENTICATED

#define siridb_server_is_available(server) \
    (server->flags & SERVER_IS_AVAILABLE) == SERVER_IS_AVAILABLE

typedef struct siridb_s siridb_t;
typedef struct sirinet_promise_s sirinet_promise_t;
typedef void (* sirinet_promise_cb_t)(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

typedef struct siridb_server_s
{
    uuid_t uuid;
    char * name; /* this is a format for address:port but we use it a lot */
    char * address;
    uint16_t port;
    uint16_t pool;
    uint16_t ref;
    uint8_t flags; /* do not use flags above 16384 */
    imap64_t * promises;
    uv_tcp_t * socket;
    uint64_t pid;
    /* fixed server properties */
    char * version;
    char * dbpath;
    char * buffer_path;
    size_t buffer_size;
    uint32_t startup_time;

} siridb_server_t;

typedef struct siridb_server_walker_s
{
    siridb_server_t * server;
    siridb_t * siridb;
} siridb_server_walker_t;

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
void siridb_server_send_pkg(
        siridb_server_t * server,
        uint32_t len,
        uint16_t tp,
        const char * content,
        uint64_t timeout,
        sirinet_promise_cb_t cb,
        void * data);
void siridb_server_send_flags(siridb_server_t * server);

/* returns the current server status (flags) as string. the returned value
 * is created with malloc() so do not forget to free the result.
 */
char * siridb_server_str_status(siridb_server_t * server);

/* return true when the given property (CLERI keyword) needs a remote query */
int siridb_server_is_remote_prop(uint32_t prop);

int siridb_server_cexpr_cb(
        siridb_server_walker_t * wserver,
        cexpr_condition_t * cond);

#define siridb_server_update_flags(org, new) \
    org = new | (org & SERVER_FLAG_AUTHENTICATED)
