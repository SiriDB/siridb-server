/*
 * promises.c - Promises SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 13-07-2016
 *
 */

#include <siri/net/promises.h>
#include <assert.h>
#include <logger/logger.h>
#include <siri/err.h>

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
sirinet_promises_t * sirinet_promises_new(
        size_t size,
        sirinet_promises_cb cb,
        void * data,
        sirinet_pkg_t * pkg)
{
    sirinet_promises_t * promises =
            (sirinet_promises_t *) malloc(sizeof(sirinet_promises_t));
    if (promises == NULL)
    {
        ERR_ALLOC
    }
    else
    {

        promises->cb = cb;
        promises->data = data;
        promises->promises = slist_new(size);
        promises->pkg = pkg;

        if (promises->promises == NULL)
        {
            free(promises);
            promises = NULL;  /* signal is raised */
        }
    }
    return promises;
}

/*
 * This function can be used to free promises with data. It assumes
 * data simple can be destroyed with simple calling free().
 */
void sirinet_promises_llist_free(slist_t * promises)
{
    sirinet_promise_t * promise;
    for (size_t i = 0; i < promises->len; i++)
    {
        promise = (sirinet_promise_t *) promises->data[i];
        if (promise != NULL)
        {
            /* make sure we free the promise and data */
            free(promise->data);
            free(promise);
        }
    }
}

/*
 * This function will clean the promises type and list. The promises->cb
 * is responsible for calling 'free' on each promise and promise->data.
 */
void sirinet_promises_on_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    sirinet_promises_t * promises = (sirinet_promises_t *) promise->data;

    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug(
                "Error occurred while sending package to '%s' (%s)",
                promise->server->name,
                sirinet_promise_strstatus((sirinet_promise_status_t) status));
        promise->data = NULL;
    }
    else
    {
        /* we can ignore errors from sirinet_pkg_dup() */
        promise->data = sirinet_pkg_dup(pkg);
    }

    slist_append(promises->promises, promise);

    SIRINET_PROMISES_CHECK(promises)
}
