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
#include <siri/cfg/cfg.h>
#include <siri/db/shard.h>
#include <siri/siri.h>

#define SIRIDB_MAX_SHARD_FN_LEN 23

static bool is_shard_fn(const char * fn)
{
    if (!isdigit(*fn) || strlen(fn) > SIRIDB_MAX_SHARD_FN_LEN)
        return false;

    fn++;
    while (*fn && isdigit(*fn))
        fn++;

    return (strlen(fn) == 4 && strncmp(fn, ".sdb", 3) == 0);
}

static bool is_temp_shard_fn(const char * fn)
{
    for (int i = 0; i < 2; i++, fn++)
        if (*fn != '_')
            return false;

    return is_shard_fn(fn);
}

int siridb_load_shards(struct siridb_s * siridb)
{
    struct stat st = {0};
    struct dirent * shard_d;
    DIR * shards_dir;
    char buffer[PATH_MAX];

    log_info("Loading shards");

    siridb_get_fn(path, SIRIDB_SHARDS_PATH);

    if (strlen(path) >= PATH_MAX - SIRIDB_MAX_SHARD_FN_LEN - 1)
    {
        log_error("Shard path too long: '%s'", path);
        return 1;
    }

    if (stat(path, &st) == -1)
    {
        log_warning(
                "Shards directory not found, creating directory '%s'.",
                path);
        if (mkdir(path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.", path);
            return 1;
        }
    }

    if ((shards_dir = opendir(path)) == NULL)
    {
        log_error("Cannot open shards directory '%s'.", path);
        return 1;
    }

    while((shard_d = readdir(shards_dir)) != NULL)
    {
        if (fstatat(dirfd(shards_dir), shard_d->d_name, &st, 0) < 0)
            continue;

        if (S_ISDIR(st.st_mode))
            continue;

        if (is_temp_shard_fn(shard_d->d_name))
        {
            snprintf(buffer, PATH_MAX, "%s%s",
                    path, shard_d->d_name);

            log_debug("Temporary shard found, we will remove file: '%s'",
                    buffer);

            if (unlink(buffer))
            {
                log_error("Could not remove temporary file: '%s'", buffer);
                closedir(shards_dir);
                return 1;
            }
        }

        if (!is_shard_fn(shard_d->d_name))
            continue;

        /* we are sure this fits since the filename is checked */
        if (siridb_shard_load(siridb, (uint64_t) atoll(shard_d->d_name)))
        {
            log_error("Error while loading shard: '%s'", shard_d->d_name);
            closedir(shards_dir);
            return 1;
        }

    }

    closedir(shards_dir);

    return 0;
}

FILE * siridb_open_shard(siridb_shard_t * shard)
{
//    siri.max_open_files
    return NULL;
}
