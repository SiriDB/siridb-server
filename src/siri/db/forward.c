/*
 * forward.c - Handle forwarding series while re-indexing
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 31-07-2016
 *
 */
#include <qpack/qpack.h>
#include <siri/async.h>
#include <siri/db/forward.h>
#include <siri/err.h>
#include <siri/net/promises.h>
#include <siri/net/protocol.h>
#include <stddef.h>

static void FORWARD_on_response(slist_t * promises, uv_async_t * handle);
static void FORWARD_free(uv_handle_t * handle);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_forward_t * siridb_forward_new(siridb_t * siridb)
{
    uint16_t size = siridb->pools->len;

    siridb_forward_t * forward = (siridb_forward_t *) malloc(
            sizeof(siridb_forward_t) + size * sizeof(qp_packer_t *));

    if (forward == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        forward->free_cb = FORWARD_free;
        forward->ref = 1; /* used as reference on (siri_async_t) handle */
        forward->siridb = siridb;

        /*
         * we keep the packer size because the number of pools might change and
         * at this point the pool->len is equal to when the insert was received
         */
        forward->size = size;

        /*
         * Allocate packers for sending data to pools. we allocate smaller
         * sizes in case we have a lot of pools.
         */
        uint32_t psize = QP_SUGGESTED_SIZE / ((size / 4) + 1);

        for (size_t n = 0; n < size; n++)
        {
            if (n == siridb->server->pool)
            {
                forward->packer[n] = NULL;
            }
            else if ((forward->packer[n] = sirinet_packer_new(psize)) != NULL)
            {
                /* cannot raise a signal since enough space is allocated */
                qp_add_type(forward->packer[n], QP_MAP_OPEN);
            }
            else
            {
                return NULL;  /* a signal is raised */
            }
        }
    }
    return forward;
}

/*
 * Destroy forward.
 */
void siridb_forward_free(siridb_forward_t * forward)
{
    /* free packer */
    for (size_t n = 0; n < forward->size; n++)
    {
        if (forward->packer[n] != NULL)
        {
            qp_packer_free(forward->packer[n]);
        }
    }

    /* free forward */
    free(forward);

#ifdef DEBUG
    log_debug("Free forward!, hooray!");
#endif
}

/*
 * Call-back function:  uv_async_cb
 *
 * In case of an error a SIGNAL is raised and a successful message will not
 * be send to the client.
 */
void siridb_forward_points_to_pools(uv_async_t * handle)
{
    siridb_forward_t * forward = (siridb_forward_t *) handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promises_t * promises = sirinet_promises_new(
            forward->size,
            (sirinet_promises_cb) FORWARD_on_response,
            handle,
            NULL);

    if (promises == NULL)
    {
        return;  /* signal is raised */
    }

    int pool_count = 0;

    for (uint16_t n = 0; n < forward->size; n++)
    {
        if (    forward->packer[n] == NULL ||
                forward->packer[n]->len == sizeof(sirinet_pkg_t) + 1)
        {
            /*
             * skip empty packer and NULL.
             * (empty packer has only sizeof(sirinet_pkg_t) + QP_MAP_OPEN)
             */
            continue;
        }
        pkg = sirinet_packer2pkg(
                forward->packer[n],
                0,
                BPROTO_INSERT_TESTED_POOL);

        /* the packer is destroyed, set to NULL */
        forward->packer[n] = NULL;

        if (siridb_pool_send_pkg(
                forward->siridb->pools->pool + n,
                pkg,
                0,
                sirinet_promises_on_response,
                promises,
                0))
        {
            log_critical("One pool is unreachable while re-indexing!");
            free(pkg);
            /*
             * TODO: we can add the package to some retry queue for this pool
             */
        }
        else
        {
            pool_count++;
        }
    }

    /* pool_count is always smaller than the initial promises->size */
    promises->promises->size = pool_count;

    SIRINET_PROMISES_CHECK(promises)
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * This function can raise a SIGNAL.
 */
static void FORWARD_on_response(slist_t * promises, uv_async_t * handle)
{
    if (promises != NULL)
    {
        sirinet_pkg_t * pkg;
        sirinet_promise_t * promise;
        siridb_forward_t * forward = (siridb_forward_t *) handle->data;

        for (size_t i = 0; i < promises->len; i++)
        {
            promise = promises->data[i];
            if (promise == NULL)
            {
                log_critical("Critical error occurred on '%s'",
                        forward->siridb->server->name);
                continue;
            }
            pkg = promise->data;

            if (pkg == NULL || pkg->tp != BPROTO_ACK_INSERT)
            {
                log_critical(
                        "Error occurred while sending points to at least '%s'",
                        promise->server->name);
            }

            /* make sure we free the promise and data */
            free(promise->data);
            sirinet_promise_decref(promise);
        }
    }

    uv_close((uv_handle_t *) handle, siri_async_close);
}

/*
 * Used as uv_close_cb.
 */
static void FORWARD_free(uv_handle_t * handle)
{
    siridb_forward_t * forward = (siridb_forward_t *) handle->data;

    /* free forward */
    siridb_forward_free(forward);

    /* free handle */
    free((uv_async_t *) handle);

}
