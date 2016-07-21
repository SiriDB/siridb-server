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
#include <assert.h>
#include <siri/version.h>
#include <siri/db/db.h>
#include <siri/net/promises.h>
#include <xpath/xpath.h>
#include <siri/err.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static void SERVERS_walk_free(siridb_server_t * server, void * args);
static int SERVERS_walk_save(siridb_server_t * server, qp_fpacker_t * fpacker);
static int SERVERS_save(siridb_t * siridb);

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a signal can be raised in case of errors)
 */
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
    if (siridb->servers == NULL)
    {
        return -1;  /* signal is raised */
    }

    /* get servers file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERVERS_FN)

    if (!xpath_file_exist(fn))
    {
        /* we do not have a servers file, lets create the first server */

        server = siridb_server_new(
                (char *) siridb->uuid,
                siri.cfg->listen_backend_address,
                strlen(siri.cfg->listen_backend_address),
                siri.cfg->listen_backend_port,
                0);
        if (server == NULL)
        {
            return -1;  /* signal is raised */
        }

        siridb_server_incref(server);
        if (llist_append(siridb->servers, server))
        {
            siridb_server_decref(server);
            return -1;  /* signal is raised */
        }

        siridb->server = server;

        if (SERVERS_save(siridb))
        {
            log_critical("Cannot save servers");
            return -1;  /* signal is raised */
        }

        return 0;
    }

    if ((unpacker = qp_unpacker_from_file(fn)) == NULL)
    {
        return -1;  /* a signal is raised in case of memory allocation errors */
    }

    /* unpacker will be freed in case macro fails */
    siridb_schema_check(SIRIDB_SERVERS_SCHEMA)

    qp_uuid = qp_object_new();
    qp_address = qp_object_new();
    qp_port = qp_object_new();
    qp_pool = qp_object_new();

    if (    qp_uuid == NULL ||
            qp_address == NULL ||
            qp_port == NULL ||
            qp_pool == NULL)
    {
        return -1;  /* signal is raised */
    }

    int rc = 0;

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
        if (server == NULL)
        {
            rc = -1;  /* signal is raised */
        }
        else
        {
            siridb_server_incref(server);

            /* append the server to the list */
            if (llist_append(siridb->servers, server))
            {
                siridb_server_decref(server);
                rc = -1;  /* signal is raised */
            }
            else if (uuid_compare(server->uuid, siridb->uuid) == 0)
            {
                /* if this is me, bind server to siridb->server */
                siridb->server = server;
            }
            else
            {
                /* if this is not me, create promises */
                server->promises = imap64_new();
                if (server->promises == NULL)
                {
                    rc = -1;  /* signal is raised */
                }
            }
        }
    }

    /* save last object, should be QP_END */
    tp = qp_next(unpacker, NULL);

    /* free objects */
    qp_object_free(qp_uuid);
    qp_object_free(qp_address);
    qp_object_free(qp_port);
    qp_object_free(qp_pool);

    /* free unpacker */
    qp_unpacker_free(unpacker);

    if (siridb->server == NULL)
    {
        log_critical("Could not find my own uuid in '%s'", SIRIDB_SERVERS_FN);
        rc = -1;
    } else if (tp != QP_END)
    {
        log_critical("Expected end of file '%s'", fn);
        rc = -1;
    }

    return rc;
}



/*
 * Destroy servers, parsing NULL is not allowed.
 */
void siridb_servers_free(llist_t * servers)
{
    llist_free_cb(servers, (llist_cb) SERVERS_walk_free, NULL);
}

/*
 * Typedef: sirinet_clserver_get_file
 *
 * Returns the length of the content for a file and set buffer with the file
 * content. Note that malloc is used to allocate memory for the buffer.
 *
 * In case of an error -1 is returned and buffer will be set to NULL.
 */
