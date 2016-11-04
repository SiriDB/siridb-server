/*
 * groups.c - Groups (saved regular expressions).
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-08-2016
 *
 * Info groups->mutex:
 *
 *  Main thread:
 *      groups->groups :    read (no lock)      write (lock)
 *      groups->nseries :   read (lock)         write (lock)
 *      groups->ngroups :   read (lock)         write (lock)
 *      group->series :     read (lock)         write (not allowed)

 *  Other threads:
 *      groups->groups :    read (lock)         write (not allowed)
 *      groups->nseries :   read (lock)         write (lock)
 *      groups->ngroups :   read (lock)         write (lock)
 *      group->series :     read (no lock)      write (lock)
 *
 *  Note:   One exception to 'not allowed' are the free functions
 *          since they only run when no other references to the object exist.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/db.h>
#include <siri/db/group.h>
#include <siri/db/groups.h>
#include <siri/db/misc.h>
#include <siri/db/series.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/siri.h>
#include <slist/slist.h>
#include <stdlib.h>
#include <unistd.h>
#include <xpath/xpath.h>

#define SIRIDB_GROUPS_SCHEMA 1
#define SIRIDB_GROUPS_FN "groups.dat"
#define GROUPS_LOOP_SLEEP 2  // 2 seconds
#define GROUPS_LOOP_DEEP 15  // x times -> 30 seconds (used when re-indexing)
#define GROUPS_RE_BATCH_SZ 1000
#define CALC_BATCH_SIZE(sz) GROUPS_RE_BATCH_SZ /((sz / 5 ) + 1) + 1;

static int GROUPS_load(siridb_groups_t * groups);
static void GROUPS_free(siridb_groups_t * groups);
static int GROUPS_pkg(siridb_group_t * group, qp_packer_t * packer);
static int GROUPS_nseries(siridb_group_t * group, void * data);
static void GROUPS_loop(uv_work_t * work);
static void GROUPS_loop_finish(uv_work_t * work, int status);
static int GROUPS_write(siridb_group_t * group, qp_fpacker_t * fpacker);
static void GROUPS_init_groups(siridb_t * siridb);
static void GROUPS_init_series(siridb_t * siridb);
static int GROUPS_2slist(siridb_group_t * group, slist_t * groups_list);
static void GROUPS_cleanup(siridb_groups_t * groups);
/*
 * In case of an error the return value is NULL and a SIGNAL is raised.
 */
siridb_groups_t * siridb_groups_new(siridb_t * siridb)
{
    log_info("Loading groups");

    siridb_groups_t * groups =
            (siridb_groups_t *) malloc(sizeof(siridb_groups_t));
    if (groups == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        groups->ref = 2;  // for the main thread and for the groups thread
        groups->fn = NULL;
        groups->groups = ct_new();
        groups->nseries = slist_new(SLIST_DEFAULT_SIZE);
        groups->ngroups = slist_new(SLIST_DEFAULT_SIZE);
        uv_mutex_init(&groups->mutex);
        groups->work.data = (siridb_t *) siridb;

        if (groups->groups == NULL || groups->nseries == NULL)
        {
            GROUPS_free(groups);
            groups = NULL;  /* signal is raised */
        }
        else if (asprintf(
                    &groups->fn,
                    "%s%s",
                    siridb->dbpath,
                    SIRIDB_GROUPS_FN) < 0 || GROUPS_load(groups))
        {
            ERR_ALLOC
            GROUPS_free(groups);
            groups = NULL;
        }
        else
        {
            groups->status = GROUPS_RUNNING;
            groups->flags = 0;

        }
    }

    return groups;
}

/*
 * Start group thread.
 */
void siridb_groups_start(siridb_groups_t * groups)
{
    uv_queue_work(
            siri.loop,
            &groups->work,
            GROUPS_loop,
            GROUPS_loop_finish);
}

/*
 * Returns 0 if successful or -1 in case of an error.
 */
void siridb_groups_add_series(
        siridb_groups_t * groups,
        siridb_series_t * series)
{
    uv_mutex_lock(&groups->mutex);

    if (slist_append_safe(&groups->nseries, series) == 0)
    {
        siridb_series_incref(series);
    }
    else
    {
        log_critical("Error while initializing series '%s' for groups",
                series->name);
    }

    uv_mutex_unlock(&groups->mutex);
}

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a signal might be raised)
 */
