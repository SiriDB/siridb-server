/*
 * tags.c - Tags.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-06-2017
 *
 */
#define _GNU_SOURCE
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/tags.h>
#include <stdlib.h>
#include <siri/db/tag.h>

static void TAGS_free(siridb_tags_t * tags);

/*
 * Return a new siridb_tags_t. In case of an error, NULL is returned.
 * (in case of an error, a signal might be raised)
 */
siridb_tags_t * siridb_tags_new(siridb_t * siridb)
{
	siridb_tags_t * tags = (siridb_tags_t *) malloc(sizeof(siridb_tags_t));
	if (tags != NULL)
	{
		tags->flags = 0;
		tags->ref = 1;
		tags->tags = ct_new();
		tags->next_id = 0;

		uv_mutex_init(&tags->mutex);

		if (asprintf(
				tags->path,
				"%s%s",
				siridb->dbpath,
				SIRIDB_TAGS_PATH) < 0 ||
			tags->tags == NULL ||
			TAGS_load(siridb))
		{
			TAGS_free(tags);
			tags = NULL;
		}
	}
	return tags;
}

void siridb_tags_decref(siridb_tags_t * tags)
{
    if (!--tags->ref)
    {
        TAGS_free(tags);
    }
}

static int TAGS_load(siridb_t * siridb)
{
    struct stat st = {0};
    struct dirent ** tags_list;
    int total, n, rc;

    if (stat(tags->path, &st) == -1)
    {
        log_warning(
                "Tags directory not found, creating directory '%s'.",
				tags->path);
        if (mkdir(tags->path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.", tags->path);
            return -1;
        }
    }

    total = scandir(tags->path, &tags_list, NULL, alphasort);

    if (total < 0)
    {
        /* no need to free tags_list when total < 0 */
        log_error("Cannot read tags directory '%s'.", tags->path);
        return -1;
    }

    rc = 0;

    for (n = 0; n < total; n++)
	{
		if (!siridb_tag_is_valid_fn(tags_list[n]->d_name))
		{
			continue;
		}

		/* we are sure this fits since the filename is checked */
		if (siridb_tag_load(siridb, tags_list[n]->d_name))
		{
		   log_error("Error while loading tag: '%s'", tags_list[n]->d_name);
		   rc = -1;
		   break;
		}
	}

	while (total--)
	{
		free(tags_list[total]);
	}

	free(tags_list);

	return rc;
}

static void TAGS_free(siridb_tags_t * tags)
{
#ifdef DEBUG
    log_debug("Free tags");
#endif
    free(tags->path);

    if (tags->tags != NULL)
    {
        ct_free(tags->tags, (ct_free_cb) siridb__tag_decref);
    }

    uv_mutex_destroy(&tags->mutex);

    free(tags);
}

