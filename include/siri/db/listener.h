/*
 * listener.h - Contains functions for processing queries.
 */
#ifndef SIRIDB_LISTENER_H_
#define SIRIDB_LISTENER_H_

#include <uv.h>
#include <siri/grammar/grammar.h>

void siridb_init_listener(void);

uv_async_cb siridb_node_get_enter(enum cleri_grammar_ids gid);
uv_async_cb siridb_node_get_exit(enum cleri_grammar_ids gid);

#endif  /* SIRIDB_LISTENER_H_ */
