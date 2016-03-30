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

cleri_children_t * cleri_new_children(void)
{
    cleri_children_t * children;
    children = (cleri_children_t *) malloc(sizeof(cleri_children_t));
    children->node = NULL;
    children->next = NULL;
    return children;
}

void cleri_children_add(cleri_children_t * children, cleri_node_t * node)
{
    if (children->node == NULL)
    {
        children->node = node;
        return;
    }

    while (children->next != NULL)
        children = children->next;

    children->next = (cleri_children_t *) malloc(sizeof(cleri_children_t));
    children->next->node = node;
    children->next->next = NULL;
}

void cleri_free_children(cleri_children_t * children)
{
    cleri_children_t * next;
    while (children != NULL)
    {
        next = children->next;
        cleri_free_node(children->node);
        free(children);
        children = next;
    }
}

void cleri_empty_children(cleri_children_t * children)
{
    cleri_children_t * current;

    /* clean root node and set to NULL */
    cleri_free_node(children->node);
    children->node = NULL;

    /* set root next to NULL */
    current = children->next;
    children->next = NULL;

    /* clean all the rest */
    while (current != NULL)
    {
        children = current->next;
        cleri_free_node(current->node);
        free(current);
        current = children;
    }
}
