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
#ifndef SIRIDB_NODES_H_
#define SIRIDB_NODES_H_

typedef struct siridb_nodes_s siridb_nodes_t;

#include <cleri/cleri.h>
#include <siri/db/db.h>
#include <uv.h>

void siridb_nodes_free(siridb_nodes_t * nodes);
void siridb_nodes_next(siridb_nodes_t ** nodes);

struct siridb_nodes_s
{
    cleri_node_t * node;
    uv_async_cb cb;
    struct siridb_nodes_s * next;
};

#endif  /* SIRIDB_NODES_H_ */
