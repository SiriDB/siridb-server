/*
 * help.c - Help for SiriDB.
 */
#include <limits.h>
#include <logger/logger.h>
#include <siri/db/db.h>
#include <siri/help/help.h>
#include <stdio.h>
#include <xpath/xpath.h>

static char * siri_help_content[HELP_COUNT] = {0};

/*
 * Returns the help mark-down content from cache if possible or otherwise
 * the help file will be read from disk.
 *
 * In case of an error NULL is returned and an error message is set.
 */
const char * siri_help_get(
        uint16_t gid,
        const char * help_name,
        char * err_msg)
{
    char ** content = &siri_help_content[gid - HELP_OFFSET];

    if (*content == NULL)
    {
        /*
         * path must be initialized for xpath_get_exec_path to handle this variable
         * correctly.
         */
        char path[XPATH_MAX];
        char fn[XPATH_MAX];

        memset(&path, 0, sizeof(path));

        if (xpath_get_exec_path(path))
        {
            sprintf(err_msg, "Error while reading executable path");
        }
        else
        {
            if (help_name[0] == '?')
            {
                snprintf(
                        fn,
                        XPATH_MAX,
                        "%shelp/help%s.md",
                        path,
                        help_name + 1);
            }
            else
            {
                snprintf(fn, XPATH_MAX, "%shelp/%s.md", path, help_name);
            }

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

/*
 * Destroy the help. (free help content in cache)
 */
void siri_help_free(void)
{
    uint_fast16_t i;

    for (i = 0; i < HELP_COUNT; i ++)
    {
        free(siri_help_content[i]);
    }
}

