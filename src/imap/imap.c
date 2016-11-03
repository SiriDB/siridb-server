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

#define IMAP_NODE_SZ 32

static void IMAP_node_free(imap_node_t * node);
static void IMAP_node_free_cb(imap_node_t * node, imap_free_cb cb);
static int IMAP_add(imap_node_t * node, uint64_t id, void * data);
static void * IMAP_get(imap_node_t * node, uint64_t id);
static void * IMAP_pop(imap_node_t * node, uint64_t id);
static void IMAP_walk(imap_node_t * node, imap_cb cb, void * data, int * rc);
static void IMAP_walkn(imap_node_t * node, imap_cb cb, void * data, size_t * n);
static void IMAP_2slist(imap_node_t * node, slist_t * slist);
static void IMAP_2slist_ref(imap_node_t * node, slist_t * slist);
static void IMAP_union_ref(imap_node_t * dest, imap_node_t * node);
static void IMAP_intersection_ref(
        imap_node_t * dest,
        imap_node_t * node,
        imap_free_cb decref_cb);
static void IMAP_difference_ref(
        imap_node_t * dest,
        imap_node_t * node,
        imap_free_cb decref_cb);
static void IMAP_symmetric_difference_ref(
        imap_node_t * dest,
        imap_node_t * node,
        imap_free_cb decref_cb);

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
        imap->slist = NULL;
    }
    return imap;
}

/*
 * Destroy imap with optional call-back function.
 */
void imap_free(imap_t * imap, imap_free_cb cb)
{
    if (imap->len)
    {
        imap_node_t * nd;

        if (cb == NULL)
        {
            for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
            {
                nd = imap->nodes + i;

                if (nd->nodes != NULL)
                {
                    IMAP_node_free(nd);
                }
            }
        }
        else
        {
            for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
            {
                nd = imap->nodes + i;

                if (nd->data != NULL)
                {
                    (*cb)(nd->data);
                }

                if (nd->nodes != NULL)
                {
                    IMAP_node_free_cb(nd, cb);
                }
            }
        }
    }

    slist_free(imap->slist);
    free(imap);
}



/*
 * Add data by id to the map.
 *
 * Returns 0 when data is overwritten and 1 if a new id/value is set.
 *
 * In case of an error we return -1 and a SIGNAL is raised.
 */
