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
#include <assert.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/async.h>
#include <siri/db/forward.h>
#include <siri/db/insert.h>
#include <siri/db/points.h>
#include <siri/db/replicate.h>
#include <siri/db/series.h>
#include <siri/err.h>
#include <siri/net/promises.h>
#include <siri/net/protocol.h>
#include <siri/net/socket.h>
#include <siri/siri.h>
#include <stdio.h>
#include <string.h>

#define MAX_INSERT_MSG 236
#define INSERT_TIMEOUT 300000  // 5 minutes
#define INSERT_AT_ONCE 3000    // one point counts as 1, a series as 100
#define WEIGHT_SERIES 50
#define WEIGHT_NEW_SERIES 100

#define SERIES_UPDATE_TS(series)    \
if (*ts < series->start)            \
{                                   \
    series->start = *ts;            \
}                                   \
if (*ts > series->end)              \
{                                   \
    series->end = *ts;              \
}

static void INSERT_free(uv_handle_t * handle);
static void INSERT_points_to_pools(uv_async_t * handle);
static void INSERT_on_response(slist_t * promises, uv_async_t * handle);
static uint16_t INSERT_get_pool(siridb_t * siridb, qp_obj_t * qp_series_name);

static void INSERT_local_free_cb(uv_async_t * handle);
static int8_t INSERT_local_work(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_series_name,
        siridb_pcache_t ** pcache);
static int INSERT_local_work_test(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_series_name,
        siridb_pcache_t ** pcache,
        siridb_forward_t ** forward);
static void INSERT_local_task(uv_async_t * handle);
static void INSERT_local_promise_cb(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);
static void INSERT_local_promise_backend_cb(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);
static int INSERT_init_local(
        siridb_t * siridb,
        sirinet_promises_t * promises,
        sirinet_pkg_t * pkg,
        uint8_t flags);

static ssize_t INSERT_assign_by_map(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[]);

static ssize_t INSERT_assign_by_array(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[],
        qp_packer_t * tmp_packer);

static int INSERT_read_points(
        siridb_t * siridb,
        qp_packer_t * packer,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj,
        ssize_t * count);



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
        return  "Unsupported value received. (only integer and float "
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
 * Destroy insert.
 */
