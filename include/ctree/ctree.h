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
#include <stdio.h>

enum
{
    CT_ERR=-1,
    CT_OK,
    CT_EXISTS,
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

typedef struct ct_s
{
    size_t len;
    ct_nodes_t * nodes;
} ct_t;

typedef int (*ct_cb_t)(const char * key, void * data, void * args);
typedef void (*ct_free_cb_t)(void * data);

ct_t * ct_new(void);
void ct_free(ct_t * ct);
void ct_free_cb(ct_t * ct, ct_free_cb_t cb);
extern int ct_is_empty(void * data);
void ** ct_get_sure(ct_t * ct, const char * key);
int ct_add(ct_t * ct, const char * key, void * data);
void * ct_get(ct_t * node, const char * key);
void * ct_pop(ct_t * ct, const char * key);
void ct_walk(ct_t * ct, ct_cb_t cb, void * args);
void ct_walkn(ct_t * ct, size_t * n, ct_cb_t cb, void * args);
