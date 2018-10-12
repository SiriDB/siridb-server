/*
 * ctree.c - Compact Binary Tree implementation.
 */
#include <ctree/ctree.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <logger/logger.h>

/* initial buffer size, this is not fixed but can grow if needed */
#define CT_BUF_SIZE 128
#define BLOCKSZ 32

static ct_node_t * CT_node_new(const char * key, size_t len, void * data);
static int CT_node_resize(ct_node_t * node, uint8_t pos);
static int CT_add(
        ct_node_t * node,
        const char * key,
        void * data);
static void * CT_pop(ct_node_t * parent, ct_node_t ** nd, const char * key);
static void CT_dec_node(ct_node_t * node);
static void CT_merge_node(ct_node_t * node);
static int CT_items(
        ct_node_t * node,
        size_t len,
        size_t buffer_sz,
        char ** buffer,
        ct_item_cb cb,
        void * args);
static int CT_values(
        ct_node_t * node,
        ct_val_cb cb,
        void * args);
static void CT_valuesn(
        ct_node_t * node,
        size_t * n,
        ct_val_cb cb,
        void * args);
static void CT_free(ct_node_t * node, ct_free_cb cb);

/*
 * Returns NULL in case an error has occurred.
 */
ct_t * ct_new(void)
{
    ct_t * ct = (ct_t *) malloc(sizeof(ct_t));
    if (ct == NULL)
    {
        return NULL;
    }

    ct->len = 0;
    ct->nodes = NULL;
    ct->offset = UINT8_MAX;
    ct->n = 0;

    return ct;
}

/*
 * Destroy ct-tree. Parsing NULL is NOT allowed.
 * Call-back function will be called on each item in the tree.
 */
void ct_free(ct_t * ct, ct_free_cb cb)
{
    if (ct->nodes != NULL)
    {
        uint_fast16_t i, end;
        for (i = 0, end = ct->n * BLOCKSZ; i < end; i++)
        {
            if ((*ct->nodes)[i] != NULL)
            {
                CT_free((*ct->nodes)[i], cb);
            }
        }
        free(ct->nodes);
    }
    free(ct);
}

/*
 * Add a new key/value. return CT_EXISTS (1) if the key already
 * exists and CT_OK (0) if not. When the key exists the value will not
 * be overwritten. CT_EXISTS is also returned when the key has length 0.
 *
 * In case of an error, CT_ERR (-1) will be returned.
 */
int ct_add(ct_t * ct, const char * key, void * data)
{
    int rc;
    ct_node_t ** nd;
    uint8_t k = (uint8_t) *key;
    if (!*key)
    {
        return CT_EXISTS;
    }

    if (CT_node_resize((ct_node_t *) ct, k / BLOCKSZ))
    {
        return CT_ERR;
    }

    nd = &(*ct->nodes)[k - ct->offset * BLOCKSZ];
    key++;

    if (*nd != NULL)
    {
        rc = CT_add(*nd, key, data);
        if (rc == CT_OK)
        {
            ct->len++;
        }
    }
    else
    {
        *nd = CT_node_new(key, strlen(key), data);
        if (*nd == NULL)
        {
            rc = CT_ERR;
        }
        else
        {
            ct->len++;
            rc = CT_OK;
        }
    }
    return rc;
}

/*
 * Returns an item or NULL if the key does not exist.
 */
void * ct_get(ct_t * ct, const char * key)
{
    void ** data = ct_getaddr(ct, key);
    return (data) ? *data : NULL;
}

/*
 * Returns the address of an item or NULL if the key does not exist.
 */
void ** ct_getaddr(ct_t * ct, const char * key)
{
    ct_node_t * nd;
    uint8_t k = (uint8_t) *key;
    uint8_t pos = k / BLOCKSZ;

    if (!*key || pos < ct->offset || pos >= ct->offset + ct->n)
    {
        return NULL;
    }

    nd = (*ct->nodes)[k - ct->offset * BLOCKSZ];

    while (nd && !strncmp(nd->key, ++key, nd->len))
    {
        key += nd->len;

        if (!*key) return &nd->data;
        if (!nd->nodes) return NULL;

        k = (uint8_t) *key;
        pos = k / BLOCKSZ;

        if (pos < nd->offset || pos >= nd->offset + nd->n)
        {
            return NULL;
        }

        nd = (*nd->nodes)[k - nd->offset * BLOCKSZ];
    }

    return NULL;
}

