/*
 * request.c - SiriDB Service Request.
 */
#define PCRE2_CODE_UNIT_WIDTH 8

#include <lock/lock.h>
#include <logger/logger.h>
#include <pcre2.h>
#include <siri/db/buffer.h>
#include <siri/db/reindex.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/service/account.h>
#include <siri/service/client.h>
#include <siri/service/request.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <xmath/xmath.h>

#define DEFAULT_TIME_PRECISION 1
#define DEFAULT_BUFFER_SIZE 1024
#define DEFAULT_DURATION_NUM 604800
#define DEFAULT_DURATION_LOG 86400
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
"# path = <buffer_path>\n" \
"\n" \
"# Buffer size in bytes. This size must be a multiple of 512 with a maximum\n" \
"# of 1048576 bytes. Be careful using large values since SiriDB will require\n" \
"# memory based on this value. A value between 1024 and 32768 is recommended.\n" \
"# size = 1024\n"

#define CHECK_DBNAME_AND_CREATE_PATH                                        \
    pcre_exec_ret = pcre2_match(                                            \
            siri.dbname_regex,                                              \
            (PCRE2_SPTR8) qp_dbname.via.raw,                                \
            qp_dbname.len,                                                  \
            0,                                                              \
            0,                                                              \
            siri.dbname_match_data,                                         \
            NULL);                                                          \
                                                                            \
    if (pcre_exec_ret < 0)                                                  \
    {                                                                       \
        snprintf(                                                           \
                err_msg,                                                    \
                SIRI_MAX_SIZE_ERR_MSG,                                      \
                "invalid database name: '%.*s'",                            \
                (int) qp_dbname.len,                                        \
                qp_dbname.via.raw);                                         \
        return CPROTO_ERR_SERVICE;                                          \
    }                                                                       \
                                                                            \
    if (llist_get(                                                          \
            siri.siridb_list,                                               \
            (llist_cb) SERVICE_find_database,                               \
            &qp_dbname) != NULL)                                            \
    {                                                                       \
        snprintf(                                                           \
                err_msg,                                                    \
                SIRI_MAX_SIZE_ERR_MSG,                                      \
                "database name already exists: '%.*s'",                     \
                (int) qp_dbname.len,                                        \
                qp_dbname.via.raw);                                         \
        return CPROTO_ERR_SERVICE;                                          \
    }                                                                       \
                                                                            \
    dbpath_len = strlen(siri.cfg->db_path) + qp_dbname.len + 2;     \
    char dbpath[dbpath_len];                                                \
    sprintf(dbpath,                                                         \
            "%s%.*s/",                                                      \
            siri.cfg->db_path,                                      \
            (int) qp_dbname.len,                                            \
            qp_dbname.via.raw);                                             \
                                                                            \
    if (stat(dbpath, &st) != -1)                                            \
    {                                                                       \
        snprintf(                                                           \
                err_msg,                                                    \
                SIRI_MAX_SIZE_ERR_MSG,                                      \
                "database directory already exists: %s",                    \
                dbpath);                                                    \
        return CPROTO_ERR_SERVICE;                                          \
    }                                                                       \
                                                                            \
    if (mkdir(dbpath, 0700) == -1)                                          \
    {                                                                       \
        snprintf(                                                           \
                err_msg,                                                    \
                SIRI_MAX_SIZE_ERR_MSG,                                      \
                "cannot create directory: %s",                              \
                dbpath);                                                    \
        return CPROTO_ERR_SERVICE;                                          \
    }                                                                       \
                                                                            \
    char dbfn[dbpath_len + max_filename_sz];                                \
    sprintf(dbfn, "%s%s", dbpath, DB_CONF_FN);                              \
                                                                            \
    fp = fopen(dbfn, "w");                                                  \
    if (fp == NULL)                                                         \
    {                                                                       \
        siri_service_request_rollback(dbpath);                              \
        snprintf(                                                           \
                err_msg,                                                    \
                SIRI_MAX_SIZE_ERR_MSG,                                      \
                "cannot open file for writing: %s",                         \
                dbfn);                                                      \
        return CPROTO_ERR_SERVICE;                                          \
    }                                                                       \
                                                                            \
    rc = fputs(DEFAULT_CONF, fp);                                           \
                                                                            \
    if (fclose(fp) || rc < 0)                                               \
    {                                                                       \
        siri_service_request_rollback(dbpath);                              \
        snprintf(                                                           \
                err_msg,                                                    \
                SIRI_MAX_SIZE_ERR_MSG,                                      \
                "cannot write file: %s",                                    \
                dbfn);                                                      \
        return CPROTO_ERR_SERVICE;                                          \
    }

