#include <math.h>
#include "../test.h"
#include <siri/db/points.h>
#include <siri/db/aggregate.h>


#define SIRIDB_MAX_SIZE_ERR_MSG 1024

static siridb_aggr_t aggr;
static char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];


static siridb_points_t * prepare_points(void)
{
    siridb_points_t * points = siridb_points_new(10, TP_INT);
    uint64_t timestamps[10] =   {3, 6, 7, 10, 11, 13, 14, 15, 25, 27};
    int64_t values[10] =        {1, 3, 0, 2,  4,  8,  3,  5,  6,  3};
    qp_via_t val;
    unsigned int i;

    siridb_init_aggregates();

    for (i = 0; i < 10; i++)
    {
        val.int64 = values[i];
        siridb_points_add_point(points, &timestamps[i], &val);
    }

    return points;
}

static int test_count(void)
{
    test_start("aggr (count)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_COUNT;
    aggr.group_by = 6;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 4);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 6 && aggrp->data->val.int64 == 2);
    _assert ((aggrp->data + 3)->ts == 30 &&
            (aggrp->data + 3)->val.int64 == 2);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_first(void)
{
    test_start("aggr (first)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_FIRST;
    aggr.group_by = 5;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 5);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 5 && aggrp->data->val.int64 == 1);
    _assert ((aggrp->data + 2)->ts == 15 &&
            (aggrp->data + 2)->val.int64 == 4);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_last(void)
{
    test_start("aggr (last)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_LAST;
    aggr.group_by = 5;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 5);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 5 && aggrp->data->val.int64 == 1);
    _assert ((aggrp->data + 2)->ts == 15 &&
            (aggrp->data + 2)->val.int64 == 5);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_max(void)
{
    test_start("aggr (max)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_MAX;
    aggr.group_by = 10;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 3);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 10 && aggrp->data->val.int64 == 3);
    _assert ((aggrp->data + 2)->ts == 30 &&
            (aggrp->data + 2)->val.int64 == 6);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}


static int test_mean(void)
{
    test_start("aggr (mean)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEAN;
    aggr.group_by = 4;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 5);
    _assert (aggrp->tp == TP_DOUBLE);
    _assert (aggrp->data->ts == 4 && aggrp->data->val.real == 1.0);
    _assert ((aggrp->data + 4)->ts == 28 &&
            (aggrp->data + 4)->val.real == 4.5);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_median(void)
{
    test_start("aggr (median)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEDIAN;
    aggr.group_by = 7;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 4);
    _assert (aggrp->tp == TP_DOUBLE);
    _assert (aggrp->data->ts == 7 && aggrp->data->val.real == 1.0);
    _assert ((aggrp->data + 1)->ts == 14 &&
            (aggrp->data + 1)->val.real == 3.5);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_median_high(void)
{
    test_start("aggr (median_high)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEDIAN_HIGH;
    aggr.group_by = 7;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 4);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 7 && aggrp->data->val.int64 == 1);
    _assert ((aggrp->data + 1)->ts == 14 &&
            (aggrp->data + 1)->val.int64 == 4);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_median_low(void)
{
    test_start("aggr (median_low)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEDIAN_LOW;
    aggr.group_by = 7;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 4);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 7 && aggrp->data->val.int64 == 1);
    _assert ((aggrp->data + 1)->ts == 14 &&
            (aggrp->data + 1)->val.int64 == 3);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_min(void)
{
    test_start("aggr (min)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_MIN;
    aggr.group_by = 2;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 9);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 4 && aggrp->data->val.int64 == 1);
    _assert ((aggrp->data + 5)->ts == 14 &&
            (aggrp->data + 5)->val.int64 == 3);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_pvariance(void)
{
    test_start("aggr (pvariance)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_PVARIANCE;
    aggr.group_by = 5;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 5);
    _assert (aggrp->tp == TP_DOUBLE);
    _assert (aggrp->data->ts == 5 && aggrp->data->val.real == 0.0);
    _assert ((aggrp->data + 2)->ts == 15 &&
            (aggrp->data + 2)->val.real == 3.5);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_stddev(void)
{
    test_start("aggr (stddev)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_STDDEV;
    aggr.group_by = 6;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 4);
    _assert (aggrp->tp == TP_DOUBLE);
    _assert (aggrp->data->ts == 6 && aggrp->data->val.real == sqrt(2.0));
    _assert ((aggrp->data + 1)->ts == 12 &&
            (aggrp->data + 1)->val.real == 2.0);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_sum(void)
{
    test_start("aggr (sum)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_SUM;
    aggr.group_by = 5;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 5);
    _assert (aggrp->tp == TP_INT);
    _assert (aggrp->data->ts == 5 && aggrp->data->val.int64 == 1);
    _assert ((aggrp->data + 2)->ts == 15 &&
            (aggrp->data + 2)->val.int64 == 20);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

static int test_variance(void)
{
    test_start("aggr (variance)");

    siridb_points_t * aggrp, * points = prepare_points();

    aggr.gid = CLERI_GID_F_VARIANCE;
    aggr.group_by = 6;
    aggr.limit = 0;
    aggr.offset = 0;

    aggrp = siridb_aggregate_run(points, &aggr, err_msg);

    _assert (aggrp != NULL);
    _assert (aggrp->len == 4);
    _assert (aggrp->tp == TP_DOUBLE);
    _assert (aggrp->data->ts == 6 && aggrp->data->val.real == 2.0);
    _assert ((aggrp->data + 1)->ts == 12 &&
            (aggrp->data + 1)->val.real == 4.0);

    siridb_points_free(aggrp);
    siridb_points_free(points);

    return test_end();
}

int main()
{
    return (
        test_count() ||
        test_first() ||
        test_last() ||
        test_max() ||
        test_mean() ||
        test_median() ||
        test_median_high() ||
        test_median_low() ||
        test_min() ||
        test_pvariance() ||
        test_stddev() ||
        test_sum() ||
        test_variance() ||
        0
    );
}
