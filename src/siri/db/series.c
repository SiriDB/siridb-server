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

#define SIRIDB_SERIES_FN "series.dat"
#define SIRIDB_SERIES_SCHEMA 1

static int save_series(siridb_t * siridb);

siridb_series_t * siridb_new_series(uint32_t id, uint8_t tp)
{
    siridb_series_t * series;
    series = (siridb_series_t *) malloc(sizeof(siridb_series_t));
    series->id = id;
    series->tp = tp;
    return series;
}

siridb_series_t * siridb_create_series(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp)
{
    siridb_series_t * series;
    qp_fpacker_t * fpacker;
    size_t len = strlen(series_name);

    series = siridb_new_series(++siridb->max_series_id, tp);

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

uint8_t siridb_qp_map_tp(qp_types_t tp)
{
    switch (tp)
    {
    case QP_INT64:
        return SIRIDB_SERIES_TP_INT;
    case QP_DOUBLE:
        return SIRIDB_SERIES_TP_DOUBLE;
    case QP_RAW:
        return SIRIDB_SERIES_TP_STRING;
    default:
        log_critical("No map found for %d", tp);
        return 0;
    }
}

void siridb_free_series(siridb_series_t * series)
{
    log_debug("Free series ID : %d", series->id);
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
                (uint32_t) qp_series_id->via->int64,
                (uint8_t) qp_series_tp->via->int64);

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
