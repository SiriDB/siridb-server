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
#ifndef SIRINET_PROMISES_H_
#define SIRINET_PROMISES_H_

typedef struct sirinet_promises_s sirinet_promises_t;

#include <slist/slist.h>
typedef void (* sirinet_promises_cb)(
        slist_t * promises,
        void * data);

#include <siri/net/promise.h>
#include <siri/net/pkg.h>

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

struct sirinet_promises_s
{
    sirinet_promises_cb cb;
    slist_t * promises;
    void * data;
    sirinet_pkg_t * pkg;
};

#endif  /* SIRINET_PROMISES_H_ */
