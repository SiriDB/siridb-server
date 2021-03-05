/*
 * time.c - Time- and time precision functions and constants.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/time.h>
#include <siri/err.h>
#include <stddef.h>
#include <xmath/xmath.h>


/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Node: can be destroyed be using free().
 */
siridb_time_t * siridb_time_new(siridb_timep_t precision)
{
    siridb_time_t * time = malloc(sizeof(siridb_time_t));
    if (time == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        time->precision = precision;
        time->factor = (uint64_t) xmath_ipow(1000, precision);
        time->ts_sz = (precision == SIRIDB_TIME_SECONDS) ?
                sizeof(uint32_t) : sizeof(uint64_t);
    }
    return time;
}

uint64_t siridb_time_parse(const char * str, size_t len)
{
    uint64_t ts = atoll(str);
    switch (str[len - 1])
    {
    case 's':
        return ts;
    case 'm':
        return ts * 60;
    case 'h':
        return ts * 3600;
    case 'd':
        return ts * 86400;
    case 'w':
        return ts * 604800;
    }
    /* we should NEVER get here */
    log_critical("Unexpected time char received: '%c'", str[len - 1]);
    assert (0);
    return 0;
}

uint64_t siridb_time_in_seconds(siridb_t * siridb, int64_t ts)
{
    return ts / siridb->time->factor;
}

uint64_t siridb_time_now(siridb_t * siridb, struct timespec now)
{
    return now.tv_sec * siridb->time->factor +
        now.tv_nsec * (siridb->time->factor / 1000000000.0);
}
