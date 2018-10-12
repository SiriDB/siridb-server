/*
 * servers.h - Collection of SiriDB servers.
 */
#ifndef SIRIDB_SERVERS_H_
#define SIRIDB_SERVERS_H_

#include <siri/db/db.h>
#include <uuid/uuid.h> /* install: apt-get install uuid-dev */
#include <siri/net/promise.h>

int siridb_servers_load(siridb_t * siridb);
void siridb_servers_free(llist_t * servers);
siridb_server_t * siridb_servers_by_uuid(llist_t * servers, uuid_t uuid);
siridb_server_t * siridb_servers_by_name(llist_t * servers, const char * name);
siridb_server_t * siridb_servers_by_replica(
        llist_t * servers,
        siridb_server_t * replica);

void siridb_servers_send_pkg(
        vec_t * servers,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data);
void siridb_servers_send_flags(llist_t * servers);
ssize_t siridb_servers_get_file(char ** buffer, siridb_t * siridb);
int siridb_servers_online(siridb_t * siridb);
int siridb_servers_available(siridb_t * siridb);
int siridb_servers_list(siridb_server_t * server, uv_async_t * handle);
int siridb_servers_check_version(siridb_t * siridb, char * version);
int siridb_servers_save(siridb_t * siridb);
int siridb_servers_register(siridb_t * siridb, siridb_server_t * server);
vec_t * siridb_servers_other2vec(siridb_t * siridb);

#endif  /* SIRIDB_SERVERS_H_ */