int siridb_groups_add_group(
        siridb_groups_t * groups,
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg)
{
    int rc;

    siridb_group_t * group = siridb_group_new(
            source,
            source_len,
            err_msg);

    if (group == NULL)
	{
    	return -1; /* err_msg is set and a SIGNAL is possibly raised */
	}

    if (siridb_group_set_name(groups, group, name, err_msg))
    {
    	siridb_group_decref(group);
        return -1;  /* err_msg is set and a SIGNAL is possibly raised */
    }

    uv_mutex_lock(&groups->mutex);

    rc = ct_add(groups->groups, name, group);

    switch (rc)
    {
    case CT_EXISTS:
    	siridb_group_decref(group);
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Group '%s' already exists.",
                name);
        break;

    case CT_ERR:
    	siridb_group_decref(group);
        sprintf(err_msg, "Memory allocation error.");
        break;

    case CT_OK:
        if (slist_append_safe(&groups->ngroups, group))
        {
        	siridb_group_decref(group);
            sprintf(err_msg, "Memory allocation error.");
            rc = -1;
        }
        else
        {
            siridb_group_incref(group);
        }
        break;

    default:
        assert (0);
        break;
    }

    uv_mutex_unlock(&groups->mutex);

    return rc;
}

inline void siridb_groups_destroy(siridb_groups_t * groups)
{
    groups->status = GROUPS_STOPPING;
}

/*
 * Main thread.
 */
int siridb_groups_save(siridb_groups_t * groups)
{
    qp_fpacker_t * fpacker;

    log_debug("Write groups to file: '%s'", groups->fn);

    return (
        /* open a new user file */
        (fpacker = qp_open(groups->fn, "w")) == NULL ||

        /* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int16(fpacker, SIRIDB_GROUPS_SCHEMA) ||

        /* we can and should skip this if we have no users to save */
        ct_values(groups->groups, (ct_val_cb) GROUPS_write, fpacker) ||

        /* close file pointer */
        qp_close(fpacker)) ? EOF : 0;

}

/*
 * Typedef: sirinet_clserver_get_file
 *
 * Returns the length of the content for a file and set buffer with the file
 * content. Note that malloc is used to allocate memory for the buffer.
 *
 * In case of an error -1 is returned and buffer will be set to NULL.
 */
ssize_t siridb_groups_get_file(char ** buffer, siridb_t * siridb)
{
    return xpath_get_content(buffer, siridb->groups->fn);
}

/*
 * Initialize each 'n' group property with the local value.
 */
void siridb_groups_init_nseries(siridb_groups_t * groups)
{
    ct_values(groups->groups, (ct_val_cb) GROUPS_nseries, NULL);
}

/*
 * Main thread.
 *
 * Returns NULL and raises a signal in case of an error.
 */
sirinet_pkg_t * siridb_groups_pkg(siridb_groups_t * groups, uint16_t pid)
{
    qp_packer_t * packer = sirinet_packer_new(8192);
    int rc;

    if (packer == NULL || qp_add_type(packer, QP_ARRAY_OPEN))
    {
        return NULL;  /* signal is raised */
    }

    rc = ct_values(groups->groups, (ct_val_cb) GROUPS_pkg, packer);

    if (rc)
    {
        /*  signal is raised when not 0 */
        qp_packer_free(packer);
        return NULL;
    }

    return sirinet_packer2pkg(packer, pid, BPROTO_RES_GROUPS);
}

/*
 * Main thread.
 *
 * Returns 0 if successful or -1 when the group is not found.
 * (in case not found an error message is set)
 *
 * Note: when saving the groups to disk has failed, we log critical but
 * the function still returns 0;
 */
int siridb_groups_drop_group(
        siridb_groups_t * groups,
        const char * name,
        char * err_msg)
{
    uv_mutex_lock(&groups->mutex);

    siridb_group_t * group = (siridb_group_t *) ct_pop(groups->groups, name);

    uv_mutex_unlock(&groups->mutex);

    if (group == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Group '%s' does not exist.",
                name);
        return -1;
    }

    group->flags |= GROUP_FLAG_DROPPED;
    siridb_group_decref(group);

    if (siridb_groups_save(groups))
    {
        log_critical("Cannot save groups to file: '%s'", groups->fn);
    }

    return 0;
}

