/*
 * imap.c - map for uint64_t integer keys
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 03-08-2016
 *
 */
#include <assert.h>
#include <imap/imap.h>
#include <logger/logger.h>
#include <siri/err.h>
#include <stdio.h>
#include <stdlib.h>

#define IMAP_NODE_SZ 16

static void IMAP_node_free(imap_node_t * node);
static void IMAP_node_free_cb(imap_node_t * node, imap_cb cb, void * data);
static int IMAP_add(imap_node_t * node, uint64_t id, void * data);
static void * IMAP_get(imap_node_t * node, uint64_t id);
static void * IMAP_pop(imap_node_t * node, uint64_t id);
static void IMAP_walk(imap_node_t * node, imap_cb cb, void * data, int * rc);
static void IMAP_walkn(imap_node_t * node, imap_cb cb, void * data, size_t * n);
static void IMAP_2slist(imap_node_t * node, slist_t * slist);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
imap_t * imap_new(void)
{
    imap_t * imap = (imap_t *) calloc(
            1,
            sizeof(imap_t) + IMAP_NODE_SZ * sizeof(imap_node_t));
    if (imap == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        imap->len = 0;
    }
    return imap;
}

/*
 * Destroy imap. (parsing NULL is NOT allowed)
 */
void imap_free(imap_t * imap)
{
    if (imap->len)
    {
        imap_node_t * nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            if ((nd = imap->nodes + i)->nodes != NULL)
            {
                IMAP_node_free(nd);
            }
        }
    }
    free(imap);
}

/*
 * Destroy imap using a call-back function. (parsing NULL is NOT allowed)
 */
void imap_free_cb(imap_t * imap, imap_cb cb, void * data)
{
    if (imap->len)
    {
        imap_node_t * nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            nd = imap->nodes + i;

            if (nd->data != NULL)
            {
                (*cb)(nd->data, data);
            }

            if (nd->nodes != NULL)
            {
                IMAP_node_free_cb(nd, cb, data);
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
int imap_add(imap_t * imap, uint64_t id, void * data)
{
#ifdef DEBUG
    /* insert NULL is not allowed */
    assert (data != NULL);
#endif

    imap_node_t * nd = imap->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        if (nd->data == NULL)
        {
            imap->len++;
        }

        nd->data = data;

        return 0;
    }

    int rc = IMAP_add(nd, id - 1, data);

    if (rc > 0)
    {
        imap->len++;
    }

    return rc;
}

/*
 * Returns data by a given id, or NULL when not found.
 */
void * imap_get(imap_t * imap, uint64_t id)
{
    imap_node_t * nd = imap->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        return nd->data;
    }

    return (nd->nodes == NULL) ? NULL : IMAP_get(nd, id - 1);
}

/*
 * Remove and return an item by id or return NULL in case the id is not found.
 */
void * imap_pop(imap_t * imap, uint64_t id)
{
    void * data;
    imap_node_t * nd = imap->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        if ((data = nd->data) != NULL)
        {
            nd->data = NULL;
            imap->len--;
        }

        return data;
    }

    data = (nd->nodes == NULL) ? NULL : IMAP_pop(nd, id - 1);
    imap->len -= (data != NULL);

    return data;
}

/*
 * Run the call-back function on all items in the map.
 *
 * All the results are added together and are returned as the result of
 * this function.
 */
