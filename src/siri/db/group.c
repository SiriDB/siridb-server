/*
 * group.c - Group (saved regular expressions).
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-08-2016
 *
 */
#include <siri/db/group.h>
#include <siri/err.h>
#include <stdlib.h>
#include <siri/db/series.h>
#include <siri/db/re.h>

/*
 * Returns a group or NULL in case of an error. When this error is critical,
 * a SIGNAL is raised but in any case err_msg is set with an appropriate
 * error message.
 */
siridb_group_t * siridb_group_new(
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg)
{
    siridb_group_t * group = (siridb_group_t *) malloc(sizeof(siridb_group_t));
    if (group == NULL)
    {
        ERR_ALLOC
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        group->name = strdup(name);
        group->source = strndup(source, source_len);
        group->series = slist_new(SLIST_DEFAULT_SIZE);
        group->regex = NULL;
        group->regex_extra = NULL;

        if (    group->name == NULL ||
                group->source == NULL ||
                group->series == NULL)
        {
            ERR_ALLOC
            sprintf(err_msg, "Memory allocation error.");
            siridb_group_free(group);
            group = NULL;
        }
        else if (siridb_re_compile(
                &group->regex,
                &group->regex_extra,
                source,
                source_len,
                err_msg))
        {
            /* not critical, err_msg is set */
            siridb_group_free(group);
            group = NULL;
        }
    }
    return group;
}

/*
 * Destroy a group object. Parsing NULL is not allowed.
 */
void siridb_group_free(siridb_group_t * group)
{
    free(group->name);
    free(group->source);

    if (group->series != NULL)
    {
        for (size_t i = 0; i < group->series->len; i++)
        {
            siridb_series_decref((siridb_series_t *) group->series->data[i]);
        }
        slist_free(group->series);
    }

    free(group->regex);
    free(group->regex_extra);

    free(group);
}

/*
 * Remove dropped series from the group and shrink memory usage the the
 * series list.
 */
void siridb_group_cleanup(siridb_group_t * group)
{
    size_t dropped = 0;
    siridb_series_t * series;

    for (size_t i = 0; i < group->series->len; i++)
    {
        series = (siridb_series_t *) group->series->data[i];

        if (series->flags & SIRIDB_SERIES_IS_DROPPED)
        {
            siridb_series_decref(series);
            dropped++;
        }
        else if (dropped)
        {
            group->series->data[i - dropped] = series;
        }
    }

    group->series->len -= dropped;

    slist_compact(&group->series);
}
