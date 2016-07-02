/*
 * imap64.c - map for uint64_t integer keys
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-03-2016
 *
 */
#include <stdio.h>
#include <imap64/imap64.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <logger/logger.h>
#include <siri/err.h>

static imap64_node_t * IMAP64_new_node(void);
static void IMAP64_free_node(imap64_node_t * node);
static int IMAP64_add(
        imap64_t * imap,
        imap64_node_t * node,
        uint64_t id,
        void * data);
static void * IMAP64_get(imap64_node_t * node, uint64_t id);
static void * IMAP64_pop(imap64_t * imap, imap64_node_t * node, uint64_t id);
static void IMAP64_walk(imap64_node_t * node, imap64_cb_t cb, void * args);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
imap64_t * imap64_new(void)
{
    imap64_t * imap = (imap64_t *) calloc(1, sizeof(imap64_t));
    if (imap == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        imap->len = 0;
        imap->data = NULL;
    }
    return imap;
}

/*
 * Destroy imap64. (parsing NULL is NOT allowed)
 */
void imap64_free(imap64_t * imap)
{
    uint_fast8_t i;

    if (imap->len)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (imap->node[i] == NULL)
            {
                continue;
            }
            IMAP64_free_node(imap->node[i]);
        }
    }
    free(imap);
}

/*
 * Add data by id to the map.
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 *
 * Warning: existing data will be overwritten!
 */
int imap64_add(imap64_t * imap, uint64_t id, void * data)
{
    uint_fast8_t key;
    imap64_node_t * node;

#ifdef DEBUG
    assert (data != NULL);
#endif

    if (!id)
    {
        if (imap->data == NULL)
        {
            imap->len++;
        }
        imap->data = data;
        return 0;
    }

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (imap->node[key] == NULL)
    {
        node = imap->node[key] = IMAP64_new_node();
    }
    else
    {
        node = imap->node[key];
    }

    if (id < IMAP64_NODE_SZ)
    {
        if (node->node[id] == NULL)
        {
            node->size++;
            node->node[id] = IMAP64_new_node();
            imap->len++;
        }
        else if (node->node[id]->data == NULL)
        {
            imap->len++;
        }

        node->node[id]->data = data;
    }
    else
    {
        if (IMAP64_add(imap, node, id, data))
        {
            return -1;
        }
    }

    return 0;
}

/*
 * Returns data by a given id, or NULL when not found.
 */
void * imap64_get(imap64_t * imap, uint64_t id)
{
    uint_fast8_t key;

    if (!id)
    {
        return imap->data;
    }

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    return (id < IMAP64_NODE_SZ) ?
        ((imap->node[key] == NULL || imap->node[key]->node[id] == NULL) ?
                NULL : imap->node[key]->node[id]->data) :

        ((imap->node[key] == NULL) ?
                NULL : IMAP64_get(imap->node[key], id));
}

/*
 * Remove and return an item by id or return NULL in case the id is not found.
 */
void * imap64_pop(imap64_t * imap, uint64_t id)
{
    void * data;
    uint_fast8_t key;

    if (!id)
    {
        if ((data = imap->data) == NULL)
        {
            return NULL;
        }

        imap->len--;
        imap->data = NULL;
        return data;
    }

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (id < IMAP64_NODE_SZ)
    {
        if (    imap->node[key] == NULL ||
                imap->node[key]->node[id] == NULL ||
                (data = imap->node[key]->node[id]->data) == NULL)
        {
            return NULL;
        }

        imap->len--;
        imap->node[key]->node[id]->data = NULL;

        if (!imap->node[key]->node[id]->size)
        {
            free(imap->node[key]->node[id]);
            imap->node[key]->node[id] = NULL;
            if (!--imap->node[key]->size)
            {
                free(imap->node[key]);
                imap->node[key] = NULL;
            }
        }
        return data;
    }
    else
    {
        if (imap->node[key] == NULL)
        {
            return NULL;
        }

        data = IMAP64_pop(imap, imap->node[key], id);

        if (!imap->node[key]->size)
        {
            free(imap->node[key]);
            imap->node[key] = NULL;
        }

        return data;
    }
}

