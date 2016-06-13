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
#include <logger/logger.h>
#include <siri/db/nodes.h>
#include <stdlib.h>


siridb_nodes_t * siridb_nodes_chain(siridb_walker_t * walker)
{
    siridb_nodes_t * current = walker->enter_nodes;

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

void siridb_nodes_next(siridb_nodes_t ** nodes)
{
    siridb_nodes_t * next = (*nodes)->next;
    free(*nodes);
    *nodes = next;
}

void siridb_nodes_free(siridb_nodes_t * nodes)
{
    /* the node self will be closed when freeing the parse result */
    siridb_nodes_t * next;

    while (nodes != NULL)
    {
        next = nodes->next;
        free(nodes);
        nodes = next;
    }
}

