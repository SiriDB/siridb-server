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

slist_t * slist_new(size_t size)
{
    /* sizeof(slist_t) is 16 bytes, only for len and size and data[] is
     * excluded.
     */
    slist_t * slist = (slist_t *)
            malloc(sizeof(slist_t) + sizeof(void *) * size);
    slist->size = size;
    slist->len = 0;
    return slist;
}

void slist_free(slist_t * slist)
{
    free(slist);
}

void slist_append(slist_t * slist, void * data)
{
    slist->data[slist->len] = data;
    slist->len++;
}

void slist_append_save(slist_t ** slist, void * data)
{
    if ((*slist)->len == (*slist)->size)
    {
        (*slist)->size++;
        *slist = realloc(
                *slist,
                sizeof(slist_t) + sizeof(void *) * (*slist)->size);
    }
    (*slist)->data[(*slist)->len] = data;
    (*slist)->len++;
}

