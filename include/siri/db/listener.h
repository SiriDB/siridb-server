/*
 * listener.h - Contains functions for processing queries.
 */
#ifndef SIRIDB_LISTENER_H_
#define SIRIDB_LISTENER_H_

#include <uv.h>
#include <siri/grammar/grammar.h>

static uv_async_cb SIRIDB_NODE_ENTER[CLERI_END];
static uv_async_cb SIRIDB_NODE_EXIT[CLERI_END];

void siridb_init_listener(void);

static inline uv_async_cb siridb_node_get_enter(enum cleri_grammar_ids gid)
{
    return SIRIDB_NODE_ENTER[gid];
}

static inline uv_async_cb siridb_node_get_exit(enum cleri_grammar_ids gid)
{
    return SIRIDB_NODE_EXIT[gid];
}

#endif  /* SIRIDB_LISTENER_H_ */