static cproto_server_t SERVICE_on_new_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t SERVICE_on_change_password(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t SERVICE_on_drop_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t SERVICE_on_drop_database(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t SERVICE_on_new_database(
        qp_unpacker_t * qp_unpacker,
        char * err_msg);
static cproto_server_t SERVICE_on_new_replica_or_pool(
        qp_unpacker_t * qp_unpacker,
        uint16_t pid,
        sirinet_stream_t * client,
        int req,
        char * err_msg);
static cproto_server_t SERVICE_on_get_version(
        qp_unpacker_t * qp_unpacker,
        qp_packer_t ** packaddr,
        char * err_msg);
static cproto_server_t SERVICE_on_get_accounts(
        qp_unpacker_t * qp_unpacker,
        qp_packer_t ** packaddr,
        char * err_msg);
static cproto_server_t SERVICE_on_get_databases(
        qp_unpacker_t * qp_unpacker,
        qp_packer_t ** packaddr,
        char * err_msg);
static int8_t SERVICE_time_precision(qp_obj_t * qp_time_precision);
static int64_t SERVICE_duration(qp_obj_t * qp_duration, uint8_t time_precision);
static int SERVICE_list_databases(siridb_t * siridb, qp_packer_t * packer);
static int SERVICE_find_database(siridb_t * siridb, qp_obj_t * dbname);
static int SERVICE_list_accounts(
        siri_service_account_t * account,
        qp_packer_t * packer);
static void SERVICE_on_drop_database_cb(vec_t *, void *);

static size_t max_filename_sz;

/*
 * Initialize service requests. (called once when initializing SiriDB)
 */
int siri_service_request_init(void)
{
    max_filename_sz = xmath_max_size(
            3,
            strlen(DB_CONF_FN),
            strlen(DB_DAT_FN),
            strlen(REINDEX_FN));

    int pcre_error_num;
    PCRE2_SIZE pcre_error_offset;

    pcre2_code * regex;
    pcre2_match_data * match_data;

    regex = pcre2_compile(
                (PCRE2_SPTR8) "^[a-zA-Z][a-zA-Z0-9-_]{0,18}[a-zA-Z0-9]$",
                PCRE2_ZERO_TERMINATED,
                0,
                &pcre_error_num,
                &pcre_error_offset,
                NULL);
    if (regex == NULL)
    {
        return -1;
    }
    match_data = pcre2_match_data_create_from_pattern(regex, NULL);

    if(match_data == NULL)
    {
        pcre2_match_data_free(match_data);
        pcre2_code_free(regex);
        return -1;
    }

    siri.dbname_regex = regex;
    siri.dbname_match_data = match_data;

    return 0;
}

/*
 * Destroy service requests. (only called when exiting SiriDB)
 */
void siri_service_request_destroy(void)
{
    pcre2_match_data_free(siri.dbname_match_data);
    pcre2_code_free(siri.dbname_regex);
}

/*
 * Returns CPROTO_ACK_SERVICE or CPROTO_DEFERRED when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
cproto_server_t siri_service_request(
        int tp,
        qp_unpacker_t * qp_unpacker,
        qp_packer_t ** packaddr,
        uint16_t pid,
        sirinet_stream_t * client,
        char * err_msg)
{
    switch ((service_request_t) tp)
    {
    case SERVICE_NEW_ACCOUNT:
        return SERVICE_on_new_account(qp_unpacker, err_msg);
    case SERVICE_CHANGE_PASSWORD:
        return SERVICE_on_change_password(qp_unpacker, err_msg);
    case SERVICE_DROP_ACCOUNT:
        return SERVICE_on_drop_account(qp_unpacker, err_msg);
    case SERVICE_NEW_DATABASE:
        return SERVICE_on_new_database(qp_unpacker, err_msg);
    case SERVICE_NEW_POOL:
        return SERVICE_on_new_replica_or_pool(
                qp_unpacker,
                pid,
                client,
                SERVICE_NEW_POOL,
                err_msg);
    case SERVICE_NEW_REPLICA:
        return SERVICE_on_new_replica_or_pool(
                qp_unpacker,
                pid,
                client,
                SERVICE_NEW_REPLICA,
                err_msg);
    case SERVICE_DROP_DATABASE:
        return SERVICE_on_drop_database(qp_unpacker, err_msg);
    case SERVICE_GET_VERSION:
        return SERVICE_on_get_version(qp_unpacker, packaddr, err_msg);
    case SERVICE_GET_ACCOUNTS:
        return SERVICE_on_get_accounts(qp_unpacker, packaddr, err_msg);
    case SERVICE_GET_DATABASES:
        return SERVICE_on_get_databases(qp_unpacker, packaddr, err_msg);
    default:
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }
}

/*
 * Returns CPROTO_ACK_SERVICE when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
static cproto_server_t SERVICE_on_new_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    qp_obj_t qp_key, qp_account, qp_password;

    qp_account.tp = QP_HOOK;
    qp_password.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "account",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_account) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "password",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_password) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_account.tp == QP_HOOK || qp_password.tp == QP_HOOK)
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    return (siri_service_account_new(
            &siri,
            &qp_account,
            &qp_password,
            0,
            err_msg) ||
            siri_service_account_save(&siri, err_msg)) ?
                    CPROTO_ERR_SERVICE : CPROTO_ACK_SERVICE;
}

/*
 * Returns CPROTO_ACK_SERVICE when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
static cproto_server_t SERVICE_on_change_password(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    qp_obj_t qp_key, qp_account, qp_password;

    qp_account.tp = QP_HOOK;
    qp_password.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "account",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_account) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "password",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_password) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_account.tp == QP_HOOK || qp_password.tp == QP_HOOK)
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    return (siri_service_account_change_password(
            &siri,
            &qp_account,
            &qp_password,
            err_msg) ||
            siri_service_account_save(&siri, err_msg)) ?
                    CPROTO_ERR_SERVICE : CPROTO_ACK_SERVICE;
}

/*
 * Returns CPROTO_ACK_SERVICE when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
static cproto_server_t SERVICE_on_drop_account(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    qp_obj_t qp_key, qp_target;

    qp_target.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "account",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_target) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_target.tp == QP_HOOK)
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (siri.accounts->len == 1)
    {
        sprintf(err_msg,
                "at least one service account is required, "
                "cannot drop the last service account");
        return CPROTO_ERR_SERVICE;
    }

    return (siri_service_account_drop(
            &siri,
            &qp_target,
            err_msg) ||
            siri_service_account_save(&siri, err_msg)) ?
                    CPROTO_ERR_SERVICE : CPROTO_ACK_SERVICE;
}

/*
 * Returns CPROTO_ACK_SERVICE when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
static cproto_server_t SERVICE_on_drop_database(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    bool ignore_offline;
    siridb_t * siridb;
    qp_obj_t qp_key, qp_target, qp_ignore_offline;
    sirinet_pkg_t * pkg;
    vec_t * servers;

    qp_target.tp = QP_HOOK;
    qp_ignore_offline.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "database",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_target) == QP_RAW)
        {
            continue;
        }

        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "ignore_offline",
                    qp_key.len) == 0 &&
                qp_is_bool(qp_next(qp_unpacker, &qp_ignore_offline)))
        {
            continue;
        }
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_target.tp == QP_HOOK)
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    ignore_offline = (
            qp_ignore_offline.tp != QP_HOOK &&
            qp_ignore_offline.tp == QP_TRUE
    );

    siridb = siridb_get_by_qp(siri.siridb_list, &qp_target);
    if (siridb == NULL)
    {
        sprintf(err_msg, "cannot find database '%.*s'",
                (int) qp_target.len, (char *) qp_target.via.raw);
        return CPROTO_ERR_SERVICE;
    }

    if (!ignore_offline && !siridb_servers_online(siridb))
    {
        sprintf(err_msg,
                "at least one server is off-line, "
                "set `ignore_offline` to true if you want to "
                "ignore off-line servers");
        return CPROTO_ERR_SERVICE;
    }

    pkg = sirinet_pkg_new(0, 0, BPROTO_DROP_DATABASE, NULL);
    servers = siridb_servers_other2vec(siridb);
    if (pkg == NULL || servers == NULL)
    {
        free(pkg);
        vec_free(servers);
        sprintf(err_msg, "memory allocation error");
        return CPROTO_ERR_SERVICE;
    }

    siridb_servers_send_pkg(
            servers,
            pkg,
            0,
            SERVICE_on_drop_database_cb,
            NULL);

    siridb_drop(siridb);
    vec_free(servers);

    return CPROTO_ACK_SERVICE;
}

/*
 * Returns CPROTO_ACK_SERVICE when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
static cproto_server_t SERVICE_on_new_database(
        qp_unpacker_t * qp_unpacker,
        char * err_msg)
{
    FILE * fp;
    qp_obj_t
        qp_key,
        qp_dbname,
        qp_time_precision,
        qp_buffer_size,
        qp_duration_num,
        qp_duration_log;
    size_t dbpath_len;
    int pcre_exec_ret;
    int rc;
    struct stat st;
    int8_t time_precision;
    int64_t buffer_size, duration_num, duration_log;
    siridb_t * siridb;
    uuid_t uuid;

    memset(&st, 0, sizeof(struct stat));

    if (siri.siridb_list->len == MAX_NUMBER_DB)
    {
        sprintf(err_msg,
                "maximum number of databases is reached (%zd)",
                siri.siridb_list->len);
        return CPROTO_ERR_SERVICE;
    }

    qp_dbname.tp = QP_HOOK;
    qp_time_precision.tp = QP_HOOK;
    qp_buffer_size.tp = QP_HOOK;
    qp_duration_num.tp = QP_HOOK;
    qp_duration_log.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, NULL)))
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "dbname",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_dbname) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "time_precision",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_time_precision) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "buffer_size",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_buffer_size) == QP_INT64)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "duration_num",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_duration_num) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "duration_log",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_duration_log) == QP_RAW)
        {
            continue;
        }
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_dbname.tp == QP_HOOK)
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    time_precision = (qp_time_precision.tp == QP_HOOK) ?
            DEFAULT_TIME_PRECISION : SERVICE_time_precision(&qp_time_precision);
    if (time_precision == -1)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "invalid time precision: '%.*s' (expecting s, ms, us or ns)",
                (int) qp_time_precision.len,
                qp_time_precision.via.raw);
        return CPROTO_ERR_SERVICE;
    }

    duration_num = (qp_duration_num.tp == QP_HOOK) ?
            DEFAULT_DURATION_NUM * xmath_ipow(1000, time_precision):
            SERVICE_duration(&qp_duration_num, time_precision);

    if (duration_num == -1)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "invalid number duration: '%.*s' "
                "(valid examples: 6h, 2d or 1w)",
                (int) qp_duration_num.len,
                qp_duration_num.via.raw);
        return CPROTO_ERR_SERVICE;
    }

    duration_log = (qp_duration_log.tp == QP_HOOK) ?
            DEFAULT_DURATION_LOG * xmath_ipow(1000, time_precision):
            SERVICE_duration(&qp_duration_log, time_precision);

    if (duration_log == -1)
    {
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "invalid log duration: '%.*s' "
                "(valid examples: 6h, 2d or 1w)",
                (int) qp_duration_log.len,
                qp_duration_log.via.raw);
        return CPROTO_ERR_SERVICE;
    }

    buffer_size = (qp_buffer_size.tp == QP_HOOK) ?
            DEFAULT_BUFFER_SIZE : qp_buffer_size.via.int64;

    if (!siridb_buffer_is_valid_size(buffer_size))
    {
        sprintf(err_msg,
                "invalid buffer size: %" PRId64
                " (expecting a multiple of 512 with a maximum of %" PRId64 ")",
                buffer_size,
                (int64_t) MAX_BUFFER_SZ);
        return CPROTO_ERR_SERVICE;
    }

    CHECK_DBNAME_AND_CREATE_PATH

    sprintf(dbfn, "%s%s", dbpath, DB_DAT_FN);
    fp = qp_open(dbfn, "w");
    if (fp == NULL)
    {
        siri_service_request_rollback(dbpath);
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot open file for writing: %s",
                dbfn);
        return CPROTO_ERR_SERVICE;
    }
    rc = 0;
    uuid_generate(uuid);

    if (qp_fadd_type(fp, QP_ARRAY_OPEN) ||
        qp_fadd_int64(fp, SIRIDB_SCHEMA) ||
        qp_fadd_raw(fp, (const unsigned char *) uuid, 16) ||
        qp_fadd_raw(fp, qp_dbname.via.raw, qp_dbname.len) ||
        qp_fadd_int64(fp, time_precision) ||
        qp_fadd_int64(fp, buffer_size) ||
        qp_fadd_int64(fp, duration_num) ||
        qp_fadd_int64(fp, duration_log) ||
        qp_fadd_string(fp, "NAIVE") ||
        qp_fadd_double(fp, DEF_DROP_THRESHOLD) ||
        qp_fadd_int64(fp, DEF_SELECT_POINTS_LIMIT) ||
        qp_fadd_int64(fp, DEF_LIST_LIMIT) ||
        qp_fadd_type(fp, QP_NULL) ||
        qp_fadd_int64(fp, 0) ||
        qp_fadd_int64(fp, 0) ||
        qp_fadd_type(fp, QP_ARRAY_CLOSE))
    {
        rc = -1;
    }

    if (qp_close(fp) || rc == -1)
    {
        siri_service_request_rollback(dbpath);
        snprintf(
                err_msg,
                SIRI_MAX_SIZE_ERR_MSG,
                "cannot write file: %s",
                dbfn);
        return CPROTO_ERR_SERVICE;
    }

    siridb = siridb_new(dbpath, LOCK_QUIT_IF_EXIST);
    if (siridb == NULL)
    {
        siri_service_request_rollback(dbpath);
        sprintf(err_msg, "error loading database");
        return CPROTO_ERR_SERVICE;
    }

    siridb->server->flags |= SERVER_FLAG_RUNNING;

    /* Force one heart-beat */
    siri_heartbeat_force();

    return CPROTO_ACK_SERVICE;
}

/*
 * Returns CPROTO_DEFERRED when successful.
 * In case of an error CPROTO_ERR_SERVICE can be returned in which case err_msg
 * is set, or CPROTO_ERR_SERVICE_INVALID_REQUEST is returned.
 */
static cproto_server_t SERVICE_on_new_replica_or_pool(
        qp_unpacker_t * qp_unpacker,
        uint16_t pid,
        sirinet_stream_t * client,
        int req,
        char * err_msg)
{
    FILE * fp;
    qp_obj_t
        qp_key,
        qp_dbname,
        qp_pool,
        qp_host,
        qp_port,
        qp_username,
        qp_password;
    size_t dbpath_len;
    int pcre_exec_ret;
    int rc;
    struct stat st;
    uint16_t port;
    uuid_t uuid;

    memset(&st, 0, sizeof(struct stat));

    if (siri.siridb_list->len == MAX_NUMBER_DB)
    {
        sprintf(err_msg,
                "maximum number of databases is reached (%zd)",
                siri.siridb_list->len);
        return CPROTO_ERR_SERVICE;
    }

    qp_dbname.tp = QP_HOOK;
    qp_pool.tp = QP_HOOK;
    qp_host.tp = QP_HOOK;
    qp_port.tp = QP_HOOK;
    qp_username.tp = QP_HOOK;
    qp_password.tp = QP_HOOK;

    if (!qp_is_map(qp_next(qp_unpacker, &qp_key)))
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    while (qp_next(qp_unpacker, &qp_key) == QP_RAW)
    {
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "dbname",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_dbname) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw, "pool", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_pool) == QP_INT64)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw, "host", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_host) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw, "port", qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_port) == QP_INT64)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "username",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_username) == QP_RAW)
        {
            continue;
        }
        if (    strncmp(
                    (const char *) qp_key.via.raw,
                    "password",
                    qp_key.len) == 0 &&
                qp_next(qp_unpacker, &qp_password) == QP_RAW)
        {
            continue;
        }

        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_dbname.tp == QP_HOOK ||
        (
            (req == SERVICE_NEW_POOL && qp_pool.tp != QP_HOOK) ||
            (req == SERVICE_NEW_REPLICA && qp_pool.tp == QP_HOOK)
        ) ||
        qp_host.tp == QP_HOOK ||
        qp_port.tp == QP_HOOK ||
        qp_username.tp == QP_HOOK ||
        qp_password.tp == QP_HOOK)
    {
        return CPROTO_ERR_SERVICE_INVALID_REQUEST;
    }

    if (qp_port.via.int64 < 1 || qp_port.via.int64 > 65535)
    {
        sprintf(err_msg,
                "invalid port number: %" PRId64
                " (expecting a value between 0 and 65536)",
                qp_port.via.int64);
        return CPROTO_ERR_SERVICE;
    }

    port = (uint16_t) qp_port.via.int64;
    uuid_generate(uuid);

    CHECK_DBNAME_AND_CREATE_PATH

    if (req == SERVICE_NEW_POOL)
    {
        sprintf(dbfn, "%s%s", dbpath, REINDEX_FN);
        fp = fopen(dbfn, "w");
        if (fp == NULL || fclose(fp))
        {
            siri_service_request_rollback(dbpath);
            snprintf(
                    err_msg,
                    SIRI_MAX_SIZE_ERR_MSG,
                    "cannot open file for writing: %s",
                    dbfn);
            return CPROTO_ERR_SERVICE;
        }
    }

    if (siri_service_client_request(
            pid,
            port,
            /* -1 = new pool  */
            (req == SERVICE_NEW_POOL) ? -1 : qp_pool.via.int64,
            &uuid,
            &qp_host,
            &qp_username,
            &qp_password,
            &qp_dbname,
            dbpath,
            client,
            err_msg))
    {
        siri_service_request_rollback(dbpath);
        return CPROTO_ERR_SERVICE;
    }
    return CPROTO_DEFERRED;
}

