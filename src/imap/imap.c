/*
 * imap.c - Lookup map for uint64_t integer keys with set operation support.
 */
#include <assert.h>
#include <imap/imap.h>
#include <logger/logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAP_NODE_SZ 32

static void IMAP_node_free(imap_node_t * node);
static void IMAP_node_free_cb(imap_node_t * node, imap_free_cb cb);
static int IMAP_set(imap_node_t * node, uint64_t id, void * data);
static int IMAP_add(imap_node_t * node, uint64_t id, void * data);
static void * IMAP_pop(imap_node_t * node, uint64_t id);
static void IMAP_walk(imap_node_t * node, imap_cb cb, void * data, int * rc);
static void IMAP_walkn(imap_node_t * node, imap_cb cb, void * data, size_t * n);
static void IMAP_2vec(imap_node_t * node, vec_t * vec);
static void IMAP_2vec_ref(imap_node_t * node, vec_t * vec);
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

static imap_node_t IMAP_empty_node = {
        .data = NULL,
        .nodes = NULL,
        .size = 0,
};

static inline uint8_t IMAP_node_size(imap_node_t * node)
{
    return node->key == IMAP_NODE_SZ ? IMAP_NODE_SZ : 1;
}

static inline imap_node_t * IMAP_unsafe_node(imap_node_t * node, uint8_t key)
{
    return node->key == IMAP_NODE_SZ
            ? node->nodes + key
            : node->nodes;
}

static inline imap_node_t * IMAP_get_node(imap_node_t * node, uint8_t key)
{
    return node->key == IMAP_NODE_SZ
            ? node->nodes + key
            : node->key == key ? node->nodes : NULL;
}

static inline imap_node_t * IMAP_ensure_node(imap_node_t * node, uint8_t key)
{
    return node->key == IMAP_NODE_SZ
            ? node->nodes + key
            : node->key == key ? node->nodes : &IMAP_empty_node;
}

static inline int IMAP_node_grow(imap_node_t * node)
{
    imap_node_t * tmp = calloc(IMAP_NODE_SZ, sizeof(imap_node_t));
    if (!tmp)
    {
        return -1;
    }
    memcpy(tmp + node->key, node->nodes, sizeof(imap_node_t));
    free(node->nodes);
    node->nodes = tmp;
    node->key = IMAP_NODE_SZ;
    return 0;
}

/*
 * Returns NULL in case an error has occurred.
 */
imap_t * imap_new(void)
{
    imap_t * imap = (imap_t *) calloc(
            1,
            sizeof(imap_t) + IMAP_NODE_SZ * sizeof(imap_node_t));
    if (imap == NULL)
    {
        return NULL;
    }

    imap->len = 0;
    imap->vec = NULL;

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
        uint_fast8_t i;

        if (cb == NULL)
        {
            for (i = 0; i < IMAP_NODE_SZ; i++)
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
            for (i = 0; i < IMAP_NODE_SZ; i++)
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

    vec_free(imap->vec);
    free(imap);
}

/*
 * Add data by id to the map.
 *
 * Returns 0 when data is overwritten and 1 if a new id/value is set.
 *
 * In case of an error we return -1.
 */
int imap_set(imap_t * imap, uint64_t id, void * data)
{
    /* insert NULL is not allowed */
    assert (data != NULL);
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
        rc = IMAP_set(nd, id - 1, data);

        if (rc > 0)
        {
            imap->len++;
        }
    }

    if (imap->vec != NULL && (
            rc < 1 || vec_append_safe(&imap->vec, data)))
    {
        vec_free(imap->vec);
        imap->vec = NULL;
    }

    return rc;
}

/*
 * Add data by id to the map.
 *
 * Returns 0 when data is added. Data will NOT be overwritten.
 *
 * In case of a memory error we return -1. When the id already exists -2 will
 * be returned.
 */
int imap_add(imap_t * imap, uint64_t id, void * data)
{
    /* insert NULL is not allowed */
    assert (data != NULL);

    imap_node_t * nd = imap->nodes + (id % IMAP_NODE_SZ);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        if (nd->data != NULL)
        {
            return -2;
        }

        imap->len++;
        nd->data = data;
    }
    else
    {
        int rc = IMAP_add(nd, id - 1, data);
        if (rc)
        {
            return rc;
        }
        imap->len++;
    }

    if (imap->vec != NULL && vec_append_safe(&imap->vec, data))
    {
        vec_free(imap->vec);
        imap->vec = NULL;
    }

    return 0;
}

