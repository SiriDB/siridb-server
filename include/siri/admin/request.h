/*
 * request.h - SiriDB Administrative Request.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2017
 *
 */
#pragma once
#include <qpack/qpack.h>
#include <siri/net/protocol.h>

typedef enum
{
    ADMIN_NEW_ACCOUNT_,
    ADMIN_CHANGE_PASSWORD_,
    ADMIN_DROP_ACCOUNT_,
    ADMIN_NEW_DATABASE_,
    ADMIN_NEW_POOL,
    ADMIN_GET_VERSION=64,
    ADMIN_GET_ACCOUNTS,
    ADMIN_GET_DATABASES
} admin_request_t;

int siri_admin_request_init(void);
void siri_admin_request_destroy(void);
cproto_server_t siri_admin_request(
        int tp,
        qp_unpacker_t * qp_unpacker,
        qp_obj_t * qp_account,
        qp_packer_t ** packaddr,
        uint16_t pid,
        uv_stream_t * client,
        char * err_msg);
void siri_admin_request_rollback(const char * dbpath);
