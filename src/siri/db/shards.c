/*
 * shards.c - SiriDB shards.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */

#include <siri/db/shards.h>
#include <logger/logger.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <siri/db/shard.h>
#include <siri/siri.h>

#define SIRIDB_MAX_SHARD_FN_LEN 23

static bool is_shard_fn(const char * fn);
static bool is_temp_shard_fn(const char * fn);

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a SIGNAL might be raised in case of an error)
 */
int siridb_shards_load(siridb_t * siridb)
{
    struct stat st = {0};
    struct dirent ** shard_list;
    char buffer[PATH_MAX];
    int n, total, rc = 0;

    log_info("Loading shards");

    SIRIDB_GET_FN(path, SIRIDB_SHARDS_PATH);

    if (strlen(path) >= PATH_MAX - SIRIDB_MAX_SHARD_FN_LEN - 1)
    {
        log_error("Shard path too long: '%s'", path);
        return -1;
    }

    if (stat(path, &st) == -1)
    {
        log_warning(
                "Shards directory not found, creating directory '%s'.",
                path);
        if (mkdir(path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.", path);
            return -1;
        }
    }

    total = scandir(path, &shard_list, NULL, alphasort);

    if (total < 0)
    {
        /* no need to free shard_list when total < 0 */
        log_error("Cannot read shards directory '%s'.", path);
        return -1;
    }

    for (n = 0; n < total; n++)
    {
        if (is_temp_shard_fn(shard_list[n]->d_name))
        {
            snprintf(buffer, PATH_MAX, "%s%s",
                   path, shard_list[n]->d_name);

            log_warning("Temporary shard found, we will remove file: '%s'",
                   buffer);

            if (unlink(buffer))
            {
                log_error("Could not remove temporary file: '%s'", buffer);
                rc = -1;
                break;
            }
        }

        if (!is_shard_fn(shard_list[n]->d_name))
        {
            continue;
        }

        /* we are sure this fits since the filename is checked */
        if (siridb_shard_load(siridb, (uint64_t) atoll(shard_list[n]->d_name)))
        {
           log_error("Error while loading shard: '%s'", shard_list[n]->d_name);
           rc = -1;
           break;
        }
    }

    while (total--)
    {
        free(shard_list[total]);
    }
    free(shard_list);

    return rc;
}

/*
 * Returns true if fn is a shard filename, false if not.
 */
static bool is_shard_fn(const char * fn)
{
    if (!isdigit(*fn) || strlen(fn) > SIRIDB_MAX_SHARD_FN_LEN)
    {
        return false;
    }

    fn++;
    while (*fn && isdigit(*fn))
    {
        fn++;
    }

    return (strcmp(fn, ".sdb") == 0);
}

/*
 * Returns true if fn is a temp shard filename, false if not.
 */
static bool is_temp_shard_fn(const char * fn)
{
    for (int i = 0; i < 2; i++, fn++)
    {
        if (*fn != '_')
        {
            return false;
        }
    }
    return is_shard_fn(fn);
}
