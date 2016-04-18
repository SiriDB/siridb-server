/*
 * insert.h - Handler database inserts.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 24-03-2016
 *
 */
#pragma once

#include <siri/db/db.h>
#include <qpack/qpack.h>
#include <uv.h>


#define SIRIDB_INSERT_ERR_SIZE 8

typedef enum
{
    ERR_EXPECTING_ARRAY=-SIRIDB_INSERT_ERR_SIZE,
    ERR_EXPECTING_SERIES_NAME,
    ERR_EXPECTING_MAP_OR_ARRAY,
    ERR_EXPECTING_INTEGER_TS,
    ERR_TIMESTAMP_OUT_OF_RANGE,
    ERR_UNSUPPORTED_VALUE,
    ERR_EXPECTING_AT_LEAST_ONE_POINT,
    ERR_EXPECTING_NAME_AND_POINTS

} siridb_insert_err_t;

struct siridb_s;
struct qp_unpacker_s;
struct qp_packer_s;
struct qp_obj_s;

typedef struct siridb_insert_s
{
    uint64_t pid;
    uv_handle_t * client;
    size_t size;        /* number of points */
    uint16_t packer_size; /* number of packers (one for each pool) */
    struct qp_packer_s * packer[];
} siridb_insert_t;

int32_t siridb_insert_assign_pools(
        struct siridb_s * siridb,
        struct qp_unpacker_s * unpacker,
        struct qp_obj_s * qp_obj,
        struct qp_packer_s * packer[]);

const char * siridb_insert_err_msg(siridb_insert_err_t err);

void siridb_insert_points(
        uint64_t pid,
        uv_handle_t * client,
        size_t size,
        uint16_t packer_size,
        struct qp_packer_s * packer[]);