/*
 * Returns an item or NULL if the key does not exist.
 */
void * ct_getn(ct_t * ct, const char * key, size_t n)
{
    size_t diff = 1;
    ct_node_t * nd;
    uint8_t k = (uint8_t) *key;
    uint8_t pos = k / BLOCKSZ;

    if (!n || pos < ct->offset || pos >= ct->offset + ct->n)
    {
        return NULL;
    }

    nd = (*ct->nodes)[k - ct->offset * BLOCKSZ];

    while (nd)
    {
        key += diff;
        n -= diff;

        if (n < nd->len || strncmp(nd->key, key, nd->len))
        {
            return NULL;
        }

        if (nd->len == n) return nd->data;
        if (!nd->nodes) return NULL;

        k = (uint8_t) key[nd->len];
        pos = k / BLOCKSZ;

        if (pos < nd->offset || pos >= nd->offset + nd->n)
        {
            return NULL;
        }

        diff = nd->len + 1; /* n - diff is at least 0 */
        nd = (*nd->nodes)[k - nd->offset * BLOCKSZ];
    }

    return NULL;
}

/*
 * Removes and returns an item from the tree or NULL when not found.
 *
 * (re-allocation might fail but this is not critical)
 */
void * ct_pop(ct_t * ct, const char * key)
{
    ct_node_t ** nd;
    void * data;
    uint8_t k = (uint8_t) *key;
    uint8_t pos = k / BLOCKSZ;

    if (!*key || pos < ct->offset || pos >= ct->offset + ct->n)
    {
        data = NULL;
    }
    else
    {
        nd = &(*ct->nodes)[k - ct->offset * BLOCKSZ];

        if (*nd == NULL)
        {
            data = NULL;
        }
        else
        {
            data = CT_pop(NULL, nd, key + 1);
            if (data != NULL)
            {
                ct->len--;
            }
        }
    }

    return data;
}

/*
 * Loop over all items in the tree and perform the call-back on each item.
 *
 * Looping stops on the first call-back returning a non-zero value.
 *
 * Returns 0 when the call-back is called on all items, -1 in case of
 * an allocation error or 1 when looping did not finish because of a non
 * zero return value.
 */
int ct_items(ct_t * ct, ct_item_cb cb, void * args)
{
    size_t buffer_sz = CT_BUF_SIZE;
    size_t len = 1;
    ct_node_t * nd;
    int rc = 0;
    char * buffer = (char *) malloc(buffer_sz);
    uint_fast16_t i, end;

    if (buffer == NULL)
    {
        return -1;
    }
    for (i = 0, end = ct->n * BLOCKSZ; !rc && i < end; i++)
    {
        if ((nd = (*ct->nodes)[i]) == NULL)
        {
            continue;
        }
        *buffer = (char) (i + ct->offset * BLOCKSZ);
        rc = CT_items(nd, len, buffer_sz, &buffer, cb, args);
    }
    free(buffer);
    return rc;
}

/*
 * Loop over all values in the tree and perform the call-back on each value.
 *
 * Returns the sum of all the call-backs.
 */
int ct_values(ct_t * ct, ct_val_cb cb, void * args)
{
    ct_node_t * nd;
    int rc = 0;
    uint_fast16_t i, end;

    for (i = 0, end = ct->n * BLOCKSZ; i < end; i++)
    {
        if ((nd = (*ct->nodes)[i]) == NULL)
        {
            continue;
        }
        rc += CT_values(nd, cb, args);
    }

    return rc;
}

/*
 * Walking stops either when the call-back is called on each value or
 * when 'n' is zero. 'n' will be decremented by the result of each call-back.
 */
void ct_valuesn(ct_t * ct, size_t * n, ct_val_cb cb, void * args)
{
    ct_node_t * nd;
    uint_fast16_t i, end;

    for (i = 0, end = ct->n * BLOCKSZ; *n && i < end; i++)
    {
        if ((nd = (*ct->nodes)[i]) == NULL)
        {
            continue;
        }
        CT_valuesn(nd, n, cb, args);
    }
}

