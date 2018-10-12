/*
 * xstr.h - Extra String functions used by SiriDB.
 */
#ifndef XSTR_H_
#define XSTR_H_

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

void xstr_upper_case(char * sptr);
void xstr_lower_case(char * sptr);
void xstr_replace_char(char * sptr, char orig, char repl);
int xstr_replace_str(char * str, char * o, char * r, size_t n);
void xstr_split_join(char * pt, char split_chr, char join_chr);

/* do not use trim when the char is created with *alloc */
void xstr_trim(char ** str, char chr);
bool xstr_is_empty(const char * str);
bool xstr_is_int(const char * str);
bool xstr_is_float(const char * str);
bool xstr_is_graph(const char * str);
double xstr_to_double(const char * src, size_t len);
uint64_t xstr_to_uint64(const char * src, size_t len);
char * xstr_dup(const char * src, size_t * n);

/* important: 'dest' needs to be freed */
size_t xstr_extract_string(char * dest, const char * source, size_t len);

#endif  /* XSTR_H_ */
