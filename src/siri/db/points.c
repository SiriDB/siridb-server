/*
 * points.c - Array object for points.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#include <siri/db/points.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <siri/err.h>
#include <unistd.h>

#define MAX_ITERATE_MERGE_COUNT 1000
#define POINTS_MAX_QSORT 250000

static void POINTS_sort_while_merge(slist_t * plist, siridb_points_t * points);
static void POINTS_merge_and_sort(slist_t * plist, siridb_points_t * points);
static void POINTS_simple_sort(siridb_points_t * points);
static int POINTS_compare(const void * a, const void * b);
static void POINTS_highest_and_merge(slist_t * plist, siridb_points_t * points);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_points_t * siridb_points_new(size_t size, points_tp tp)
{
    siridb_points_t * points =
            (siridb_points_t *) malloc(sizeof(siridb_points_t));
    if (points == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        points->len = 0;
        points->tp = tp;
        points->content = NULL;
        points->data = (siridb_point_t *) malloc(sizeof(siridb_point_t) * size);
        if (points->data == NULL)
        {
            ERR_ALLOC
            free(points);
            points = NULL;
        }
    }
    return points;
}

/*
 * Destroy points. (parsing NULL is NOT allowed)
 */
void siridb_points_free(siridb_points_t * points)
{
    free(points->content);
    free(points->data);
    free(points);
}

/*
 * Add a point to points. (points are sorted by timestamp so the new point
 * will be inserted at the correct position.
 *
 * Warning:
 *      this functions assumes points to be large enough to hold the new
 *      point and is therefore not safe.
 */
void siridb_points_add_point(
        siridb_points_t * points,
        uint64_t * ts,
        qp_via_t * val)
{
    size_t i;
    siridb_point_t * point;

    for (   i = points->len;
            i-- > 0 && (points->data + i)->ts > *ts;
            *(points->data + i + 1) = *(points->data + i));

    points->len++;

    point = points->data + i + 1;

    point->ts = *ts;
    point->val = *val;
}

/*
 * Returns siri_err and raises a SIGNAL in case an error has occurred.
 */
int siridb_points_pack(siridb_points_t * points, qp_packer_t * packer)
{
    qp_add_type(packer, QP_ARRAY_OPEN);
    if (points->len)
    {
        siridb_point_t * point;
        point = points->data;
        switch (points->tp)
        {
        case TP_INT:
            for (size_t i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_int64(packer, point->val.int64);
            }
            break;
        case TP_DOUBLE:
            for (size_t i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_double(packer, point->val.real);
            }
            break;
        case TP_STRING:
            /* TODO: handle string type */
            assert (0);
            break;
        }
    }
    qp_add_type(packer, QP_ARRAY_CLOSE);

    return siri_err;
}

/*
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
inline int siridb_points_raw_pack(
        siridb_points_t * points,
        qp_packer_t * packer)
{
    return (qp_add_type(packer, QP_ARRAY_OPEN) ||
            qp_add_int8(packer, points->tp) ||
            qp_add_int32(packer, points->len) ||
            qp_add_raw(
                packer,
                (char *) points->data,
                points->len * sizeof(siridb_point_t)) ||
            qp_add_type(packer, QP_ARRAY_CLOSE)) ? -1 : 0;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (err_msg is set when an error has occurred)
 * Use this function only when having at least two 'series' in the list.
 */
siridb_points_t * siridb_points_merge(slist_t * plist, char * err_msg)
{
#ifdef DEBUG
    assert (plist->len >= 2);
#endif
    siridb_points_t * points;
    siridb_points_t * tpts = NULL;
    size_t n = 0;
    size_t i;
    uint8_t int2double = 0;

    for (i = 0; i < plist->len; )
    {
        points = (siridb_points_t *) plist->data[i];

        if (!points->len)
        {
            if (!--plist->len)
            {
                /* all series are empty, return the last one */
                return plist->data[i];
            }
            /* cleanup empty points */
            siridb_points_free(plist->data[i]);

            /* shrink plist length and fill gap */
            plist->data[i] = plist->data[plist->len];

            continue;
        }

        n += points->len;

        if (tpts != NULL && tpts->tp != points->tp)
        {
            if (tpts->tp == TP_STRING || points->tp == TP_STRING)
            {
                sprintf(err_msg, "Cannot merge string and number series.");
                return NULL;
            }
            int2double = 1;
        }

        /*
         * Decrement the points->len so we can use this not truly as length
         * property but as position of the last element.
         */
        points->len--;
        tpts = points;
        i++;
    }

#ifdef DEBUG
    /* we have at least one series left */
    assert (plist->len >= 1);
#endif

    if (plist->len == 1)
    {
        /*
         * Return the only left points since there is nothing to merge as set
         * list length to 0. We do have to restore the points length since
         * this is decremented by one.
         */
        points = (siridb_points_t *) slist_pop(plist);
        points->len++;
        return points;
    }

    points = siridb_points_new(n, (int2double) ? TP_DOUBLE : tpts->tp);


    if (points == NULL)
    {
        sprintf(err_msg, "Memory allocation error.");
    }
    else
    {
        usleep(1000);

        /*
         * When both series from type double and type integer are merged
         * we need to promote the integer series to double.
         */
        if (int2double)
        {
            size_t j;
            for (i = 0; i < plist->len; i++)
            {
                tpts = (siridb_points_t *) plist->data[i];
                if (tpts->tp == TP_INT)
                {
                    for (j = 0; j <= tpts->len; j++)
                    {
                        tpts->data[j].val.real =
                                (double) tpts->data[j].val.int64;
                    }
                    usleep(100);
                }
            }
        }

        points->len = n;

        /*
         * Select a merge method depending on the ratio series and points.
         */
        if (plist->len < 4)
        {
            POINTS_sort_while_merge(plist, points);
        }
        else if (n == plist->len || n < POINTS_MAX_QSORT)
        {
            POINTS_merge_and_sort(plist, points);
        }
        else
        {
            POINTS_highest_and_merge(plist, points);
        }
    }
    return points;
}

