/*
 * slist.h - Simple List
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 06-06-2016
 *
 */
#pragma once

#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>

#define SLIST_DEFAULT_SIZE 8

typedef struct slist_object_s
{
    uint32_t ref;
} slist_object_t;

typedef struct slist_s
{
    size_t size;
    size_t len;
    void * data[];
} slist_t;

slist_t * slist_new(size_t size);
slist_t * slist_copy(slist_t * source);
void slist_compact(slist_t ** slist);
int slist_append_safe(slist_t ** slist, void * data);

/*
 * Expects the object to have a object->ref (uint_xxx_t) on top of the
 * objects definition.
 */
#define slist_object_incref(object) ((slist_object_t * ) object)->ref++

/*
 * Expects the object to have a object->ref (uint_xxx_t) on top of the
 * objects definition.
 *
 * WARNING: Be careful using 'slist_object_decref' only when being sure
 *          there are still references left on the object since an object
 *          probably needs specific cleanup tasks.
 */
#define slist_object_decref(object) ((slist_object_t * ) object)->ref--

/*
 * Append data to the list. This functions assumes the list can hold the new
 * data is therefore not safe.
 */
#define slist_append(slist, _data) slist->data[slist->len++] = _data

/*
 * Destroy the simple list.
 */
#define slist_free(slist) free(slist)

/*
 * Pop the last item from the list
 */
#define slist_pop(slist) slist->data[--slist->len]
