/*
 * buffer.h - Buffer for integer and double series.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 01-04-2016
 *
 */
#ifndef SIRIDB_BUFFER_H_
#define SIRIDB_BUFFER_H_

#include <siri/db/db.h>
#include <siri/db/series.h>
#include <siri/db/points.h>
#include <unistd.h>

#define MAX_BUFFER_SZ 10485760

int siridb_buffer_new_series(
        siridb_t * siridb,
        siridb_series_t * series);

int siridb_buffer_open(siridb_t * siridb);

int siridb_buffer_load(siridb_t * siridb);

int siridb_buffer_write_len(
        siridb_t * siridb,
        siridb_series_t * series);


int siridb_buffer_write_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val);

int siridb_buffer_fsync(siridb_t * siridb);


#endif  /* SIRIDB_BUFFER_H_ */
