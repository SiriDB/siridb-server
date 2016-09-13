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

#include <siri/db/re.h>

static void GROUP_free(siridb_group_t * group);

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
        group->ref = 1;
        group->flags = GROUP_FLAG_NEW;
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
            GROUP_free(group);
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
            GROUP_free(group);
            group = NULL;
        }
    }
    return group;
}

inline void siridb_group_incref(siridb_group_t * group)
{
    group->ref++;
}

void siridb_group_decref(siridb_group_t * group)
{
    if (!--group->ref)
    {
        GROUP_free(group);
    }
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

int siridb_group_test_series(siridb_group_t * group, siridb_series_t * series)
{
    /* skip if group has flags set. (DROPPED or NEW) */
    int rc = (group->flags) ? -2 : pcre_exec(
            group->regex,
            group->regex_extra,
            series->name,
            series->name_len,
            0,                     // start looking at this point
            0,                     // OPTIONS
            NULL,
            0);                    // length of sub_str_vec

    if (!rc)
    {
        if (slist_append_safe(&group->series, series))
        {
            log_critical(
                    "Cannot append series '%s' to group '%s'",
                    series->name,
                    group->name);
            rc = -1;
        }
        else
        {
            siridb_series_incref(series);
        }
    }

    return rc;
}


/*
 * Destroy a group object. Parsing NULL is not allowed.
 */
static void GROUP_free(siridb_group_t * group)
{
#ifdef DEBUG
    log_debug("Free group '%s'", group->name);
#endif
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
