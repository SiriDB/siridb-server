/*
 * promises.h - Collection for promised.
 */
#ifndef SIRINET_PROMISES_H_
#define SIRINET_PROMISES_H_

typedef struct sirinet_promises_s sirinet_promises_t;

#include <vec/vec.h>
typedef void (* sirinet_promises_cb)(
        vec_t * promises,
        void * data);

#include <siri/net/promise.h>
#include <siri/net/pkg.h>

sirinet_promises_t * sirinet_promises_new(
        size_t size,
        sirinet_promises_cb cb,
        void * data,
        sirinet_pkg_t * pkg);
void sirinet_promises_llist_free(vec_t * promises);
void sirinet_promises_on_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

#define SIRINET_PROMISES_CHECK(promises)                    \
if (promises->promises->len == promises->promises->size)    \
{                                                           \
    free(promises->pkg);                                    \
    promises->cb(promises->promises, promises->data);       \
    vec_free(promises->promises);                         \
    free(promises);                                         \
}

struct sirinet_promises_s
{
    sirinet_promises_cb cb;
    vec_t * promises;
    void * data;
    sirinet_pkg_t * pkg;
};

#endif  /* SIRINET_PROMISES_H_ */
