/*
 * shards.c - SiriDB shards.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 * Info shards->mutex:
 *
 *  Main thread:
 *      siridb->shards :    read (lock)         write (lock)

 *  Other threads:
 *      siridb->shards :    read (lock)         write (lock)
 *
 */
#include <ctype.h>
#include <dirent.h>
#include <logger/logger.h>
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <siri/siri.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SIRIDB_MAX_SHARD_FN_LEN 23

static bool is_shard_fn(const char * fn);
static bool is_temp_shard_fn(const char * fn);

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a SIGNAL might be raised in case of an error)
 */
int siridb_shards_load(siridb_t * siridb)
{
    struct stat st = {0};
    struct dirent ** shard_list;
    char buffer[PATH_MAX];
    int n, total, rc = 0;

    log_info("Loading shards");

    SIRIDB_GET_FN(path, SIRIDB_SHARDS_PATH);

    if (strlen(path) >= PATH_MAX - SIRIDB_MAX_SHARD_FN_LEN - 1)
    {
        log_error("Shard path too long: '%s'", path);
        return -1;
    }

    if (stat(path, &st) == -1)
    {
        log_warning(
                "Shards directory not found, creating directory '%s'.",
                path);
        if (mkdir(path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.", path);
            return -1;
        }
    }

    total = scandir(path, &shard_list, NULL, alphasort);

    if (total < 0)
    {
        /* no need to free shard_list when total < 0 */
        log_error("Cannot read shards directory '%s'.", path);
        return -1;
    }

    for (n = 0; n < total; n++)
    {
        if (is_temp_shard_fn(shard_list[n]->d_name))
        {
            snprintf(buffer, PATH_MAX, "%s%s",
                   path, shard_list[n]->d_name);

            log_warning("Temporary shard found, we will remove file: '%s'",
                   buffer);

            if (unlink(buffer))
            {
                log_error("Could not remove temporary file: '%s'", buffer);
                rc = -1;
                break;
            }
        }

        if (!is_shard_fn(shard_list[n]->d_name))
        {
            continue;
        }

        /* we are sure this fits since the filename is checked */
        if (siridb_shard_load(siridb, (uint64_t) atoll(shard_list[n]->d_name)))
        {
           log_error("Error while loading shard: '%s'", shard_list[n]->d_name);
           rc = -1;
           break;
        }
    }

    while (total--)
    {
        free(shard_list[total]);
    }
    free(shard_list);

    return rc;
}

//void siridb_shards_drop_work(uv_work_t * work)
//{
//    uv_async_t * handle = (uv_async_t *) work->data;
//    siridb_query_t * query = (siridb_query_t *) handle->data;
//    query_select_t * q_drop = (query_drop_t *) query->data;
//
//    int rc = 0;
//    /*
//     * We need to check for SiriDB errors because this task is running in
//     * another thread. In case a siri_err is set, this means we are in forced
//     * closing state and we should not use the handle but let siri close it.
//     */
//    if (!siri_err)
//    {
//        switch (rc)
//        {
//        case -1:
//            sprintf(query->err_msg, "Memory allocation error.");
//            /* no break */
//        case 1:
//            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
//            return;
//        }
//
//        if (query->nodes == NULL)
//        {
//            siridb_send_query_result(handle);
//        }
//        else
//        {
//            uv_async_send(handle);
//        }
//    }
//}
//
//void siridb_shards_drop_work_finished(uv_work_t * work, int status)
//{
//    if (status)
//    {
//        log_error("Drop shards work failed (error: %s)",
//                uv_strerror(status));
//    }
//    siri_async_decref((uv_async_t **) &work->data);
//
//    free(work);
//}

/*
 * Returns siri_err which is 0 if successful or a negative integer in case
 * of an error. (a SIGNAL is also raised in case of an error)
 */
int siridb_shards_add_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points)
{
    siridb_shard_t * shard;
    uint64_t duration = siridb_series_isnum(series) ?
            siridb->duration_num : siridb->duration_log;
    uint64_t shard_start, shard_end, shard_id;
    uint_fast32_t start, end, num_chunks, pstart, pend;
    uint16_t chunk_sz;
    size_t size;
    long int pos;

    for (end = 0; end < points->len;)
    {
        shard_start = points->data[end].ts / duration * duration;
        shard_end = shard_start + duration;
        shard_id = shard_start + series->mask;

        if ((shard = imap_get(siridb->shards, shard_id)) == NULL)
        {
            shard = siridb_shard_create(
                    siridb,
                    shard_id,
                    duration,
                    siridb_series_isnum(series) ?
                            SIRIDB_SHARD_TP_NUMBER : SIRIDB_SHARD_TP_LOG,
                    NULL);
            if (shard == NULL)
            {
                return -1;  /* signal is raised */
            }
        }

        for (   start = end;
                end < points->len && points->data[end].ts < shard_end;
                end++);

        if (start != end)
        {
            size = end - start;

            num_chunks = (size - 1) / shard->max_chunk_sz + 1;
            chunk_sz = size / num_chunks + (size % num_chunks != 0);

            for (pstart = start; pstart < end; pstart += chunk_sz)
            {
                pend = pstart + chunk_sz;
                if (pend > end)
                {
                    pend = end;
                }

                if ((pos = siridb_shard_write_points(
                        siridb,
                        series,
                        shard,
                        points,
                        pstart,
                        pend)) < 0)
                {
                    log_critical(
                            "Could not write points to shard id '%llu",
                            (unsigned long long) shard->id);
                }
                else
                {
                    siridb_series_add_idx(
                            series,
                            shard,
                            points->data[pstart].ts,
                            points->data[pend - 1].ts,
                            pos,
                            pend - pstart);
                    if (shard->replacing != NULL)
                    {
                        siridb_shard_write_points(
                               siridb,
                               series,
                               shard->replacing,
                               points,
                               pstart,
                               pend);
                    }
                }
            }
        }
    }
    return siri_err;
}

/*
 * Returns true if fn is a shard filename, false if not.
 */
static bool is_shard_fn(const char * fn)
{
    if (!isdigit(*fn) || strlen(fn) > SIRIDB_MAX_SHARD_FN_LEN)
    {
        return false;
    }

    fn++;
    while (*fn && isdigit(*fn))
    {
        fn++;
    }

    return (strcmp(fn, ".sdb") == 0);
}

/*
 * Returns true if fn is a temp shard filename, false if not.
 */
static bool is_temp_shard_fn(const char * fn)
{
    for (int i = 0; i < 2; i++, fn++)
    {
        if (*fn != '_')
        {
            return false;
        }
    }
    return is_shard_fn(fn);
}
