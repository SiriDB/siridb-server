/*
 * walker.h - creates enter and exit nodes
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-06-2016
 *
 */
#include <siri/db/walker.h>

siridb_walker_t * siridb_walker_new(
        siridb_t * siridb,
        const uint64_t now,
        int * flags)
{
    siridb_walker_t * walker;
    walker = (siridb_walker_t *) malloc(sizeof(siridb_walker_t));
    walker->siridb = siridb;
    walker->now = now;
    walker->flags = flags;
    walker->enter_nodes = NULL;
    walker->exit_nodes = NULL;
    return walker;
}

void siridb_walker_free(siridb_walker_t * walker)
{
    siridb_nodes_free(walker->enter_nodes);
    siridb_nodes_free(walker->exit_nodes);
    free(walker);
}

void siridb_walker_append(
        siridb_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb)
{
    siridb_nodes_t ** parent;

    if (walker->enter_nodes == NULL)
    {
        parent = &walker->enter_nodes;
    }
    else
    {
        siridb_nodes_t * current = walker->enter_nodes;

        while (current->next != NULL)
            current = current->next;

        parent = &current->next;
    }

    (*parent) = (siridb_nodes_t *) malloc(sizeof(siridb_nodes_t));
    (*parent)->node = node;
    (*parent)->cb = cb;
    (*parent)->next = NULL;
}

void siridb_walker_insert(
        siridb_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb)
{
    siridb_nodes_t * current = walker->exit_nodes;

    walker->exit_nodes =
            (siridb_nodes_t *) malloc(sizeof(siridb_nodes_t));
    walker->exit_nodes->node = node;
    walker->exit_nodes->cb = cb;
    walker->exit_nodes->next = current;
}
