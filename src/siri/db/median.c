/*
 * median.c - Calculate median, median high and median low.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-04-2016
 *
 */


#include <siri/db/median.h>
#include <siri/db/points.h>
#include <stdbool.h>

static double find_n_int64(
        int64_t * seq,
        uint64_t size,
        uint64_t n);

static double find_n_double(
        double * seq,
        uint64_t size,
        uint64_t n);

static double find_median_real_int64(
        int64_t * seq,
        uint64_t size,
        uint64_t n,
        int64_t a,
        int64_t b,
        bool found_a,
        bool found_b);

static double find_median_real_double(
        double * seq,
        uint64_t size,
        uint64_t n,
        double a,
        double b,
        bool found_a,
        bool found_b);

void siridb_median_find_n(
        siridb_point_t * point,
        siridb_points_t * points,
        uint64_t n)
{
    uint64_t i, size_l, size_r;

    size_l = size_r = 0;

    if (points->tp == TP_INT)
    {
        int64_t pivot, v;
        int64_t arr_l[points->len - 1];
        int64_t arr_r[points->len - 1];
        pivot = points->data->val.int64;

        for (i = 1; i < points->len; i++)
        {
            v = points->data[i].val.int64;
            if (v > pivot)
            {
                arr_r[size_r] = v;
                size_r++;
            }
            else
            {
                arr_l[size_l] = v;
                size_l++;
            }
        }
        point->val.int64 = (size_l == n) ? pivot : ((size_l > n) ?
                find_n_int64(arr_l, size_l, n) :
                find_n_int64(arr_r, size_r, n - size_l - 1));
    }
    else
    {
        double pivot, v;
        double arr_l[points->len - 1];
        double arr_r[points->len - 1];
        pivot = points->data->val.real;
        for (i = 1; i < points->len; i++)
        {
            v = points->data[i].val.real;
            if (v > pivot)
            {
                arr_r[size_r] = v;
                size_r++;
            }
            else
            {
                arr_l[size_l] = v;
                size_l++;
            }
        }

        point->val.real = (size_l == n) ? pivot : (size_l > n) ?
                find_n_double(arr_l, size_l, n) :
                find_n_double(arr_r, size_r, n - size_l - 1);
    }
}

void siridb_median_real(
        struct siridb_point_s * point,
        struct siridb_points_s * points,
        double percentage)
{
    uint64_t i, size_l, size_r, n;
    bool found_a, found_b;

    n = points->len * percentage;

    size_l = size_r = 0;

    if (points->tp == TP_INT)
    {
        int64_t pivot, v, a, b;
        int64_t arr_l[points->len - 1];
        int64_t arr_r[points->len - 1];
        a = b = 0;
        pivot = points->data->val.int64;
        for (i = 1; i < points->len; i++)
        {
            v = points->data[i].val.int64;
            if (v > pivot)
            {
                arr_r[size_r] = v;
                size_r++;
            }
            else
            {
                arr_l[size_l] = v;
                size_l++;
            }
        }

        if (size_l == n - 1)
        {
            a = pivot;
            found_a = true;
        }
        else if (size_l == n)
        {
            b = pivot;
            found_b = true;
        }

        point->val.real = ((!found_b && size_l > n) || size_l > n - 1) ?
            find_median_real_int64(
                    arr_l, size_l, n, a, b, found_a, found_b) :
            find_median_real_int64(
                    arr_r, size_r, n - size_l - 1, a, b, found_a, found_b);
    }
    else
    {
        double pivot, v, a, b;
        double arr_l[points->len - 1];
        double arr_r[points->len - 1];
        a = b = 0.0;
        pivot = points->data->val.real;
        for (i = 1; i < points->len; i++)
        {
            v = points->data[i].val.real;
            if (v > pivot)
            {
                arr_r[size_r] = v;
                size_r++;
            }
            else
            {
                arr_l[size_l] = v;
                size_l++;
            }
        }
        if (size_l == n - 1)
        {
            a = pivot;
            found_a = true;
        }
        else if (size_l == n)
        {
            b = pivot;
            found_b = true;
        }
        point->val.real = ((!found_b && size_l > n) || size_l > n - 1) ?
            find_median_real_double(
                    arr_l, size_l, n, a, b, found_a, found_b) :
            find_median_real_double(
                    arr_r, size_r, n - size_l - 1, a, b, found_a, found_b);
    }
}

