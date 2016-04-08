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
#include <siri/db/points.h>

struct siridb_s;
struct siridb_points_s;

typedef struct siridb_shard_s
{
    uint64_t id;
    uint8_t tp; /* SIRIDB_SERIES_TP_INT, ...DOUBLE or ...STRING */
    uint8_t status;
    FILE * fp;
} siridb_shard_t;

void siridb_free_shard(siridb_shard_t * shard);

int siridb_load_shard(struct siridb_s * siridb, uint64_t id);
siridb_shard_t * siridb_create_shard(
        struct siridb_s * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp);



int siridb_shard_write_points(
        struct siridb_s * siridb,
        siridb_shard_t * shard,
        struct siridb_points_s * points,
        uint32_t start,
        uint32_t end);