/*
 * Returns CPROTO_ACK_SERVICE_DATA when successful.
 * In case of an error CPROTO_ERR_SERVICE will be returned and err_msg is set
 */
static cproto_server_t SERVICE_on_get_version(
        qp_unpacker_t * qp_unpacker __attribute__((unused)),
        qp_packer_t ** packaddr,
        char * err_msg)
{
    qp_packer_t * packer = sirinet_packer_new(128);
    if (packer != NULL)
    {
        if (!qp_add_type(packer, QP_ARRAY_OPEN) &&
            !qp_add_string(packer, SIRIDB_VERSION))
        {
            *packaddr = packer;
            return CPROTO_ACK_SERVICE_DATA;
        }

        /* error, free packer */
        qp_packer_free(packer);
    }
    sprintf(err_msg, "memory allocation error");
    return CPROTO_ERR_SERVICE;
}

static cproto_server_t SERVICE_on_get_accounts(
        qp_unpacker_t * qp_unpacker __attribute__((unused)),
        qp_packer_t ** packaddr,
        char * err_msg)
{
    qp_packer_t * packer = sirinet_packer_new(128);

    if (packer != NULL)
    {
        qp_add_type(packer, QP_ARRAY_OPEN);

        if (!llist_walk(
                siri.accounts,
                (llist_cb) SERVICE_list_accounts,
                packer))
        {
            *packaddr = packer;
            return CPROTO_ACK_SERVICE_DATA;
        }

        /* error, free packer */
        qp_packer_free(packer);
    }
    sprintf(err_msg, "memory allocation error");
    return CPROTO_ERR_SERVICE;
}

