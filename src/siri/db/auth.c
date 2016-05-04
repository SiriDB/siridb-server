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
#include <siri/net/handle.h>
#include <stdlib.h>
#include <string.h>

sirinet_msg_t siridb_auth_request(
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
        return SN_MSG_UNKNOWN_DATABASE;

    if ((user = siridb_users_get_user(siridb, username, password)) == NULL)
        return SN_MSG_INVALID_CREDENTIALS;

    ((sirinet_handle_t *) client->data)->siridb = siridb;
    ((sirinet_handle_t *) client->data)->user = user;

    return SN_MSG_AUTH_SUCCESS;
}

