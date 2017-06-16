/*
 * tag.c - Tag.
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
#include <siri/db/tag.h>
#include <stdlib.h>
#include <siri/db/series.h>

/*
 * Can be used as a callback, in other cases go for the macro.
 */
void siridb__tag_decref(siridb_tag_t * tag)
{
    if (!--tag->ref)
    {
        siridb__tag_free(tag);
    }
}

/*
 * NEVER call  this function but rather call siridb_tag_decref instead.
 *
 * Destroy a tag object. Parsing NULL is not allowed.
 */
void siridb__tag_free(siridb_tag_t * tag)
{
#ifdef DEBUG
    log_debug("Free tag: '%s'", tag->name);
#endif
    free(tag->name);
    free(tag->fn);

    if (tag->series != NULL)
    {
        imap_free(tag->series, (imap_free_cb) siridb__series_decref);
    }

    free(tag);
}
