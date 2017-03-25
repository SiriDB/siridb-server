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

#define CLIENT_REQUEST_TIMEOUT 15000  // 15 seconds
#define CLIENT_FLAGS_TIMEOUT 1

enum
{
    CLIENT_STATUS_INIT,
    CLIENT_STATUS_CONNECTING
};

static void CLIENT_write_cb(uv_write_t * req, int status);
static void CLIENT_on_connect(uv_connect_t * req, int status);
static void CLIENT_on_data(uv_stream_t * client, sirinet_pkg_t * pkg);
static void CLIENT_request_timeout(uv_timer_t * handle);

int siri_admin_client_request(
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
//    struct in6_addr sa6;

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
    adm_client->host = strndup(host->via.raw, host->len);
    adm_client->username = strndup(username->via.raw, username->len);
    adm_client->password = strndup(password->via.raw, password->len);
    adm_client->dbname = strndup(dbname->via.raw, dbname->len);
    adm_client->dbpath = strdup(dbpath);
    adm_client->client = client;
    adm_client->status = CLIENT_STATUS_INIT;
    adm_client->flags = 0;

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

        return 0;
    }
    sprintf(err_msg, "invalid ipv4");
    return -1;
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

    log_warning("%s (status code: %d)", err_msg, adm_client->status);

    siri_admin_request_rollback(adm_client->dbpath);

    sirinet_socket_decref(siri.socket);

    uv_close((uv_handle_t *) &siri.timer, NULL);
}

static int CLIENT_send_pkg(
        siri_admin_client_t * adm_client,
        sirinet_pkg_t * pkg)
{
    adm_client->status++;

    uv_write_t * req = (uv_write_t *) malloc(sizeof(uv_write_t));
    if (req == NULL)
    {
        log_critical("memory allocation error");
        return -1;
    }
    req->data = adm_client;

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

    return 0;
}

/*
 * Write call-back.
 */
static void CLIENT_write_cb(uv_write_t * req, int status)
{
    siri_admin_client_t * adm_client = (siri_admin_client_t *)  req->data;

    if (status)
    {
        log_error("Socket write error: %s", uv_strerror(status));

        uv_timer_stop(&siri.timer);

        CLIENT_err(adm_client, uv_strerror(status));
    }

    free(req);
}

/*
 * This function can raise a SIGNAL.
 */
static void CLIENT_on_connect(uv_connect_t * req, int status)
{
    sirinet_socket_t * ssocket = req->handle->data;
    siri_admin_client_t * adm_client = (siri_admin_client_t *) ssocket->origin;

    adm_client->status = CLIENT_STATUS_CONNECTING;
    uv_timer_init(siri.loop, &siri.timer);

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
                pkg = sirinet_packer2pkg(packer, 0, BPROTO_AUTH_REQUEST);
                if (CLIENT_send_pkg(adm_client, pkg))
                {
                    free(pkg);
                }
            }

        }
    }
    else
    {
        log_error("Connecting to SiriDB server '%s:%u' failed (error: %s)",
                adm_client->host,
                adm_client->port,
                uv_strerror(status));

        CLIENT_err(adm_client, uv_strerror(status));
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
            LOGC("Authentication success!");
            /* no break */
        default:
            CLIENT_err(adm_client, "unexpected response received");
        }
    }
}

/*
 * Timeout received.
 */
static void CLIENT_request_timeout(uv_timer_t * handle)
{
    siri_admin_client_t * adm_client = (siri_admin_client_t *) handle->data;

    adm_client->flags |= CLIENT_FLAGS_TIMEOUT;

    CLIENT_err(adm_client, "request timeout");
}
