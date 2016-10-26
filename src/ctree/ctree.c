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
#include <logger/logger.h>
#include <siri/err.h>

/* initial buffer size, this is not fixed but can grow if needed */
#define CT_BUFFER_ALLOC_SIZE 128
#define BLOCKSZ 32

static ct_node_t * CT_node_new(const char * key, void * data);
static int CT_node_resize(ct_node_t * node, uint8_t pos);
static int CT_add(
        ct_node_t * node,
        const char * key,
        void * data);
static void * CT_get(ct_node_t * node, const char * key);
static void ** CT_getaddr(ct_node_t * node, const char * key);
static void * CT_getn(ct_node_t * node, const char * key, size_t * n);
static void * CT_pop(ct_node_t * parent, ct_node_t ** nd, const char * key);
static void ** CT_get_sure(ct_node_t * node, const char * key);
static void CT_dec_node(ct_node_t * node);
static void CT_merge_node(ct_node_t * node);
static int CT_items(
        ct_node_t * node,
        size_t * pn,
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

static char dummy = '\0';
char * CT_EMPTY = &dummy;

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
ct_t * ct_new(void)
{
    ct_t * ct = (ct_t *) malloc(sizeof(ct_t));
    if (ct == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        ct->len = 0;
        ct->nodes = NULL;
        ct->offset = 255;
        ct->n = 0;
    }
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
        for (uint_fast16_t i = 0, end = ct->n * BLOCKSZ; i < end; i++)
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
 * Can be used to check if ct_get_sure() has set an CT_EMPTY
 */
inline int ct_is_empty(void * data)
{
    return data == CT_EMPTY;
}

/*
 * Returns an item from the list or CT_EMPTY if the key does not exist.
 * The address or CT_EMPTY should then be used to set a new value.
 *
 * In case of an error, NULL is returned and a SIGNAL is raised.
 */
void ** ct_get_sure(ct_t * ct, const char * key)
{
    ct_node_t ** nd;
    void ** data = NULL;
    uint8_t k = (uint8_t) *key;

    if (CT_node_resize((ct_node_t *) ct, k / BLOCKSZ))
    {
        return NULL;  /* signal is raised */
    }

    nd = &(*ct->nodes)[k - ct->offset * BLOCKSZ];

    if (*nd != NULL)
    {
        data = CT_get_sure(*nd, key + 1);
        if (data != NULL && *data == CT_EMPTY)
        {
            ct->len++;
        }
    }
    else
    {
        *nd = CT_node_new(key + 1, CT_EMPTY);
        if (*nd != NULL)
        {
            data = &(*nd)->data;
            ct->len++;
        }
    }
    return data;
}

/*
 * Add a new key/value. return CT_EXISTS (1) if the key already
 * exists and CT_OK (0) if not. When the key exists the value will not
 * be overwritten.
 *
 * In case of an error, CT_ERR (-1) will be returned and a SIGNAL is raised.
 */
int ct_add(ct_t * ct, const char * key, void * data)
{
    int rc;
    ct_node_t ** nd;
    uint8_t k = (uint8_t) *key;

    if (CT_node_resize((ct_node_t *) ct, k / BLOCKSZ))
    {
        return CT_ERR;
    }

    nd = &(*ct->nodes)[k - ct->offset * BLOCKSZ];

    if (*nd != NULL)
    {
        rc = CT_add(*nd, key + 1, data);
        if (rc == CT_OK)
        {
            ct->len++;
        }
    }
    else
    {
        *nd = CT_node_new(key + 1, data);
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
    ct_node_t * nd;
    uint8_t k = (uint8_t) *key;
    uint8_t pos = k / BLOCKSZ;

    if (pos < ct->offset || pos >= ct->offset + ct->n)
    {
        return NULL;
    }
    nd = (*ct->nodes)[k - ct->offset * BLOCKSZ];

    return (nd == NULL) ? NULL : CT_get(nd, key + 1);
}

/*
 * Returns the address of an item or NULL if the key does not exist.
 */
void ** ct_getaddr(ct_t * ct, const char * key)
{
    ct_node_t * nd;
    uint8_t k = (uint8_t) *key;
    uint8_t pos = k / BLOCKSZ;

    if (pos < ct->offset || pos >= ct->offset + ct->n)
    {
        return NULL;
    }
    nd = (*ct->nodes)[k - ct->offset * BLOCKSZ];

    return (nd == NULL) ? NULL : CT_getaddr(nd, key + 1);
}

/*
 * Returns an item or NULL if the key does not exist.
 */
void * ct_getn(ct_t * ct, const char * key, size_t n)
{
    ct_node_t * nd;
    uint8_t k = (uint8_t) *key;
    uint8_t pos = k / BLOCKSZ;

    if (!n || pos < ct->offset || pos >= ct->offset + ct->n)
    {
        return NULL;
    }
    nd = (*ct->nodes)[k - ct->offset * BLOCKSZ];

    return (nd == NULL) ? NULL : CT_getn(nd, key + 1, &n);
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

    if (pos < ct->offset || pos >= ct->offset + ct->n)
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
 * an allocation error, or 1 when looping did not finish because of a non
 * zero return value.
 *
 * (in case of -1 (allocation error) a SIGNAL is raised)
 */
inline int ct_items(ct_t * ct, ct_item_cb cb, void * args)
{
    size_t n = 1;
    int rc = ct_itemsn(ct, &n, cb, args);
    return (n == 0) ? 1 : rc;
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

    for (uint_fast16_t i = 0, end = ct->n * BLOCKSZ; i < end; i++)
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

    for (uint_fast16_t i = 0, end = ct->n * BLOCKSZ; *n && i < end; i++)
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
 * when 'n' is zero. 'n' will be decremented by one on each successful
 * call-back.
 *
 * Returns 0 when successful or -1 and a SIGNAL is raised in case of an error.
 */
int ct_itemsn(ct_t * ct, size_t * n, ct_item_cb cb, void * args)
{
#ifdef DEBUG
    assert (n != NULL);
#endif

    size_t buffer_sz = CT_BUFFER_ALLOC_SIZE;
    size_t len = 1;
    ct_node_t * nd;
    int rc = 0;
    char * buffer = (char *) malloc(buffer_sz);
    if (buffer == NULL)
    {
        ERR_ALLOC
        return -1;
    }
    for (   uint_fast16_t i = 0, end = ct->n * BLOCKSZ;
            *n && i < end;
            i++)
    {
        if ((nd = (*ct->nodes)[i]) == NULL)
        {
            continue;
        }
        *buffer = (char) (i + ct->offset * BLOCKSZ);
        rc = CT_items(nd, n, len, buffer_sz, &buffer, cb, args);
    }
    free(buffer);
    return rc;
}

/*
 * Loop over all items in the tree and perform the call-back on each item.
 * Walking stops either when the call-back is called on each item or
 * when 'pn' is zero. 'pn' will be decremented by one on each successful
 * call-back.
 *
 * Returns 0 when successful or -1 and a SIGNAL is raised in case of an error.
 */
static int CT_items(
        ct_node_t * node,
        size_t * pn,
        size_t len,
        size_t buffer_sz,
        char ** buffer,
        ct_item_cb cb,
        void * args)
{
    size_t sz = strlen(node->key);

    if (sz + len + 1 > buffer_sz)
    {
        char * tmp;
        buffer_sz =
                ((sz + len) / CT_BUFFER_ALLOC_SIZE + 1) * CT_BUFFER_ALLOC_SIZE;
        tmp = (char *) realloc(*buffer, buffer_sz);
        if (tmp == NULL)
        {
            ERR_ALLOC;
            *pn = 0;
            return -1;
        }
        *buffer = tmp;
    }

    memcpy(*buffer + len, node->key, sz + 1);

    if (node->data != NULL)
    {
        if ((*cb)(*buffer, node->data, args))
        {
            (*pn)--;
        }
    }

    if (node->nodes != NULL)
    {
        ct_node_t * nd;
        len += sz + 1;

        for (   uint_fast16_t i = 0, end = node->n * BLOCKSZ;
                *pn && i < end;
                i++)
        {
            if ((nd = (*node->nodes)[i]) == NULL)
            {
                continue;
            }
            *(*buffer + len - 1) = (char) (i + node->offset * BLOCKSZ);
            if (CT_items(nd, pn, len, buffer_sz, buffer, cb, args))
            {
                return -1;
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

        for (uint_fast16_t i = 0, end = node->n * BLOCKSZ; i < end; i++)
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

        for (uint_fast16_t i = 0, end = node->n * BLOCKSZ; *n && i < end; i++)
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
 * In case of CT_ERR a SIGNAL is raised too and in case of CT_EXISTS the
 * existing item is not overwritten.
 */
static int CT_add(
        ct_node_t * node,
        const char * key,
        void * data)
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

            uint8_t k = (uint8_t) *key;

            if (node->nodes == NULL)
            {
                if (CT_node_resize(node, k / BLOCKSZ))
                {
                    return CT_ERR;  /* signal is raised */
                }

                ct_node_t * nd = CT_node_new(key + 1, data);
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
                return CT_ERR;  /* signal is raised */
            }

            ct_node_t ** nd = &(*node->nodes)[k - node->offset * BLOCKSZ];

            if (*nd != NULL)
            {
                return CT_add(*nd, key + 1, data);
            }

            *nd = CT_node_new(key + 1, data);
            if (*nd == NULL)
            {
                return CT_ERR;
            }
            node->size++;
            return CT_OK;
        }

        if (*key != *pt)
        {
            char * tmp;
            uint8_t k = (uint8_t) *pt;

            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
            if (new_nodes == NULL)
            {
                ERR_ALLOC
                return CT_ERR;
            }

            /* create new nodes with rest of node pt */
            ct_node_t * nd = (*new_nodes)[k % BLOCKSZ] =
                    CT_node_new(pt + 1, node->data);
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
                k = (uint8_t) *key;

                if (CT_node_resize(node, k / BLOCKSZ))
                {
                    return CT_ERR;  /* signal is raised */
                }

                nd = CT_node_new(key + 1, data);
                if (nd == NULL)
                {
                    return CT_ERR;
                }

                node->size = 2;
                node->data = NULL;
                (*node->nodes)[k - node->offset * BLOCKSZ] = nd;
            }

            /* write terminator */
            *pt = 0;

            /* re-allocate the key to free some space */
            tmp = (char *) realloc(node->key, pt - node->key + 1);
            if (tmp == NULL)
            {
                ERR_ALLOC
                return CT_ERR;
            }
            node->key = tmp;

            return CT_OK;
        }
    }

    /* we should never get here */
    return CT_ERR;
}

/*
 * Returns an item or NULL if the key does not exist.
 */
static void * CT_get(ct_node_t * node, const char * key)
{
    char * pt = node->key;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
            {
                return node->data;
            }

            if (node->nodes == NULL)
            {
                return NULL;
            }
            uint8_t k = (uint8_t) *key;
            uint8_t pos = k / BLOCKSZ;

            if (pos < node->offset || pos >= node->offset + node->n)
            {
                return NULL;
            }

            ct_node_t * nd = (*node->nodes)[k - node->offset * BLOCKSZ];

            return (nd == NULL) ? NULL : CT_get(nd, key + 1);
        }
        if (*key != *pt)
        {
            return NULL;
        }
    }

    /* we should never get here */
    return NULL;
}

/*
 * Returns an address of an item or NULL if the key does not exist.
 */
static void ** CT_getaddr(ct_node_t * node, const char * key)
{
    char * pt = node->key;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
            {
                return &node->data;
            }

            if (node->nodes == NULL)
            {
                return NULL;
            }
            uint8_t k = (uint8_t) *key;
            uint8_t pos = k / BLOCKSZ;

            if (pos < node->offset || pos >= node->offset + node->n)
            {
                return NULL;
            }

            ct_node_t * nd = (*node->nodes)[k - node->offset * BLOCKSZ];

            return (nd == NULL) ? NULL : CT_get(nd, key + 1);
        }
        if (*key != *pt)
        {
            return NULL;
        }
    }

    /* we should never get here */
    return NULL;
}

/*
 * Returns an item or NULL if the key does not exist.
 */
static void * CT_getn(ct_node_t * node, const char * key, size_t * n)
{
    char * pt = node->key;

    for (;(*n)-- ; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (!(*n))
            {
                return node->data;
            }

            if (node->nodes == NULL)
            {
                return NULL;
            }
            uint8_t k = (uint8_t) *key;
            uint8_t pos = k / BLOCKSZ;

            if (pos < node->offset || pos >= node->offset + node->n)
            {
                return NULL;
            }

            ct_node_t * nd = (*node->nodes)[k - node->offset * BLOCKSZ];

            return (nd == NULL) ? NULL : CT_getn(nd, key + 1, n);
        }
        if (!(*n) || *key != *pt)
        {
            return NULL;
        }
    }

    /* we should never get here */
    return NULL;
}

