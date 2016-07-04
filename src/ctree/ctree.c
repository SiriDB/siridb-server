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
#define CT_BUFFER_ALLOC_SIZE 256

static ct_node_t * CT_new_node(const char * key, void * data);
static int CT_add(
        ct_node_t * node,
        const char * key,
        void * data);
static void * CT_get(ct_node_t * node, const char * key);
static void * CT_pop(ct_node_t * parent, ct_node_t ** nd, const char * key);
static void ** CT_get_sure(ct_node_t * node, const char * key);
static void CT_dec_node(ct_node_t * node);
static void CT_merge_node(ct_node_t * node);
static void CT_walk(
        ct_node_t * node,
        size_t * pn,
        size_t len,
        size_t buffer_sz,
        char * buffer,
        ct_cb_t cb,
        void * args);

static void CT_free(ct_node_t * node);
static void CT_free_cb(ct_node_t * node, ct_free_cb_t cb);

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
        ct->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
    }
    return ct;
}

/*
 * Destroy ct-tree. Parsing NULL is NOT allowed.
 */
void ct_free(ct_t * ct)
{
    if (ct->nodes != NULL)
    {
        for (uint_fast16_t i = 256; i--;)
        {
            if ((*ct->nodes)[i] != NULL)
            {
                CT_free((*ct->nodes)[i]);
            }
        }
        free(ct->nodes);
    }
    free(ct);
}

/*
 * Destroy ct_tree. (parsing NULL is NOT allowed)
 * Call-back function will be called on each item in the tree.
 */
