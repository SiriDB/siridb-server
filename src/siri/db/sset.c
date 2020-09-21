/*
 * sset.c - Set operations on series.
 */

#include <assert.h>
#include <siri/db/sset.h>
#include <stdlib.h>
#include <siri/db/series.h>

siridb_sset_t * siridb_sset_new(imap_t * series_map, imap_update_cb update_cb)
{
    siridb_sset_t * sset = malloc(sizeof(siridb_sset_t));
    if (sset == NULL)
    {
        return NULL;
    }

    sset->series_map = series_map;
    sset->update_cb = update_cb;

    return sset;
}

void siridb_sset_free(siridb_sset_t * sset)
{
    if (sset == NULL)
    {
        return;
    }

    if (sset->series_map)
    {
        imap_free(sset->series_map, (imap_free_cb) &siridb__series_decref);
    }

    free(sset);
}
