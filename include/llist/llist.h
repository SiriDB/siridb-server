/*
 * llist.h - Linked List implementation.
 */
#ifndef LLIST_H_
#define LLIST_H_

typedef struct llist_s llist_t;
typedef struct llist_node_s llist_node_t;

#include <stdio.h>
#include <vec/vec.h>

typedef int (*llist_cb)(void * data, void * args);

llist_t * llist_new(void);
void llist_free_cb(llist_t * llist, llist_cb cb, void * args);
int llist_append(llist_t * llist, void * data);
int llist_walk(llist_t * llist, llist_cb cb, void * args);
void llist_walkn(llist_t * llist, size_t * n, llist_cb cb, void * args);
vec_t * llist2vec(llist_t * llist);
void * llist_get(llist_t * llist, llist_cb cb, void * args);
void * llist_remove(llist_t * llist, llist_cb cb, void * args);
void * llist_pop(llist_t * llist);
void * llist_shift(llist_t * llist);

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

#endif  /* LLIST_H_ */
