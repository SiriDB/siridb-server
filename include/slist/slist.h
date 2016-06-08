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

typedef struct slist_s
{
    size_t size;
    size_t len;
    void * data[];
} slist_t;

slist_t * slist_new(size_t size);
void slist_append(slist_t * slist, void * data);
void slist_free(slist_t * slist);
