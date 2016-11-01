/*
 * aggregate.c - SiriDB aggregation methods.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 15-04-2016
 *
 */
#include <assert.h>
#include <limits.h>
#include <logger/logger.h>
#include <siri/db/aggregate.h>
#include <siri/db/median.h>
#include <siri/db/variance.h>
#include <siri/err.h>
#include <siri/grammar/grammar.h>
#include <slist/slist.h>
#include <stddef.h>
#include <strextra/strextra.h>

#define AGGR_NEW                                    \
if ((aggr = AGGREGATE_new(gid)) == NULL)            \
{                                                   \
    sprintf(err_msg, "Memory allocation error.");   \
    siridb_aggregate_list_free(slist);              \
    return NULL;                                    \
}

#define SLIST_APPEND                                \
if (slist_append_safe(&slist, aggr))                \
{                                                   \
    ERR_ALLOC                                       \
    sprintf(err_msg, "Memory allocation error.");   \
    AGGREGATE_free(aggr);                           \
    siridb_aggregate_list_free(slist);              \
    return NULL;                                    \
}

typedef int (* AGGR_cb)(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

#define GROUP_TS(point) \
    (point->ts + aggr->group_by - 1) / aggr->group_by * aggr->group_by

static AGGR_cb AGGREGATES[F_OFFSET];

static siridb_aggr_t * AGGREGATE_new(uint32_t gid);
static void AGGREGATE_free(siridb_aggr_t * aggr);
static int AGGREGATE_init_filter(
        siridb_aggr_t * aggr,
        cleri_node_t * node,
        char * err_msg);
static siridb_points_t * AGGREGATE_derivative(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);
static siridb_points_t * AGGREGATE_difference(
        siridb_points_t * source,
        char * err_msg);
static siridb_points_t * AGGREGATE_filter(
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

/*
 * Initialize aggregates.
 */
void siridb_init_aggregates(void)
{
    for (uint_fast16_t i = 0; i < F_OFFSET; i++)
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
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
slist_t * siridb_aggregate_list(cleri_children_t * children, char * err_msg)
{
    uint32_t gid;
    slist_t * slist = slist_new(SLIST_DEFAULT_SIZE);
    if (slist == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        siridb_aggr_t * aggr;

        if (slist != NULL)
        {
            while (1)
            {
                gid = children->node->children->node->cl_obj->via.dummy->gid;

                switch (gid)
                {
                case CLERI_GID_F_FILTER:
                    AGGR_NEW
                    {
                        cleri_node_t * onode;
                        if (    children->node->children->node->children->
                                next->next->next->next == NULL)
                        {
                            aggr->filter_opr = CEXPR_EQ;
                            onode = children->node->children->node->
                                    children->next->next->node->
                                    children->node;
                        }
                        else
                        {
                            aggr->filter_opr = cexpr_operator_fn(
                                    children->node->children->node->
                                    children->next->next->node->
                                    children->node);
                            onode = children->node->children->node->
                                    children->next->next->next->node->
                                    children->node;
                        }
                        if (AGGREGATE_init_filter(aggr, onode, err_msg))
                        {
                            AGGREGATE_free(aggr);
                            siridb_aggregate_list_free(slist);
                            return NULL;
                        }
                    }

                    SLIST_APPEND

                    break;

                case CLERI_GID_F_DERIVATIVE:
                    AGGR_NEW
                    {
                        cleri_node_t * dlist = children->node->children->
                                node->children->next->next->node;

                        if (dlist->children->node != NULL)
                        {
                            /* result is at least positive, checked earlier */
                            aggr->timespan =
                                    (double) dlist->children->node->result;

                            if (!aggr->timespan)
                            {
                                sprintf(err_msg,
                                        "Time-span must be an integer value "
                                        "larger than zero.");
                                AGGREGATE_free(aggr);
                                siridb_aggregate_list_free(slist);
                                return NULL;
                            }

                            if (dlist->children->next != NULL)
                            {
                                /* result is always positive */
                                aggr->group_by = dlist->children->next->next->
                                        node->result;

                                if (!aggr->group_by)
                                {
                                    sprintf(err_msg,
                                            "Group by time must be an integer "
                                            "value larger than zero.");
                                    AGGREGATE_free(aggr);
                                    siridb_aggregate_list_free(slist);
                                    return NULL;
                                }

                                aggr->timespan /= (double) aggr->group_by;
                            }
                        }
                    }

                    SLIST_APPEND

                    break;

                case CLERI_GID_F_DIFFERENCE:
                    AGGR_NEW
                    if (children->node->children->node->children->
                                next->next->next != NULL)
                    {
                        /* result is always positive, checked earlier */
                        aggr->group_by = children->node->children->node->
                                children->next->next->node->children->
                                node->result;

                        if (!aggr->group_by)
                        {
                            sprintf(err_msg,
                                    "Group by time must be an integer value "
                                    "larger than zero.");
                            AGGREGATE_free(aggr);
                            siridb_aggregate_list_free(slist);
                            return NULL;
                        }
                    }

                    SLIST_APPEND

                    break;

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
                    AGGR_NEW
                    aggr->group_by = children->node->children->node->children->
                            next->next->node->result;

                    if (!aggr->group_by)
                    {
                        sprintf(err_msg,
                                "Group by time must be an integer value "
                                "larger than zero.");
                        AGGREGATE_free(aggr);
                        siridb_aggregate_list_free(slist);
                        return NULL;
                    }

                    SLIST_APPEND

                    break;

                case CLERI_GID_F_POINTS:
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
        }
    }
    return slist;
}

/*
 * Destroy aggregates list. (parsing NULL is not allowed)
 */
void siridb_aggregate_list_free(slist_t * alist)
{
    for (size_t i = 0; i < alist->len; i++)
    {
        AGGREGATE_free(alist->data[i]);
    }
    free(alist);
}

siridb_points_t * siridb_aggregate_run(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (source->len);
#endif

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

    default:
        assert (0);
        break;
    }

    return NULL;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
static siridb_aggr_t * AGGREGATE_new(uint32_t gid)
{
    siridb_aggr_t * aggr = (siridb_aggr_t *) malloc(sizeof(siridb_aggr_t));
    if (aggr == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        aggr->gid = gid;
        aggr->group_by = 0;
        aggr->timespan = 1.0;
        aggr->filter_tp = TP_INT;  /* when string we malloc/free
                                                  * aggr->filter_via.raw */
    }
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
    }
    free(aggr);
}

/*
 * Returns 0 if successful or -1 in case or an error.
 * A signal is raised in case of an allocation error and the err_msg is set
 * for any error.
 */
static int AGGREGATE_init_filter(
        siridb_aggr_t * aggr,
        cleri_node_t * node,
        char * err_msg)
{
    switch (node->cl_obj->via.dummy->gid)
    {
    case CLERI_GID_R_INTEGER:
        aggr->filter_tp = TP_INT;
        aggr->filter_via.int64 = atoll(node->str);
        break;

    case CLERI_GID_R_FLOAT:
        aggr->filter_tp = TP_DOUBLE;
        aggr->filter_via.real = strx_to_double(node->str, node->len);
        break;

    case CLERI_GID_STRING:
        aggr->filter_tp = TP_STRING;
        aggr->filter_via.raw = (char *) malloc(node->len - 1);
        if (aggr->filter_via.raw == NULL)
        {
            ERR_ALLOC
            sprintf(err_msg, "Memory allocation error.");
            return -1;
        }
        strx_extract_string(aggr->filter_via.raw, node->str, node->len);
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

static siridb_points_t * AGGREGATE_derivative(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
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
                if (cexpr_str_cmp(
                        aggr->filter_opr,
                        spt->val.raw,
                        value.raw))
                {
                    dpt->ts = spt->ts;
                    dpt->val = spt->val;
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
            break;
        }

        points->len = dpt - points->data;

        if (source->len > points->len)
        {
            dpt = (siridb_point_t *) realloc(
                    points->data,
                    points->len * sizeof(siridb_point_t));
            if (dpt == NULL && points->len)
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
    case CLERI_GID_F_DERIVATIVE:
        points = siridb_points_new(max_sz, TP_DOUBLE);
        break;
    case CLERI_GID_F_COUNT:
        points = siridb_points_new(max_sz, TP_INT);
        break;
    case CLERI_GID_F_MEDIAN_HIGH:
    case CLERI_GID_F_MAX:
    case CLERI_GID_F_MEDIAN_LOW:
    case CLERI_GID_F_MIN:
    case CLERI_GID_F_SUM:
    case CLERI_GID_F_DIFFERENCE:
        points = siridb_points_new(max_sz, group.tp);
        break;
    default:
        assert (0);
        points = NULL;
    }

    if (points == NULL)
    {
        return NULL;  /* signal is raised */
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
        point = (siridb_point_t *)
                realloc(points->data, points->len * sizeof(siridb_point_t));
        if (point == NULL && points->len)
        {
            /* not critical */
            log_error("Re-allocation points failed.");
        }
        else
        {
            points->data = point;
        }
    }
#ifdef DEBUG
    else
    {
        /* if not smaller it must be equal */
        assert (points->len == max_sz);
    }
#endif

    return points;
}

static int aggr_count(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg)
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
#ifdef DEBUG
    assert (points->len);
#endif

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use difference() on string type.");
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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use max() on string type.");
        return -1;
    }

    if (points->tp == TP_INT)
    {
        int64_t max = points->data->val.int64;
        for (size_t i = 1; i < points->len; i++)
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
        for (size_t i = 1; i < points->len; i++)
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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

    double sum = 0.0;

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use mean() on string type.");
        return -1;

    case TP_INT:
        for (size_t i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.int64;
        }
        break;

    case TP_DOUBLE:
        for (size_t i = 0; i < points->len; i++)
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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

    if (points->tp == TP_STRING)
    {
        sprintf(err_msg, "Cannot use min() on string type.");
        return -1;
    }

    if (points->tp == TP_INT)
    {
        int64_t min = points->data->val.int64;
        for (size_t i = 1; i < points->len; i++)
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
        for (size_t i = 1; i < points->len; i++)
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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

    switch (points->tp)
    {
    case TP_STRING:
        sprintf(err_msg, "Cannot use sum() on string type.");
        return -1;

    case TP_INT:
        {
            int64_t sum = 0;
            int64_t tmp;
            for (size_t i = 0; i < points->len; i++)
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
            for (size_t i = 0; i < points->len; i++)
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
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

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
