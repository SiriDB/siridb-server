/*
 * series.c - SiriDB Time Series.
 *
 *
 * Info siridb->series_mutex:
 *
 *  Main thread:
 *      siridb->series_map :    read (no lock)      write (lock)
 *      series->idx :           read (lock)         write (lock)
 *
 *  Other threads:
 *      siridb->series_map :    read (lock)          write (not allowed)
 *      series->idx :           read (lock)         write (lock)
 *
 *  Note:   One exception to 'not allowed' are the free functions
 *          since they only run when no other references to the object exist.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <logger/logger.h>
#include <siri/db/buffer.h>
#include <siri/db/db.h>
#include <siri/db/misc.h>
#include <siri/db/series.h>
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <siri/err.h>
#include <siri/siri.h>
#include <xpath/xpath.h>

#define SIRIDB_SERIES_FN "series.dat"
#define SIRIDB_CORRUPT_FN "corrupt_series.dat"
#define SIRIDB_DROPPED_FN ".dropped"
#define SIRIDB_MAX_SERIES_ID_FN ".max_series_id"
#define SIRIDB_SERIES_SCHEMA 1
#define DROPPED_DUMMY 1

/*
 * Used for storing double and integers as string. this is not very important
 * if it will not store all characters generated so 64 is more than enough
 */
#define STR_TYPE_BUF_SZ 64
static char str_type_buf[STR_TYPE_BUF_SZ];

static int SERIES_save(siridb_t * siridb);
static int SERIES_load(siridb_t * siridb, imap_t * dropped);
static int SERIES_read_dropped(siridb_t * siridb, imap_t * dropped);
static int SERIES_open_new_dropped_file(siridb_t * siridb);
static int SERIES_open_dropped_file(siridb_t * siridb);
static int SERIES_update_max_id(siridb_t * siridb);
static void SERIES_update_start(siridb_series_t *__restrict series);
static void SERIES_update_end(siridb_series_t *__restrict series);
static void SERIES_update_overlap(siridb_series_t *__restrict series);
static inline int SERIES_pack(siridb_series_t * series, qp_fpacker_t * fpacker);
static void SERIES_idx_sort(
        idx_t * idx,
        uint_fast32_t start,
        uint_fast32_t end);

static siridb_series_t * SERIES_new(
        siridb_t * siridb,
        uint32_t id,
        uint8_t tp,
        uint16_t pool,
        const char * name);

const char series_type_map[3][8] = {
        "integer",
        "float",
        "string"
};

const uint8_t SERIES_SFC =
        SIRIDB_SHARD_HAS_NEW_VALUES | SIRIDB_SHARD_IS_LOADING;

/*
 * Call-back used to compare series properties.
 *
 * Returns 1 when series match or 0 if not.
 */
int siridb_series_cexpr_cb(siridb_series_t * series, cexpr_condition_t * cond)
{
    switch ((enum cleri_grammar_ids) cond->prop)
    {
    case CLERI_GID_K_LENGTH:
        return cexpr_int_cmp(cond->operator, series->length, cond->int64);
    case CLERI_GID_K_START:
        return cexpr_int_cmp(cond->operator, series->start, cond->int64);
    case CLERI_GID_K_END:
        return cexpr_int_cmp(cond->operator, series->end, cond->int64);
    case CLERI_GID_K_POOL:
        return cexpr_int_cmp(cond->operator, series->pool, cond->int64);
    case CLERI_GID_K_SHARD_DURATION:
        return cexpr_int_cmp(
                cond->operator,
                (series->idx ? series->idx->shard->duration : 0),
                cond->int64);
    case CLERI_GID_K_TYPE:
        return cexpr_int_cmp(cond->operator, series->tp, cond->int64);
    case CLERI_GID_K_NAME:
        return cexpr_str_cmp(cond->operator, series->name, cond->str);
    default:
        /* we must NEVER get here */
        log_critical("Unexpected series property received: %d", cond->prop);
        assert (0);
    }
    return -1;
}

