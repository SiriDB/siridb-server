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
siridb_points_t * siridb_points_new(size_t size, uint8_t tp)
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
        case SIRIDB_POINTS_TP_INT:
            for (size_t i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_int64(packer, point->val.int64);
            }
            break;
        case SIRIDB_POINTS_TP_DOUBLE:
            for (size_t i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_double(packer, point->val.real);
            }
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


