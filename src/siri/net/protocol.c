/*
 * protocol.c - Protocol definitions for SiriDB.
 */
#include <siri/net/protocol.h>
#include <stdio.h>

char protocol_str[512];

const char * sirinet_cproto_client_str(cproto_client_t n)
{
    switch (n)
    {
    case CPROTO_REQ_QUERY: return "CPROTO_REQ_QUERY";
    case CPROTO_REQ_INSERT: return "CPROTO_REQ_INSERT";
    case CPROTO_REQ_AUTH: return "CPROTO_REQ_AUTH";
    case CPROTO_REQ_PING: return "CPROTO_REQ_PING";

    /* start internal usage */
    case CPROTO_REQ_REGISTER_SERVER: return "CPROTO_REQ_REGISTER_SERVER";
    case CPROTO_REQ_FILE_SERVERS: return "CPROTO_REQ_FILE_SERVERS";
    case CPROTO_REQ_FILE_USERS: return "CPROTO_REQ_FILE_USERS";
    case CPROTO_REQ_FILE_GROUPS: return "CPROTO_REQ_FILE_GROUPS";
    case CPROTO_REQ_FILE_DATABASE: return "CPROTO_REQ_FILE_DATABASE";
    /* end internal usage */

    case CPROTO_REQ_SERVICE: return "CPROTO_REQ_SERVICE";

    default:
        sprintf(protocol_str, "CPROTO_CLIENT_TYPE_UNKNOWN (%d)", n);
        return protocol_str;
    }
}

const char * sirinet_cproto_server_str(cproto_server_t n)
{
    switch (n)
    {
    case CPROTO_RES_QUERY: return "CPROTO_RES_QUERY";
    case CPROTO_RES_INSERT: return "CPROTO_RES_INSERT";
    case CPROTO_RES_AUTH_SUCCESS: return "CPROTO_RES_AUTH_SUCCESS";
    case CPROTO_RES_ACK: return "CPROTO_RES_ACK";
    case CPROTO_RES_FILE: return "CPROTO_RES_FILE";

    case CPROTO_ACK_SERVICE: return "CPROTO_ACK_SERVICE";
    case CPROTO_ACK_SERVICE_DATA: return "CPROTO_ACK_SERVICE_DATA";

    case CPROTO_ERR_MSG: return "CPROTO_ERR_MSG";
    case CPROTO_ERR_QUERY: return "CPROTO_ERR_QUERY";
    case CPROTO_ERR_INSERT: return "CPROTO_ERR_INSERT";
    case CPROTO_ERR_SERVER: return "CPROTO_ERR_SERVER";
    case CPROTO_ERR_POOL: return "CPROTO_ERR_POOL";
    case CPROTO_ERR_USER_ACCESS: return "CPROTO_ERR_USER_ACCESS";
    case CPROTO_ERR: return "CPROTO_ERR";
    case CPROTO_ERR_NOT_AUTHENTICATED: return "CPROTO_ERR_NOT_AUTHENTICATED";
    case CPROTO_ERR_AUTH_CREDENTIALS: return "CPROTO_ERR_AUTH_CREDENTIALS";
    case CPROTO_ERR_AUTH_UNKNOWN_DB: return "CPROTO_ERR_AUTH_UNKNOWN_DB";
    case CPROTO_ERR_FILE: return "CPROTO_ERR_FILE";
    case CPROTO_ERR_SERVICE: return "CPROTO_ERR_SERVICE";
    case CPROTO_ERR_SERVICE_INVALID_REQUEST: return "CPROTO_ERR_SERVICE_INVALID_REQUEST";
    default:
        sprintf(protocol_str, "CPROTO_SERVER_TYPE_UNKNOWN (%d)", n);
        return protocol_str;
    }
}

