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

typedef struct siri_admin_client_s
{
    uint8_t status;
    uint8_t flags;
    uint16_t pid;
    uint16_t port;
    char * host;
    char * username;
    char * password;
    char * dbname;
    char * dbpath;
    uv_stream_t * client;
} siri_admin_client_t;

int siri_admin_client_request(
        uint16_t pid,
        uint16_t port,
        qp_obj_t * host,
        qp_obj_t * username,
        qp_obj_t * password,
        qp_obj_t * dbname,
        const char * dbpath,
        uv_stream_t * client,
        char * err_msg);

void siri_admin_client_free(siri_admin_client_t * adm_client);
