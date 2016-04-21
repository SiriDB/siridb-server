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

void upper_case(char * sptr);
void lower_case(char * sptr);
void replace_char(char * sptr, char orig, char repl);

/* do not use trim when the char is created with *alloc */
void trim(char ** str, char chr);
bool match(const char * str, const char * prefix);
bool starts_with(const char * str, const char * prefix);
bool is_empty(const char * str);
int strincmp(const char * stra, const char * strb, size_t len);
bool is_int(const char * str);
bool is_float(const char * str);
bool is_graph(const char * str);

/* important: 'dest' needs to be freed */
void extract_string(char ** dest, const char * source, size_t len);

/* Not used yet */
void add_spaces(char *dest, int num_of_spaces);
