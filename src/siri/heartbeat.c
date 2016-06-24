/*
 * heartbeat.c - Heart-beat task SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * There is one and only one heart-beat task thread running for SiriDB. For
 * this reason we do not need to parse data but we should only take care for
 * locks while writing data.
 *
 * changes
 *  - initial version, 17-06-2016
 *
 */
#include <siri/heartbeat.h>
#include <logger/logger.h>
#include <siri/db/server.h>
#include <uv.h>

static uv_timer_t heartbeat;

#define HEARTBEAT_INIT_TIMEOUT 1000

static void HEARTBEAT_cb(uv_timer_t * handle);

void siri_heartbeat_init(siri_t * siri)
{
    uint64_t timeout = siri->cfg->heartbeat_interval * 1000;
    siri->heartbeat = &heartbeat;
    uv_timer_init(siri->loop, &heartbeat);
    uv_timer_start(&heartbeat, HEARTBEAT_cb, HEARTBEAT_INIT_TIMEOUT, timeout);
}

void siri_heartbeat_stop(void)
{
    /* stop the timer so it will not run again */
    uv_timer_stop(&heartbeat);
    uv_close((uv_handle_t *) &heartbeat, NULL);
}

static void HEARTBEAT_cb(uv_timer_t * handle)
{
    siridb_t * siridb;
    siridb_server_t * server;

    llist_node_t * siridb_node;
    llist_node_t * server_node;

#ifdef DEBUG
    log_debug("Start heart-beat task");
#endif

    uv_mutex_lock(&siri.siridb_mutex);

    siridb_node = siri.siridb_list->first;

    while (siridb_node != NULL)
    {
        siridb = siridb_node->data;

        server_node = siridb->servers->first;
        while (server_node != NULL)
        {
            server = server_node->data;

            if (    server != siridb->server &&
                    server->socket == NULL)
            {
                siridb_server_connect(siridb, server);
            }
            else if (siridb_server_is_available(server))
            {
                siridb_server_send_flags(server);
            }

            server_node = server_node->next;
        }

        siridb_node = siridb_node->next;
    }

    uv_mutex_unlock(&siri.siridb_mutex);

}

