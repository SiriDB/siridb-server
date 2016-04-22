/*
 * time.h - Time- and time precision functions and constants.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <siri/db/db.h>
#include <stddef.h>
#include <time.h>

struct siridb_s;
struct timespec;

typedef enum siridb_time_tp
{
    SIRIDB_TIME_DEFAULT=-1, /* use this when asking for the db default */
    SIRIDB_TIME_SECONDS,
    SIRIDB_TIME_MILLISECONDS,
    SIRIDB_TIME_MICROSECONDS,
    SIRIDB_TIME_NANOSECONDS,
    SIRIDB_TIME_END
} siridb_timep_t;

typedef struct siridb_time_s
{
    siridb_timep_t precision;
    uint32_t factor;
    size_t ts_sz;
} siridb_time_t;

siridb_time_t * siridb_new_time(siridb_timep_t precision);

const char * siridb_time_short_map[SIRIDB_TIME_END];

int siridb_int64_valid_ts(struct siridb_s * siridb, int64_t ts);

uint32_t siridb_time_in_seconds(struct siridb_s * siridb, int64_t ts);

uint64_t siridb_time_now(struct siridb_s * siridb, struct timespec now);

uint64_t siridb_time_parse(const char * str, size_t len);
