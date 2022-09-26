/*
 * servers.c - Collection of SiriDB servers.
 */
#include <assert.h>
#include <llist/llist.h>
#include <logger/logger.h>
#include <procinfo/procinfo.h>
#include <qpack/qpack.h>
#include <siri/db/db.h>
#include <siri/db/query.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/db/misc.h>
#include <siri/db/tee.h>
#include <siri/err.h>
#include <siri/net/promises.h>
#include <siri/net/tcp.h>
#include <siri/db/queries.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <xpath/xpath.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static int SERVERS_walk_free(siridb_server_t * server, void * args);
static int SERVERS_walk_save(siridb_server_t * server, qp_fpacker_t * fpacker);

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a signal can be raised in case of errors)
 */
int siridb_servers_load(siridb_t * siridb)
{
    log_info("Loading servers");

    qp_unpacker_t * unpacker;
    qp_obj_t qp_uuid;
    qp_obj_t qp_address;
    qp_obj_t qp_port;
    qp_obj_t qp_pool;
    siridb_server_t * server;
    qp_types_t tp;

    /* we should not have any servers at this moment */
    assert(siridb->servers == NULL);

    /* create a new server list */
    siridb->servers = llist_new();
    if (siridb->servers == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    /* get servers file name */
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_SERVERS_FN)

    if (!xpath_file_exist(fn))
    {
        /* we do not have a servers file, lets create the first server */

        server = siridb_server_new(
                (char *) siridb->uuid,
                siri.cfg->server_address,
                strlen(siri.cfg->server_address),
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
            ERR_ALLOC
            return -1;
        }

        siridb->server = server;

        if (siridb_servers_save(siridb))
        {
            log_critical("Cannot save servers");
            return -1;  /* signal is raised */
        }

        return 0;
    }

    if ((unpacker = qp_unpacker_ff(fn)) == NULL)
    {
        return -1;  /* a signal is raised in case of memory allocation errors */
    }

    /* unpacker will be freed in case macro fails */
    siridb_misc_schema_check(SIRIDB_SERVERS_SCHEMA)


    int rc = 0;

    while (qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, &qp_uuid) == QP_RAW &&
            qp_uuid.len == 16 &&
            qp_next(unpacker, &qp_address) == QP_RAW &&
            qp_next(unpacker, &qp_port) == QP_INT64 &&
            qp_next(unpacker, &qp_pool) == QP_INT64)
    {
        server = siridb_server_new(
                (const char *) qp_uuid.via.raw,
                (const char *) qp_address.via.raw,
                qp_address.len,
                (uint16_t) qp_port.via.int64,
                (uint16_t) qp_pool.via.int64);
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
                rc = -1;
            }
            else if (uuid_compare(server->uuid, siridb->uuid) == 0)
            {
                /* if this is me, bind server to siridb->server */
                siridb->server = server;
            }
            else
            {
                /* if this is not me, create promises */
                server->promises = omap_create();
                if (server->promises == NULL)
                {
                    log_critical("Memory allocation error");
                    rc = -1;
                }
            }
        }
    }

    /* save last object, should be QP_END */
    tp = qp_next(unpacker, NULL);

    /* free unpacker */
    qp_unpacker_ff_free(unpacker);

    if (siridb->server == NULL)
    {
        log_critical("Could not find my own uuid in '%s'", SIRIDB_SERVERS_FN);
        rc = -1;
    }
    else if (tp != QP_END)
    {
        log_critical("Expected end of file '%s'", fn);
        rc = -1;
    }
    else if (siridb_server_update_address(
            siridb,
            siridb->server,
            siri.cfg->server_address,
            siri.cfg->listen_backend_port) < 0)
    {
        rc = -1;  /* logging is done, set result code to -1 */
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
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_SERVERS_FN)

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
        log_info("Register '%s' as a replica server for pool %u",
                server->name,
                server->pool);

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
            assert (siridb->replicate == NULL);
            assert (siridb->fifo == NULL);
            assert (siridb->replica == NULL);
            siridb->replica = server;
            siridb->fifo = siridb_fifo_new(siridb);

            if (siridb->fifo == NULL)
            {
                log_critical(
                        "Cannot initialize fifo buffer for replica server");
                /* signal is set */
                return -1;
            }

            siridb_initsync_t * initsync = siridb_initsync_open(siridb, 1);
            if (initsync == NULL && siridb->series_map->len)
            {
                log_critical(
                        "Cannot register '%s' because creating initial "
                        "synchronization file has failed",
                        server->name);
                return -1;
            }

            if (siridb_replicate_init(siridb, initsync))
            {
                log_critical(
                        "Cannot register '%s' because replicate task "
                        "cannot be initialized",
                        server->name);
                return -1;
            }
        }

        siridb_pool_add_server(pool, server);
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
        log_info("Register '%s' for a new pool with id %u",
                server->name,
                server->pool);

        pool = siridb_pools_append(siridb->pools, server);
        if (pool == NULL)
        {
            log_critical(
                    "Cannot register '%s' because creating a new pool "
                    "has failed",
                    server->name);
            return -1;
        }

        /* this is a new server for a new pool */
        siridb->reindex = siridb_reindex_open(siridb, 1);
    }

    if (llist_append(siridb->servers, server) || siridb_servers_save(siridb))
    {
        log_critical("Cannot save server '%s'", server->name);
        ERR_ALLOC
        return -1;
    }

    siridb_server_incref(server);

    /*
     * Force one heart-beat to connect to the new server and
     * sending the updated flags to the other servers.
     */
    siri_heartbeat_force();

    if (siridb->reindex != NULL)
    {
        siridb_reindex_start(siridb->reindex->timer);
    }

    return 0;

}

