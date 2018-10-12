/*
 * re.h - Helpers for regular expressions.
 */
#ifndef SIRIDB_RE_H_
#define SIRIDB_RE_H_

#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stddef.h>

int siridb_re_compile(
        pcre2_code ** regex,
        pcre2_match_data ** match_data,
        const char * source,
        size_t len,
        char * err_msg);


#endif  /* SIRIDB_RE_H_ */
