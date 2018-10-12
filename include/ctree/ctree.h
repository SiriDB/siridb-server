/*
 * ctree.h - Compact Binary Tree implementation.
 */
#ifndef CTREE_H_
#define CTREE_H_

enum
{
    CT_ERR=-1,
    CT_OK,
    CT_EXISTS,
};

typedef struct ct_s ct_t;
typedef struct ct_node_s ct_node_t;
typedef struct ct_node_s * ct_nodes_t[32];

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

typedef int (*ct_item_cb)(
        const char * key,
        size_t len,
        void * data,
        void * args);
typedef int (*ct_val_cb)(void * data, void * args);
typedef void (*ct_free_cb)(void * data);

ct_t * ct_new(void);
void ct_free(ct_t * ct, ct_free_cb cb);
int ct_add(ct_t * ct, const char * key, void * data);
void * ct_get(ct_t * node, const char * key);
void ** ct_getaddr(ct_t * ct, const char * key);
void * ct_getn(ct_t * ct, const char * key, size_t n);
void * ct_pop(ct_t * ct, const char * key);
int ct_items(ct_t * ct, ct_item_cb cb, void * args);
int ct_values(ct_t * ct, ct_val_cb cb, void * args);
void ct_valuesn(ct_t * ct, size_t * n, ct_val_cb cb, void * args);

struct ct_node_s
{
    uint8_t offset;
    uint8_t n;
    uint8_t size;
    uint8_t pad0;
    uint32_t len;
    ct_nodes_t * nodes;
    char * key;
    void * data;
};

struct ct_s
{
    uint8_t offset;
    uint8_t n;
    uint16_t pad0;
    uint32_t len;
    ct_nodes_t * nodes;
};

#endif  /* CTREE_H_ */