/*
 * Return a llist_t object containing all servers except 'this' server.
 *
 * Note: this function does not increase the servers reference counter.
 *
 * In case of an error, NULL is returned.
 */
vec_t * siridb_servers_other2vec(siridb_t * siridb)
{
    siridb_server_t * server;
    vec_t * servers = vec_new(siridb->servers->len - 1);
    llist_node_t * node;

    if (servers == NULL)
    {
        return NULL;
    }

    for (   node = siridb->servers->first;
            node != NULL;
            node = node->next)
    {
        server = node->data;
        if (server != siridb->server)
        {
            vec_append(servers, server);
        }
    }

    return servers;
}

/*
 * This function sends a package to all online server.
 *
 * Note:a signal can be raised but the callback will always be called.
 *
 * If promises could not be created, the 'cb' function will still be called
 * using the arguments (NULL, data).
 */
void siridb_servers_send_pkg(
        vec_t * servers,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data)
{
    sirinet_promises_t * promises =
            sirinet_promises_new(servers->len, cb, data, pkg);
    if (promises == NULL)
    {
        free(pkg);
        cb(NULL, data);
    }
    else
    {
        sirinet_pkg_t * dup;
        siridb_server_t * server;
        size_t i;

        for (i = 0; i < servers->len; i++)
        {
            server = (siridb_server_t *) servers->data[i];

            if (siridb_server_is_online(server))
            {
                if ((dup = sirinet_pkg_dup(pkg)) == NULL ||
                    siridb_server_send_pkg(
                        server,
                        dup,
                        timeout,
                        (sirinet_promise_cb) sirinet_promises_on_response,
                        promises,
                        0 /* flags */))
                {
                    log_critical(
                            "Allocation error while trying to send a package "
                            "to '%s'", server->name);
                    free(dup);
                    vec_append(promises->promises, NULL);
                }
            }
            else
            {
                log_debug("Cannot send package to '%s' (server is offline)",
                        server->name);
                vec_append(promises->promises, NULL);
            }

        }
        SIRINET_PROMISES_CHECK(promises)
    }
}

