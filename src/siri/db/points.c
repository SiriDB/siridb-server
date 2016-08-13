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

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_points_t * siridb_points_new(size_t size, siridb_points_tp tp)
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
    LOGC("START MERGING");
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

    LOGC("MERGING STEP 1");

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
        LOGC("MERGING STEP 2");

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
                }
            }
        }
        LOGC("MERGING STEP 3");

        points->len = n;

        siridb_points_t ** m;

        while (plist->len)
        {
            m = (siridb_points_t **) &plist->data[0];

            for (i = 1; i < plist->len; i++)
            {
                tpts = (siridb_points_t *) plist->data[i];
                if (((siridb_point_t *) tpts->data)[tpts->len].ts >
                    ((siridb_point_t *) (*m)->data)[(*m)->len].ts)
                {
                    m = (siridb_points_t **) &plist->data[i];
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
        }

        LOGC("MERGING FINISHED...");


#ifdef DEBUG
        /* size should be exactly zero */
        assert (n == 0);
#endif
    }
    return points;
}
