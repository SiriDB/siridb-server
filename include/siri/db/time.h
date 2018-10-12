/*
 * time.h - Time- and time precision functions and constants.
 */
#ifndef SIRIDB_TIME_H_
#define SIRIDB_TIME_H_

typedef enum
{
    SIRIDB_TIME_DEFAULT=-1, /* use this when asking for the db default */
    SIRIDB_TIME_SECONDS,
    SIRIDB_TIME_MILLISECONDS,
    SIRIDB_TIME_MICROSECONDS,
    SIRIDB_TIME_NANOSECONDS,
    SIRIDB_TIME_END
} siridb_timep_t;

typedef struct siridb_time_s siridb_time_t;

#include <inttypes.h>
#include <siri/db/db.h>
#include <stddef.h>
#include <time.h>

const char * siridb_time_short_map[SIRIDB_TIME_END];
siridb_time_t * siridb_time_new(siridb_timep_t precision);
uint32_t siridb_time_in_seconds(siridb_t * siridb, int64_t ts);
uint64_t siridb_time_now(siridb_t * siridb, struct timespec now);
uint64_t siridb_time_parse(const char * str, size_t len);

struct siridb_time_s
{
    siridb_timep_t precision;
    uint32_t factor;
    size_t ts_sz;
};

static inline int siridb_int64_valid_ts(siridb_time_t * time, int64_t ts)
{
    return (time->precision == SIRIDB_TIME_SECONDS) ?
         ts >= 0 && ts < 4294967296 : ts >= 0;
}

#endif  /* SIRIDB_TIME_H_ */
