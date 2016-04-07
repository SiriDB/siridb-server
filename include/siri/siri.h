/*
 * siri.h - global methods for SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/grammar/grammar.h>
#include <siri/db/db.h>

struct cleri_grammar_s;
struct siridb_list_s;

typedef struct siri_s
{
    uv_loop_t * loop;
    uint16_t max_open_files;
    struct cleri_grammar_s * grammar;
    struct siridb_list_s * siridb_list;
} siri_t;


void siri_setup_logger(void);


int siri_start(void);
void siri_free(void);

siri_t siri;
