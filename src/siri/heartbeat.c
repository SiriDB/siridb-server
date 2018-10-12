/*
 * heartbeat.c - Heart-beat task SiriDB.
 *
 * There is one and only one heart-beat task thread running for SiriDB. For
 * this reason we do not need to parse data but we should only take care for
 * locks while writing data.
 */
#include <logger/logger.h>
#include <siri/db/server.h>
#include <siri/heartbeat.h>
#include <uv.h>

static uv_timer_t heartbeat;

#define HEARTBEAT_INIT_TIMEOUT 1000

static void HEARTBEAT_cb(uv_timer_t * handle);

void siri_heartbeat_init(siri_t * siri)
{
    uint64_t repeat = siri->cfg->heartbeat_interval * 1000;
    siri->heartbeat = &heartbeat;
    uv_timer_init(siri->loop, &heartbeat);
    uv_timer_start(
            &heartbeat,
            HEARTBEAT_cb,
            HEARTBEAT_INIT_TIMEOUT,
            repeat);
}

void siri_heartbeat_stop(siri_t * siri)
{
    /* stop the timer so it will not run again */
    uv_timer_stop(&heartbeat);
    uv_close((uv_handle_t *) &heartbeat, NULL);

    /* we do not need the heart-beat anymore */
    siri->heartbeat = NULL;
}

void siri_heartbeat_force(void)
{
    HEARTBEAT_cb(NULL);
}

static void HEARTBEAT_cb(uv_timer_t * handle __attribute__((unused)))
{
    siridb_t * siridb;
    siridb_server_t * server;

    llist_node_t * siridb_node;
    llist_node_t * server_node;

    log_debug("Start heart-beat task");

    siridb_node = siri.siridb_list->first;

    while (siridb_node != NULL)
    {
        siridb = (siridb_t *) siridb_node->data;

        server_node = siridb->servers->first;
        while (server_node != NULL)
        {
            server = (siridb_server_t *) server_node->data;

            if (server != siridb->server && server->client == NULL)
            {
                siridb_server_connect(siridb, server);
            }
            else if (siridb_server_is_online(server))
            {
                siridb_server_send_flags(server);
            }

            server_node = server_node->next;
        }

        siridb_node = siridb_node->next;
    }

}

