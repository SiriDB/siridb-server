/*
 * children.c - linked list for keeping node results
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/children.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <siri/err.h>

/*
 * Returns NULL and sets a signal in case an error has occurred.
 */
cleri_children_t * cleri_children_new(void)
{
    cleri_children_t * children =
            (cleri_children_t *) malloc(sizeof(cleri_children_t));
    if (children == NULL)
    {
        ERR_ALLOC
        return NULL;
    }
    children->node = NULL;
    children->next = NULL;
    return children;
}

/*
 * Appends a node to children but a signal is set in case an error occurs.
 * (in case of an error, children remains unchanged)
 */
void cleri_children_add(cleri_children_t * children, cleri_node_t * node)
{
    if (children->node == NULL)
    {
        children->node = node;
        return;
    }

    while (children->next != NULL)
    {
        children = children->next;
    }

    children->next = (cleri_children_t *) malloc(sizeof(cleri_children_t));
    if (children->next == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        children->next->node = node;
        children->next->next = NULL;
    }
}

/*
 * Destroy children.
 */
void cleri_children_free(cleri_children_t * children)
{
    cleri_children_t * next;
    while (children != NULL)
    {
        next = children->next;
        cleri_node_free(children->node);
        free(children);
        children = next;
    }
}