/*
 * Run the call-back function on all items in the map.
 */
void imap64_walk(imap64_t * imap, imap64_cb_t cb, void * args)
{
    uint_fast8_t i;

    if (imap->data != NULL)
    {
        (*cb)(imap->data, args);
    }

    if (imap->len)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (imap->node[i] == NULL)
            {
                continue;
            }
            IMAP64_walk(imap->node[i], cb, args);
        }
    }
}

/*
 * Return a new node or raise a SIGNAL and return NULL in case of an error.
 */
static imap64_node_t * IMAP64_new_node(void)
{
    imap64_node_t * node = (imap64_node_t *) calloc(1, sizeof(imap64_node_t));
    if (node == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        node->size = 0;
        node->data = NULL;
    }
    return node;
}

/*
 * Destroy imap64. (parsing NULL is NOT allowed)
 */
static void IMAP64_free_node(imap64_node_t * node)
{
    uint_fast8_t i;

    if (node->size)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (node->node[i] == NULL)
            {
                continue;
            }
            IMAP64_free_node(node->node[i]);
        }
    }
    free(node);
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
static int IMAP64_add(
        imap64_t * imap,
        imap64_node_t * node,
        uint64_t id,
        void * data)
{
    uint_fast8_t key;

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (node->node[key] == NULL)
    {
        node->size++;
        node = node->node[key] = IMAP64_new_node();
        if (node == NULL)
        {
            return -1;
        }
    }
    else
    {
        node = node->node[key];
    }

    if (id < IMAP64_NODE_SZ)
    {
        if (node->node[id] == NULL)
        {
            node->size++;
            if ((node->node[id] = IMAP64_new_node()) == NULL)
            {
                return -1;
            }
            imap->len++;
        }
        else if (node->node[id]->data == NULL)
        {
            imap->len++;
        }

        node->node[id]->data = data;
    }
    else
    {
        if (IMAP64_add(imap, node, id, data))
        {
            return -1;
        }
    }

    return 0;
}

/*
 * Return an item by id or NULL when not found.
 */
static void * IMAP64_get(imap64_node_t * node, uint64_t id)
{
    uint_fast8_t key;

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    return (id < IMAP64_NODE_SZ) ?
         ((node->node[key] == NULL || node->node[key]->node[id] == NULL) ?
                NULL : node->node[key]->node[id]->data) :

         ((node->node[key] == NULL) ?
                NULL : IMAP64_get(node->node[key], id));
}

/*
 * Remove and return an item by id or return NULL in case the id is not found.
 */
static void * IMAP64_pop(imap64_t * imap, imap64_node_t * node, uint64_t id)
{
    void * data;
    uint_fast8_t key;

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (id < IMAP64_NODE_SZ)
    {
        if (    node->node[key] == NULL ||
                node->node[key]->node[id] == NULL ||
                (data = node->node[key]->node[id]->data) == NULL)
        {
            return NULL;
        }

        imap->len--;

        node->node[key]->node[id]->data = NULL;

        if (!node->node[key]->node[id]->size)
        {
            free(node->node[key]->node[id]);
            node->node[key]->node[id] = NULL;
            if (!--node->node[key]->size)
            {
                free(node->node[key]);
                node->node[key] = NULL;
                node->size--;
            }
        }
        return data;
    }
    else
    {
        if (node->node[key] == NULL)
        {
            return NULL;
        }

        data = IMAP64_pop(imap, node->node[key], id);

        if (!node->node[key]->size)
        {
            free(node->node[key]);
            node->node[key] = NULL;
            node->size--;
        }

        return data;
    }
}

/*
 * Recursive function, call-back function will be called on each item.
 */
static void IMAP64_walk(imap64_node_t * node, imap64_cb_t cb, void * args)
{
    uint_fast8_t i;

    if (node->data != NULL)
    {
        (*cb)(node->data, args);
    }

    if (node->size)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (node->node[i] == NULL)
            {
                continue;
            }
            IMAP64_walk(node->node[i], cb, args);
        }
    }
}
