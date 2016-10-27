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
extern void * CT_EMPTY;

typedef struct ct_node_s * ct_nodes_t[32];

typedef struct ct_node_s
{
    uint8_t offset;
    uint8_t n;
    uint8_t size;
    uint8_t pad0;
    uint32_t pad1;
    ct_nodes_t * nodes;
    char * key;
    void * data;
} ct_node_t;

typedef struct ct_s
{
    uint8_t offset;
    uint8_t n;
    uint16_t pad0;
    uint32_t len;
    ct_nodes_t * nodes;
} ct_t;

typedef int (*ct_item_cb)(const char * key, void * data, void * args);
typedef int (*ct_val_cb)(void * data, void * args);
typedef void (*ct_free_cb)(void * data);

ct_t * ct_new(void);
void ct_free(ct_t * ct, ct_free_cb cb);
void ** ct_get_sure(ct_t * ct, const char * key);
int ct_add(ct_t * ct, const char * key, void * data);
void * ct_get(ct_t * node, const char * key);
void ** ct_getaddr(ct_t * ct, const char * key);
void * ct_getn(ct_t * ct, const char * key, size_t n);
void * ct_pop(ct_t * ct, const char * key);
int ct_items(ct_t * ct, ct_item_cb cb, void * args);
int ct_itemsn(ct_t * ct, size_t * n, ct_item_cb cb, void * args);
int ct_values(ct_t * ct, ct_val_cb cb, void * args);
void ct_valuesn(ct_t * ct, size_t * n, ct_val_cb cb, void * args);

/*
 * Can be used to check if ct_get_sure() has set an CT_EMPTY
 */
#define ct_is_empty(data) data == CT_EMPTY