void series_update_start_end(siridb_series_t * series)
{
    SERIES_update_start(series);
    SERIES_update_end(series);
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 *
 * Warnings:
 * -    Do not call this function after a siri_err has occurred since this
 *      would risk a stack overflow in case this series has caused the siri_err
 *      and the buffer length is not reset.
 *
 * -    This method will update the series->length but updating the time-stamps
 *      (series->start and series->end) should be done outside this function.
 */
int siridb_series_add_point(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        uint64_t * ts,
        qp_via_t * val)
{
    assert (!siri_err);
    assert (series->buffer != NULL);
    int rc = 0;

    series->length++;

    /* add point in memory
     * (memory can hold 1 more point than we can hold on disk)
     */
    siridb_points_add_point(series->buffer, ts, val);

    if (series->buffer->len == siridb->buffer->len)
    {
        if (siridb_shards_add_points(
                siridb,
                series,
                series->buffer))
        {
            rc = -1;  /* signal is raised */
        }
        else
        {
            series->buffer->len = 0;
            if (siridb_buffer_write_empty(siridb->buffer, series))
            {
                ERR_FILE
                rc = -1;
            }
        }
    }
    else
    {
        if (siridb_buffer_write_point(siridb->buffer, series, ts, val))
        {
            ERR_FILE
            log_critical("Cannot write new point to buffer");
            rc = -1;
        }
    }

    return rc;
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 *
 * Warnings:
 * -    Do not call this function after a siri_err has occurred since this
 *      would risk a stack overflow in case this series has caused the siri_err
 *      and the buffer length is not reset.
 *
 * -    This method will update the series->length but updating the time-stamps
 *      (series->start and series->end) should be done outside this function.
 */
int siridb_series_add_pcache(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        siridb_pcache_t *__restrict pcache)
{
    if (pcache->len > siridb->buffer->len || series->buffer == NULL)
    {
        series->length += pcache->len;

        return siridb_shards_add_points(
                siridb,
                series,
                (siridb_points_t *) pcache);
    }

    if (pcache->len + series->buffer->len > siridb->buffer->len)
    {
        series->length += pcache->len;

        siridb_points_t *__restrict points = series->buffer;
        size_t i = points->len;
        siridb_point_t *__restrict point;

        while (i--)
        {
            point = points->data + i;
            if (siridb_pcache_add_point(pcache, &point->ts, &point->val))
            {
                return -1;  /* signal is raised */
            }
        }

        if (siridb_shards_add_points(
                siridb,
                series,
                (siridb_points_t *) pcache))
        {
            return -1;  /* signal is raised */
        }

        series->buffer->len = 0;
        if (siridb_buffer_write_empty(siridb->buffer, series))
        {
            ERR_FILE
            return -1;
        }
    }
    else
    {
        siridb_point_t *__restrict point;
        size_t i;

        for (i = 0; i < pcache->len; i++)
        {
            point = pcache->data + i;

            if (siridb_series_add_point(
                    siridb,
                    series,
                    &point->ts,
                    &point->val))
            {
                return -1;  /* signal is raised */
            }
        }
    }

    return 0;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * This function adds the new series to siridb->series_map and siridb->series.
 */
siridb_series_t * siridb_series_new(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp)
{
    siridb_series_t * series;

    siridb->max_series_id++;
    series = SERIES_new(
            siridb,
            siridb->max_series_id,
            tp,
            siridb->server->pool,
            series_name);

    if (series == NULL)
    {
        return NULL;  /* signal is raised */
    }
    /* add series to the store */
    if (qp_fadd_type(siridb->store, QP_ARRAY3) ||
        qp_fadd_raw(
                siridb->store,
                (const unsigned char *) series_name,
                series->name_len + 1) ||
        qp_fadd_int64(siridb->store, (int64_t) series->id) ||
        qp_fadd_int64(siridb->store, (int64_t) series->tp) ||
        qp_flush(siridb->store))
    {
        ERR_FILE
        log_critical("Cannot write series '%s' to store.", series_name);
        siridb__series_free(series);
        return NULL;
    }

    /* create a buffer for series (except string series) */
    if (tp != TP_STRING && siridb_buffer_new_series(siridb->buffer, series))
    {
        /* signal is raised */
        log_critical("Could not create buffer for series '%s'.",
                series_name);
        siridb__series_free(series);
        return NULL;
    }

    if (imap_add(siridb->series_map, series->id, series))
    {
        log_critical("Error adding series '%s' to the internal imap.",
                series_name);
        siridb__series_free(series);
        ERR_ALLOC
        return NULL;
    }

    if (ct_add(siridb->series, series->name, series))
    {
        log_critical("Error adding series '%s' to the internal smap.",
                series_name);
        imap_pop(siridb->series_map, series->id);
        siridb__series_free(series);
        ERR_ALLOC
        return NULL;
    }

    /* we can ignore the result code since this is not critical and logging
     * is done by the function.
     */
    siridb_groups_add_series(siridb->groups, series);

    return series;
}

/*
 * NEVER call this function but call siridb_series_decref instead.
 *
 * Destroy series. (parsing NULL is not allowed)
 */
void siridb__series_free(siridb_series_t *__restrict series)
{
    siridb_shard_t * shard;
    uint_fast32_t i;

    /* mark shards with dropped series flag */
    for (i = 0; i < series->idx_len; i++)
    {
        shard = series->idx[i].shard;
        shard->flags |= SIRIDB_SHARD_HAS_DROPPED_SERIES;
        siridb_shard_decref(shard);
    }

    if (series->buffer != NULL)
    {
        siridb_points_free(series->buffer);
        if (series->flags & SIRIDB_SERIES_IS_DROPPED)
        {
            vec_append_safe(
                &series->siridb->buffer->empty,
                (void *) series->bf_offset);
        }
    }

    free(series->idx);
    free(series->name);
    free(series);
}

/*
 * Returns 0 if successful or -1 in case or an error.
 * (a SIGNAL might be raised)
 */
int siridb_series_load(siridb_t * siridb)
{
    imap_t * dropped;

    /* we must have a server because we need to know the pool id */
    assert (siridb->server != NULL);
    log_info("Loading series");

    dropped = imap_new();

    if (dropped == NULL)
    {
        return -1;
    }

    if (SERIES_read_dropped(siridb, dropped) ||
        SERIES_load(siridb, dropped))
    {
        imap_free(dropped, NULL);
        return -1;
    }

    imap_free(dropped, NULL);

    if (SERIES_update_max_id(siridb) ||
        SERIES_open_new_dropped_file(siridb) ||
        siridb_series_open_store(siridb))
    {
        return -1;
    }

    return 0;
}

/*
 * This function should only be called when new values are added.
 * For example, during optimization we do not use this function for
 * replacing indexes. This way we can set the HAS_NEW_VALUES correctly.
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int siridb_series_add_idx(
        siridb_series_t *__restrict series,
        siridb_shard_t *__restrict shard,
        uint64_t start_ts,
        uint64_t end_ts,
        uint32_t pos,
        uint16_t len,
        uint16_t cinfo)
{
    idx_t * idx;
    uint32_t i = series->idx_len;
    series->idx_len++;

    /* never zero */
    idx = (idx_t *) realloc(
            series->idx,
            series->idx_len * sizeof(idx_t));
    if (idx == NULL)
    {
        ERR_ALLOC
        series->idx_len--;
        return -1;
    }
    series->idx = idx;

    for (; i && start_ts < series->idx[i - 1].start_ts; i--)
    {
        series->idx[i] = series->idx[i - 1];
    }

    idx = series->idx + i;

    /* Do not set the new values check when shard is loading. */
    if (!(shard->flags & SERIES_SFC))
    {
        shard->flags |= SIRIDB_SHARD_HAS_NEW_VALUES;
    }

    idx->start_ts = start_ts;
    idx->end_ts = end_ts;
    idx->len = len;
    idx->shard = shard;
    idx->pos = pos;
    idx->cinfo = cinfo;

    /* We do not have to save an overlap since it will be detected again when
     * reading the shard at startup.
     */
    if (    (i > 0 &&
            start_ts < series->idx[i - 1].end_ts) ||
            (i + 1 < series->idx_len &&
            end_ts > series->idx[i + 1].start_ts))
    {
        shard->flags |= SIRIDB_SHARD_HAS_OVERLAP;
        series->flags |= SIRIDB_SERIES_HAS_OVERLAP;
    }

    siridb_shard_incref(shard);

    return 0;
}

/*
 * Remove series from the indexes and mark the series as 'dropped'.
 *
 * Do not forget to call 'siridb_series_drop_commit' to commit the drop.
 */
void siridb_series_drop_prepare(siridb_t * siridb, siridb_series_t * series)
{
    /* remove series from map */
    imap_pop(siridb->series_map, series->id);

    /* remove series from tree */
    ct_pop(siridb->series, series->name);

    series->flags |= SIRIDB_SERIES_IS_DROPPED;
}

/*
 * Write dropped series to file and decrement series reference counter.
 * Function 'siridb_series_drop_prepare' should be called before this function.
 *
 * In case we cannot write the series_id to the drop file we log critical and
 * return -1 but no signal is raised.
 *
 * Warning: do not forget to call 'siridb_series_flush_dropped()'
 *
 * Return 0 if successful.
 */
int siridb_series_drop_commit(siridb_t * siridb, siridb_series_t * series)
{
    assert (series->flags & SIRIDB_SERIES_IS_DROPPED);

    int rc = 0;

    if (siridb->dropped_fp == NULL && SERIES_open_dropped_file(siridb))
    {
        rc = -1;
    }
    /* we are sure the pointer is at the end of the file */
    else if (fwrite(&series->id, sizeof(uint32_t), 1, siridb->dropped_fp) != 1)
    {
        log_critical("Cannot write %d to dropped cache file.", series->id);
        rc = -1;
    };

    /* decrement reference to series */
    siridb_series_decref(series);

    return rc;
}

/*
 * In case we cannot write the series_id to the drop file we log critical but
 * no other action is taken.
 *
 * Warning: do not forget to call 'siridb_series_flush_dropped()'
 *
 * Return 0 if successful or -1 in case the dropped series was not written
 * to file. The series is still dropped from memory in the case.
 *
 * In case the series is flagged as 'dropped', 0 (successful) is returned.
 */
int siridb_series_drop(siridb_t * siridb, siridb_series_t * series)
{
    /*
     * This function is used from build series maps in async functions.
     * Therefore a series might already be dropped, for example by a second
     * drop statement or the re-index task.
     */
    if (~series->flags & SIRIDB_SERIES_IS_DROPPED)
    {
        siridb_series_drop_prepare(siridb, series);
        return siridb_series_drop_commit(siridb, series);
    }
    return 0;
}

/*
 * Return 0 if successful or EOF if not.
 *
 * Logging is done but no error is raised in case of failure.
 *
 * This function flushes the dropped file and sets the dropped series flag
 * for groups update thread.
 */
int siridb_series_flush_dropped(siridb_t * siridb)
{
    int rc = 0;

    if (siridb->dropped_fp == NULL && SERIES_open_dropped_file(siridb))
    {
        rc = -1;
    }
    else if (fflush(siridb->dropped_fp))
    {
        siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_DROPPED_FN)
        log_critical("Could not flush dropped file: '%s'", fn);
        rc = -1;
    }

    siridb->groups->flags |= GROUPS_FLAG_DROPPED_SERIES;
    siridb->tags->flags |= TAGS_FLAG_DROPPED_SERIES;

    return rc;
}

/*
 * Re-allocations in this function can fail but are not critical.
 *
 * Note that 'series' can be destroyed when series->length has reached zero.
 */
void siridb_series_remove_shard(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        siridb_shard_t *__restrict shard)
{
    idx_t *__restrict idx;
    uint_fast32_t i, offset;
    uint64_t start = shard->id - series->mask;
    uint64_t end = start + shard->duration;

    i = offset = 0;

    for (   idx = series->idx;
            i < series->idx_len;
            i++, idx++)
    {
        if (idx->shard == shard)
        {
            siridb_shard_decref(shard);
            offset++;
            series->length -= idx->len;
        }
        else if (offset)
        {
            series->idx[i - offset] = series->idx[i];
        }
    }

    if (offset)
    {
        if (!series->length)
        {
            series->idx_len = 0;

            if (siridb_series_drop(siridb, series))
            {
                siridb_series_flush_dropped(siridb);
            }
        }
        else
        {
            series->idx_len -= offset;
            idx = (idx_t *) realloc(
                        series->idx,
                        series->idx_len * sizeof(idx_t));
            if (idx == NULL && series->idx_len)
            {
                log_error("Re-allocation failed while removing series from "
                        "shard index");
            }
            else
            {
                series->idx = idx;
            }
            if (series->start >= start && series->start < end)
            {
                SERIES_update_start(series);
            }
            if (series->end < end && series->end > start)
            {
                SERIES_update_end(series);
            }
        }
    }
}

/*
 * Update series properties.
 *
 * (series might be destroyed if series->length is zero)
 */
void siridb_series_update_props(siridb_t * siridb, siridb_series_t * series)
{
    if (series->tp != TP_STRING && series->buffer == NULL)
    {
        log_error(
            "Drop '%s' (%" PRIu32 ") since no buffer is found for this series",
            series->name,
            series->id);
        siridb_series_drop(siridb, series);
    }
    else
    {
        SERIES_update_start(series);
        SERIES_update_end(series);

        if (!series->length)
        {
            log_warning(
                "Drop '%s' (%" PRIu32 ") since no data is found for this series",
                series->name,
                series->id);
            siridb_series_drop(siridb, series);
        }
    }
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_points_t * siridb_series_get_points(
        siridb_series_t *__restrict series,
        uint64_t *__restrict start_ts,
        uint64_t *__restrict end_ts)
{
    idx_t *__restrict idx;
    siridb_points_t *__restrict points;
    siridb_point_t *__restrict point;
    size_t len, size;
    uint32_t i;
    uint32_t indexes[series->idx_len];
    len = i = size = 0;

    for (   idx = series->idx;
            i < series->idx_len;
            i++, idx++)
    {
        if (    (start_ts == NULL || idx->end_ts >= *start_ts) &&
                (end_ts == NULL || idx->start_ts < *end_ts))
        {
            size += idx->len;
            indexes[len] = i;
            len++;
        }
    }

    size += (series->buffer == NULL) ? 0 : series->buffer->len;
    points = siridb_points_new(size, series->tp);

    if (points == NULL)
    {
        ERR_ALLOC  /* TODO: maybe remove ERR_ALLOC */
        return NULL;
    }

    for (i = 0; i < len; i++)
    {
        idx = series->idx + indexes[i];
        siridb_shard_get_points_callback(idx->shard->flags, series)(
                points,
                idx,
                start_ts,
                end_ts,
                series->flags & SIRIDB_SERIES_HAS_OVERLAP);
        /* errors can be ignored here */
    }

    if (series->buffer != NULL)
    {
        /* create pointer to buffer and get current length */
        point = series->buffer->data;
        len = series->buffer->len;

        /* crop start buffer if needed */
        if (start_ts != NULL)
        {
            for (; len && point->ts < *start_ts; point++, len--);
        }

        /* crop end buffer if needed */
        if (end_ts != NULL && len)
        {
            siridb_point_t *__restrict p;

            for (   p = point + len - 1;
                    len && p->ts >= *end_ts;
                    p--, len--);
        }

        /* add buffer points */
        for (; len; point++, len--)
        {
            siridb_points_add_point(points, &point->ts, &point->val);
        }
    }

    if (points->len < size && siridb_points_resize(points, points->len))
    {
        log_error("Re-allocation points has failed");
    }

    return points;
}

/*
 * Can be used instead of the macro function when need as callback function.
 */
void siridb__series_decref(siridb_series_t * series)
{
    siridb_series_decref(series);
}

siridb_points_t * siridb_series_get_first(
        siridb_series_t * series, int * required_shard)
{
    siridb_points_t * buf = series->buffer;
    siridb_points_t * points;
    uint64_t start;

    if (buf != NULL &&
        buf->len &&
        buf->data->ts == series->start)
    {
        points = siridb_points_new(1, series->tp);
        if (points == NULL)
        {
            return NULL;
        }

        /* string type does not have a buffer so we don't have to worry */
        points->data->ts = buf->data->ts;
        points->data->val = buf->data->val;
        points->len = 1;
        return points;
    }

    (*required_shard)++;

    /* if not in the buffer, then if must be in a shard */
    assert (series->idx_len);

    idx_t * first = series->idx;

    points = siridb_points_new(first->len, series->tp);
    if (points == NULL)
    {
        return NULL;
    }

    start = series->start + 1;

    siridb_shard_get_points_callback(first->shard->flags, series)(
            points,
            first,
            NULL,
            &start,
            series->flags & SIRIDB_SERIES_HAS_OVERLAP);

    /* we must have at least one point, more points are possible when
     * having multiple points at the same time-stamp. */
    assert (points->len);

    while (points->len > 1)
    {
        --points->len;
        if (points->tp == TP_STRING)
        {
            free((points->data + points->len)->val.str);
        }
    }

    return points;
}

siridb_points_t * siridb_series_get_last(
        siridb_series_t * series, int * required_shard)
{
    siridb_points_t * buf = series->buffer;
    siridb_points_t * points;
    siridb_point_t * point;

    if (buf != NULL &&
        buf->len &&
        (point = buf->data + (buf->len - 1))->ts == series->end)
    {
        points = siridb_points_new(1, series->tp);
        if (points == NULL)
        {
            return NULL;
        }

        /* string type does not have a buffer so we don't have to worry */
        points->data->ts = point->ts;
        points->data->val = point->val;
        points->len = 1;
        return points;
    }

    (*required_shard)++;

    /* if not in the buffer, then if must be in a shard */
    assert (series->idx_len);

    /* if not in the buffer, then if must be in a shard */

    size_t i = series->idx_len - 1;
    idx_t * idx = series->idx + i;
    idx_t * last = idx;

    for (; i && last->shard == (--idx)->shard; --i)
    {
        if (idx->end_ts > last->end_ts)
        {
            last = idx;
        }
    }

    points = siridb_points_new(last->len, series->tp);
    if (points == NULL)
    {
        return NULL;
    }

    siridb_shard_get_points_callback(last->shard->flags, series)(
            points,
            last,
            &last->end_ts,
            NULL,
            series->flags & SIRIDB_SERIES_HAS_OVERLAP);

    /* we must have at least one point, more points are possible when
     * having multiple points at the same time-stamp. */
    assert (points->len);

    while (points->len > 1)
    {
        --points->len;
        if (points->tp == TP_STRING)
        {
            free((points->data + points->len)->val.str);
        }
    }

    return points;
}

siridb_points_t * siridb_series_get_count(siridb_series_t * series)
{
    siridb_points_t * points = siridb_points_new(1, TP_INT);
    if (points != NULL)
    {
        points->data->ts = series->end;
        points->data->val.int64 = series->length;
        points->len = 1;
    }
    return points;
}

void siridb_series_ensure_type(siridb_series_t * series, qp_obj_t * qp_obj)
{
    switch(series->tp)
    {
    case TP_INT:
        if (qp_obj->tp != QP_INT64)
        {
            if (qp_obj->tp == QP_DOUBLE)
            {
                double d = qp_obj->via.real;
                qp_obj->via.int64 = (int64_t) d;
            }
            else if (qp_obj->tp == QP_RAW)
            {
                char * s = strndup(qp_obj->via.str, qp_obj->len);
                qp_obj->via.int64 = \
                        (s == NULL) ? 0 : (int64_t) strtoll(s, NULL, 10);
                free(s);
            }
            else
            {
                assert(0);
            }
            qp_obj->tp = QP_INT64;
        }
        return;
    case TP_DOUBLE:
        if (qp_obj->tp != QP_DOUBLE)
        {
            if (qp_obj->tp == QP_INT64)
            {
                int64_t i = qp_obj->via.int64;
                qp_obj->via.real = (double) i;
            }
            else if (qp_obj->tp == QP_RAW)
            {
                char * s = strndup(qp_obj->via.str, qp_obj->len);
                qp_obj->via.real = \
                        (s == NULL) ? 0.0 : strtod(s, NULL);
                free(s);
            }
            else
            {
                assert(0);
            }
            qp_obj->tp = QP_DOUBLE;
        }
        return;
    case TP_STRING:
        if (qp_obj->tp != QP_RAW)
        {
            if (qp_obj->tp == QP_INT64)
            {
                qp_obj->len = snprintf(
                        str_type_buf,
                        STR_TYPE_BUF_SZ,
                        "%" PRId64,
                        qp_obj->via.int64);
                qp_obj->via.str = str_type_buf;
            }
            else if (qp_obj->tp == QP_DOUBLE)
            {
                qp_obj->len = snprintf(
                        str_type_buf,
                        STR_TYPE_BUF_SZ,
                        "%f",
                        qp_obj->via.real);
                qp_obj->via.str = str_type_buf;
            }
            else
            {
                assert(0);
            }
            qp_obj->tp = QP_RAW;
        }
        return;
    }
    assert (0);
}

/*
 * Calculate the server id.
 * Returns 0 or 1, representing a server in a pool)
 */
uint8_t siridb_series_server_id_by_name(const char * name)
{
    uint32_t n;

    /* get sum series name to calculate series mask (for sharding) */
    for (n = 0; *name; name++)
    {
        n += *name;
    }

   return (uint8_t) ((n / 11) % 2);
}

/*
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of a critical
 * error.
 * Note that we also return 0 if we had to recover a shard. In this case you
 * can find the errors we had to recover in the log file. (log level should
 * be at least 'ERROR' for all error logs)
 */
int siridb_series_optimize_shard(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        siridb_shard_t *__restrict shard)
{
    idx_t *__restrict idx;

    uint_fast32_t i, start, end, new_idx;
    uint64_t max_ts;
    size_t size;
    siridb_points_t *__restrict points;
    int rc;
    uint16_t cinfo = 0;
    max_ts = (shard->id + shard->duration) - series->mask;

    rc = new_idx = end = i = size = start = 0;

    for (   idx = series->idx;
            i < series->idx_len && idx->start_ts < max_ts;
            i++, idx++)
    {
        if (idx->shard == shard->replacing)
        {
            if (!end)
            {
                end = start = i;
            }
            size += idx->len;
            end++;

            /*
             * we have at least 2 references to the shard so we never
             * reach 0 here.  (this ref + optimize ref)
             */
            siridb_shard_decref(shard->replacing);
        }
        else if (idx->shard == shard && end)
        {
            new_idx++;
        }
    }

    if (!end)
    {
        /* no data for this series is found in the shard */
        return rc;
    }

    end += new_idx;

    size_t pos;
    uint16_t chunk_sz;
    uint_fast32_t num_chunks, pstart, pend, diff;
    siridb_shard_get_points_cb get_points_cb = \
            siridb_shard_get_points_callback(shard->replacing->flags, series);

    points = siridb_points_new(size, series->tp);
    if (points == NULL)
    {
        /* TODO: check if we can remove this ERR_ALLOC */
        ERR_ALLOC
        return -1;
    }

    for (i = start; i < end; i++)
    {
        idx = series->idx + i;
        /* we can have indexes for this 'new' shard which we should skip */
        if (idx->shard == shard->replacing && get_points_cb(
                    points,
                    idx,
                    NULL,
                    NULL,
                    series->flags & SIRIDB_SERIES_HAS_OVERLAP))
        {
            /* an error occurred while reading points, logging is done */
            size -= idx->len;
        }
    }

    num_chunks = (size - 1) / shard->max_chunk_sz + 1;
    chunk_sz = size / num_chunks + (size % num_chunks != 0);
    i = start;

    for (pstart = 0; pstart < size; pstart += chunk_sz)
    {
        pend = pstart + chunk_sz;
        if (pend > size)
        {
            pend = size;
        }

        if ((pos = siridb_shard_write_points(
                siridb,
                series,
                shard,
                points,
                pstart,
                pend,
                siri.optimize->idx_fp,
                &cinfo)) == 0)
        {
            log_critical(
                    "Cannot write points to shard id '%" PRIu64 "'",
                    shard->id);
            rc = -1;  /* signal is raised */
            num_chunks--;
        }
        else
        {
            /*
             * We should always find a spot for this index since the number
             * of chunks cannot grow.
             */
            do
            {
                idx = series->idx + i;
                i++;
            }
            while (idx->shard == shard);

            assert (idx->shard == shard->replacing);

            idx->shard = shard;
            idx->start_ts = points->data[pstart].ts;
            idx->end_ts = points->data[pend - 1].ts;
            idx->len = pend - pstart;
            idx->pos = pos;
            idx->cinfo = cinfo;
            siridb_shard_incref(shard);
        }
    }

    siridb_points_free(points);

    if (new_idx)
    {
        /*
         * We might have skipped new_indexes while writing new blocks and
         * possible some new_indexes exist at the wrong place in the index.
         *
         * Therefore we must sort the series index part containing data
         * for this shard.
         */
        SERIES_idx_sort(series->idx, start, end - 1);

        /*
         * We need to set 'i' to the correct value since 'i' has possible
         * not walked over all 'new indexes'.
         *
         * (in case new_idx is 0, i is already equal to the value set below)
         */
        i = start + new_idx + num_chunks;
    }

    if (i < end)
    {
        /* get the difference */
        diff = end - i;

        /* new length is current length minus difference */
        series->idx_len -= diff;

        for (; i < series->idx_len; i++)
        {
            series->idx[i] = series->idx[i + diff];
        }

        /* shrink memory to the new size */
        idx = (idx_t *) realloc(
                series->idx,
                series->idx_len * sizeof(idx_t));
        if (idx == NULL && series->idx_len)
        {
            /* this is not critical since the original allocated block still
             * works.
             */
            log_error("Shrinking memory for one series has failed!");
        }
        else
        {
            series->idx = idx;
        }
    }
    else
    {
        /* start must be equal to end if not smaller */
        assert (i == end);
    }

    if (series->flags & SIRIDB_SERIES_HAS_OVERLAP)
    {
        SERIES_update_overlap(series);
    }

    return rc;
}

/*
 * Open SiriDB series store file.
 *
 * Returns 0 if successful or -1 in case of an error.
 */
int siridb_series_open_store(siridb_t * siridb)
{
    /* macro get series file name */
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_SERIES_FN)

    if ((siridb->store = qp_open(fn, "a")) == NULL)
    {
        log_critical("Cannot open file '%s' for appending", fn);
        return -1;
    }
    return 0;
}

/*
 * Will sort an index to its correct order. The start of idx should be correct
 * with a valid shard. All replaced shard indexes are sorted towards the end.
 */
static void SERIES_idx_sort(
        idx_t * idx,
        uint_fast32_t start,
        uint_fast32_t end)
{
    siridb_shard_t * shard = idx[start].shard;
    idx_t * a, * b;
    uint_fast32_t i = start;
    uint_fast32_t n, m;
    idx_t tmp;

    /*
     * Since the first position is always correct we can leave this one alone.
     */
    while (++i < end)
    {
        a = idx + i;
        b = idx + i + 1;
        /*
         * We only need to swap if at least the last position is of the correct
         * shard and if the time-stamps should be swapped.
         */
        if (    b->shard == shard &&
                (a->shard != shard || a->start_ts > b->start_ts))
        {
            /*
             * Swap at least a and b but also check if we can swap a - 1 with b
             */
            n = i - start;
            m = i + 1;
            tmp = idx[m];
            do
            {
                idx[m] = idx[m - 1];
                m--;  /* we must decrement here */
            }
            while (--n && (
                    idx[m - 1].shard != tmp.shard ||
                    idx[m - 1].start_ts > tmp.start_ts));
            idx[m] = tmp;
        }
    }
}

/*
 * Updates series->flags and remove SIRIDB_SERIES_HAS_OVERLAP if possible.
 * This function never sets an overlap and therefore should not be called
 * as long as the overlap flag is not set.
 */
static void SERIES_update_overlap(siridb_series_t *__restrict series)
{
    uint_fast32_t i;

    assert (series->flags & SIRIDB_SERIES_HAS_OVERLAP);

    for (i = 1; i < series->idx_len; i++)
    {
        if (series->idx[i - 1].end_ts > series->idx[i].start_ts)
        {
            return;
        }
    }
    series->flags &= ~SIRIDB_SERIES_HAS_OVERLAP;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
static siridb_series_t * SERIES_new(
        siridb_t * siridb,
        uint32_t id,
        uint8_t tp,
        uint16_t pool,
        const char * name)
{
    uint32_t n;
    siridb_series_t * series = malloc(sizeof(siridb_series_t));
    if (series == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        series->name = strdup(name);
        if (series->name == NULL)
        {
            ERR_ALLOC
            free(series);
            series = NULL;
        }
        else
        {
            /* we use the length a lot and we have room so store this info */
            series->name_len = strlen(series->name);
            series->id = id;
            series->tp = tp;
            series->ref = 1;
            series->length = 0;
            series->start = -1;
            series->end = 0;
            series->buffer = NULL;
            series->pool = pool;
            series->flags = 0;
            series->idx_len = 0;
            series->idx = NULL;
            series->siridb = siridb;

            /* get sum series name to calculate series mask (for sharding) */
            for (n = 0; *name; name++)
            {
                n += *name;
            }

            series->mask = (tp == TP_STRING) ?
                    (uint16_t) ((n / 11) % siridb->shard_mask_log) + 600 :
                    (uint16_t) ((n / 11) % siridb->shard_mask_num);

            if ((bool) ((n / 11) % 2))
            {
                series->flags |= SIRIDB_SERIES_IS_SERVER_ONE;
            }

            /* make sure these two are exactly the same */
            assert (siridb_series_server_id(series) ==
                    siridb_series_server_id_by_name(series->name));

            if (siridb->time->precision == SIRIDB_TIME_SECONDS)
            {
                series->flags |= SIRIDB_SERIES_IS_32BIT_TS;
            }
        }
    }
    return series;
}

/*
 * Raises a SIGNAL in case or an error.
 *
 * Returns always 0 but the result will be ignored since this function is used
 * in ct_walk().
 */
static inline int SERIES_pack(siridb_series_t * series, qp_fpacker_t * fpacker)
{
    return (qp_fadd_type(fpacker, QP_ARRAY3) ||
            qp_fadd_raw(
                    fpacker,
                    (unsigned char *) series->name,
                    series->name_len + 1) ||
            qp_fadd_int64(fpacker, (int64_t) series->id) ||
            qp_fadd_int64(fpacker, (int64_t) series->tp));
}

/*
 * Returns 0 if successful or a negative integer in case of an error.
 * (SIGNAL is raised in case of an error)
 */
static int SERIES_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    log_debug("Cleanup series file");

    /* macro get series file name */
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_SERIES_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
    {
        ERR_FILE
        log_critical("Cannot open file '%s' for writing", fn);
        return EOF;
    }


    if (/* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int64(fpacker, SIRIDB_SERIES_SCHEMA))
    {
        ERR_FILE
    }
    else
    {
        if (imap_walk(siridb->series_map, (imap_cb) &SERIES_pack, fpacker))
        {
            ERR_FILE
        }
    }
    /* close file pointer */
    if (qp_close(fpacker))
    {
        ERR_FILE
    }

    return siri_err;
}

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a SIGNAL might be raised but -1 should be considered critical in any case)
 */
static int SERIES_read_dropped(siridb_t * siridb, imap_t * dropped)
{
    char * buffer;
    char * pt;
    long int size;
    int rc = 0;
    FILE * fp;

    log_debug("Loading dropped series");

    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_DROPPED_FN)

    if ((fp = fopen(fn, "r")) == NULL)
    {
        /* no drop file, we have nothing to do */
        return 0;
    }

    /* get file size */
    if (fseeko(fp, 0, SEEK_END) ||
        (size = ftello(fp)) < 0 ||
        fseeko(fp, 0, SEEK_SET))
    {
        fclose(fp);
        log_critical("Cannot read size of file '%s'", fn);
        rc = -1;
    }
    else if (size)
    {

        buffer = malloc(size);
        if (buffer == NULL)
        {
            log_critical("Cannot allocate buffer for reading dropped series");
            rc = -1;
        }
        else if (fread(buffer, size, 1, fp) == 1)
        {
            char * end = buffer + size;
            for (   pt = buffer;
                    pt < end;
                    pt += sizeof(uint32_t))
            {
                if (imap_set(
                        dropped,
                        (uint32_t) *((uint32_t *) pt),
                        (int *) DROPPED_DUMMY) == -1)
                {
                    log_critical("Cannot add id to dropped map");
                    rc = -1;
                }
            }
        }
        else
        {
            log_critical("Cannot read %ld bytes from file '%s'", size, fn);
            rc = -1;
        }
        free(buffer);
    }

    fclose(fp);

    return rc;
}