/*
 * Returns data by a given id, or NULL when not found.
 */
void * imap_get(imap_t * imap, uint64_t id)
{
    imap_node_t * nd = imap->nodes + (id % IMAP_NODE_SZ);
    do
    {
        id /= IMAP_NODE_SZ;

        if (!id) return nd->data;
        if (!nd->nodes) return NULL;

        nd = IMAP_get_node(nd, --id % IMAP_NODE_SZ);
    }
    while (nd);

    return NULL;
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

        if (imap->vec != NULL)
        {
            vec_free(imap->vec);
            imap->vec = NULL;
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
        uint_fast8_t i;

        for (i = 0; i < IMAP_NODE_SZ; i++)
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
        uint_fast8_t i;

        for (i = 0; *n && i < IMAP_NODE_SZ; i++)
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
 * Returns NULL in case an error has occurred.
 *
 * When successful a BORROWED pointer to vec is returned.
 */
vec_t * imap_vec(imap_t * imap)
{
    if (imap->vec == NULL)
    {
        imap->vec = vec_new(imap->len);

        if (imap->vec != NULL && imap->len)
        {
            imap_node_t * nd;
            uint_fast8_t i;

            for (i = 0; i < IMAP_NODE_SZ; i++)
            {
                nd = imap->nodes + i;

                if (nd->data != NULL)
                {
                    vec_append(imap->vec, nd->data);
                }

                if (nd->nodes != NULL)
                {
                    IMAP_2vec(nd, imap->vec);
                }
            }
        }
    }
    return imap->vec;
}

/*
 * Returns NULL in case an error has occurred.
 *
 * When successful a the vec is returned and imap->vec is set to NULL.
 *
 * This can be used when being sure this is the only time you need the list
 * and prevents making a copy.
 */
vec_t * imap_vec_pop(imap_t * imap)
{
    vec_t * vec = imap_vec(imap);
    imap->vec = NULL;
    return vec;
}

/*
 * Returns NULL in case an error has occurred.
 *
 * When successful a NEW vec is returned.
 */
vec_t * imap_2vec(imap_t * imap)
{
    vec_t * vec = imap_vec(imap);
    if (vec != NULL)
    {
        vec = vec_copy(vec);
    }
    return vec;
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
 * Returns NULL in case an error has occurred.
 */
vec_t * imap_2vec_ref(imap_t * imap)
{
    if (imap->vec == NULL)
    {
        imap->vec = vec_new(imap->len);

        if (imap->vec != NULL && imap->len)
        {
            imap_node_t * nd;
            uint_fast8_t i;

            for (i = 0; i < IMAP_NODE_SZ; i++)
            {
                nd = imap->nodes + i;

                if (nd->data != NULL)
                {
                    vec_append(imap->vec, nd->data);
                    vec_object_incref(nd->data);
                }

                if (nd->nodes != NULL)
                {
                    IMAP_2vec_ref(nd, imap->vec);
                }
            }
        }
    }
    else
    {
        size_t i;
        for (i = 0; i < imap->vec->len; i++)
        {
            vec_object_incref(imap->vec->data[i]);
        }
    }

    return (imap->vec == NULL) ? NULL : vec_copy(imap->vec);
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
        imap_free_cb decref_cb __attribute__((unused)))
{
    if (dest->vec != NULL)
    {
        vec_free(dest->vec);
        dest->vec = NULL;
    }

    if (imap->len)
    {
        imap_node_t * dest_nd;
        imap_node_t * imap_nd;
        uint_fast8_t i;

        for (i = 0; i < IMAP_NODE_SZ; i++)
        {
            dest_nd = dest->nodes + i;
            imap_nd = imap->nodes + i;

            if (imap_nd->data != NULL)
            {
                if (dest_nd->data != NULL)
                {
                    /* this must be the same object */
                    assert (imap_nd->data == dest_nd->data);
                    /* we are sure there is a ref left */
                    vec_object_decref(imap_nd->data);
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
                    dest_nd->key = imap_nd->key;
                    dest->len += dest_nd->size;
                }
            }
        }
    }

    /* cleanup source imap */
    vec_free(imap->vec);
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
    if (dest->vec != NULL)
    {
        vec_free(dest->vec);
        dest->vec = NULL;
    }

    imap_node_t * dest_nd;
    imap_node_t * imap_nd;
    uint_fast8_t i;

    for (i = 0; i < IMAP_NODE_SZ; i++)
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
            dest_nd->size = 0;
        }
    }

    /* cleanup source imap */
    vec_free(imap->vec);
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
    if (dest->vec != NULL)
    {
        vec_free(dest->vec);
        dest->vec = NULL;
    }

    if (imap->len)
    {
        imap_node_t * dest_nd;
        imap_node_t * imap_nd;
        uint_fast8_t i;

        for (i = 0; i < IMAP_NODE_SZ; i++)
        {
            dest_nd = dest->nodes + i;
            imap_nd = imap->nodes + i;

            if (imap_nd->data != NULL)
            {
                if (dest_nd->data != NULL)
                {
                    /* this must be the same object */
                    assert (imap_nd->data == dest_nd->data);

                    /* we are sure to have one ref left */
                    vec_object_decref(dest_nd->data);
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
    vec_free(imap->vec);
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
    if (dest->vec != NULL)
    {
        vec_free(dest->vec);
        dest->vec = NULL;
    }

    if (imap->len)
    {
        imap_node_t * dest_nd;
        imap_node_t * imap_nd;
        uint_fast8_t i;

        for (i = 0; i < IMAP_NODE_SZ; i++)
        {
            dest_nd = dest->nodes + i;
            imap_nd = imap->nodes + i;

            if (imap_nd->data != NULL)
            {
                if (dest_nd->data != NULL)
                {
                    /* this must be the same object */
                    assert (imap_nd->data == dest_nd->data);

                    /* we are sure to have one ref left */
                    vec_object_decref(dest_nd->data);

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
                    dest_nd->key = imap_nd->key;
                    dest->len += dest_nd->size;
                }
            }
        }
    }

    /* cleanup source imap */
    vec_free(imap->vec);
    free(imap);
}

static void IMAP_node_free(imap_node_t * node)
{
    imap_node_t * nd = node->nodes, * end = nd + IMAP_node_size(node);
    do
    {
        if (nd->nodes)
        {
            IMAP_node_free(nd);
        }
    }
    while (++nd < end);

    free(node->nodes);
}

static void IMAP_node_free_cb(imap_node_t * node, imap_free_cb cb)
{
    imap_node_t * nd = node->nodes, * end = nd + IMAP_node_size(node);
    do
    {
        if (nd->data)
        {
            (*cb)(nd->data);
        }

        if (nd->nodes)
        {
            IMAP_node_free_cb(nd, cb);
        }
    }
    while (++nd < end);

    free(node->nodes);
}

/*
 * Add data by id to the map.
 *
 * Returns 0 when data is overwritten and 1 if a new id/value is set.
 *
 * In case of an error we return -1.
 */
static int IMAP_set(imap_node_t * node, uint64_t id, void * data)
{
    int rc;
    uint8_t key = id % IMAP_NODE_SZ;
    imap_node_t * nd;

    if (!node->size)
    {
        node->nodes = calloc(1, sizeof(imap_node_t));
        if (!node->nodes)
        {
            return -1;
        }
        node->key = key;
    }
    else if (node->key != key &&
             node->key != IMAP_NODE_SZ &&
             IMAP_node_grow(node))
    {
        return -1;
    }

    nd = IMAP_unsafe_node(node, key);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        rc = (nd->data == NULL);

        nd->data = data;
        node->size += rc;

        return rc;
    }

    rc = IMAP_set(nd, id - 1, data);

    if (rc > 0)
    {
        node->size++;
    }

    return rc;
}

/*
 * Add data by id to the map.
 *
 * Returns 0 when data is added. Data will NOT be overwritten.
 *
 * In case of a memory error we return -1. If the id
 * already exists -2 will be returned.
 */
static int IMAP_add(imap_node_t * node, uint64_t id, void * data)
{
    int rc;
    uint8_t key = id % IMAP_NODE_SZ;
    imap_node_t * nd;

    if (!node->size)
    {
        node->nodes = calloc(1, sizeof(imap_node_t));
        if (!node->nodes)
        {
            return -1;
        }
        node->key = key;
    }
    else if (node->key != key &&
             node->key != IMAP_NODE_SZ &&
             IMAP_node_grow(node))
    {
        return -1;
    }

    nd = IMAP_unsafe_node(node, key);
    id /= IMAP_NODE_SZ;

    if (!id)
    {
        if (nd->data != NULL)
        {
            return -2;
        }

        nd->data = data;
        node->size++;

        return 0;
    }

    rc = IMAP_add(nd, id - 1, data);

    if (rc == 0)
    {
        node->size++;
    }

    return rc;
}

static void * IMAP_pop(imap_node_t * node, uint64_t id)
{
    void * data;
    imap_node_t * nd = IMAP_get_node(node, id % IMAP_NODE_SZ);
    if (!nd)
    {
        return NULL;
    }

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
    imap_node_t * nd = node->nodes, * end = nd + IMAP_node_size(node);

    do
    {
        if (nd->data != NULL)
        {
            *rc += (*cb)(nd->data, data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_walk(nd, cb, data, rc);
        }
    }
    while (++nd < end);
}

static void IMAP_walkn(imap_node_t * node, imap_cb cb, void * data, size_t * n)
{
    imap_node_t * nd = node->nodes, * end = nd + IMAP_node_size(node);

    for (; *n && nd < end; ++nd)
    {
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

static void IMAP_2vec(imap_node_t * node, vec_t * vec)
{
    imap_node_t * nd = node->nodes, * end = nd + IMAP_node_size(node);

    do
    {
        if (nd->data != NULL)
        {
            vec_append(vec, nd->data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_2vec(nd, vec);
        }
    }
    while (++nd < end);
}

static void IMAP_2vec_ref(imap_node_t * node, vec_t * vec)
{
    imap_node_t * nd = node->nodes, * end = nd + IMAP_node_size(node);

    do
    {
        if (nd->data != NULL)
        {
            vec_append(vec, nd->data);
            vec_object_incref(nd->data);
        }

        if (nd->nodes != NULL)
        {
            IMAP_2vec_ref(nd, vec);
        }
    }
    while (++nd < end);
}

static void IMAP_union_ref(imap_node_t * dest, imap_node_t * node)
{
    imap_node_t * dest_nd;
    imap_node_t * node_nd;
    uint_fast8_t i, m;

    if (dest->key != IMAP_NODE_SZ &&
        node->key != dest->key &&
        IMAP_node_grow(dest))
    {
        abort();
    }

    for (i = dest->key & 0x1f, m = i + IMAP_node_size(dest); i < m; i++)
    {
        dest_nd = IMAP_unsafe_node(dest, i);
        node_nd = IMAP_ensure_node(node, i);

        if (node_nd->data != NULL)
        {
            if (dest_nd->data != NULL)
            {
                /* this must be the same object */
                assert (node_nd->data == dest_nd->data);
                /* we are sure there is a ref left */
                vec_object_decref(node_nd->data);
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
                dest_nd->key = node_nd->key;
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
    uint_fast8_t i, m;

    for (i = dest->key & 0x1f, m = i + IMAP_node_size(dest); i < m; i++)
    {
        dest_nd = IMAP_unsafe_node(dest, i);
        node_nd = IMAP_ensure_node(node, i);

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
            dest_nd->size = 0;
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
    uint_fast8_t i, m;

    for (i = dest->key & 0x1f, m = i + IMAP_node_size(dest); i < m; i++)
    {
        dest_nd = IMAP_unsafe_node(dest, i);
        node_nd = IMAP_ensure_node(node, i);

        if (node_nd->data != NULL)
        {
            if (dest_nd->data != NULL)
            {
                /* this must be the same object */
                assert (node_nd->data == dest_nd->data);
                /* we are sure to have one ref left */
                vec_object_decref(dest_nd->data);
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
                if (!dest_nd->size)
                {
                    free(dest_nd->nodes);
                    dest_nd->nodes = NULL;
                }
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
    uint_fast8_t i, m;

    if (dest->key != IMAP_NODE_SZ &&
        node->key != dest->key &&
        IMAP_node_grow(dest))
    {
        abort();
    }

    for (i = dest->key & 0x1f, m = i + IMAP_node_size(dest); i < m; i++)
    {
        dest_nd = IMAP_unsafe_node(dest, i);
        node_nd = IMAP_ensure_node(node, i);

        if (node_nd->data != NULL)
        {
            if (dest_nd->data != NULL)
            {
                /* this must be the same object */
                assert (node_nd->data == dest_nd->data);

                /* we are sure to have one ref left */
                vec_object_decref(dest_nd->data);

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
                dest_nd->key = node_nd->key;
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
