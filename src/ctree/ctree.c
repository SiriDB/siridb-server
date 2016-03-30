/*
 * ctree.c - Compact Binary Tree implementation.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-03-2016
 *
 */
#include <ctree/ctree.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

/* initial buffer size, this is not fixed but can grow if needed */
#define CT_BUFFER_ALLOC_SIZE 256

static ct_node_t * new_node(const char * key, void * data);
static int pv_add(ct_node_t * node, const char * key, void * data);
static void * pv_get(ct_node_t * node, const char * key);
static void * pv_pop(ct_node_t * parent, ct_node_t ** nd, const char * key);
static void ** pv_get_sure(ct_node_t * node, const char * key);
static void pv_dec_node(ct_node_t * node);
static void pv_merge_node(ct_node_t * node);
static void pv_walk(
        ct_node_t * node,
        size_t len,
        size_t buffer_sz,
        char * buffer,
        ct_cb_t cb,
        void * args);

ct_empty_t ct_empty = {};
ct_empty_t * CT_EMPTY = &ct_empty;

ct_node_t * ct_new(void)
{
    ct_node_t * node = (ct_node_t *) malloc(sizeof(ct_node_t));
    node->data = NULL;
    node->key = NULL;
    node->size = 0;
    node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
    return node;
}

void ct_free(ct_node_t * node)
{
    int i = 255;
    if (node->nodes != NULL)
    {
        while (i--)
            if ((*node->nodes)[i] != NULL)
                ct_free((*node->nodes)[i]);
    }
    free(node->nodes);
    free(node->key);
    free(node);
}

int ct_is_empty(void * data)
{
    return data == CT_EMPTY;
}

void ** ct_get_sure(ct_node_t * node, const char * key)
{
    ct_node_t ** nd;

    nd = &(*node->nodes)[(uint_fast8_t) *key];

    if (*nd != NULL)
        return pv_get_sure(*nd, key + 1);

    *nd = new_node(key + 1, CT_EMPTY);

    return &(*nd)->data;
}

int ct_add(ct_node_t * node, const char * key, void * data)
{
    ct_node_t ** nd;

    nd = &(*node->nodes)[(uint_fast8_t) *key];

    if (*nd != NULL)
        return pv_add(*nd, key + 1, data);

    *nd = new_node(key + 1, data);

    return CT_OK;
}

void * ct_get(ct_node_t * node, const char * key)
{
    ct_node_t * nd;

    nd = (*node->nodes)[(uint_fast8_t) *key];

    return (nd == NULL) ? NULL : pv_get(nd, key + 1);
}

void * ct_pop(ct_node_t * node, const char * key)
{
    ct_node_t ** nd;

    nd = &(*node->nodes)[(uint_fast8_t) *key];

    return (*nd == NULL) ? NULL : pv_pop(NULL, nd, key + 1);
}

void ct_walk(ct_node_t * node, ct_cb_t cb, void * args)
{
    size_t buffer_sz = CT_BUFFER_ALLOC_SIZE;
    size_t len = 1;
    ct_node_t * nd;
    char * buffer = (char *) malloc(buffer_sz);
    for (*buffer = 255; (*buffer)--;)
    {
        if ((nd = (*node->nodes)[(uint_fast8_t) *buffer]) == NULL)
            continue;
        pv_walk(nd, len, buffer_sz, buffer, cb, args);
    }
}

static void pv_walk(
        ct_node_t * node,
        size_t len,
        size_t buffer_sz,
        char * buffer,
        ct_cb_t cb,
        void * args)
{
    size_t sz = strlen(node->key);
    char * pt;
    ct_node_t * nd;

    if (sz + len + 1 > buffer_sz)
    {
        buffer_sz =
                ((sz + len) / CT_BUFFER_ALLOC_SIZE + 1) * CT_BUFFER_ALLOC_SIZE;
        buffer = (char *) realloc(buffer, buffer_sz);
    }

    memcpy(buffer + len, node->key, sz + 1);

    if (node->data != NULL)
        (*cb)(buffer, node->data);

    if (node->nodes != NULL)
    {
        len += sz + 1;
        pt = buffer + len - 1;

        for (*pt = 255; (*pt)--;)
        {
            if ((nd = (*node->nodes)[(uint_fast8_t) *pt]) == NULL)
                continue;
            pv_walk(nd, len, buffer_sz, buffer, cb, args);
        }
    }
}

static int pv_add(ct_node_t * node, const char * key, void * data)
{
    char * pt = node->key;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
            {
                if (node->data != NULL)
                {
                    /* duplicate key */
                    return CT_EXISTS;
                }
                node->data = data;

                return CT_OK;
            }

            if (node->nodes == NULL)
            {
                node->size = 1;
                node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
                (*node->nodes)[(uint_fast8_t) *key] =
                        new_node(key + 1, data);

                return CT_OK;
            }

            ct_node_t ** nd = &(*node->nodes)[(uint_fast8_t) *key];

            if (*nd != NULL)
                return pv_add(*nd, key + 1, data);

            node->size++;
            *nd = new_node(key + 1, data);

            return CT_OK;
        }

        if (*key != *pt)
        {
            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));

            /* create new nodes with rest of node pt */
            ct_node_t * nd = (*new_nodes)[(uint_fast8_t) *pt] =
                    new_node(pt + 1, node->data);

            /* bind the -rest- of current node to the new nodes */
            nd->nodes = node->nodes;
            nd->size = node->size;

            /* the current nodes should become the new nodes */
            node->nodes = new_nodes;

            if (*key == 0)
            {
                node->size = 1;
                /* end of our key, store data in this node */
                node->data = data;
            }
            else
            {
                /* we have more, make sure data for this node is NULL and
                 * add rest of our key to the nodes.
                 */
                node->size = 2;
                node->data = NULL;
                (*new_nodes)[(uint_fast8_t) *key] =
                        new_node(key + 1, data);
            }

            /* write terminator */
            *pt = 0;

            /* re-allocate the key to free some space */
            node->key = (char *) realloc(node->key, pt - node->key + 1);

            return CT_OK;
        }
    }

    /* we should never get here */
    return CT_CRITICAL;
}

