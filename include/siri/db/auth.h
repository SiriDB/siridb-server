/*
 * auth.h - Handle SiriDB authentication.
 */
#ifndef SIRIDB_AUTH_H_
#define SIRIDB_AUTH_H_

#include <stddef.h>
#include <siri/net/clserver.h>
#include <qpack/qpack.h>
#include <siri/net/protocol.h>

cproto_server_t siridb_auth_user_request(
        sirinet_stream_t * client,
        qp_obj_t * qp_username,
        qp_obj_t * qp_password,
        qp_obj_t * qp_dbname);

bproto_server_t siridb_auth_server_request(
        sirinet_stream_t * client,
        qp_obj_t * qp_uuid,
        qp_obj_t * qp_dbname,
        qp_obj_t * qp_version,
        qp_obj_t * qp_min_version);

#endif  /* SIRIDB_AUTH_H_ */