static int SERIES_keep_corrupt_series(siridb_t * siridb)
{
    int rc;
    siridb_misc_get_fn(series_fn, siridb->dbpath, SIRIDB_SERIES_FN)
    siridb_misc_get_fn(corrupt_fn, siridb->dbpath, SIRIDB_CORRUPT_FN)

    (void) unlink(corrupt_fn);
    rc = rename(series_fn, corrupt_fn);
    if (rc == 0)
    {
        log_warning("Keep previous '%s' as '%s'", series_fn, corrupt_fn);
    }
    else
    {
        log_error("Cannot rename '%s' to '%s'", series_fn, corrupt_fn);
    }
    return rc;
}

static int SERIES_load(siridb_t * siridb, imap_t * dropped)
{
    qp_unpacker_t * unpacker;
    qp_obj_t qp_series_name;
    qp_obj_t qp_series_id;
    qp_obj_t qp_series_tp;
    siridb_series_t * series;
    qp_types_t tp;
    uint32_t series_id;
    uint8_t series_tp;

    /* we should not have any series at this moment */
    assert(siridb->max_series_id == 0);

    /* get series file name */
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_SERIES_FN)

    if (!xpath_file_exist(fn))
    {
        /* missing series file, create an empty file and return  */
        return SERIES_save(siridb);
    }

    if ((unpacker = qp_unpacker_ff(fn)) == NULL)
    {
        return -1;
    }

    /* unpacker will be freed in case schema check fails */
    siridb_misc_schema_check(SIRIDB_SERIES_SCHEMA)

    while (qp_next(unpacker, NULL) == QP_ARRAY3 &&
            qp_next(unpacker, &qp_series_name) == QP_RAW &&
            qp_next(unpacker, &qp_series_id) == QP_INT64 &&
            qp_next(unpacker, &qp_series_tp) == QP_INT64)
    {
        series_id = (uint32_t) qp_series_id.via.int64;

        /* update max_series_id */
        if (series_id > siridb->max_series_id)
        {
            siridb->max_series_id = series_id;
        }

        if (imap_get(dropped, series_id) == NULL)
        {
            series_tp = (uint8_t) qp_series_tp.via.int64;
            series = SERIES_new(
                    siridb,
                    series_id,
                    series_tp,
                    siridb->server->pool,
                    (const char *) qp_series_name.via.raw);

            if (series != NULL)
            {
                /* add series to c-tree */
                int rc = ct_add(siridb->series, series->name, series);

                if (rc == CT_EXISTS)
                {
                    /* Duplicate series found */
                    siridb_series_t * other = ct_get(
                            siridb->series,
                            series->name);

                    log_error(
                            "Series '%s' with ID %"PRIu32" has a duplicate "
                            "ID %"PRIu32", "
                            "(SiriDB will keep the highest ID)",
                            series->name,
                            series->id,
                            other->id);

                    if (other->id >= series->id)
                    {
                        siridb__series_free(series);
                        continue;
                    }

                    (void) ct_pop(siridb->series, series->name);
                    (void) imap_pop(siridb->series_map, other->id);

                    siridb__series_free(other);

                    rc = ct_add(siridb->series, series->name, series);
                }

                if(rc || imap_add(siridb->series_map, series->id, series))
                {
                    log_critical("series cannot be added");
                    return -1;
                }
            }
        }
    }

    /* save last object, should be QP_END */
    tp = qp_next(unpacker, NULL);

    if (tp != QP_END)
    {
        double start = (double) (unpacker->end - unpacker->source);
        double pos = (double) (unpacker->pt - unpacker->source);

        if (pos / start < 0.8)
        {
            log_critical(
                    "Cannot read at least 80 percent of '%s', "
                    "do not continue as this leads to data loss",
                    fn);
            /* free unpacker */
            qp_unpacker_ff_free(unpacker);
            return -1;
        }

        log_error(
                "Expected end of file '%s'; "
                "Create a backup and continue", fn);

        (void) SERIES_keep_corrupt_series(siridb);
    }

    /* free unpacker */
    qp_unpacker_ff_free(unpacker);

    /*
     * In case of a siri_err we should not         return -1;
     * overwrite series because the
     * file then might be incomplete.
     */
    if (siri_err || SERIES_save(siridb))
    {
        log_critical("Cannot write series index to disk");
        return -1;  /* signal is raised */
    }

    return siri_err;
}

