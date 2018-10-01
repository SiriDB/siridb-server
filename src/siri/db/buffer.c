/*
 * buffer.c - Buffer for integer and double series.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 01-04-2016
 *
 */
#include <logger/logger.h>
#include <siri/db/buffer.h>
#include <siri/db/db.h>
#include <siri/db/misc.h>
#include <siri/db/shard.h>
#include <siri/siri.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <xpath/xpath.h>
#include <assert.h>

#define SIRIDB_BUFFER_FN "buffer.dat"

/* when set to 1, no caching is done. 1 is the minimum value. */
#define SIRIDB_BUFFER_CACHE 64

static int buffer__create_new(siridb_t * siridb, siridb_series_t * series);
static int buffer__use_empty(siridb_t * siridb, siridb_series_t * series);
static void buffer__migrate_to_new(char * pt, size_t sz);

/* buffer__start cannot conflict with a series_id since id 0 is never used */
static const uint32_t buffer__start = 0x00000000;
static const uint64_t buffer__end = 0xffffffffffffffff;


/*
 * Returns 0 if success or EOF in case of an error.
 */
int siridb_buffer_write_empty(
        siridb_t * siridb,
        siridb_series_t * series)
{
    memcpy(siridb->buffer_clear + 4, &series->id, sizeof(uint32_t));
    return (
        /* go to the series position in buffer */
        fseeko( siridb->buffer_fp,
                series->bf_offset,
                SEEK_SET) ||

        /* write end ts */
        fwrite( siridb->buffer_clear,
                siridb->buffer_size,
                1,
                siridb->buffer_fp) != 1) ? EOF : 0;
}

/*
 * Waring: we must check if the new point fits inside the buffer before using
 * the 'siridb_buffer_write_point()' function.
 *
 * Returns 0 if success or EOF in case of an error.
 */
