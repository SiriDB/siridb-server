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
#include <stddef.h>

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