const char * sirinet_bproto_client_str(bproto_client_t n)
{
    switch (n)
    {
    case BPROTO_AUTH_REQUEST: return "BPROTO_AUTH_REQUEST";
    case BPROTO_FLAGS_UPDATE: return "BPROTO_FLAGS_UPDATE";
    case BPROTO_LOG_LEVEL_UPDATE: return "BPROTO_LOG_LEVEL_UPDATE";
    case BPROTO_REPL_FINISHED: return "BPROTO_REPL_FINISHED";
    case BPROTO_QUERY_SERVER: return "BPROTO_QUERY_SERVER";
    case BPROTO_QUERY_UPDATE: return "BPROTO_QUERY_UPDATE";
    case BPROTO_INSERT_POOL: return "BPROTO_INSERT_POOL";
    case BPROTO_INSERT_SERVER: return "BPROTO_INSERT_SERVER";
    case BPROTO_INSERT_TEST_POOL: return "BPROTO_INSERT_TEST_POOL";
    case BPROTO_INSERT_TEST_SERVER: return "BPROTO_INSERT_TEST_SERVER";
    case BPROTO_INSERT_TESTED_POOL: return "BPROTO_INSERT_TESTED_POOL";
    case BPROTO_INSERT_TESTED_SERVER: return "BPROTO_INSERT_TESTED_SERVER";
    case BPROTO_REGISTER_SERVER: return "BPROTO_REGISTER_SERVER";
    case BPROTO_DROP_SERIES: return "BPROTO_DROP_SERIES";
    case BPROTO_REQ_GROUPS: return "BPROTO_REQ_GROUPS";
    case BPROTO_ENABLE_BACKUP_MODE: return "BPROTO_ENABLE_BACKUP_MODE";
    case BPROTO_DISABLE_BACKUP_MODE: return "BPROTO_DISABLE_BACKUP_MODE";
    default:
        sprintf(protocol_str, "BPROTO_CLIENT_TYPE_UNKNOWN (%d)", n);
        return protocol_str;
    }
}

const char * sirinet_bproto_server_str(bproto_server_t n)
{
    switch (n)
    {
    case BPROTO_RES_QUERY: return "BPROTO_RES_QUERY";
    case BPROTO_ERR_QUERY: return "BPROTO_ERR_QUERY";
    case BPROTO_ERR_SERVER: return "BPROTO_ERR_SERVER";
    case BPROTO_ERR_POOL: return "BPROTO_ERR_POOL";
    case BPROTO_AUTH_ERR_UNKNOWN_UUID: return "BPROTO_AUTH_ERR_UNKNOWN_UUID";
    case BPROTO_AUTH_ERR_UNKNOWN_DBNAME: return "BPROTO_AUTH_ERR_UNKNOWN_DBNAME";
    case BPROTO_AUTH_ERR_INVALID_UUID: return "BPROTO_AUTH_ERR_INVALID_UUID";
    case BPROTO_AUTH_ERR_VERSION_TOO_OLD: return "BPROTO_AUTH_ERR_VERSION_TOO_OLD";
    case BPROTO_AUTH_ERR_VERSION_TOO_NEW: return "BPROTO_AUTH_ERR_VERSION_TOO_NEW";
    case BPROTO_ERR_NOT_AUTHENTICATED: return "BPROTO_ERR_NOT_AUTHENTICATED";
    case BPROTO_ERR_INSERT: return "BPROTO_ERR_INSERT";
    case BPROTO_ERR_REGISTER_SERVER: return "BPROTO_ERR_REGISTER_SERVER";
    case BPROTO_ERR_DROP_SERIES: return "BPROTO_ERR_DROP_SERIES";
    case BPROTO_ERR_ENABLE_BACKUP_MODE: return "BPROTO_ERR_ENABLE_BACKUP_MODE";
    case BPROTO_ERR_DISABLE_BACKUP_MODE: return "BPROTO_ERR_DISABLE_BACKUP_MODE";
    case BPROTO_AUTH_SUCCESS: return "BPROTO_AUTH_SUCCESS";
    case BPROTO_ACK_FLAGS: return "BPROTO_ACK_FLAGS";
    case BPROTO_ACK_LOG_LEVEL: return "BPROTO_ACK_LOG_LEVEL";
    case BPROTO_ACK_INSERT: return "BPROTO_ACK_INSERT";
    case BPROTO_ACK_REPL_FINISHED: return "BPROTO_ACK_REPL_FINISHED";
    case BPROTO_ACK_REGISTER_SERVER: return "BPROTO_ACK_REGISTER_SERVER";
    case BPROTO_ACK_DROP_SERIES: return "BPROTO_ACK_DROP_SERIES";
    case BPROTO_ACK_ENABLE_BACKUP_MODE: return "BPROTO_ACK_ENABLE_BACKUP_MODE";
    case BPROTO_ACK_DISABLE_BACKUP_MODE: return "BPROTO_ACK_DISABLE_BACKUP_MODE";
    case BPROTO_RES_GROUPS: return "BPROTO_RES_GROUPS";
    default:
        sprintf(protocol_str, "BPROTO_SERVER_TYPE_UNKNOWN (%d)", n);
        return protocol_str;
    }
}
