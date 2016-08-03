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
static void IMAP64_free_node_cb(
        imap64_node_t * node,
        imap64_cb cb,
        void * args);
static int IMAP64_add(
        imap64_t * imap,
        imap64_node_t * node,
        uint64_t id,
        void * data,
        uint8_t overwrite);
static void * IMAP64_get(imap64_node_t * node, uint64_t id);
static void * IMAP64_pop(imap64_t * imap, imap64_node_t * node, uint64_t id);
static void IMAP64_walk(
        imap64_node_t * node,
        int * rc,
        imap64_cb cb,
        void * args);
static void IMAP64_walkn(
        imap64_node_t * node,
        size_t * n,
        imap64_cb cb,
        void * args);
static void IMAP64_2slist(imap64_node_t * node, slist_t * slist);

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
    if (imap->len)
    {
        for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (imap->nodes[i] != NULL)
            {
                IMAP64_free_node(imap->nodes[i]);
            }
        }
    }
    free(imap);
}

/*
 * Destroy imap64 using a call-back function. (parsing NULL is NOT allowed)
 */
void imap64_free_cb(imap64_t * imap, imap64_cb cb, void * args)
{
    if (imap->data != NULL)
    {
        (*cb)(imap->data, args);
    }

    if (imap->len)
    {
        for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (imap->nodes[i] != NULL)
            {
                IMAP64_free_node_cb(imap->nodes[i], cb, args);
            }
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
int imap64_add(imap64_t * imap, uint64_t id, void * data, uint8_t overwrite)
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
        else if (!overwrite)
        {
            return IMAP64_EXISTS;
        }

        imap->data = data;
        return IMAP64_OK;
    }

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (imap->nodes[key] == NULL)
    {
        node = imap->nodes[key] = IMAP64_new_node();
        if (node == NULL)
        {
            return IMAP64_ERR;
        }
    }
    else
    {
        node = imap->nodes[key];
    }

    if (id < IMAP64_NODE_SZ)
    {
        if (node->nodes[id] == NULL)
        {
            node->size++;
            node->nodes[id] = IMAP64_new_node();
            imap->len++;
        }
        else if (node->nodes[id]->data == NULL)
        {
            imap->len++;
        }
        else if (!overwrite)
        {
            return IMAP64_EXISTS;
        }

        node->nodes[id]->data = data;

        return IMAP64_OK;
    }

    return IMAP64_add(imap, node, id, data, overwrite);
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
        ((imap->nodes[key] == NULL || imap->nodes[key]->nodes[id] == NULL) ?
                NULL : imap->nodes[key]->nodes[id]->data) :

        ((imap->nodes[key] == NULL) ?
                NULL : IMAP64_get(imap->nodes[key], id));
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
        if (    imap->nodes[key] == NULL ||
                imap->nodes[key]->nodes[id] == NULL ||
                (data = imap->nodes[key]->nodes[id]->data) == NULL)
        {
            return NULL;
        }

        imap->len--;
        imap->nodes[key]->nodes[id]->data = NULL;

        if (!imap->nodes[key]->nodes[id]->size)
        {
            free(imap->nodes[key]->nodes[id]);
            imap->nodes[key]->nodes[id] = NULL;
            if (!--imap->nodes[key]->size)
            {
                free(imap->nodes[key]);
                imap->nodes[key] = NULL;
            }
        }
        return data;
    }
    else
    {
        if (imap->nodes[key] == NULL)
        {
            return NULL;
        }

        data = IMAP64_pop(imap, imap->nodes[key], id);

        if (!imap->nodes[key]->size)
        {
            free(imap->nodes[key]);
            imap->nodes[key] = NULL;
        }

        return data;
    }
}

/*
 * Run the call-back function on all items in the map.
 *
 * All the results are added together and are returned as the result of
 * this function.
 */
int imap64_walk(imap64_t * imap, imap64_cb cb, void * args)
{
    uint_fast8_t i;
    imap64_node_t * nd;
    int rc = 0;

    if (imap->data != NULL)
    {
        rc += (*cb)(imap->data, args);
    }

    if (imap->len)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if ((nd = imap->nodes[i]) != NULL)
            {
                if (nd->data != NULL)
                {
                    rc += (*cb)(nd->data, args);
                }
                if (nd->size)
                {
                    IMAP64_walk(nd, &rc, cb, args);
                }
            }
        }
    }

    return rc;
}

/*
 * Recursive function, call-back function will be called on each item.
 *
 * Walking stops either when the call-back is called on each value or
 * when 'n' is zero. 'n' will be decremented by the result of each call-back.
 */
