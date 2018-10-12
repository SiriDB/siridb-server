/*
 * protocol.c - Protocol definitions for SiriDB.
 */
#ifndef SIRINET_PROTOCOL_H_
#define SIRINET_PROTOCOL_H_

typedef enum
{
    /* Public requests */
    CPROTO_REQ_QUERY=0,                 /* (query, time_precision)          */
    CPROTO_REQ_INSERT=1,                /* series with points map/array     */
    CPROTO_REQ_AUTH=2,                  /* (user, password, dbname)         */
    CPROTO_REQ_PING=3,                  /* empty                            */

    /* Internal usage only */
    CPROTO_REQ_REGISTER_SERVER=6,       /* (uuid, host, port, pool)         */
    CPROTO_REQ_FILE_SERVERS=7,          /* empty                            */
    CPROTO_REQ_FILE_USERS=8,            /* empty                            */
    CPROTO_REQ_FILE_GROUPS=9,           /* empty                            */
    CPROTO_REQ_FILE_DATABASE=10,        /* empty                            */

    /* Public Service API request */
    CPROTO_REQ_SERVICE=32,              /* (user, password, request, {...}) */
} cproto_client_t;

typedef enum
{
    /* success */
    CPROTO_RES_QUERY=0,                 /* {query response data}            */
    CPROTO_RES_INSERT=1,                /* {"success_msg": ...}             */
    CPROTO_RES_AUTH_SUCCESS=2,          /* empty                            */
    CPROTO_RES_ACK=3,                   /* empty                            */
    CPROTO_RES_FILE=5,                  /* file content                     */

    /* Service API success */
    CPROTO_ACK_SERVICE=32,                /* empty                          */
    CPROTO_ACK_SERVICE_DATA=33,           /* [...]                          */

    /* errors 64-69 are errors with messages */
    CPROTO_ERR_MSG=64,                  /* {"error_msg": ...}               */
    CPROTO_ERR_QUERY=65,                /* {"error_msg": ...}               */
    CPROTO_ERR_INSERT=66,               /* {"error_msg": ...}               */
    CPROTO_ERR_SERVER=67,               /* {"error_msg": ...}               */
    CPROTO_ERR_POOL=68,                 /* {"error_msg": ...}               */
    CPROTO_ERR_USER_ACCESS=69,          /* {"error_msg": ...}               */
    CPROTO_ERR=70,                      /* empty (use for unexpected errors)*/
    CPROTO_ERR_NOT_AUTHENTICATED=71,    /* empty                            */
    CPROTO_ERR_AUTH_CREDENTIALS=72,     /* empty                            */
    CPROTO_ERR_AUTH_UNKNOWN_DB=73,      /* empty                            */
    CPROTO_ERR_FILE=75,                 /* empty                            */

    /* Service API errors */
    CPROTO_ERR_SERVICE=96,                /* {"error_msg": ...}             */
    CPROTO_ERR_SERVICE_INVALID_REQUEST=97,/* empty                          */
    CPROTO_DEFERRED=127                 /* deferred...                      */
} cproto_server_t;

typedef enum
{
    BPROTO_AUTH_REQUEST=128,            /* (uuid, dbname, flags, version,
                                           min_version, dbpath, buffer_path,
                                           buffer_size, startup_time,
                                           address, port) */
    BPROTO_FLAGS_UPDATE,                /* flags                            */
    BPROTO_LOG_LEVEL_UPDATE,            /* log_level                        */
    BPROTO_REPL_FINISHED,               /* empty                            */
    BPROTO_QUERY_SERVER,                /* (query, time_precision)          */
    BPROTO_QUERY_UPDATE,                /* (query, time_precision)          */
    BPROTO_INSERT_POOL,                 /* {series: points, ...}            */
    BPROTO_INSERT_SERVER,               /* {series: points, ...}            */
    BPROTO_INSERT_TEST_POOL,            /* {series: points, ...}            */
    BPROTO_INSERT_TEST_SERVER,          /* {series: points, ...}            */
    BPROTO_INSERT_TESTED_POOL,          /* {series: points, ...}            */
    BPROTO_INSERT_TESTED_SERVER,        /* {series: points, ...}            */
    BPROTO_REGISTER_SERVER,             /* (uuid, host, port, pool)         */
    BPROTO_DROP_SERIES,                 /* series_name                      */
    BPROTO_REQ_GROUPS,                  /* empty                            */
    BPROTO_ENABLE_BACKUP_MODE,          /* empty                            */
    BPROTO_DISABLE_BACKUP_MODE,         /* empty                            */
} bproto_client_t;