int imap_walk(imap_t * imap, imap_cb cb, void * data)
{
    int rc = 0;

    if (imap->len)
    {
        imap_node_t * nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            nd = imap->nodes + i;

            if (nd->data != NULL)
            {
                rc += (*cb)(nd->data, data);
            }

            if (nd->nodes != NULL)
            {
                IMAP_walk(nd, cb, data, &rc);
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
void imap_walkn(imap_t * imap, size_t * n, imap_cb cb, void * data)
{
    if (imap->len)
    {
        imap_node_t * nd;

        for (uint8_t i = 0; *n && i < IMAP_NODE_SZ; i++)
        {
            nd = imap->nodes + i;

            if (nd->data != NULL && !(*n -= (*cb)(nd->data, data)))
            {
                return;
            }

            if (nd->nodes != NULL)
            {
                IMAP_walkn(nd, cb, data, n);
            }
        }
    }
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
slist_t * imap_2slist(imap_t * imap)
{
    slist_t * slist = slist_new(imap->len);

    if (slist == NULL)
    {
        ERR_ALLOC
    }
    else if (imap->len)
    {
        imap_node_t * nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            nd = imap->nodes + i;

            if (nd->data != NULL)
            {
                slist_append(slist, nd->data);
            }

            if (nd->nodes != NULL)
            {
                IMAP_2slist(nd, slist);
            }
        }
    }
    return slist;
}

static void IMAP_node_free(imap_node_t * node)
{
    imap_node_t * nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        if ((nd = node->nodes + i)->nodes != NULL)
        {
            IMAP_node_free(nd);
        }
    }

    free(node->nodes);
}

static void IMAP_node_free_cb(imap_node_t * node, imap_cb cb, void * data)
{
    imap_node_t * nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        nd = node->nodes + i;

        if (nd->data != NULL)
        {
            (*cb)(nd->data, data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_node_free_cb(nd, cb, data);
        }
    }
    free(node->nodes);
}

/*
 * Add data by id to the map.
 *
 * Returns 0 when data is overwritten and 1 if a new id/value is set.
 *
 * In case of an error we return -1 and a SIGNAL is raised.
 */
static int IMAP_add(imap_node_t * node, uint64_t id, void * data)
{
    if (!node->size)
    {
        node->nodes = (imap_node_t *) calloc(
                IMAP_NODE_SZ,
                sizeof(imap_node_t));

        if (node->nodes == NULL)
        {
            ERR_ALLOC
            return -1;
        }
    }

    int rc;
    imap_node_t * nd = node->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        rc = (nd->data == NULL);

        nd->data = data;
        node->size += rc;

        return rc;
    }

    rc = IMAP_add(nd, id - 1, data);

    if (rc > 0)
    {
        node->size++;
    }

    return rc;
}

static void * IMAP_get(imap_node_t * node, uint64_t id)
{
    imap_node_t * nd = node->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        return nd->data;
    }

    return (nd->nodes == NULL) ? NULL : IMAP_get(nd, id - 1);
}

static void * IMAP_pop(imap_node_t * node, uint64_t id)
{
    void * data;
    imap_node_t * nd = node->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        if ((data = nd->data) != NULL)
        {
            if (--node->size)
            {
                nd->data = NULL;
            }
            else
            {
                free(node->nodes);
                node->nodes = NULL;
            }
        }

        return data;
    }

    data = (nd->nodes == NULL) ? NULL : IMAP_pop(nd, id - 1);

    if (data != NULL && !--node->size)
    {
        free(node->nodes);
        node->nodes = NULL;
    }

    return data;
}

static void IMAP_walk(imap_node_t * node, imap_cb cb, void * data, int * rc)
{
    imap_node_t * nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        nd = node->nodes + i;

        if (nd->data != NULL)
        {
            *rc += (*cb)(nd->data, data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_walk(nd, cb, data, rc);
        }
    }
}

static void IMAP_walkn(imap_node_t * node, imap_cb cb, void * data, size_t * n)
{
    imap_node_t * nd;

    for (uint8_t i = 0; *n && i < IMAP_NODE_SZ; i++)
    {
        nd = node->nodes + i;

        if (nd->data != NULL && !(*n -= (*cb)(nd->data, data)))
        {
             return;
        }

        if (nd->nodes != NULL)
        {
            IMAP_walkn(nd, cb, data, n);
        }
    }
}

static void IMAP_2slist(imap_node_t * node, slist_t * slist)
{
    imap_node_t * nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        nd = node->nodes + i;

        if (nd->data != NULL)
        {
            slist_append(slist, nd->data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_2slist(nd, slist);
        }
    }
}
