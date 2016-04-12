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
#define SIRIDB_SERIES_SCHEMA 1

static int save_series(siridb_t * siridb);

static siridb_series_t * siridb_new_series(
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

void siridb_series_add_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val)
{
    log_debug("Add point to series...");
    if (series->buffer != NULL)
    {
        /* add point in memory
         * (memory can hold 1 more point than we can hold on disk)
         */
        siridb_points_add_point(series->buffer->points, ts, val);

        if (series->buffer->points->len == series->buffer->points->size)
        {
            log_debug("Buffer is full, write to shards");
            siridb_buffer_to_shards(siridb, series);
            series->buffer->points->len = 0;
            siridb_buffer_write_len(siridb, series);
        }
        else
            siridb_buffer_write_point(siridb, series, ts, val);
    }
}

siridb_series_t * siridb_create_series(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp)
{
    siridb_series_t * series;
    qp_fpacker_t * fpacker;
    size_t len = strlen(series_name);

    series = siridb_new_series(
            siridb, ++siridb->max_series_id, tp, series_name);

    /* We only should add the series to series_map and assume the caller
     * takes responsibility adding the series to SiriDB -> series
     */
    imap32_add(siridb->series_map, series->id, series);

    siridb_get_fn(fn, SIRIDB_SERIES_FN);

    /* create a buffer for series (except string series) */
    if (tp != SIRIDB_SERIES_TP_STRING &&
            siridb_buffer_new_series(siridb, series))
        log_critical("Could not create buffer for series '%s'.", series_name);

    if ((fpacker = qp_open(fn, "a")) == NULL)
    {
        log_critical("Could not write series '%s' to disk.", series_name);
        return series;
    }

    qp_fadd_type(fpacker, QP_ARRAY3);
    qp_fadd_raw(fpacker, series_name, len + 1);
    qp_fadd_int32(fpacker, (int32_t) series->id);
    qp_fadd_int8(fpacker, (int8_t) series->tp);

    /* close file packer */
    qp_close(fpacker);

    return series;
}

void siridb_free_series(siridb_series_t * series)
{
    siridb_free_buffer(series->buffer);
    free(series->index->idx);
    free(series->index);
    free(series);
}

int siridb_load_series(siridb_t * siridb)
{
    qp_unpacker_t * unpacker;
    qp_obj_t * qp_series_name;
    qp_obj_t * qp_series_id;
    qp_obj_t * qp_series_tp;
    siridb_series_t * series;
    qp_types_t tp;

    /* we should not have any users at this moment */
    assert(siridb->max_series_id == 0);

    /* get series file name */
    siridb_get_fn(fn, SIRIDB_SERIES_FN);

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
        series = siridb_new_series(
                siridb,
                (uint32_t) qp_series_id->via->int64,
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

    return 0;
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
    siridb_get_fn(fn, SIRIDB_SERIES_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
        return 1;

    /* open a new array */
    qp_fadd_type(fpacker, QP_ARRAY_OPEN);

    /* write the current schema */
    qp_fadd_int8(fpacker, SIRIDB_SERIES_SCHEMA);

    ct_walk(siridb->series, (ct_cb_t) &pack_series, fpacker);

    /* close file pointer */
    qp_close(fpacker);

    return 0;
}

void siridb_add_idx_num32(
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
