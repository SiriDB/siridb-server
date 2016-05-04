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
#include <siri/db/buffer.h>
#include <siri/db/db.h>
#include <siri/db/shard.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <logger/logger.h>
#include <string.h>

#define SIRIDB_BUFFER_FN "buffer.dat"

#define ALLOC_BUFFER                                                        \
series->buffer = (siridb_buffer_t *) malloc(sizeof(siridb_buffer_t));       \
series->buffer->points = siridb_new_points(                                 \
        /* this buffer can hold 1 more point than actual on disk */         \
        siridb->buffer_len, series->tp);


void siridb_free_buffer(siridb_buffer_t * buffer)
{
    if (buffer != NULL)
    {
        siridb_free_points(buffer->points);
        free(buffer);
    }
}

void siridb_buffer_to_shards(siridb_t * siridb, siridb_series_t * series)
{
    siridb_shard_t * shard;
    uint64_t duration = SIRIDB_SERIES_ISNUM(series) ?
            siridb->duration_num : siridb->duration_log;

    uint64_t shard_start, shard_end, shard_id;
    uint_fast32_t start, end;

    for (end = 0; end < series->buffer->points->len;)
    {
        shard_start =
                series->buffer->points->data[end].ts / duration * duration;
        shard_end = shard_start + duration;
        shard_id = shard_start + series->mask;

        if ((shard = imap64_get(siridb->shards, shard_id)) == NULL)
        {
            shard = siridb_shard_create(
                    siridb,
                    shard_id,
                    duration,
                    series->tp);
        }

        for (   start = end;
                end < series->buffer->points->len &&
                    series->buffer->points->data[end].ts < shard_end;
                end++);

        if (start != end)
        {
            siridb_shard_write_points(
                    siridb,
                    series,
                    shard,
                    series->buffer->points,
                    start,
                    end);
        }

    }
}

void siridb_buffer_write_len(
        siridb_t * siridb,
        siridb_series_t * series)
{
    /* go to the series position in buffer */
    fseek(  siridb->buffer_fp,
            series->buffer->bf_offset + sizeof(uint32_t),
            SEEK_SET);

    /* write new length */
    fwrite(&series->buffer->points->len, sizeof(size_t), 1, siridb->buffer_fp);
}

void siridb_buffer_write_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val)
{
    siridb_buffer_write_len(siridb, series);

    /* jump to position where to write the new point */
    fseek(siridb->buffer_fp, 16 * (series->buffer->points->len - 1), SEEK_CUR);

    /* write time-stamp */
    fwrite(ts, sizeof(uint64_t), 1, siridb->buffer_fp);

    /* write value */
    fwrite(val, sizeof(qp_via_t), 1, siridb->buffer_fp);
}

int siridb_buffer_new_series(siridb_t * siridb, siridb_series_t * series)
{
    /* get file descriptor */
    int buffer_fd = fileno(siridb->buffer_fp);

    /* jump to end of buffer */
    if (fseek(siridb->buffer_fp, 0, SEEK_END))
        return -1;

    /* allocate new buffer */
    ALLOC_BUFFER

    /* bind the current offset to the new series */
    series->buffer->bf_offset = ftell(siridb->buffer_fp);

    /* write series ID to buffer */
    if (fwrite(&series->id, sizeof(uint32_t), 1, siridb->buffer_fp) != 1)
        return -1;

    /* fill buffer with zeros */
    if (ftruncate(buffer_fd, series->buffer->bf_offset + siridb->buffer_size))
        return -1;

    /* commit changes to disk */
    if (fsync(buffer_fd))
        return -1;

    return 0;
}

int siridb_open_buffer(siridb_t * siridb)
{
    siridb_get_fn(fn, SIRIDB_BUFFER_FN)

    if ((siridb->buffer_fp = fopen(fn, "r+")) == NULL)
    {
        log_critical("Cannot open '%s' for reading and writing", fn);
        return 1;
    }

    return 0;
}

int siridb_load_buffer(siridb_t * siridb)
{
    FILE * fp;
    FILE * fp_temp;
    size_t read_at_once = 8;
    size_t num, i, j;
    char buffer[siridb->buffer_size * read_at_once];
    char * pt;
    long int offset = 0;
    siridb_series_t * series;

    log_info("Read and cleanup buffer");

    siridb_get_fn(fn, SIRIDB_BUFFER_FN)
    siridb_get_fn(fn_temp, "__" SIRIDB_BUFFER_FN)

    if (access(fn_temp, F_OK) != -1)
    {
        log_error(
                "Temporary buffer file found: '%s'. "
                "Check if something went wrong or remove this file", fn_temp);
        return 1;
    }

    if (access(fn, F_OK) == -1)
    {
        log_warning("Buffer file '%s' not found, create a new one.", fn);
        if ((fp = fopen(fn, "w")) == NULL)
        {
            log_critical("Cannot create buffer file '%s'.", fn);
            return 1;
        }
        fclose(fp);
        return 0;
    }

    if ((fp = fopen(fn, "r")) == NULL)
    {
        log_critical("Cannot open '%s' for reading", fn);
        return 1;
    }

    if ((fp_temp = fopen(fn_temp, "w")) == NULL)
    {
        log_critical("Cannot open '%s' for writing", fn_temp);
        fclose(fp);
        return 1;
    }

    while ((num = fread(buffer, siridb->buffer_size, read_at_once, fp)))
    {
        for (i = 0; i < num; i++)
        {

            pt = buffer + i * siridb->buffer_size;

            series = (siridb_series_t *)
                    imap32_get(siridb->series_map, *((uint32_t *) pt));

            if (series == NULL)
                continue;

            ALLOC_BUFFER

            series->buffer->bf_offset = offset;

            pt += sizeof(uint32_t);

            for (   j = (size_t) *pt, pt += sizeof(size_t);
                    j--;
                    pt += 16)
            {
                siridb_points_add_point(
                        series->buffer->points,
                        (uint64_t *) pt,
                        (qp_via_t *) (pt + 8));
            }

            offset += siridb->buffer_size;

            /* write to output file and check if write was successful */
            if ((fwrite(buffer + i * siridb->buffer_size,
                    siridb->buffer_size, 1, fp_temp) != 1))
            {
                log_critical("Could not write to temporary buffer file: '%s'",
                        fn_temp);

                fclose(fp);
                fclose(fp_temp);

                return 1;
            }
        }
    }

    fclose(fp);
    fclose(fp_temp);

    if (rename(fn_temp, fn))
    {
        log_critical("Could not rename '%s' to '%s'.", fn_temp, fn);
        return 1;
    }

    return 0;
}
