/*
 * gramp.h - SiriDB Grammar Properties.
 *
 * Note: we need this file up-to-date with the grammar. The grammar has
 *       keywords starting with K_ so the will all be sorted.
 *       KW_OFFSET should be set to the first keyword and KW_COUNT needs the
 *       last keyword in the grammar.
 *
 */
#ifndef SIRI_GRAMP_H_
#define SIRI_GRAMP_H_

#include <siri/grammar/grammar.h>
#include <siri/inc.h>

/* keywords */
#define KW_OFFSET CLERI_GID_K_ACCESS
#define KW_COUNT CLERI_GID_K_WRITE + 1 - KW_OFFSET

/* aggregation functions */
#define F_OFFSET CLERI_GID_F_COUNT

/* help statements */
#define HELP_OFFSET CLERI_GID_HELP_ACCESS
#define HELP_COUNT CLERI_GID_HELP_TIMEZONES + 1 - HELP_OFFSET

#if CLERI_VERSION_MAJOR >= 1
typedef struct cleri_node_s cleri_children_t;
#define cleri_gn(__child) (__child)

#else
#define cleri_gn(__child) (__child)->node
#endif

#if CLERI_VERSION_MAJOR >= 1 || CLERI_VERSION_MINOR >= 11
#define cleri_parse_m(__g, __q) cleri_parse2(__g, __q, 0)
#else
#define cleri_parse_m(__g, __q) cleri_parse(__g, __q)
#endif

#if CLERI_VERSION_MAJOR >= 1 || CLERI_VERSION_MINOR >= 12
#if SIRIDB_IS64BIT
#define CLERI_NODE_DATA(__node) ((int64_t)(__node)->data)
#define CLERI_NODE_DATA_ADDR(__node) ((int64_t *) &(__node)->data)
#else
#define CLERI_NODE_DATA(__node) *((int64_t *)(__node)->data)
#define CLERI_NODE_DATA_ADDR(__node) ((int64_t *)(__node)->data)
#endif
#else
#define CLERI_NODE_DATA(__node) (__node)->result
#define CLERI_NODE_DATA_ADDR(__node) &(__node)->result
#endif

#endif  /* SIRI_GRAMP_H_ */
