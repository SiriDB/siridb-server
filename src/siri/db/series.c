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

#define SIRIDB_SERIES_FN "series.dat"
#define SIRIDB_DROPPED_FN ".dropped.dat"
#define SIRIDB_SERIES_SCHEMA 1
#define BEND series->buffer->points->data[series->buffer->points->len - 1].ts
#define DROPPED_DUMMY 1

static int save_series(siridb_t * siridb);
static void series_free(siridb_series_t * series);
static int load_series(siridb_t * siridb, imap32_t * dropped);
static int read_dropped(siridb_t * siridb, imap32_t * dropped);
static int open_store(siridb_t * siridb);

static void pack_series(
        const char * key,
        siridb_series_t * series,
        qp_fpacker_t * fpacker);

static siridb_series_t * new_series(
        siridb_t * siridb,
        uint32_t id,
        uint8_t tp,
        const char * sn);


void siridb_series_add_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val)
{
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

static int open_new_dropped_file(siridb_t * siridb)
{
    SIRIDB_GET_FN(fn, SIRIDB_DROPPED_FN)

    if ((siridb->dropped_fp = fopen(fn, "w")) == NULL)
    {
        log_critical("Cannot open '%s' for writing", fn);
        return 1;
    }

    return 0;
}

siridb_series_t * siridb_series_new(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp)
{
    siridb_series_t * series;
    size_t len = strlen(series_name);

    series = new_series(
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
            qp_fadd_int8(siridb->store, (int8_t) series->tp))
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

    rc = read_dropped(siridb, dropped);

    if (!rc)
        rc = load_series(siridb, dropped);

    imap32_free(dropped);

    if (!rc)
        rc = open_new_dropped_file(siridb);

    if (!rc)
        rc = open_store(siridb);

    return rc;
}

void siridb_series_add_idx_num32(
        siridb_series_idx_t * index,
        struct siridb_shard_s * shard,
        uint32_t start_ts,
        uint32_t end_ts,
        uint32_t pos,
        uint16_t len)
{
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

    idx->start_ts = start_ts;
    idx->end_ts = end_ts;
    idx->len = len;
    idx->shard = shard;
    idx->pos = pos;

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
    if (shard->id % siridb->duration_num != series->mask)
        return;

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
    }
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
    points = siridb_new_points(size, series->tp);

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
        series_free(series);
    }
}

static void series_free(siridb_series_t * series)
{
    log_debug("Free series!");
    siridb_free_buffer(series->buffer);
    free(series->index->idx);
    free(series->index);
    free(series);
}

static siridb_series_t * new_series(
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

static void pack_series(
        const char * key,
        siridb_series_t * series,
        qp_fpacker_t * fpacker)
{
    qp_fadd_type(fpacker, QP_ARRAY3);
    qp_fadd_raw(fpacker, key, strlen(key) + 1);
    qp_fadd_int32(fpacker, (int32_t) series->id);
    qp_fadd_int8(fpacker, (int8_t) series->tp);
}

static int save_series(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

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

    ct_walk(siridb->series, (ct_cb_t) &pack_series, fpacker);

    /* close file pointer */
    qp_close(fpacker);

    return 0;
}

static int read_dropped(siridb_t * siridb, imap32_t * dropped)
{
    char * buffer;
    char * pt;
    long int size;
    int rc = 0;
    FILE * fp;

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

static int load_series(siridb_t * siridb, imap32_t * dropped)
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
        return save_series(siridb);
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
        if (imap32_get(dropped, series_id) == NULL)
        {
            series = new_series(
                    siridb,
                    series_id,
                    (uint8_t) qp_series_tp->via->int64,
                    qp_series_name->via->raw);

            /* update max_series_id */
            if (series->id > siridb->max_series_id)
                siridb->max_series_id = series->id;

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

    if (save_series(siridb))
    {
        log_critical("Cannot write series index to disk");
        return -1;
    }

    return 0;
}

static int open_store(siridb_t * siridb)
{
    /* macro get series file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERIES_FN)

    if ((siridb->store = qp_open(fn, "a")) == NULL)
    {
        log_critical("Cannot open file '%s' for appending", fn);
        return 1;
    }

    return 0;
}

