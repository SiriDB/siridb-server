/*
 * node.c - contains logic for cleri nodes which we need to parse.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#include <siri/db/node.h>
#include <logger/logger.h>
#include <stdlib.h>

siridb_node_walker_t * siridb_new_node_walker(
        siridb_t * siridb,
        const uint64_t now)
{
    siridb_node_walker_t * walker;
    walker = (siridb_node_walker_t *) malloc(sizeof(siridb_node_walker_t));
    walker->siridb = siridb;
    walker->now = now;
    walker->enter_nodes = NULL;
    walker->exit_nodes = NULL;
    return walker;
}

void siridb_append_enter_node(
        siridb_node_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb)
{
    siridb_node_list_t ** parent;

    if (walker->enter_nodes == NULL)
    {
        parent = &walker->enter_nodes;
    }
    else
    {
        siridb_node_list_t * current = walker->enter_nodes;

        while (current->next != NULL)
            current = current->next;

        parent = &current->next;
    }

    (*parent) = (siridb_node_list_t *) malloc(sizeof(siridb_node_list_t));
    (*parent)->node = node;
    (*parent)->cb = cb;
    (*parent)->next = NULL;
}

void siridb_insert_exit_node(
        siridb_node_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb)
{
    siridb_node_list_t * current = walker->exit_nodes;

    walker->exit_nodes =
            (siridb_node_list_t *) malloc(sizeof(siridb_node_list_t));
    walker->exit_nodes->node = node;
    walker->exit_nodes->cb = cb;
    walker->exit_nodes->next = current;
}

siridb_node_list_t * siridb_node_chain(siridb_node_walker_t * walker)
{
    siridb_node_list_t * current = walker->enter_nodes;

    if (walker->enter_nodes == NULL)
    {
        current = walker->exit_nodes;
        free(walker);
        return current;
    }
    /* find the last enter node */
    while (current->next != NULL)
        current = current->next;

    /* link exit nodes to the enter nodes */
    current->next = walker->exit_nodes;

    /* reset the current */
    current = walker->enter_nodes;

    /* free walker since we do not need it anymore */
    free(walker);

    return current;
}

void siridb_node_next(siridb_node_list_t ** node_list)
{
    siridb_node_list_t * next = (*node_list)->next;
    free(*node_list);
    *node_list = next;
}

void siridb_free_node_list(siridb_node_list_t * node_list)
{
    /* the node self will be closed when freeing the parse result */
    siridb_node_list_t * next;

    while (node_list != NULL)
    {
        next = node_list->next;
        free(node_list);
        node_list = next;
    }
}

