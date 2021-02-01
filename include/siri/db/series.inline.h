#include <siri/db/series.h>
#include <siri/db/shard.h>

static inline uint64_t siridb_series_duration(siridb_series_t * series)
{
    return series->idx_len ? series->idx->shard->duration : 0;
}