/*
 * Loop over all items in the tree and perform the call-back on each item.
 * Walking stops either when the call-back is called on each item or
 * when a callback return a non zero value.
 *
 * Returns 0 when successful or -1 in case of an error or 1 if looping has
 * stopped because a failed callback.
 */
static int CT_items(
        ct_node_t * node,
        size_t len,
        size_t buffer_sz,
        char ** buffer,
        ct_item_cb cb,
        void * args)
{
    if (node->len + len + 1 > buffer_sz)
    {
        char * tmp;
        buffer_sz = ((node->len + len) / CT_BUF_SIZE + 1) * CT_BUF_SIZE;
        tmp = (char *) realloc(*buffer, buffer_sz);
        if (tmp == NULL)
        {
            return -1;
        }
        *buffer = tmp;
    }

    memcpy(*buffer + len, node->key, node->len);
    len += node->len;

    if (node->data != NULL && (*cb)(*buffer, len, node->data, args))
    {
        return 1;
    }

    if (node->nodes != NULL)
    {
        ct_node_t * nd;
        int rc;
        uint_fast16_t i, end;

        for (i = 0, end = node->n * BLOCKSZ; i < end; i++)
        {
            if ((nd = (*node->nodes)[i]) == NULL)
            {
                continue;
            }
            *(*buffer + len) = (char) (i + node->offset * BLOCKSZ);
            rc = CT_items(nd, len + 1, buffer_sz, buffer, cb, args);
            if (rc)
            {
                return rc;
            }
        }
    }

    return 0;
}

/*
 * Loop over all values in the tree and perform the call-back on each value.
 *
 * The value returned is the sum of all the call-backs.
 */
static int CT_values(
        ct_node_t * node,
        ct_val_cb cb,
        void * args)
{
    int rc = 0;

    if (node->data != NULL)
    {
        rc += (*cb)(node->data, args);
    }

    if (node->nodes != NULL)
    {
        ct_node_t * nd;
        uint_fast16_t i, end;

        for (i = 0, end = node->n * BLOCKSZ; i < end; i++)
        {
            if ((nd = (*node->nodes)[i]) == NULL)
            {
                continue;
            }
            rc += CT_values(nd, cb, args);
        }
    }

    return rc;
}

/*
 * Loop over all values in the tree and perform the call-back on each value.
 * */
static void CT_valuesn(
        ct_node_t * node,
        size_t * n,
        ct_val_cb cb,
        void * args)
{
    if (node->data != NULL)
    {
        *n -= (*cb)(node->data, args);
    }

    if (node->nodes != NULL)
    {
        ct_node_t * nd;
        uint_fast16_t i, end;

        for (i = 0, end = node->n * BLOCKSZ; *n && i < end; i++)
        {
            if ((nd = (*node->nodes)[i]) == NULL)
            {
                continue;
            }
            CT_valuesn(nd, n, cb, args);
        }
    }
}

/*
 * Returns CT_OK when the item is added, CT_EXISTS if the item already exists,
 * or CT_ERR in case or an error.
 * In case of CT_EXISTS the existing item is not overwritten.
 */
