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
#include <siri/filehandler.h>
#include <stdbool.h>

typedef struct cleri_grammar_s cleri_grammar_t;
typedef struct siridb_list_s siridb_list_t;
typedef struct siri_fh_s siri_fh_t;

typedef struct siri_s
{
    uv_loop_t * loop;
    cleri_grammar_t * grammar;
    siridb_list_t * siridb_list;
    siri_fh_t * fh;

} siri_t;


void siri_setup_logger(void);


int siri_start(void);
void siri_free(void);

siri_t siri;
