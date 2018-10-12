/*
 * promise.c - Promise used for sending to data to SiriDB servers.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/err.h>
#include <siri/net/promise.h>

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

/*
 * Creating a new promise is done in 'siridb_server_send_pkg'.
 */


