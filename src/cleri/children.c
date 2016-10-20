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
#include <stdlib.h>

/*
 * Returns NULL and in case an error has occurred.
 */
cleri_children_t * cleri_children_new(void)
{
    cleri_children_t * children =
            (cleri_children_t *) malloc(sizeof(cleri_children_t));
    if (children != NULL)
    {
		children->node = NULL;
		children->next = NULL;
    }
    return children;
}

/*
 * Appends a node to children but a signal is set in case an error occurs.
 *
 * Returns 0 when successful or -1 in case of an error.
 */
int cleri_children_add(cleri_children_t * children, cleri_node_t * node)
{
    if (children->node == NULL)
    {
        children->node = node;
        return 0;
    }

    while (children->next != NULL)
    {
        children = children->next;
    }

    children->next = (cleri_children_t *) malloc(sizeof(cleri_children_t));
    if (children->next == NULL)
    {
        return -1;
    }
    else
    {
        children->next->node = node;
        children->next->next = NULL;
    }
    return 0;
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

