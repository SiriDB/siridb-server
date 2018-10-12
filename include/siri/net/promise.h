/*
 * promise.c - Promise used for sending to data to SiriDB servers.
 */
#ifndef SIRINET_PROMISE_H_
#define SIRINET_PROMISE_H_

#define PROMISE_DEFAULT_TIMEOUT 30000  /* 30 seconds  */

typedef enum
{
    PROMISE_TIMEOUT_ERROR=-4,   /* in case of a time out            */
    PROMISE_WRITE_ERROR,        /* socket write error               */
    PROMISE_CANCELLED_ERROR,    /* timer is cancelled               */
    PROMISE_PKG_TYPE_ERROR,     /* unexpected package type received */
    PROMISE_SUCCESS=0
} sirinet_promise_status_t;

typedef struct sirinet_promise_s sirinet_promise_t;
typedef void (* sirinet_promise_cb)(
        sirinet_promise_t * promise,
        void * data,
        int status);

#include <uv.h>
#include <siri/net/stream.h>
#include <siri/db/server.h>
#include <siri/net/pkg.h>



const char * sirinet_promise_strstatus(sirinet_promise_status_t status);

#define sirinet_promise_incref(promise) promise->ref++
#define sirinet_promise_decref(promise) if (!--promise->ref) free(promise)

/* the callback will always be called and is responsible to free the promise */
struct sirinet_promise_s
{
    uint16_t pid;
    uint16_t ref;
    uv_timer_t * timer;
    sirinet_promise_cb cb;
    siridb_server_t * server;
    sirinet_pkg_t * pkg;
    void * data;
};

#endif  /* SIRINET_PROMISE_H_ */
