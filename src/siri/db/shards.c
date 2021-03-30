/*
 * shards.c - Collection of SiriDB shards.
 *
 * Info shards->mutex:
 *
 *  Main thread:
 *      siridb->shards :    read (lock)         write (lock)

 *  Other threads:
 *      siridb->shards :    read (lock)         write (lock)
 *
 *  Note: since series->idx hold a reference to a shard, a lock to the
 *        series_mutex is required in some cases.
 */
#include <ctype.h>
#include <dirent.h>
#include <logger/logger.h>
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <siri/db/series.inline.h>
#include <siri/db/misc.h>
#include <siri/siri.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <siri/db/db.h>
#include <xpath/xpath.h>
#include <omap/omap.h>

#define SIRIDB_SHARD_LEN 37

static bool SHARDS_must_migrate_shard(
        char * fn,
        const char * ext,
        uint64_t * shard_id)
{
    size_t n = strlen(fn);
    char * tmp = NULL;

    if (n < 5)
    {
        return false;
    }

    *shard_id = strtoull(fn, &tmp, 10);

    if (tmp == NULL)
    {
        return false;
    }

    return strcmp(tmp, ext) == 0;
}

static bool SHARDS_read_id_and_duration(
        char * fn,
        const char * ext,
        uint64_t * shard_id,
        uint64_t * duration)
{
    size_t n = strlen(fn);
    char * tmp = NULL;

    if (n != SIRIDB_SHARD_LEN)
    {
        return false;
    }

    *shard_id = strtoull(fn, &tmp, 16);
    if (tmp == NULL)
    {
        return false;
    }
    fn = tmp;

    if (*fn != '_')
    {
        return false;
    }

    ++fn;

    *duration = strtoull(fn, &tmp, 16);
    if (tmp == NULL)
    {
        return false;
    }
    fn = tmp;
    return strcmp(fn, ext) == 0;
}

/*
 * Returns true if fn is a temp shard or index filename, false if not.
 */
static bool SHARDS_is_temp_fn(char * fn)
{
    size_t n = strlen(fn);

    return (n > 8 &&
            fn[0] == '_' &&
            fn[1] == '_' &&
            fn[n-4] == '.' && ((
                fn[n-3] == 's' &&
                fn[n-2] == 'd' &&
                fn[n-1] == 'b'
            ) || (
                fn[n-3] == 'i' &&
                fn[n-2] == 'd' &&
                fn[n-1] == 'x'
    )));
}

static bool SHARDS_remove_shard_file(const char * path, const char * fn)
{
    siridb_misc_get_fn(shard_path, path, fn);
    if (unlink(shard_path))
    {
        log_error("Error while removing shard file: '%s'", shard_path);
        return false;
    }

    siridb_shard_idx_file(index_path, shard_path);
    if (xpath_file_exist(index_path) && unlink(index_path))
    {
        log_error("Error while removing index file: '%s'", index_path);
        return false;
    }

    log_warning("Shard file '%s' removed", fn);
    return true;
}


/*
 * Returns 0 if successful or -1 in case of an error.
 * (a SIGNAL might be raised in case of an error)
 */
