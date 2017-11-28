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
    uint64_t diff;
    uint64_t mdiff = 0;
    uint64_t mask = 0xffffffffffffffff & points->data[i].val.u;


    for (uint16_t i = 1; i < points->len; i++)
    {
        diff = points->data[i].ts - points->data[i-1].ts;
        if (diff > mdiff)
        {
            mdiff = diff;
        }


    }
    printf("mdiff = %llu\n", mdiff);

    uint8_t shift;
    for (shift = 56; (mdiff << shift) >> shift != mdiff; shift -= 8);

    printf("shift = %u\n", shift);





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
        cf.d = 0.0;

        points_add_point(points, &ts, &cf);
    }

    compress(points);

    points_destroy(points);


    printf("Finished\n");
}
