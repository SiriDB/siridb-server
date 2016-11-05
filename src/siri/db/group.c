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
#include <assert.h>
#include <siri/db/db.h>
#include <siri/db/group.h>
#include <siri/db/re.h>
#include <siri/db/series.h>
#include <siri/err.h>
#include <siri/grammar/grammar.h>
#include <slist/slist.h>
#include <stdlib.h>
#include <strextra/strextra.h>

#define SIRIDB_MIN_GROUP_LEN 1
#define SIRIDB_MAX_GROUP_LEN 255

/*
 * Returns a group or NULL in case of an error. When this error is critical,
 * a SIGNAL is raised but in any case err_msg is set with an appropriate
 * error message.
 */
siridb_group_t * siridb_group_new(
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
        group->n = 0;
        group->flags = GROUP_FLAG_INIT;
        group->name = NULL;
        group->source = strndup(source, source_len);
        group->series = slist_new(SLIST_DEFAULT_SIZE);
        group->regex = NULL;
        group->regex_extra = NULL;

        if (    group->source == NULL ||
                group->series == NULL)
        {
            ERR_ALLOC
            sprintf(err_msg, "Memory allocation error.");
            siridb__group_free(group);
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
        	siridb__group_free(group);
            group = NULL;
        }
    }
    return group;
}

/*
 * Returns 0 when successful or a positive value in case the name is not valid.
 * A negative value is returned and a signal is raised in case a critical error
 * has occurred.
 *
 * (err_msg is set in case of all errors)
 */
int siridb_group_set_name(
        siridb_groups_t * groups,
        siridb_group_t * group,
        const char * name,
        char * err_msg)
{
    if (strlen(name) < SIRIDB_MIN_GROUP_LEN)
    {
        sprintf(err_msg, "Group name should be at least %d characters.",
                SIRIDB_MIN_GROUP_LEN);
        return 1;
    }

    if (strlen(name) > SIRIDB_MAX_GROUP_LEN)
    {
        sprintf(err_msg, "Group name should be at most %d characters.",
                SIRIDB_MAX_GROUP_LEN);
        return 1;
    }

    if (ct_get(groups->groups, name) != NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Group '%s' already exists.",
                name);
        return 1;
    }

    if (group->name != NULL)
    {
        int rc;

        /* group already exists */
        uv_mutex_lock(&groups->mutex);

        rc  = ( ct_pop(groups->groups, group->name) == NULL ||
                ct_add(groups->groups, name, group));

        uv_mutex_unlock(&groups->mutex);

        if (rc)
        {
            ERR_C
            snprintf(err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Critical error while replacing group name '%s' with '%s' "
                    "in tree.",
                    group->name,
                    name);
            return -1;
        }
    }

    free(group->name);
    group->name = strdup(name);

    if (group->name == NULL)
    {
        ERR_ALLOC
        sprintf(err_msg, "Memory allocation error.");
        return -1;
    }

    return 0;
}

/*
 * Can be used as a callback, in other cases go for the macro.
 */
void siridb__group_decref(siridb_group_t * group)
{
    if (!--group->ref)
    {
    	siridb__group_free(group);
    }
}

/*
 * Remove dropped series from the group and shrink memory usage the the
 * series list.
 *
 * (Group thread)
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

/*
 * Group thread.
 */
int siridb_group_test_series(siridb_group_t * group, siridb_series_t * series)
{
    /* skip if group has flags set. (DROPPED or INIT) */
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
 * Returns 0 when successful or -1 in case or an error.
 *
 * Signal might be raised in case of a memory error;
 * (err_msg is always set in case of an error)
 */
int siridb_group_update_expression(
        siridb_groups_t * groups,
        siridb_group_t * group,
        const char * source,
        size_t source_len,
        char * err_msg)
{
    char * new_source = strdup(source);
    pcre * new_regex;
    pcre_extra * new_regex_extra;
    siridb_series_t * series;

    if (new_source == NULL)
    {
        ERR_ALLOC
        sprintf(err_msg, "Memory allocation error.");
        return -1;
    }

    if (siridb_re_compile(
            &new_regex,
            &new_regex_extra,
            source,
            source_len,
            err_msg))
    {
        free(new_source);
        return -1;  /* err_msg is set */
    }

    uv_mutex_lock(&groups->mutex);

    /* replace group expression */
    free(group->source);
    free(group->regex);
    free(group->regex_extra);

    group->source = new_source;
    group->regex = new_regex;
    group->regex_extra = new_regex_extra;

    for (size_t i = 0; i < group->series->len; i++)
    {
    	series = (siridb_series_t *) group->series->data[i];
    	siridb_series_decref(series);
    }

    group->series->len = 0;

    slist_compact(&group->series);

    if (~group->flags & GROUP_FLAG_INIT)
    {
        group->flags |= GROUP_FLAG_INIT;

        if (slist_append_safe(&groups->ngroups, group))
        {
            /* we log critical since allocation errors are critical, this does
             * however not influence the running SiriDB in is not critical in that
             * perspective.
             */
            log_critical("Cannot add group to list for initialization.");
        }
        else
        {
            siridb_group_incref(group);
        }
    }

    uv_mutex_unlock(&groups->mutex);

    return 0;
}

int siridb_group_cexpr_cb(siridb_group_t * group, cexpr_condition_t * cond)
{
    switch (cond->prop)
    {
    case CLERI_GID_K_NAME:
        return cexpr_str_cmp(cond->operator, group->name, cond->str);
    case CLERI_GID_K_SERIES:
        return cexpr_int_cmp(cond->operator, group->n, cond->int64);
    case CLERI_GID_K_EXPRESSION:
        return cexpr_str_cmp(cond->operator, group->source, cond->str);
    }

    log_critical("Unknown group property received: %d", cond->prop);
    assert (0);
    return -1;
}

/*
 * This function can raise a SIGNAL. In this case the packer is not filled
 * with the correct values.
 */
void siridb_group_prop(siridb_group_t * group, qp_packer_t * packer, int prop)
{
    switch (prop)
    {
    case CLERI_GID_K_NAME:
        qp_add_string(packer, group->name);
        break;
    case CLERI_GID_K_SERIES:
        qp_add_int64(packer, (int64_t) group->n);
        break;
    case CLERI_GID_K_EXPRESSION:
        qp_add_string(packer, group->source);
    }
}

/*
 * Returns true when the given property (CLERI keyword) needs a remote query
 */
int siridb_group_is_remote_prop(uint32_t prop)
{
    return (prop == CLERI_GID_K_SERIES) ? 1 : 0;
}

/*
 * NEVER call  this function but rather call siridb_group_decref instead.
 *
 * Destroy a group object. Parsing NULL is not allowed.
 */
void siridb__group_free(siridb_group_t * group)
{
#ifdef DEBUG
    log_debug("Free group: '%s'", group->name);
#endif
    free(group->name);
    free(group->source);

    if (group->series != NULL)
    {
    	siridb_series_t * series;
        for (size_t i = 0; i < group->series->len; i++)
        {
        	series = (siridb_series_t *) group->series->data[i];
            siridb_series_decref(series);
        }
        slist_free(group->series);
    }

    free(group->regex);
    free(group->regex_extra);
    free(group);
}
