/*
 * request.c - SiriDB Administrative Request.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2017
 *
 */
#include <siri/admin/account.h>
#include <stddef.h>
#include <siri/admin/request.h>
#include <siri/siri.h>
#include <logger/logger.h>

#define DEFAULT_CONF \
"#\n" \
"# Welcome to the SiriDB configuration file\n" \
"#\n" \
"\n" \
"[buffer]\n" \
"# Alternative path to save the buffer file.\n" \
"# In case you later plan to change this location you manually need to move\n" \
"# the buffer file to the new location.\n" \
"# path = <buffer_path>\n"

static cproto_server_t ADMIN_on_new_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t ADMIN_on_change_password(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t ADMIN_on_drop_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg,
        qp_obj_t * qp_account);
static cproto_server_t ADMIN_on_new_database(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);

int siri_admin_request_init(void)
{
    const char * pcre_error_str;
    int pcre_error_offset;

    pcre * regex;
    pcre_extra * regex_extra;

    pcre = pcre_compile(
                "^[a-zA-Z][a-zA-Z0-9-_]{,18}[a-zA-Z0-9]$",
                0,
                &pcre_error_str,
                &pcre_error_offset,
                NULL);
    if (pcre == NULL)
    {
        return -1;
    }
    regex_extra = pcre_study(regex, 0, &pcre_error_str);

    /* pcre_study() returns NULL for both errors and when it can not
     * optimize the regex.  The last argument is how one checks for
     * errors (it is NULL if everything works, and points to an error
     * string otherwise. */
    if(pcre_error_str != NULL)
    {
        free(regex_extra);
        free(pcre);
        return -1;
    }
    return 0;
}

void siri_admin_request_destroy(void)
{
    free(siri.dbname_regex);
    free(siri.dbname_regex_extra);
}

cproto_server_t siri_admin_request(
        int tp,
        qp_unpacker_t * qp_unpacker,
        qp_obj_t * qp_account,
        char * err_msg)
{
    switch ((admin_request_t) tp)
    {
    case ADMIN_NEW_ACCOUNT:
        return ADMIN_on_new_account(qp_unpacker, err_msg);
    case ADMIN_CHANGE_PASSWORD:
        return ADMIN_on_change_password(qp_unpacker, err_msg);
    case ADMIN_DROP_ACCOUNT:
        return ADMIN_on_drop_account(qp_unpacker, err_msg, qp_account);
    case ADMIN_NEW_DATABASE:
        return ADMIN_on_new_database(qp_unpacker, err_msg);
    default:
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }
}

static cproto_server_t ADMIN_on_new_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    qp_obj_t qp_key, qp_account, qp_password;

    qp_account.tp = QP_HOOK;
    qp_password.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(qp_key.via.raw, "account", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_account) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(qp_key.via.raw, "password", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_password) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    if (qp_account.tp == QP_HOOK || qp_password.tp == QP_HOOK)
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    return (siri_admin_account_new(
            &siri,
            &qp_account,
            &qp_password,
            0,
            err_msg) ||
            siri_admin_account_save(&siri, err_msg)) ?
                    CPROTO_ERR_ADMIN : CPROTO_SUCCESS_ADMIN;
}

static cproto_server_t ADMIN_on_change_password(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    qp_obj_t qp_key, qp_account, qp_password;

    qp_account.tp = QP_HOOK;
    qp_password.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(qp_key.via.raw, "account", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_account) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(qp_key.via.raw, "password", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_password) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    if (qp_account.tp == QP_HOOK || qp_password.tp == QP_HOOK)
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    return (siri_admin_account_change_password(
            &siri,
            &qp_account,
            &qp_password,
            err_msg) ||
            siri_admin_account_save(&siri, err_msg)) ?
                    CPROTO_ERR_ADMIN : CPROTO_SUCCESS_ADMIN;
}

static cproto_server_t ADMIN_on_drop_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg,
        qp_obj_t * qp_account)
{
    qp_obj_t qp_key, qp_target;

    qp_target.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(qp_key.via.raw, "account", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_target) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    if (qp_target.tp == QP_HOOK)
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    if (qp_target.len == qp_account->len &&
        strncmp(qp_target.via.raw, qp_account->via.raw, qp_target.len) == 0)
    {
        sprintf(err_msg, "cannot drop your own account");
        return CPROTO_ERR_ADMIN;
    }

    return (siri_admin_account_drop(
            &siri,
            &qp_target,
            err_msg) ||
            siri_admin_account_save(&siri, err_msg)) ?
                    CPROTO_ERR_ADMIN : CPROTO_SUCCESS_ADMIN;
}

static cproto_server_t ADMIN_on_new_database(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    qp_obj_t qp_key, qp_dbname;
    size_t dbpath_len;
    int pcre_exec_ret;
    int sub_str_vec[2];

    qp_dbname.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(qp_key.via.raw, "dbname", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_dbname) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }

    if (qp_dbname.tp == QP_HOOK)
    {
        return CPROTO_ERR_ADMIN_INVALID_REQUEST;
    }


    pcre_exec_ret = pcre_exec(
            siri.dbname_regex,
            siri.dbname_regex_extra,
            qp_dbname.via.raw,
            qp_dbname.len,
            0,                     // start looking at this point
            0,                     // OPTIONS
            sub_str_vec,
            2);                    // length of sub_str_vec

    if (pcre_exec_ret < 0)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "invalid database name: '%.*s'",
                qp_dbname.len,
                qp_dbname.via.raw);
        return CPROTO_ERR_ADMIN;
    }

    dbpath_len = strlen(siri.cfg->default_db_path) + qp_dbname.len + 2;

    char dbpath[dbpath_len];
    sprintf(dbpath,
            "%s%.*s\\",
            siri.cfg->default_db_path,
            (int) qp_dbname.len,
            qp_dbname.via.raw);

}

static int
