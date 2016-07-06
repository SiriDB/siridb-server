/*
 * insert.c - Handler database inserts.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 24-03-2016
 *
 */
#include <siri/db/insert.h>
#include <qpack/qpack.h>
#include <logger/logger.h>
#include <stdio.h>
#include <string.h>
#include <siri/siri.h>
#include <siri/net/protocol.h>
#include <siri/db/series.h>
#include <siri/db/points.h>
#include <siri/net/socket.h>
#include <assert.h>

static void INSERT_free(uv_handle_t * handle);
static void INSERT_points_to_pools(uv_async_t * handle);
static int INSERT_points_to_pool(siridb_insert_t * insert, qp_packer_t * packer);

static ssize_t INSERT_assign_by_map(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[],
        qp_obj_t * qp_obj);

static ssize_t INSERT_assign_by_array(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[],
        qp_obj_t * qp_obj,
        qp_packer_t * tmp_packer);

static int INSERT_read_points(
        siridb_t * siridb,
        qp_packer_t * packer,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj,
        int32_t * count);


/*
 * Return an error message for an insert err.
 */
const char * siridb_insert_err_msg(siridb_insert_err_t err)
{
    switch (err)
    {
    case ERR_EXPECTING_ARRAY:
        return  "Expecting an array with points.";
    case ERR_EXPECTING_SERIES_NAME:
        return  "Expecting a series name (string value) with an array of "
                "points where each point should be an integer time-stamp "
                "with a value.";
    case ERR_EXPECTING_MAP_OR_ARRAY:
        return   "Expecting an array or map containing series and points.";
    case ERR_EXPECTING_INTEGER_TS:
        return  "Expecting an integer value as time-stamp.";
    case ERR_TIMESTAMP_OUT_OF_RANGE:
        return  "Received at least one time-stamp which is out-of-range.";
    case ERR_UNSUPPORTED_VALUE:
        return  "Unsupported value received. (only integer, string and float "
                "values are supported).";
    case ERR_EXPECTING_AT_LEAST_ONE_POINT:
        return  "Expecting a series to have at least one point.";
    case ERR_EXPECTING_NAME_AND_POINTS:
        return  "Expecting a map with name and points.";
    case ERR_MEM_ALLOC:
        return  "Critical memory allocation error";
    default:
        assert (0);
        break;
    }
    return "Unknown err";
}

/*
 * Returns a negative value in case of an error or a value equal to zero or
 * higher representing the number of points processed.
 *
 * This function can set a SIGNAL when not enough space in the packer can be
 * allocated for the points and ERR_MEM_ALLOC will be the return value if this
 * is the case.
 */
ssize_t siridb_insert_assign_pools(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj,
        qp_packer_t * packer[])
{
    ssize_t rc;
    qp_types_t tp;

    /*
     * Allocate packers for sending data to pools. we allocate smaller
     * sizes in case we have a lot of pools.
     */
    uint32_t psize = QP_SUGGESTED_SIZE / ((siridb->pools->len / 4) + 1);

    for (size_t n = 0; n < siridb->pools->len; n++)
    {
        /* These packers will be freed either in clserver in case of an error,
         * or by siridb_free_insert(..) in case of success.
         */
        if ((packer[n] = qp_packer_new(psize)) == NULL)
        {
            return ERR_MEM_ALLOC;  /* a signal is raised */
        }

        /* cannot raise a signal since enough space is allocated */
        qp_add_type(packer[n], QP_MAP_OPEN);
    }

    tp = qp_next(unpacker, NULL);

    if (qp_is_map(tp))
    {
        rc = INSERT_assign_by_map(siridb, unpacker, packer, qp_obj);
    }
    else if (qp_is_array(tp))
    {
        qp_packer_t * tmp_packer = qp_packer_new(QP_SUGGESTED_SIZE);
        if (tmp_packer != NULL)
        {
            rc = INSERT_assign_by_array(siridb, unpacker, packer, qp_obj, tmp_packer);
            qp_packer_free(tmp_packer);
        }
    }
    else
    {
        rc = ERR_EXPECTING_MAP_OR_ARRAY;
    }
    return (siri_err) ? ERR_MEM_ALLOC : rc;
}

/*
 * Prepare a new insert object an start an async call to 'INSERT_points_to_pools'.
 *
 * In case of an error a SIGNAL is raised and 'INSERT_points_to_pools' will not
 * be called.
 */
