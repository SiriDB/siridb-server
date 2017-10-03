/*
 * slist.c - Simple List
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 06-06-2016
 *
 */
#include <logger/logger.h>
#include <slist/slist.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SLIST_MAX_SZ 512

/*
 * Returns NULL in case an error has occurred.
 *
 * In case the size is unknown, SLIST_DEFAULT_SIZE is recommended since in this
 * case we can do a re-allocation with multiples of 64K.
 */
slist_t * slist_new(size_t size)
{
    /* sizeof(slist_t) is 16 bytes, only for len and size and data[] is
     * excluded.
     */
    slist_t * slist = (slist_t *)
            malloc(sizeof(slist_t) + sizeof(void *) * size);

    if (slist == NULL)
    {
        return NULL;
    }
    slist->size = size;
    slist->len = 0;
    return slist;
}

/*
 * Returns NULL in case an error has occurred.
 */
slist_t * slist_copy(slist_t * source)
{
    size_t size = sizeof(slist_t) + sizeof(void *) * source->size;
    slist_t * slist = (slist_t *) malloc(size);
    if (slist == NULL)
    {
        return NULL;
    }
    memcpy(slist, source, size);
    return slist;
}

/*
 * Returns 0 if successful or -1 in case of an error.
 * (in case of an error the list is unchanged)
 */
int slist_append_safe(slist_t ** slist, void * data)
{
    if ((*slist)->len == (*slist)->size)
    {
        slist_t * tmp;

        size_t sz = (*slist)->size;

        /* double the size when > 0 and <  SLIST_MAX_SZ */
        (*slist)->size = (sz >= SLIST_DEFAULT_SIZE) ?
                (sz <= SLIST_MAX_SZ) ?
                        sz * 2 : sz + SLIST_MAX_SZ : SLIST_DEFAULT_SIZE;

        tmp = (slist_t *) realloc(
                *slist,
                sizeof(slist_t) + sizeof(void *) * (*slist)->size);

        if (tmp == NULL)
        {
            /* an error has occurred */
            (*slist)->size = sz;
            return -1;
        }

        /* overwrite the original value with the new one */
        *slist = tmp;
    }

    slist_append((*slist), data);

    return 0;
}

/*
 * Compact memory used for the slist object when the list has more than
 * SLIST_DEFAULT_SIZE free space. After the compact the list has
 * a size which is SLIST_DEFAULT_SIZE greater than its length.
 */
void slist_compact(slist_t ** slist)
{
    size_t sz = (*slist)->size;

    if (sz - (*slist)->len > SLIST_DEFAULT_SIZE)
    {
        slist_t * tmp;

        (*slist)->size = (*slist)->len + SLIST_DEFAULT_SIZE;

        tmp = (slist_t *) realloc(
                *slist,
                sizeof(slist_t) + sizeof(void *) * (*slist)->size);

        if (tmp == NULL)
        {
            /* an error has occurred; log and restore size */
            log_error("Error has occurred while re-allocating less space");
            (*slist)->size = sz;
        }
        else
        {
            /* overwrite the original value with the new one */
            *slist = tmp;
        }
    }
}
