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
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define SIRIDB_SERIES_FN "series.dat"
#define SIRIDB_SERIES_SCHEMA 1


siridb_series_t * siridb_new_series(uint32_t id, uint8_t tp)
{
    siridb_series_t * series;
    series = (siridb_series_t *) malloc(sizeof(siridb_series_t));
    series->id = id;
    series->tp = tp;
    return series;
}

void siridb_free_series(siridb_series_t * series)
{
    log_debug("Free series ID : %d", series->id);
    free(series);
}

int siridb_read_series(siridb_t * siridb)
{
    qp_unpacker_t * unpacker;
    qp_obj_t * qp_series_name = NULL;
    qp_obj_t * qp_series_id = NULL;
    qp_obj_t * qp_series_tp = NULL;
    siridb_series_t * series;
    qp_types_t tp;

    /* we should not have any users at this moment */
    assert(siridb->max_series_id == 0);

    /* get series file name */
    char fn[strlen(siridb->dbpath) + strlen(SIRIDB_SERIES_FN) + 1];
    sprintf(fn, "%s%s", siridb->dbpath, SIRIDB_SERIES_FN);

    if (access(fn, R_OK) == -1)
    {
        // missing series file, create an empty file and return
        return save_series(siridb);
    }

    if ((unpacker = qp_from_file_unpacker(fn)) == NULL)
        return 1;

    if (!qp_is_array(qp_next_object(unpacker)) ||
            qp_next_object(unpacker) != QP_INT64 ||
            unpacker->qp_obj->via->int64 != SIRIDB_SERIES_SCHEMA)
    {
        log_critical("Invalid schema detected in '%s'", fn);
        qp_free_unpacker(unpacker);
        return 1;
    }

    while (qp_next_object(unpacker) == QP_ARRAY3 &&
            qp_copy_next_object(unpacker, &qp_series_name) == QP_RAW &&
            qp_copy_next_object(unpacker, &qp_series_id) == QP_INT64 &&
            qp_copy_next_object(unpacker, &qp_series_tp) == QP_INT64)
    {
        series = siridb_new_series(
                (uint32_t) qp_series_id->via->int64,
                (uint8_t) qp_series_tp->via->int64);
        ct_add(siridb->series, qp_series_name->via->raw, series);
        imap32_add(siridb->series_map, series->id, series);
    }

    /* save last object, should be QP_END */
    tp = qp_next_object(unpacker);

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
        qp_packer_t * packer)
{
    log_debug("Series '%s' with ID %d", key, series->id);
    qp_add_array3(packer);
    qp_add_string_term(packer, key);
    qp_add_int32(packer, (int32_t) series->id);
    qp_add_int8(packer, (int8_t) series->tp);
}


static int save_series(siridb_t * siridb)
{
    FILE * fp;
    qp_packer_t * packer;

    /* get series file name */
    char fn[strlen(siridb->dbpath) + strlen(SIRIDB_SERIES_FN) + 1];
    sprintf(fn, "%s%s", siridb->dbpath, SIRIDB_SERIES_FN);

    if ((fp = fopen(fn, "w")) == NULL)
        return 1;

    packer = qp_new_packer(8192);

    /* open a new array */
    qp_array_open(packer);

    /* write the current schema */
    qp_add_int8(packer, SIRIDB_SERIES_SCHEMA);

    ct_walk(siridb->series, &pack_series, packer);

    /* write output to file */
    fwrite(packer->buffer, packer->len, 1, fp);

    /* close file pointer */
    fclose(fp);

    /* free packer */
    qp_free_packer(packer);

    return 0;
}