int imap_add(imap_t * imap, uint64_t id, void * data)
{
#ifdef DEBUG
    /* insert NULL is not allowed */
    assert (data != NULL);
#endif
    int rc;
    imap_node_t * nd = imap->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        rc = (nd->data == NULL);

        imap->len += rc;
        nd->data = data;
    }
    else
    {
        rc = IMAP_add(nd, id - 1, data);

        if (rc > 0)
        {
            imap->len++;
        }
    }

    if (imap->slist != NULL && (
            rc < 1 || slist_append_safe(&imap->slist, data)))
    {
        slist_free(imap->slist);
        imap->slist = NULL;
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

    if (id)
    {
        data = (nd->nodes == NULL) ? NULL : IMAP_pop(nd, id - 1);
    }
    else if ((data = nd->data) != NULL)
    {
        nd->data = NULL;
    }

    if (data != NULL)
    {
        imap->len--;

        if (imap->slist != NULL)
        {
            slist_free(imap->slist);
            imap->slist = NULL;
        }
    }

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
 *
 * When successful a BORROWED pointer to slist is returned.
 */
slist_t * imap_slist(imap_t * imap)
{
    if (imap->slist == NULL)
    {
        imap->slist = slist_new(imap->len);

        if (imap->slist != NULL && imap->len)
        {
            imap_node_t * nd;

            for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
            {
                nd = imap->nodes + i;

                if (nd->data != NULL)
                {
                    slist_append(imap->slist, nd->data);
                }

                if (nd->nodes != NULL)
                {
                    IMAP_2slist(nd, imap->slist);
                }
            }
        }
    }
    return imap->slist;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * When successful a the slist is returned and imap->slist is set to NULL.
 *
 * This can be used when being sure this is the only time you need the list
 * and prevents making a copy.
 */
slist_t * imap_slist_pop(imap_t * imap)
{
    slist_t * slist = imap_slist(imap);
    imap->slist = NULL;
    return slist;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * When successful a NEW slist is returned.
 */
slist_t * imap_2slist(imap_t * imap)
{
    slist_t * slist = imap_slist(imap);
    if (slist != NULL)
    {
        slist = slist_copy(slist);
    }
    return slist;
}

/*
 * Use this function to create a s-list copy and update the ref count
 * for each object. We expect each object to have object->ref (uint_xxx_t) on
 * top of the object definition.
 *
 * There is no function to handle the decrement for the ref count since they
 * are different for each object. Best is to handle the decrement while looping
 * over the returned list.
 *
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
slist_t * imap_2slist_ref(imap_t * imap)
{
    if (imap->slist == NULL)
    {
        imap->slist = slist_new(imap->len);

        if (imap->slist != NULL && imap->len)
        {
            imap_node_t * nd;

            for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
            {
                nd = imap->nodes + i;

                if (nd->data != NULL)
                {
                    slist_append(imap->slist, nd->data);
                    slist_object_incref(nd->data);
                }

                if (nd->nodes != NULL)
                {
                    IMAP_2slist_ref(nd, imap->slist);
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < imap->slist->len; i++)
        {
            slist_object_incref(imap->slist->data[i]);
        }
    }

    return (imap->slist == NULL) ? NULL : slist_copy(imap->slist);
}

/*
 * Map 'dest' will be the union between the two maps. Map 'imap' will be
 * destroyed so it cannot be used anymore.
 *
 * This function can call 'decref_cb' when an item is removed from the map.
 * We only call the function for sure when the item is removed from both maps.
 * When we are sure the item still exists in the 'dest' map and is only removed
 * from the 'imap', we simply decrement the ref counter.
 */
void imap_union_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb)
{
    if (dest->slist != NULL)
    {
        slist_free(dest->slist);
        dest->slist = NULL;
    }

    if (imap->len)
    {
        imap_node_t * dest_nd;
        imap_node_t * imap_nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            dest_nd = dest->nodes + i;
            imap_nd = imap->nodes + i;

            if (imap_nd->data != NULL)
            {
                if (dest_nd->data != NULL)
                {
#ifdef DEBUG
                    /* this must be the same object */
                    assert (imap_nd->data == dest_nd->data);
#endif
                    /* we are sure there is a ref left */
                    slist_object_decref(imap_nd->data);
                }
                else
                {
                    dest_nd->data = imap_nd->data;
                    dest->len++;
                }
            }

            if (imap_nd->nodes != NULL)
            {
                if (dest_nd->nodes != NULL)
                {
                    size_t tmp = dest_nd->size;
                    IMAP_union_ref(dest_nd, imap_nd);
                    dest->len += dest_nd->size - tmp;
                }
                else
                {
                    dest_nd->nodes = imap_nd->nodes;
                    dest_nd->size = imap_nd->size;
                    dest->len += dest_nd->size;
                }
            }
        }
    }

    /* cleanup source imap */
    slist_free(imap->slist);
    free(imap);
}

/*
 * Map 'dest' will be the intersection between the two maps. Map 'imap' will be
 * destroyed so it cannot be used anymore.
 *
 * This function can call 'decref_cb' when an item is removed from the map.
 * We only call the function for sure when the item is removed from both maps.
 * When we are sure the item still exists in the 'dest' map and is only removed
 * from the 'imap', we simply decrement the ref counter.
 */
void imap_intersection_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb)
{
    if (dest->slist != NULL)
    {
        slist_free(dest->slist);
        dest->slist = NULL;
    }

    imap_node_t * dest_nd;
    imap_node_t * imap_nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        dest_nd = dest->nodes + i;
        imap_nd = imap->nodes + i;
        if (imap_nd->data != NULL)
        {
            (*decref_cb)(imap_nd->data);
        }
        else if (dest_nd->data != NULL)
        {
            (*decref_cb)(dest_nd->data);
            dest_nd->data = NULL;
            dest->len--;
        }

        if (imap_nd->nodes != NULL)
        {
            if (dest_nd->nodes != NULL)
            {
                size_t tmp = dest_nd->size;
                IMAP_intersection_ref(dest_nd, imap_nd, decref_cb);
                dest->len -= tmp - dest_nd->size;
            }
            else
            {
                IMAP_node_free_cb(imap_nd, decref_cb);
            }
        }
        else if (dest_nd->nodes != NULL)
        {
            dest->len -= dest_nd->size;
            IMAP_node_free_cb(dest_nd, decref_cb);
            dest_nd->nodes = NULL;
        }
    }

    /* cleanup source imap */
    slist_free(imap->slist);
    free(imap);
}

/*
 * Map 'dest' will be the difference between the two maps. Map 'imap' will be
 * destroyed so it cannot be used anymore.
 *
 * This function can call 'decref_cb' when an item is removed from the map.
 * We only call the function for sure when the item is removed from both maps.
 * When we are sure the item still exists in the 'dest' map and is only removed
 * from the 'imap', we simply decrement the ref counter.
 */
