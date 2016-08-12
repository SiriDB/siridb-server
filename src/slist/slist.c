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

#include <slist/slist.h>
#include <stddef.h>
#include <stdlib.h>
#include <logger/logger.h>
#include <siri/err.h>
#include <string.h>

#define SLIST_AUTO_GROW_SIZE 8

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
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
        ERR_ALLOC
    }
    else
    {
        slist->size = size;
        slist->len = 0;
    }
    return slist;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
slist_t * slist_copy(slist_t * source)
{
    size_t size = sizeof(slist_t) + sizeof(void *) * source->size;
    slist_t * slist = (slist_t *) malloc(size);
    if (slist == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        memcpy(slist, source, size);
    }
    return slist;
}

/*
 * Destroy the simple list.
 */
inline void slist_free(slist_t * slist)
{
    free(slist);
}

/*
 * Append data to the list. This functions assumes the list can hold the new
 * data is therefore not safe.
 */
inline void slist_append(slist_t * slist, void * data)
{
    slist->data[slist->len++] = data;
}

/*
 * Pop the last item from the list
 */
inline void * slist_pop(slist_t * slist)
{
    return slist->data[--slist->len];
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

        /* increment size */
        (*slist)->size += SLIST_AUTO_GROW_SIZE;
        tmp = (slist_t *) realloc(
                *slist,
                sizeof(slist_t) + sizeof(void *) * (*slist)->size);

        if (tmp == NULL)
        {
            /* an error has occurred */
            (*slist)->size -= SLIST_AUTO_GROW_SIZE;
            return -1;
        }

        /* overwrite the original value with the new one */
        *slist = tmp;
    }

    (*slist)->data[(*slist)->len++] = data;

    return 0;
}
