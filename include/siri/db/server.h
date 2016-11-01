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
#include <imap/imap.h>
#include <cexpr/cexpr.h>
#include <uv.h>
#include <siri/net/promise.h>
#include <siri/net/pkg.h>

#define FLAG_KEEP_PKG 1
#define FLAG_ONLY_CHECK_ONLINE 2

#define SERVER_FLAG_RUNNING 1
#define SERVER_FLAG_SYNCHRONIZING 2
#define SERVER_FLAG_REINDEXING 4
#define SERVER_FLAG_BACKUP_MODE 8
//#define SERVER_FLAG_APPLYING_MODE 16
#define SERVER_FLAG_AUTHENTICATED 32  /* must be the last (we depend on this)
                                         and will NEVER be set on 'this'
                                         server */

#define SERVER__IS_ONLINE 33  // RUNNING + AUTHENTICATED
#define SERVER__IS_SYNCHRONIZING 35  // RUNNING + SYNCHRONIZING + AUTHENTICATED
#define SERVER__IS_REINDEXING 37  // RUNNING + REINDEXING + AUTHENTICATED

#define SERVER__SELF_ONLINE 1  // RUNNING
#define SERVER__SELF_SYNCHRONIZING 3  // RUNNING + SYNCHRONIZING
#define SERVER__SELF_REINDEXING 5  // RUNNING + REINDEXING


/*
 * A server is  'connected' when at least connected.
 */
#define siridb_server_is_connected(server) \
    (server->socket != NULL)

/*
 * A server is  'online' when at least running and authenticated.
 */
#define siridb_server_is_online(server) \
((server->flags & SERVER__IS_ONLINE) == SERVER__IS_ONLINE)
#define siridb_server_self_online(server) \
((server->flags & SERVER__SELF_ONLINE) == SERVER__SELF_ONLINE)

/*
 * A server is  'available' when exactly running and authenticated.
 */
#define siridb_server_is_available(server) \
(server->flags == SERVER__IS_ONLINE)
#define siridb_server_self_available(server) \
(server->flags == SERVER__SELF_ONLINE)

/*
 * A server is  'synchronizing' when exactly running, authenticated and
 * synchronizing.
 */
#define siridb_server_is_synchronizing(server) \
(server->flags == SERVER__IS_SYNCHRONIZING)
#define siridb_server_self_synchronizing(server) \
(server->flags == SERVER__SELF_SYNCHRONIZING)

/*
 * A server is  'accessible' when exactly running, authenticated and
 * optionally re-indexing.
 */
#define siridb_server_is_accessible(server) \
(server->flags == SERVER__IS_ONLINE || server->flags == SERVER__IS_REINDEXING)
#define siridb_server_self_accessible(server) \
(server->flags == SERVER__SELF_ONLINE || server->flags == SERVER__SELF_REINDEXING)



typedef struct siridb_s siridb_t;
typedef struct sirinet_promise_s sirinet_promise_t;
typedef void (* sirinet_promise_cb)(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

typedef struct siridb_server_s
{
    uint16_t ref;  /* keep ref on top */
    uint16_t port;
    uint16_t pool;
    uint8_t flags; /* do not use flags above 16384 */
    uint8_t id; /* set when added to a pool to either 0 or 1 */
    uuid_t uuid;
    char * name; /* this is a format for address:port but we use it a lot */
    char * address;
    imap_t * promises;
    uv_tcp_t * socket;
    uint16_t pid;
    /* fixed server properties */
    uint8_t ip_support;
    uint8_t pad0;
    uint32_t startup_time;
    char * libuv;
    char * version;
    char * dbpath;
    char * buffer_path;
    size_t buffer_size;
} siridb_server_t;

typedef struct siridb_server_walker_s
{
    siridb_server_t * server;
    siridb_t * siridb;
} siridb_server_walker_t;

typedef struct siridb_server_async_s
{
    uint16_t pid;
    uv_stream_t * client;
} siridb_server_async_t;


siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool);

int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb);
void siridb_server_connect(siridb_t * siridb, siridb_server_t * server);
int siridb_server_send_pkg(
        siridb_server_t * server,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data,
        int flags);
void siridb_server_send_flags(siridb_server_t * server);
int siridb_server_update_address(
        siridb_t * siridb,
        siridb_server_t * server,
        const char * address,
        uint16_t port);
char * siridb_server_str_status(siridb_server_t * server);
siridb_server_t * siridb_server_from_node(
        siridb_t * siridb,
        cleri_node_t * server_node,
        char * err_msg);
int siridb_server_drop(siridb_t * siridb, siridb_server_t * server);
int siridb_server_is_remote_prop(uint32_t prop);
int siridb_server_cexpr_cb(
        siridb_server_walker_t * wserver,
        cexpr_condition_t * cond);
siridb_server_t * siridb_server_register(
        siridb_t * siridb,
        char * data,
        size_t len);
void siridb__server_free(siridb_server_t * server);

#define siridb_server_update_flags(org, new) \
    org = new | (org & SERVER_FLAG_AUTHENTICATED)

#define siridb_server_incref(server) server->ref++

/*
 * Decrement server reference counter and free the server when zero is reached.
 * When the server is destroyed, all remaining server->promises are cancelled
 * and each promise->cb() will be called.
 */
#define siridb_server_decref(server__) \
	if (!--server__->ref) siridb__server_free(server__)
