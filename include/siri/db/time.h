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

struct siridb_s;

typedef enum siridb_time_tp
{
    SIRIDB_TIME_DEFAULT=-1, /* use this when asking for the db default */
    SIRIDB_TIME_SECONDS,
    SIRIDB_TIME_MILLISECONDS,
    SIRIDB_TIME_MICROSECONDS,
    SIRIDB_TIME_NANOSECONDS,
    SIRIDB_TIME_END
} siridb_time_t;

const char * siridb_time_short_map[SIRIDB_TIME_END];

int siridb_int64_valid_ts(struct siridb_s * siridb, int64_t ts);
