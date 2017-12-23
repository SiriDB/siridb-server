/*
 * client.c - Client for expanding a siridb database.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 24-03-2017
 *
 */
#include <siri/admin/client.h>
#include <siri/siri.h>
#include <logger/logger.h>
#include <siri/net/socket.h>
#include <string.h>
#include <siri/net/protocol.h>
#include <siri/admin/request.h>
#include <stdarg.h>
#include <lock/lock.h>
#include <siri/db/server.h>
#include <siri/version.h>

// 15 seconds
#define CLIENT_REQUEST_TIMEOUT 15000
#define CLIENT_FLAGS_TIMEOUT 1
#define CLIENT_FLAGS_NO_ROLLBACK 2
#define MAX_VERSION_LEN 12

enum
{
    CLIENT_REQUEST_INIT,
    CLIENT_REQUEST_STATUS,
    CLIENT_REQUEST_POOLS,
    CLIENT_REQUEST_FILE_USERS,
    CLIENT_REQUEST_FILE_GROUPS,
    CLIENT_REQUEST_FILE_SERVERS,
    CLIENT_REQUEST_FILE_DATABASE,
    CLIENT_REQUEST_REGISTER_SERVER
};

static void CLIENT_write_cb(uv_write_t * req, int status);
static void CLIENT_on_connect(uv_connect_t * req, int status);
static void CLIENT_on_data(uv_stream_t * client, sirinet_pkg_t * pkg);
static void CLIENT_request_timeout(uv_timer_t * handle);
static void CLIENT_on_auth_success(siri_admin_client_t * adm_client);
static int CLIENT_resolve_dns(
        siri_admin_client_t * adm_client,
        int ai_family,
        char * err_msg);
static void CLIENT_on_resolved(
        uv_getaddrinfo_t * resolver,
        int status,
        struct addrinfo * res);
static void CLIENT_err(
        siri_admin_client_t * adm_client,
        const char * fmt,
        ...);