void siridb_groups_decref(siridb_groups_t * groups)
{
    if (!--groups->ref)
    {
        GROUPS_free(groups);
    }
}

/*
 * Main thread.
 */
static int GROUPS_pkg(siridb_group_t * group, qp_packer_t * packer)
{
    int rc = 0;
    rc += qp_add_type(packer, QP_ARRAY2);
    rc += qp_add_string_term(packer, group->name);
    rc += qp_add_int64(packer, group->series->len);
    return rc;
}

/*
 * Main thread.
 */
static int GROUPS_nseries(siridb_group_t * group, void * data)
{
    group->n = group->series->len;
    return 0;
}

static void GROUPS_free(siridb_groups_t * groups)
{
#ifdef DEBUG
    log_debug("Free groups");
#endif
    free(groups->fn);

    if (groups->nseries != NULL)
    {
    	siridb_series_t * series;
        for (size_t i = 0; i < groups->nseries->len; i++)
        {
        	series = (siridb_series_t *) groups->nseries->data[i];
        	siridb_series_decref(series);
        }
        slist_free(groups->nseries);
    }

    if (groups->groups != NULL)
    {
        ct_free(groups->groups, (ct_free_cb) siridb__group_decref);
    }

    if (groups->ngroups != NULL)
    {
    	siridb_group_t * group;
        for (size_t i = 0; i < groups->ngroups->len; i++)
        {
        	group = (siridb_group_t *) groups->ngroups->data[i];
            siridb_group_decref(group);
        }
        slist_free(groups->ngroups);
    }

    uv_mutex_destroy(&groups->mutex);

    free(groups);
}

/*
 * Group thread.
 */
static int GROUPS_2slist(siridb_group_t * group, slist_t * groups_list)
{
    siridb_group_incref(group);
    slist_append(groups_list, group);
    return 0;
}



/*
 * Group thread.
 */
static void GROUPS_loop(uv_work_t * work)
{
    siridb_t * siridb = (siridb_t *) work->data;
    siridb_groups_t * groups = siridb->groups;
    uint64_t mod_test = 0;

    while (groups->status != GROUPS_STOPPING)
    {
        sleep(GROUPS_LOOP_SLEEP);

        if (siridb_is_reindexing(siridb) && (++mod_test % GROUPS_LOOP_DEEP))
        {
            continue;
        }

        switch((siridb_groups_status_t) groups->status)
        {
        case GROUPS_RUNNING:
            /*
             * Order of the steps below is important. Series must be handled
             * before groups.
             */
            if (groups->nseries->len)
            {
                GROUPS_init_series(siridb);
            }
            if (groups->ngroups->len)
            {
                GROUPS_init_groups(siridb);
            }
            if (groups->flags & GROUPS_FLAG_DROPPED_SERIES)
            {
                GROUPS_cleanup(siridb->groups);
            }
            break;

        case GROUPS_STOPPING:
            break;

        default:
            assert (0);
            break;
        }
    }

    groups->status = GROUPS_CLOSED;
}

static void GROUPS_loop_finish(uv_work_t * work, int status)
{
    /*
     * Main Thread
     */
    siridb_t * siridb = (siridb_t *) work->data;

    /* decrement groups reference counter */
    siridb_groups_decref(siridb->groups);
}

static int GROUPS_load(siridb_groups_t * groups)
{
    int rc = 0;

    if (!xpath_file_exist(groups->fn))
    {
        /* no groups file, create a new one */
        return siridb_groups_save(groups);
    }

    qp_unpacker_t * unpacker = siridb_misc_open_schema_file(
            SIRIDB_GROUPS_SCHEMA,
            groups->fn);

    if (unpacker == NULL)
    {
        rc = -1;
    }
    else
    {
        qp_obj_t qp_name, qp_source;
        char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

        while ( !rc &&
                qp_is_array(qp_next(unpacker, NULL)) &&
                qp_next(unpacker, &qp_name) == QP_RAW &&
                qp_is_raw_term(&qp_name) &&
                qp_next(unpacker, &qp_source) == QP_RAW)
        {
            rc = siridb_groups_add_group(
                    groups,
                    qp_name.via.raw,
                    qp_source.via.raw,
                    qp_source.len,
                    err_msg);
        }
        qp_unpacker_ff_free(unpacker);
    }

    return rc;
}

