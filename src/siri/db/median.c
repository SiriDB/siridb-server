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
#include <assert.h>
#include <logger/logger.h>

static double find_n_int64(
        int64_t * seq,
        uint64_t size,
        int64_t * other,
        uint64_t n);

static double find_n_double(
        double * seq,
        uint64_t size,
        double * other,
        uint64_t n);

static double find_median_real_int64(
        int64_t * seq,
        uint64_t size,
        int64_t * other,
        uint64_t n,
        int64_t a,
        int64_t b,
        bool found_a,
        bool found_b);

static double find_median_real_double(
        double * seq,
        uint64_t size,
        double * other,
        uint64_t n,
        double a,
        double b,
        bool found_a,
        bool found_b);

int siridb_median_find_n(
        siridb_point_t * point,
        siridb_points_t * points,
        uint64_t n)
{
#ifdef DEBUG
	assert (points->len >= 2);
#endif
	int rc = 0;
    uint64_t i, npivot, size_l, size_r;

    size_l = size_r = 0;
    npivot = points->len / 2;

    if (points->tp == TP_INT)
    {
        int64_t pivot, v;

        int64_t * arr_l =
        		(int64_t *) malloc(sizeof(int64_t) * 2 * (points->len - 1));

        if (arr_l == NULL)
        {
        	log_critical("Memory allocation error occurred.");
        	rc = -1;
        }
        else
        {
            int64_t * arr_r = arr_l + points->len - 1;
			int64_t * equal = arr_l;
			uint64_t * equal_size = &size_l;

			pivot = points->data[npivot].val.int64;

			for (i = 0; i < points->len; i++)
			{
				if (i == npivot)
				{
					continue;
				}
				v = points->data[i].val.int64;
				if (v == pivot)
				{
					equal[(*equal_size)++] = v;
					if (equal == arr_l)
					{
						equal = arr_r;
						equal_size = &size_r;
					}
					else
					{
						equal = arr_l;
						equal_size = &size_l;
					}
				}
				else if (v > pivot)
				{
					arr_r[size_r++] = v;
				}
				else
				{
					arr_l[size_l++] = v;
				}
			}
			point->val.int64 = (size_l == n) ? pivot : ((size_l > n) ?
					find_n_int64(arr_l, size_l, arr_r, n) :
					find_n_int64(arr_r, size_r, arr_l, n - size_l - 1));
        }
        free(arr_l);
    }
    else
    {
        double pivot, v;

        double * arr_l =
        		(double *) malloc(sizeof(double) * 2 * (points->len - 1));

        if (arr_l == NULL)
        {
        	log_critical("Memory allocation error occurred.");
        	rc = -1;
        }
        else
        {
            double * arr_r = arr_l + points->len - 1;
        	double * equal = arr_l;
			uint64_t * equal_size = &size_l;

			pivot = points->data[npivot].val.real;
			for (i = 0; i < points->len; i++)
			{
				if (i == npivot)
				{
					continue;
				}
				v = points->data[i].val.real;
				if (v == pivot)
				{
					equal[(*equal_size)++] = v;
					if (equal == arr_l)
					{
						equal = arr_r;
						equal_size = &size_r;
					}
					else
					{
						equal = arr_l;
						equal_size = &size_l;
					}
				}
				else if (v > pivot)
				{
					arr_r[size_r++] = v;
				}
				else
				{
					arr_l[size_l++] = v;
				}
			}

			point->val.real = (size_l == n) ? pivot : (size_l > n) ?
					find_n_double(arr_l, size_l, arr_r, n) :
					find_n_double(arr_r, size_r, arr_l, n - size_l - 1);
        }
        free(arr_l);
    }
    return rc;
}

