/*
 * promise.h - Promise SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 21-06-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/net/socket.h>
#include <siri/db/server.h>
#include <siri/net/pkg.h>

#define PROMISE_DEFAULT_TIMEOUT 30000  // 30 seconds

typedef struct siridb_server_s siridb_server_t;
typedef struct sirinet_promise_s sirinet_promise_t;

typedef enum
{
    PROMISE_TIMEOUT_ERROR=-4,   // in case of a time out
    PROMISE_WRITE_ERROR,        // socket write error
    PROMISE_CANCELLED_ERROR,    // timer is cancelled
    PROMISE_PKG_TYPE_ERROR,     // unexpected package type received
    PROMISE_SUCCESS=0
} sirinet_promise_status_t;

typedef void (* sirinet_promise_cb)(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

/* the callback will always be called and is responsible to free the promise */
typedef struct sirinet_promise_s
{
    uv_timer_t * timer;
    sirinet_promise_cb cb;
    siridb_server_t * server;
    uint64_t pid;
    void * data;
} sirinet_promise_t;

const char * sirinet_promise_strstatus(sirinet_promise_status_t status);


