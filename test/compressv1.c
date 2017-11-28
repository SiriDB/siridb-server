#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>


#define TP_INT 0
#define TP_DOUBLE 1


typedef union
{
    double d;
    int64_t i;
    uint64_t u;
} cast_t;

typedef struct
{
    uint64_t ts;
    cast_t val;
} point_t;

typedef struct
{
    uint8_t tp;
    uint16_t len;
    size_t size;
    point_t * data;
} points_t;


points_t * points_new(size_t size, uint8_t tp)
{
    points_t * points = malloc(sizeof(points_t));
    points->len = 0;
    points->tp = tp;
    points->data = malloc(sizeof(point_t) * size);
    return points;
}

void points_destroy(points_t * points)
{
    free(points->data);
    free(points);
}

void points_add_point(
        points_t *__restrict points,
        uint64_t * ts,
        cast_t * val)
{
    size_t i;
    point_t * point;

    for (   i = points->len;
            i-- > 0 && (points->data + i)->ts > *ts;
            *(points->data + i + 1) = *(points->data + i));

    points->len++;
    point = points->data + i + 1;
    point->ts = *ts;
    point->val = *val;
}



unsigned char * compress(points_t * points)
{
    uint64_t start = points->data[0].ts;
    uint64_t * diff;
    int64_t * a;
    int64_t * b;
    uint64_t tdiff = 0;
    uint64_t vdiff = 0;
    uint8_t flip;
    uint8_t tshift;
    uint8_t vshift;

    size_t store_raw_diff = 0;
    const uint64_t store_raw_mask = UINT64_C(0xff80000000000000);

    for (size_t i = points->len; --i;)
    {
        diff = &points->data[i].ts;
        a = &points->data[i-1].val.i;
        b = &points->data[i].val.i;

        *diff -= points->data[i-1].ts;
        if (*diff > tdiff)
        {
            tdiff = *diff;
        }

        if (store_raw_diff)
        {
            continue;
        }

        diff = &points->data[i].val.u;

        if (*a > *b)
        {
            *diff = *a - *b;
            flip = 1;
        }
        else
        {
            *diff = *b - *a;
            flip = 0;
        }

        if (*diff & store_raw_mask)
        {
            store_raw_diff = i;
        }
        else
        {
            *diff <<= 1;
            *diff |= flip;
        }

        if (*diff > vdiff)
        {
            vdiff = *diff;
        }
    }

    printf("tdiff = %llu\n", tdiff);
    printf("vdiff = %llu\n", vdiff);

    for (tshift = 56; (tdiff << tshift) >> tshift != tdiff; tshift -= 8);
    if (vdiff)
    {
        for (vshift = 56; (vdiff << vshift) >> vshift != vdiff; vshift -= 8);
    }
    else
    {
        vshift = 64;
    }

    printf("store_raw_diff = %zu\n", store_raw_diff);
    printf("tshift = %u\n", tshift);
    printf("vshift = %u\n", vshift);

    return NULL;
}

void uncompress(unsigned char * c)
{


}


int main()
{
    size_t sz = 10;
    points_t * points = points_new(sz, TP_DOUBLE);
    for (int i = 0; i < sz; i++)
    {
        int r = rand() % 60;
        uint64_t ts = 1511797596 + i*300 + r;
        cast_t cf;
        cf.i = 40;

        points_add_point(points, &ts, &cf);
    }

    uint8_t a, b, c;
    double f = 1.0;

    a = 1;
    b = 126;
    int store_raw_diff = -1;
    uint8_t flip;
    if (a > b)
    {
        c = a - b;
        flip = 1;
    }
    else
    {
        c = b - a;
        flip = 0;
    }

    if (c & (1<<7))
    {
        store_raw_diff = 0;
    }
    else
    {
        c <<= 1;
        c |= flip;
    }

    if (c & 1)
    {
        b = a - (c >> 1);
    }
    else
    {
        b = a + (c >> 1);
    }


    printf("c = %u\n", c);
    printf("b = %u\n", b);


    compress(points);

    points_destroy(points);


    printf("Finished\n");
}