/*
 * Success and error messages are set this way so that we have one range
 * for error messages between 64.. 191.
 *
 * Client Success messages in range 0..63
 * Client Error messages in range 64..126 (127 is used as deferred)
 * Back-end Error messages in range 128..191
 * Back-end Success messages in range 192..254 (exclude 255)
 */
typedef enum
{
    /* Mappings to client protocol messages */
    /* success */
    BPROTO_RES_QUERY=CPROTO_RES_QUERY,          /* {query response data}    */

    /* errors */
    BPROTO_ERR_QUERY=CPROTO_ERR_QUERY,          /* {"error_msg": ...}       */
    BPROTO_ERR_SERVER=CPROTO_ERR_SERVER,        /* {"error_msg": ...}       */
    BPROTO_ERR_POOL=CPROTO_ERR_POOL,            /* {"error_msg": ...}       */

    /* Back-end specific protocol messages */
    /* errors */
    BPROTO_AUTH_ERR_UNKNOWN_UUID=128,           /* empty                    */
    BPROTO_AUTH_ERR_UNKNOWN_DBNAME,             /* empty                    */
    BPROTO_AUTH_ERR_INVALID_UUID,               /* empty                    */
    BPROTO_AUTH_ERR_VERSION_TOO_OLD,            /* empty                    */
    BPROTO_AUTH_ERR_VERSION_TOO_NEW,            /* empty                    */
    BPROTO_ERR_NOT_AUTHENTICATED,               /* empty                    */
    BPROTO_ERR_INSERT,                          /* empty                    */
    BPROTO_ERR_REGISTER_SERVER,                 /* empty                    */
    BPROTO_ERR_DROP_SERIES,                     /* empty                    */
    BPROTO_ERR_ENABLE_BACKUP_MODE,              /* empty                    */
    BPROTO_ERR_DISABLE_BACKUP_MODE,             /* empty                    */

    /* success */
    BPROTO_AUTH_SUCCESS=192,                    /* empty                    */
    BPROTO_ACK_FLAGS,                           /* empty                    */
    BPROTO_ACK_LOG_LEVEL,                       /* empty                    */
    BPROTO_ACK_INSERT,                          /* empty                    */
    BPROTO_ACK_REPL_FINISHED,                   /* empty                    */
    BPROTO_ACK_REGISTER_SERVER,                 /* empty                    */
    BPROTO_ACK_DROP_SERIES,                     /* empty                    */
    BPROTO_ACK_ENABLE_BACKUP_MODE,              /* empty                    */
    BPROTO_ACK_DISABLE_BACKUP_MODE,             /* empty                    */
    BPROTO_RES_GROUPS                           /* [[name, series], ...]    */

} bproto_server_t;

#define sirinet_protocol_is_error(tp) (tp >= 64 && tp < 192)
#define sirinet_protocol_is_error_msg(tp) (tp >= 64 && tp < 70)

const char * sirinet_cproto_client_str(cproto_client_t n);
const char * sirinet_cproto_server_str(cproto_server_t n);
const char * sirinet_bproto_client_str(bproto_client_t n);
const char * sirinet_bproto_server_str(bproto_server_t n);

#endif  /* SIRINET_PROTOCOL_H_ */