void imap64_walkn(imap64_t * imap, size_t * n, imap64_cb cb, void * args)
{
    imap64_node_t * nd;

    if (!(*n))
    {
        return;
    }

    if (imap->data != NULL)
    {
        *n -= (*cb)(imap->data, args);
    }

    if (imap->len)
    {
        for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ && *n; i++)
        {
            if ((nd = imap->nodes[i]) != NULL)
            {
                if (nd->data != NULL)
                {
                    *n -= (*cb)(nd->data, args);
                }

                if (nd->size)
                {
                    IMAP64_walkn(nd, n, cb, args);
                }
            }
        }
    }
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
slist_t * imap64_2slist(imap64_t * imap)
{
    slist_t * slist = slist_new(imap->len);
    if (slist == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        imap64_node_t * nd;

        if (imap->data != NULL)
        {
            slist_append(slist, imap->data);
        }

        if (imap->len)
        {
            for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
            {
                if ((nd = imap->nodes[i]) != NULL)
                {
                    if (nd->data != NULL)
                    {
                        slist_append(slist, nd->data);
                    }

                    if (nd->size)
                    {
                        IMAP64_2slist(nd, slist);
                    }
                }
            }
        }
    }
    return slist;
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
    if (node->size)
    {
        for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (node->nodes[i] != NULL)
            {
                IMAP64_free_node(node->nodes[i]);
            }
        }
    }
    free(node);
}

/*
 * Destroy imap64. (parsing NULL is NOT allowed)
 */
static void IMAP64_free_node_cb(
        imap64_node_t * node,
        imap64_cb cb,
        void * args)
{
    if (node->data != NULL)
    {
        (*cb)(node->data, args);
    }

    if (node->size)
    {
        for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (node->nodes[i] != NULL)
            {
                IMAP64_free_node_cb(node->nodes[i], cb, args);
            }
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
        void * data,
        uint8_t overwrite)
{
    uint_fast8_t key;

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (node->nodes[key] == NULL)
    {
        node->size++;
        node = node->nodes[key] = IMAP64_new_node();
        if (node == NULL)
        {
            return IMAP64_ERR;
        }
    }
    else
    {
        node = node->nodes[key];
    }

    if (id < IMAP64_NODE_SZ)
    {
        if (node->nodes[id] == NULL)
        {
            node->size++;
            if ((node->nodes[id] = IMAP64_new_node()) == NULL)
            {
                return IMAP64_ERR;
            }
            imap->len++;
        }
        else if (node->nodes[id]->data == NULL)
        {
            imap->len++;
        }
        else if (!overwrite)
        {
            return IMAP64_EXISTS;
        }

        node->nodes[id]->data = data;

        return IMAP64_OK;
    }

    return IMAP64_add(imap, node, id, data, overwrite);
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
         ((node->nodes[key] == NULL || node->nodes[key]->nodes[id] == NULL) ?
                NULL : node->nodes[key]->nodes[id]->data) :

         ((node->nodes[key] == NULL) ?
                NULL : IMAP64_get(node->nodes[key], id));
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
        if (    node->nodes[key] == NULL ||
                node->nodes[key]->nodes[id] == NULL ||
                (data = node->nodes[key]->nodes[id]->data) == NULL)
        {
            return NULL;
        }

        imap->len--;

        node->nodes[key]->nodes[id]->data = NULL;

        if (!node->nodes[key]->nodes[id]->size)
        {
            free(node->nodes[key]->nodes[id]);
            node->nodes[key]->nodes[id] = NULL;
            if (!--node->nodes[key]->size)
            {
                free(node->nodes[key]);
                node->nodes[key] = NULL;
                node->size--;
            }
        }
        return data;
    }
    else
    {
        if (node->nodes[key] == NULL)
        {
            return NULL;
        }

        data = IMAP64_pop(imap, node->nodes[key], id);

        if (!node->nodes[key]->size)
        {
            free(node->nodes[key]);
            node->nodes[key] = NULL;
            node->size--;
        }

        return data;
    }
}

/*
 * Recursive function, call-back function will be called on each item.
 */
static void IMAP64_walk(
        imap64_node_t * node,
        int * rc,
        imap64_cb cb,
        void * args)
{
    imap64_node_t * nd;
    for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
    {
        if ((nd = node->nodes[i]) != NULL)
        {
            if (nd->data != NULL)
            {
                *rc += (*cb)(nd->data, args);
            }

            if (nd->size)
            {
                IMAP64_walk(nd, rc, cb, args);
            }
        }
    }
}

/*
 * Recursive function, call-back function will be called on each item.
 */
static void IMAP64_walkn(
        imap64_node_t * node,
        size_t * n,
        imap64_cb cb,
        void * args)
{
    imap64_node_t * nd;

    for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ && *n; i++)
    {
        if ((nd = node->nodes[i]) != NULL)
        {
            if (nd->data != NULL)
            {
                *n -= (*cb)(nd->data, args);
            }

            if (nd->size)
            {
                IMAP64_walkn(nd, n, cb, args);
            }
        }
    }
}

/*
 * Recursive function to fill a s-list object.
 */
static void IMAP64_2slist(imap64_node_t * node, slist_t * slist)
{
    imap64_node_t * nd;

    for (uint_fast8_t i = 0; i < IMAP64_NODE_SZ; i++)
    {
        if ((nd = node->nodes[i]) != NULL)
        {
            if (nd->data != NULL)
            {
                slist_append(slist, nd->data);
            }

            if (nd->size)
            {
                IMAP64_2slist(nd, slist);
            }
        }
    }
}