/*
 * Returns a item from the tree or CT_EMPTY in case the item is not found.
 * In case of an error, NULL is returned and a SIGNAL is raised.
 */
static void ** CT_get_sure(ct_node_t * node, const char * key)
{
    char * pt = node->key;

    for (;; key++, pt++)
    {
        if (*pt == 0)
        {
            if  (*key == 0)
            {
                if (node->data == NULL)
                {
                    node->data = CT_EMPTY;
                }

                return &node->data;
            }

            uint8_t k = (uint8_t) *key;

            if (node->nodes == NULL)
            {
                node->size = 1;

                if (CT_node_resize(node, k / BLOCKSZ))
                {
                    return NULL;  /* signal is raised */
                }

                ct_node_t * nd = CT_node_new(key + 1, CT_EMPTY);
                return (nd == NULL) ?
                    NULL :
                    &((*node->nodes)[k - node->offset * BLOCKSZ] = nd)->data;
            }

            if (CT_node_resize(node, k / BLOCKSZ))
            {
                return NULL;  /* signal is raised */
            }

            ct_node_t ** nd = &(*node->nodes)[k - node->offset * BLOCKSZ];

            if (*nd != NULL)
            {
                return CT_get_sure(*nd, key + 1);
            }

            node->size++;

            *nd = CT_node_new(key + 1, CT_EMPTY);
            if (*nd == NULL)
            {
                return NULL;
            }

            return &(*nd)->data;
        }

        if (*key != *pt)
        {
            char * tmp;
            uint8_t k = (uint8_t) *pt;

            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
            if (new_nodes == NULL)
            {
                ERR_ALLOC
                return NULL;
            }

            /* bind the -rest- of current node to the new nodes */
            ct_node_t * nd = (*new_nodes)[k % BLOCKSZ] =
                    CT_node_new(pt + 1, node->data);
            if (nd == NULL)
            {
                return NULL;
            }

            nd->size = node->size;
            nd->nodes = node->nodes;
            nd->offset = node->offset;
            nd->n = node->n;

            /* the current nodes should become the new nodes */
            node->nodes = new_nodes;
            node->offset = k / BLOCKSZ;
            node->n = 1;

            /* write terminator */
            *pt = 0;

            k = (uint8_t) *key;

            if (CT_node_resize(node, k / BLOCKSZ))
            {
                return NULL;  /* signal is raised */
            }

            /* re-allocate the key to free some space */
            tmp = (char *) realloc(node->key, pt - node->key + 1);
            if (tmp == NULL)
            {
                ERR_ALLOC
                return NULL;
            }
            node->key = tmp;

            if (*key == 0)
            {
                node->size = 1;
                node->data = CT_EMPTY;
                return &node->data;
            }

            node->size = 2;
            node->data = NULL;

            nd = CT_node_new(key + 1, CT_EMPTY);
            return (nd == NULL) ?
                NULL :
                &((*node->nodes)[k - node->offset * BLOCKSZ] = nd)->data;
        }
    }

    /* we should never get here */
    return NULL;
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
#ifdef DEBUG
    assert(node->size == 1 && node->data == NULL);
#endif

    size_t len_key = strlen(node->key);
    size_t len_child_key;
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

    /* we need the length of the child key as well */
    len_child_key = strlen(child_node->key);

    /* re-allocate enough space for the key + child_key */
    tmp = (char *) realloc(node->key, len_key + len_child_key + 2);
    if (tmp == NULL)
    {
        log_error("Re-allocation failed while merging nodes in a c-tree");
        return;
    }
    node->key = tmp;

    /* set node char */
    node->key[len_key] = (char) (i + node->offset * BLOCKSZ);

    /* append rest of the child key */
    memcpy(node->key + len_key + 1, child_node->key, len_child_key + 1);

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
 * A SIGNAL can be raised in case an error occurred. If this happens the node
 * size will still be decremented by one but the tree is not optionally merged.
 * This means that in case of an error its still possible to call ct_free() or
 * ct_free_cb() to destroy the tree.
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
                    CT_merge_node(*nd);
                }

                return data;
            }

            if (node->nodes == NULL)
            {
                /* nothing to pop */
                return NULL;
            }

            uint8_t k = (uint8_t) *key;
            uint8_t pos = k / BLOCKSZ;

            if (pos < node->offset || pos >= node->offset + node->n)
            {
                return NULL;
            }

            ct_node_t ** next = &(*node->nodes)[k - node->offset * BLOCKSZ];

            return (*next == NULL) ? NULL : CT_pop(node, next, key + 1);
        }
        if (*key != *pt)
        {
            /* nothing to pop */
            return NULL;
        }
    }

    /* we should never get here */
    return NULL;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