static cproto_server_t SERVICE_on_get_databases(
        qp_unpacker_t * qp_unpacker __attribute__((unused)),
        qp_packer_t ** packaddr,
        char * err_msg)
{
    qp_packer_t * packer = sirinet_packer_new(128);

    if (packer != NULL)
    {
        qp_add_type(packer, QP_ARRAY_OPEN);

        if (!llist_walk(
                siri.siridb_list,
                (llist_cb) SERVICE_list_databases,
                packer))
        {
            *packaddr = packer;
            return CPROTO_ACK_SERVICE_DATA;
        }

        /* error, free packer */
        qp_packer_free(packer);
    }
    sprintf(err_msg, "memory allocation error");
    return CPROTO_ERR_SERVICE;
}

void siri_service_request_rollback(const char * dbpath)
{
    size_t dbpath_len = strlen(dbpath);
    char dbfn[dbpath_len + max_filename_sz];

    sprintf(dbfn, "%s%s", dbpath, DB_CONF_FN);
    unlink(dbfn);
    sprintf(dbfn, "%s%s", dbpath, DB_DAT_FN);
    unlink(dbfn);
    sprintf(dbfn, "%s%s", dbpath, REINDEX_FN);
    unlink(dbfn);
    if (rmdir(dbpath))
    {
        log_error("Roll-back creating new database has failed.");
    }
}