static double find_median_real_int64(
        int64_t * seq,
        uint64_t size,
        uint64_t n,
        int64_t a,
        int64_t b,
        bool found_a,
        bool found_b)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    int64_t pivot, v;
    int64_t arr_l[size - 1];
    int64_t arr_r[size - 1];
    pivot = seq[0];

    for (i = 1; i < size; i++)
    {
        v = seq[i];
        if (v > pivot)
        {
            arr_r[size_r] = v;
            size_r++;
        }
        else
        {
            arr_l[size_l] = v;
            size_l++;
        }
    }

    if (size_l == n - 1)
    {
        a = pivot;
        found_a = true;
    }
    else if (size_l == n)
    {
        b = pivot;
        found_b = true;
    }

    if (found_a && found_b)
        return (a + b) / 2.0;

    if ((!found_b && size_l > n) || size_l > n - 1)
        return find_median_real_int64(arr_l, size_l, n, a, b, found_a, found_b);

    return find_median_real_int64(
                arr_r, size_r, n - size_l - 1, a, b, found_a, found_b);

}

static double find_median_real_double(
        double * seq,
        uint64_t size,
        uint64_t n,
        double a,
        double b,
        bool found_a,
        bool found_b)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    double pivot, v;
    double arr_l[size - 1];
    double arr_r[size - 1];
    pivot = seq[0];
    for (i = 1; i < size; i++)
    {
        v = seq[i];
        if (v > pivot)
        {
            arr_r[size_r] = v;
            size_r++;
        }
        else
        {
            arr_l[size_l] = v;
            size_l++;
        }
    }
    if (size_l == n - 1)
    {
        a = pivot;
        found_a = true;
    }
    else if (size_l == n)
    {
        b = pivot;
        found_b = true;
    }

    if (found_a && found_b)
        return (a + b) / 2.0;

    if ((!found_b && size_l > n) || size_l > n - 1)
        return find_median_real_double(arr_l, size_l, n, a, b, found_a, found_b);

    return find_median_real_double(
                arr_r, size_r, n - size_l - 1, a, b, found_a, found_b);

}

static double find_n_int64(
        int64_t * seq,
        uint64_t size,
        uint64_t n)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    int64_t pivot, v;
    int64_t arr_l[size-1];
    int64_t arr_r[size-1];

    pivot = seq[0];

    for (i = 1; i < size; i++)
    {
        v = seq[i];
        if (v > pivot)
        {
            arr_r[size_r] = v;
            size_r++;
        }
        else
        {
            arr_l[size_l] = v;
            size_l++;
        }
    }
    if (size_l == n)
        return pivot;

    if (size_l > n)
        return find_n_int64(arr_l, size_l, n);

    return find_n_int64(arr_r, size_r, n - size_l - 1);
}

static double find_n_double(
        double * seq,
        uint64_t size,
        uint64_t n)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    double pivot, v;
    double arr_l[size-1];
    double arr_r[size-1];

    pivot = seq[0];

    for (i = 1; i < size; i++)
    {
        v = seq[i];
        if (v > pivot)
        {
            arr_r[size_r] = v;
            size_r++;
        }
        else
        {
            arr_l[size_l] = v;
            size_l++;
        }
    }
    if (size_l == n)
        return pivot;

    if (size_l > n)
        return find_n_double(arr_l, size_l, n);

    return find_n_double(arr_r, size_r, n - size_l - 1);
}