static int CT_add(
        ct_node_t * node,
        const char * key,
        void * data)
{
    size_t n;
    for (n = 0; n < node->len; n++, key++)
    {
        char * pt = node->key + n;
        if (*key != *pt)
        {
            size_t new_sz;
            uint8_t k = (uint8_t) *pt;
            ct_node_t * nd;

            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
            if (new_nodes == NULL)
            {
                return CT_ERR;
            }

            /* create new nodes with rest of node pt */
            nd = (*new_nodes)[k % BLOCKSZ] =
                    CT_node_new(pt + 1, node->len - n - 1, node->data);
            if (nd == NULL)
            {
                return CT_ERR;
            }

            /* bind the -rest- of current node to the new nodes */
            nd->nodes = node->nodes;
            nd->size = node->size;
            nd->offset = node->offset;
            nd->n = node->n;

            /* the current nodes should become the new nodes */
            node->nodes = new_nodes;
            node->offset = k / BLOCKSZ;
            node->n = 1;

            if (!*key)
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
                k = (uint8_t) *key;

                if (CT_node_resize(node, k / BLOCKSZ))
                {
                    return CT_ERR;
                }
                key++;
                nd = CT_node_new(key, strlen(key), data);
                if (nd == NULL)
                {
                    return CT_ERR;
                }

                node->size = 2;
                node->data = NULL;
                (*node->nodes)[k - node->offset * BLOCKSZ] = nd;
            }

            /* re-allocate the key to free some space */
            if ((new_sz = pt - node->key))
            {
                char * tmp = (char *) realloc(node->key, new_sz);
                if (tmp != NULL)
                {
                    node->key = tmp;
                }
            }
            else
            {
                free(node->key);
                node->key = NULL;
            }
            node->len = new_sz;

            return CT_OK;
        }
    }

    if (*key)
    {
        uint8_t k = (uint8_t) *key;

        if (node->nodes == NULL)
        {
            if (CT_node_resize(node, k / BLOCKSZ))
            {
                return CT_ERR;
            }
            key++;
            ct_node_t * nd = CT_node_new(key, strlen(key), data);
            if (nd == NULL)
            {
                return CT_ERR;
            }

            node->size = 1;
            (*node->nodes)[k - node->offset * BLOCKSZ ] = nd;

            return CT_OK;
        }

        if (CT_node_resize(node, k / BLOCKSZ))
        {
            return CT_ERR;
        }

        ct_node_t ** nd = &(*node->nodes)[k - node->offset * BLOCKSZ];
        key++;

        if (*nd != NULL)
        {
            return CT_add(*nd, key, data);
        }

        *nd = CT_node_new(key, strlen(key), data);
        if (*nd == NULL)
        {
            return CT_ERR;
        }
        node->size++;
        return CT_OK;
    }

    if (node->data != NULL)
    {
        /* duplicate key */
        return CT_EXISTS;
    }
    node->data = data;

    return CT_OK;
}

/*
 * Merge a child node with its parent for cleanup.
 *
 * This function should only be called when exactly one node is left and the
 * node itself has no data.
 *
 * In case re-allocation fails the tree remains unchanged and therefore
 * can still be used.
 */
static void CT_merge_node(ct_node_t * node)
{
    assert(node->size == 1 && node->data == NULL);
    ct_node_t * child_node;
    char * tmp;
    uint_fast16_t i, end;

    for (i = 0, end = node->n * BLOCKSZ; i < end; i++)
    {
        if ((*node->nodes)[i] != NULL)
        {
            break;
        }
    }
    /* this is the child node we need to merge */
    child_node = (*node->nodes)[i];

    /* re-allocate enough space for the key + child_key + 1 char */
    tmp = (char *) realloc(node->key, node->len + child_node->len + 1);
    if (tmp == NULL)
    {
        log_error("Re-allocation failed while merging nodes in a c-tree");
        return;
    }
    node->key = tmp;

    /* set node char */
    node->key[node->len++] = (char) (i + node->offset * BLOCKSZ);

    /* append rest of the child key */
    memcpy(node->key + node->len, child_node->key, child_node->len);
    node->len += child_node->len;

    /* free nodes (has only the child node left so nothing else
     * needs cleaning */
    free(node->nodes);

    /* bind child nodes properties to the current node */
    node->nodes = child_node->nodes;
    node->size = child_node->size;
    node->offset = child_node->offset;
    node->n = child_node->n;
    node->data = child_node->data;

    /* free child key */
    free(child_node->key);

    /* free child node */
    free(child_node);
}

/*
 * This function can fail but in that case the tree is still usable.
 */
static void CT_dec_node(ct_node_t * node)
{
    if (node == NULL)
    {
        /* this is the root node */
        return;
    }

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
        CT_merge_node(node);
    }
}

/*
 * Removes and returns an item from the tree or NULL when not found.
 */
