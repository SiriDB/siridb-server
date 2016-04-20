/*
 * index.h - Series index for regular expressions
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 11-03-2016
 *
 */
#pragma once

typedef struct siridb_index_s
{
    char * index;
    size_t size;
} siridb_index_t;

static siridb_build_index(siridb_index_t * index);
