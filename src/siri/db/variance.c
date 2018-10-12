/*
 * variance.c - Calculate variance for points.
 */
#include <assert.h>
#include <math.h>
#include <siri/db/points.h>
#include <siri/db/variance.h>

double siridb_variance(siridb_points_t * points)
{
    double mean = 0.0;
    double variance = 0.0;
    size_t i;

    switch (points->tp)
    {
    case TP_INT:
        for (i = 0; i < points->len; i++)
        {
           mean += (points->data + i)->val.int64;
        }

        mean /= points->len;

        for (i = 0; i < points->len; i++)
        {
            variance += pow(
                   (double) (points->data + i)->val.int64 - mean,
                   2);
        }
        break;\
    case TP_DOUBLE:
        for (i = 0; i < points->len; i++)
        {
           mean += (points->data + i)->val.real;
        }

        mean /= points->len;

        for (i = 0; i < points->len; i++)
        {
            variance += pow(
                   (points->data + i)->val.real - mean,
                   2);
        }
        break;
    default:
        assert (0);
        break;
    }

    return variance;
}
