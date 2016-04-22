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
#include <uv.h>

struct siridb_s;
struct cleri_node_s;

typedef struct siridb_node_list_s
{
    struct cleri_node_s * node;
    uv_async_cb cb;
    struct siridb_node_list_s * next;
} siridb_node_list_t;

typedef struct siridb_node_walker_s
{
    struct siridb_s * siridb;
    uint64_t now;
    siridb_node_list_t * enter_nodes;
    siridb_node_list_t * exit_nodes;
} siridb_node_walker_t;

siridb_node_walker_t * siridb_new_node_walker(
        struct siridb_s * siridb,
        const uint64_t now);

/* this free's the walker and returns a combined enter/exit siridb_node_t */
siridb_node_list_t * siridb_node_chain(siridb_node_walker_t * walker);

void siridb_free_node_list(siridb_node_list_t * node_list);

void siridb_append_enter_node(
        siridb_node_walker_t * walker,
        struct cleri_node_s * node,
        uv_async_cb cb);

void siridb_insert_exit_node(
        siridb_node_walker_t * walker,
        struct cleri_node_s * node,
        uv_async_cb cb);

void siridb_node_next(siridb_node_list_t ** node_list);