static ct_node_t * CT_node_new(const char * key, void * data)
{
    ct_node_t * node = (ct_node_t *) malloc(sizeof(ct_node_t));
    if (node == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        node->data = data;
        node->size = 0;
        node->offset = 255;
        node->n = 0;
        node->nodes = NULL;
        if ((node->key = strdup(key)) == NULL)
        {
            ERR_ALLOC
            free(node);
            node = NULL;
        }
    }
    return node;
}

/*
 * Returns 0 is successful or -1 and a SIGNAL is raised in case of an error.
 *
 * In case of an error, 'ct' remains unchanged.
 */
static int CT_node_resize(ct_node_t * node, uint8_t pos)
{
#ifdef DEBUG
    assert (pos < 8);
#endif
    int rc = 0;

    if (node->nodes == NULL)
    {
        node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
        if (node->nodes == NULL)
        {
            ERR_ALLOC
            rc = -1;
        }
        else
        {
            node->offset = pos;
            node->n = 1;
        }
    }
    else
    {
        if (pos < node->offset)
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
                ERR_ALLOC
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
            node->n += diff;
            tmp = (ct_nodes_t *) realloc(
                    node->nodes,
                    node->n * sizeof(ct_nodes_t));
            if (tmp == NULL && node->n)
            {
                ERR_ALLOC
                node->n -= diff;
                rc = -1;
            }
            else
            {
                node->nodes = tmp;
                memset(node->nodes + oldn, 0, diff * sizeof(ct_nodes_t));
            }
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
        for (uint_fast16_t i = 0, end = node->n * BLOCKSZ; i < end; i++)
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

