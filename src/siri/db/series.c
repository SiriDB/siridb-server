/*
 * series.c - Series
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 29-03-2016
 *
 */
#include <siri/db/series.h>
#include <stdlib.h>
#include <logger/logger.h>
#include <unistd.h>
#include <siri/db/db.h>
#include <siri/db/buffer.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <siri/db/shard.h>
#include <siri/siri.h>
#include <siri/db/condition.h>

#define SIRIDB_SERIES_FN "series.dat"
#define SIRIDB_DROPPED_FN ".dropped"
#define SIRIDB_MAX_SERIES_ID_FN ".max_series_id"
#define SIRIDB_SERIES_SCHEMA 1
#define BEND series->buffer->points->data[series->buffer->points->len - 1].ts
#define DROPPED_DUMMY 1

static int SERIES_save(siridb_t * siridb);
static void SERIES_free(siridb_series_t * series);
static int SERIES_load(siridb_t * siridb, imap32_t * dropped);
static int SERIES_read_dropped(siridb_t * siridb, imap32_t * dropped);
static int SERIES_open_new_dropped_file(siridb_t * siridb);
static int SERIES_open_store(siridb_t * siridb);
static int SERIES_update_max_id(siridb_t * siridb);
static void SERIES_update_start_num32(siridb_series_t * series);
static void SERIES_update_end_num32(siridb_series_t * series);



static int SERIES_pack(
        const char * key,
        siridb_series_t * series,
        qp_fpacker_t * fpacker);

static siridb_series_t * SERIES_new(
        siridb_t * siridb,
        uint32_t id,
        uint8_t tp,
        const char * sn);

const char series_type_map[3][8] = {
        "integer",
        "float",
        "string"
};

int siridb_series_validator(
        siridb_series_t * series,
        siridb_condition_t * condition)
{
    switch (condition->prop)
    {
    case CLERI_GID_K_LENGTH:
        return siridb_condition_int_cmp(
                condition->operator,
                series->length,
                condition->val.int64);
    }
    /* we should never get here */
    assert (0);

    return -1;
}

void siridb_series_add_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val)
{
    series->length++;
    if (*ts < series->start)
    {
        series->start = *ts;
    }
    if (*ts > series->end)
    {
        series->end = *ts;
    }
    if (series->buffer != NULL)
    {
        /* add point in memory
         * (memory can hold 1 more point than we can hold on disk)
         */
        siridb_points_add_point(series->buffer->points, ts, val);

        if (series->buffer->points->len == siridb->buffer_len)
        {
            siridb_buffer_to_shards(siridb, series);
            series->buffer->points->len = 0;
            siridb_buffer_write_len(siridb, series);
        }
        else
            siridb_buffer_write_point(siridb, series, ts, val);
    }
}

siridb_series_t * siridb_series_new(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp)
{
    siridb_series_t * series;
    size_t len = strlen(series_name);

    series = SERIES_new(
            siridb, ++siridb->max_series_id, tp, series_name);

    /* We only should add the series to series_map and assume the caller
     * takes responsibility adding the series to SiriDB -> series
     */
    imap32_add(siridb->series_map, series->id, series);

    /* create a buffer for series (except string series) */
    if (
            tp != SIRIDB_SERIES_TP_STRING &&
            siridb_buffer_new_series(siridb, series))
    {
        log_critical("Could not create buffer for series '%s'.", series_name);
    }

    if (
            qp_fadd_type(siridb->store, QP_ARRAY3) ||
            qp_fadd_raw(siridb->store, series_name, len + 1) ||
            qp_fadd_int32(siridb->store, (int32_t) series->id) ||
            qp_fadd_int8(siridb->store, (int8_t) series->tp) ||
            qp_flush(siridb->store))
    {
        log_critical("Cannot write series '%s' to store.", series_name);
    }
    return series;
}

int siridb_series_load(siridb_t * siridb)
{
    int rc;
    imap32_t * dropped;

    dropped = imap32_new();

    rc = SERIES_read_dropped(siridb, dropped);

    if (!rc)
        rc = SERIES_load(siridb, dropped);

    imap32_free(dropped);

    if (!rc)
        rc = SERIES_update_max_id(siridb);

    if (!rc)
        rc = SERIES_open_new_dropped_file(siridb);

    if (!rc)
        rc = SERIES_open_store(siridb);

    return rc;
}

