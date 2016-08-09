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
#include <siri/db/aggregate.h>
#include <siri/grammar/grammar.h>
#include <siri/db/median.h>
#include <assert.h>
#include <logger/logger.h>
#include <siri/err.h>
#include <slist/slist.h>

#define GROUP_TS(point) \
    (point->ts + aggr->group_by - 1) / aggr->group_by * aggr->group_by

static siridb_aggr_cb AGGREGATES[F_OFFSET];

static int aggr_count(
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

static int aggr_sum(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

void siridb_init_aggregates(void)
{
    for (uint_fast16_t i = 0; i < F_OFFSET; i++)
    {
        AGGREGATES[i] = NULL;
    }

    AGGREGATES[CLERI_GID_F_COUNT - F_OFFSET] = aggr_count;
    AGGREGATES[CLERI_GID_F_MAX - F_OFFSET] = aggr_max;
    AGGREGATES[CLERI_GID_F_MEAN - F_OFFSET] = aggr_mean;
    AGGREGATES[CLERI_GID_F_MEDIAN - F_OFFSET] = aggr_median;
    AGGREGATES[CLERI_GID_F_MEDIAN_HIGH - F_OFFSET] = aggr_median_high;
    AGGREGATES[CLERI_GID_F_MEDIAN_LOW - F_OFFSET] = aggr_median_low;
    AGGREGATES[CLERI_GID_F_MIN - F_OFFSET] = aggr_min;
    AGGREGATES[CLERI_GID_F_SUM - F_OFFSET] = aggr_sum;

#ifdef DEBUG
    for (uint_fast16_t i = 0; i < F_OFFSET; i++)
    {
        siridb_aggregates[i] = AGGREGATES[i];
    }
#endif
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
        aggr->cb = AGGREGATES[gid - F_OFFSET];
    }
    return aggr;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
slist_t * siridb_aggregate_list(cleri_node_t * node)
{
    cleri_children_t * children = node->children->node->children;
    uint32_t gid;
    slist_t * slist = slist_new(SLIST_DEFAULT_SIZE);
    if (slist != NULL)
    {
        siridb_aggr_t * aggr;
        if (slist != NULL)
        {
            while (1)
            {
                gid = children->node->children->node->cl_obj->via.dummy->gid;

                switch (gid)
                {
                case CLERI_GID_F_COUNT:
                case CLERI_GID_F_MEAN:
                case CLERI_GID_F_SUM:
                    aggr = AGGREGATE_new(gid);
                    if (aggr != NULL)
                    {
                        aggr->group_by =
                                children->node->children->node->children->
                                next->next->node->result;

                        if (slist_append_safe(&slist, aggr))
                        {
                            ERR_ALLOC
                        }
                    }
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

siridb_points_t * siridb_aggregate_run(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (source->len);
#endif

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

    /* create new points with max possible size after re-indexing */
    points = siridb_points_new(
            max_sz,
            (aggr->cb == aggr_mean || aggr->cb == aggr_median) ?
                    SIRIDB_POINTS_TP_DOUBLE :
            (aggr->cb == aggr_count) ? SIRIDB_POINTS_TP_INT : group.tp);

    if (points == NULL)
    {
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
            if (aggr->cb(point, &group, aggr, err_msg))
            {
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
    if (aggr->cb(point, &group, aggr, err_msg))
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

static int aggr_max(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use max() on string type.");
        return -1;
    }

    if (points->tp == SIRIDB_POINTS_TP_INT)
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

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use mean() on string type.");
        return -1;
    }

    if (points->tp == SIRIDB_POINTS_TP_INT)
    {
        for (size_t i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.int64;
        }
    }
    else
    {
        for (size_t i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.real;
        }
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

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use median() on string type.");
        return -1;
    }

    if (points->len % 2 == 1)
    {
        siridb_median_find_n(point, points, points->len / 2);
        if (points->tp == SIRIDB_POINTS_TP_INT)
        {
            point->val.real = (double) point->val.int64;
        }
    }
    else
        siridb_median_real(point, points, 0.5);

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

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use median_high() on string type.");
        return -1;
    }

    siridb_median_find_n(point, points, points->len / 2);

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

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use median_low() on string type.");
        return -1;
    }

    siridb_median_find_n(point, points, (points->len - 1) / 2);

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

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use min() on string type.");
        return -1;
    }

    if (points->tp == SIRIDB_POINTS_TP_INT)
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

static int aggr_sum(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg)
{
#ifdef DEBUG
    assert (points->len);
#endif

    if (points->tp == SIRIDB_POINTS_TP_STRING)
    {
        sprintf(err_msg, "Cannot use sum() on string type.");
        return -1;
    }

    if (points->tp == SIRIDB_POINTS_TP_INT)
    {
        int64_t sum = 0;
        for (size_t i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.int64;
        }
        point->val.int64 = sum;
    }
    else
    {
        double sum = 0.0;
        for (size_t i = 0; i < points->len; i++)
        {
            sum += (points->data + i)->val.real;
        }
        point->val.real = sum;
    }

    return 0;
}
