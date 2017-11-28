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

#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stddef.h>

int siridb_re_compile(
        pcre2_code ** regex,
        pcre2_match_data ** match_data,
        const char * source,
        size_t len,
        char * err_msg);
