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

slist_t * slist_new(ssize_t size)
{
    slist_t * slist = (slist_t *)
            malloc(sizeof(slist_t) + sizeof(void *) * size);
    slist->size = size;
    slist->len = 0;
    slist->data = NULL;
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


