/*
 * buffersync.c - Buffer sync.
 */
#include <logger/logger.h>
#include <siri/db/server.h>
#include <siri/db/buffer.h>
#include <siri/buffersync.h>
#include <uv.h>


static uv_timer_t buffersync;

#define BUFFERSYNC_INIT_TIMEOUT 1000

static void BUFFERSYNC_cb(uv_timer_t * handle);

void siri_buffersync_init(siri_t * siri)
{
    uint64_t repeat = (uint64_t) siri->cfg->buffer_sync_interval;
    if (repeat == 0)
    {
        siri->buffersync = NULL;
        return;
    }
    siri->buffersync = &buffersync;
    uv_timer_init(siri->loop, &buffersync);
    uv_timer_start(
            &buffersync,
            BUFFERSYNC_cb,
            BUFFERSYNC_INIT_TIMEOUT,
            repeat < 100 ? 100 : repeat);
}

void siri_buffersync_stop(siri_t * siri)
{
    if (siri->buffersync != NULL)
    {
        /* stop the timer so it will not run again */
        uv_timer_stop(&buffersync);
        uv_close((uv_handle_t *) &buffersync, NULL);
        siri->buffersync = NULL;
    }
}


static void BUFFERSYNC_cb(uv_timer_t * handle __attribute__((unused)))
{
    siridb_t * siridb;
    llist_node_t * siridb_node;

    siridb_node = siri.siridb_list->first;

    while (siridb_node != NULL)
    {
        siridb = (siridb_t *) siridb_node->data;

        /* flush the buffer, maybe on each insert or another interval? */
        if (siridb_buffer_fsync(siridb->buffer))
        {
            log_critical("fsync() has failed on the buffer file");
        }

        siridb_node = siridb_node->next;
    }
}