/*
 * Open a new SiriDB drop series file.
 *
 * Returns 0 if successful or -1 in case of an error.
 */
static int SERIES_open_new_dropped_file(siridb_t * siridb)
{
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_DROPPED_FN)

    if ((siridb->dropped_fp = fopen(fn, "w")) == NULL)
    {
        log_critical("Cannot open '%s' for writing", fn);
        return -1;
    }
    return 0;
}

/*
 * Open SiriDB drop series file.
 *
 * Returns 0 if successful or -1 in case of an error.
 */
static int SERIES_open_dropped_file(siridb_t * siridb)
{
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_DROPPED_FN)

    if ((siridb->dropped_fp = fopen(fn, "a")) == NULL)
    {
        log_critical("Cannot open '%s' for appending", fn);
        return -1;
    }
    return 0;
}

/*
 * When series are dropped, the store still has this series so when
 * SiriDB starts the next time we will include this dropped series by
 * counting the max_series_id. A second restart could be a problem if
 * not all shards are optimized because now the store does not have the
 * last removed series and therefore the max_series_id could be set to
 * a value for which shards still have data. Creating a new series and
 * another SiriDB restart before the optimize has finished could lead
 * to problems.
 *
 * Saving max_series_id at startup solves this issue because it will
 * include the dropped series.
 *
 * Returns 0 if successful or -1 in case of an error.
 */