void ct_free_cb(ct_t * ct, ct_free_cb_t cb)
{
    if (ct->nodes != NULL)
    {
        for (uint_fast16_t i = 256; i--;)
        {
            if ((*ct->nodes)[i] != NULL)
            {
                CT_free_cb((*ct->nodes)[i], cb);
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
/*
 * In case of an error, NULL is returned and a SIGNAL is raised.
 */
void ** ct_get_sure(ct_t * ct, const char * key)
{
    ct_node_t ** nd;
    void ** data = NULL;

    nd = &(*ct->nodes)[(uint_fast8_t) *key];

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
        *nd = CT_new_node(key + 1, CT_EMPTY);
        if (*nd != NULL)
        {
            data = &(*nd)->data;
            ct->len++;
        }
    }
    return data;
}

/*
 * Add a new key/value. return CT_EXISTS if the key already
 * exists and CT_OK if not. When the key exists the value will not
 * be overwritten.
 *
 * In case of an error, CT_ERR will be returned and a SIGNAL is raised.
 */
int ct_add(ct_t * ct, const char * key, void * data)
{
    int rc;
    ct_node_t ** nd;

    nd = &(*ct->nodes)[(uint_fast8_t) *key];

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
        *nd = CT_new_node(key + 1, data);
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

    nd = (*ct->nodes)[(uint_fast8_t) *key];

    return (nd == NULL) ? NULL : CT_get(nd, key + 1);
}

/*
 * Removes and returns an item from the tree or NULL when not found.
 * In case of an error a SIGNAL is raised and should be checked with 'siri_err'.
 * (ct_free or ct_free_cb can still be used when this happens)
 */
void * ct_pop(ct_t * ct, const char * key)
{
    ct_node_t ** nd;
    void * data;

    nd = &(*ct->nodes)[(uint_fast8_t) *key];

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

    return data;
}

/*
 * Loop over all items in the tree and perform the call-back on each item.
 */
inline void ct_walk(ct_t * ct, ct_cb_t cb, void * args)
{
    ct_walkn(ct, NULL, cb, args);
}

/*
 * Loop over all items in the tree and perform the call-back on each item.
 * Walking stops either when the call-back is called on each item or
 * when 'n' is zero. 'n' will be decremented by one on each successful
 * call-back.
 */
void ct_walkn(ct_t * ct, size_t * n, ct_cb_t cb, void * args)
{
    size_t buffer_sz = CT_BUFFER_ALLOC_SIZE;
    size_t len = 1;
    ct_node_t * nd;
    char * buffer = (char *) malloc(buffer_sz);

    for (*buffer = 255; (n == NULL || *n) && (*buffer)--;)
    {
        if ((nd = (*ct->nodes)[(uint_fast8_t) *buffer]) == NULL)
        {
            continue;
        }
        CT_walk(nd, n, len, buffer_sz, buffer, cb, args);
    }
}

/*
 * Loop over all items in the tree and perform the call-back on each item.
 * Walking stops either when the call-back is called on each item or
 * when 'pn' is zero. 'pn' will be decremented by one on each successful
 * call-back.
 */
static void CT_walk(
        ct_node_t * node,
        size_t * pn,
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
    {
        if ((*cb)(buffer, node->data, args) && pn != NULL)
        {
            (*pn)--;
        }
    }
    if (node->nodes != NULL)
    {
        len += sz + 1;
        pt = buffer + len - 1;

        for (*pt = 255; (pn == NULL || *pn) && (*pt)--;)
        {
            if ((nd = (*node->nodes)[(uint_fast8_t) *pt]) == NULL)
            {
                continue;
            }
            CT_walk(nd, pn, len, buffer_sz, buffer, cb, args);
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

            if (node->nodes == NULL)
            {
                node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
                                if (node->nodes == NULL)
                {
                    ERR_ALLOC
                    return CT_ERR;
                }

                ct_node_t * nd = CT_new_node(key + 1, data);
                if (nd == NULL)
                {
                    return CT_ERR;
                }

                node->size = 1;
                (*node->nodes)[(uint_fast8_t) *key] = nd;

                return CT_OK;
            }

            ct_node_t ** nd = &(*node->nodes)[(uint_fast8_t) *key];

            if (*nd != NULL)
            {
                return CT_add(*nd, key + 1, data);
            }

            *nd = CT_new_node(key + 1, data);
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

            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
            if (new_nodes == NULL)
            {
                ERR_ALLOC
                return CT_ERR;
            }

            /* create new nodes with rest of node pt */
            ct_node_t * nd = (*new_nodes)[(uint_fast8_t) *pt] =
                    CT_new_node(pt + 1, node->data);
            if (nd == NULL)
            {
                return CT_ERR;
            }

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
                nd = CT_new_node(key + 1, data);
                if (nd == NULL)
                {
                    return CT_ERR;
                }
                node->size = 2;
                node->data = NULL;
                (*new_nodes)[(uint_fast8_t) *key] = nd;
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

            ct_node_t * nd = (*node->nodes)[(uint_fast8_t) *key];

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

            if (node->nodes == NULL)
            {
                node->size = 1;
                node->nodes = (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
                if (node->nodes == NULL)
                {
                    ERR_ALLOC
                    return NULL;
                }

                ct_node_t * nd = CT_new_node(key + 1, CT_EMPTY);
                return (nd == NULL) ? NULL :
                        &((*node->nodes)[(uint_fast8_t) *key] = nd)->data;
            }

            ct_node_t ** nd = &(*node->nodes)[(uint_fast8_t) *key];

            if (*nd != NULL)
            {
                return CT_get_sure(*nd, key + 1);
            }

            node->size++;

            *nd = CT_new_node(key + 1, CT_EMPTY);
            if (*nd == NULL)
            {
                return NULL;
            }

            return &(*nd)->data;
        }

        if (*key != *pt)
        {
            char * tmp;

            /* create new nodes */
            ct_nodes_t * new_nodes =
                    (ct_nodes_t *) calloc(1, sizeof(ct_nodes_t));
            if (new_nodes == NULL)
            {
                ERR_ALLOC
                return NULL;
            }

            /* bind the -rest- of current node to the new nodes */
            ct_node_t * nd = (*new_nodes)[(uint_fast8_t) *pt] =
                    CT_new_node(pt + 1, node->data);
            if (nd == NULL)
            {
                return NULL;
            }

            nd->size = node->size;
            nd->nodes = node->nodes;

            /* the current nodes should become the new nodes */
            node->nodes = new_nodes;

            /* write terminator */
            *pt = 0;

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

            nd = CT_new_node(key + 1, CT_EMPTY);
            return (nd == NULL) ?
                    NULL : &((*new_nodes)[(uint_fast8_t) *key] = nd)->data;
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
 * A SIGNAL can be raised in case an error occurred. If this happens the tree
 * remains unchanged and therefore can be destroyed using ct_free().
 */
static void CT_merge_node(ct_node_t * node)
{
#ifdef DEBUG
    assert(node->size == 1 && node->data == NULL);
#endif

    int i;
    size_t len_key = strlen(node->key);
    size_t len_child_key;
    ct_node_t * child_node;
    char * tmp;

    for (i = 0; i < 256; i++)
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
        ERR_ALLOC
        return;
    }
    node->key = tmp;

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
 * In case of an error a SIGNAL is raised and should be checked with 'siri_err'.
 * (ct_free or ct_free_cb can still be used when this happens)
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
                    CT_free(node);

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

            ct_node_t ** next = &(*node->nodes)[(uint_fast8_t) *key];

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
static ct_node_t * CT_new_node(const char * key, void * data)
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
 * Destroy ct_tree. (parsing NULL is NOT allowed)
 */
static void CT_free(ct_node_t * node)
{
    if (node->nodes != NULL)
    {
        for (uint_fast16_t i = 256; i--;)
        {
            if ((*node->nodes)[i] != NULL)
            {
                CT_free((*node->nodes)[i]);
            }
        }
        free(node->nodes);
    }
    free(node->key);
    free(node);
}

/*
 * Destroy ct_tree. (parsing NULL is NOT allowed)
 * Call-back function will be called on each item in the tree.
 */
static void CT_free_cb(ct_node_t * node, ct_free_cb_t cb)
{
    if (node->nodes != NULL)
    {
        for (uint_fast16_t i = 256; i--;)
        {
            if ((*node->nodes)[i] != NULL)
            {
                CT_free_cb((*node->nodes)[i], cb);
            }
        }
        free(node->nodes);
    }
    if (node->data != NULL)
    {
        cb(node->data);
    }
    free(node->key);
    free(node);
}
