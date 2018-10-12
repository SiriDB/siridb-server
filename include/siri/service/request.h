/*
 * request.h - SiriDB Service Request.
 */
#ifndef SIRI_SERVICE_REQUEST_H_
#define SIRI_SERVICE_REQUEST_H_

typedef enum
{
    SERVICE_NEW_ACCOUNT_,
    SERVICE_CHANGE_PASSWORD_,
    SERVICE_DROP_ACCOUNT_,
    SERVICE_NEW_DATABASE_,
    SERVICE_NEW_POOL,
    SERVICE_NEW_REPLICA,
    SERVICE_GET_VERSION=64,
    SERVICE_GET_ACCOUNTS,
    SERVICE_GET_DATABASES
} service_request_t;

#include <qpack/qpack.h>
#include <siri/net/protocol.h>

int siri_service_request_init(void);
void siri_service_request_destroy(void);
cproto_server_t siri_service_request(
        int tp,
        qp_unpacker_t * qp_unpacker,
        qp_obj_t * qp_account,
        qp_packer_t ** packaddr,
        uint16_t pid,
        sirinet_stream_t * client,
        char * err_msg);
void siri_service_request_rollback(const char * dbpath);


#endif  /* SIRI_SERVICE_REQUEST_H_ */