int siridb_shards_load(siridb_t * siridb)
{
    struct stat st;
    struct dirent ** shard_list;
    char buffer[XPATH_MAX];
    int n, total, rc = 0;
    uint64_t shard_id, duration;
    bool ignore_broken_data = siri.cfg->ignore_broken_data;

    memset(&st, 0, sizeof(struct stat));

    log_info("Loading shards");

    siridb_misc_get_fn(path, siridb->dbpath, SIRIDB_SHARDS_PATH);

    if (strlen(path) >= XPATH_MAX - SIRIDB_SHARD_LEN - 1)
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
        char * base_fn = shard_list[n]->d_name;

        if (SHARDS_is_temp_fn(base_fn))
        {
            snprintf(buffer, XPATH_MAX, "%s%s", path, base_fn);

            log_warning("Removing temporary file: '%s'", buffer);

            if (unlink(buffer))
            {
                log_error("Error while removing temporary file: '%s'", buffer);
                rc = -1;
                break;
            }
        }

        if (!SHARDS_read_id_and_duration(
                base_fn,
                ".sdb",
                &shard_id,
                &duration))
        {
            if (SHARDS_must_migrate_shard(
                    base_fn,
                    ".sdb",
                    &shard_id))
            {
                log_warning("Migrate shard: '%s'", base_fn);
                if (siridb_shard_migrate(siridb, shard_id, &duration))
                {
                    log_error("Error while migrating shard: '%s'", base_fn);
                    if (!ignore_broken_data || !SHARDS_remove_shard_file(path, base_fn))
                    {
                        rc = -1;
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            else
            {
                continue;
            }
        }

        /* we are sure this fits since the filename is checked */
        if (siridb_shard_load(siridb, shard_id, duration))
        {
           log_error("Error while loading shard: '%s'", base_fn);
            if (!ignore_broken_data || !SHARDS_remove_shard_file(path, base_fn))
            {
                rc = -1;
                break;
            }
            else
            {
                continue;
            }
        }
    }

    while (total--)
    {
        free(shard_list[total]);
    }
    free(shard_list);

    return rc;
}

void siridb_shards_destroy_cb(omap_t * shards)
{
    omap_destroy(shards, (omap_destroy_cb) &siridb__shard_decref);
}

/*
 * Returns siri_err which is 0 if successful or a negative integer in case
 * of an error. (a SIGNAL is also raised in case of an error)
 */
int siridb_shards_add_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points)
{
    bool is_num = siridb_series_isnum(series);
    siridb_shard_t * shard;
    omap_t * shards;
    uint64_t duration = siridb_series_duration(series);

    uv_mutex_lock(&siridb->values_mutex);

    uint64_t expire_at = is_num ? siridb->exp_at_num : siridb->exp_at_log;

    if (duration == 0)
    {
        uint64_t interval = siri.cfg->shard_auto_duration
                ? siridb_points_get_interval(points)
                : 0;

        duration = interval
            ? siridb_shard_duration_from_interval(siridb, interval)
            : is_num
            ? siridb->duration_num
            : siridb->duration_log;
    }

    uv_mutex_unlock(&siridb->values_mutex);

    uint64_t shard_start, shard_end, shard_id;
    uint_fast32_t start, end, num_chunks, pstart, pend;
    uint16_t chunk_sz;
    uint16_t cinfo = 0;
    size_t size, pos;

    for (end = 0; end < points->len;)
    {
        shard_start = points->data[end].ts / duration * duration;
        shard_end = shard_start + duration;
        shard_id = shard_start + series->mask;

        for (   start = end;
                end < points->len && points->data[end].ts < shard_end;
                end++);

        if (shard_end < expire_at)
        {
            series->length -= end - start;
            series_update_start_end(series);
            continue;
        }

        shard = NULL;
        shards = imap_get(siridb->shards, shard_id);
        if (shards != NULL)
        {
            shard = omap_get(shards, duration);
            /* shard may be NULL if no shard according the duration is found */
        }
        else
        {
            shards = omap_create();
            if (shards == NULL || imap_add(siridb->shards, shard_id, shards))
            {
                ERR_ALLOC
                return -1;  /* might leak a few bytes */
            }
        }

        if (shard == NULL)
        {
            shard = siridb_shard_create(
                    siridb,
                    shards,
                    shard_id,
                    duration,
                    is_num ? SIRIDB_SHARD_TP_NUMBER : SIRIDB_SHARD_TP_LOG,
                    NULL);
            if (shard == NULL)
            {
                return -1;  /* signal is raised */
            }
        }

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
                        pend,
                        NULL,
                        &cinfo)) == 0)
                {
                    log_critical(
                            "Could not write points to shard '%s'",
                            shard->fn);
                }
                else
                {
                    siridb_series_add_idx(
                            series,
                            shard,
                            points->data[pstart].ts,
                            points->data[pend - 1].ts,
                            pos,
                            pend - pstart,
                            cinfo);
                    if (shard->replacing != NULL)
                    {
                        siridb_shard_write_points(
                               siridb,
                               series,
                               shard->replacing,
                               points,
                               pstart,
                               pend,
                               NULL,
                               &cinfo);
                    }
                }
            }
        }
    }
    return siri_err;
}

static inline int SHARDS_count_cb(omap_t * omap, size_t * n)
{
    *n += omap->n;
    return 0;
}

size_t siridb_shards_n(siridb_t * siridb)
{
    size_t n = 0;
    imap_walk(siridb->shards, (imap_cb) SHARDS_count_cb, &n);
    return n;
}

static inline int SHARDS_to_vec_cb(omap_t * omap, vec_t * v)
{
    omap_iter_t iter = omap_iter(omap);
    siridb_shard_t * shard;
    for (;iter && (shard = iter->data_); iter = iter->next_)
    {
        vec_append(v, shard);
        siridb_shard_incref(shard);
    }
    return 0;
}

vec_t * siridb_shards_vec(siridb_t * siridb)
{
    size_t n = siridb_shards_n(siridb);
    vec_t * vec = vec_new(n);
    if (vec == NULL)
    {
        return NULL;
    }
    (void) imap_walk(siridb->shards, (imap_cb) SHARDS_to_vec_cb, vec);
    return vec;
}

double siridb_shards_count_percent(
        siridb_t * siridb,
        uint64_t end_ts,
        uint8_t tp)
{
    size_t i;
    double percent = 1.0;
    vec_t * shards_list = NULL;
    size_t count = 0;
    size_t total = 0;
    uint64_t duration = tp == SIRIDB_SHARD_TP_NUMBER
            ? siridb->duration_num
            : siridb->duration_log;

    uv_mutex_lock(&siridb->shards_mutex);

    shards_list = siridb_shards_vec(siridb);

    uv_mutex_unlock(&siridb->shards_mutex);

    if (shards_list == NULL || shards_list->len == 0)
    {
        free(shards_list);
        return 0.0;
    }

    for (i = 0; i < shards_list->len; i++)
    {
        siridb_shard_t * shard = (siridb_shard_t *) shards_list->data[i];
        if (shard->tp == tp)
        {
            ++total;
            count += ((shard->id - shard->id % duration) + duration) < end_ts;
        }
        siridb_shard_decref(shard);
    }

    percent = total ? (double) count / (double) total : 0.0;
    free(shards_list);
    return percent;
}
