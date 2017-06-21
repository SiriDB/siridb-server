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

#include <cleri/object.h>
#include <siri/db/db.h>
#include <uv.h>

typedef struct siridb_s siridb_t;
typedef struct cleri_node_s cleri_node_t;

typedef struct siridb_nodes_s
{
    cleri_node_t * node;
    uv_async_cb cb;
    struct siridb_nodes_s * next;
} siridb_nodes_t;

void siridb_nodes_free(siridb_nodes_t * nodes);
void siridb_nodes_next(siridb_nodes_t ** nodes);
