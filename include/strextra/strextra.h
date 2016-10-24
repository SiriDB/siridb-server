/*
 * strextra.h - Extra String functions used by SiriDB
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-03-2016
 *
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

void strx_upper_case(char * sptr);
void strx_lower_case(char * sptr);
void strx_replace_char(char * sptr, char orig, char repl);
int strx_replace_str(char * str, char * o, char * r, size_t n);
void strx_split_join(char * pt, char split_chr, char join_chr);

/* do not use trim when the char is created with *alloc */
void strx_trim(char ** str, char chr);
bool strx_is_empty(const char * str);
bool strx_is_int(const char * str);
bool strx_is_float(const char * str);
bool strx_is_graph(const char * str);
double strx_to_double(const char * src, size_t len);
uint64_t strx_to_uint64(const char * src, size_t len);

/* important: 'dest' needs to be freed */
size_t strx_extract_string(char * dest, const char * source, size_t len);