void siridb_series_add_idx_num32(
        siridb_series_idx_t * index,
        siridb_shard_t * shard,
        uint32_t start_ts,
        uint32_t end_ts,
        uint32_t pos,
        uint16_t len)
{
    /* This function should only be called when new values are added.
     * For example, during optimization we do not use this function for
     * replacing indexes. This way we can also set the HAS_NEW_VALUES
     * correctly.
     */

    idx_num32_t * idx;
    uint32_t i = index->len;
    index->len++;

    index->idx = (idx_num32_t *) realloc(
            (idx_num32_t *) index->idx,
            index->len * sizeof(idx_num32_t));

    for (; i && start_ts < ((idx_num32_t *) (index->idx))[i - 1].start_ts; i--)
        ((idx_num32_t *) index->idx)[i] =
                ((idx_num32_t *) index->idx)[i - 1];

    idx = ((idx_num32_t *) (index->idx)) + i;

    /* Check here for new values since we now can compare the current
     * idx->shard with shard. We only set NEW_VALUES when we already have
     * data for this series in the shard and when not loading and not set
     * already. (NEW_VALUES is used for optimize detection and there is
     * nothing to optimize if this is the fist data for this series inside
     * the shard.
     */
    if (    ((shard->status &
                (SIRIDB_SHARD_HAS_NEW_VALUES |
                        SIRIDB_SHARD_IS_LOADING)) == 0) &&
            ((i > 0 && ((idx_num32_t *) (index->idx))[i - 1].shard == shard) ||
            (i < index->len - 1 && idx->shard == shard)))
    {
        shard->status |= SIRIDB_SHARD_HAS_NEW_VALUES;
        siridb_shard_write_status(shard);
    }

    idx->start_ts = start_ts;
    idx->end_ts = end_ts;
    idx->len = len;
    idx->shard = shard;
    idx->pos = pos;

    /* We do not have to save an overlap since it will be detected again when
     * reading the shard at startup.
     */
    if (    (i > 0 &&
            start_ts < ((idx_num32_t *) (index->idx))[i - 1].start_ts) ||
            (++i < index->len &&
            end_ts > ((idx_num32_t *) (index->idx))[i].start_ts))
    {
        shard->status |= SIRIDB_SHARD_HAS_OVERLAP;
        index->has_overlap = 1;
    }
}

void siridb_series_remove_shard_num32(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard)
{
#ifdef DEBUG
    assert (shard->id % siridb->duration_num == series->mask);
#endif

    idx_num32_t * idx;
    uint_fast32_t i, offset;

    i = offset = 0;

    for (   idx = (idx_num32_t *) series->index->idx;
            i < series->index->len;
            i++, idx++)
    {
        if (idx->shard == shard)
        {
            offset++;
            series->length -= idx->len;
        }
        else if (offset)
        {
            ((idx_num32_t *) series->index->idx)[i - offset] =
                    ((idx_num32_t *) series->index->idx)[i];
        }
    }
    if (offset)
    {
        series->index->len -= offset;
        series->index->idx = (idx_num32_t *) realloc(
                    (idx_num32_t *) series->index->idx,
                    series->index->len * sizeof(idx_num32_t));
        uint64_t start = shard->id - series->mask;
        uint64_t end = start + siridb->duration_num;
        if (series->start >= start && series->start < end)
        {
            SERIES_update_start_num32(series);
        }
        if (series->end < end && series->end > start)
        {
            SERIES_update_end_num32(series);
        }
    }
}

void siridb_series_update_props(siridb_series_t * series, void * args)
{
    SERIES_update_start_num32(series);
    SERIES_update_end_num32(series);
}