int siridb_median_real(
		siridb_point_t * point,
        siridb_points_t * points,
        double percentage)
{
#ifdef DEBUG
	assert (points->len >= 2);
#endif
	int rc = 0;
    uint64_t i, npivot, size_l, size_r, n;
    bool found_a, found_b;

    n = points->len * percentage;

    size_l = size_r = 0;
    found_a = found_b = false;
    npivot = points->len / 2;

    if (points->tp == TP_INT)
    {
        int64_t pivot, v, a, b;

        int64_t * arr_l =
        		(int64_t *) malloc(sizeof(int64_t) * 2 * (points->len - 1));

        if (arr_l == NULL)
        {
        	log_critical("Memory allocation error occurred.");
        	rc = -1;
        }
        else
        {
            int64_t * arr_r = arr_l + points->len - 1;
			int64_t * equal = arr_l;
			uint64_t * equal_size = &size_l;

			a = b = 0;
			pivot = points->data[npivot].val.int64;
			for (i = 0; i < points->len; i++)
			{
				if (i == npivot)
				{
					continue;
				}
				v = points->data[i].val.int64;
				if (v == pivot)
				{
					equal[(*equal_size)++] = v;
					if (equal == arr_l)
					{
						equal = arr_r;
						equal_size = &size_r;
					}
					else
					{
						equal = arr_l;
						equal_size = &size_l;
					}
				}
				else if (v > pivot)
				{
					arr_r[size_r++] = v;
				}
				else
				{
					arr_l[size_l++] = v;
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
						arr_l, size_l, arr_r, n, a, b, found_a, found_b) :
				find_median_real_int64(
						arr_r,
						size_r,
						arr_l,
						n - size_l - 1,
						a,
						b,
						found_a,
						found_b);
        }
        free(arr_l);
    }
    else
    {
        double pivot, v, a, b;

        double * arr_l =
        		(double *) malloc(sizeof(double) * 2 * (points->len - 1));

        if (arr_l == NULL)
        {
        	log_critical("Memory allocation error occurred.");
        	rc = -1;
        }
        else
        {
            double * arr_r = arr_l + points->len - 1;
			double * equal = arr_l;
			uint64_t * equal_size = &size_l;

			a = b = 0.0;
			pivot = points->data[npivot].val.real;
			for (i = 0; i < points->len; i++)
			{
				if (i == npivot)
				{
					continue;
				}
				v = points->data[i].val.real;
				if (v == pivot)
				{
					equal[(*equal_size)++] = v;
					if (equal == arr_l)
					{
						equal = arr_r;
						equal_size = &size_r;
					}
					else
					{
						equal = arr_l;
						equal_size = &size_l;
					}
				}
				else if (v > pivot)
				{
					arr_r[size_r++] = v;
				}
				else
				{
					arr_l[size_l++] = v;
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
						arr_l, size_l, arr_r, n, a, b, found_a, found_b) :
				find_median_real_double(
						arr_r,
						size_r,
						arr_l,
						n - size_l - 1,
						a,
						b,
						found_a,
						found_b);
        }
        free(arr_l);
    }
    return rc;
}

static double find_median_real_int64(
        int64_t * seq,
        uint64_t size,
        int64_t * other,
        uint64_t n,
        int64_t a,
        int64_t b,
        bool found_a,
        bool found_b)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    int64_t pivot, v;
    int64_t * equal = seq;
    uint64_t * equal_size = &size_l;
    uint64_t npivot = size / 2;

    pivot = seq[npivot];

    for (i = 0; i < size; i++)
    {
    	if (i == npivot)
    	{
    		continue;
    	}
        v = seq[i];
        if (v == pivot)
        {
            equal[(*equal_size)++] = v;
            if (equal == seq)
            {
                equal = other;
                equal_size = &size_r;
            }
            else
            {
                equal = seq;
                equal_size = &size_l;
            }
        }
        else if (v > pivot)
        {
            other[size_r++] = v;
        }
        else
        {
            seq[size_l++] = v;
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
    {
        return (a + b) / 2.0;
    }

    if ((!found_b && size_l > n) || size_l > n - 1)
    {
        return find_median_real_int64(
                seq, size_l, other, n, a, b, found_a, found_b);
    }

    return find_median_real_int64(
            other, size_r, seq, n - size_l - 1, a, b, found_a, found_b);

}

static double find_median_real_double(
        double * seq,
        uint64_t size,
        double * other,
        uint64_t n,
        double a,
        double b,
        bool found_a,
        bool found_b)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    double pivot, v;
    double * equal = seq;
    uint64_t * equal_size = &size_l;
    uint64_t npivot = size / 2;

    pivot = seq[npivot];

    for (i = 0; i < size; i++)
    {
    	if (i == npivot)
    	{
    		continue;
    	}
        v = seq[i];
        if (v == pivot)
        {
            equal[(*equal_size)++] = v;
            if (equal == seq)
            {
                equal = other;
                equal_size = &size_r;
            }
            else
            {
                equal = seq;
                equal_size = &size_l;
            }
        }
        else if (v > pivot)
        {
            other[size_r++] = v;
        }
        else
        {
            seq[size_l++] = v;
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
    {
        return (a + b) / 2.0;
    }

    if ((!found_b && size_l > n) || size_l > n - 1)
    {
        return find_median_real_double(
                seq, size_l, other, n, a, b, found_a, found_b);
    }

    return find_median_real_double(
            other, size_r, seq, n - size_l - 1, a, b, found_a, found_b);

}

static double find_n_int64(
        int64_t * seq,
        uint64_t size,
        int64_t * other,
        uint64_t n)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    int64_t pivot, v;
    int64_t * equal = seq;
    uint64_t * equal_size = &size_l;
    uint64_t npivot = size / 2;

    pivot = seq[npivot];

    for (i = 0; i < size; i++)
    {
    	if (i == npivot)
    	{
    		continue;
    	}
        v = seq[i];
        if (v == pivot)
        {
            equal[(*equal_size)++] = v;
            if (equal == seq)
            {
                equal = other;
                equal_size = &size_r;
            }
            else
            {
                equal = seq;
                equal_size = &size_l;
            }
        }
        else if (v > pivot)
        {
            other[size_r++] = v;
        }
        else
        {
            seq[size_l++] = v;
        }
    }

    if (size_l == n)
    {
        return pivot;
    }

    if (size_l > n)
    {
        return find_n_int64(seq, size_l, other, n);
    }

    return find_n_int64(other, size_r, seq, n - size_l - 1);
}

static double find_n_double(
        double * seq,
        uint64_t size,
        double * other,
        uint64_t n)
{
    uint64_t i, size_l, size_r;
    size_l = size_r = 0;
    double pivot, v;
    double * equal = seq;
    uint64_t * equal_size = &size_l;
    uint64_t npivot = size / 2;

    pivot = seq[npivot];

    for (i = 0; i < size; i++)
    {
    	if (i == npivot)
    	{
    		continue;
    	}
        v = seq[i];
        if (v == pivot)
        {
            equal[(*equal_size)++] = v;
            if (equal == seq)
            {
                equal = other;
                equal_size = &size_r;
            }
            else
            {
                equal = seq;
                equal_size = &size_l;
            }
        }
        else if (v > pivot)
        {
            other[size_r++] = v;
        }
        else
        {
            seq[size_l++] = v;
        }
    }
    if (size_l == n)
    {
    	return pivot;
    }

    if (size_l > n)
    {
    	return find_n_double(seq, size_l, other, n);
    }

    return find_n_double(other, size_r, seq, n - size_l - 1);
}
