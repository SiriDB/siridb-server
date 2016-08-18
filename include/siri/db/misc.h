/*
 * misc.h - Miscellaneous functions used by SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-08-2016
 *
 */
#pragma once

#include <qpack/qpack.h>
#include <inttypes.h>

qp_unpacker_t * siridb_misc_open_schema_file(uint8_t schema, const char * fn);