void siridb_insert_free(siridb_insert_t * insert)
{
    /* free packer */
    for (size_t n = 0; n < insert->packer_size; n++)
    {
        if (insert->packer[n] != NULL)
        {
            qp_packer_free(insert->packer[n]);
        }
    }

    /* free insert */
    free(insert);

#ifdef DEBUG
    log_debug("Free insert!, hooray!");
#endif
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
        qp_packer_t * packer[])
{
    ssize_t rc = 0;
    qp_types_t tp;
    tp = qp_next(unpacker, NULL);

    if (qp_is_map(tp))
    {
        rc = INSERT_assign_by_map(siridb, unpacker, packer);
    }
    else if (qp_is_array(tp))
    {
        qp_packer_t * tmp_packer = qp_packer_new(QP_SUGGESTED_SIZE);
        if (tmp_packer != NULL)
        {
            rc = INSERT_assign_by_array(
                    siridb,
                    unpacker,
                    packer,
                    tmp_packer);
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
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_insert_t * siridb_insert_new(
        siridb_t * siridb,
        uint16_t pid,
        uv_stream_t * client)
{
    siridb_insert_t * insert = (siridb_insert_t *) malloc(
            sizeof(siridb_insert_t) +
            siridb->pools->len * sizeof(qp_packer_t *));

    if (insert == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        insert->free_cb = INSERT_free;
        insert->ref = 1;  /* used as reference on (siri_async_t) handle */

        insert->flags = (siridb->flags & SIRIDB_FLAG_REINDEXING) ?
                INSERT_FLAG_TEST : 0;

        /* n-points will be set later to the correct value */
        insert->npoints = 0;

        /* save PID and client so we can respond to the client */
        insert->pid = pid;
        insert->client = client;

        /*
         * we keep the packer size because the number of pools might change and
         * at this point the pool->len is equal to when the insert was received
         */
        insert->packer_size = siridb->pools->len;

        /*
         * Allocate packers for sending data to pools. we allocate smaller
         * sizes in case we have a lot of pools.
         */
        uint32_t psize = QP_SUGGESTED_SIZE / ((siridb->pools->len / 4) + 1);

        for (size_t n = 0; n < siridb->pools->len; n++)
        {
            if ((insert->packer[n] = sirinet_packer_new(psize)) == NULL)
            {
                return NULL;  /* a signal is raised */
            }

            /* cannot raise a signal since enough space is allocated */
            qp_add_type(insert->packer[n], QP_MAP_OPEN);
        }
    }
    return insert;
}

/*
 * Bind n-points to insert object, lock the client and start async task.
 *
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
int siridb_insert_points_to_pools(siridb_insert_t * insert, size_t npoints)
{
    uv_async_t * handle = (uv_async_t *) malloc(sizeof(uv_async_t));
    if (handle == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    /* bind the number of points to insert object */
    insert->npoints= npoints;

    /* increment the client reference counter */
    sirinet_socket_incref(insert->client);

    uv_async_init(siri.loop, handle, INSERT_points_to_pools);
    handle->data = (void *) insert;

    uv_async_send(handle);
    return 0;
}

int insert_init_backend_local(
        siridb_t * siridb,
        uv_stream_t * client,
        sirinet_pkg_t * pkg,
        uint8_t flags)
{
    sirinet_promise_t * promise =
            (sirinet_promise_t *) malloc(sizeof(sirinet_promise_t));
    if (promise == NULL)
    {
        ERR_ALLOC
        return -1;
    }
    siridb_insert_local_t * ilocal =
            (siridb_insert_local_t *) malloc(sizeof(siridb_insert_local_t));
    if (ilocal == NULL)
    {
        free(promise);
        ERR_ALLOC
        return -1;
    }

    uv_async_t * handle = (uv_async_t *) malloc(sizeof(uv_async_t));
    if (handle == NULL)
    {
        free(promise);
        free(ilocal);
        ERR_ALLOC
        return -1;
    }

    ilocal->free_cb = (uv_close_cb) INSERT_local_free_cb;
    ilocal->ref = 1;
    ilocal->promise = promise;
    ilocal->siridb = siridb;
    ilocal->flags = flags;
    ilocal->status = INSERT_LOCAL_CANCELLED;
    ilocal->forward = NULL;
    ilocal->pcache = NULL;

    promise->pkg = sirinet_pkg_dup(pkg);
    if (promise->pkg == NULL)
    {
        free(promise);
        free(ilocal);
        free(handle);
        ERR_ALLOC
        return -1;
    }
    qp_unpacker_init(&ilocal->unpacker, promise->pkg->data, promise->pkg->len);

    sirinet_socket_incref(client);
    promise->data = client;

    promise->cb = (sirinet_promise_cb) INSERT_local_promise_backend_cb;
    promise->pid = promise->pkg->pid;
    promise->ref = 1;
    promise->timer = NULL;
    promise->server = NULL;

    handle->data = ilocal;

    qp_next(&ilocal->unpacker, NULL); // map
    qp_next(&ilocal->unpacker, &ilocal->qp_series_name); // first series or end

    siridb->active_tasks++;
    siridb->insert_tasks++;

    uv_async_init(siri.loop, handle, INSERT_local_task);
    uv_async_send(handle);

    return 0;
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * This function can raise a SIGNAL.
 */
static void INSERT_on_response(slist_t * promises, uv_async_t * handle)
{
    if (promises != NULL)
    {
        sirinet_pkg_t * pkg;
        sirinet_promise_t * promise;
        siridb_insert_t * insert = (siridb_insert_t *) handle->data;
        siridb_t * siridb =
                ((sirinet_socket_t *) insert->client->data)->siridb;

        char msg[MAX_INSERT_MSG];

        /* the packer size is big enough to hold MAX_INSERT_MSG + some overhead
         * for creating the QPack message */
        qp_packer_t * packer = sirinet_packer_new(256);

        if (packer != NULL)
        {
            cproto_server_t tp = CPROTO_RES_INSERT;

            for (size_t i = 0; i < promises->len; i++)
            {
                promise = promises->data[i];
                if (siri_err || promise == NULL)
                {
                    snprintf(msg,
                            MAX_INSERT_MSG,
                            "Critical error occurred on '%s'",
                            siridb->server->name);
                    tp = CPROTO_ERR_INSERT;
                    continue;
                }
                pkg = (sirinet_pkg_t *) promise->data;

                if (pkg == NULL || pkg->tp != BPROTO_ACK_INSERT)
                {
                    snprintf(msg,
                            MAX_INSERT_MSG,
                            "Error occurred while sending points to at "
                            "least '%s'",
                            promise->server->name);
                    tp = CPROTO_ERR_INSERT;
                }

                /* make sure we free the promise and data */
                free(promise->data);
                sirinet_promise_decref(promise);
            }

            /* this will fit for sure */
            qp_add_type(packer, QP_MAP_OPEN);

            if (tp == CPROTO_ERR_INSERT)
            {
                qp_add_raw(packer, "error_msg", 9);
            }
            else
            {
                qp_add_raw(packer, "success_msg", 11);
                snprintf(msg,
                        MAX_INSERT_MSG,
                        "Successfully inserted %zd point(s).",
                        insert->npoints);
                log_info(msg);
                siridb->received_points += insert->npoints;
            }

            qp_add_string(packer, msg);

            sirinet_pkg_t * response_pkg = sirinet_packer2pkg(
                    packer,
                    insert->pid,
                    tp);

            sirinet_pkg_send((uv_stream_t *) insert->client, response_pkg);
        }
    }

    uv_close((uv_handle_t *) handle, siri_async_close);
}

static void INSERT_local_free_cb(uv_async_t * handle)
{
    siridb_insert_local_t * ilocal = (siridb_insert_local_t *) handle->data;

    /* this destroys the pkg and unpacker */
    free(ilocal->promise->pkg);

    if (ilocal->forward != NULL)
    {
        uv_async_t * fwd = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (fwd == NULL || siri_err)
        {
            if (fwd == NULL)
            {
                ERR_ALLOC
            }
            ilocal->status = INSERT_LOCAL_ERROR;
            siridb_forward_free(ilocal->forward);
        }
        else
        {
            uv_async_init(siri.loop, fwd, siridb_forward_points_to_pools);
            fwd->data = (void *) ilocal->forward;
            uv_async_send(fwd);
        }
    }

    ilocal->promise->cb(ilocal->promise, NULL, ilocal->status);

    ilocal->siridb->active_tasks--;
    ilocal->siridb->insert_tasks--;
    if (ilocal->pcache != NULL)
    {
        siridb_pcache_free(ilocal->pcache);
    }
    free(ilocal);
    free(handle);
}

/*
 * Returns insert->status
 */
static int8_t INSERT_local_work(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_series_name,
        siridb_pcache_t ** pcache)
{
    qp_types_t tp;
    siridb_series_t ** series;
    qp_obj_t qp_series_ts;
    qp_obj_t qp_series_val;
    uint64_t * ts;
    int n = INSERT_AT_ONCE;

    /*
     * we check for siri_err because siridb_series_add_point()
     * should never be called twice on the same series after an
     * error has occurred.
     */
    while ( !siri_err &&
            qp_is_raw_term(qp_series_name) &&
            (n -= WEIGHT_SERIES) > 0)
    {
        series = (siridb_series_t **) ct_get_sure(
                siridb->series,
                qp_series_name->via.raw);
        if (series == NULL)
        {
            log_critical(
                    "Error getting or create series: '%s'",
                    qp_series_name->via.raw);
            return INSERT_LOCAL_ERROR;  /* signal is raised */
        }

        qp_next(unpacker, NULL); // array open
        qp_next(unpacker, NULL); // first point array2
        qp_next(unpacker, &qp_series_ts); // first ts
        qp_next(unpacker, &qp_series_val); // first val

        if (ct_is_empty(*series))
        {
            *series = siridb_series_new(
                    siridb,
                    qp_series_name->via.raw,
                    SIRIDB_QP_MAP2_TP(qp_series_val.tp));

            if (*series == NULL)
            {
                log_critical(
                        "Error creating series: '%s'",
                        qp_series_name->via.raw);
                return INSERT_LOCAL_ERROR;  /* signal is raised */
            }

            n -= WEIGHT_NEW_SERIES;
        }

        ts = (uint64_t *) &qp_series_ts.via.int64;
        SERIES_UPDATE_TS((*series))

        if (siridb_series_add_point(
                siridb,
                *series,
                ts,
                &qp_series_val.via))
        {
            return INSERT_LOCAL_ERROR;  /* signal is raised */
        }

        if ((tp = qp_next(unpacker, qp_series_name)) == QP_ARRAY2)
        {
            if (*pcache == NULL)
            {
                *pcache = siridb_pcache_new((*series)->tp);
                if (*pcache == NULL)
                {
                    return INSERT_LOCAL_ERROR;  /* signal is raised */
                }
            }
            else
            {
                (*pcache)->tp = (*series)->tp;
                (*pcache)->len = 0;
            }

            do
            {
                qp_next(unpacker, &qp_series_ts); // ts
                qp_next(unpacker, &qp_series_val); // val

                ts = (uint64_t *) &qp_series_ts.via.int64;
                SERIES_UPDATE_TS((*series))

                if (siridb_pcache_add_point(
                        *pcache,
                        ts,
                        &qp_series_val.via))
                {
                    return INSERT_LOCAL_ERROR;  /* signal is raised */
                }

                n--;
            }
            while ((tp = qp_next(unpacker, qp_series_name)) == QP_ARRAY2);

            if (siridb_series_add_pcache(
                    siridb,
                    *series,
                    *pcache))
            {
                return INSERT_LOCAL_ERROR;  /* signal is raised */
            }
        }


        if (tp == QP_ARRAY_CLOSE)
        {
            qp_next(unpacker, qp_series_name);
        }
    }

    return siri_err;  /* expected to be 0 */
}

/*
 * Returns insert->status
 */
static int INSERT_local_work_test(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_series_name,
        siridb_pcache_t ** pcache,
        siridb_forward_t ** forward)
{
    qp_types_t tp;
    siridb_series_t * series;
    uint16_t pool;
    uint64_t * ts;
    const char * series_name;
    char * pt;
    qp_obj_t qp_series_ts;
    qp_obj_t qp_series_val;
    int n = INSERT_AT_ONCE;

    /*
     * we check for siri_err because siridb_series_add_point()
     * should never be called twice on the same series after an
     * error has occurred.
     */
    while ( !siri_err &&
            qp_is_raw_term(qp_series_name) &&
            (n -= WEIGHT_SERIES) > 0)
    {
        series_name = qp_series_name->via.raw;
        series = (siridb_series_t *) ct_get(siridb->series, series_name);
        if (series == NULL)
        {
            /* the series does not exist so check what to do... */
            pool = siridb_lookup_sn(siridb->pools->lookup, series_name);

            if (pool == siridb->server->pool)
            {
                /*
                 * This is the correct pool so create the series and
                 * add the points.
                 */

                /* save pointer position and read series type */
                pt = unpacker->pt;
                qp_next(unpacker, NULL); // array open
                qp_next(unpacker, NULL); // first point array2
                qp_next(unpacker, NULL); // first ts
                qp_next(unpacker, &qp_series_val); // first val

                /* restore pointer position */
                unpacker->pt = pt;

                series = siridb_series_new(
                        siridb,
                        series_name,
                        SIRIDB_QP_MAP2_TP(qp_series_val.tp));

                if (series == NULL ||
                    ct_add(siridb->series, series->name, series))
                {
                    log_critical("Error creating series: '%s'", series_name);
                    return INSERT_LOCAL_ERROR;  /* signal is raised */
                }

                n -= WEIGHT_NEW_SERIES;
            }
            else if (siridb->replica == NULL ||
                    siridb_series_server_id_by_name(series_name) ==
                    		siridb->server->id)
            {
                /*
                 * Forward the series to the correct pool because 'this' server
                 * is responsible for the series.
                 */
                if (*forward == NULL)
                {
                    if ((*forward = siridb_forward_new(siridb)) == NULL)
                    {
                        return INSERT_LOCAL_ERROR;  /* signal is raised */
                    }
                }
                /* testing is not needed since we check for siri_err later */
                qp_add_raw(
                        (*forward)->packer[pool],
                        series_name,
                        qp_series_name->len);
                qp_packer_extend_fu((*forward)->packer[pool], unpacker);
                qp_next(unpacker, qp_series_name);
                continue;  /* expected to be 0 */
            }
            else
            {
                /*
                 * Skip this series since it will forwarded to the correct
                 * pool by the replica server.
                 */
                qp_skip_next(unpacker);  // array
                qp_next(unpacker, qp_series_name);
                continue;  /* expected to be 0 */
            }
        }

        qp_next(unpacker, NULL); // array open
        qp_next(unpacker, NULL); // first point array2
        qp_next(unpacker, &qp_series_ts); // first ts
        qp_next(unpacker, &qp_series_val); // first val

        ts = (uint64_t *) &qp_series_ts.via.int64;
        SERIES_UPDATE_TS(series)

        if (siridb_series_add_point(
                siridb,
                series,
                ts,
                &qp_series_val.via))
        {
            return INSERT_LOCAL_ERROR;  /* signal is raised */
        }

        if ((tp = qp_next(unpacker, qp_series_name)) == QP_ARRAY2)
        {
            if (*pcache == NULL)
            {
                *pcache = siridb_pcache_new(series->tp);
                if (*pcache == NULL)
                {
                    return INSERT_LOCAL_ERROR;  /* signal is raised */
                }
            }
            else
            {
                (*pcache)->tp = series->tp;
                (*pcache)->len = 0;
            }

            do
            {
                qp_next(unpacker, &qp_series_ts); // ts
                qp_next(unpacker, &qp_series_val); // val

                ts = (uint64_t *) &qp_series_ts.via.int64;
                SERIES_UPDATE_TS(series)

                if (siridb_pcache_add_point(
                        *pcache,
                        ts,
                        &qp_series_val.via))
                {
                    return INSERT_LOCAL_ERROR;  /* signal is raised */
                }

                n--;
            }
            while ((tp = qp_next(unpacker, qp_series_name)) == QP_ARRAY2);

            if (siridb_series_add_pcache(
                    siridb,
                    series,
                    *pcache))
            {
                return INSERT_LOCAL_ERROR;  /* signal is raised */
            }
        }

        if (tp == QP_ARRAY_CLOSE)
        {
            qp_next(unpacker, qp_series_name);
        }
    }

    return siri_err;  /* expected to be 0 */
}

static void INSERT_local_task(uv_async_t * handle)
{

    siridb_insert_local_t * ilocal = (siridb_insert_local_t *) handle->data;
    qp_unpacker_t * unpacker = &ilocal->unpacker;

    /*
     * we check for siri_err because siridb_series_add_point()
     * should never be called twice on the same series after an
     * error has occurred.
     */
    if (ilocal->status == INSERT_LOCAL_ERROR)
    {
        uv_close((uv_handle_t *) handle, siri_async_close);
        return;
    }

    if (!qp_is_raw_term(&ilocal->qp_series_name))
    {
        ilocal->status = INSERT_LOCAL_SUCESS;
        uv_close((uv_handle_t *) handle, siri_async_close);
        return;
    }

    siridb_t * siridb = ilocal->siridb;

    if (siridb->buffer_fp == NULL && siridb_buffer_open(siridb))
    {
        ERR_FILE
        ilocal->status = INSERT_LOCAL_ERROR;
        uv_close((uv_handle_t *) handle, siri_async_close);
        return;
    }

    if (siridb->store == NULL && siridb_series_open_store(siridb))
    {
        ERR_FILE
        ilocal->status = INSERT_LOCAL_ERROR;
        uv_close((uv_handle_t *) handle, siri_async_close);
        return;
    }

    uv_mutex_lock(&siridb->series_mutex);
    uv_mutex_lock(&siridb->shards_mutex);

    if ((ilocal->flags & INSERT_FLAG_TEST) || (
            (siridb->flags & SIRIDB_FLAG_REINDEXING) &&
            (~ilocal->flags & INSERT_FLAG_TESTED)))
    {
        /*
         * We can use INSERT_local_work_test even if 'this' server has not set
         * the REINDEXING flag yet, since this does not depend on 'prev_lookup'
         */
        if (INSERT_local_work_test(
                siridb,
                unpacker,
                &ilocal->qp_series_name,
                &ilocal->pcache,
                &ilocal->forward))
        {
            ilocal->status = INSERT_LOCAL_ERROR;
        }
    }
    else
    {
        /* siri_err is raised in case of an error */
        if (INSERT_local_work(
                siridb,
                unpacker,
                &ilocal->qp_series_name,
                &ilocal->pcache))
        {
            ilocal->status = INSERT_LOCAL_ERROR;
        }
    }
    uv_mutex_unlock(&siridb->series_mutex);
    uv_mutex_unlock(&siridb->shards_mutex);

    uv_async_send(handle);
}

static void INSERT_local_promise_cb(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
#ifdef DEBUG
    assert (pkg == NULL);
#endif
    pkg = sirinet_pkg_new(
            0,
            0,
            status == 0 ? BPROTO_ACK_INSERT : BPROTO_ERR_INSERT,
            NULL);
    sirinet_promises_t * promises = (sirinet_promises_t *) promise->data;
    promise->data = pkg;
    slist_append(promises->promises, (void *) promise);

    SIRINET_PROMISES_CHECK(promises)
}

static void INSERT_local_promise_backend_cb(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
#ifdef DEBUG
    assert (pkg == NULL);
#endif
    uv_stream_t * client = (uv_stream_t *) promise->data;

    pkg = sirinet_pkg_new(
            promise->pid,
            0,
            status == 0 ? BPROTO_ACK_INSERT : BPROTO_ERR_INSERT,
            NULL);

    if (pkg != NULL)
    {
        sirinet_pkg_send(client, pkg);
    }
    sirinet_socket_decref(client);
    sirinet_promise_decref(promise);
}

/*
 * Start async insert task.
 *
 * This function is responsible for calling free on pkg.
 */
static int INSERT_init_local(
        siridb_t * siridb,
        sirinet_promises_t * promises,
        sirinet_pkg_t * pkg,
        uint8_t flags)
{
    sirinet_promise_t * promise =
            (sirinet_promise_t *) malloc(sizeof(sirinet_promise_t));
    if (promise == NULL)
    {
        free(pkg);
        ERR_ALLOC
        return -1;
    }
    siridb_insert_local_t * ilocal =
            (siridb_insert_local_t *) malloc(sizeof(siridb_insert_local_t));
    if (ilocal == NULL)
    {
        free(pkg);
        free(promise);
        ERR_ALLOC
        return -1;
    }

    uv_async_t * handle = (uv_async_t *) malloc(sizeof(uv_async_t));
    if (handle == NULL)
    {
        free(pkg);
        free(promise);
        free(ilocal);
        ERR_ALLOC
        return -1;
    }

    ilocal->free_cb = (uv_close_cb) INSERT_local_free_cb;
    ilocal->ref = 1;
    ilocal->promise = promise;
    ilocal->siridb = siridb;
    qp_unpacker_init(&ilocal->unpacker, pkg->data, pkg->len);
    ilocal->flags = flags;
    ilocal->status = INSERT_LOCAL_CANCELLED;
    ilocal->forward = NULL;
    ilocal->pcache = NULL;

    promise->pkg = pkg;
    promise->data = promises;
    /* We do not need an increment here since this is the local server */
    promise->server = siridb->server;
    promise->cb = (sirinet_promise_cb) INSERT_local_promise_cb;
    promise->pid = 0;
    promise->ref = 1;
    promise->timer = NULL;

    handle->data = ilocal;

    qp_next(&ilocal->unpacker, NULL); // map
    qp_next(&ilocal->unpacker, &ilocal->qp_series_name); // first series or end

    siridb->active_tasks++;
    siridb->insert_tasks++;
    uv_async_init(siri.loop, handle, INSERT_local_task);
    uv_async_send(handle);

    return 0;
}



/*
 * Call-back function:  uv_async_cb
 *
 * In case of an error a SIGNAL is raised and a successful message will not
 * be send to the client.
 */
static void INSERT_points_to_pools(uv_async_t * handle)
{
    siridb_insert_t * insert = (siridb_insert_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) insert->client->data)->siridb;
    uint16_t pool = siridb->server->pool;
    sirinet_pkg_t * pkg, * repl_pkg;
    sirinet_promises_t * promises = sirinet_promises_new(
            siridb->pools->len,
            (sirinet_promises_cb) INSERT_on_response,
            handle,
            NULL);

    if (promises == NULL)
    {
        return;  /* signal is raised */
    }

    int pool_count = 0;

    for (uint16_t n = 0; n < insert->packer_size; n++)
    {
        if (insert->packer[n]->len == PKG_HEADER_SIZE + 1)
        {
            /*
             * skip empty packer.
             * (empty packer has only PKG_HEADER_SIZE + QP_MAP_OPEN)
             */
            qp_packer_free(insert->packer[n]);
        }
        else if (n == pool)
        {
            if (siridb->replica != NULL)
            {
                repl_pkg = siridb->replicate->initsync == NULL ? NULL :
                        siridb_replicate_pkg_filter(
                            siridb,
                            insert->packer[n]->buffer + PKG_HEADER_SIZE,
                            insert->packer[n]->len - PKG_HEADER_SIZE,
                            insert->flags);

                pkg = sirinet_packer2pkg(
                        insert->packer[n],
                        0,
                        (insert->flags & INSERT_FLAG_TEST) ?
                            BPROTO_INSERT_TEST_SERVER :
                        (insert->flags & INSERT_FLAG_TESTED) ?
                            BPROTO_INSERT_TESTED_SERVER :
                            BPROTO_INSERT_SERVER);

                /* send to replica, use repl_pkg if needed */
                siridb_replicate_pkg(
                        siridb,
                        repl_pkg == NULL ? pkg : repl_pkg);

                free(repl_pkg);
            }
            else
            {
                pkg = sirinet_packer2pkg(insert->packer[n], 0, 0);
            }

            if (INSERT_init_local(
                    siridb,
                    promises,
                    pkg,
                    insert->flags) == 0)
            {
                pool_count++;
            }
        }
        else
        {
            pkg = sirinet_packer2pkg(
                    insert->packer[n],
                    0,
                    (insert->flags & INSERT_FLAG_TEST) ?
                            BPROTO_INSERT_TEST_POOL : BPROTO_INSERT_POOL);
            if (siridb_pool_send_pkg(
                    siridb->pools->pool + n,
                    pkg,
                    INSERT_TIMEOUT,
                    sirinet_promises_on_response,
                    promises,
                    0))
            {
                free(pkg);
                log_error(
                    "Although we have checked and validated each pool "
                    "had at least one server available, it seems that the "
                    "situation has changed and we cannot send points to "
                    "pool %u", n);
            }
            else
            {
                pool_count++;
            }
        }
        insert->packer[n] = NULL;
    }

    /* pool_count is always smaller than the initial promises->size */
    promises->promises->size = pool_count;

    SIRINET_PROMISES_CHECK(promises)
}

/*
 * Returns the correct pool.
 */
static uint16_t INSERT_get_pool(siridb_t * siridb, qp_obj_t * qp_series_name)
{
    uint16_t pool;

    if (~siridb->flags & SIRIDB_FLAG_REINDEXING)
    {
        /* when not re-indexing, select the correct pool */
        pool = siridb_lookup_sn_raw(
                siridb->pools->lookup,
                qp_series_name->via.raw,
                qp_series_name->len);
    }
    else
    {
        if (ct_getn(
                siridb->series,
                qp_series_name->via.raw,
                qp_series_name->len) != NULL)
        {
            /*
             * we are re-indexing and at least at this moment still own the
             * series
             */
            pool = siridb->server->pool;
        }
        else
        {
            /*
             * We are re-indexing and do not have the series.
             * Select the correct pool BEFORE re-indexing was
             * started or the new correct pool if this pool is
             * the previous correct pool. (we can do this now
             * because we known we don't have the series)
             */
#ifdef DEBUG
            assert (siridb->pools->prev_lookup != NULL);
#endif
            pool = siridb_lookup_sn_raw(
                    siridb->pools->prev_lookup,
                    qp_series_name->via.raw,
                    qp_series_name->len);

            if (pool == siridb->server->pool)
            {
                pool = siridb_lookup_sn_raw(
                        siridb->pools->lookup,
                        qp_series_name->via.raw,
                        qp_series_name->len);
            }
        }
    }
    return pool;
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
        qp_packer_t * packer[])
{
    int tp;  /* use int instead of qp_types_t for negative values */
    uint16_t pool;
    ssize_t count = 0;
    qp_obj_t qp_obj;

    tp = qp_next(unpacker, &qp_obj);

    while ( tp == QP_RAW &&
            qp_obj.len &&
            qp_obj.len < SIRIDB_SERIES_NAME_LEN_MAX)
    {
        pool = INSERT_get_pool(siridb, &qp_obj);

        qp_add_raw_term(packer[pool],
                qp_obj.via.raw,
                qp_obj.len);

        if ((tp = INSERT_read_points(
                siridb,
                packer[pool],
                unpacker,
                &qp_obj,
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
        qp_packer_t * tmp_packer)
{
    int tp;  /* use int instead of qp_types_t for negative values */
    uint16_t pool;
    ssize_t count = 0;
    qp_obj_t qp_obj;
    tp = qp_next(unpacker, &qp_obj);

    while (tp == QP_MAP2)
    {
        if (qp_next(unpacker, &qp_obj) != QP_RAW)
        {
            return ERR_EXPECTING_NAME_AND_POINTS;
        }

        if (strncmp(qp_obj.via.raw, "points", qp_obj.len) == 0)
        {
            if ((tp = INSERT_read_points(
                    siridb,
                    tmp_packer,
                    unpacker,
                    &qp_obj,
                    &count)) < 0 || tp != QP_RAW)
            {
                return (tp < 0) ? tp : ERR_EXPECTING_NAME_AND_POINTS;
            }
        }

        if (strncmp(qp_obj.via.raw, "name", qp_obj.len) == 0)
        {
            if (    qp_next(unpacker, &qp_obj) != QP_RAW ||
                    !qp_obj.len ||
                    qp_obj.len >= SIRIDB_SERIES_NAME_LEN_MAX)
            {
                return ERR_EXPECTING_NAME_AND_POINTS;
            }

            pool = INSERT_get_pool(siridb, &qp_obj);

            qp_add_raw_term(packer[pool],
                    qp_obj.via.raw,
                    qp_obj.len);
        }
        else
        {
            return ERR_EXPECTING_NAME_AND_POINTS;
        }

        if (tmp_packer->len)
        {
            qp_packer_extend(packer[pool], tmp_packer);
            tmp_packer->len = 0;
            tp = qp_next(unpacker, &qp_obj);
        }
        else
        {
            if (qp_next(unpacker, &qp_obj) != QP_RAW ||
                    strncmp(qp_obj.via.raw, "points", qp_obj.len))
            {
                return ERR_EXPECTING_NAME_AND_POINTS;
            }

            if ((tp = INSERT_read_points(
                    siridb,
                    packer[pool],
                    unpacker,
                    &qp_obj,
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
        ssize_t * count)
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

        if (!siridb_int64_valid_ts(siridb, qp_obj->via.int64))
        {
            return ERR_TIMESTAMP_OUT_OF_RANGE;
        }

        qp_add_int64(packer, qp_obj->via.int64);

        switch (qp_next(unpacker, qp_obj))
        {
        case QP_RAW:
            return ERR_UNSUPPORTED_VALUE;
//            TODO: Add support for strings
//            qp_add_raw(packer, qp_obj->via.raw, qp_obj->len);
//            break;

        case QP_INT64:
            qp_add_int64(packer, qp_obj->via.int64);
            break;

        case QP_DOUBLE:
            qp_add_double(packer, qp_obj->via.real);
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

    /* decrement the client reference counter */
    sirinet_socket_decref(insert->client);

    /* free insert */
    siridb_insert_free(insert);

    /* free handle */
    free((uv_async_t *) handle);

}


