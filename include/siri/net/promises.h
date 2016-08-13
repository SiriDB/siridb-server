/*
 * promises.h - Promises SiriDB.
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
#pragma once
#include <siri/net/promise.h>
#include <slist/slist.h>
#include <siri/net/pkg.h>


typedef struct sirinet_promise_s sirinet_promise_t;

typedef void (* sirinet_promises_cb)(
        slist_t * promises,
        void * data);

typedef struct sirinet_promises_s
{
    sirinet_promises_cb cb;
    slist_t * promises;
    void * data;
    sirinet_pkg_t * pkg;
} sirinet_promises_t;

sirinet_promises_t * sirinet_promises_new(
        size_t size,
        sirinet_promises_cb cb,
        void * data,
        sirinet_pkg_t * pkg);
void sirinet_promises_llist_free(slist_t * promises);
void sirinet_promises_on_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

#define SIRINET_PROMISES_CHECK(promises)                    \
if (promises->promises->len == promises->promises->size)    \
{                                                           \
    free(promises->pkg);                                    \
    promises->cb(promises->promises, promises->data);       \
    slist_free(promises->promises);                         \
    free(promises);                                         \
}
