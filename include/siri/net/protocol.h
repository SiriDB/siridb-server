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

typedef enum sirinet_protocal_msg_client_tp
{
    MP_RES_AUTH_SUCCESS,                    // Empty
    MP_RES_QUERY,                           // {query response data}
    MP_RES_INSERT,                          // {"success_msg": ...}

    MP_ERR_QUERY=64,                        // {"error_msg": ...}
    MP_ERR_INSERT,                          // {"error_msg": ...}
    MP_ERR_SERVER,                          // {"error_msg": ...}
    MP_ERR_POOL,                            // {"error_msg": ...}
    MP_ERR_INSUFFICIENT_PRIVILEGES,         // {"error_msg": ...}
    MP_ERR_INVALID_CREDENTIALS,             // Empty
    MP_ERR_NOT_AUTHENTICATED,               // Empty
    MP_ERR_UNKNOWN_DATABASE,                // Empty

} mp_client_t;

typedef enum sirinet_protocal_msg_server_tp
{
    MP_REQ_AUTH,                            // (user, password, dbname)
    MP_REQ_QUERY,                           // (query, time_precision)
    MP_REQ_INSERT,                          // series with points map/array

} mp_server_t;


typedef enum sirinet_protocal_msg_tp
{

    SN_MSG_INVALID_CREDENTIALS,             // Empty
    SN_MSG_AUTH_SUCCESS,                    // Empty
    SN_MSG_NOT_AUTHENTICATED,               // Empty
    SN_MSG_QUERY,                           // (query, time_precision)
    SN_MSG_INSERT,                          // [...series with points...]
    SN_MSG_INSERT_ERROR,                    // string
    SN_MSG_QUERY_ERROR,                     // string
    SN_MSG_RESULT,                          // query/insert response data
    SN_MSG_UNKNOWN_DATABASE,                // invalid_db_name
    SN_MSG_CHECK_REGISTER_SERVER,           // {uuid,pool,address,port,version}
    SN_MSG_INSUFFICIENT_PRIVILEGES,         // Empty
    SN_MSG_REGISTER_SERVER,                 // {uuid,pool,address,port,version}
    SN_MSG_FILE,                            // file (bin data)
    SN_MSG_REQUEST_SERVERS_FILE,            // Empty
    SN_MSG_REQUEST_USER_ACCESS_FILE,        // Empty
    SN_MSG_REQUEST_GROUPS_FILE,             // Empty
    SN_MSG_REQUEST_NETWORK_ACCESS_FILE,     // Empty
    SN_MSG_ERROR,                           // string
    SN_MSG_FILE_NOT_FOUND_ERROR,            // Empty
    SN_MSG_UNKNOWN_PKG_TIPE,                // int -> type
    SN_MSG_SUCCESS,                         // Empty
    SN_MSG_SERVER_ERROR,                    // string
    SN_MSG_POOL_ERROR,                      // string
    SN_MSG_PING,                            // Empty
    SN_MSG_ACK,                             // Empty
} sirinet_msg_t;

typedef enum sirinet_protocol_backend_client_tp
{
    BP_AUTH_REQUEST=128,                    /* (uuid, dbname, flags, version,
                                                min_version, dbpath, buffer_path,
                                                buffer_size, startup_time) */
    BP_FLAGS_UPDATE,                        // flags
    BP_QUERY_SERVER,                        // (query, time_precision)
    BP_QUERY_POOL,                          // (query, time_precision)
    BP_LOG_LEVEL_UPDATE,                    // log_level
} bp_client_t;

typedef enum sirinet_protocol_backend_server_tp
{
    BP_ERR_QUERY=MP_ERR_QUERY,              // {"error_msg": ...}
    BP_RES_QUERY=MP_RES_QUERY,              // {query response data}

    BP_AUTH_SUCCESS=128,                    // Empty
    BP_FLAGS_ACK,                           // Empty
    BP_LOG_LEVEL_ACK,                       // Empty
    BP_AUTH_ERROR_UNKNOWN_UUID,             // Empty
    BP_AUTH_ERROR_UNKNOWN_DBNAME,           // Empty
    BP_AUTH_ERROR_INVALID_UUID,             // Empty
    BP_AUTH_ERROR_VERSION_TOO_OLD,          // Empty
    BP_AUTH_ERROR_VERSION_TOO_NEW,          // Empty
    BP_ERROR_NOT_AUTHENTICATED,             // Empty
} bp_server_t;

