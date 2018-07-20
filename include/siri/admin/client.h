/*
 * client.h - Client for expanding a siridb database.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 24-03-2017
 *
 */
#pragma once

#include <inttypes.h>
#include <uv.h>
#include <qpack/qpack.h>
#include <siri/net/pkg.h>
#include <uuid/uuid.h>

typedef struct siri_admin_client_s
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
    uv_stream_t * client;
    sirinet_pkg_t * pkg;
} siri_admin_client_t;

int siri_admin_client_request(
        uint16_t pid,
        uint16_t port,
        int pool,
        uuid_t * uuid,
        qp_obj_t * host,
        qp_obj_t * username,
        qp_obj_t * password,
        qp_obj_t * dbname,
        const char * dbpath,
        uv_stream_t * client,
        char * err_msg);

void siri_admin_client_free(siri_admin_client_t * adm_client);
