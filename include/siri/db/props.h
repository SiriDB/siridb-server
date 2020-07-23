/*
 * props.h - Functions to return SiriDB properties.
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

siridb_props_cb props_get_cb(int i);
void props_set_who_am_i(char * s);
char * props_get_who_am_i(void);


#endif  /* SIRIDB_PROPS_H_ */
