/*
 * protocol.h - Protocol for SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 17-03-2016
 *
 */
#pragma once

typedef enum
{
    CPROTO_RES_QUERY,                           // {query response data}
    CPROTO_RES_INSERT,                          // {"success_msg": ...}
    CPROTO_RES_ACK,                             // empty
    CPROTO_RES_AUTH_SUCCESS,                    // empty

    CPROTO_ERR_QUERY=64,                        // {"error_msg": ...}
    CPROTO_ERR_INSERT,                          // {"error_msg": ...}
    CPROTO_ERR_SERVER,                          // {"error_msg": ...}
    CPROTO_ERR_POOL,                            // {"error_msg": ...}
    CPROTO_ERR_USER_ACCESS,                     // {"error_msg": ...}
    CPROTO_ERR_NOT_AUTHENTICATED,               // empty
    CPROTO_ERR_AUTH_CREDENTIALS,                // empty
    CPROTO_ERR_AUTH_UNKNOWN_DB,                 // empty
} cproto_client_t;

typedef enum
{
    CPROTO_REQ_QUERY,                           // (query, time_precision)
    CPROTO_REQ_INSERT,                          // series with points map/array
    CPROTO_REQ_AUTH,                            // (user, password, dbname)
    CPROTO_REQ_PING,                            // empty
} mproto_server_t;

typedef enum
{
    BPROTO_AUTH_REQUEST=128,                    /* (uuid, dbname, flags, version,
                                                min_version, dbpath, buffer_path,
                                                buffer_size, startup_time) */
    BPROTO_FLAGS_UPDATE,                        // flags
    BPROTO_LOG_LEVEL_UPDATE,                    // log_level
    BPROTO_REPL_FINISHED,                       // empty
    BPROTO_QUERY_SERVER,                        // (query, time_precision)
    BPROTO_QUERY_POOL,                          // (query, time_precision)
    BPROTO_INSERT_POOL,                         // {series: points, ...}
    BPROTO_INSERT_SERVER,                       // {series: points, ...}
} bproto_client_t;

/*
 * Success and error messages are set this way so that we have one range
 * for error messages between 64.. 191.
 *
 * Client Success messages in range 0..63
 * Client Error messages in range 64..127
 * Back-end Error messages in range 128..191
 * Back-end Success messages in range 192..255
 */
typedef enum
{
    BPROTO_RES_QUERY=CPROTO_RES_QUERY,          // {query response data}

    BPROTO_ERR_QUERY=CPROTO_ERR_QUERY,          // {"error_msg": ...}

    BPROTO_AUTH_ERR_UNKNOWN_UUID=128,           // empty
    BPROTO_AUTH_ERR_UNKNOWN_DBNAME,             // empty
    BPROTO_AUTH_ERR_INVALID_UUID,               // empty
    BPROTO_AUTH_ERR_VERSION_TOO_OLD,            // empty
    BPROTO_AUTH_ERR_VERSION_TOO_NEW,            // empty
    BPROTO_ERR_NOT_AUTHENTICATED,               // empty
    BPROTO_ERR_INSERT,                          // empty

    BPROTO_AUTH_SUCCESS=192,                    // empty
    BPROTO_ACK_FLAGS,                           // empty
    BPROTO_ACK_LOG_LEVEL,                       // empty
    BPROTO_ACK_INSERT,                          // empty
    BPROTO_ACK_REPL_FINISHED,                   // empty

} bproto_server_t;

#define sirinet_protocol_is_error(tp) (tp >= 64 && tp < 192)