static void CLIENT_send_pkg(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_error_msg(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_register_server(siri_admin_client_t * adm_client);
static void CLIENT_on_file_database(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_file_servers(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_file_groups(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_file_users(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_request_pools(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);
static void CLIENT_on_request_status(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg);

/*
 * Initializes a request for a new pool or replica and returns 0 when
 * successful. In case of an error, err_msg will be set and a value other than
 * 0 is returned. (a signal can be raised.) When successful and only when
 * successful, a response will be send to the provided client using the given
 * pid.
 */
int siri_admin_client_request(
        uint16_t pid,
        uint16_t port,
        int pool,
        uuid_t * uuid,
        qp_obj_t * host,
        qp_obj_t * username,
        qp_obj_t * password,
        qp_obj_t * dbname,
        const char * dbpath,
        uv_stream_t * client,
        char * err_msg)
{
    sirinet_socket_t * ssocket;
    siri_admin_client_t * adm_client;
    struct in_addr sa;
    struct in6_addr sa6;

    if (siri.socket != NULL)
    {
        sprintf(err_msg, "manage socket already in use");
        return -1;
    }

    siri.socket = sirinet_socket_new(SOCKET_MANAGE, &CLIENT_on_data);
    if (siri.socket == NULL)
    {
        sprintf(err_msg, "memory allocation error");
        return -1;
    }

    uv_tcp_init(siri.loop, siri.socket);

    adm_client = (siri_admin_client_t *) malloc(sizeof(siri_admin_client_t));
    if (adm_client == NULL)
    {
        sirinet_socket_decref(siri.socket);
        sprintf(err_msg, "memory allocation error");
        return -1;
    }
    adm_client->pid = pid;
    adm_client->port = port;
    adm_client->host = strndup(
            (const char *) host->via.raw, host->len);
    adm_client->username = strndup(
            (const char *) username->via.raw, username->len);
    adm_client->password = strndup(
            (const char *) password->via.raw, password->len);
    adm_client->dbname = strndup(
            (const char *) dbname->via.raw, dbname->len);
    adm_client->dbpath = strdup(dbpath);
    adm_client->client = client;
    adm_client->request = CLIENT_REQUEST_INIT;
    adm_client->flags = 0;
    adm_client->pool = pool;
    memcpy(&adm_client->uuid, uuid, 16);

    ssocket = (sirinet_socket_t *) siri.socket->data;
    ssocket->origin = (void *) adm_client;

    sirinet_socket_incref(adm_client->client);

    if (adm_client->host == NULL ||
        adm_client->username == NULL ||
        adm_client->password == NULL ||
        adm_client->dbname == NULL ||
        adm_client->dbpath == NULL)
    {
        sirinet_socket_decref(siri.socket);
        sprintf(err_msg, "memory allocation error");
        return -1;
    }

    if (inet_pton(AF_INET, adm_client->host, &sa))
    {
        /* IPv4 */
        struct sockaddr_in dest;

        uv_connect_t * req = (uv_connect_t *) malloc(sizeof(uv_connect_t));
        if (req == NULL)
        {
            sirinet_socket_decref(siri.socket);
            sprintf(err_msg, "memory allocation error");
            return -1;
        }
        log_debug(
                "Trying to connect to '%s:%u'...",
                adm_client->host,
                adm_client->port);

        uv_ip4_addr(adm_client->host, adm_client->port, &dest);
        uv_tcp_connect(
                req,
                siri.socket,
                (const struct sockaddr *) &dest,
                CLIENT_on_connect);
    }
    else if (inet_pton(AF_INET6, adm_client->host, &sa6))
    {
        /* IPv6 */
        struct sockaddr_in6 dest6;

        uv_connect_t * req = (uv_connect_t *) malloc(sizeof(uv_connect_t));
        if (req == NULL)
        {
            sirinet_socket_decref(siri.socket);
            sprintf(err_msg, "memory allocation error");
            return -1;
        }
        log_debug(
                "Trying to connect to '%s:%u'...",
                adm_client->host,
                adm_client->port);
        uv_ip6_addr(adm_client->host, adm_client->port, &dest6);
        uv_tcp_connect(
                req,
                siri.socket,
                (const struct sockaddr *) &dest6,
                CLIENT_on_connect);
    }
    else
    {
        if (CLIENT_resolve_dns(
                adm_client,
                dns_req_family_map[siri.cfg->ip_support],
                err_msg))
        {
            sirinet_socket_decref(siri.socket);
            return -1;  /* err_msg is set */
        }
    }
    uv_timer_init(siri.loop, &siri.timer);
    return 0;
}

/*
 * Destroy the client request. (will be called when the socket is destroyed)
 */
void siri_admin_client_free(siri_admin_client_t * adm_client)
{
    if (adm_client != NULL)
    {
        sirinet_socket_decref(adm_client->client);
        free(adm_client->host);
        free(adm_client->username);
        free(adm_client->password);
        free(adm_client->dbname);
        free(adm_client->dbpath);
        free(adm_client);
    }
}
/*
 * Try to get an ip address from dns.
 *
 * This function should return 0 when we can start an attempt for discovering
 * dns. When the result is not zero, CLIENT_on_resolved will not be called and
 * err_msg is set.
 */
static int CLIENT_resolve_dns(
        siri_admin_client_t * adm_client,
        int ai_family,
        char * err_msg)
{
    struct addrinfo hints;
    hints.ai_family = ai_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;

    uv_getaddrinfo_t * resolver =
            (uv_getaddrinfo_t *) malloc(sizeof(uv_getaddrinfo_t));

    if (resolver == NULL)
    {
        sprintf(err_msg, "memory allocation error");
        return -1;
    }

    int result;
    resolver->data = adm_client;

    char port[6]= {'\0'};
    sprintf(port, "%u", adm_client->port);

    result = uv_getaddrinfo(
            siri.loop,
            resolver,
            (uv_getaddrinfo_cb) CLIENT_on_resolved,
            adm_client->host,
            port,
            &hints);

    if (result)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "getaddrinfo call error %s",
                uv_err_name(result));
        free(resolver);
    }

    return result;
}

/*
 * Callback used to check if resolving an ip address was successful.
 */
static void CLIENT_on_resolved(
        uv_getaddrinfo_t * resolver,
        int status,
        struct addrinfo * res)
{
    siri_admin_client_t * adm_client = (siri_admin_client_t *) resolver->data;

    if (status < 0)
    {
        CLIENT_err(
                adm_client,
                "cannot resolve ip address for '%s' (error: %s)",
                adm_client->host,
                uv_err_name(status));
    }
    else
    {
        uv_connect_t * req = (uv_connect_t *) malloc(sizeof(uv_connect_t));
        if (req == NULL)
        {
            CLIENT_err(adm_client, "memory allocation error");
        }
        else
        {
            uv_tcp_connect(
                    req,
                    siri.socket,
                    (const struct sockaddr *) res->ai_addr,
                    CLIENT_on_connect);
        }
    }

    uv_freeaddrinfo(res);
    free(resolver);
}

/*
 * In case a client request fails, this function should be called to end
 * the request. A package with the error message will be send to the client.
 */
static void CLIENT_err(
        siri_admin_client_t * adm_client,
        const char * fmt,
        ...)
{
    char err_msg[SIRI_MAX_SIZE_ERR_MSG];

    va_list args;
    va_start(args, fmt);
    vsnprintf(err_msg, SIRI_MAX_SIZE_ERR_MSG, fmt, args);
    va_end(args);

    sirinet_pkg_t * package = sirinet_pkg_err(
            adm_client->pid,
            strlen(err_msg),
            CPROTO_ERR_ADMIN,
            err_msg);

    if (package != NULL)
    {
        sirinet_pkg_send(adm_client->client, package);
    }

    log_error(err_msg);

    if (~adm_client->flags & CLIENT_FLAGS_NO_ROLLBACK)
    {
        siri_admin_request_rollback(adm_client->dbpath);
    }

    sirinet_socket_decref(siri.socket);

    uv_close((uv_handle_t *) &siri.timer, NULL);
}

/*
 * Send a package to the 'other' SiriDB server. This function can be used for
 * sending api requests for all database information required to create the
 * database. Note that all packages are send with the same pid so packages
 * should be send in sequence, not parallel.
 *
 * Note: pkg will be freed by calling this function.
 */
static void CLIENT_send_pkg(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));
    if (req == NULL)
    {
        free(pkg);
        CLIENT_err(adm_client, "memory allocation error");
    }
    req->data = adm_client;

    adm_client->pkg = pkg;

    /* set the correct check bit */
    pkg->checkbit = pkg->tp ^ 255;

    uv_timer_start(
            &siri.timer,
            CLIENT_request_timeout,
            CLIENT_REQUEST_TIMEOUT,
            0);

    siri.timer.data = adm_client;

    uv_buf_t wrbuf = uv_buf_init(
            (char *) pkg,
            sizeof(sirinet_pkg_t) + pkg->len);

    uv_write(
            req,
            (uv_stream_t *) siri.socket,
            &wrbuf,
            1,
            CLIENT_write_cb);
}

/*
 * Write call-back.
 */
static void CLIENT_write_cb(uv_write_t * req, int status)
{
    siri_admin_client_t * adm_client = (siri_admin_client_t *)  req->data;

    if (status)
    {
        uv_timer_stop(&siri.timer);
        CLIENT_err(adm_client, "socket write error: %s", uv_strerror(status));
    }

    free(adm_client->pkg);
    free(req);
}

/*
 * Called when a connection to the 'other' siridb server is made.
 * An authentication request will be send by user/password since this will be
 * using the client connection. (a signal can be raised)
 */
static void CLIENT_on_connect(uv_connect_t * req, int status)
{
    sirinet_socket_t * ssocket = req->handle->data;
    siri_admin_client_t * adm_client = (siri_admin_client_t *) ssocket->origin;

    if (status == 0)
    {
        log_debug(
                "Connected to SiriDB server: '%s:%u', "
                "sending authentication request",
                adm_client->host, adm_client->port);

        uv_read_start(
                req->handle,
                sirinet_socket_alloc_buffer,
                sirinet_socket_on_data);

        sirinet_pkg_t * pkg;
        qp_packer_t * packer = sirinet_packer_new(512);

        if (packer == NULL)
        {
            CLIENT_err(adm_client, "memory allocation error");
        }
        else
        {
            if (qp_add_type(packer, QP_ARRAY3) ||
                qp_add_string(packer, adm_client->username) ||
                qp_add_string(packer, adm_client->password) ||
                qp_add_string(packer, adm_client->dbname))
            {
                qp_packer_free(packer);
            }
            else
            {
                pkg = sirinet_packer2pkg(packer, 0, CPROTO_REQ_AUTH);
                CLIENT_send_pkg(adm_client, pkg);
            }

        }
    }
    else
    {
        CLIENT_err(
                adm_client,
                "connecting to server '%s:%u' failed with error: %s",
                adm_client->host,
                adm_client->port,
                uv_strerror(status));
    }
    free(req);
}

/*
 * on-data call-back function.
 */
static void CLIENT_on_data(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    sirinet_socket_t * ssocket = client->data;
    siri_admin_client_t * adm_client = (siri_admin_client_t *) ssocket->origin;
    log_debug(
            "Client response received (pid: %" PRIu16
            ", len: %" PRIu32 ", tp: %s)",
            pkg->pid,
            pkg->len,
            sirinet_cproto_server_str(pkg->tp));

    if (adm_client->flags & CLIENT_FLAGS_TIMEOUT)
    {
        log_error("Client response received which was timed-out earlier");
    }
    else
    {
        uv_timer_stop(&siri.timer);

        switch ((cproto_server_t) pkg->tp)
        {
        case CPROTO_RES_AUTH_SUCCESS:
            CLIENT_on_auth_success(adm_client);
            break;
        case CPROTO_RES_QUERY:
            switch (adm_client->request)
            {
            case CLIENT_REQUEST_STATUS:
                CLIENT_on_request_status(adm_client, pkg);
                break;
            case CLIENT_REQUEST_POOLS:
                CLIENT_on_request_pools(adm_client, pkg);
                break;
            default:
                CLIENT_err(adm_client, "unexpected query response");
            }
            break;
        case CPROTO_RES_FILE:
            switch (adm_client->request)
            {
            case CLIENT_REQUEST_FILE_USERS:
                CLIENT_on_file_users(adm_client, pkg);
                break;
            case CLIENT_REQUEST_FILE_GROUPS:
                CLIENT_on_file_groups(adm_client, pkg);
                break;
            case CLIENT_REQUEST_FILE_SERVERS:
                CLIENT_on_file_servers(adm_client, pkg);
                break;
            case CLIENT_REQUEST_FILE_DATABASE:
                CLIENT_on_file_database(adm_client, pkg);
                break;
            default:
                CLIENT_err(adm_client, "unexpected query response");
            }
            break;
        case CPROTO_RES_ACK:
            switch (adm_client->request)
            {
            case CLIENT_REQUEST_REGISTER_SERVER:
                CLIENT_on_register_server(adm_client);
                break;
            default:
                CLIENT_err(adm_client, "unexpected query response");
            }
            break;
        case CPROTO_ERR_AUTH_CREDENTIALS:
            CLIENT_err(
                    adm_client,
                    "invalid credentials for database '%s' on server '%s:%u'",
                    adm_client->dbname,
                    adm_client->host,
                    adm_client->port);
            break;
        case CPROTO_ERR_AUTH_UNKNOWN_DB:
            CLIENT_err(
                    adm_client,
                    "database '%s' does not exist on server '%s:%u'",
                    adm_client->dbname,
                    adm_client->host,
                    adm_client->port);
            break;
        case CPROTO_ERR_MSG:
        case CPROTO_ERR_QUERY:
        case CPROTO_ERR_INSERT:
        case CPROTO_ERR_SERVER:
        case CPROTO_ERR_POOL:
        case CPROTO_ERR_USER_ACCESS:
            CLIENT_on_error_msg(adm_client, pkg);
            break;
        default:
            CLIENT_err(
                    adm_client,
                    "unexpected response (%u) received from server '%s:%u'",
                    pkg->tp,
                    adm_client->host,
                    adm_client->port);
        }
    }
}

/*
 * Called when register server was successful.
 */
static void CLIENT_on_register_server(siri_admin_client_t * adm_client)
{
    sirinet_pkg_t * package = sirinet_pkg_new(
            adm_client->pid,
            0,
            CPROTO_ACK_ADMIN,
            NULL);

    if (package != NULL)
    {
        sirinet_pkg_send(adm_client->client, package);
    }

    log_info(
            "Finished registering server on database '%s'",
            adm_client->dbname);

    sirinet_socket_decref(siri.socket);
    uv_close((uv_handle_t *) &siri.timer, NULL);
}

/*
 * Called when database.dat is received.
 */
static void CLIENT_on_file_database(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    FILE * fp;
    qp_unpacker_t unpacker;
    qp_obj_t qp_uuid;
    siridb_t * siridb;
    int rc;
    char fn[strlen(adm_client->dbpath) + 13]; // 13 = strlen("database.dat")+1
    sprintf(fn, "%sdatabase.dat", adm_client->dbpath);

    qp_unpacker_init(&unpacker, pkg->data, pkg->len);

    if (qp_is_array(qp_next(&unpacker, NULL)) &&
        /* schema check is not required at this moment but can be done here */
        qp_next(&unpacker, NULL) == QP_INT64 &&
        qp_next(&unpacker, &qp_uuid) == QP_RAW &&
        qp_uuid.len == 16)
    {
        memcpy(unpacker.pt - 16, &adm_client->uuid, 16);
    }
    else
    {
        CLIENT_err(adm_client, "invalid database file received");
        return;
    }

    fp = fopen(fn, "w");

    if (fp == NULL)
    {
        CLIENT_err(adm_client, "cannot write or create file: %s", fn);
        return;
    }

    rc = fwrite(pkg->data, pkg->len, 1, fp);

    if (fclose(fp) || rc != 1)
    {
        CLIENT_err(adm_client, "cannot write or create file: %s", fn);
        return;
    }

    siridb = siridb_new(adm_client->dbpath, LOCK_QUIT_IF_EXIST);

    if (siridb == NULL)
    {
        CLIENT_err(adm_client, "error loading database");
        return;
    }

    /* roll-back is not possible anymore */
    adm_client->flags |= CLIENT_FLAGS_NO_ROLLBACK;

    siridb->server->flags |= SERVER_FLAG_RUNNING;

    /* Force one heart-beat */
    siri_heartbeat_force();

    sirinet_pkg_t * package;
    qp_packer_t * packer = sirinet_packer_new(512);

    if (packer == NULL)
    {
        CLIENT_err(adm_client, "memory allocation error");
        return;
    }

    adm_client->request = CLIENT_REQUEST_REGISTER_SERVER;

    if (qp_add_type(packer, QP_ARRAY4) ||
        qp_add_raw(packer, (const unsigned char *) &adm_client->uuid, 16) ||
        qp_add_string(packer, siri.cfg->server_address) ||
        qp_add_int32(packer, (int32_t) siri.cfg->listen_backend_port) ||
        qp_add_int32(packer, (int32_t) adm_client->pool))
    {
        qp_packer_free(packer);
        CLIENT_err(adm_client, "memory allocation error");
        return;
    }

    package = sirinet_packer2pkg(packer, 0, CPROTO_REQ_REGISTER_SERVER);
    CLIENT_send_pkg(adm_client, package);
}

/*
 * Called when servers.dat is received.
 */
static void CLIENT_on_file_servers(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    FILE * fp;
    qp_unpacker_t unpacker;
    qp_types_t tp;
    int rc, n, close_num;
    char fn[strlen(adm_client->dbpath) + 12]; // 12 = strlen("servers.dat") + 1
    sprintf(fn, "%sservers.dat", adm_client->dbpath);

    fp = fopen(fn, "w");
    if (fp == NULL)
    {
        CLIENT_err(adm_client, "cannot write or create file: %s", fn);
        return;
    }

    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    tp = qp_next(&unpacker, NULL);
    if (tp >= QP_ARRAY0 && tp <= QP_ARRAY5)
    {
        pkg->data[0] = QP_ARRAY_OPEN;
    }
    else if (tp != QP_ARRAY_OPEN)
    {
        CLIENT_err(adm_client, "invalid server status response");
        return;
    }

    /* schema checking is not required at this moment but can be done here */
    qp_next(&unpacker, NULL);

    tp = qp_next(&unpacker, NULL);

    if (!qp_is_array(tp))
    {
        CLIENT_err(adm_client, "invalid server status response");
        return;
    }

    close_num = (tp == QP_ARRAY_OPEN) ? 1 : 0;

    /* trim closing */
    for (n = pkg->len;
         (unsigned char) pkg->data[n - 1] == QP_ARRAY_CLOSE;
         n--);

    rc = (fwrite(pkg->data, n, 1, fp) == 1) ? 0 : EOF;

    if (close_num)
    {
        rc += qp_fadd_type(fp, QP_ARRAY_CLOSE);
    }

    rc += qp_fadd_type(fp, QP_ARRAY4);
    rc += qp_fadd_raw(fp, (const unsigned char *) &adm_client->uuid, 16);
    rc += qp_fadd_string(fp, siri.cfg->server_address);
    rc += qp_fadd_int32(fp, (int32_t) siri.cfg->listen_backend_port);
    rc += qp_fadd_int32(fp, (int32_t) adm_client->pool);
    rc += fclose(fp);

    if (rc)
    {
        CLIENT_err(adm_client, "cannot write or create file: %s", fn);
    }
    else
    {
        sirinet_pkg_t * package;
        adm_client->request = CLIENT_REQUEST_FILE_DATABASE;
        package = sirinet_pkg_new(0, 0, CPROTO_REQ_FILE_DATABASE, NULL);
        if (package == NULL)
        {
            CLIENT_err(adm_client, "memory allocation error");
        }
        else
        {
            CLIENT_send_pkg(adm_client, package);
        }
    }
}

/*
 * Called when groups.dat is received.
 */
static void CLIENT_on_file_groups(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    FILE * fp;
    char fn[strlen(adm_client->dbpath) + 11]; // 11 = strlen("groups.dat") + 1
    sprintf(fn, "%sgroups.dat", adm_client->dbpath);

    fp = fopen(fn, "w");
    if (fp == NULL)
    {
        CLIENT_err(adm_client, "cannot write or create file: %s", fn);
    }
    else
    {
        int rc = fwrite(pkg->data, pkg->len, 1, fp);

        if (fclose(fp) || rc != 1)
        {
            CLIENT_err(adm_client, "cannot write data to file: %s", fn);
        }
        else
        {
            adm_client->request = CLIENT_REQUEST_FILE_SERVERS;
            sirinet_pkg_t * package =
                    sirinet_pkg_new(0, 0, CPROTO_REQ_FILE_SERVERS, NULL);
            if (package == NULL)
            {
                CLIENT_err(adm_client, "memory allocation error");
            }
            else
            {
                CLIENT_send_pkg(adm_client, package);
            }
        }
    }
}

/*
 * Called when users.dat is received.
 */
static void CLIENT_on_file_users(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    FILE * fp;
    char fn[strlen(adm_client->dbpath) + 10]; // 10 = strlen("users.dat") + 1
    sprintf(fn, "%susers.dat", adm_client->dbpath);

    fp = fopen(fn, "w");
    if (fp == NULL)
    {
        CLIENT_err(adm_client, "cannot write or create file: %s", fn);
    }
    else
    {
        int rc = fwrite(pkg->data, pkg->len, 1, fp);

        if (fclose(fp) || rc != 1)
        {
            CLIENT_err(adm_client, "cannot write data to file: %s", fn);
        }
        else
        {
            adm_client->request = CLIENT_REQUEST_FILE_GROUPS;
            sirinet_pkg_t * package =
                    sirinet_pkg_new(0, 0, CPROTO_REQ_FILE_GROUPS, NULL);
            if (package == NULL)
            {
                CLIENT_err(adm_client, "memory allocation error");
            }
            else
            {
                CLIENT_send_pkg(adm_client, package);
            }
        }
    }
}

/*
 * Called when 'list pools ...' response is received.
 * This function will check which pool number will be assigned for a new pool
 * or checks if a given pool for a new replica is valid.
 */
static void CLIENT_on_request_pools(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_val;
    qp_obj_t qp_pool;
    qp_obj_t qp_servers;
    int columns_found = 0;
    int validate_pool = -1;

    if (!qp_is_map(qp_next(&unpacker, NULL)))
    {
        CLIENT_err(adm_client, "invalid server status response");
        return;
    }

    qp_next(&unpacker, &qp_val);

    while (qp_val.tp == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_val.via.raw,
                    "columns",
                    qp_val.len) == 0 &&
                qp_is_array(qp_next(&unpacker, NULL)) &&
                qp_next(&unpacker, &qp_val) == QP_RAW &&
                strncmp(
                    (const char *) qp_val.via.raw, "pool", qp_val.len) == 0 &&
                qp_next(&unpacker, &qp_val) == QP_RAW &&
                strncmp(
                    (const char *) qp_val.via.raw, "servers", qp_val.len) == 0)
        {
            if (qp_next(&unpacker, &qp_val) == QP_ARRAY_CLOSE)
            {
                qp_next(&unpacker, &qp_val);
            }
            columns_found = 1;
            continue;
        }
        if (    strncmp(
                    (const char *) qp_val.via.raw, "pools", qp_val.len) == 0 &&
                qp_is_array(qp_next(&unpacker, NULL)))
        {
            qp_next(&unpacker, &qp_val);

            while (qp_is_array(qp_val.tp))
            {
                if (qp_next(&unpacker, &qp_pool) == QP_INT64 &&
                    qp_next(&unpacker, &qp_servers) == QP_INT64)
                {
                    if (adm_client->pool < 0)
                    {
                        /* looking for a new pool */
                        if (qp_pool.via.int64 > validate_pool)
                        {
                            validate_pool = qp_pool.via.int64;
                        }
                    }
                    else
                    {
                        if (qp_pool.via.int64 == adm_client->pool)
                        {
                            if (qp_servers.via.int64 > 1)
                            {
                                CLIENT_err(
                                        adm_client,
                                        "pool %d has already %" PRId64
                                        " servers",
                                        adm_client->pool,
                                        qp_servers.via.int64);
                                return;
                            }
                            else
                            {
                                validate_pool = 0;
                            }
                        }
                    }
                }
                else
                {
                    CLIENT_err(adm_client, "invalid server status response");
                    return;
                }
                if (qp_next(&unpacker, &qp_val) == QP_ARRAY_CLOSE)
                {
                    qp_next(&unpacker, &qp_val);
                }
            }
            if (qp_val.tp == QP_ARRAY_CLOSE)
            {
                qp_next(&unpacker, &qp_val);
            }
            continue;
        }

        CLIENT_err(adm_client, "invalid server status response");
        return;
    }

    if (!columns_found)
    {
        CLIENT_err(adm_client, "invalid server status response");
    }
    else
    {
        sirinet_pkg_t * package;

        if (adm_client->pool < 0)
        {
            if (validate_pool == -1)
            {
                CLIENT_err(adm_client, "invalid server status response");
                return;
            }
            /* set new correct pool in case we request a new pool */
            adm_client->pool = validate_pool + 1;
        }
        else if (validate_pool == -1)
        {
            CLIENT_err(
                    adm_client,
                    "pool %d does not exist",
                    adm_client->pool);
            return;
        }

        adm_client->request = CLIENT_REQUEST_FILE_USERS;
        package = sirinet_pkg_new(0, 0, CPROTO_REQ_FILE_USERS, NULL);
        if (package == NULL)
        {
            CLIENT_err(adm_client, "memory allocation error");
        }
        else
        {
            CLIENT_send_pkg(adm_client, package);
        }
    }
}

