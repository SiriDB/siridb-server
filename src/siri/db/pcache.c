/*
 * pcache.h - Points structure with notion or its size
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-10-2016
 *
 */

#include <assert.h>
#include <siri/db/pcache.h>
#include <siri/err.h>
#include <stddef.h>

#define PCACHE_DEFAULT_SIZE 64

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * A pcache object can be parsed to all points functions.
 */
siridb_pcache_t * siridb_pcache_new(points_tp tp)
{
    siridb_pcache_t * pcache =
            (siridb_pcache_t *) malloc(sizeof(siridb_pcache_t));
    if (pcache == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        pcache->size = PCACHE_DEFAULT_SIZE;
        pcache->len = 0;
        pcache->tp = tp;
        pcache->content = NULL;
        pcache->str_sz = 0;
        pcache->data = (siridb_point_t *) malloc(
                sizeof(siridb_point_t) * PCACHE_DEFAULT_SIZE);

        if (pcache->data == NULL)
        {
            ERR_ALLOC
            free(pcache);
            pcache = NULL;
        }
    }
    return pcache;
}

/*
 * Add a point to points. (points are sorted by timestamp so the new point
 * will be inserted at the correct position.
 *
 * Returns 0 if successful or -1 and a signal is raised in case of an error.
 */
int siridb_pcache_add_point(
        siridb_pcache_t * pcache,
        uint64_t * ts,
        qp_via_t * val)
{
    if (pcache->len == pcache->size)
    {
        pcache->size *= 2;
        siridb_point_t * tmp = (siridb_point_t *) realloc(
                        pcache->data,
                        sizeof(siridb_point_t) * pcache->size);
        if (tmp == NULL)
        {
            log_error(
                    "Cannot re-allocate memory for %lu points",
                    pcache->size);

            ERR_ALLOC
            /* restore original size */
            pcache->size /= 2;
            return -1;
        }
        pcache->data = tmp;
    }

    siridb_points_add_point((siridb_points_t *) pcache, ts, val);

    return 0;
}
