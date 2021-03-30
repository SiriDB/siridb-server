/*
 * buffer.h - Buffer for integer and double values.
 */
#ifndef SIRIDB_BUFFER_H_
#define SIRIDB_BUFFER_H_

typedef struct siridb_buffer_s siridb_buffer_t;

#include <siri/db/db.h>
#include <siri/db/series.h>
#include <siri/db/points.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_BUFFER_SZ 1048576

siridb_buffer_t * siridb_buffer_new(void);
void siridb_buffer_free(siridb_buffer_t * buffer);
void siridb_buffer_close(siridb_buffer_t * buffer);
bool siridb_buffer_is_valid_size(ssize_t ssize);
void siridb_buffer_set_path(siridb_buffer_t * buffer, const char * str);
int siridb_buffer_new_series(
        siridb_buffer_t * buffer,
        siridb_series_t * series);
int siridb_buffer_open(siridb_buffer_t * buffer);
int siridb_buffer_load(siridb_t * siridb);
int siridb_buffer_test_path(siridb_t * siridb);
int siridb_buffer_write_empty(
        siridb_buffer_t * buffer,
        siridb_series_t * series);
int siridb_buffer_write_point(
        siridb_buffer_t * buffer,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val);

struct siridb_buffer_s
{
    size_t size;            /* size for one series inside the buffer */
    size_t _to_size;        /* optional new size from database.conf */
    size_t len;             /* number of points allocated per series */
    char * template;        /* template for writing an empty buffer */
    char * path;            /* path where the buffer file is stored */
    vec_t * empty;        /* list with empty buffer spaces */
    FILE * fp;              /* buffer file pointer */
    int fd;                 /* buffer file descriptor */
};

static inline int siridb_buffer_fsync(siridb_buffer_t * buffer)
{
    return (buffer->fp == NULL) ? 0 : fsync(buffer->fd);
}

#endif  /* SIRIDB_BUFFER_H_ */
