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
#pragma once

#include <cleri/object.h>
#include <siri/db/nodes.h>
#include <uv.h>

typedef struct siridb_s siridb_t;
typedef struct siridb_nodes_s siridb_nodes_t;
typedef struct cleri_node_s cleri_node_t;

typedef struct siridb_walker_s
{
    siridb_t * siridb;
    uint64_t now;
    uint8_t * flags;
    siridb_nodes_t * start;
    siridb_nodes_t * enter_nodes;
    siridb_nodes_t * exit_nodes;
} siridb_walker_t;

siridb_walker_t * siridb_walker_new(
        siridb_t * siridb,
        const uint64_t now,
        uint8_t * flags);

/* free the walker and return the nodes which are kept in the walker.
 * note: the nodes are created using 'malloc()' so they must be destroyed
 *       using siridb_nodes_free().
 */
siridb_nodes_t * siridb_walker_free(siridb_walker_t * walker);

int siridb_walker_append(
        siridb_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb);

int siridb_walker_insert(
        siridb_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb);
