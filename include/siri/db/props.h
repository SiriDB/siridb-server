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
#ifndef SIRIDB_PROPS_H_
#define SIRIDB_PROPS_H_

#include <siri/grammar/gramp.h>
#include <siri/db/db.h>
#include <qpack/qpack.h>

void siridb_init_props(void);

typedef void (* siridb_props_cb)(
        siridb_t * siridb,
        qp_packer_t * packer,
        int flags);

siridb_props_cb siridb_props[KW_COUNT];

char * who_am_i;

#endif  /* SIRIDB_PROPS_H_ */