/*
 * Called when 'list servers ...' response is received.
 * This function will check if each current server has status running.
 * (this is a pre-check, the final register call does check for all servers
 * to have the running status once more)
 */
static void CLIENT_on_request_status(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_val;
    qp_obj_t qp_name;
    qp_obj_t qp_status;
    qp_obj_t qp_version;
    int columns_found = 0;
    int servers_found = 0;
    char version[MAX_VERSION_LEN];

    if (!qp_is_map(qp_next(&unpacker, NULL)))
    {
        CLIENT_err(adm_client, "invalid server status response");
        return;
    }

    qp_next(&unpacker, &qp_val);

    while (qp_val.tp == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_val.via.raw,
                    "columns",
                    qp_val.len) == 0 &&
                qp_is_array(qp_next(&unpacker, NULL)) &&
                qp_next(&unpacker, &qp_val) == QP_RAW &&
                strncmp(
                    (const char *) qp_val.via.raw, "name", qp_val.len) == 0 &&
                qp_next(&unpacker, &qp_val) == QP_RAW &&
                strncmp(
                    (const char *) qp_val.via.raw,
                    "status",
                    qp_val.len) == 0 &&
                qp_next(&unpacker, &qp_val) == QP_RAW &&
                strncmp(
                    (const char *) qp_val.via.raw, "version", qp_val.len) == 0)
        {
            if (qp_next(&unpacker, &qp_val) == QP_ARRAY_CLOSE)
            {
                qp_next(&unpacker, &qp_val);
            }
            columns_found = 1;
            continue;
        }
        if (    strncmp(
                    (const char *) qp_val.via.raw,
                    "servers",
                    qp_val.len) == 0 &&
                qp_is_array(qp_next(&unpacker, NULL)))
        {
            qp_next(&unpacker, &qp_val);

            while (qp_is_array(qp_val.tp))
            {
                if (qp_next(&unpacker, &qp_name) == QP_RAW &&
                    qp_next(&unpacker, &qp_status) == QP_RAW &&
                    qp_next(&unpacker, &qp_version) == QP_RAW &&
                    qp_version.len < MAX_VERSION_LEN)
                {
                    /* copy and null terminate version */
                    memcpy(version, qp_version.via.raw, qp_version.len);
                    version[qp_version.len] = '\0';

                    if (siri_version_cmp(version, SIRIDB_VERSION))
                    {
                        CLIENT_err(
                                adm_client,
                                "server '%.*s' is running on version %s "
                                "while version %s is expected. (all SiriBD "
                                "servers should run the same version)",
                                (int) qp_name.len,
                                qp_name.via.raw,
                                version,
                                SIRIDB_VERSION);
                        return;
                    }

                    if (strncmp(
                            (const char *) qp_status.via.raw,
                            "running",
                            qp_status.len) != 0)
                    {
                        CLIENT_err(
                                adm_client,
                                "server '%.*s' has status: '%.*s'",
                                (int) qp_name.len,
                                qp_name.via.raw,
                                (int) qp_status.len,
                                qp_status.via.raw);
                        return;
                    }
                    servers_found++;
                }
                else
                {
                    CLIENT_err(adm_client, "invalid server status response");
                    return;
                }
                if (qp_next(&unpacker, &qp_val) == QP_ARRAY_CLOSE)
                {
                    qp_next(&unpacker, &qp_val);
                }
            }
            if (qp_val.tp == QP_ARRAY_CLOSE)
            {
                qp_next(&unpacker, &qp_val);
            }
            continue;
        }

        CLIENT_err(adm_client, "invalid server status response");
        return;
    }

    if (!servers_found || !columns_found)
    {
        CLIENT_err(adm_client, "invalid server status response");
    }
    else
    {
        sirinet_pkg_t * package;
        qp_packer_t * packer = sirinet_packer_new(512);
        if (packer == NULL)
        {
            CLIENT_err(adm_client, "memory allocation error");
        }
        else
        {
            adm_client->request = CLIENT_REQUEST_POOLS;

            /* no need to check since this will always fit */
            qp_add_type(packer, QP_ARRAY1);
            qp_add_string(packer, "list pools pool, servers");
            package = sirinet_packer2pkg(packer, 0, CPROTO_REQ_QUERY);
            CLIENT_send_pkg(adm_client, package);
        }
    }
}