void imap_difference_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb)
{
    if (dest->slist != NULL)
    {
        slist_free(dest->slist);
        dest->slist = NULL;
    }

    if (imap->len)
    {
        imap_node_t * dest_nd;
        imap_node_t * imap_nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            dest_nd = dest->nodes + i;
            imap_nd = imap->nodes + i;

            if (imap_nd->data != NULL)
            {
                if (dest_nd->data != NULL)
                {
#ifdef DEBUG
                    /* this must be the same object */
                    assert (imap_nd->data == dest_nd->data);
#endif
                    /* we are sure to have one ref left */
                    slist_object_decref(dest_nd->data);
                    dest_nd->data = NULL;
                    dest->len--;

                }
                /* now we are not sure anymore if we have ref left */
                (*decref_cb)(imap_nd->data);
            }

            if (imap_nd->nodes != NULL)
            {
                if (dest_nd->nodes != NULL)
                {
                    size_t tmp = dest_nd->size;
                    IMAP_difference_ref(dest_nd, imap_nd, decref_cb);
                    dest->len -= tmp - dest_nd->size;
                }
                else
                {
                    IMAP_node_free_cb(imap_nd, decref_cb);
                }
            }
        }
    }

    /* cleanup source imap */
    slist_free(imap->slist);
    free(imap);
}

/*
 * Map 'dest' will be the symmetric difference between the two maps. Map 'imap'
 * will be destroyed so it cannot be used anymore.
 *
 * This function can call 'decref_cb' when an item is removed from the map.
 * We only call the function for sure when the item is removed from both maps.
 * When we are sure the item still exists in the 'dest' map and is only removed
 * from the 'imap', we simply decrement the ref counter.
 */