void siridb_insert_points(
        uint64_t pid,
        uv_handle_t * client,
        size_t size,
        uint16_t packer_size,
        qp_packer_t * packer[])
{
    uv_async_t * handle = (uv_async_t *) malloc(sizeof(uv_async_t));
    if (handle == NULL)
    {
        ERR_ALLOC
        return;
    }

    siridb_insert_t * insert = (siridb_insert_t *) malloc(
            sizeof(siridb_insert_t) + packer_size * sizeof(qp_packer_t *));
    if (insert == NULL)
    {
        ERR_ALLOC
        free(handle);
        return;
    }

    insert->free_cb = INSERT_free;
    insert->pid = pid;
    insert->client = client;
    insert->size = size;
    insert->response = NULL;

    /*
     * we keep the packer size because the number of pools might change and
     * at this point the pool->len is equal to when the insert was received
     */
    insert->packer_size = packer_size;
    memcpy(insert->packer, packer, sizeof(qp_packer_t *) * packer_size);

    uv_async_init(siri.loop, handle, INSERT_points_to_pools);
    handle->data = (void *) insert;
    uv_async_send(handle);
}

/*
 * Returns 0 if successful and no siri_err is raised or a negative value in
 * case of errors.
 *
 * Note that insert->packer[] will be ignored, instead a pointer the correct
 * packer should be parsed to this function.
 *
 * (a SIGNAL will be raised in case of an error)
 */
static int INSERT_points_to_pool(siridb_insert_t * insert, qp_packer_t * packer)
{
    qp_types_t tp;
    siridb_series_t ** series;
    siridb_t * siridb = ((sirinet_socket_t *) insert->client->data)->siridb;
    qp_obj_t * qp_series_name = qp_object_new();
    qp_obj_t * qp_series_ts = qp_object_new();
    qp_obj_t * qp_series_val = qp_object_new();
    if (qp_series_name == NULL || qp_series_ts == NULL || qp_series_val == NULL)
    {
        ERR_ALLOC
        qp_object_free_safe(qp_series_name);
        qp_object_free_safe(qp_series_ts);
        qp_object_free_safe(qp_series_val);
        return -1;
    }

    uv_mutex_lock(&siridb->series_mutex);
    uv_mutex_lock(&siridb->shards_mutex);

    qp_unpacker_t * unpacker = qp_unpacker_new(packer->buffer, packer->len);
    if (unpacker != NULL)
    {
        qp_next(unpacker, NULL); // map
        tp = qp_next(unpacker, qp_series_name); // first series or end

        /*
         * we check for siri_err because siridb_series_add_point()
         * should never be called twice on the same series after an
         * error has occurred.
         */
        while (!siri_err && tp == QP_RAW)
        {
            series = (siridb_series_t **) ct_get_sure(
                    siridb->series,
                    qp_series_name->via->raw);
            if (series == NULL)
            {
                log_critical(
                        "Error getting or create series: '%s'",
                        qp_series_name->via->raw);
                break;  /* signal is raised */
            }

            qp_next(unpacker, NULL); // array open
            qp_next(unpacker, NULL); // first point array2
            qp_next(unpacker, qp_series_ts); // first ts
            qp_next(unpacker, qp_series_val); // first val

            if (ct_is_empty(*series))
            {
                *series = siridb_series_new(
                        siridb,
                        qp_series_name->via->raw,
                        SIRIDB_QP_MAP2_TP(qp_series_val->tp));
                if (*series == NULL)
                {
                    log_critical(
                            "Error creating series: '%s'",
                            qp_series_name->via->raw);
                    break;  /* signal is raised */
                }
            }

            if (siridb_series_add_point(
                    siridb,
                    *series,
                    (uint64_t *) &qp_series_ts->via->int64,
                    qp_series_val->via))
            {
                break;  /* signal is raised */
            }

            while ((tp = qp_next(unpacker, qp_series_name)) == QP_ARRAY2)
            {
                qp_next(unpacker, qp_series_ts); // ts
                qp_next(unpacker, qp_series_val); // val
                if (siridb_series_add_point(
                        siridb,
                        *series,
                        (uint64_t *) &qp_series_ts->via->int64,
                        qp_series_val->via))
                {
                    break;  /* signal is raised */
                }
            }
            if (tp == QP_ARRAY_CLOSE)
            {
                tp = qp_next(unpacker, qp_series_name);
            }
        }
        qp_unpacker_free(unpacker);
    }

    uv_mutex_unlock(&siridb->series_mutex);
    uv_mutex_unlock(&siridb->shards_mutex);

    qp_object_free(qp_series_name);
    qp_object_free(qp_series_ts);
    qp_object_free(qp_series_val);

    return siri_err;
}

