/*
 * util/omap.h
 */
#ifndef OMAP_H_
#define OMAP_H_

enum
{
    OMAP_ERR_EXIST  =-2,
    OMAP_ERR_ALLOC  =-1,
    OMAP_SUCCESS    =0
};

typedef struct omap_s omap_t;
typedef struct omap__s omap__t;
typedef struct omap__s * omap_iter_t;

#include <inttypes.h>

typedef void (*omap_destroy_cb)(void * data);

/* private */
struct omap__s
{
    omap__t * next_;
    uint64_t id_;
    void * data_;
};

omap_t * omap_create(void);
void omap_destroy(omap_t * omap, omap_destroy_cb cb);
int omap_add(omap_t * omap, uint64_t id, void * data);
void * omap_set(omap_t * omap, uint64_t id, void * data);
void * omap_get(omap_t * omap, uint64_t id);
uint64_t * omap_last_id(omap_t * omap);
void * omap_rm(omap_t * omap, uint64_t id);
static inline omap_iter_t omap_iter(omap_t * omap);
static inline uint64_t omap_iter_id(omap_iter_t iter);
#define omap_each(iter__, dt__, var__) \
        dt__ * var__; \
        iter__ && \
        (var__ = (dt__ *) iter__->data_); \
        iter__ = iter__->next_

struct omap_s
{
    omap__t * next_;
    size_t n;
};

static inline omap_iter_t omap_iter(omap_t * omap)
{
    return omap->next_;
}

static inline uint64_t omap_iter_id(omap_iter_t iter)
{
    return iter->id_;
}


#endif  /* OMAP_H_ */
