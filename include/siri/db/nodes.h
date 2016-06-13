/*
 * node.h - contains logic for cleri nodes which we need to parse.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#pragma once

#include <cleri/node.h>
#include <siri/db/db.h>
#include <siri/db/walker.h>
#include <uv.h>

typedef struct siridb_s siridb_t;
typedef struct cleri_node_s cleri_node_t;
typedef struct siridb_walker_s siridb_walker_t;

typedef struct siridb_nodes_s
{
    cleri_node_t * node;
    uv_async_cb cb;
    struct siridb_nodes_s * next;
} siridb_nodes_t;

/* this free's the walker and returns a combined enter/exit siridb_node_t */
siridb_nodes_t * siridb_nodes_chain(siridb_walker_t * walker);

void siridb_nodes_free(siridb_nodes_t * nodes);

void siridb_nodes_next(siridb_nodes_t ** nodes);