siridb_points_t * siridb_series_get_points_num32(
        siridb_series_t * series,
        uint64_t * start_ts,
        uint64_t * end_ts)
{
    idx_num32_t * idx;
    siridb_points_t * points;
    siridb_point_t * point;
    size_t len, size;
    uint_fast32_t i;
    uint32_t indexes[series->index->len];

    len = i = size = 0;

    for (   idx = (idx_num32_t *) series->index->idx;
            i < series->index->len;
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

    size += series->buffer->points->len;
    points = siridb_points_new(size, series->tp);

    for (i = 0; i < len; i++)
    {
        siridb_shard_get_points_num32(
                points,
                (idx_num32_t *) series->index->idx + indexes[i],
                start_ts,
                end_ts,
                series->index->has_overlap);
    }

    /* create pointer to buffer and get current length */
    point = series->buffer->points->data;
    len = series->buffer->points->len;

    /* crop start buffer if needed */
    if (start_ts != NULL)
        for (; len && point->ts < *start_ts; point++, len--);

    /* crop end buffer if needed */
    if (end_ts != NULL && len)
        for (   siridb_point_t * p = point + len - 1;
                len && p->ts >= *end_ts;
                p--, len--);

    /* add buffer points */
    for (; len; point++, len--)
        siridb_points_add_point(points, &point->ts, &point->val);

    if (points->len < size)
        /* shrink allocation size */
        points->data = (siridb_point_t *)
                realloc(points->data, points->len * sizeof(siridb_point_t));
#ifdef DEBUG
    else
        /* size must be equal if not smaller */
        assert (points->len == size);
#endif

    return points;
}

inline void siridb_series_incref(siridb_series_t * series)
{
    series->ref++;
}

void siridb_series_decref(siridb_series_t * series)
{
    if (!--series->ref)
    {
        SERIES_free(series);
    }
}

void siridb_series_optimize_shard_num32(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard)
{
#ifdef DEBUG
    assert (shard->id % siridb->duration_num == series->mask);
#endif

    idx_num32_t * idx;
    uint_fast32_t i, start, end, max_ts;
    size_t size;
    siridb_points_t * points;

    max_ts = (shard->id + siridb->duration_num) - series->mask;

    end = i = size = 0;

    for (   idx = (idx_num32_t *) series->index->idx;
            i < series->index->len && idx->start_ts < max_ts;
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
        }
    }

    if (!end)
    {
        /* no data for this series is found in the shard */
        return;
    }

    long int pos;
    uint16_t chunk_sz;
    uint_fast32_t num_chunks, pstart, pend;

    points = siridb_points_new(size, series->tp);

    for (i = start; i < end; i++)
    {
        siridb_shard_get_points_num32(
                points,
                (idx_num32_t *) series->index->idx + i,
                NULL,
                NULL,
                series->index->has_overlap);
    }

    num_chunks = (size - 1) / siri.cfg->max_chunk_points + 1;
    chunk_sz = size / num_chunks + (size % num_chunks != 0);

    for (pstart = 0; pstart < size; pstart += chunk_sz)
    {
        pend = pstart + chunk_sz;
        if (pend > size)
            pend = size;

        if ((pos = siridb_shard_write_points(
                siridb,
                series,
                shard,
                points,
                pstart,
                pend)) < 0)
        {
            log_critical("Cannot write points to shard id '%ld'", shard->id);
        }
        else
        {
            idx = (idx_num32_t *) series->index->idx + start;
            idx->shard = shard;
            idx->start_ts = (uint32_t) points->data[pstart].ts;
            idx->end_ts = (uint32_t) points->data[pend - 1].ts;
            idx->len = pend - pstart;
            idx->pos = pos;
        }
        start++;
    }

    siridb_points_free(points);

    if (start < end)
    {
        /* save the difference in variable i */
        i = end - start;

        /* new length is current length minus difference */
        series->index->len -= i;

        for (; start < series->index->len; start++)
        {
            ((idx_num32_t *) series->index->idx)[start] =
                    ((idx_num32_t *) series->index->idx)[start + i];
        }

        /* shrink memory to the new size */
        series->index->idx = (idx_num32_t *) realloc(
                (idx_num32_t *) series->index->idx,
                series->index->len * sizeof(idx_num32_t));
    }
#ifdef DEBUG
    else
        /* start must be equal to end if not smaller */
        assert (start == end);
#endif

}

static void SERIES_free(siridb_series_t * series)
{
//    log_debug("Free series!");
    siridb_free_buffer(series->buffer);
    free(series->index->idx);
    free(series->index);
    free(series);
}

