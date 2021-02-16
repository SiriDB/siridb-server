/*
 * imap.h - Lookup map for uint64_t integer keys with set operation support.
 */
#ifndef IMAP_H_
#define IMAP_H_

typedef struct imap_node_s imap_node_t;
typedef struct imap_s imap_t;

#include <inttypes.h>
#include <stddef.h>
#include <vec/vec.h>

typedef int (*imap_cb)(void * data, void * args);
typedef void (*imap_free_cb)(void * data);

typedef void (*imap_update_cb)(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb);

imap_t * imap_new(void);
void imap_free(imap_t * imap, imap_free_cb cb);
int imap_set(imap_t * imap, uint64_t id, void * data);
int imap_add(imap_t * imap, uint64_t id, void * data);
void * imap_get(imap_t * imap, uint64_t id);
void * imap_pop(imap_t * imap, uint64_t id);
int imap_walk(imap_t * imap, imap_cb cb, void * data);
void imap_walkn(imap_t * imap, size_t * n, imap_cb cb, void * data);
vec_t * imap_vec(imap_t * imap);
vec_t * imap_vec_pop(imap_t * imap);
vec_t * imap_2vec(imap_t * imap);
vec_t * imap_2vec_ref(imap_t * imap);
void imap_union_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb);
void imap_intersection_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb);
void imap_difference_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb);
void imap_symmetric_difference_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb);


struct imap_node_s
{
    uint32_t size;
    uint8_t key;
    uint8_t pad8;
    uint16_t pad16;
    void * data ;
    imap_node_t * nodes;
};

struct imap_s
{
    size_t len;
    vec_t * vec;
    imap_node_t nodes[];
};

#endif  /* IMAP_H_ */
