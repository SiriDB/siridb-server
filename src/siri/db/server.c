/*
 * server.c - SiriDB Server.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 17-03-2016
 *
 */
#include <siri/db/server.h>
#include <logger/logger.h>
#include <siri/db/query.h>
#include <uuid/uuid.h> /* install: apt-get install uuid-dev */
#include <unistd.h>
#include <assert.h>
#include <siri/cfg/cfg.h>
#include <string.h>
#include <qpack/qpack.h>

#define SIRIDB_SERVERS_FN "servers.dat"
#define SIRIDB_SERVERS_SCHEMA 1

static siridb_server_t * new_server(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool);
static void append_server(siridb_t * siridb, siridb_server_t * server);
static int save_servers(siridb_t * siridb);
static siridb_servers_t * new_servers(void);
static void free_server(siridb_server_t * server);
static void update_server_name(siridb_server_t * server);

int siridb_load_servers(siridb_t * siridb)
{
    qp_unpacker_t * unpacker;
    qp_obj_t * qp_uuid = NULL;
    qp_obj_t * qp_address = NULL;
    qp_obj_t * qp_port = NULL;
    qp_obj_t * qp_pool = NULL;
    siridb_server_t * server;
    qp_types_t tp;

    /* we should not have any servers at this moment */
    assert(siridb->servers == NULL);

    /* create a new server list */
    siridb->servers = new_servers();

    /* get servers file name */
    char fn[strlen(siridb->dbpath) + strlen(SIRIDB_SERVERS_FN) + 1];
    sprintf(fn, "%s%s", siridb->dbpath, SIRIDB_SERVERS_FN);

    if (access(fn, R_OK) == -1)
    {
        /* we do not have a servers file, lets create the first server */
        //        uuid_generate_time(uuid);

        server = new_server(
                (char *) siridb->uuid,
                siri_cfg.listen_backend_address,
                strlen(siri_cfg.listen_backend_address),
                siri_cfg.listen_backend_port,
                0);

        append_server(siridb, server);

        siridb->server = server;

        if (save_servers(siridb))
        {
            log_critical("Cannot save servers");
            return 1;
        }

        return 0;
    }

    if ((unpacker = qp_from_file_unpacker(fn)) == NULL)
        return 1;

    if (!qp_is_array(qp_next_object(unpacker)) ||
            qp_next_object(unpacker) != QP_INT64 ||
            unpacker->qp_obj->via->int64 != SIRIDB_SERVERS_SCHEMA)
    {
        log_critical("Invalid schema detected in '%s'", fn);
        qp_free_unpacker(unpacker);
        return 1;
    }

    while (qp_is_array(qp_next_object(unpacker)) &&
            qp_copy_next_object(unpacker, &qp_uuid) == QP_RAW &&
            qp_uuid->len == 16 &&
            qp_copy_next_object(unpacker, &qp_address) == QP_RAW &&
            qp_copy_next_object(unpacker, &qp_port) == QP_INT64 &&
            qp_copy_next_object(unpacker, &qp_pool) == QP_INT64)
    {
        server = new_server(
                qp_uuid->via->raw,
                qp_address->via->raw,
                qp_address->len,
                (uint16_t) qp_port->via->int64,
                (uint16_t) qp_pool->via->int64);

        /* append the server to the list */
        append_server(siridb, server);

        /* if this is me, bind server to siridb */
        if (strncmp((char *) server->uuid, (char *) siridb->uuid, 16) == 0)
            siridb->server = server;
    }

    /* save last object, should be QP_END */
    tp = qp_next_object(unpacker);

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
        return 1;
    }

    if (tp != QP_END)
    {
        log_critical("Expected end of file '%s'", fn);
        return 1;
    }

    return 0;
}

void siridb_free_servers(siridb_servers_t * servers)
{
    siridb_servers_t * next;

    while (servers != NULL)
    {
        next = servers->next;
        free_server(servers->server);
        free(servers);
        servers = next;
    }
}

int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb)
{
    int i = 0;
    for (i = 0; i < 16; i++)
        if (sa->uuid[i] < sb->uuid[i])
            return -(i + 1);
        else if (sa->uuid[i] > sb->uuid[i])
            return i + 1;
    return 0;
}

static void append_server(siridb_t * siridb, siridb_server_t * server)
{
    siridb_servers_t * servers = siridb->servers;

    if (servers->server == NULL)
    {
        servers->server = server;
        return;
    }

    while (servers->next != NULL)
        servers = servers->next;

    servers->next = (siridb_servers_t *) malloc(sizeof(siridb_servers_t));
    servers->next->server = server;
    servers->next->next = NULL;
}

static siridb_server_t * new_server(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool)
{
    siridb_server_t * server =
            (siridb_server_t *) malloc(sizeof(siridb_server_t));
    /* copy uuid */
    memcpy(server->uuid, uuid, 16);

    /* initialize with NULL, update_server_name() sets the correct name */
    server->name = NULL;

    /* copy address */
    server->address = (char *) malloc(address_len + 1);
    memcpy(server->address, address, address_len);
    server->address[address_len] = 0;

    server->port = port;
    server->pool = pool;

    /* sets address:port to name property */
    update_server_name(server);

    return server;
}

static void free_server(siridb_server_t * server)
{
    free(server->name);
    free(server->address);
    free(server);
}

static siridb_servers_t * new_servers(void)
{
    siridb_servers_t * servers =
            (siridb_servers_t *) malloc(sizeof(siridb_servers_t));
    servers->server = NULL;
    servers->next = NULL;
    return servers;
}

static void update_server_name(siridb_server_t * server)
{
    /* start len with 2, on for : and one for 0 terminator */
    size_t len = 2;
    uint16_t i = server->port;

    assert(server->port > 0);

    /* append 'string' length for server->port */
    for (; i; i /= 10, len++);

    /* append 'address' length */
    len += strlen(server->address);

    /* allocate enough space */
    server->name = (server->name == NULL) ?
            (char *) malloc(len) : (char *) realloc(server->name, len);

    /* set the name */
    sprintf(server->name, "%s:%d", server->address, server->port);
}

static int save_servers(siridb_t * siridb)
{
    FILE * fp;
    qp_packer_t * packer;
    siridb_servers_t * current = siridb->servers;

    /* get servers file name */
    char fn[strlen(siridb->dbpath) + strlen(SIRIDB_SERVERS_FN) + 1];
    sprintf(fn, "%s%s", siridb->dbpath, SIRIDB_SERVERS_FN);

    if ((fp = fopen(fn, "w")) == NULL)
        return 1;

    packer = qp_new_packer(8192);

    /* open a new array */
    qp_array_open(packer);

    /* write the current schema */
    qp_add_int8(packer, SIRIDB_SERVERS_SCHEMA);

    /* we can and should skip this if we have no users to save */
    if (current->server != NULL)
    {
        while (current != NULL)
        {
            qp_add_array4(packer);
            qp_add_raw(packer, (char *) &current->server->uuid[0], 16);
            qp_add_string(packer, current->server->address);
            qp_add_int32(packer, (int32_t) current->server->port);
            qp_add_int32(packer, (int32_t) current->server->pool);
            current = current->next;
        }
    }

    /* write output to file */
    fwrite(packer->buffer, packer->len, 1, fp);

    /* close file pointer */
    fclose(fp);

    /* free packer */
    qp_free_packer(packer);

    return 0;
}
