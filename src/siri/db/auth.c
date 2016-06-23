/*
 * auth.c - Handle SiriDB authentication.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#include <siri/db/auth.h>
#include <logger/logger.h>
#include <siri/db/db.h>
#include <siri/siri.h>
#include <siri/net/protocol.h>
#include <siri/db/users.h>
#include <siri/db/servers.h>
#include <siri/net/socket.h>
#include <stdlib.h>
#include <string.h>

sirinet_msg_t siridb_auth_user_request(
        uv_handle_t * client,
        qp_obj_t * qp_username,
        qp_obj_t * qp_password,
        qp_obj_t * qp_dbname)
{
    siridb_t * siridb;
    siridb_user_t * user;

    char dbname[qp_dbname->len + 1];
    memcpy(dbname, qp_dbname->via->raw, qp_dbname->len);
    dbname[qp_dbname->len] = 0;

    char username[qp_username->len + 1];
    memcpy(username, qp_username->via->raw, qp_username->len);
    username[qp_username->len] = 0;

    char password[qp_password->len + 1];
    memcpy(password, qp_password->via->raw, qp_password->len);
    password[qp_password->len] = 0;

    if ((siridb = siridb_get(siri.siridb_list, dbname)) == NULL)
    {
        return SN_MSG_UNKNOWN_DATABASE;
    }

    if ((user = siridb_users_get_user(siridb->users, username, password)) == NULL)
    {
        return SN_MSG_INVALID_CREDENTIALS;
    }

    ((sirinet_socket_t *) client->data)->siridb = siridb;
    ((sirinet_socket_t *) client->data)->origin = user;
    siridb_user_incref(user);

    return SN_MSG_AUTH_SUCCESS;
}

bp_server_t siridb_auth_server_request(
        uv_handle_t * client,
        qp_obj_t * qp_uuid,
        qp_obj_t * qp_dbname)
{
    siridb_t * siridb;
    siridb_server_t * server;
    uuid_t uuid;

    char dbname[qp_dbname->len + 1];
    memcpy(dbname, qp_dbname->via->raw, qp_dbname->len);
    dbname[qp_dbname->len] = 0;

    if (qp_uuid->len != 16)
    {
        return BP_AUTH_ERROR_INVALID_UUID;
    }

    memcpy(uuid, qp_uuid->via->raw, 16);

    if ((siridb = siridb_get(siri.siridb_list, dbname)) == NULL)
    {
        return BP_AUTH_ERROR_UNKNOWN_DBNAME;
    }

    if ((server = siridb_servers_get_server(siridb, uuid)) == NULL)
    {
        return BP_AUTH_ERROR_UNKNOWN_UUID;
    }

    ((sirinet_socket_t *) client->data)->siridb = siridb;
    ((sirinet_socket_t *) client->data)->origin = server;

    /* we must increment the server reference counter */
    siridb_server_incref(server);

    return BP_AUTH_SUCCESS;
}

