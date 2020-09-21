/*
 * util/omap.h
 */
#include <assert.h>
#include <stdlib.h>
#include <omap/omap.h>

static void * omap__rm(omap_t * omap, omap__t ** omap_);
static omap__t * omap__new(uint64_t id, void * data, omap__t * next);

omap_t * omap_create(void)
{
    omap_t * omap = malloc(sizeof(omap_t));
    if (!omap)
    {
        return NULL;
    }

    omap->next_ = NULL;
    omap->n = 0;

    return omap;
}

void omap_destroy(omap_t * omap, omap_destroy_cb cb)
{
    if (!omap)
    {
        return;
    }
    omap__t * cur = (omap__t *) omap;
    omap__t * tmp;

    for (; (tmp = cur->next_); cur = tmp)
    {
        if (cb && tmp)
        {
            (*cb)(tmp->data_);
        }
        free(cur);
    }
    free(cur);
}

/*
 * In case of a duplicate id the return value is OMAP_ERR_EXIST and data
 * will NOT be overwritten. On success the return value is OMAP_SUCCESS and
 * if a memory error has occurred the return value is OMAP_ERR_ALLOC.
 */
int omap_add(omap_t * omap, uint64_t id, void * data)
{
    assert (omap);
    assert (data);
    omap__t * cur, * tmp;

    for (   cur = (omap__t *) omap;
            cur->next_ && cur->next_->id_ < id;
            cur = cur->next_);

    if (cur->next_ && cur->next_->id_ == id)
    {
        return OMAP_ERR_EXIST;
    }

    tmp = omap__new(id, data, cur->next_);
    if (!tmp)
    {
        return OMAP_ERR_ALLOC;
    }

    omap->n++;
    cur->next_ = tmp;

    return OMAP_SUCCESS;
}

/*
 * In case of a duplicate id the return value is the previous value and data
 * will be overwritten. On success the return value is equal to void*data and
 * if a memory error has occurred the return value is NULL.
 */
void * omap_set(omap_t * omap, uint64_t id, void * data)
{
    assert (omap);
    assert (data);
    omap__t * cur, * tmp;

    for (   cur = (omap__t *) omap;
            cur->next_ && cur->next_->id_ < id;
            cur = cur->next_);

    if (cur->next_ && cur->next_->id_ == id)
    {
        void * prev = cur->next_->data_;
        cur->next_->data_ = data;
        return prev;
    }

    tmp = omap__new(id, data, cur->next_);
    if (!tmp)
        return NULL;

    omap->n++;
    cur->next_ = tmp;

    return data;
}

void * omap_get(omap_t * omap, uint64_t id)
{
    omap__t * cur = (omap__t *) omap;
    while ((cur = cur->next_) && cur->id_ < id);

    return cur && cur->id_ == id ? cur->data_ : NULL;
}

void * omap_rm(omap_t * omap, uint64_t id)
{
    omap__t * cur, * prev = (omap__t *) omap;
    while ((cur = prev->next_) && cur->id_ < id)
    {
        prev = cur;
    }

    return cur && cur->id_ == id ? omap__rm(omap, &prev->next_) : NULL;
}

static void * omap__rm(omap_t * omap, omap__t ** omap_)
{
    omap__t * cur = *omap_;
    void * data = cur->data_;
    *omap_ = cur->next_;

    free(cur);
    --omap->n;

    return data;
}

static omap__t * omap__new(uint64_t id, void * data, omap__t * next)
{
    omap__t * omap = malloc(sizeof(omap__t));
    if (!omap)
        return NULL;

    omap->id_ = id;
    omap->data_ = data;
    omap->next_ = next;

    return omap;
}
