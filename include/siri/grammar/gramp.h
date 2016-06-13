/*
 * gramp.h - SiriDB Grammar Properties.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * Note: we need this file up-to-date with the grammar. The grammar has
 *       keywords starting with K_ so the will all be sorted.
 *       KW_OFFSET should be set to the first keyword and KW_COUNT needs the
 *       last keyword in the grammar.
 *
 * changes
 *  - initial version, 15-04-2016
 *
 */
#pragma once

#include <siri/grammar/grammar.h>

#define KW_OFFSET CLERI_GID_K_ACCESS
#define KW_COUNT CLERI_GID_K_WRITE + 1 - KW_OFFSET
