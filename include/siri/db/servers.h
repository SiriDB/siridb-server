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

typedef struct siridb_s siridb_t;

int siridb_servers_load(siridb_t * siridb);
void siridb_servers_free(siridb_t * siridb);
