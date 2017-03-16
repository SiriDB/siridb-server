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

int siri_admin_request(int tp, qp_unpacker_t * qp_unpacker);