static void * CT_pop(ct_node_t * parent, ct_node_t ** nd, const char * key)
{
    ct_node_t * node = *nd;
    if (strncmp(node->key, key, node->len))
    {
        return NULL;
    }

    key += node->len;

    if (!*key)
    {
        void * data = node->data;
        if (node->size == 0)
        {
            /* no child nodes, lets clean up this node */
            CT_free(node, NULL);

            /* make sure to set the node to NULL so the parent
             * can do its cleanup correctly */
            *nd = NULL;

            /* size of parent should be minus one */
            CT_dec_node(parent);

            return data;
        }
        /* we cannot clean this node, set data to NULL */
        node->data = NULL;

        if (node->size == 1)
        {
            /* we have only one child, we can merge this
             * child with this one */
            CT_merge_node(node);
        }

        return data;
    }

    if (node->nodes != NULL)
    {
        uint8_t k = (uint8_t) *key;
        uint8_t pos = k / BLOCKSZ;

        if (pos < node->offset || pos >= node->offset + node->n)
        {
            return NULL;
        }

        ct_node_t ** next = &(*node->nodes)[k - node->offset * BLOCKSZ];

        return (*next == NULL) ? NULL : CT_pop(node, next, key + 1);
    }
    return NULL;
}

/*
 * Returns NULL in case an error has occurred.
 */
static ct_node_t * CT_node_new(const char * key, size_t len, void * data)
{
    ct_node_t * node = (ct_node_t *) malloc(sizeof(ct_node_t));
    if (node == NULL)
    {
        return NULL;
    }
    node->len = len;
    node->data = data;
    node->size = 0;
    node->offset = UINT8_MAX;
    node->n = 0;
    node->nodes = NULL;
    if (len)
    {
        node->key = (char *) malloc(len);
        if (node->key == NULL)
        {
            free(node);
            return NULL;
        }
        memcpy(node->key, key, len);
    }
    else
    {
        node->key = NULL;
    }
    return node;
}

/*
 * Returns 0 is successful or -1 in case of an error.
 *
 * In case of an error, 'ct' remains unchanged.
 */
static int CT_node_resize(ct_node_t * node, uint8_t pos)
{
    int rc = 0;

    if (node->nodes == NULL)
    {
        node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
        if (node->nodes == NULL)
        {
            rc = -1;
        }
        else
        {
            node->offset = pos;
            node->n = 1;
        }
    }
    else if (pos < node->offset)
    {
        ct_nodes_t * tmp;
        uint8_t diff = node->offset - pos;
        uint8_t oldn = node->n;
        node->n += diff;
        tmp = (ct_nodes_t *) realloc(
                node->nodes,
                node->n * sizeof(ct_nodes_t));
        if (tmp == NULL && node->n)
        {
            node->n -= diff;
            rc = -1;
        }
        else
        {
            node->nodes = tmp;
            node->offset = pos;
            memmove(node->nodes + diff,
                    node->nodes,
                    oldn * sizeof(ct_nodes_t));
            memset(node->nodes, 0, diff * sizeof(ct_nodes_t));
        }
    }
    else if (pos >= node->offset + node->n)
    {
        ct_nodes_t * tmp;
        uint8_t diff = pos - node->offset - node->n + 1;
        uint8_t oldn = node->n;
        node->n += diff;  /* assert node->n > 0 */
        tmp = (ct_nodes_t *) realloc(
                node->nodes,
                node->n * sizeof(ct_nodes_t));
        if (tmp == NULL)
        {
            node->n -= diff;
            rc = -1;
        }
        else
        {
            node->nodes = tmp;
            memset(node->nodes + oldn, 0, diff * sizeof(ct_nodes_t));
        }
    }

    return rc;
}

/*
 * Destroy ct_tree. (parsing NULL is NOT allowed)
 * Call-back function will be called on each item in the tree.
 */
static void CT_free(ct_node_t * node, ct_free_cb cb)
{
    if (node->nodes != NULL)
    {
        uint_fast16_t i, end;
        for (i = 0, end = node->n * BLOCKSZ; i < end; i++)
        {
            if ((*node->nodes)[i] != NULL)
            {
                CT_free((*node->nodes)[i], cb);
            }
        }
        free(node->nodes);
    }
    if (cb != NULL && node->data != NULL)
    {
        (*cb)(node->data);
    }
    free(node->key);
    free(node);
}