/*
 * This merge method works best when having both a lot of series and having
 * any number of points for each series.
 */
static void POINTS_highest_and_merge(slist_t * plist, siridb_points_t * points)
{
    siridb_points_t ** m = NULL;
    size_t i, n, l = 0;
    siridb_points_t ** tpts;
    uint64_t highest = 0;
    n = points->len;

    while (plist->len)
    {
        if (m != NULL)
        {
            tpts = m;
            m = NULL;
            for (; l < plist->len; l++, tpts++)
            {
                if (((*tpts)->data + (*tpts)->len)->ts == highest)
                {
                    m = tpts;
                    break;
                }
            }
        }

        if (m == NULL)
        {
            m = (siridb_points_t **) plist->data;

            for (i = 1, tpts = m + 1; i < plist->len; i++, tpts++)
            {
                if (((*tpts)->data + (*tpts)->len)->ts >
                    ((*m)->data + (*m)->len)->ts)
                {
                    m = tpts;
                    l = i;
                    highest = ((*m)->data + (*m)->len)->ts;
                }
            }
        }

        points->data[--n] = (*m)->data[(*m)->len];

        if ((*m)->len--)
        {
            m++;
            l++;
        }
        else
        {
            siridb_points_free(*m);

            if (--plist->len)
            {
                *m = (siridb_points_t *) plist->data[plist->len];
            }
        }

        if (!(n % MAX_ITERATE_MERGE_COUNT))
        {
            usleep(3000);
        }
    }
#ifdef DEBUG
    /* size should be exactly zero */
    assert (n == 0);
#endif
}


/*
 * This merge method works best for only a few series with possible a lot
 * of points.
 */
static void POINTS_sort_while_merge(slist_t * plist, siridb_points_t * points)
{
    siridb_points_t ** m;
    size_t i, n;
    siridb_points_t ** tpts;
    n = points->len;

    while (plist->len)
    {
        m = (siridb_points_t **) plist->data;

        for (i = 1, tpts = m + 1; i < plist->len; i++, tpts++)
        {
            if (((*tpts)->data + (*tpts)->len)->ts >
                ((*m)->data + (*m)->len)->ts)
            {
                m = tpts;
            }
        }

        points->data[--n] = (*m)->data[(*m)->len];

        if (!(*m)->len--)
        {
            siridb_points_free(*m);

            if (--plist->len)
            {
                *m = (siridb_points_t *) plist->data[plist->len];
            }
        }

        if (!(n % MAX_ITERATE_MERGE_COUNT))
        {
            usleep(3000);
        }
    }
#ifdef DEBUG
    /* size should be exactly zero */
    assert (n == 0);
#endif
}

/*
 * This merge method works best when having a lot of series with only one point
 * or when the total number of points it not so high.
 */
static void POINTS_merge_and_sort(slist_t * plist, siridb_points_t * points)
{
    siridb_points_t ** m;
    size_t i, n;
    n = points->len;
    uint8_t use_simple_sort = n == plist->len;

    while (plist->len)
    {
        for (i = 0; i < plist->len; i++)
        {
            m = (siridb_points_t **) &plist->data[i];

            points->data[--n] = (*m)->data[(*m)->len];

            if (!(*m)->len--)
            {
                siridb_points_free(*m);
                if (--plist->len)
                {
                    *m = (siridb_points_t *) plist->data[plist->len];
                }
            }

            if (!(n % MAX_ITERATE_MERGE_COUNT))
            {
                usleep(3000);
            }
        }
    }

#ifdef DEBUG
    /* size should be exactly zero */
    assert (n == 0);
#endif

    usleep(5000);

    if (use_simple_sort)
    {
        POINTS_simple_sort(points);
    }
    else
    {
        qsort(  points->data,
                points->len,
                sizeof(siridb_point_t),
                &POINTS_compare);
    }
}

inline static int POINTS_compare(const void * a, const void * b)
{
    return (((siridb_point_t *) a)->ts - ((siridb_point_t *) b)->ts);
}
/*
 * Sort points by time-stamp.
 *
 * Warning: this function should only be used in another thread.
 */
static void POINTS_simple_sort(siridb_points_t * points)
{
    if (points->len < 2)
    {
        return;  /* nothing to do... */
    }
    size_t n = 0;
    size_t i;
    siridb_point_t * pa, * pb;
    siridb_point_t tmp;
    while(++n < points->len)
    {
        pa = points->data + n - 1;
        pb = points->data + n;
        if (pa->ts > pb->ts)
        {
            tmp = points->data[n];
            i = n;
            do
            {
                points->data[i] = points->data[i - 1];
            }
            while (--i && points->data[i - 1].ts > tmp.ts);
            points->data[i] = tmp;

            usleep(100);
        }
    }
}
