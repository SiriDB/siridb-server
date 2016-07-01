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

/*
 * Returns NULL and raises a signal in case an error has occurred.
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
        return NULL;
    }

    slist->size = size;
    slist->len = 0;

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
void slist_append(slist_t * slist, void * data)
{
    slist->data[slist->len] = data;
    slist->len++;
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
        (*slist)->size++;

        tmp = realloc(
                *slist,
                sizeof(slist_t) + sizeof(void *) * (*slist)->size);

        if (tmp == NULL)
        {
            /* an error has occurred */
            (*slist)->size--;
            return -1;
        }

        /* overwrite the original value with the new one */
        *slist = tmp;
    }

    (*slist)->data[(*slist)->len] = data;
    (*slist)->len++;

    return 0;
}

