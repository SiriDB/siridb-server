/*
 * promise.c - Promise SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 23-06-2016
 *
 */

#include <siri/net/promise.h>
#include <assert.h>
#include <logger/logger.h>

const char * sirinet_promise_strstatus(sirinet_promise_status_t status)
{
    switch (status)
    {
    case PROMISE_SUCCESS: return "success";
    case PROMISE_TIMEOUT_ERROR: return "timed out";
    case PROMISE_WRITE_ERROR: return "write error";
    case PROMISE_CANCELLED_ERROR: return "cancelled";
    case PROMISE_PKG_TYPE_ERROR: return "unexpected package type";
    }
    /* all cases MUST be handled */
    assert(0);
    return "";
}

sirinet_promises_t * sirinet_promises_new(
        size_t size,
        sirinet_promises_cb_t cb,
        void * data)
{
    sirinet_promises_t * promises =
            (sirinet_promises_t *) malloc(sizeof(sirinet_promises_t));
    promises->promises = slist_new(size);
    promises->cb = cb;
    promises->data = data;
    return promises;
}

void sirinet_promise_on_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    /* This method will clean the promises type and list. The promises->cb
     * is responsible for calling 'free' on each promise and promise->data.
     */
    sirinet_promises_t * promises = promise->data;

    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug(
                "Error occurred while sending package to '%s' (%s)",
                promise->server->name,
                sirinet_promise_strstatus(status));
        promise->data = NULL;
    }
    else
    {
        promise->data = sirinet_pkg_dup(pkg);
    }

    slist_append(promises->promises, promise);

    SIRINET_PROMISES_CHECK(promises)
}
