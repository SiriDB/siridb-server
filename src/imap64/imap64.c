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

static imap64_node_t * pv_new_node(void);
static void pv_free_node(imap64_node_t * node);
static void pv_add(
        imap64_t * imap,
        imap64_node_t * node,
        uint64_t id,
        void * data);
static void * pv_get(imap64_node_t * node, uint64_t id);
static void * pv_pop(imap64_t * imap, imap64_node_t * node, uint64_t id);
static void pv_walk(imap64_node_t * node, imap64_cb_t cb, void * args);

imap64_t * imap64_new(void)
{
    imap64_t * imap = (imap64_t *) calloc(1, sizeof(imap64_t));
    imap->len = 0;
    imap->data = NULL;
    return imap;
}

void imap64_free(imap64_t * imap)
{
    uint_fast8_t i;

    if (imap->len)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (imap->node[i] == NULL)
                continue;
            pv_free_node(imap->node[i]);
        }
    }

    free(imap);
}

void imap64_add(imap64_t * imap, uint64_t id, void * data)
{
    uint_fast8_t key;
    imap64_node_t * node;

    assert (data != NULL);

    if (!id)
    {
        if (imap->data == NULL)
            imap->len++;
        imap->data = data;
        return;
    }

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (imap->node[key] == NULL)
        node = imap->node[key] = pv_new_node();

    if (id < IMAP64_NODE_SZ)
    {
        if (node->node[id] == NULL)
        {
            node->size++;
            node->node[id] = pv_new_node();
            imap->len++;
        }
        else if (node->node[id]->data == NULL)
            imap->len++;

        node->node[id]->data = data;
    }
    else
        pv_add(imap, node, id, data);
}

void * imap64_get(imap64_t * imap, uint64_t id)
{
    uint_fast8_t key;

    if (!id)
        return imap->data;

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (id < IMAP64_NODE_SZ)
        return (imap->node[key] == NULL || imap->node[key]->node[id] == NULL) ?
                NULL : imap->node[key]->node[id]->data;
    else
        return (imap->node[key] == NULL) ?
                NULL : pv_get(imap->node[key], id);
}

void * imap64_pop(imap64_t * imap, uint64_t id)
{
    void * data;
    uint_fast8_t key;

    if (!id)
    {
        if ((data = imap->data) == NULL)
            return NULL;

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
            return NULL;

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
            return NULL;

        data = pv_pop(imap, imap->node[key], id);

        if (!imap->node[key]->size)
        {
            free(imap->node[key]);
            imap->node[key] = NULL;
        }

        return data;
    }
}

void imap64_walk(imap64_t * imap, imap64_cb_t cb, void * args)
{
    uint_fast8_t i;

    if (imap->data != NULL)
        (*cb)(imap->data, args);

    if (imap->len)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (imap->node[i] == NULL)
                continue;
            pv_walk(imap->node[i], cb, args);
        }
    }
}

static imap64_node_t * pv_new_node(void)
{
    imap64_node_t * node = (imap64_node_t *) calloc(1, sizeof(imap64_node_t));
    node->size = 0;
    node->data = NULL;
    return node;
}

static void pv_free_node(imap64_node_t * node)
{
    uint_fast8_t i;

    if (node->size)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (node->node[i] == NULL)
                continue;
            pv_free_node(node->node[i]);
        }
    }

    free(node);
}

static void pv_add(
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
        node = node->node[key] = pv_new_node();
    }

    if (id < IMAP64_NODE_SZ)
    {
        if (node->node[id] == NULL)
        {
            node->size++;
            node->node[id] = pv_new_node();
            imap->len++;
        }
        else if (node->node[id]->data == NULL)
            imap->len++;

        node->node[id]->data = data;
    }
    else
        pv_add(imap, node, id, data);
}

static void * pv_get(imap64_node_t * node, uint64_t id)
{
    uint_fast8_t key;

    key = id % IMAP64_NODE_SZ;
    id /= IMAP64_NODE_SZ;

    if (id < IMAP64_NODE_SZ)
        return (node->node[key] == NULL || node->node[key]->node[id] == NULL) ?
                NULL : node->node[key]->node[id]->data;
    else
        return (node->node[key] == NULL) ?
                NULL : pv_get(node->node[key], id);
}

static void * pv_pop(imap64_t * imap, imap64_node_t * node, uint64_t id)
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
            return NULL;

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
            return NULL;

        data = pv_pop(imap, node->node[key], id);

        if (!node->node[key]->size)
        {
            free(node->node[key]);
            node->node[key] = NULL;
            node->size--;
        }

        return data;
    }
}

static void pv_walk(imap64_node_t * node, imap64_cb_t cb, void * args)
{
    uint_fast8_t i;

    if (node->data != NULL)
        (*cb)(node->data, args);

    if (node->size)
    {
        for (i = 0; i < IMAP64_NODE_SZ; i++)
        {
            if (node->node[i] == NULL)
                continue;
            pv_walk(node->node[i], cb, args);
        }
    }
}
