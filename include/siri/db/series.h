/*
 * series.h - Series
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 29-03-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <siri/db/db.h>
#include <qpack/qpack.h>

struct siridb_s;

#define SIRIDB_SERIES_TP_INT 0
#define SIRIDB_SERIES_TP_DOUBLE 1
#define SIRIDB_SERIES_TP_STRING 2

typedef struct siridb_series_s
{
    uint32_t id;
    uint8_t tp;
} siridb_series_t;

siridb_series_t * siridb_new_series(uint32_t id, uint8_t tp);
uint8_t siridb_qp_map_tp(qp_types_t tp);

void siridb_free_series(siridb_series_t * series);

int siridb_load_series(struct siridb_s * siridb);
