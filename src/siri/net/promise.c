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

const char * sirinet_promise_strstatus(sirinet_promise_status_t status)
{
    switch (status)
    {
    case PROMISE_SUCCESS: return "success";
    case PROMISE_TMEOUT_ERROR: return "timed out";
    case PROMISE_WRITE_ERROR: return "write error";
    case PROMISE_CANCELLED_ERROR: return "cancelled";
    }
    /* all cases MUST be handled */
    assert(0);
    return "";
}


static void promise_on_response(
        sirinet_promise_t * promise,
        const sirinet_pkg_t * pkg,
        int status)
{
    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug(
                "Error occurred while sending package to '%s' (%s)",
                promise->server->name,
                sirinet_promise_strstatus(status));
    }

    sirinet_promises_t * promises = promise->data;

    /* pkg is NULL when and only when an error has occurred */
#ifdef DEBUG
    assert (pkg != NULL || status);
#endif
    promise->data = pkg;
    slist_append(promises->promises, promise);

    if (promises->promises->len == promises->promises->size)
    {
        promises->cb(promises->promises, promises->data);
        free(promises);
    }

}