/*
 * Main thread.
 */
static int GROUPS_write(siridb_group_t * group, qp_fpacker_t * fpacker)
{
    int rc = 0;

    rc += qp_fadd_type(fpacker, QP_ARRAY2);
    rc += qp_fadd_raw(fpacker, group->name, strlen(group->name) + 1);
    rc += qp_fadd_string(fpacker, group->source);

    return rc;
}

/*
 * Group thread.
 */
static void GROUPS_init_series(siridb_t * siridb)
{
    siridb_groups_t * groups = siridb->groups;
    siridb_series_t * series;

    uv_mutex_lock(&groups->mutex);

    /* calculate modulo size  [1..1001] */
    int m = CALC_BATCH_SIZE(groups->groups->len);

    while (groups->nseries->len)
    {
        series = (siridb_series_t *) slist_pop(groups->nseries);

        if (~series->flags & SIRIDB_SERIES_IS_DROPPED)
        {
            ct_values(
                    groups->groups,
                    (ct_val_cb) siridb_group_test_series,
                    series);
        }

        siridb_series_decref(series);

        if (groups->nseries->len % m == 0)
        {
            uv_mutex_unlock(&groups->mutex);

            usleep(10000);  // 10ms

            uv_mutex_lock(&groups->mutex);

            /* re-calculate modulo size [1..1001] */
            m = CALC_BATCH_SIZE(groups->groups->len);
        }
    }

    uv_mutex_unlock(&groups->mutex);
}

/*
 * Group thread.
 */
static void GROUPS_init_groups(siridb_t * siridb)
{
#ifdef DEBUG
    /* do not run this function when no groups need initialization */
    assert (siridb->groups->ngroups->len);
#endif

    siridb_groups_t * groups = siridb->groups;
    siridb_group_t * group;

    slist_t * series_list;
    siridb_series_t * series;
    uv_mutex_lock(&siridb->series_mutex);

    series_list = (groups->nseries->len) ?
            NULL : imap_2slist_ref(siridb->series_map);

    uv_mutex_unlock(&siridb->series_mutex);

    if (series_list == NULL)
    {
        log_info("Skip processing groups since new series are added.");
        return;
    }

    uv_mutex_lock(&groups->mutex);

    while (groups->ngroups->len)
    {
        group = (siridb_group_t *) slist_pop(groups->ngroups);

#ifdef DEBUG
        /* we must be sure this group is empty */
        assert (group->series->len == 0);
#endif

        if (~group->flags & GROUP_FLAG_DROPPED)
        {
            /* remove INIT flag from group */
            group->flags &= ~GROUP_FLAG_INIT;

            for (size_t i = 0; i < series_list->len; i++)
            {
                series = (siridb_series_t *) series_list->data[i];

                siridb_group_test_series(group, series);

                if (i % GROUPS_RE_BATCH_SZ == 0)
                {
                    uv_mutex_unlock(&groups->mutex);

                    usleep(10000);  // 10ms

                    uv_mutex_lock(&groups->mutex);
                }
            }
        }

        siridb_group_decref(group);

        uv_mutex_unlock(&groups->mutex);

        usleep(10000);  // 10ms

        uv_mutex_lock(&groups->mutex);
    }

    uv_mutex_unlock(&groups->mutex);

    for (size_t i = 0; i < series_list->len; i++)
    {
        series = (siridb_series_t *) series_list->data[i];
        siridb_series_decref(series);
    }

    slist_free(series_list);
}

/*
 * Group thread.
 */
static void GROUPS_cleanup(siridb_groups_t * groups)
{
    uv_mutex_lock(&groups->mutex);

    groups->flags &= ~GROUPS_FLAG_DROPPED_SERIES;

    slist_t * groups_list = slist_new(groups->groups->len);

    ct_values(groups->groups, (ct_val_cb) GROUPS_2slist, groups_list);

    uv_mutex_unlock(&groups->mutex);

    siridb_group_t * group;

    while (groups_list->len)
    {
        group = (siridb_group_t *) slist_pop(groups_list);

        if (!group->flags)
        {
            uv_mutex_lock(&groups->mutex);

            siridb_group_cleanup(group);

            uv_mutex_unlock(&groups->mutex);
        }

        siridb_group_decref(group);

        usleep(10000);  // 10ms
    }

    slist_free(groups_list);
}