siridb_server_t * siridb_servers_by_uuid(llist_t * servers, uuid_t uuid)
{
    llist_node_t * node = servers->first;
    siridb_server_t * server;

    while (node != NULL)
    {
        server = (siridb_server_t *) node->data;
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
        server = (siridb_server_t *) node->data;
        if (strcmp(server->name, name) == 0)
        {
            return server;
        }
        node = node->next;
    }
    return NULL;
}

siridb_server_t * siridb_servers_by_replica(
        llist_t * servers,
        siridb_server_t * replica)
{

    llist_node_t * node = servers->first;
    siridb_server_t * server;
    while (node != NULL)
    {
        server = (siridb_server_t *) node->data;
        if (server != replica && server->pool == replica->pool)
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
         * 'this' server will be skipped as should.
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
 * if at least one server is not online. ('this' server is NOT included)
 *
 * Server is 'online' when at least running and authenticated but not
 * queue-full
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
        node = node->next;
    }
    return 1;
}

/*
 * Returns 1 (true) if all servers are available, 0 (false)
 * if at least one server is not available. ('this' server is NOT included)
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

int siridb_servers_list(siridb_server_t * server, uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_list_t * qlist = query->data;
    vec_t * props = qlist->props;
    siridb_t * siridb = query->siridb;
    cexpr_t * where_expr = qlist->where_expr;
    size_t i;

    siridb_server_walker_t wserver = {
        .server=server,
        .siridb=siridb
    };

    if (where_expr != NULL && !cexpr_run(
            where_expr,
            (cexpr_cb_t) siridb_server_cexpr_cb,
            &wserver))
    {
        return 0;  /* false */
    }

    qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (i = 0; i < props->len; i++)
    {
        switch(*((uint32_t *) props->data[i]))
        {
        case CLERI_GID_K_ADDRESS:
            qp_add_string(query->packer, server->address);
            break;
        case CLERI_GID_K_BUFFER_PATH:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                            siridb->buffer->path :
                            (server->buffer_path != NULL) ?
                                    server->buffer_path : "");
            break;
        case CLERI_GID_K_BUFFER_SIZE:
            qp_add_int64(
                    query->packer,
                    (siridb->server == server) ?
                            siridb->buffer->size : server->buffer_size);
            break;
        case CLERI_GID_K_DBPATH:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                        siridb->dbpath :
                        (server->dbpath != NULL) ?
                            server->dbpath : "");
            break;
        case CLERI_GID_K_IP_SUPPORT:
            qp_add_string(
                    query->packer,
                    sirinet_tcp_ip_support_str((siridb->server == server) ?
                            siri.cfg->ip_support : server->ip_support));
            break;
        case CLERI_GID_K_LIBUV:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                            uv_version_string() :
                            (server->libuv != NULL) ?
                                    server->libuv : "");
            break;
        case CLERI_GID_K_NAME:
            qp_add_string(query->packer, server->name);
            break;
        case CLERI_GID_K_ONLINE:
            qp_add_type(
                    query->packer,
                    (siridb->server == server || server->client != NULL) ?
                            QP_TRUE : QP_FALSE);
            break;
        case CLERI_GID_K_POOL:
            qp_add_int64(query->packer, (int64_t) server->pool);
            break;
        case CLERI_GID_K_PORT:
            qp_add_int64(query->packer, (int64_t) server->port);
            break;
        case CLERI_GID_K_STARTUP_TIME:
            qp_add_int64(
                    query->packer,
                    (siridb->server == server) ?
                            siri.startup_time : server->startup_time);
            break;
        case CLERI_GID_K_STATUS:
            {
                char * status = siridb_server_str_status(server);
                qp_add_string(query->packer, status);
                free(status);
            }

            break;
        case CLERI_GID_K_UUID:
            {
                char uuid[37];
                uuid_unparse_lower(server->uuid, uuid);
                qp_add_string(query->packer, uuid);
            }
            break;
        case CLERI_GID_K_VERSION:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                            SIRIDB_VERSION :
                            (server->version != NULL) ?
                                    server->version : "");
            break;
        /* all properties below are 'remote properties'. if a remote property
         * is detected we should perform the query on each server and only for
         * that specific server.
         */
        case CLERI_GID_K_ACTIVE_HANDLES:
            qp_add_int64(
                    query->packer,
                    (int64_t) siri.loop->active_handles);
            break;
        case CLERI_GID_K_ACTIVE_TASKS:
            qp_add_int64(
                    query->packer,
                    (int64_t) siridb->tasks.active);
            break;
        case CLERI_GID_K_IDLE_PERCENTAGE:
            qp_add_int64(
                    query->packer,
                    siridb_get_idle_percentage(siridb));
            break;
        case CLERI_GID_K_IDLE_TIME:
            qp_add_int64(
                    query->packer,
                    (int64_t) siridb->tasks.idle_time);
            break;
        case CLERI_GID_K_LOG_LEVEL:
            qp_add_string(query->packer, Logger.level_name);
            break;
        case CLERI_GID_K_MAX_OPEN_FILES:
            qp_add_int64(
                    query->packer,
                    (int64_t) siri.cfg->max_open_files);
            break;
        case CLERI_GID_K_MEM_USAGE:
            qp_add_int64(
                    query->packer,
                    (int64_t) (procinfo_total_physical_memory() / 1024));
            break;
        case CLERI_GID_K_FIFO_FILES:
            qp_add_int64(
                    query->packer,
                    (int64_t) siridb_fifo_size(siridb->fifo));
            break;
        case CLERI_GID_K_OPEN_FILES:
            qp_add_int64(query->packer, siridb_open_files(siridb));
            break;
        case CLERI_GID_K_RECEIVED_POINTS:
            qp_add_int64(query->packer, siridb->received_points);
            break;
        case CLERI_GID_K_REINDEX_PROGRESS:
            qp_add_string(query->packer, siridb_reindex_progress(siridb));
            break;
        case CLERI_GID_K_SELECTED_POINTS:
            qp_add_int64(query->packer, siridb->selected_points);
            break;
        case CLERI_GID_K_SYNC_PROGRESS:
            qp_add_string(query->packer, siridb_initsync_sync_progress(siridb));
            break;
        case CLERI_GID_K_UPTIME:
            qp_add_int64(
                    query->packer,
                    siridb_get_uptime(siridb));
            break;
        }
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    return 1;  /* true  */
}

