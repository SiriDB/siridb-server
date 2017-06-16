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
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/tags.h>
#include <stdlib.h>
#include <siri/db/tag.h>

static void TAGS_free(siridb_tags_t * tags);

siridb_tags_t * siridb_tags_new(const char * dbpath)
{
	siridb_tags_t * tags = (siridb_tags_t *) malloc(sizeof(siridb_tags_t));
	if (tags != NULL)
	{
		tags->flags = 0;
		tags->ref = 1;
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
