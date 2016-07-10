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
    CPROTO_RES_ACK,                             // Empty
    CPROTO_RES_AUTH_SUCCESS,                    // Empty

    CPROTO_ERR_QUERY=64,                        // {"error_msg": ...}
    CPROTO_ERR_INSERT,                          // {"error_msg": ...}
    CPROTO_ERR_SERVER,                          // {"error_msg": ...}
    CPROTO_ERR_POOL,                            // {"error_msg": ...}
    CPROTO_ERR_USER_ACCESS,                     // {"error_msg": ...}
    CPROTO_ERR_NOT_AUTHENTICATED,               // Empty
    CPROTO_ERR_AUTH_CREDENTIALS,                // Empty
    CPROTO_ERR_AUTH_UNKNOWN_DB,                 // Empty
} cproto_client_t;

typedef enum
{
    CPROTO_REQ_QUERY,                           // (query, time_precision)
    CPROTO_REQ_INSERT,                          // series with points map/array
    CPROTO_REQ_AUTH,                            // (user, password, dbname)
    CPROTO_REQ_PING,                            // Empty
} mproto_server_t;

typedef enum
{
    BPROTO_AUTH_REQUEST=128,                    /* (uuid, dbname, flags, version,
                                                min_version, dbpath, buffer_path,
                                                buffer_size, startup_time) */
    BPROTO_FLAGS_UPDATE,                        // flags
    BPROTO_LOG_LEVEL_UPDATE,                    // log_level
    BPROTO_QUERY_SERVER,                        // (query, time_precision)
    BPROTO_QUERY_POOL,                          // (query, time_precision)
    BPROTO_INSERT_POOL,                         // {series: points, ...}
    BPROTO_INSERT_SERVER,                       // {series: points, ...}
} bproto_client_t;

typedef enum
{
    BPROTO_RES_QUERY=CPROTO_RES_QUERY,          // {query response data}
    BPROTO_ERR_QUERY=CPROTO_ERR_QUERY,          // {"error_msg": ...}

    BPROTO_AUTH_SUCCESS=128,                    // Empty
    BPROTO_ACK_FLAGS,                           // Empty
    BPROTO_ACK_LOG_LEVEL,                       // Empty
    BPROTO_ACK_INSERT,                          // Empty
    BPROTO_AUTH_ERR_UNKNOWN_UUID,               // Empty
    BPROTO_AUTH_ERR_UNKNOWN_DBNAME,             // Empty
    BPROTO_AUTH_ERR_INVALID_UUID,               // Empty
    BPROTO_AUTH_ERR_VERSION_TOO_OLD,            // Empty
    BPROTO_AUTH_ERR_VERSION_TOO_NEW,            // Empty
    BPROTO_ERR_NOT_AUTHENTICATED,               // Empty
    BPROTO_ERR_INSERT,                          // Empty
} bproto_server_t;