void imap_symmetric_difference_ref(
        imap_t * dest,
        imap_t * imap,
        imap_free_cb decref_cb)
{
    if (dest->slist != NULL)
    {
        slist_free(dest->slist);
        dest->slist = NULL;
    }

    if (imap->len)
    {
        imap_node_t * dest_nd;
        imap_node_t * imap_nd;

        for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
        {
            dest_nd = dest->nodes + i;
            imap_nd = imap->nodes + i;

            if (imap_nd->data != NULL)
            {
                if (dest_nd->data != NULL)
                {
#ifdef DEBUG
                    /* this must be the same object */
                    assert (imap_nd->data == dest_nd->data);
#endif
                    /* we are sure to have one ref left */
                    slist_object_decref(dest_nd->data);

                    /* but now we are not sure anymore */
                    (*decref_cb)(imap_nd->data);

                    dest_nd->data = NULL;
                    dest->len--;
                }
                else
                {
                    dest_nd->data = imap_nd->data;
                    dest->len++;
                }
            }

            if (imap_nd->nodes != NULL)
            {
                if (dest_nd->nodes != NULL)
                {
                    size_t tmp = dest_nd->size;
                    IMAP_symmetric_difference_ref(
                            dest_nd,
                            imap_nd,
                            decref_cb);
                    dest->len += dest_nd->size - tmp;
                }
                else
                {
                    dest_nd->nodes = imap_nd->nodes;
                    dest_nd->size = imap_nd->size;
                    dest->len += dest_nd->size;
                }
            }
        }
    }

    /* cleanup source imap */
    slist_free(imap->slist);
    free(imap);
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

static void IMAP_node_free_cb(imap_node_t * node, imap_free_cb cb)
{
    imap_node_t * nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        nd = node->nodes + i;

        if (nd->data != NULL)
        {
            (*cb)(nd->data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_node_free_cb(nd, cb);
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

static void IMAP_2slist_ref(imap_node_t * node, slist_t * slist)
{
    imap_node_t * nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        nd = node->nodes + i;

        if (nd->data != NULL)
        {
            slist_append(slist, nd->data);
            slist_object_incref(nd->data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_2slist_ref(nd, slist);
        }
    }
}

static void IMAP_union_ref(imap_node_t * dest, imap_node_t * node)
{
    imap_node_t * dest_nd;
    imap_node_t * node_nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        dest_nd = dest->nodes + i;
        node_nd = node->nodes + i;

        if (node_nd->data != NULL)
        {
            if (dest_nd->data != NULL)
            {
#ifdef DEBUG
                /* this must be the same object */
                assert (node_nd->data == dest_nd->data);
#endif
                /* we are sure there is a ref left */
                slist_object_decref(node_nd->data);
            }
            else
            {
                dest_nd->data = node_nd->data;
                dest->size++;
            }
        }

        if (node_nd->nodes != NULL)
        {
            if (dest_nd->nodes != NULL)
            {
                size_t tmp = dest_nd->size;
                IMAP_union_ref(dest_nd, node_nd);
                dest->size += dest_nd->size - tmp;
            }
            else
            {
                dest_nd->nodes = node_nd->nodes;
                dest_nd->size = node_nd->size;
                dest->size += dest_nd->size;
            }
        }
    }
    free(node->nodes);
}

static void IMAP_intersection_ref(
        imap_node_t * dest,
        imap_node_t * node,
        imap_free_cb decref_cb)
{
    imap_node_t * dest_nd;
    imap_node_t * node_nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        dest_nd = dest->nodes + i;
        node_nd = node->nodes + i;

        if (node_nd->data != NULL)
        {
            (*decref_cb)(node_nd->data);
        }
        else if (dest_nd->data != NULL)
        {
            (*decref_cb)(dest_nd->data);
            dest_nd->data = NULL;
            dest->size--;
        }

        if (node_nd->nodes != NULL)
        {
            if (dest_nd->nodes != NULL)
            {
                size_t tmp = dest_nd->size;
                IMAP_intersection_ref(dest_nd, node_nd, decref_cb);
                dest->size -= tmp - dest_nd->size;
            }
            else
            {
                IMAP_node_free_cb(node_nd, decref_cb);
            }
        }
        else if (dest_nd->nodes != NULL)
        {
            dest->size -= dest_nd->size;
            IMAP_node_free_cb(dest_nd, decref_cb);
            dest_nd->nodes = NULL;
        }

    }

    if (!dest->size)
    {
    	IMAP_node_free(dest);
    	dest->nodes = NULL;
    }

    free(node->nodes);
}

static void IMAP_difference_ref(
        imap_node_t * dest,
        imap_node_t * node,
        imap_free_cb decref_cb)
{
    imap_node_t * dest_nd;
    imap_node_t * node_nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        dest_nd = dest->nodes + i;
        node_nd = node->nodes + i;

        if (node_nd->data != NULL)
        {
            if (dest_nd->data != NULL)
            {
#ifdef DEBUG
                /* this must be the same object */
                assert (node_nd->data == dest_nd->data);
#endif
                /* we are sure to have one ref left */
                slist_object_decref(dest_nd->data);
                dest_nd->data = NULL;
                dest->size--;

            }
            /* now we are not sure anymore if we have ref left */
            (*decref_cb)(node_nd->data);
        }

        if (node_nd->nodes != NULL)
        {
            if (dest_nd->nodes != NULL)
            {
                size_t tmp = dest_nd->size;
                IMAP_difference_ref(dest_nd, node_nd, decref_cb);
                dest->size -= tmp - dest_nd->size;
            }
            else
            {
                IMAP_node_free_cb(node_nd, decref_cb);
            }
        }
    }

    if (!dest->size)
    {
    	IMAP_node_free(dest);
    	dest->nodes = NULL;
    }

    free(node->nodes);
}

static void IMAP_symmetric_difference_ref(
        imap_node_t * dest,
        imap_node_t * node,
        imap_free_cb decref_cb)
{
    imap_node_t * dest_nd;
    imap_node_t * node_nd;

    for (uint8_t i = 0; i < IMAP_NODE_SZ; i++)
    {
        dest_nd = dest->nodes + i;
        node_nd = node->nodes + i;

        if (node_nd->data != NULL)
        {
            if (dest_nd->data != NULL)
            {
#ifdef DEBUG
                /* this must be the same object */
                assert (node_nd->data == dest_nd->data);
#endif
                /* we are sure to have one ref left */
                slist_object_decref(dest_nd->data);

                /* but now we are not sure anymore */
                (*decref_cb)(node_nd->data);

                dest_nd->data = NULL;
                dest->size--;
            }
            else
            {
                dest_nd->data = node_nd->data;
                dest->size++;
            }
        }

        if (node_nd->nodes != NULL)
        {
            if (dest_nd->nodes != NULL)
            {
                size_t tmp = dest_nd->size;
                IMAP_symmetric_difference_ref(
                        dest_nd,
                        node_nd,
                        decref_cb);
                dest->size += dest_nd->size - tmp;
            }
            else
            {
                dest_nd->nodes = node_nd->nodes;
                dest_nd->size = node_nd->size;
                dest->size += dest_nd->size;
            }
        }
    }

    if (!dest->size)
    {
    	IMAP_node_free(dest);
    	dest->nodes = NULL;
    }

    free(node->nodes);
}