static siridb_series_t * SERIES_new(
        siridb_t * siridb,
        uint32_t id,
        uint8_t tp,
        const char * sn)
{
    uint32_t n = 0;
    siridb_series_t * series;
    series = (siridb_series_t *) malloc(sizeof(siridb_series_t));
    series->id = id;
    series->tp = tp;
    series->ref = 1;
    series->length = 0;
    series->start = -1;
    series->end = 0;
    series->buffer = NULL;
    series->index = (siridb_series_idx_t *) malloc(sizeof(siridb_series_idx_t));
    series->index->len = 0;
    series->index->has_overlap = 0;
    series->index->idx = NULL;

    /* get sum series name to calculate series mask (for sharding) */
    for (; *sn; sn++)
        n += *sn;

    series->mask = (n / 11) % ((tp == SIRIDB_SERIES_TP_STRING) ?
            siridb->shard_mask_log : siridb->shard_mask_num);

    return series;
}

static int SERIES_pack(
        const char * key,
        siridb_series_t * series,
        qp_fpacker_t * fpacker)
{
    qp_fadd_type(fpacker, QP_ARRAY3);
    qp_fadd_raw(fpacker, key, strlen(key) + 1);
    qp_fadd_int32(fpacker, (int32_t) series->id);
    qp_fadd_int8(fpacker, (int8_t) series->tp);
    return 0;
}

static int SERIES_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    log_debug("Cleanup series file");

    /* macro get series file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERIES_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
    {
        log_critical("Cannot open file '%s' for writing", fn);
        return 1;
    }

    /* open a new array */
    qp_fadd_type(fpacker, QP_ARRAY_OPEN);

    /* write the current schema */
    qp_fadd_int8(fpacker, SIRIDB_SERIES_SCHEMA);

    ct_walk(siridb->series, (ct_cb_t) &SERIES_pack, fpacker);

    /* close file pointer */
    qp_close(fpacker);

    return 0;
}

