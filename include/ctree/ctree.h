/*
 * ctree.h - Compact Binary Tree implementation.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-03-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <stddef.h>

enum
{
    CT_CRITICAL=-2,
    CT_EXISTS,
    CT_OK
};

/* ct_get_sure() will set a pointer to CT_EMPTY and returns the address so
 * it can be used for custom data. We do not use NULL since we take NULL as if
 * the node does not exist. */
extern char * CT_EMPTY;

typedef struct ct_node_s * ct_nodes_t[256];

typedef struct ct_node_s
{
    char * key;
    void * data;
    uint_fast8_t size;
    ct_nodes_t * nodes;
} ct_node_t;

typedef void (*ct_cb_t)(const char * key, void * data, void * args);
typedef void (*ct_free_cb_t)(void * data);

/* create a new compact binary tree */
ct_node_t * ct_new(void);

/* call ct_free when finished using ct */
void ct_free(ct_node_t * node);

/* the callback will be called before freeing the node. this can be used
 * to perform some action on the data when freeing the c-tree. */
void ct_free_cb(ct_node_t * node, ct_free_cb_t cb);

/* can be used to check if get_sure has set an CT_EMPTY */
extern int ct_is_empty(void * data);

/* gets a value or set an CT_EMPTY if the key is not there. the address
 * is returned and can be used to set a new value. */
void ** ct_get_sure(ct_node_t * node, const char * key);

/* add a new key/value. return CT_EXISTS if the key already
 * exists and CT_OK if not. when the key exists the value will not
 * be overwritten. */
int ct_add(ct_node_t * node, const char * key, void * data);

/* return the value or NULL if the key does not exist. */
void * ct_get(ct_node_t * node, const char * key);

/* remove a key/value and return the value. NULL will be returned if
 * the key did not exist. */
void * ct_pop(ct_node_t * node, const char * key);

void ct_walk(ct_node_t * node, ct_cb_t cb, void * args);
void ct_walkn(ct_node_t * node, size_t n, ct_cb_t cb, void * args);
