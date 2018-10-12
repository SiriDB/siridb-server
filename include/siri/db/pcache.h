/*
 * pcache.h - Points structure with notion of its size.
 */
#ifndef SIRIDB_PCACHE_H_
#define SIRIDB_PCACHE_H_

typedef struct siridb_pcache_s siridb_pcache_t;

#include <siri/db/points.h>

siridb_pcache_t * siridb_pcache_new(points_tp tp);
int siridb_pcache_add_point(
        siridb_pcache_t * pcache,
        uint64_t * ts,
        qp_via_t * val);
int siridb_pcache_add_ts_obj(
        siridb_pcache_t * pcache,
        uint64_t * ts,
        qp_obj_t * obj);

#define siridb_pcache_free(pcache) \
    siridb_points_free((siridb_points_t *) pcache)

struct siridb_pcache_s
{
    size_t len;
    points_tp tp;
    siridb_point_t * data;
    size_t size;   /* addition to normal points type */
};
#endif  /* SIRIDB_PCACHE_H_ */
