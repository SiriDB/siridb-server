/*
 * points.h - Array object for points.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#pragma once
#include <stdlib.h>
#include <inttypes.h>
#include <qpack/qpack.h>
#include <slist/slist.h>

typedef enum
{
    TP_INT,
    TP_DOUBLE,
    TP_STRING
} points_tp;

typedef struct siridb_point_s
{
    uint64_t ts;
    qp_via_t val;
} siridb_point_t;

typedef struct siridb_points_s
{
    size_t len;
    points_tp tp;
    char * content;
    siridb_point_t * data;
} siridb_points_t;

siridb_points_t * siridb_points_new(size_t size, points_tp tp);
void siridb_points_free(siridb_points_t * points);
void siridb_points_add_point(
        siridb_points_t *__restrict points,
        uint64_t * ts,
        qp_via_t * val);
siridb_points_t * siridb_points_copy(siridb_points_t * points);
int siridb_points_pack(siridb_points_t * points, qp_packer_t * packer);
void siridb_points_ts_correction(siridb_points_t * points, double factor);
int siridb_points_raw_pack(siridb_points_t * points, qp_packer_t * packer);
siridb_points_t * siridb_points_merge(slist_t * plist, char * err_msg);
unsigned char * siridb_points_zip_double(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size);
unsigned char * siridb_points_zip_int(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size);
void siridb_points_unzip_int(
        siridb_points_t * points,
        unsigned char * bits,
        uint16_t len,
        uint16_t cinfo,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
void siridb_points_unzip_double(
        siridb_points_t * points,
        unsigned char * bits,
        uint16_t len,
        uint16_t cinfo,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
size_t siridb_points_get_size_zipped(uint16_t cinfo, uint16_t len);


#define siridb_points_zip(p__, s__, e__, c__, z__) \
((p__)->tp == TP_INT) ? siridb_points_zip_int(p__, s__, e__, c__, z__) :\
siridb_points_zip_double(p__, s__, e__, c__, z__)