ssize_t siridb_servers_get_file(char ** buffer, siridb_t * siridb)
{
    /* get servers file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERVERS_FN)

    return xpath_get_content(buffer, fn);
}

/*
 * Returns 0 and increments server->ref by one if successful.
 *
 * In case of an error -1 is returned. (and a SIGNAL might be raised if
 * this is a critical error)
 */
int siridb_servers_register(siridb_t * siridb, siridb_server_t * server)
{
    siridb_pool_t * pool;

    if (server->pool < siridb->pools->len)
    {
        /* this is a new server for an existing pool */
        pool = siridb->pools->pool + server->pool;
        if (pool->len != 1)
        {
            log_error("Cannot register '%s' since pool %d contains %d servers",
                    server->name,
                    server->pool,
                    pool->len);
            return -1;
        }

        if (siridb->server->pool == server->pool)
        {
            /* this is a replica for 'this' pool */
#ifdef DEBUG
            assert (siridb->replicate == NULL);
            assert (siridb->fifo == NULL);
            assert (siridb->replica == NULL);
#endif
            siridb->replica = server;
            siridb->fifo = siridb_fifo_new(siridb);

            if (siridb->fifo == NULL)
            {
                log_critical(
                        "Cannot initialize fifo buffer for replica server");
                /* signal is set */
                return -1;
            }

            if (siridb_replicate_init(siridb) ||
                siridb_replicate_create(siridb))
            {
                log_critical(
                        "Cannot register '%s' because an initial replica "
                        "file could not be created",
                        server->name);
                return -1;
            }
        }
    }
    else if (server->pool > siridb->pools->len)
    {
        log_error("Cannot register '%s' since pool %d is unexpected",
                server->name,
                server->pool);
        return -1;
    }
    else
    {
        /* this is a new server for a new pool */
        assert (0);
        /* TODO: here the logic for a new pool */
    }

    siridb_pool_add_server(pool, server);
    siridb_server_incref(server);

    return 0;

}

void siridb_servers_send_pkg(
        siridb_t * siridb,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
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

        if (siridb_server_is_online(server))
        {
            siridb_server_send_pkg(
                    server,
                    pkg,
                    timeout,
                    sirinet_promises_on_response,
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

/*
 * This function can raise a SIGNAL.
 *
 * Use this to send the current server->flags to all servers.
 */
void siridb_servers_send_flags(llist_t * servers)
{
    llist_node_t * node = servers->first;
    siridb_server_t * server;

    while (node != NULL)
    {
        server = node->data;
        /*
         * The AUTHENTICATED flag is never set on 'this' server and therefore
         * skipped as should.
         */
        if (siridb_server_is_online(server))
        {
            siridb_server_send_flags(server);
        }
        node = node->next;
    }
}

/*
 * Returns 1 (true) if all servers are online, 0 (false)
 * if at least one server is online. ('this' server is NOT included)
 *
 * A server is considered  'online' when connected and authenticated.
 */
int siridb_servers_online(siridb_t * siridb)
{
    llist_node_t * node = siridb->servers->first;
    siridb_server_t * server;
    while (node != NULL)
    {
        server = (siridb_server_t *) node->data;
        if (siridb->server != server && !siridb_server_is_online(server))
        {
            return 0;
        }
    }
    return 1;
}

/*
 * Returns 1 (true) if all servers are available, 0 (false)
 * if at least one server is available. ('this' server is NOT included)
 *
 * A server is  'available' when and ONLY when connected and authenticated.
 */
int siridb_servers_available(siridb_t * siridb)
{
    llist_node_t * node = siridb->servers->first;
    siridb_server_t * server;
    while (node != NULL)
    {
        server = (siridb_server_t *) node->data;
        if (siridb->server != server && !siridb_server_is_available(server))
        {
            return 0;
        }
        node = node->next;
    }
    return 1;
}

static void SERVERS_walk_free(siridb_server_t * server, void * args)
{
    siridb_server_decref(server);
}

static int SERVERS_walk_save(siridb_server_t * server, qp_fpacker_t * fpacker)
{
    int rc = 0;
    rc += qp_fadd_type(fpacker, QP_ARRAY4);
    rc += qp_fadd_raw(fpacker, (char *) &server->uuid[0], 16);
    rc += qp_fadd_string(fpacker, server->address);
    rc += qp_fadd_int32(fpacker, (int32_t) server->port);
    rc += qp_fadd_int32(fpacker, (int32_t) server->pool);
    return rc;
}

/*
 * Return 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
static int SERVERS_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    /* get servers file name */
    SIRIDB_GET_FN(fn, SIRIDB_SERVERS_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
    {
        ERR_FILE
        return -1;
    }

    if (
        /* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int16(fpacker, SIRIDB_SERVERS_SCHEMA))
    {
        ERR_FILE
        qp_close(fpacker);
        return -1;
    }

    if (llist_walk(siridb->servers, (llist_cb) SERVERS_walk_save, fpacker))
    {
        ERR_FILE
        qp_close(fpacker);
        return -1;
    }

    /* close file pointer */
    if (qp_close(fpacker))
    {
        ERR_FILE
        return -1;
    }

    return 0;
}
