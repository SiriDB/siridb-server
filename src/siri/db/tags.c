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
#include <vec/vec.h>
#include <siri/db/series.h>
#include <siri/net/protocol.h>
#include <unistd.h>
#include <siri/siri.h>

static void TAGS_free(siridb_tags_t * tags);
static int TAGS_load(siridb_t * siridb);
static int TAGS_dropped_series(
        siridb_tag_t * tag,
        void * data __attribute__((unused)));
static int TAGS_nseries(
        siridb_tag_t * tag,
        void * data __attribute__((unused)));

/*
 * Initialize tags. Returns 0 if successful or -1 in case of an error.
 */
int siridb_tags_init(siridb_t * siridb)
{
    log_info("Loading tags");
    siridb->tags = (siridb_tags_t *) malloc(sizeof(siridb_tags_t));
    if (siridb->tags == NULL)
    {
        return -1;
    }
    siridb->tags->flags = 0;
    siridb->tags->ref = 1;
    siridb->tags->next_id = 0;
    siridb->tags->tags = ct_new();

    uv_mutex_init(&siridb->tags->mutex);

    if (asprintf(
            &siridb->tags->path,
            "%s%s",
            siridb->dbpath,
            SIRIDB_TAGS_PATH) < 0 ||
            siridb->tags->tags == NULL ||
            TAGS_load(siridb))
    {
        TAGS_free(siridb->tags);
        siridb->tags = NULL;
        return -1;
    }

    return 0;
}

void siridb_tags_incref(siridb_tags_t * tags)
{
    tags->ref++;
}

void siridb_tags_decref(siridb_tags_t * tags)
{
    if (!--tags->ref)
    {
        TAGS_free(tags);
    }
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
int siridb_tags_drop_tag(
        siridb_tags_t * tags,
        const char * name,
        char * err_msg)
{
    uv_mutex_lock(&tags->mutex);

    siridb_tag_t * tag = (siridb_tag_t *) ct_pop(tags->tags, name);

    uv_mutex_unlock(&tags->mutex);

    if (tag == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Tag '%s' does not exist.",
                name);
        return -1;
    }

    tag->flags |= TAG_FLAG_CLEANUP;
    siridb_tag_decref(tag);

    return 0;
}

siridb_tag_t * siridb_tags_add_n(
        siridb_tags_t * tags,
        const char * name,
        size_t name_len)
{
    siridb_tag_t * tag = siridb_tag_new(tags, tags->next_id++);

    if (tag != NULL)
    {
        tag->name = strndup(name, name_len);
        if (tag->name == NULL || ct_add(tags->tags, tag->name, tag))
        {
            siridb_tag_decref(tag);
            tag = NULL;
        }
    }
    return tag;
}

siridb_tag_t * siridb_tags_add(siridb_tags_t * tags, const char * name)
{
    siridb_tag_t * tag = siridb_tag_new(tags, tags->next_id++);

    if (tag != NULL)
    {
        tag->name = strdup(name);
        if (tag->name == NULL || ct_add(tags->tags, name, tag))
        {
            siridb_tag_decref(tag);
            tag = NULL;
        }
    }
    return tag;
}


/*
 * This function is called from the "Group" thread.
 */
void siridb_tags_dropped_series(siridb_tags_t * tags)
{
    uv_mutex_lock(&tags->mutex);

    ct_values(tags->tags, (ct_val_cb) TAGS_dropped_series, tags);

    tags->flags &= ~TAGS_FLAG_DROPPED_SERIES;

    uv_mutex_unlock(&tags->mutex);
}

static int TAGS__save_cb(
        siridb_tag_t * tag,
        void * data __attribute__((unused)))
{
    siridb_tag_save(tag);
    tag->flags &= ~ TAG_FLAG_REQUIRE_SAVE;

    usleep(10000);  // 10ms
    return 0;
}

void siridb_tags_save(siridb_tags_t * tags)
{
    uv_mutex_lock(&tags->mutex);

    ct_values(tags->tags, (ct_val_cb) TAGS__save_cb, NULL);

    tags->flags &= ~TAGS_FLAG_REQUIRE_SAVE;

    uv_mutex_unlock(&tags->mutex);
}

/*
 * Initialize each 'n' group property with the local value.
 */
void siridb_tags_init_nseries(siridb_tags_t * tags)
{
    ct_values(tags->tags, (ct_val_cb) TAGS_nseries, NULL);
}

/*
 * Main thread.
 */
static int TAGS_pkg(siridb_tag_t * tag, qp_packer_t * packer)
{
    int rc = 0;
    rc += qp_add_type(packer, QP_ARRAY2);
    rc += qp_add_string_term(packer, tag->name);
    rc += qp_add_int64(packer, tag->series->len);
    return rc;
}

/*
 * Main thread.
 *
 * Returns NULL and raises a signal in case of an error.
 */
