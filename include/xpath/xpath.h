/*
 * xpath.h - Path and file tools.
 */
#ifndef XPATH_H_
#define XPATH_H_

#include <stdio.h>
#include <limits.h>

#ifndef PATH_MAX
#define XPATH_MAX 4096
#else
#define XPATH_MAX PATH_MAX
#endif

int xpath_file_exist(const char * fn);
int xpath_is_dir(const char * path);
ssize_t xpath_get_content(char ** buffer, const char * fn);
int xpath_get_exec_path(char * path);

#endif  /* XPATH_H_ */
