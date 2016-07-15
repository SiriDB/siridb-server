/*
 * xpath.c - Path and file tools
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 15-07-2016
 *
 */

#include <xpath/xpath.h>
#include <stdio.h>
#include <sys/stat.h>

/*
 * Test if file exist using the effective user.
 */
int xpath_file_exist(const char * fn)
{
    FILE * fp;
    fp = fopen(fn, "r");
    if (fp == NULL)
    {
        return 0;
    }
    fclose(fp);
    return 1;
}

/*
 * Test if a path is a directory
 */
int xpath_is_dir(const char * path)
{
    struct stat st = {0};
    stat(path, &st);
    return S_ISDIR(st.st_mode);
}
