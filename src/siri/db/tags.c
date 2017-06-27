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
#include <slist/slist.h>
#include <siri/db/series.h>
#include <siri/net/protocol.h>
#include <unistd.h>
#include <siri/siri.h>

static void TAGS_free(siridb_tags_t * tags);
static int TAGS_load(siridb_t * siridb);
static int TAGS_pkg(siridb_tag_t * tag, qp_packer_t * packer);
static int TAGS_ctmap_update(siridb_tag_t * tag, ct_t * lookup);
static int TAGS_2slist(siridb_tag_t * tag, slist_t * tags_list);
static int TAGS_dropped_series(siridb_tags_t * tags, siridb_tag_t * tag);

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
	siridb->tags->tags = ct_new();
	siridb->tags->cleanup = slist_new(SLIST_DEFAULT_SIZE);
	siridb->tags->next_id = 0;

	uv_mutex_init(&siridb->tags->mutex);

	if (asprintf(
			&siridb->tags->path,
			"%s%s",
			siridb->dbpath,
			SIRIDB_TAGS_PATH) < 0 ||
			siridb->tags->tags == NULL ||
			siridb->tags->cleanup == NULL ||
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

siridb_tag_t * siridb_tags_add(siridb_tags_t * tags, const char * name)
{
	siridb_tag_t * tag = siridb_tag_new(tags->next_id++, tags->path);
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

/*
 * This function will set and unset the mutex lock.
 */
void siridb_tags_cleanup(uv_async_t * handle)
{
	siridb_tags_t * tags = (siridb_tags_t *) handle->data;
	siridb_tag_t * tag, * rmtag;

	uv_mutex_lock(&tags->mutex);

	while (tags->cleanup->len)
	{
		tag = (siridb_tag_t *) slist_pop(tags->cleanup);

		if (!tag->series->len &&
			(rmtag = (siridb_tag_t *) ct_pop(tags->tags, tag->name)) != NULL)
		{
#ifdef DEBUG
			assert(rmtag == tag && (tag->flags & TAG_FLAG_CLEANUP));
#endif
			siridb_tag_decref(rmtag);
		}
	}

	uv_mutex_unlock(&tags->mutex);

	siridb_tags_decref(tags);

	uv_close((uv_handle_t *) handle, (uv_close_cb) free);
}

ct_t * siridb_tags_lookup(siridb_tags_t * tags)
{
	ct_t * lookup = ct_new();
	if (lookup != NULL)
	{
		ct_values(tags->tags, (ct_val_cb) &TAGS_ctmap_update, lookup);
	}
	return lookup;
}

/*
 * This function is called from the "Group" thread.
 */
void siridb_tags_dropped_series(siridb_tags_t * tags)
{
	siridb_tag_t * tag;
	slist_t * tags_list;

	uv_mutex_lock(&tags->mutex);

	tags_list = slist_new(tags->tags->len);

	tags->flags &= ~TAGS_FLAG_DROPPED_SERIES;

	ct_values(tags->tags, (ct_val_cb) TAGS_2slist, tags_list);

	uv_mutex_unlock(&tags->mutex);

	while (tags_list->len)
	{
		tag = (siridb_tag_t *) slist_pop(tags_list);

		uv_mutex_lock(&tags->mutex);

		TAGS_dropped_series(tags, tag);

		siridb_tag_decref(tag);

		uv_mutex_unlock(&tags->mutex);

		usleep(10000);  // 10ms
	}

	slist_free(tags_list);

	if (tags->cleanup->len)
	{
		uv_async_t * cleanup = (uv_async_t *) malloc(sizeof(uv_async_t));

		if (cleanup == NULL)
		{
			log_critical("Allocation error while creating cleanup task");
			return;
		}
		siridb_tags_incref(tags);

		cleanup->data = (void *) tags;

		uv_async_init(siri.loop,
				cleanup,
				(uv_async_cb) siridb_tags_cleanup);
		uv_async_send(cleanup);
	}
}

void siridb_tags_save(siridb_tags_t * tags)
{
	siridb_tag_t * tag;
	slist_t * tags_list;

	uv_mutex_lock(&tags->mutex);

	tags_list = slist_new(tags->tags->len);

	tags->flags &= ~TAGS_FLAG_REQUIRE_SAVE;

	ct_values(tags->tags, (ct_val_cb) TAGS_2slist, tags_list);

	uv_mutex_unlock(&tags->mutex);

	while (tags_list->len)
	{
		tag = (siridb_tag_t *) slist_pop(tags_list);

		if (tag->flags & TAG_FLAG_REQUIRE_SAVE)
		{
			uv_mutex_lock(&tags->mutex);

			siridb_tag_save(tag);

			tag->flags &= ~ TAG_FLAG_REQUIRE_SAVE;

			uv_mutex_unlock(&tags->mutex);
		}

		siridb_tag_decref(tag);

		usleep(10000);  // 10ms
	}

	slist_free(tags_list);
}

/*
 * This function is called from the "Group" thread.
 */
static int TAGS_2slist(siridb_tag_t * tag, slist_t * tags_list)
{
    siridb_tag_incref(tag);
    slist_append(tags_list, tag);
    return 0;
}

/*
 * This function is called from the "Group" thread.
 */
static int TAGS_dropped_series(siridb_tags_t * tags, siridb_tag_t * tag)
{
	slist_t * tag_series = imap_slist_pop(tag->series);
	siridb_series_t * series, * s = NULL;

	if (tag_series != NULL)
	{
		for (size_t i = 0; i < tag_series->len; i++)
		{
			series = (siridb_series_t *) tag_series->data[i];
			if (series->flags & SIRIDB_SERIES_IS_DROPPED)
			{
				s = (siridb_series_t *) imap_pop(tag->series, series->id);
#ifdef DEBUG
				assert (s != NULL);
#endif
				siridb_series_decref(s);
			}
		}

		if (s == NULL)
		{
			/* unchanged, we can put the list back */
			tag->series->slist = tag_series;
		}
		else
		{
			slist_free(tag_series);

			if (!tag->series->len && (~tag->flags & TAG_FLAG_CLEANUP))
			{
				tag->flags |= TAG_FLAG_CLEANUP;
				if (slist_append_safe(&tags->cleanup, tag))
				{
					log_critical(
							"Unexpected error while appending tag to "
							"cleanup list");
				}
			}
		}
	}

	return 0;
}

static int TAGS_ctmap_update(siridb_tag_t * tag, ct_t * lookup)
{
	if (tag->series->len)
	{
		volatile uintptr_t iptr = (uint32_t) tag->series->len;
		return ct_add(lookup, tag->name, (uint32_t *) iptr);
	}
	return 0;
}

/*
 * Main thread.
 */
static int TAGS_pkg(siridb_tag_t * tag, qp_packer_t * packer)
{
    int rc = 0;
    rc += qp_add_type(packer, QP_ARRAY2);
    rc += qp_add_string_term(packer, tag->name);
    rc += qp_add_int64(packer, (int64_t) tag->series->len);
    return rc;
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
#ifdef DEBUG
    log_debug("Free tags");
#endif
    free(tags->path);

    if (tags->cleanup != NULL)
    {
    	slist_free(tags->cleanup);
    }

	uv_mutex_lock(&tags->mutex);

    if (tags->tags != NULL)
    {
        ct_free(tags->tags, (ct_free_cb) siridb__tag_decref);
    }

    uv_mutex_unlock(&tags->mutex);

    uv_mutex_destroy(&tags->mutex);

    free(tags);
}

