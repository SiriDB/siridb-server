/*
 * llist.h - Linked List
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 03-05-2016
 *
 */
#pragma once

#include <llist/private/llist.h>

typedef struct llist_node_s llist_node_t;

typedef struct llist_s
{
    size_t len;
    llist_node_t * first;
    llist_node_t * last;
} llist_t;

/* create a new llist */
llist_t * llist_new(void);

/* destroy llist */
void llist_free(llist_t * llist);

/* append data at the end */
void llist_append(llist_t * llist, void * data);

/* insert data at the beginning */
void llist_insert(llist_t * llist, void * data);

