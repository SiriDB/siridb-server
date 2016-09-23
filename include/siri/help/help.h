/*
 * help.h - Help for SiriDB
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 23-09-2016
 *
 */
#pragma once

#include <siri/grammar/gramp.h>

const char * siri_help_get(
        uint16_t gid,
        const char * help_name,
        char * err_msg);

void siri_help_free(void);
