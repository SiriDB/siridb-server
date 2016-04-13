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
#include <siri/db/series.h>
#include <siri/filehandler.h>

#define SIRIDB_SHARD_HAS_OVERLAP 1
#define SIRIDB_SHARD_MANUAL_OPTIMIZE 2
#define SIRIDB_SHARD_HAS_NEW_VALUES 4
#define SIRIDB_SHARD_HAS_REMOVED_SERIES 8

struct siridb_s;
struct siridb_points_s;
struct siridb_series_s;
struct idx_num32_s;
struct idx_num64_s;

typedef struct siridb_shard_s
{
    uint64_t id;
    uint8_t tp; /* SIRIDB_SERIES_TP_INT, ...DOUBLE or ...STRING */
    uint8_t status;
    siri_fp_t * fp;
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
        struct siridb_series_s * series,
        siridb_shard_t * shard,
        struct siridb_points_s * points,
        uint32_t start,
        uint32_t end);

struct siridb_points_s * siridb_shard_get_points_num32(
        struct siridb_s * siridb,
        struct siridb_points_s * points,
        struct idx_num32_s * idx,
        uint8_t has_overlap);
struct siridb_points_s * siridb_shard_get_points_num64(
        struct siridb_s * siridb,
        struct idx_num64_s * idx);
