/*
 * llist.c - Linked List
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 03-05-2016
 *
 */

#include <stddef.h>
#include <llist/llist.h>
#include <stdlib.h>

static llist_node_t * LLIST_node_new(void * data);

llist_t * llist_new(void)
{
    llist_t * llist;
    llist = (llist_t *) malloc(sizeof(llist_t));
    llist->len = 0;
    llist->first = NULL;
    llist->last = NULL;
    return llist;
}

void llist_free_cb(llist_t * llist, llist_cb_t cb, void * args)
{
    llist_node_t * node = llist->first;
    llist_node_t * next;

    while (node != NULL)
    {
        cb(node->data, args);
        next = node->next;
        free(node);
        node = next;
    }
    free(llist);
}

void llist_append(llist_t * llist, void * data)
{
    llist->len++;
    if (llist->last == NULL)
    {
        llist->last = LLIST_node_new(data);
        llist->first = llist->last;
    }
    else
    {
        llist->last->next = LLIST_node_new(data);
        llist->last = llist->last->next;
    }
}

void llist_walk(llist_t * llist, llist_cb_t cb, void * args)
{
    llist_node_t * node = llist->first;
    while (node != NULL)
    {
        cb(node->data, args);
        node = node->next;
    }
}

void llist_walkn(llist_t * llist, size_t * n, llist_cb_t cb, void * args)
{
    llist_node_t * node = llist->first;
    while (node != NULL && *n)
    {
        if (cb(node->data, args))
        {
            (*n)--;
        }
        node = node->next;
    }
}

void * llist_pop(llist_t * llist, llist_cb_t cb, void * args)
{
    llist_node_t * node = llist->first;
    llist_node_t * prev = NULL;
    void * data;

    while (node != NULL)
    {
        if (cb(node->data, args))
        {
            if (prev == NULL)
            {
                llist->first = node->next;
            }
            else
            {
                prev->next = node->next;
                if (prev->next == NULL)
                {
                    llist->last = prev;
                }
            }

            data = node->data;
            free(node);
            return data;
        }
        prev = node;
        node = node->next;
    }
    return NULL;
}

void * llist_get(llist_t * llist, llist_cb_t cb, void * args)
{
    llist_node_t * node = llist->first;
    while (node != NULL)
    {
        if (cb(node->data, args))
        {
            return node->data;
        }
        node = node->next;
    }
    return NULL;
}

slist_t * llist2slist(llist_t * llist)
{
    slist_t * slist = slist_new(llist->len);
    llist_node_t * node = llist->first;
    size_t n;

    for  (n = 0; node != NULL; n++, node = node->next)
    {
        slist->data[n] = node->data;
    }

    slist->len = n;

    return slist;
}

static llist_node_t * LLIST_node_new(void * data)
{
    llist_node_t * llist_node;
    llist_node = (llist_node_t *) malloc(sizeof(llist_node_t));
    llist_node->data = data;
    llist_node->next = NULL;
    return llist_node;
}