static int SERIES_read_dropped(siridb_t * siridb, imap32_t * dropped)
{
    char * buffer;
    char * pt;
    long int size;
    int rc = 0;
    FILE * fp;

    log_debug("Read dropped series");

    SIRIDB_GET_FN(fn, SIRIDB_DROPPED_FN)

    if (access(fn, R_OK) == -1)
    {
        /* no drop file, we have nothing to do */
        return 0;
    }

    if ((fp = fopen(fn, "r")) == NULL)
    {
        log_critical("Cannot open '%s' for reading", fn);
        return -1;
    }

    /* get file size */
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size == -1)
    {
        log_critical("Cannot read size of file '%s'", fn);
        rc = -1;
    }
    else if (size)
    {

        buffer = (char *) malloc(size);

        if (fread(buffer, size, 1, fp) == 1)
        {
            char * end = buffer + size;
            for (   pt = buffer;
                    pt < end;
                    pt += sizeof(uint32_t))
            {
                imap32_add(
                        dropped,
                        (uint32_t) *((uint32_t *) pt),
                        (int *) DROPPED_DUMMY);
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

static int SERIES_load(siridb_t * siridb, imap32_t * dropped)
{
    qp_unpacker_t * unpacker;
    qp_obj_t * qp_series_name;
    qp_obj_t * qp_series_id;
    qp_obj_t * qp_series_tp;
    siridb_series_t * series;
    qp_types_t tp;
    uint32_t series_id;

    /* we should not have any users at this moment */
    assert(siridb->max_series_id == 0);

    /* get series file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERIES_FN)

    if (access(fn, R_OK) == -1)
    {
        // missing series file, create an empty file and return
        return SERIES_save(siridb);
    }

    if ((unpacker = qp_from_file_unpacker(fn)) == NULL)
        return 1;

    /* unpacker will be freed in case macro fails */
    siridb_schema_check(SIRIDB_SERIES_SCHEMA)

    qp_series_name = qp_new_object();
    qp_series_id = qp_new_object();
    qp_series_tp = qp_new_object();

    while (qp_next(unpacker, NULL) == QP_ARRAY3 &&
            qp_next(unpacker, qp_series_name) == QP_RAW &&
            qp_next(unpacker, qp_series_id) == QP_INT64 &&
            qp_next(unpacker, qp_series_tp) == QP_INT64)
    {
        series_id = (uint32_t) qp_series_id->via->int64;

        /* update max_series_id */
        if (series_id > siridb->max_series_id)
            siridb->max_series_id = series_id;

        if (imap32_get(dropped, series_id) == NULL)
        {
            series = SERIES_new(
                    siridb,
                    series_id,
                    (uint8_t) qp_series_tp->via->int64,
                    qp_series_name->via->raw);

            /* add series to c-tree */
            ct_add(siridb->series, qp_series_name->via->raw, series);

            /* add series to imap32 */
            imap32_add(siridb->series_map, series->id, series);
        }
    }

    /* save last object, should be QP_END */
    tp = qp_next(unpacker, NULL);

    /* free objects */
    qp_free_object(qp_series_name);
    qp_free_object(qp_series_id);
    qp_free_object(qp_series_tp);

    /* free unpacker */
    qp_free_unpacker(unpacker);

    if (tp != QP_END)
    {
        log_critical("Expected end of file '%s'", fn);
        return 1;
    }

    if (SERIES_save(siridb))
    {
        log_critical("Cannot write series index to disk");
        return -1;
    }

    return 0;
}

static int SERIES_open_new_dropped_file(siridb_t * siridb)
{
    SIRIDB_GET_FN(fn, SIRIDB_DROPPED_FN)

    if ((siridb->dropped_fp = fopen(fn, "w")) == NULL)
    {
        log_critical("Cannot open '%s' for writing", fn);
        return 1;
    }

    return 0;
}

static int SERIES_open_store(siridb_t * siridb)
{
    /* macro get series file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERIES_FN)

    if ((siridb->store = qp_open(fn, "a")) == NULL)
    {
        log_critical("Cannot open file '%s' for appending", fn);
        return -1;
    }

    return 0;
}

static int SERIES_update_max_id(siridb_t * siridb)
{
    /* When series are dropped, the store still has this series so when
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
     */
    int rc = 0;
    FILE * fp;
    uint32_t max_series_id = 0;

    SIRIDB_GET_FN(fn, SIRIDB_MAX_SERIES_ID_FN)

    if (access(fn, R_OK) == 0)
    {
        if ((fp = fopen(fn, "r")) == NULL)
        {
            log_critical("Cannot open file '%s' for reading", fn);
            return -1;
        }

        if (fread(&max_series_id, sizeof(uint32_t), 1, fp) != 1)
        {
            log_critical("Cannot read max_series_id from '%s'", fn);
            fclose(fp);
            return -1;
        }

        if (max_series_id > siridb->max_series_id)
        {
            siridb->max_series_id = max_series_id;
        }

        fclose(fp);
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

        log_debug("Write max series id (%ld)", siridb->max_series_id);

        if (fwrite(&siridb->max_series_id, sizeof(uint32_t), 1, fp) != 1)
        {
            log_critical("Cannot write max_series_id to file '%s'", fn);
            rc = -1;
        }

        fclose(fp);
    }

    return rc;
}

static void SERIES_update_start_num32(siridb_series_t * series)
{
    series->start = (series->index->len) ?
            ((idx_num32_t *) series->index->idx)->start_ts : -1;

    if (series->buffer->points->len)
    {
        siridb_point_t * point = series->buffer->points->data;
        if (point->ts < series->start)
        {
            series->start = point->ts;
        }
    }
}

static void SERIES_update_end_num32(siridb_series_t * series)
{
    if (series->index->len)
    {
        uint32_t start = 0;
        idx_num32_t * idx;
        for (uint_fast32_t i = series->index->len; i--;)
        {
            idx = (idx_num32_t *) series->index->idx + i;

            if (idx->end_ts < start)
                break;

            start = idx->start_ts;
            if (idx->end_ts > series->end)
                series->end = idx->end_ts;
        }
    }
    else
    {
        series->end = 0;
    }
    if (series->buffer->points->len)
    {
        siridb_point_t * point = series->buffer->points->data +
                series->buffer->points->len - 1;
        if (point->ts > series->end)
        {
            series->end = point->ts;
        }
    }
}