/*
 * In case of an error a SIGNAL is raised and a successful message will not
 * be send to the client.
 */
static void INSERT_points_to_pools(uv_async_t * handle)
{
    siridb_insert_t * insert = (siridb_insert_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) insert->client->data)->siridb;
    uint16_t pool = siridb->server->pool;

    for (uint16_t n = 0; n < insert->packer_size; n++)
    {
        if (insert->packer[n]->len == 1)
        {
            /* skip empty packer. (empty packer has only QP_MAP_OPEN) */
            continue;
        }
        if (n == pool)
        {
            INSERT_points_to_pool(insert, insert->packer[n]);
        }
        else
        {
            siridb_server_send_pkg(
                    server,
                    len,
                    tp,
                    content,
                    timeout,
                    sirinet_promise_on_response,
                    promises);
        }
    }

    insert->response = qp_packer_new(64);

    if (insert->response != NULL)
    {
        /* this will fit for sure */
        qp_add_type(insert->response, QP_MAP_OPEN);
        if (siri_err)
        {
            qp_add_raw(insert->response, "error_msg", 9);
            qp_add_fmt(
                    insert->response,
                    "Error inserting %zd point(s).",
                    insert->size);

            log_error("Error inserting %zd point(s).", insert->size);
        }
        else
        {
            qp_add_raw(insert->response, "success_msg", 11);
            qp_add_fmt(
                    insert->response,
                    "Inserted %zd point(s) successfully.",
                    insert->size);

            log_info("Inserted %zd point(s) successfully.", insert->size);

            siridb->received_points += insert->size;
        }

        sirinet_pkg_t * pkg = sirinet_pkg_new(
                insert->pid,
                insert->response->len,
                SN_MSG_RESULT,
                insert->response->buffer);

        if (pkg != NULL)
        {
            /* ignore result code, signal can be raised */
            sirinet_pkg_send((uv_stream_t *) insert->client, pkg);

            /* free package */
            free(pkg);
        }
    }

    uv_close((uv_handle_t *) handle, insert->free_cb);
}

/*
 * Returns a negative value in case of an error or a value equal to zero or
 * higher representing the number of points processed.
 *
 * This function can set a SIGNAL when not enough space in the packer can be
 * allocated for the points and should be checked with 'siri_err'.
 */
static ssize_t INSERT_assign_by_map(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[],
        qp_obj_t * qp_obj)
{
    qp_types_t tp;
    uint16_t pool;
    int32_t count = 0;

    tp = qp_next(unpacker, qp_obj);

    while (tp == QP_RAW)
    {
        pool = siridb_pool_sn_raw(
                siridb,
                qp_obj->via->raw,
                qp_obj->len);

        qp_add_raw_term(packer[pool],
                qp_obj->via->raw,
                qp_obj->len);

        if ((tp = INSERT_read_points(
                siridb,
                packer[pool],
                unpacker,
                qp_obj,
                &count)) < 0)
        {
            return tp;
        }
    }

    if (tp != QP_END && tp != QP_MAP_CLOSE)
    {
        return ERR_EXPECTING_SERIES_NAME;
    }

    return count;
}

/*
 * Returns a negative value in case of an error or a value equal to zero or
 * higher representing the number of points processed.
 *
 * This function can set a SIGNAL when not enough space in the packer can be
 * allocated for the points and should be checked with 'siri_err'.
 */