static void * pv_get(ct_node_t * node, const char * key)
{
    char * pt = node->key;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
                return node->data;

            if (node->nodes == NULL)
                return NULL;

            ct_node_t * nd = (*node->nodes)[(uint_fast8_t) *key];

            return (nd == NULL) ? NULL : pv_get(nd, key + 1);
        }
        if (*key != *pt)
            return NULL;
    }

    /* we should never get here */
    return NULL;
}

static void ** pv_get_sure(ct_node_t * node, const char * key)
{
    char * pt = node->key;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
            {
                if (node->data == NULL)
                    node->data = CT_EMPTY;

                return &node->data;
            }

            if (node->nodes == NULL)
            {
                node->size = 1;
                node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
                return &((*node->nodes)[(uint_fast8_t) *key] =
                        new_node(key + 1, CT_EMPTY))->data;
            }

            ct_node_t ** nd = &(*node->nodes)[(uint_fast8_t) *key];

            if (*nd != NULL)
                return pv_get_sure(*nd, key + 1);

            node->size++;

            *nd = new_node(key + 1, CT_EMPTY);

            return &(*nd)->data;
        }

        if (*key != *pt)
        {
            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));

            /* bind the -rest- of current node to the new nodes */
            ct_node_t * nd = (*new_nodes)[(uint_fast8_t) *pt] =
                    new_node(pt + 1, node->data);

            nd->size = node->size;
            nd->nodes = node->nodes;

            /* the current nodes should become the new nodes */
            node->nodes = new_nodes;

            /* write terminator */
            *pt = 0;

            /* re-allocate the key to free some space */
            node->key = (char *) realloc(node->key, pt - node->key + 1);

            if (*key == 0)
            {
                node->size = 1;
                node->data = CT_EMPTY;
                return &node->data;
            }

            node->size = 2;
            node->data = NULL;
            return &((*new_nodes)[(uint_fast8_t) *key] =
                    new_node(key + 1, CT_EMPTY))->data;
        }
    }

    /* we should never get here */
    return NULL;
}

static void pv_merge_node(ct_node_t * node)
{
    /* this function should only be called when 1 node is left and the
     * node itself has no data
     */
    assert(node->size == 1 && node->data == NULL);

    int i;
    size_t len_key = strlen(node->key);
    size_t len_child_key;
    ct_node_t * child_node;

    for (i = 0; i < 256; i++)
        if ((*node->nodes)[i] != NULL)
            break;
    /* this is the child node we need to merge */
    child_node = (*node->nodes)[i];

    /* we need the length of the child key as well */
    len_child_key = strlen(child_node->key);

    /* re-allocate enough space for the key + child_key */
    node->key = (char *) realloc(node->key, len_key + len_child_key + 2);

    /* set node char */
    node->key[len_key] = i;

    /* append rest of the child key */
    memcpy(node->key + len_key + 1, child_node->key, len_child_key + 1);

    /* free nodes (has only the child node left so nothing else
     * needs cleaning */
    free(node->nodes);

    /* bind child nodes and size to current node */
    node->nodes = child_node->nodes;
    node->size = child_node->size;

    /* free child key */
    free(child_node->key);

    /* free child node */
    free(child_node);
}

static void pv_dec_node(ct_node_t * node)
{
    if (node == NULL)
        /* this is the root node */
        return;

    node->size--;

    if (node->size == 0)
    {
        /* we can free nodes since they are no longer used */
        free(node->nodes);

        /* make sure to set nodes to NULL */
        node->nodes = NULL;
    }
    else if (node->size == 1 && node->data == NULL)
    {
        pv_merge_node(node);
    }
}

static void * pv_pop(ct_node_t * parent, ct_node_t ** nd, const char * key)
{
    char * pt = (*nd)->key;
    ct_node_t * node = *nd;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
            {
                void * data = node->data;
                if ((*nd)->size == 0)
                {
                    /* no child nodes, lets clean up this node */
                    ct_free(node);

                    /* make sure to set the node to NULL so the parent
                     * can do its cleanup correctly */
                    *nd = NULL;

                    /* size of parent should be minus one */
                    pv_dec_node(parent);

                    return data;
                }
                /* we cannot clean this node, set data to NULL */
                node->data = NULL;

                if (node->size == 1)
                    /* we have only one child, we can merge this
                     * child with this one */
                    pv_merge_node(*nd);

                return data;
            }

            if (node->nodes == NULL)
                /* nothing to pop */
                return NULL;

            ct_node_t ** next = &(*node->nodes)[(uint_fast8_t) *key];

            return (*next == NULL) ? NULL : pv_pop(node, next, key + 1);
        }
        if (*key != *pt)
            /* nothing to pop */
            return NULL;
    }

    /* we should never get here */
    return NULL;
}

static ct_node_t * new_node(const char * key, void * data)
{
    size_t size = strlen(key) + 1;
    ct_node_t * node = (ct_node_t *) malloc(sizeof(ct_node_t));

    node->key = (char *) malloc(size);
    memcpy(node->key, key, size);

    node->data = data;
    node->size = 0;
    node->nodes = NULL;

    return node;
}
