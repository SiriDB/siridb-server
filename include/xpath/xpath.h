/*
 * xpath.h - Path and file tools
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 15-07-2016
 *
 */
#pragma once

#include <stdio.h>
#include <limits.h>

#ifndef PATH_MAX
#define SIRI_PATH_MAX 4096
#else
#define SIRI_PATH_MAX PATH_MAX
#endif

int xpath_file_exist(const char * fn);
int xpath_is_dir(const char * path);
ssize_t xpath_get_content(char ** buffer, const char * fn);
int xpath_get_exec_path(char * path);
