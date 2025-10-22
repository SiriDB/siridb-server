/*
 * aggregate.c - SiriDB aggregation methods.
 */
#include <assert.h>
#include <limits.h>
#include <logger/logger.h>
#include <siri/db/aggregate.h>
#include <siri/db/median.h>
#include <siri/db/variance.h>
#include <siri/grammar/grammar.h>
#include <siri/grammar/gramp.h>
#include <siri/db/re.h>
#include <vec/vec.h>
#include <stddef.h>
#include <xstr/xstr.h>
#include <math.h>

#define AGGR_NEW                                    \
if ((aggr = AGGREGATE_new(gid)) == NULL)            \
{                                                   \
    sprintf(err_msg, "Memory allocation error.");   \
    siridb_aggregate_list_free(vec);              \
    return NULL;                                    \
}

#define VEC_APPEND                                \
if (vec_append_safe(&vec, aggr))                \
{                                                   \
    sprintf(err_msg, "Memory allocation error.");   \
    AGGREGATE_free(aggr);                           \
    siridb_aggregate_list_free(vec);              \
    return NULL;                                    \
}

typedef int (* AGGR_cb)(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

#define GROUP_TS(point) \
    (point->ts + aggr->group_by - 1) / aggr->group_by * aggr->group_by + \
    aggr->offset

static AGGR_cb AGGREGATES[F_OFFSET];

static siridb_aggr_t * AGGREGATE_new(uint32_t gid);
static int AGGREGATE_regex_cmp(siridb_aggr_t * aggr, char * val);
static void AGGREGATE_free(siridb_aggr_t * aggr);
static int AGGREGATE_init_filter(
        siridb_aggr_t * aggr,
        cleri_node_t * node,
        char * err_msg);
static siridb_points_t * AGGREGATE_limit(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);
static siridb_points_t * AGGREGATE_derivative(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);
static siridb_points_t * AGGREGATE_difference(
        siridb_points_t * source,
        char * err_msg);
static siridb_points_t * AGGREGATE_timeval(
        siridb_points_t * source,
        char * err_msg);
static siridb_points_t * AGGREGATE_interval(
        siridb_points_t * source,
        char * err_msg);
static siridb_points_t * AGGREGATE_filter(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);
static siridb_points_t * AGGREGATE_to_one(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);
static siridb_points_t * AGGREGATE_group_by(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_count(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_derivative(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_difference(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_max(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_mean(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_median(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_median_high(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_median_low(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_min(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_pvariance(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_sum(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_variance(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_stddev(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_first(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

static int aggr_last(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

/*
 * Initialize aggregates.
 */
void siridb_init_aggregates(void)
{
    uint_fast16_t i;
    for (i = 0; i < F_OFFSET; i++)
    {
        AGGREGATES[i] = NULL;
    }

    /* filter is not included since we only use these for group_by functions */
    AGGREGATES[CLERI_GID_F_COUNT - F_OFFSET] = aggr_count;
    AGGREGATES[CLERI_GID_F_DERIVATIVE - F_OFFSET] = aggr_derivative;
    AGGREGATES[CLERI_GID_F_DIFFERENCE - F_OFFSET] = aggr_difference;
    AGGREGATES[CLERI_GID_F_MAX - F_OFFSET] = aggr_max;
    AGGREGATES[CLERI_GID_F_MEAN - F_OFFSET] = aggr_mean;
    AGGREGATES[CLERI_GID_F_MEDIAN - F_OFFSET] = aggr_median;
    AGGREGATES[CLERI_GID_F_MEDIAN_HIGH - F_OFFSET] = aggr_median_high;
    AGGREGATES[CLERI_GID_F_MEDIAN_LOW - F_OFFSET] = aggr_median_low;
    AGGREGATES[CLERI_GID_F_MIN - F_OFFSET] = aggr_min;
    AGGREGATES[CLERI_GID_F_PVARIANCE - F_OFFSET] = aggr_pvariance;
    AGGREGATES[CLERI_GID_F_SUM - F_OFFSET] = aggr_sum;
    AGGREGATES[CLERI_GID_F_VARIANCE - F_OFFSET] = aggr_variance;
    AGGREGATES[CLERI_GID_F_STDDEV - F_OFFSET] = aggr_stddev;
    AGGREGATES[CLERI_GID_F_FIRST - F_OFFSET] = aggr_first;
    AGGREGATES[CLERI_GID_F_LAST - F_OFFSET] = aggr_last;
}

/*
 * Returns NULL in case an error has occurred and the err_msg will be set.
 */
vec_t * siridb_aggregate_list(cleri_children_t * children, char * err_msg)
{
    uint32_t gid;
    siridb_aggr_t * aggr = NULL;
    vec_t * vec = vec_new(VEC_DEFAULT_SIZE);
    if (vec == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
        return NULL;
    }

    /* Loop over all aggregations */
    while (1)
    {
        gid = cleri_gn(cleri_gn(children)->children)->cl_obj->gid;

        switch (gid)
        {
        case CLERI_GID_F_OFFSET:
            if (aggr == NULL || aggr->group_by == 0)
            {
                sprintf(err_msg,
                        "Offset must be used after an aggregation method.");
                siridb_aggregate_list_free(vec);
                return NULL;
            }
                /* group_by is always > 0 */
            aggr->offset = CLERI_NODE_DATA(
                    cleri_gn(cleri_gn(cleri_gn(children)
                    ->children)->children->next->next)) % aggr->group_by;
            break;
        case CLERI_GID_F_LIMIT:
            AGGR_NEW
            {
                int64_t limit = CLERI_NODE_DATA(
                        cleri_gn(cleri_gn(cleri_gn(children)
                        ->children)->children->next->next));

                if (limit <= 0)
                {
                    sprintf(err_msg,
                            "Limit must be an integer value "
                            "larger than zero.");
                    AGGREGATE_free(aggr);
                    siridb_aggregate_list_free(vec);
                    return NULL;
                }

                aggr->limit = limit;

                gid = cleri_gn(cleri_gn(cleri_gn(
                        cleri_gn(children)->children)->children->next->
                        next->next->next)->children)->cl_obj->gid;

                switch (gid)
                {
                case CLERI_GID_K_MEAN:
                    aggr->gid = CLERI_GID_F_MEAN;
                    break;

                case CLERI_GID_K_MEDIAN:
                    aggr->gid = CLERI_GID_F_MEDIAN;
                    break;

                case CLERI_GID_K_MEDIAN_LOW:
                    aggr->gid = CLERI_GID_F_MEDIAN_LOW;
                    break;

                case CLERI_GID_K_MEDIAN_HIGH:
                    aggr->gid = CLERI_GID_F_MEDIAN_HIGH;
                    break;

                case CLERI_GID_K_SUM:
                    aggr->gid = CLERI_GID_F_SUM;
                    break;

                case CLERI_GID_K_MIN:
                    aggr->gid = CLERI_GID_F_MIN;
                    break;

                case CLERI_GID_K_MAX:
                    aggr->gid = CLERI_GID_F_MAX;
                    break;

                case CLERI_GID_K_COUNT:
                    aggr->gid = CLERI_GID_F_COUNT;
                    break;

                case CLERI_GID_K_VARIANCE:
                    aggr->gid = CLERI_GID_F_VARIANCE;
                    break;

                case CLERI_GID_K_PVARIANCE:
                    aggr->gid = CLERI_GID_F_PVARIANCE;
                    break;

                case CLERI_GID_K_STDDEV:
                    aggr->gid = CLERI_GID_F_STDDEV;
                    break;

                case CLERI_GID_K_FIRST:
                    aggr->gid = CLERI_GID_F_FIRST;
                    break;

                case CLERI_GID_K_LAST:
                    aggr->gid = CLERI_GID_F_LAST;
                    break;

                case CLERI_GID_K_TIMEVAL:
                    aggr->gid = CLERI_GID_F_TIMEVAL;
                    break;

                case CLERI_GID_K_INTERVAL:
                    aggr->gid = CLERI_GID_F_INTERVAL;
                    break;

                default:
                    assert (0);
                    break;
                }
            }

            VEC_APPEND

            break;

        case CLERI_GID_F_TIMEVAL:
        case CLERI_GID_F_INTERVAL:
            AGGR_NEW
            VEC_APPEND

            break;
        case CLERI_GID_F_FILTER:
            AGGR_NEW
            {
                cleri_node_t * onode;
                if (    cleri_gn(cleri_gn(children)->children)->children->
                        next->next->next->next == NULL)
                {
                    aggr->filter_opr = CEXPR_EQ;
                    onode = cleri_gn(cleri_gn(cleri_gn(cleri_gn(children)
                            ->children)->children->next->next)->children);
                }
                else
                {
                    aggr->filter_opr = cexpr_operator_fn(
                            cleri_gn(cleri_gn(cleri_gn(cleri_gn(children)
                            ->children)->children->next->next)->children));
                    onode = cleri_gn(cleri_gn(cleri_gn(cleri_gn(children)
                            ->children)->children->next->next->next)->
                            children);
                }
                if (AGGREGATE_init_filter(aggr, onode, err_msg))
                {
                    AGGREGATE_free(aggr);
                    siridb_aggregate_list_free(vec);
                    return NULL;  /* err_msg is set */
                }
            }

            VEC_APPEND

            break;

        case CLERI_GID_F_DERIVATIVE:
            AGGR_NEW
            {
                cleri_node_t * dlist = cleri_gn(cleri_gn(cleri_gn(children)
                        ->children)->children->next->next);

                if (dlist->children != NULL && cleri_gn(dlist->children) != NULL)
                {
                    /* result is at least positive, checked earlier */
                    aggr->timespan =
                            (double) CLERI_NODE_DATA(cleri_gn(dlist->children));

                    if (!aggr->timespan)
                    {
                        sprintf(err_msg,
                                "Time-span must be an integer value "
                                "larger than zero.");
                        AGGREGATE_free(aggr);
                        siridb_aggregate_list_free(vec);
                        return NULL;
                    }

                    if (dlist->children->next != NULL)
                    {
                        /* result is always positive */
                        aggr->group_by = CLERI_NODE_DATA(
                                cleri_gn(dlist->children->next->next));

                        if (!aggr->group_by)
                        {
                            sprintf(err_msg,
                                    "Group by time must be an integer "
                                    "value larger than zero.");
                            AGGREGATE_free(aggr);
                            siridb_aggregate_list_free(vec);
                            return NULL;
                        }

                        aggr->timespan /= (double) aggr->group_by;
                    }
                }
            }

            VEC_APPEND

            break;

        case CLERI_GID_F_DIFFERENCE:
        case CLERI_GID_F_COUNT:
        case CLERI_GID_F_MAX:
        case CLERI_GID_F_MEAN:
        case CLERI_GID_F_MEDIAN:
        case CLERI_GID_F_MEDIAN_HIGH:
        case CLERI_GID_F_MEDIAN_LOW:
        case CLERI_GID_F_MIN:
        case CLERI_GID_F_PVARIANCE:
        case CLERI_GID_F_SUM:
        case CLERI_GID_F_VARIANCE:
        case CLERI_GID_F_STDDEV:
        case CLERI_GID_F_FIRST:
        case CLERI_GID_F_LAST:
            AGGR_NEW
            if (cleri_gn(cleri_gn(children)->children)
                    ->children->next->next->next != NULL)
            {
                /* result is always positive, checked earlier */
                aggr->group_by = CLERI_NODE_DATA(
                        cleri_gn(cleri_gn(cleri_gn(cleri_gn(children)
                        ->children)->children->next->next)->children));

                if (!aggr->group_by)
                {
                    sprintf(err_msg,
                            "Group by time must be an integer value "
                            "larger than zero.");
                    AGGREGATE_free(aggr);
                    siridb_aggregate_list_free(vec);
                    return NULL;
                }
            }

            VEC_APPEND

            break;

        case CLERI_GID_F_ALL:
            break;

        case CLERI_GID_F_POINTS:
            log_warning("Keyword 'points' is deprecated, "
                        "use '*' or 'all' instead.");
            break;

        default:
            assert (0);
            break;
        }

        if (children->next == NULL)
        {
            break;
        }

        children = children->next->next;
    }

    return vec;
}

/*
 * Destroy aggregates list. (parsing NULL is not allowed)
 */
void siridb_aggregate_list_free(vec_t * alist)
{
    size_t i;
    for (i = 0; i < alist->len; i++)
    {
        AGGREGATE_free(alist->data[i]);
    }
    free(alist);
}

/*
 * Returns 1 (true) if at least one aggregation requires all points to be queried.
 */
int siridb_aggregate_can_skip(cleri_children_t * children)
{
    cleri_node_t * nd = cleri_gn(cleri_gn(cleri_gn(cleri_gn(children)
            ->children)->children)->children);

    switch (nd->cl_obj->gid)
    {
    case CLERI_GID_F_COUNT:
    case CLERI_GID_F_FIRST:
    case CLERI_GID_F_LAST:
        return nd->children->next->next->next == NULL;

    default:
        return 0;
    }
}

/*
 * Return a new allocated points object or the same object as source.
 * In case of an error NULL is returned and an error message is set.
 */
siridb_points_t * siridb_aggregate_run(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    assert (source->len);
    if (aggr->limit)
    {
        return AGGREGATE_limit(source, aggr, err_msg);
    }

    if (aggr->group_by)
    {
        return AGGREGATE_group_by(source, aggr, err_msg);
    }

    switch (aggr->gid)
    {
    case CLERI_GID_F_DIFFERENCE:
        return AGGREGATE_difference(source, err_msg);

    case CLERI_GID_F_DERIVATIVE:
        return AGGREGATE_derivative(source, aggr, err_msg);

    case CLERI_GID_F_FILTER:
        return AGGREGATE_filter(source, aggr, err_msg);

    case CLERI_GID_F_INTERVAL:
        return AGGREGATE_interval(source, err_msg);

    case CLERI_GID_F_TIMEVAL:
        return AGGREGATE_timeval(source, err_msg);

    default:
        return AGGREGATE_to_one(source, aggr, err_msg);
    }

    return NULL;
}

/*
 * Returns NULL in case an error has occurred.
 */
static siridb_aggr_t * AGGREGATE_new(uint32_t gid)
{
    siridb_aggr_t * aggr = malloc(sizeof(siridb_aggr_t));
    if (aggr == NULL)
    {
        return NULL;
    }
    aggr->gid = gid;
    aggr->group_by = 0;
    aggr->limit = 0;
    aggr->offset = 0;
    aggr->timespan = 1.0;
    aggr->regex = NULL;
    aggr->match_data = NULL;
    aggr->filter_via.raw = NULL;
    aggr->filter_tp = TP_INT;  /* when string we must cleanup more */
    return aggr;
}

/*
 * Destroy aggregate object. (parsing NULL is not allowed)
 */
static void AGGREGATE_free(siridb_aggr_t * aggr)
{
    if (aggr->filter_tp == TP_STRING)
    {
        free(aggr->filter_via.raw);
        pcre2_code_free(aggr->regex);
        pcre2_match_data_free(aggr->match_data);
    }
    free(aggr);
}

/*
 * Returns 0 if successful or -1 in case or an error.
 * The err_msg is set for any error.
 */
static int AGGREGATE_init_filter(
        siridb_aggr_t * aggr,
        cleri_node_t * node,
        char * err_msg)
{
    switch (node->cl_obj->gid)
    {
    case CLERI_GID_K_NAN:
        aggr->filter_tp = TP_DOUBLE;
        aggr->filter_via.real = NAN;
        break;

    case CLERI_GID_K_INF:
        aggr->filter_tp = TP_DOUBLE;
        aggr->filter_via.real = INFINITY;
        break;

    case CLERI_GID_K_NINF:
        aggr->filter_tp = TP_DOUBLE;
        aggr->filter_via.real = -INFINITY;
        break;

    case CLERI_GID_R_INTEGER:
        aggr->filter_tp = TP_INT;
        aggr->filter_via.int64 = atoll(node->str);
        break;

    case CLERI_GID_R_FLOAT:
        aggr->filter_tp = TP_DOUBLE;
        aggr->filter_via.real = xstr_to_double(node->str);
        break;

    case CLERI_GID_STRING:
        aggr->filter_tp = TP_STRING;
        aggr->filter_via.raw = malloc(node->len - 1);
        if (aggr->filter_via.raw == NULL)
        {
            sprintf(err_msg, "Memory allocation error.");
            return -1;
        }
        xstr_extract_string(
                (char *) aggr->filter_via.raw, node->str, node->len);
        return 0;

    case CLERI_GID_R_REGEX:
        if (aggr->filter_opr != CEXPR_EQ && aggr->filter_opr != CEXPR_NE)
        {
            sprintf(err_msg,
                    "Regular expressions can only be used with 'equal' (==) "
                    "or 'not equal' (!=) operator.");
            return -1;
        }
        aggr->filter_tp = TP_STRING;
        /* extract and compile regular expression */
        if (siridb_re_compile(
                &aggr->regex,
                &aggr->match_data,
                node->str,
                node->len,
                err_msg))
        {
            return -1;  /* error_msg is set */
        }
        return 0;

    default:
        assert (0);
        break;
    }

    if (aggr->filter_opr == CEXPR_IN || aggr->filter_opr == CEXPR_NI)
    {
        sprintf(err_msg,
                "Operator '%s' can only be used with strings.",
                (aggr->filter_opr == CEXPR_IN) ? "~" : "!~");
        return -1;
    }

    return 0;
}

static siridb_points_t * AGGREGATE_limit(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    if (source->len <= aggr->limit)
    {
        return source;
    }

    uint64_t timespan =
            source->data[source->len - 1].ts - source->data[0].ts;

    aggr->group_by = timespan / aggr->limit + 1;
    aggr->offset = (source->data[0].ts - 1) % aggr->group_by;

    return AGGREGATE_group_by(source, aggr, err_msg);
}

static siridb_points_t * AGGREGATE_derivative(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    if (source->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use derivative() on string type.");
        return NULL;
    }

    size_t len = source->len - 1;
    siridb_points_t * points = siridb_points_new(len, TP_DOUBLE);

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        points->len = len;

        if (len)
        {
            siridb_point_t * prev = source->data;
            siridb_point_t * spt;
            siridb_point_t * dpt;
            size_t i;

            switch (source->tp)
            {
            case TP_INT:
                for (   i = 0, spt= prev + 1, dpt = points->data;
                        i < len;
                        i++, spt++, dpt++)
                {
                    dpt->ts = spt->ts;
                    dpt->val.real = ((double) spt->val.int64 - prev->val.int64)
                            / (double) (spt->ts - prev->ts) * aggr->timespan;
                    prev = spt;
                }
                break;

            case TP_DOUBLE:
                for (   i = 0, spt= prev + 1, dpt = points->data;
                        i < len;
                        i++, spt++, dpt++)
                {
                    dpt->ts = spt->ts;
                    dpt->val.real = (spt->val.real - prev->val.real)
                            / (double) (spt->ts - prev->ts) * aggr->timespan;
                    prev = spt;
                }
                break;

            default:
                assert (0);
                break;
            }
        }
    }
    return points;
}

static siridb_points_t * AGGREGATE_difference(
        siridb_points_t * source,
        char * err_msg)
{
    if (source->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use difference() on string type.");
        return NULL;
    }

    size_t len = source->len - 1;
    siridb_points_t * points = siridb_points_new(len, source->tp);

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        points->len = len;

        if (len)
        {
            siridb_point_t * prev = source->data;
            siridb_point_t * spt;
            siridb_point_t * dpt;
            size_t i;

            switch (source->tp)
            {
            case TP_INT:
                {
                    int64_t first = prev->val.int64;
                    int64_t last;
                    for (   i = 0, spt= prev + 1, dpt = points->data;
                            i < len;
                            i++, spt++, dpt++)
                    {
                        last = spt->val.int64;

                        if ((first > 0 && last < LLONG_MIN + first) ||
                                (first < 0 && last > LLONG_MAX + first))
                        {
                            sprintf(err_msg,
                                "Overflow detected while using difference().");
                            siridb_points_free(points);
                            return NULL;
                        }

                        dpt->ts = spt->ts;
                        dpt->val.int64 = last - first;

                        first = last;
                    }

                }
                break;

            case TP_DOUBLE:
                for (   i = 0, spt= prev + 1, dpt = points->data;
                        i < len;
                        i++, spt++, dpt++)
                {
                    dpt->ts = spt->ts;
                    dpt->val.real = spt->val.real - prev->val.real;
                    prev = spt;
                }
                break;

            default:
                assert (0);
                break;
            }
        }
    }
    return points;
}

static siridb_points_t * AGGREGATE_interval(
        siridb_points_t * source,
        char * err_msg)
{
    size_t len = source->len - 1;
    siridb_points_t * points = siridb_points_new(len, TP_INT);

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        points->len = len;

        if (len)
        {
            siridb_point_t * prev = source->data;
            siridb_point_t * curr = prev + 1;
            siridb_point_t * end = source->data + source->len;
            siridb_point_t * dest = points->data;

            for (; curr < end; ++prev, ++curr, ++dest)
            {
                uint64_t diff = curr->ts - prev->ts;
                if (diff > LLONG_MAX)
                {
                    sprintf(err_msg,
                        "Overflow detected while using difference().");
                    siridb_points_free(points);
                    return NULL;
                }
                dest->ts = curr->ts;
                dest->val.int64 = (int64_t) diff;
            }
        }
    }
    return points;
}

static siridb_points_t * AGGREGATE_timeval(
        siridb_points_t * source,
        char * err_msg)
{
    siridb_points_t * points = siridb_points_new(source->len, TP_INT);

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        siridb_point_t * curr = source->data;
        siridb_point_t * end = source->data + source->len;
        siridb_point_t * dest = points->data;

        points->len = source->len;

        for (; curr < end; ++curr, ++dest)
        {
            if (curr->ts > LLONG_MAX)
            {
                sprintf(err_msg,
                    "Overflow detected while using difference().");
                siridb_points_free(points);
                return NULL;
            }
            dest->ts = curr->ts;
            dest->val.int64 = (int64_t) curr->ts;
        }
    }
    return points;
}

static int AGGREGATE_regex_cmp(siridb_aggr_t * aggr, char * val)
{
    int ret;
    ret = pcre2_match(
            aggr->regex,
            (PCRE2_SPTR8) val,
            strlen(val),
            0,                     /* start looking at this point   */
            0,                     /* OPTIONS                       */
            aggr->match_data,
            0);                    /* length of sub_str_vec         */
    return aggr->filter_opr == CEXPR_EQ ? ret >= 0 : ret < 0;
}

static siridb_points_t * AGGREGATE_filter(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    qp_via_t value = aggr->filter_via;

    if (source->tp != aggr->filter_tp)
    {
        if (source->tp == TP_STRING)
        {
            sprintf(err_msg, "Cannot use a number filter on string type.");
            return NULL;
        }

        switch (aggr->filter_tp)
        {
        case TP_STRING:
            sprintf(err_msg, "Cannot use a string filter on number type.");
            return NULL;

        case TP_INT:
            value.real = (double) value.int64;
            break;

        case TP_DOUBLE:
            value.int64 = (int64_t) value.real;
            break;

        default:
            assert (0);
            break;
        }
    }

    siridb_points_t * points = siridb_points_new(source->len, source->tp);

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        size_t i;
        siridb_point_t * spt;
        siridb_point_t * dpt;
        switch ((points_tp) source->tp)
        {
        case TP_STRING:
            for (   i = 0, spt = source->data, dpt = points->data;
                    i < source->len;
                    i++, spt++)
            {
                if (value.str != NULL  /* NULL is a regular expression  */
                        ? cexpr_str_cmp(
                                aggr->filter_opr,
                                spt->val.str, value.str)
                        : AGGREGATE_regex_cmp(aggr, spt->val.str))
                {
                    dpt->ts = spt->ts;
                    dpt->val.str = strdup(spt->val.str);
                    if (dpt->val.str == NULL)
                    {
                        sprintf(err_msg, "Memory allocation error.");
                        siridb_points_free(points);
                        return NULL;
                    }
                    dpt++;
                }
            }
            break;

        case TP_INT:
            for (   i = 0, spt = source->data, dpt = points->data;
                    i < source->len;
                    i++, spt++)
            {
                if (cexpr_int_cmp(
                        aggr->filter_opr,
                        spt->val.int64,
                        value.int64))
                {
                    dpt->ts = spt->ts;
                    dpt->val = spt->val;
                    dpt++;
                }
            }
            break;

        case TP_DOUBLE:
            for (   i = 0, spt = source->data, dpt = points->data;
                    i < source->len;
                    i++, spt++)
            {
                if (cexpr_double_cmp(
                        aggr->filter_opr,
                        spt->val.real,
                        value.real))
                {
                    dpt->ts = spt->ts;
                    dpt->val = spt->val;
                    dpt++;
                }
            }
            break;

        default:
            assert (0);
            dpt = NULL;
            break;
        }

        points->len = dpt - points->data;

        if (source->len > points->len)
        {
            if (points->len == 0)
            {
                free(points->data);
                points->data = NULL;
            }
            else
            {
                dpt = (siridb_point_t *) realloc(
                        points->data,
                        points->len * sizeof(siridb_point_t));
                if (dpt == NULL)
                {
                    /* not critical */
                    log_error("Error while re-allocating memory for points");
                }
                else
                {
                    points->data = dpt;
                }
            }
        }
    }

    return points;
}

static siridb_points_t * AGGREGATE_to_one(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    siridb_points_t * points;
    /* get correct callback function */
    AGGR_cb aggr_cb = AGGREGATES[aggr->gid - F_OFFSET];

    /* create new points with max possible size after re-indexing */
    switch(aggr->gid)
    {
    case CLERI_GID_F_MEAN:
    case CLERI_GID_F_MEDIAN:
    case CLERI_GID_F_PVARIANCE:
    case CLERI_GID_F_VARIANCE:
    case CLERI_GID_F_STDDEV:
        points = siridb_points_new(1, TP_DOUBLE);
        break;
    case CLERI_GID_F_COUNT:
        points = siridb_points_new(1, TP_INT);
        break;
    case CLERI_GID_F_MEDIAN_HIGH:
    case CLERI_GID_F_MAX:
    case CLERI_GID_F_MEDIAN_LOW:
    case CLERI_GID_F_MIN:
    case CLERI_GID_F_SUM:
    case CLERI_GID_F_FIRST:
    case CLERI_GID_F_LAST:
        points = siridb_points_new(1, source->tp);
        break;
    default:
        assert (0);
        points = NULL;
    }

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
        return NULL;
    }

    /* set time-stamp */
    points->data->ts = source->data[
        (aggr->gid == CLERI_GID_F_FIRST) ? 0 : (source->len - 1)].ts;

    /* set value */
    if (aggr_cb(points->data, source, aggr, err_msg))
    {
        /* error occurred, return NULL */
        siridb_points_free(points);
        return NULL;
    }

    points->len++;
    return points;
}

static siridb_points_t * AGGREGATE_group_by(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    siridb_point_t * point;
    siridb_points_t * points;
    siridb_points_t group;
    uint64_t max_sz;
    uint64_t goup_ts;
    size_t start, end;

    group.tp = source->tp;

    max_sz = ((source->data + source->len - 1)->ts - source->data->ts)
            / aggr->group_by + 2;

    if (max_sz > source->len)
    {
        max_sz = source->len;
    }

    /* get correct callback function */
    AGGR_cb aggr_cb = AGGREGATES[aggr->gid - F_OFFSET];

    /* create new points with max possible size after re-indexing */
    switch(aggr->gid)
    {
    case CLERI_GID_F_MEAN:
    case CLERI_GID_F_MEDIAN:
    case CLERI_GID_F_PVARIANCE:
    case CLERI_GID_F_VARIANCE:
    case CLERI_GID_F_STDDEV:
    case CLERI_GID_F_DERIVATIVE:
        points = siridb_points_new(max_sz, TP_DOUBLE);
        break;
    case CLERI_GID_F_COUNT:
    case CLERI_GID_F_TIMEVAL:
    case CLERI_GID_F_INTERVAL:
        points = siridb_points_new(max_sz, TP_INT);
        break;
    case CLERI_GID_F_MEDIAN_HIGH:
    case CLERI_GID_F_MAX:
    case CLERI_GID_F_MEDIAN_LOW:
    case CLERI_GID_F_MIN:
    case CLERI_GID_F_SUM:
    case CLERI_GID_F_DIFFERENCE:
    case CLERI_GID_F_FIRST:
    case CLERI_GID_F_LAST:
        points = siridb_points_new(max_sz, group.tp);
        break;
    default:
        assert (0);
        points = NULL;
    }

    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
        return NULL;
    }

    goup_ts = GROUP_TS(source->data);

    for(start = end = 0; end < source->len; end++)
    {
        if ((source->data + end)->ts > goup_ts)
        {
            group.data = (source->data + start);
            group.len = end - start;
            point = points->data + points->len;
            point->ts = goup_ts;
            if (aggr_cb(point, &group, aggr, err_msg))
            {
                /* error occurred, return NULL */
                siridb_points_free(points);
                return NULL;
            }
            points->len++;
            start = end;
            goup_ts = GROUP_TS((source->data + end));
        }
    }

    group.data = (source->data + start);
    group.len = end - start;
    point = points->data + points->len;
    point->ts = goup_ts;
    if (aggr_cb(point, &group, aggr, err_msg))
    {
        /* error occurred, return NULL */
        siridb_points_free(points);
        return NULL;
    }
    points->len++;

    if (points->len < max_sz)
    {
        /* shrink points allocation */
        if (points->len == 0)
        {
            free(points->data);
            points->data = NULL;
        }
        else
        {
            point = realloc(points->data, points->len * sizeof(siridb_point_t));
            if (point == NULL)
            {
                /* not critical */
                log_error("Re-allocation points failed.");
            }
            else
            {
                points->data = point;
            }
        }
    }
    /* else { assert (points->len == max_sz); } */

    return points;
}

static int aggr_count(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg __attribute__((unused)))
{
    point->val.int64 = points->len;
    return 0;
}

static int aggr_derivative(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg)
{
    assert (points->len);

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use derivative() on string type.");
        return -1;
    }

    if (points->len == 1)
    {
        point->val.real = 0.0;
    }
    else
    {
        double first, last;

        switch (points->tp)
        {
        case TP_INT:
            first = (double) points->data->val.int64;
            last = (double) (points->data + points->len - 1)->val.int64;
            break;
        case TP_DOUBLE:
            first = points->data->val.real;
            last = (points->data + points->len - 1)->val.real;
            break;
        default:
            assert (0);
            first = 0.0;
            last = 0.0;
            break;
        }

        /* time-span is actually a factor when used with group_by */
        point->val.real = (last- first) * aggr->timespan;
    }

    return 0;
}

static int aggr_difference(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use difference() on string type.");
        return -1;

    case TP_INT:
        if (points->len == 1)
        {
            point->val.int64 = 0;
        }
        else
        {
            int64_t first = points->data->val.int64;
            int64_t last = (points->data + points->len - 1)->val.int64;

            if ((first > 0 && last < LLONG_MIN + first) ||
                    (first < 0 && last > LLONG_MAX + first))
            {
                sprintf(err_msg, "Overflow detected while using difference().");
                return -1;
            }

            point->val.int64 = last- first;
        }
        break;

    case TP_DOUBLE:
        if (points->len == 1)
        {
            point->val.real = 0.0;
        }
        else
        {
            point->val.real =
                    (points->data + points->len - 1)->val.real -
                    points->data->val.real;
        }
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}

static int aggr_max(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use max() on string type.");
        return -1;
    }

    if (points->tp == TP_INT)
    {
        int64_t max = points->data->val.int64;
        size_t i;
        for (i = 1; i < points->len; i++)
        {
            if ((points->data + i)->val.int64 > max)
            {
                max = (points->data + i)->val.int64;
            }
        }
        point->val.int64 = max;
    }
    else
    {
        double max = points->data->val.real;
        size_t i;
        for (i = 1; i < points->len; i++)
        {
            if ((points->data + i)->val.real > max)
            {
                max = (points->data + i)->val.real;
            }
        }
        point->val.real = max;
    }

    return 0;
}

static int aggr_mean(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    double sum = 0.0;
    size_t i;

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use mean() on string type.");
        return -1;

    case TP_INT:
        for (i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.int64;
        }
        break;

    case TP_DOUBLE:
        for (i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.real;
        }
        break;

    default:
        assert (0);
        break;
    }

    point->val.real = sum / points->len;

    return 0;
}

static int aggr_median(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use median() on string type.");
        return -1;
    }

    if (points->len == 1)
    {
        if (points->tp == TP_INT)
        {
            point->val.real = (double) points->data->val.int64;
        }
        else
        {
            point->val.real = points->data->val.real;
        }
    }
    else if (points->len % 2 == 1)
    {
        siridb_median_find_n(point, points, points->len / 2);
        if (points->tp == TP_INT)
        {
            point->val.real = (double) point->val.int64;
        }
    }
    else if (siridb_median_real(point, points, 0.5))
    {
        sprintf(err_msg, "Memory allocation error in median.");
        return -1;
    }

    return 0;
}

static int aggr_median_high(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use median_high() on string type.");
        return -1;
    }

    if (points->len == 1)
    {
        if (points->tp == TP_INT)
        {
            point->val.int64 = points->data->val.int64;
        }
        else
        {
            point->val.real = points->data->val.real;
        }
    }
    else if (siridb_median_find_n(point, points, points->len / 2))
    {
        sprintf(err_msg, "Memory allocation error in median high.");
        return -1;
    }
    return 0;
}

static int aggr_median_low(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use median_low() on string type.");
        return -1;
    }

    if (points->len == 1)
    {
        if (points->tp == TP_INT)
        {
            point->val.int64 = points->data->val.int64;
        }
        else
        {
            point->val.real = points->data->val.real;
        }
    }
    else if (siridb_median_find_n(point, points, (points->len - 1) / 2))
    {
        sprintf(err_msg, "Memory allocation error in median low.");
        return -1;
    }
    return 0;
}

static int aggr_min(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use min() on string type.");
        return -1;
    }

    if (points->tp == TP_INT)
    {
        int64_t min = points->data->val.int64;
        size_t i;
        for (i = 1; i < points->len; i++)
        {
            if ((points->data + i)->val.int64 < min)
            {
                min = (points->data + i)->val.int64;
            }
        }
        point->val.int64 = min;
    }
    else
    {
        double min = points->data->val.real;
        size_t i;
        for (i = 1; i < points->len; i++)
        {
            if ((points->data + i)->val.real < min)
            {
                min = (points->data + i)->val.real;
            }
        }
        point->val.real = min;
    }

    return 0;
}

static int aggr_pvariance(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use pvariance() on string type.");
        return -1;

    case TP_INT:
    case TP_DOUBLE:
        point->val.real = siridb_variance(points) / points->len;
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}

static int aggr_sum(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use sum() on string type.");
        return -1;

    case TP_INT:
        {
            int64_t sum = 0;
            int64_t tmp;
            size_t i;
            for (i = 0; i < points->len; i++)
            {
                tmp = (points->data + i)->val.int64;
                if ((tmp > 0 && sum > LLONG_MAX - tmp) ||
                        (tmp < 0 && sum < LLONG_MIN - tmp))
                {
                    sprintf(err_msg, "Overflow detected while using sum().");
                    return -1;
                }
                sum += tmp;
            }
            point->val.int64 = sum;
        }
        break;

    case TP_DOUBLE:
        {
            double sum = 0.0;
            size_t i;
            for (i = 0; i < points->len; i++)
            {
                sum += (points->data + i)->val.real;
            }
            point->val.real = sum;
        }
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}

static int aggr_variance(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use variance() on string type.");
        return -1;

    case TP_INT:
    case TP_DOUBLE:
        point->val.real = (points->len > 1) ?
                siridb_variance(points) / (points->len - 1) : 0.0;
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}

static int aggr_stddev(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg)
{
    assert (points->len);

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use stddev() on string type.");
        return -1;

    case TP_INT:
    case TP_DOUBLE:
        point->val.real = (points->len > 1) ?
                sqrt(siridb_variance(points) / (points->len - 1)) : 0.0;
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}

static int aggr_first(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg __attribute__((unused)))
{
    assert (points->len);

    siridb_point_t * source = points->data;

    switch (points->tp)
    {
    case TP_STRING:
        point->ts = source->ts;
        point->val.str = strdup(source->val.str);
        if (point->val.str == NULL)
        {
            sprintf(err_msg, "Memory allocation error.");
            return -1;
        }
        break;

    case TP_INT:
    case TP_DOUBLE:
        point->val = source->val;
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}

static int aggr_last(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr __attribute__((unused)),
        char * err_msg __attribute__((unused)))
{
    assert (points->len);

    siridb_point_t * source = points->data + (points->len - 1);

    switch (points->tp)
    {
    case TP_STRING:
        point->ts = source->ts;
        point->val.str = strdup(source->val.str);
        if (point->val.str == NULL)
        {
            sprintf(err_msg, "Memory allocation error.");
            return -1;
        }
        break;

    case TP_INT:
    case TP_DOUBLE:
        point->val = source->val;
        break;

    default:
        assert (0);
        break;
    }

    return 0;
}