int siridb_buffer_write_last_point(
        siridb_t * siridb,
        siridb_series_t * series)
{
    siridb_point_t * point;
    const size_t sz = sizeof(uint64_t) + sizeof(qp_via_t);
    char buf[sz];
    int last_idx = series->buffer->len - 1;
    assert (last_idx >= 0);

    point = series->buffer->data + last_idx;

    memcpy(buf, &point->ts, sizeof(uint64_t));
    memcpy(buf + sizeof(uint64_t), &point->val, sizeof(qp_via_t));

    return (
        /* jump to position where to write the new point */
        fseeko( siridb->buffer_fp,
                series->bf_offset + 8 + (16 * last_idx),
                SEEK_SET) ||

        /* write time-stamp and value */
        fwrite(buf, sz, 1, siridb->buffer_fp) != 1) ? EOF : 0;
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int siridb_buffer_new_series(siridb_t * siridb, siridb_series_t * series)
{
    /* allocate new buffer */
    series->buffer = siridb_points_new(siridb->buffer_len, series->tp);
    if (series->buffer == NULL)
    {
        return -1;  /* signal is raised */
    }

    return (siridb->empty_buffers->len) ?
            buffer__use_empty(siridb, series) :
            buffer__create_new(siridb, series);
}

int siridb_buffer_fsync(siridb_t * siridb)
{
    if (siridb->buffer_fp == NULL)
    {
        return 0;
    }
    int buffer_fd = fileno(siridb->buffer_fp);
    return (buffer_fd != -1) ? fsync(buffer_fd) : -1;
}

/*
 * Returns 0 if successful or -1 in case of an error.
 */
int siridb_buffer_open(siridb_t * siridb)
{
    int buffer_fd, rc;
    siridb_misc_get_fn(fn, siridb->buffer_path, SIRIDB_BUFFER_FN)

    if ((siridb->buffer_fp = fopen(fn, "r+")) == NULL)
    {
        log_critical("Cannot open '%s' for reading and writing", fn);
        return -1;
    }

    buffer_fd = fileno(siridb->buffer_fp);

    if (buffer_fd == -1)
    {
        log_critical("Cannot get file descriptor: '%s'", fn);
        return -1;
    }

    rc = posix_fadvise(buffer_fd, 0, 0, POSIX_FADV_RANDOM|POSIX_FADV_DONTNEED);
    if (rc)
    {
        log_warning("Cannot set advice for file access: '%s' (%d)", fn, rc);
    }

    return 0;
}

static void buffer__migrate_to_new(char * pt, size_t sz)
{
    char * npt = pt;
    char * end = pt + sz;
    uint32_t series_id = *((uint32_t *) pt);
    pt += sizeof(uint32_t);
    size_t num = *((size_t *) pt);
    pt += sizeof(size_t);

    memcpy(npt, &buffer__start, sizeof(uint32_t));
    npt += sizeof(uint32_t);
    memcpy(npt, &series_id, sizeof(uint32_t));
    npt += sizeof(uint32_t);
    memmove(npt, pt, num * 16);
    npt += num * 16;

    for (; npt < end; npt += sizeof(uint64_t))
    {
        memcpy(npt, &buffer__end, sizeof(uint64_t));
    }
}

/*
 * Returns 0 if successful or -1 in case of an error.
 * (signal might be raised)
 */
int siridb_buffer_load(siridb_t * siridb)
{
    FILE * fp;
    FILE * fp_temp;
    size_t read_at_once = 8;
    size_t num, i;
    char buffer[siridb->buffer_size * read_at_once];
    char * pt, * end;
    long int offset = 0;
    siridb_series_t * series;
    _Bool log_migrate = 1;
    uint32_t buf_start, series_id;
    uint64_t * ts;

    log_info("Loading and cleanup buffer");

    siridb->buffer_clear = malloc(siridb->buffer_size);
    if (siridb->buffer_clear == NULL)
    {
        log_critical("Allocation error while loading buffer");
        return -1;
    }

    for (   pt = siridb->buffer_clear,
            end = siridb->buffer_clear + siridb->buffer_size;
            pt < end;
            pt += sizeof(uint64_t))
    {
        memcpy(pt, &buffer__end, sizeof(uint64_t));
    }

    memcpy(siridb->buffer_clear, &buffer__start, sizeof(uint32_t));

    siridb_misc_get_fn(fn, siridb->buffer_path, SIRIDB_BUFFER_FN)
    siridb_misc_get_fn(fn_temp, siridb->buffer_path, "__" SIRIDB_BUFFER_FN)

    if (xpath_file_exist(fn_temp))
    {
        log_error(
                "Temporary buffer file found: '%s'. "
                "Check if something went wrong or remove this file", fn_temp);
        return -1;
    }

    if ((fp = fopen(fn, "r")) == NULL)
    {
        if (siridb->series_map->len)
        {
            log_critical("Buffer file '%s' not found.", fn);
            return -1;
        }
        log_warning("Buffer file '%s' not found, create a new one.", fn);

        if ((fp = fopen(fn, "w")) == NULL)
        {
            log_critical("Cannot create buffer file '%s'.", fn);
            return -1;
        }
        return fclose(fp);
    }

    if ((fp_temp = fopen(fn_temp, "w")) == NULL)
    {
        log_critical("Cannot open '%s' for writing", fn_temp);
        fclose(fp);
        return -1;
    }

    while ((num = fread(buffer, siridb->buffer_size, read_at_once, fp)))
    {
        for (i = 0; i < num; i++)
        {
            pt = buffer + i * siridb->buffer_size;

            buf_start = *((uint32_t *) pt);
            if (buf_start != buffer__start)
            {
                if (log_migrate)
                {
                    log_warning("Buffer will be migrated");
                    log_migrate = 0;
                }
                buffer__migrate_to_new(pt, siridb->buffer_size);
            }

            pt += sizeof(uint32_t);
            series_id = *((uint32_t *) pt);
            pt += sizeof(uint32_t);

            series = imap_get(siridb->series_map, series_id);

            if (series == NULL)
            {
                continue;
            }

            series->buffer = siridb_points_new(siridb->buffer_len, series->tp);
            if (series->buffer == NULL)
            {
                log_critical("Cannot allocate a buffer for series id %u",
                        series->id);
                fclose(fp);
                fclose(fp_temp);

                return -1;  /* signal is raised */
            }

            series->bf_offset = offset;

            for (; *(ts = (uint64_t *) pt) != buffer__end; pt += 16)
            {
                qp_via_t * val = (qp_via_t *) (pt + 8);
                siridb_points_add_point(series->buffer, ts, val);
            }

            offset += siridb->buffer_size;

            /* increment series->length which is 0 at this time */
            series->length += series->buffer->len;

            /* write to output file and check if write was successful */
            if ((fwrite(buffer + i * siridb->buffer_size,
                    siridb->buffer_size, 1, fp_temp) != 1))
            {
                log_critical("Could not write to temporary buffer file: '%s'",
                        fn_temp);
                fclose(fp);
                fclose(fp_temp);

                return -1;
            }
        }
    }

    if (fclose(fp) ||
        fclose(fp_temp) ||
        rename(fn_temp, fn))
    {
        log_critical("Could not rename '%s' to '%s'.", fn_temp, fn);
        return -1;
    }

    return 0;
}

void siridb_buffer_free(siridb_t * siridb)
{
    if (siridb->buffer_fp != NULL)
    {
        fclose(siridb->buffer_fp);
        siridb->buffer_fp = NULL;
    }
    free(siridb->buffer_clear);
    siridb->buffer_clear = NULL;
}

/*
 * Reserve a space in the buffer for a new series. The position of this space
 * in the buffer is read from siridb->empty_buffers so this list must have
 * at least on spot available.
 *
 * Returns 0 if successful or -1 and a signal is raised in case of an error.
 *
 * Note that an available spot must be checked before calling this function.
 * This functions has undefined behavior if no spot is found.
 */
static int buffer__use_empty(siridb_t * siridb, siridb_series_t * series)
{
    series->bf_offset = (long int) slist_pop(siridb->empty_buffers);

    if (siridb_buffer_write_empty(siridb, series))
    {
        ERR_FILE
        return -1;
    }

    return 0;
}

/*
 * Create new space in the buffer and use one position for the new series.
 * The number of positions that will be allocated is defined by
 * SIRIDB_BUFFER_CACHE and must be at least one to hold the new series.
 *
 * Returns 0 if successful or -1 and a signal is raised in case of an error.
 */
static int buffer__create_new(siridb_t * siridb, siridb_series_t * series)
{
    long int buffer_pos;
    /* get file descriptor */
    int buffer_fd = fileno(siridb->buffer_fp);

    if (buffer_fd == -1)
    {
        ERR_FILE
        return -1;
    }

    /* jump to end of buffer */
    if (fseeko(siridb->buffer_fp, 0, SEEK_END))
    {
        ERR_FILE
        return -1;
    }

    /* bind the current offset to the new series */
    if ((series->bf_offset = ftello(siridb->buffer_fp)) == -1)
    {
        ERR_FILE
        return -1;
    }

    /* write buffer start and series ID to buffer */
    if (siridb_buffer_write_empty(siridb, series))
    {
        ERR_FILE
        return -1;
    }

    buffer_pos = series->bf_offset + siridb->buffer_size * SIRIDB_BUFFER_CACHE;

    /* fill buffer with zeros if possible */
    if (ftruncate(buffer_fd, buffer_pos))
    {
        ERR_FILE
        return -1;
    }

    /* commit changes to disk */
    if (fsync(buffer_fd))
    {
        ERR_FILE
        return -1;
    }

    while ((buffer_pos -= siridb->buffer_size) > series->bf_offset)
    {
        slist_append_safe(&siridb->empty_buffers, (void *) buffer_pos);
    }

    return 0;
}
