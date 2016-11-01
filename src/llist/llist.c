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
#include <siri/err.h>

static llist_node_t * LLIST_node_new(void * data);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
llist_t * llist_new(void)
{
    llist_t * llist = (llist_t *) malloc(sizeof(llist_t));
    if (llist == NULL)
    {
    	ERR_ALLOC
    }
    else
    {
		llist->len = 0;
		llist->first = NULL;
		llist->last = NULL;
    }
    return llist;
}

/*
 * Destroys the linked list and calls a call-back function on each item.
 * The result of the call back function will be ignored.
 */
void llist_free_cb(llist_t * llist, llist_cb cb, void * args)
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

/*
 * Appends to the end of the list.
 *
 * Returns 0 if successful; -1 and a signal is raised in case an error occurred.
 * (in case of an error, the list remains unchanged)
 */
int llist_append(llist_t * llist, void * data)
{
    llist_node_t * node = LLIST_node_new(data);
    if (node == NULL)
    {
        return -1;  /* a signal is raised */
    }

    llist->len++;
    if (llist->last == NULL)
    {
        llist->last = node;
        llist->first = llist->last;
    }
    else
    {
        llist->last->next = node;
        llist->last = llist->last->next;
    }

    return 0;
}

/*
 * Walk through the list. Call-back function will be called on each item and
 * the sum of all results will be returned.
 */
int llist_walk(llist_t * llist, llist_cb cb, void * args)
{
    llist_node_t * node = llist->first;
    int rc = 0;
    while (node != NULL)
    {
        rc += cb(node->data, args);
        node = node->next;
    }
    return rc;
}

/*
 * Walk through the list. Call-back function will be called on each item and
 * 'n' will be decremented by one for each non-zero call-back result.
 * The walk will stop either on the end of the list or when 'n' is zero.
 */
void llist_walkn(llist_t * llist, size_t * n, llist_cb cb, void * args)
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

/*
 * Remove and return the first item where 'cb' is not zero
 * or NULL if not found unless 'cb' is NULL, then simple the
 * data is compared to 'args'.
 */
void * llist_remove(llist_t * llist, llist_cb cb, void * args)
{
    llist_node_t * node = llist->first;
    llist_node_t * prev = NULL;
    void * data;

    while (node != NULL)
    {
        if ((cb == NULL) ? node->data == args : cb(node->data, args))
        {
            if (prev == NULL)
            {
                /* this is the first node */
                if (llist->last == llist->first)
                {
                    /* this is the only node */
                    llist->first = llist->last = NULL;
                }
                else
                {
                    llist->first = node->next;
                }
            }
            else
            {
                prev->next = node->next;
                if (prev->next == NULL)
                {
                    /* this is the last node */
                    llist->last = prev;
                }
            }

            data = node->data;
            free(node);
            llist->len--;

            return data;
        }
        prev = node;
        node = node->next;
    }
    return NULL;
}

/*
 * Remove and return the last item in the list or NULL if empty.
 */
void * llist_pop(llist_t * llist)
{
    llist_node_t * node = llist->first;
    llist_node_t * prev = NULL;
    void * data = NULL;

    if (node != NULL)
    {
        /* get the last node */
        while (node->next != NULL)
        {
            prev = node;
            node = node->next;
        }

        if (prev == NULL)
        {
            /* this is the only node */
            llist->first = llist->last = NULL;
        }
        else
        {
            prev->next = NULL;
            llist->last = prev;
        }
        data = node->data;
        free(node);
        llist->len--;
    }

    return data;
}

/*
 * Remove and return the first item in the list or NULL if empty.
 */
void * llist_shift(llist_t * llist)
{
    llist_node_t * node = llist->first;
    void * data = NULL;

    if (node != NULL)
    {
        if (llist->last == llist->first)
        {
            llist->first = llist->last = NULL;
        }
        else
        {
            llist->first = node->next;
        }

        data = node->data;
        free(node);
        llist->len--;
    }

    return data;
}

/*
 * Return the first item where 'cb' is not zero or NULL if not found.
 */
void * llist_get(llist_t * llist, llist_cb cb, void * args)
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

/*
 * Copy the linked list to a simple list.
 * (returns NULL and sets a signal in case an error has occurred)
 */
slist_t * llist2slist(llist_t * llist)
{
    slist_t * slist = slist_new(llist->len);

    if (slist == NULL)
    {
        /* signal is set be slist_new() */
        return NULL;
    }

    llist_node_t * node = llist->first;
    size_t n;

    for  (n = 0; node != NULL; n++, node = node->next)
    {
        slist->data[n] = node->data;
    }

    slist->len = n;

    return slist;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
static llist_node_t * LLIST_node_new(void * data)
{
    llist_node_t * llist_node;
    llist_node = (llist_node_t *) malloc(sizeof(llist_node_t));
    if (llist_node == NULL)
    {
        ERR_ALLOC
        return NULL;
    }
    llist_node->data = data;
    llist_node->next = NULL;

    return llist_node;
}

