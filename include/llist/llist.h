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
#include <stdio.h>
#include <slist/slist.h>

typedef struct slist_s slist_t;

typedef struct llist_node_s
{
    void * data;
    struct llist_node_s * next;
} llist_node_t;

typedef struct llist_s
{
    size_t len;
    llist_node_t * first;
    llist_node_t * last;
} llist_t;

typedef int (*llist_cb_t)(void * data, void * args);

/* create a new llist */
llist_t * llist_new(void);

/* destroy llist */
void llist_free_cb(llist_t * llist, llist_cb_t cb, void * args);

/* append data at the end */
void llist_append(llist_t * llist, void * data);

/* insert data at the beginning */
void llist_insert(llist_t * llist, void * data);

/* walk the list */
void llist_walk(llist_t * llist, llist_cb_t cb, void * args);

/* walk the list with limit */
void llist_walkn(llist_t * llist, size_t n, llist_cb_t cb, void * args);

/* copy the linked list to a simple list */
slist_t * llist2slist(llist_t * llist);

/* return the first item where 'cb' is not zero or NULL if not found */
void * llist_get(llist_t * llist, llist_cb_t cb, void * args);

/* remove and return the first item where 'cb' is not zero
 * or NULL if not found
 */
void * llist_pop(llist_t * llist, llist_cb_t cb, void * args);