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
#include <stdio.h>
#include <sys/unistd.h>
#include <logger/logger.h>
#include <string.h>

#define SIRIDB_BUFFER_FN "buffer.dat"

int siridb_buffer_new_series(siridb_t * siridb, siridb_series_t * series)
{
    /* get file descriptor */
    int buffer_fd = fileno(siridb->buffer_fp);

    /* jump to end of buffer */
    if (fseek(siridb->buffer_fp, 0, SEEK_END))
        return -1;

    /* bind the current offset to the new series */
    series->bf_offset = ftell(siridb->buffer_fp);

    /* write series ID to buffer */
    if (fwrite(&series->id, sizeof(uint32_t), 1, siridb->buffer_fp) != 1)
        return -1;

    /* fill buffer with zeros */
    if (ftruncate(buffer_fd, series->bf_offset + siridb->buffer_size))
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
    size_t num;
    size_t i;
    char buffer[siridb->buffer_size * read_at_once];
    char * pt;
    long int offset = 0;
    siridb_series_t * series;

    siridb_get_fn(fn, SIRIDB_BUFFER_FN)
    siridb_get_fn(fn_temp, "__" SIRIDB_BUFFER_FN)

    if (access(fn_temp, R_OK) != -1)
    {
        log_error(
                "Temporary buffer file found: '%s'. "
                "Check if something went wrong or remove this file", fn_temp);
        return 1;
    }

    if (access(fn, R_OK) == -1)
    {
        log_warning("Buffer file '%s' not found, create a new one.", fn);
        if ((fp = fopen(fn, "w")) == NULL)
        {
            log_critical("Cannot create buffer file '%s'.", fn);
            return 1;
        }
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
        return 1;
    }

    while ((num = fread(buffer, siridb->buffer_size, read_at_once, fp)))
    {
        for (i = 0; i < num; i++)
        {

            pt = buffer + i * siridb->buffer_size;
            series = (siridb_series_t *)
                    imap32_get(siridb->series_map, (uint32_t) *pt);

            if (series == NULL)
                continue;

            series->bf_offset = offset;

            offset += siridb->buffer_size;
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
