/*
 * shard.h - SiriDB Shard.
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

struct siridb_s;

typedef struct siridb_shard_s
{
    uint64_t id;
    uint8_t tp; /* SIRIDB_SERIES_TP_INT, ...DOUBLE or ...STRING */
    uint8_t status;
    FILE * fp;
} siridb_shard_t;

void siridb_free_shard(siridb_shard_t * shard);

int siridb_load_shard(struct siridb_s * siridb, uint64_t id);
int siridb_create_shard(struct siridb_s * siridb, uint64_t id, uint8_t tp);
