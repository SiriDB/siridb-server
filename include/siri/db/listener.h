/*
 * listener.h - contains functions for processing queries.
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

#include <uv.h>
#include <siri/grammar/grammar.h>


uv_async_cb siridb_listen_enter[CLERI_END];
uv_async_cb siridb_listen_exit[CLERI_END];

void siridb_init_listener(void);


typedef struct siridb_list_query_s
{
    cleri_children_t * columns;

} siridb_list_query_t;
