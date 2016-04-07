/*
 * time.c - Time- and time precision functions and constants.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#include <siri/db/time.h>
#include <xmath/xmath.h>

const char * siridb_time_short_map[] = {"s", "ms", "us", "ns"};

int siridb_int64_valid_ts(siridb_t * siridb, int64_t ts)
{
    if (siridb->time_precision == SIRIDB_TIME_SECONDS)
        return ts >= 0 && ts < 4294967296;
    return ts >= 0;
}

uint32_t siridb_time_in_seconds(siridb_t * siridb, int64_t ts)
{
    return ts / ipow(1000, siridb->time_precision);
}