static int SERIES_update_max_id(siridb_t * siridb)
{
    int rc = 0;
    FILE * fp;
    uint32_t max_series_id = 0;

    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_MAX_SERIES_ID_FN)

    if ((fp = fopen(fn, "r")) != NULL)
    {
        if (fread(&max_series_id, sizeof(uint32_t), 1, fp) != 1)
        {
            log_critical("Cannot read max_series_id from '%s'", fn);
            fclose(fp);
            return -1;
        }

        if (fclose(fp))
        {
            log_critical("Cannot close max_series_id file: '%s'", fn);
            return -1;
        }

        if (max_series_id > siridb->max_series_id)
        {
            siridb->max_series_id = max_series_id;
        }
    }

    /* we only need to write max_series_id in case the one in the file is
     * smaller or does not exist and max_series_id is larger than zero.
     */
    if (max_series_id < siridb->max_series_id)
    {
        if ((fp = fopen(fn, "w")) == NULL)
        {
            log_critical("Cannot open file '%s' for writing", fn);
            return -1;
        }

        log_debug("Write max series id (%" PRIu32 ")", siridb->max_series_id);

        if (fwrite(&siridb->max_series_id, sizeof(uint32_t), 1, fp) != 1)
        {
            log_critical("Cannot write max_series_id to file '%s'", fn);
            rc = -1;
        }

        if (fclose(fp))
        {
            log_critical("Cannot save max_series_id to file '%s'", fn);
            rc = -1;
        }
    }
    return rc;
}

/*
 * Update series 'start' property.
 */
static void SERIES_update_start(siridb_series_t *__restrict series)
{
    series->start = series->idx_len ? series->idx->start_ts : UINT64_MAX;

    if (series->buffer && series->buffer->len)
    {
        siridb_point_t * point = series->buffer->data;
        if (point->ts < series->start)
        {
            series->start = point->ts;
        }
    }
}

/*
 * Update series 'end' property.
 */
static void SERIES_update_end(siridb_series_t *__restrict series)
{
    if (series->idx_len)
    {
        uint64_t start = 0;
        idx_t * idx;
        uint_fast32_t i;

        for (i = series->idx_len; i--;)
        {
            idx = series->idx + i;

            if (idx->end_ts < start)
            {
                break;
            }

            start = idx->start_ts;
            if (idx->end_ts > series->end)
            {
                series->end = idx->end_ts;
            }
        }
    }
    else
    {
        series->end = 0;
    }

    if (series->buffer && series->buffer->len)
    {
        siridb_point_t * point = series->buffer->data +
                series->buffer->len - 1;
        if (point->ts > series->end)
        {
            series->end = point->ts;
        }
    }
}


