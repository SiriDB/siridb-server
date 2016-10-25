/*
 * backup.c - Set SiriDB in backup mode.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 27-09-2016
 *
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/backup.h>
#include <siri/db/replicate.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/db/shard.h>
#include <siri/optimize.h>
#include <siri/siri.h>
#include <stddef.h>

uv_timer_t backup;

#define BACKUP_LOOP_TIMEOUT 3000

static void BACKUP_cb(uv_timer_t * timer);
static void BACKUP_walk(siridb_t * siridb, void * args);

void siri_backup_init(siri_t * siri)
{
    siri->backup = &backup;
    siri->backup->data = llist_new();

    uv_timer_init(siri->loop, siri->backup);
}

void siri_backup_destroy(siri_t * siri)
{
    llist_t * llist = (llist_t *) siri->backup->data;

    if (llist != NULL)
    {
        if (llist->len)
        {
            uv_timer_stop(siri->backup);
        }

        llist_free_cb(llist, (llist_cb) siridb_decref_cb, NULL);
    }

    uv_close((uv_handle_t *) siri->backup, NULL);
}

/*
 * Returns 0 when successful or -1 and a signal is raised in case of an error.
 */
int siri_backup_enable(siri_t * siri, siridb_t * siridb)
{
#ifdef DEBUG
    assert (~siridb->server->flags & SERVER_FLAG_BACKUP_MODE);
    assert (~siridb->server->flags & SERVER_FLAG_REINDEXING);
#endif

    llist_t * llist = (llist_t *) siri->backup->data;

    if (llist_append(llist, siridb))
    {
        return -1;  /* a signal is raised */
    }

    siridb_incref(siridb);

    siridb->server->flags |= SERVER_FLAG_BACKUP_MODE;

    siridb_servers_send_flags(siridb->servers);

    if (~siridb->server->flags & SERVER_FLAG_SYNCHRONIZING)
    {
        siri_optimize_pause();
    }

    if (siridb->replicate != NULL)
    {
        siridb_replicate_pause(siridb->replicate);
    }

    if (llist->len == 1)
    {
        uv_timer_start(
                siri->backup,
                BACKUP_cb,
                BACKUP_LOOP_TIMEOUT,
                BACKUP_LOOP_TIMEOUT);
    }

    return 0;
}

int siri_backup_disable(siri_t * siri, siridb_t * siridb)
{
#ifdef DEBUG
    assert (siridb->server->flags & SERVER_FLAG_BACKUP_MODE);
#endif

    int rc = 0;

    llist_t * llist = (llist_t *) siri->backup->data;

    if ((siridb_t *) llist_remove(llist, NULL, siridb) == siridb)
    {
        siridb_decref(siridb);
    }
    else
    {
        log_critical("Cannot find SiriDB in backup list");
        rc = -1;
    }

    siridb->server->flags &= ~SERVER_FLAG_BACKUP_MODE;

    if (siridb->fifo != NULL)
    {
        if (siridb->fifo->in->fp == NULL && siridb_fifo_open(siridb->fifo))
        {
            log_critical("Cannot open fifo file");
            rc = -1;
        }
    }

    if (siridb->replicate == NULL)
    {
        /*
         * Optimize will be continued when synchronization has finished or now
         * if we do not have a replica server.
         */
        siri_optimize_continue();
    }
    else
    {
        siridb->server->flags |= SERVER_FLAG_SYNCHRONIZING;
        siridb_replicate_continue(siridb->replicate);
    }

    if (llist->len == 0)
    {
        uv_timer_stop(siri->backup);
    }

    siridb_servers_send_flags(siridb->servers);

    return rc;
}

static void BACKUP_cb(uv_timer_t * timer)
{
    llist_walk((llist_t *) timer->data, (llist_cb) BACKUP_walk, NULL);
}

static void BACKUP_walk(siridb_t * siridb, void * args)
{
    if (    siridb->replicate != NULL &&
            siridb->replicate->status == REPLICATE_PAUSED &&
            siridb->fifo != NULL &&
            siridb->fifo->in->fp != NULL)
    {
        siridb_fifo_close(siridb->fifo);
    }

    if (siridb->buffer_fp != NULL)
    {
        if (fclose(siridb->buffer_fp) == 0)
        {
            siridb->buffer_fp = NULL;
        }
        else
        {
            log_critical("Cannot close buffer file");
        }
    }

    if (siridb->dropped_fp != NULL)
    {
        if (fclose(siridb->dropped_fp) == 0)
        {
            siridb->dropped_fp = NULL;
        }
        else
        {
            log_critical("Cannot close dropped file");
        }
    }

    if (siridb->store != NULL)
    {
        if (qp_close(siridb->store) == 0)
        {
            siridb->store = NULL;
        }
        else
        {
            log_critical("Cannot close store file");
        }
    }

    if (SIRI_OPTIMZE_IS_PAUSED)
    {
        siridb_shard_t * shard;

        /*
         * A lock is not needed since the optimize thread is paused and this
         * is running from the main thread.
         */
        slist_t * shard_list = imap_2slist(siridb->shards);

        for (size_t i = 0; i < shard_list->len; i++)
        {
            shard = (siridb_shard_t *) shard_list->data[i];

            if (shard->fp->fp != NULL)
            {
                siri_fp_close(shard->fp);
            }

            if (shard->replacing != NULL && shard->replacing->fp->fp != NULL)
            {
                siri_fp_close(shard->replacing->fp);
            }
        }

        slist_free(shard_list);
    }
}