/*
 * Called when an error message is received.
 */
static void CLIENT_on_error_msg(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);
    qp_obj_t qp_err;

    if (qp_is_map(qp_next(&unpacker, NULL)) &&
        qp_next(&unpacker, NULL) == QP_RAW &&
        qp_next(&unpacker, &qp_err) == QP_RAW)
    {
        CLIENT_err(
                adm_client,
                "error on server '%s:%u': %.*s",
                adm_client->host,
                adm_client->port,
                (int) qp_err.len,
                qp_err.via.raw);
    }
    else
    {
        CLIENT_err(
                adm_client,
                "unexpected error on server '%s:%u'",
                adm_client->host,
                adm_client->port);
    }
}

/*
 * Called when authentication is successful.
 */
static void CLIENT_on_auth_success(siri_admin_client_t * adm_client)
{
    sirinet_pkg_t * pkg;
    qp_packer_t * packer = sirinet_packer_new(512);
    if (packer == NULL)
    {
        CLIENT_err(adm_client, "memory allocation error");
    }
    else
    {
        adm_client->request = CLIENT_REQUEST_STATUS;

        /* no need to check since this will always fit */
        qp_add_type(packer, QP_ARRAY1);
        qp_add_string(packer, "list servers name, status, version");
        pkg = sirinet_packer2pkg(packer, 0, CPROTO_REQ_QUERY);
        CLIENT_send_pkg(adm_client, pkg);
    }
}

/*
 * Timeout on a client request.
 */
static void CLIENT_request_timeout(uv_timer_t * handle)
{
    siri_admin_client_t * adm_client = (siri_admin_client_t *) handle->data;

    adm_client->flags |= CLIENT_FLAGS_TIMEOUT;

    CLIENT_err(adm_client, "request timeout");
}
