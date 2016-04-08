/*
 * shards.h - SiriDB shards.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#pragma once

#include <siri/db/db.h>

#define SIRIDB_SHARDS_PATH "shards/"

struct siridb_s;




int siridb_load_shards(struct siridb_s * siridb);


// FILE * siridb_open_shard(siridb_shard_t * shard);
