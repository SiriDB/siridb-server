/*
 * servers.h - SiriDB Servers.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-06-2016
 *
 */
#pragma once

#include <siri/db/db.h>
#include <uuid/uuid.h> /* install: apt-get install uuid-dev */
#include <siri/net/promise.h>

typedef struct siridb_s siridb_t;

int siridb_servers_load(siridb_t * siridb);
void siridb_servers_free(llist_t * servers);
siridb_server_t * siridb_servers_by_uuid(llist_t * servers, uuid_t uuid);
siridb_server_t * siridb_servers_by_name(llist_t * servers, const char * name);
void siridb_servers_send_pkg(
        slist_t * servers,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data);
void siridb_servers_send_flags(llist_t * servers);
ssize_t siridb_servers_get_file(char ** buffer, siridb_t * siridb);
int siridb_servers_online(siridb_t * siridb);
int siridb_servers_available(siridb_t * siridb);
int siridb_servers_list(siridb_server_t * server, uv_async_t * handle);
int siridb_servers_save(siridb_t * siridb);
int siridb_servers_register(siridb_t * siridb, siridb_server_t * server);
slist_t * siridb_servers_other2slist(siridb_t * siridb);
