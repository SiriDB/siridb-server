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
#include <pcre.h>
#include <lock/lock.h>

#define DB_CONF_FN "database.conf"
#define DB_DAT_FN "database.dat"
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

    regex = pcre_compile(
                "^[a-zA-Z][a-zA-Z0-9-_]{,18}[a-zA-Z0-9]$",
                0,
                &pcre_error_str,
                &pcre_error_offset,
                NULL);
    if (regex == NULL)
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
        free(regex);
        return -1;
    }

    siri.dbname_regex = regex;
    siri.dbname_regex_extra = regex_extra;

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
    FILE * fp;
    qp_obj_t qp_key, qp_dbname;
    size_t dbpath_len;
    int pcre_exec_ret;
    int sub_str_vec[2];
    int rc;
    struct stat st = {0};
    int8_t time_precision;
    siridb_t * siridb;

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
                (int) qp_dbname.len,
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

    if (stat(dbpath, &st) != -1)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "database directory already exists: %s",
                dbpath);
        return CPROTO_ERR_ADMIN;
    }

    if (mkdir(dbpath, 0700) == -1)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot create directory: %s",
                dbpath);
        return CPROTO_ERR_ADMIN;
    }

    char dbfn[dbpath_len + strlen(DB_CONF_FN)];
    sprintf(dbfn, "%s%s", dbpath, DB_CONF_FN);

    fp = fopen(dbfn, "w");
    if (fp == NULL)
    {
        ADMIN_rollback_new_database(dbpath);
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot open file for writing: %s",
                dbfn);
        return CPROTO_ERR_ADMIN;
    }

    rc = fputs(DEFAULT_CONF, fp);

    if (fclose(fp) || rc < 0)
    {
        ADMIN_rollback_new_database(dbpath);
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot write file: %s",
                dbfn);
        return CPROTO_ERR_ADMIN;
    }

    sprintf(dbfn, "%s%s", dbpath, DB_DAT_FN);
    fp = qp_open(dbfn, "w");
    if (fp == NULL)
    {
        ADMIN_rollback_new_database(dbpath);
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot open file for writing: %s",
                dbfn);
        return CPROTO_ERR_ADMIN;
    }
    rc = 0;
    if (qp_fadd_type(fp, QP_ARRAY_OPEN) ||
        qp_fadd_int8(fp, SIRIDB_SHEMA) ||
        qp_fadd_raw(fp, qp_dbname.via.raw, qp_dbname.len) ||
        qp_fadd_int8(fp, time_precession) ||
        qp_fadd_int64(fp, buffer_size) ||
        qp_fadd_int64(fp, duration_num) ||
        qp_fadd_int64(fp, duration_log) ||
        qp_fadd_string(fp, "NAIVE") ||
        qp_fadd_double(fp, 1.0) ||
        qp_fadd_type(fp, QP_ARRAY_CLOSE))
    {
        rc = -1;
    }

    if (qp_close(fp) || rc == -1)
    {
        ADMIN_rollback_new_database(dbpath);
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot write file: %s",
                dbfn);
        return CPROTO_ERR_ADMIN;
    }

    siridb = siridb_new(dbpath, LOCK_QUIT_IF_EXIST);
    if (siridb == NULL)
    {
        ADMIN_rollback_new_database(dbpath);
        sprintf(err_msg, "error loading database");
        return CPROTO_ERR_ADMIN;
    }

    siridb->server->flags |= SERVER_FLAG_RUNNING;

    /* Force one heart-beat */
    siri_heartbeat_force();

    return 0;
}

static void ADMIN_rollback_new_database(const char * dbpath)
{
    size_t dbpath_len = strlen(dbpath);
    char dbfn[dbpath_len + strlen(DB_CONF_FN)];
    sprintf(dbfn, "%s%s", dbpath, DB_CONF_FN);
    unlink(dbfn);
    sprintf(dbfn, "%s%s", dbpath, DB_DAT_FN);
    unlink(dbfn);
    if (rmdir(dbpath))
    {
        log_error("Roll-back creating new database has failed.");
    }
}

static int8_t ADMIN_time_precision(qp_time_precision)
{
    if (qp_time_precision.tp)
}

