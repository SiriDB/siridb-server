/*
 * sset.h - Set operations on series.
 */
#ifndef SIRIDB_SSET_H_
#define SIRIDB_SSET_H_

typedef struct siridb_sset_s siridb_sset_t;

#include <imap/imap.h>

siridb_sset_t * siridb_sset_new(imap_t * series_map, imap_update_cb update_cb);
void siridb_sset_free(siridb_sset_t * sset);

struct siridb_sset_s
{
    imap_t * series_map;
    imap_update_cb update_cb;
};

#endif  /* SIRIDB_SSET_H_ */
