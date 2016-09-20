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
#include <logger/logger.h>
#include <stdlib.h>

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

/*
 * Returns the length of the content for a file and set buffer with the file
 * content. Note that malloc is used to allocate memory for the buffer.
 *
 * In case of an error -1 is returned and buffer will be set to NULL.
 */
ssize_t xpath_get_content(char ** buffer, const char * fn)
{
    ssize_t size;
    *buffer = NULL;

    FILE * fp = fopen(fn, "r");
    if (fp == NULL)
    {
        LOGC("Cannot open file: '%s'", fn);
        return -1;
    }

    if (fseeko(fp, 0, SEEK_END) == 0 &&
        (size = ftello(fp)) != -1 &&
        fseeko(fp, 0, SEEK_SET) == 0)
    {
        *buffer = (char *) malloc(size);
        if (*buffer != NULL)
        {
            if (fread(*buffer, size, 1, fp) != 1)
            {
                LOGC("Could not get full content from '%s'", fn);
                free(*buffer);
                *buffer = NULL;
            }
        }
    }

    fclose(fp);
    return (*buffer == NULL) ? -1 : size;
}
