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

typedef struct siridb_s siridb_t;

int siridb_servers_load(siridb_t * siridb);
void siridb_servers_free(llist_t * servers);
siridb_server_t * siridb_servers_get_server(llist_t * servers, uuid_t uuid);