/*
 * Returns the numbers of servers with a version less than the given version.
 * If all servers are at least running the given version 0 is returned.
 *
 * Note: *this* server is not checked and is expected to have at least the
 *       required version.
 */
int siridb_servers_check_version(siridb_t * siridb, char * version)
{
    siridb_server_t * server;
    int n = 0;
    llist_node_t * node;

    for (   node = siridb->servers->first;
            node != NULL;
            node = node->next)
    {
        server = (siridb_server_t *) node->data;
        if (server != siridb->server)
        {
            n += (server->version == NULL ||
                  siri_version_cmp(server->version, version) < 0) ? 1 : 0;
        }
    }
    return n;
}

/*
 * Return 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
int siridb_servers_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    /* get servers file name */
    siridb_misc_get_fn(fn, siridb->dbpath, SIRIDB_SERVERS_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
    {
        ERR_FILE
        return -1;
    }

    if (
        /* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int64(fpacker, SIRIDB_SERVERS_SCHEMA))
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

static int SERVERS_walk_free(
        siridb_server_t * server,
        void * args __attribute__((unused)))
{
    siridb_server_decref(server);
    return 0;
}

static int SERVERS_walk_save(siridb_server_t * server, qp_fpacker_t * fpacker)
{
    int rc = 0;
    rc += qp_fadd_type(fpacker, QP_ARRAY4);
    rc += qp_fadd_raw(fpacker, (unsigned char *) &server->uuid[0], 16);
    rc += qp_fadd_string(fpacker, server->address);
    rc += qp_fadd_int64(fpacker, (int64_t) server->port);
    rc += qp_fadd_int64(fpacker, (int64_t) server->pool);
    return rc;
}

