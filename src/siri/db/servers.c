/*
 * servers.c - SiriDB Servers.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-06-2016
 *
 */
#include <siri/db/servers.h>
#include <siri/db/server.h>
#include <llist/llist.h>
#include <siri/siri.h>
#include <qpack/qpack.h>
#include <logger/logger.h>
#include <unistd.h>
#include <assert.h>
#include <siri/version.h>
#include <siri/db/db.h>


#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static void SERVERS_walk_free(siridb_server_t * server, void * args);
static void SERVERS_walk_save(siridb_server_t * server, qp_fpacker_t * fpacker);
static int SERVERS_save(siridb_t * siridb);

int siridb_servers_load(siridb_t * siridb)
{
    log_info("Loading servers");

    qp_unpacker_t * unpacker;
    qp_obj_t * qp_uuid;
    qp_obj_t * qp_address;
    qp_obj_t * qp_port;
    qp_obj_t * qp_pool;
    siridb_server_t * server;
    qp_types_t tp;

    /* we should not have any servers at this moment */
    assert(siridb->servers == NULL);

    /* create a new server list */
    siridb->servers = llist_new();

    /* get servers file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERVERS_FN)

    if (access(fn, R_OK) == -1)
    {
        /* we do not have a servers file, lets create the first server */
        //        uuid_generate_time(uuid);

        server = siridb_server_new(
                (char *) siridb->uuid,
                siri.cfg->listen_backend_address,
                strlen(siri.cfg->listen_backend_address),
                siri.cfg->listen_backend_port,
                0);

        llist_append(siridb->servers, server);
        siridb_server_incref(server);

        siridb->server = server;

        if (SERVERS_save(siridb))
        {
            log_critical("Cannot save servers");
            return -1;
        }

        return 0;
    }

    if ((unpacker = qp_from_file_unpacker(fn)) == NULL)
    {
        return -1;
    }

    /* unpacker will be freed in case macro fails */
    siridb_schema_check(SIRIDB_SERVERS_SCHEMA)

    qp_uuid = qp_object_new();
    qp_address = qp_object_new();
    qp_port = qp_object_new();
    qp_pool = qp_object_new();

    while (qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, qp_uuid) == QP_RAW &&
            qp_uuid->len == 16 &&
            qp_next(unpacker, qp_address) == QP_RAW &&
            qp_next(unpacker, qp_port) == QP_INT64 &&
            qp_next(unpacker, qp_pool) == QP_INT64)
    {
        server = siridb_server_new(
                qp_uuid->via->raw,
                qp_address->via->raw,
                qp_address->len,
                (uint16_t) qp_port->via->int64,
                (uint16_t) qp_pool->via->int64);

        /* append the server to the list */
        llist_append(siridb->servers, server);
        siridb_server_incref(server);

        /* if this is me, bind server to siridb and update the version
         * or else create a client.
         */
        if (uuid_compare(server->uuid, siridb->uuid) == 0)
        {
            siridb->server = server;
        }
        else
        {
            server->promises = imap64_new();
        }
    }

    /* save last object, should be QP_END */
    tp = qp_next(unpacker, NULL);

    /* free objects */
    qp_free_object(qp_uuid);
    qp_free_object(qp_address);
    qp_free_object(qp_port);
    qp_free_object(qp_pool);

    /* free unpacker */
    qp_free_unpacker(unpacker);

    if (siridb->server == NULL)
    {
        log_critical("Could not find my own uuid in '%s'", SIRIDB_SERVERS_FN);
        return -1;
    }

    if (tp != QP_END)
    {
        log_critical("Expected end of file '%s'", fn);
        return -1;
    }

    return 0;
}

void siridb_servers_free(llist_t * servers)
{
    llist_free_cb(servers, (llist_cb_t) SERVERS_walk_free, NULL);
}

void siridb_servers_send_pkg(
        siridb_t * siridb,
        uint32_t len,
        uint16_t tp,
        const char * content,
        uint64_t timeout,
        sirinet_promises_cb_t cb,
        void * data)
{
    sirinet_promises_t * promises =
            sirinet_promises_new(siridb->servers->len - 1, cb, data);

    siridb_server_t * server;

    for (   llist_node_t * node = siridb->servers->first;
            node != NULL;
            node = node->next)
    {
        server = node->data;
        if (server == siridb->server)
        {
            continue;
        }

        if (siridb_server_is_available(server))
        {
            siridb_server_send_pkg(
                    server,
                    len,
                    tp,
                    content,
                    timeout,
                    sirinet_promise_on_response,
                    promises);
        }
        else
        {
            log_debug("Cannot send package to '%s' (server is offline)",
                    server->name);
            slist_append(promises->promises, NULL);
        }

    }
    SIRINET_PROMISES_CHECK(promises)
}

siridb_server_t * siridb_servers_by_uuid(llist_t * servers, uuid_t uuid)
{
    llist_node_t * node = servers->first;
    siridb_server_t * server;

    while (node != NULL)
    {
        server = node->data;
        if (uuid_compare(server->uuid, uuid) == 0)
        {
            return server;
        }
        node = node->next;
    }
    return NULL;
}

siridb_server_t * siridb_servers_by_name(llist_t * servers, const char * name)
{
    llist_node_t * node = servers->first;
    siridb_server_t * server;

    while (node != NULL)
    {
        server = node->data;
        if (strcmp(server->name, name) == 0)
        {
            return server;
        }
        node = node->next;
    }
    return NULL;
}

static void SERVERS_walk_free(siridb_server_t * server, void * args)
{
    siridb_server_decref(server);
}

static void SERVERS_walk_save(siridb_server_t * server, qp_fpacker_t * fpacker)
{
    qp_fadd_type(fpacker, QP_ARRAY4);
    qp_fadd_raw(fpacker, (char *) &server->uuid[0], 16);
    qp_fadd_string(fpacker, server->address);
    qp_fadd_int32(fpacker, (int32_t) server->port);
    qp_fadd_int32(fpacker, (int32_t) server->pool);
}

static int SERVERS_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    /* get servers file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERVERS_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
        return 1;

    /* open a new array */
    qp_fadd_type(fpacker, QP_ARRAY_OPEN);

    /* write the current schema */
    qp_fadd_int16(fpacker, SIRIDB_SERVERS_SCHEMA);

    llist_walk(siridb->servers, (llist_cb_t) SERVERS_walk_save, fpacker);

    /* close file pointer */
    qp_close(fpacker);

    return 0;
}