static int8_t SERVICE_time_precision(qp_obj_t * qp_time_precision)
{
    if (qp_time_precision->tp != QP_RAW)
    {
        return -1;
    }
    if (qp_time_precision->len == 1 && qp_time_precision->via.raw[0] == 's')
    {
        return 0;
    }
    else if (qp_time_precision->len == 2 && qp_time_precision->via.raw[1] == 's')
    {
        switch (qp_time_precision->via.raw[0])
        {
        case 'm': return 1;
        case 'u': return 2;
        case 'n': return 3;
        }
    }
    return -1;
}

static int64_t SERVICE_duration(qp_obj_t * qp_duration, uint8_t time_precision)
{
    char * endptr;
    long int val;

    if (qp_duration->tp != QP_RAW || qp_duration->len < 2)
    {
        return -1;
    }

    val = strtol((const char *) qp_duration->via.raw, &endptr, 10);

    if (val < 1 || val > 99 || endptr == (const char *) qp_duration->via.raw)
    {
        return -1;
    }

    if (endptr != (const char *) qp_duration->via.raw + (qp_duration->len - 1))
    {
        return -1;
    }

    switch (*endptr)
    {
    case 'h': return xmath_ipow(1000, time_precision) * val * 3600;
    case 'd': return xmath_ipow(1000, time_precision) * val * 86400;
    case 'w': return xmath_ipow(1000, time_precision) * val * 604800;
    }

    return -1;
}

static int SERVICE_list_databases(siridb_t * siridb, qp_packer_t * packer)
{
    return qp_add_string(packer, siridb->dbname);
}

static int SERVICE_find_database(siridb_t * siridb, qp_obj_t * dbname)
{
    return (
        strlen(siridb->dbname) == dbname->len &&
        strncmp(
            siridb->dbname,
            (const char *) dbname->via.raw,
            dbname->len) == 0);
}

static int SERVICE_list_accounts(
        siri_service_account_t * account,
        qp_packer_t * packer)
{
    return qp_add_string(packer, account->account);
}

static void SERVICE_on_drop_database_cb(
        vec_t * promises,
        void * data __attribute__((unused)))
{
    log_debug("drop database has been send to all online servers");

    if (promises != NULL)
    {
        size_t i;
        sirinet_promise_t * promise;
        for (i = 0; i < promises->len; i++)
        {
            promise = promises->data[i];
            if (promise != NULL)
            {
                free(promise->data);
                sirinet_promise_decref(promise);
            }
        }
    }
}