static ssize_t INSERT_assign_by_array(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[],
        qp_obj_t * qp_obj,
        qp_packer_t * tmp_packer)
{
    qp_types_t tp;
    uint16_t pool;
    int32_t count = 0;
    tp = qp_next(unpacker, qp_obj);

    while (tp == QP_MAP2)
    {
        if (qp_next(unpacker, qp_obj) != QP_RAW)
        {
            return ERR_EXPECTING_NAME_AND_POINTS;
        }

        if (strncmp(qp_obj->via->raw, "points", qp_obj->len) == 0)
        {
            if ((tp = INSERT_read_points(
                    siridb,
                    tmp_packer,
                    unpacker,
                    qp_obj,
                    &count)) < 0 || tp != QP_RAW)
            {
                return (tp < 0) ? tp : ERR_EXPECTING_NAME_AND_POINTS;
            }
        }

        if (strncmp(qp_obj->via->raw, "name", qp_obj->len) == 0)
        {
            if (qp_next(unpacker, qp_obj) != QP_RAW)
            {
                return ERR_EXPECTING_NAME_AND_POINTS;
            }

            pool = siridb_pool_sn_raw(
                    siridb,
                    qp_obj->via->raw,
                    qp_obj->len);

            qp_add_raw_term(packer[pool],
                    qp_obj->via->raw,
                    qp_obj->len);
        }
        else
        {
            return ERR_EXPECTING_NAME_AND_POINTS;
        }

        if (tmp_packer->len)
        {
            qp_packer_extend(packer[pool], tmp_packer);
            tmp_packer->len = 0;
            tp = qp_next(unpacker, qp_obj);
        }
        else
        {
            if (qp_next(unpacker, qp_obj) != QP_RAW ||
                    strncmp(qp_obj->via->raw, "points", qp_obj->len))
            {
                return ERR_EXPECTING_NAME_AND_POINTS;
            }

            if ((tp = INSERT_read_points(
                    siridb,
                    packer[pool],
                    unpacker,
                    qp_obj,
                    &count)) < 0)
            {
                return tp;
            }
        }
    }

    if (tp != QP_END && tp != QP_ARRAY_CLOSE)
    {
        return ERR_EXPECTING_SERIES_NAME;
    }

    return count;
}

/*
 * Returns a negative value in case of an error or a value equal to zero or
 * higher representing the next qpack type in the unpaker.
 *
 * This function can set a SIGNAL when not enough space in the packer can be
 * allocated for the points.
 */
static int INSERT_read_points(
        siridb_t * siridb,
        qp_packer_t * packer,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj,
        int32_t * count)
{
    qp_types_t tp;

    if (!qp_is_array(qp_next(unpacker, NULL)))
    {
        return ERR_EXPECTING_ARRAY;
    }

    qp_add_type(packer, QP_ARRAY_OPEN);

    if ((tp = qp_next(unpacker, NULL)) != QP_ARRAY2)
    {
        return ERR_EXPECTING_AT_LEAST_ONE_POINT;
    }

    for (; tp == QP_ARRAY2; (*count)++, tp = qp_next(unpacker, qp_obj))
    {
        qp_add_type(packer, QP_ARRAY2);

        if (qp_next(unpacker, qp_obj) != QP_INT64)
        {
            return ERR_EXPECTING_INTEGER_TS;
        }

        if (!siridb_int64_valid_ts(siridb, qp_obj->via->int64))
        {
            return ERR_TIMESTAMP_OUT_OF_RANGE;
        }

        qp_add_int64(packer, qp_obj->via->int64);

        switch (qp_next(unpacker, qp_obj))
        {
        case QP_RAW:
            qp_add_raw(packer,
                    qp_obj->via->raw,
                    qp_obj->len);
            break;

        case QP_INT64:
            qp_add_int64(packer,
                    qp_obj->via->int64);
            break;

        case QP_DOUBLE:
            qp_add_double(packer,
                    qp_obj->via->real);
            break;

        default:
            return ERR_UNSUPPORTED_VALUE;
        }
    }

    if (tp == QP_ARRAY_CLOSE)
    {
        tp = qp_next(unpacker, qp_obj);
    }

    qp_add_type(packer, QP_ARRAY_CLOSE);

    return tp;
}

/*
 * Used as uv_close_cb.
 */
static void INSERT_free(uv_handle_t * handle)
{
    siridb_insert_t * insert = (siridb_insert_t *) handle->data;

    /* free packer */
    for (size_t n = 0; n < insert->packer_size; n++)
    {
        qp_packer_free(insert->packer[n]);
    }

    if (insert->response != NULL)
    {
        qp_packer_free(insert->response);
    }

    /* free insert */
    free(insert);

    /* free handle */
    free((uv_async_t *) handle);

    #ifdef DEBUG
    log_debug("Free insert!, hooray!");
    #endif
}
