/*
 * aggregate.h - SiriDB aggregation methods.
 */
#ifndef SIRIDB_AGGREGATE_H_
#define SIRIDB_AGGREGATE_H_

typedef struct siridb_aggr_s siridb_aggr_t;

#include <siri/db/points.h>
#include <siri/grammar/gramp.h>
#include <vec/vec.h>
#include <cexpr/cexpr.h>
#include <qpack/qpack.h>
#include <pcre2.h>

siridb_points_t * siridb_aggregate_run(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);
void siridb_init_aggregates(void);
vec_t * siridb_aggregate_list(cleri_children_t * children, char * err_msg);
void siridb_aggregate_list_free(vec_t * alist);
int siridb_aggregate_can_skip(cleri_children_t * children);

struct siridb_aggr_s
{
    uint32_t gid;
    cexpr_operator_t filter_opr;
    uint8_t filter_tp;
    uint64_t group_by;
    uint64_t limit;
    uint64_t offset;
    double timespan;  /* used for derivative        */
    pcre2_code * regex;             \
    pcre2_match_data * match_data;
    qp_via_t filter_via;
};

#endif  /* SIRIDB_AGGREGATE_H_ */
