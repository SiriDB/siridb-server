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

#include <siri/siri.h>
#include <logger/logger.h>

int siri_admin_client_init(
        uint16_t pid,
        uint16_t port,
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

    adm_client = (siri_admin_client_t *) malloc(sizeof(siri_admin_client_t));
    if (adm_client == NULL)
    {
        sirinet_socket_decref(siri.socket);
        sprintf(err_msg, "memory allocation error");
        return -1;
    }
    adm_client->pid = pid;
    adm_client->port = port;
    adm_client->host = strndup(host->via->raw, host->len);
    adm_client->username = strndup(username->via->raw, username->len);
    adm_client->password = strndup(password->via->raw, password->len);
    adm_client->dbname = strndup(dbname->via->raw, dbname->len);
    adm_client->dbpath = strdup(dbpath);
    adm_client->client = client;

    ssocket = (sirinet_socket_t *) siri.socket->data;
    ssocket->origin = (void *) adm_client;

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

    uv_tcp_init(siri.loop, siri.socket);

    if (inet_pton(AF_INET, host, &sa))
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
        return 0;
    }
}

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

static void CLIENT_err(siri_admin_client_t * adm_client, const char * err_msg)
{
    sirinet_pkg_t * package = sirinet_pkg_err(
            adm_client->pid,
            strlen(err_msg),
            CPROTO_ERR_ADMIN,
            err_msg);

    if (package != NULL)
    {
        sirinet_pkg_send(adm_client->client, package);
    }

    log_warning(err_msg);

    siri_admin_rollback(adm_client->dbpath);

    sirinet_socket_decref(siri.socket);
}
/*
 * This function can raise a SIGNAL.
 */
static void CLIENT_on_connect(uv_connect_t * req, int status)
{
    sirinet_socket_t * ssocket = req->handle->data;
    siri_admin_client_t * adm_client = (siri_admin_client_t *) ssocket->origin;

    if (status == 0)
    {
        log_debug(
                "Manage connection created to server: '%s:%u', "
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
                pkg = sirinet_packer2pkg(packer, 0, BPROTO_AUTH_REQUEST);
                if (siridb_server_send_pkg(
                        server,
                        pkg,
                        0,
                        SERVER_on_auth_response,
                        NULL,
                        0))
                {
                    free(pkg);
                }
            }

        }
    }
    else
    {
        log_error("Connecting to back-end server '%s' failed (error: %s)",
                server->name,
                uv_strerror(status));

        sirinet_socket_decref(req->handle);
    }
    free(req);
}

/*
 * on-data call-back function.
 *In case the promise is found, promise->cb() will be called.
 */
static void CLIENT_on_data(uv_stream_t * client, sirinet_pkg_t * pkg)
{
    sirinet_socket_t * ssocket = client->data;
    sirinet_promise_t * promise = imap_pop(siri.promises, pkg->pid);
    ssocket->

    log_debug(
            "Client response received (pid: %" PRIu16
            ", len: %" PRIu32 ", tp: %s)",
            pkg->pid,
            pkg->len,
            sirinet_cproto_server_str(pkg->tp));

    if (promise == NULL)
    {
        log_warning(
                "Received a package (PID %" PRIu16
                ") which has probably timed-out earlier.",
                pkg->pid);
    }
    else
    {
        uv_timer_stop(promise->timer);
        uv_close((uv_handle_t *) promise->timer, (uv_close_cb) free);
        promise->cb(promise, pkg, PROMISE_SUCCESS);
    }
}
