/*
 * props.h - Functions to return SiriDB properties.
 */
#ifndef SIRIDB_PROPS_H_
#define SIRIDB_PROPS_H_

#include <siri/grammar/gramp.h>
#include <siri/db/db.h>
#include <qpack/qpack.h>

void siridb_init_props(void);

static char * WHO_AM_I = NULL;

typedef void (* siridb_props_cb)(
        siridb_t * siridb,
        qp_packer_t * packer,
        int flags);

static siridb_props_cb SIRIDB_PROPS[KW_COUNT];

static inline void props_set_who_am_i(char * s)
{
    WHO_AM_I = s;
}

static inline char * props_get_who_am_i(void)
{
    return WHO_AM_I;
}

static inline siridb_props_cb props_get_cb(int i)
{
    return SIRIDB_PROPS[i];
}

#endif  /* SIRIDB_PROPS_H_ */
