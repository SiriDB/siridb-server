/*
 * groups.h - Groups (saved regular expressions).
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-08-2016
 *
 */
#pragma once

#include <ctree/ctree.h>


typedef struct siridb_groups_s
{
    ct_t * groups;
    char * fn;
} siridb_groups_t;
