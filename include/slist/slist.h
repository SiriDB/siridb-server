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

#define SLIST_DEFAULT_SIZE 6

typedef struct slist_object_s
{
    uint16_t ref;
} slist_object_t;

typedef struct slist_s
{
    size_t size;
    size_t len;
    void * data[];
} slist_t;

slist_t * slist_new(size_t size);
void slist_free(slist_t * slist);
void slist_append(slist_t * slist, void * data);
int slist_append_safe(slist_t ** slist, void * data);

/*
 * Expects the object to have a object->ref (uint16_t) on top of the
 * objects definition.
 */
#define slist_object_incref(object) ((slist_object_t * ) object)->ref++

/*
 * Expects the object to have a object->ref (uint16_t) on top of the
 * objects definition.
 *
 * WARNING: Be careful using 'slist_object_decref' only when being sure
 *          there are still references left on the object since an object
 *          probably needs specific cleanup tasks.
 */
#define slist_object_decref(object) ((slist_object_t * ) object)->ref--
