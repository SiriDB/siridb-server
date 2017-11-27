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

typedef struct llist_s llist_t;
typedef struct llist_node_s llist_node_t;

#include <stdio.h>
#include <slist/slist.h>

struct llist_node_s
{
    void * data;
    llist_node_t * next;
};

struct llist_s
{
    size_t len;
    llist_node_t * first;
    llist_node_t * last;
};

typedef int (*llist_cb)(void * data, void * args);

llist_t * llist_new(void);
void llist_free_cb(llist_t * llist, llist_cb cb, void * args);
int llist_append(llist_t * llist, void * data);
int llist_walk(llist_t * llist, llist_cb cb, void * args);
void llist_walkn(llist_t * llist, size_t * n, llist_cb cb, void * args);
slist_t * llist2slist(llist_t * llist);
void * llist_get(llist_t * llist, llist_cb cb, void * args);
void * llist_remove(llist_t * llist, llist_cb cb, void * args);
void * llist_pop(llist_t * llist);
void * llist_shift(llist_t * llist);
