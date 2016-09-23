/*
 * help.c - Help for SiriDB
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 23-09-2016
 *
 */


#include <siri/help/help.h>
#include <limits.h>
#include <xpath/xpath.h>
#include <logger/logger.h>
#include <siri/db/db.h>
#include <stdio.h>

static char * siri_help_content[HELP_COUNT] = {0};

const char * siri_help_get(
        uint16_t gid,
        const char * help_name,
        char * err_msg)
{
    char ** content = &siri_help_content[gid - HELP_OFFSET];

    if (*content == NULL)
    {
        char path[PATH_MAX];
        char fn[PATH_MAX];

        if (xpath_get_exec_path(path))
        {
            sprintf(err_msg, "Error while reading executable path");
        }
        else
        {
            snprintf(fn, PATH_MAX, "%shelp/%s.md", path, help_name);

            log_debug("Reading help file: '%s'", fn);

            FILE * fp = fopen (fn, "rb");

            if (fp == NULL)
            {
                snprintf(
                        err_msg,
                        SIRIDB_MAX_SIZE_ERR_MSG,
                        "Cannot open help file: '%s'",
                        fn);
            }
            else
            {
                size_t size;

                fseeko(fp, 0, SEEK_END);
                size = ftello(fp);
                fseeko(fp, 0, SEEK_SET);

                *content = (char *) malloc (size + 1);

                if (*content == NULL)
                {
                    snprintf(
                            err_msg,
                            SIRIDB_MAX_SIZE_ERR_MSG,
                            "Memory allocation error while reading help "
                            "file: '%s'",
                            fn);
                }
                else if (fread(*content, 1, size, fp) != size)
                {
                    snprintf(
                            err_msg,
                            SIRIDB_MAX_SIZE_ERR_MSG,
                            "Error while reading help file: '%s'",
                            fn);
                    free(*content);
                    *content = NULL;
                }
                else
                {
                    (*content)[size] = '\0';
                }

                fclose(fp);


            }
        }
    }
    return *content;
}

void siri_help_free(void)
{
#ifdef DEBUG
    log_debug("Free help");
#endif

    for (uint_fast16_t i = 0; i < HELP_COUNT; i ++)
    {
        free(siri_help_content[i]);
    }
}

