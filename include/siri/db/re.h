/*
 * re.h - Helpers for regular expressions
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-08-2016
 *
 */
#pragma once

#include <pcre.h>
#include <stddef.h>

int siridb_re_compile(
        pcre ** regex,
        pcre_extra ** regex_extra,
        const char * source,
        size_t len,
        char * err_msg);
