/*
 * client.h - Client for expanding a SiriDB database.
 */
#ifndef SIRI_SERVICE_CLIENT_H_
#define SIRI_SERVICE_CLIENT_H_

typedef struct siri_service_client_s siri_service_client_t;

#include <inttypes.h>
#include <uv.h>
#include <qpack/qpack.h>
#include <uuid/uuid.h>
#include <siri/net/pkg.h>

int siri_service_client_request(
        uint16_t pid,
        uint16_t port,
        int pool,
        uuid_t * uuid,
        qp_obj_t * host,
        qp_obj_t * username,
        qp_obj_t * password,
        qp_obj_t * dbname,
        const char * dbpath,
        sirinet_stream_t * client,
        char * err_msg);

void siri_service_client_free(siri_service_client_t * adm_client);

struct siri_service_client_s
{
    uint8_t request;
    uint8_t flags;
    uint16_t pid;
    uint16_t port;
    uuid_t uuid;
    int pool;  /*       -1 for a new pool       */
    char * host;
    char * username;
    char * password;
    char * dbname;
    char * dbpath;
    sirinet_stream_t * client;
    sirinet_pkg_t * pkg;
};

#endif  /* SIRI_SERVICE_CLIENT_H_ */