sirinet_pkg_t * siridb_tags_pkg(siridb_tags_t * tags, uint16_t pid)
{
    qp_packer_t * packer = sirinet_packer_new(8192);
    int rc;

    if (packer == NULL || qp_add_type(packer, QP_ARRAY_OPEN))
    {
        return NULL;  /* signal is raised */
    }

    rc = ct_values(tags->tags, (ct_val_cb) TAGS_pkg, packer);

    if (rc)
    {
        /*  signal is raised when not 0 */
        qp_packer_free(packer);
        return NULL;
    }

    return sirinet_packer2pkg(packer, pid, BPROTO_RES_TAGS);
}

typedef struct
{
    qp_packer_t * packer;
    uint64_t id;
} TAGS_series_t;

/*
 * Main thread.
 */
static int TAGS_series_pkg(siridb_tag_t * tag, TAGS_series_t * w)
{
    return imap_get(tag->series, w->id)
            ? qp_add_string(w->packer, tag->name) == 0
            : 0;
}

/*
 * Main thread.
 *
 * Returns NULL in case of an error or no tags.
 */
sirinet_pkg_t * siridb_tags_series(siridb_series_t * series)
{
    TAGS_series_t w = {
            .packer = sirinet_packer_new(1024),
            .id = series->id,
    };

    if (w.packer == NULL ||
        qp_add_type(w.packer, QP_ARRAY_OPEN) ||
        qp_add_string_term_n(w.packer, series->name, series->name_len))
    {
        return NULL;
    }

    if (ct_values(
            series->siridb->tags->tags,
            (ct_val_cb) TAGS_series_pkg,
            &w) == 0)
    {
        qp_packer_free(w.packer);
        return NULL;
    }

    return sirinet_packer2pkg(w.packer, 0, BPROTO_SERIES_TAGS);
}

/*
 * Main thread.
 */
static int TAGS_empty_tag_pkg(siridb_tag_t * tag, qp_packer_t * packer)
{
    return tag->series->len == 0
            ? qp_add_string(packer, tag->name) == 0
            : 0;
}

/*
 * Main thread.
 *
 * Returns NULL in case of an error or no empty tags.
 */
sirinet_pkg_t * siridb_tags_empty(siridb_tags_t * tags)
{
    qp_packer_t * packer = sirinet_packer_new(1024);

    if (packer == NULL || qp_add_type(packer, QP_ARRAY_OPEN))
    {
        return NULL;
    }

    if (ct_values(
            tags->tags,
            (ct_val_cb) TAGS_empty_tag_pkg,
            packer) == 0)
    {
        qp_packer_free(packer);
        return NULL;
    }

    return sirinet_packer2pkg(packer, 0, BPROTO_EMPTY_TAGS);
}

/*
 * Main thread.
 */
static int TAGS_nseries(
        siridb_tag_t * tag,
        void * data __attribute__((unused)))
{
    tag->n = tag->series->len;
    return 0;
}

/*
 * This function is called from the "Group" thread.
 */
static int TAGS_dropped_series(
        siridb_tag_t * tag,
        void * data __attribute__((unused)))
{
    vec_t * tag_series = imap_vec_pop(tag->series);
    siridb_series_t * series;

    if (tag_series != NULL)
    {
        size_t i;
        for (i = 0; i < tag_series->len; i++)
        {
            series = (siridb_series_t *) tag_series->data[i];
            if ((series->flags & SIRIDB_SERIES_IS_DROPPED) &&
                imap_pop(tag->series, series->id))
            {
                siridb_series_decref(series);
                siridb_tags_set_require_save(tag->tags, tag);
            }
        }

        vec_free(tag_series);
    }

    usleep(10000);  // 10ms

    return 0;
}

static int TAGS_load(siridb_t * siridb)
{
    struct stat st = {0};
    struct dirent ** tags_list;
    int total, n, rc;
    siridb_tag_t * tag;

    if (stat(siridb->tags->path, &st) == -1)
    {
        log_warning(
                "Tags directory not found, creating directory '%s'.",
                siridb->tags->path);
        if (mkdir(siridb->tags->path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.", siridb->tags->path);
            return -1;
        }
    }

    total = scandir(siridb->tags->path, &tags_list, NULL, alphasort);

    if (total < 0)
    {
        /* no need to free tags_list when total < 0 */
        log_error("Cannot read tags directory '%s'.", siridb->tags->path);
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
        tag = siridb_tag_load(siridb, tags_list[n]->d_name);
        if (tag == NULL)
        {
           log_error("Error while loading tag: '%s'", tags_list[n]->d_name);
           rc = -1;
           break;
        }

        if (!tag->series->len)
        {
            log_warning("Removing tag '%s' since it has no series", tag->name);
            tag->flags |= TAG_FLAG_CLEANUP;
            siridb_tag_decref(tag);
            continue;
        }

        if (ct_add(siridb->tags->tags, tag->name, tag))
        {
            log_error("Cannot add tag to collection");
            siridb_tag_decref(tag);
            rc = -1;
            break;
        }

        if (tag->id >= siridb->tags->next_id)
        {
            siridb->tags->next_id = tag->id + 1;
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
    free(tags->path);

    uv_mutex_lock(&tags->mutex);

    if (tags->tags != NULL)
    {
        ct_free(tags->tags, (ct_free_cb) siridb__tag_decref);
    }

    uv_mutex_unlock(&tags->mutex);

    uv_mutex_destroy(&tags->mutex);

    free(tags);
}
