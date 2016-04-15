/*
 * props.h - Functions to return SiriDB properties.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 17-03-2016
 *
 */
#pragma once

#include <siri/grammar/gramp.h>
#include <siri/db/db.h>
#include <qpack/qpack.h>

struct siridb_s;
struct qp_packer_s;


void siridb_init_props(void);

typedef void (* siridb_props_cb)(
        struct siridb_s * siridb,
        struct qp_packer_s * packer,
        int flags);

siridb_props_cb siridb_props[KW_COUNT];

char * who_am_i;
