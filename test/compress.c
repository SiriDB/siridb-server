#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

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

points_t * points_copy(points_t * points)
{
    points_t * p = malloc(sizeof(points_t));

    p->len =  points->len;
    p->tp =  points->tp;
    p->data = malloc(sizeof(point_t) * points->len);
    memcpy(p->data, points->data, sizeof(point_t) * points->len);
    return p;
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


const int raw_values_threshold = 7;

unsigned char * zip_int(points_t * points, uint16_t * csz, size_t * size)
{

    const uint64_t store_raw_mask = UINT64_C(0xff80000000000000);
    uint64_t * diff;
    int64_t * a, * b;
    size_t store_raw_diff = 0;
    uint64_t tdiff = 0;
    uint64_t vdiff = 0;
    unsigned char * bits, *pt;
    uint64_t mask, ts, val;
    size_t i;
    int vcount = 0;
    int tcount = 0;
    int shift, bcount;

    for (i = points->len; --i;)
    {
        diff = &points->data[i].ts;
        *diff -= points->data[i-1].ts;

        a = &points->data[i-1].val.i;
        b = &points->data[i].val.i;

        tdiff |= *diff;

        if (store_raw_diff)
        {
            continue;
        }

        diff = &points->data[i].val.u;

        if (*a > *b)
        {
            *diff = *a - *b;
            if (*diff & store_raw_mask)
            {
                store_raw_diff = i;
                *b = *a - *diff;
                vdiff |= store_raw_mask;
                continue;
            }
            *diff <<= 1;
            *diff |= 1;
        }
        else
        {
            *diff = *b - *a;
            if (*diff & store_raw_mask)
            {
                store_raw_diff = i;
                *b = *diff + *a;
                vdiff |= store_raw_mask;
                continue;
            }
            *diff <<= 1;
        }

        vdiff |= *diff;
    }

    for (i = 1, mask = 0xff; i <= 8; mask <<= 8, i++)
    {
        if (tdiff & mask)
        {
            tcount = i;
        }
        if (vdiff & mask)
        {
            vcount = i;
        }
    }

    *csz = (uint8_t) store_raw_diff ? 0xff : vcount;
    *csz <<= 8;
    *csz |= (uint8_t) tcount;

    printf("tcount = %d\n", tcount);
    printf("vcount = %d\n", vcount);
    printf("store_raw_diff = %zu\n", store_raw_diff);

    bcount = tcount + vcount;
    *size = 16 + bcount * (points->len - 1);
    bits = (unsigned char *) malloc(*size);
    pt = bits;

    memcpy(pt, &points->data->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(pt, &points->data->val.u, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    tcount *= 8;
    vcount *= 8;

    for (i = 1; i < points->len; i++)
    {
        ts = points->data[i].ts;
        val = points->data[i].val.u;

        for (shift = tcount; (shift -= 8) >= 0; ++pt)
        {
            *pt = ts >> shift;
        }

        if (!store_raw_diff)
        {
            for (shift = vcount; (shift -= 8) >= 0; ++pt)
            {
                *pt = val >> shift;
            }
            continue;
        }

        if (i > store_raw_diff)
        {
            if (val & 1)
            {
                val >>= 1;
                points->data[i].val.i = points->data[i-1].val.i - val;
            }
            else
            {
                val >>= 1;
                points->data[i].val.i = val + points->data[i-1].val.i;
            }
        }

        memcpy(pt, &val, sizeof(uint64_t));
        pt += sizeof(uint64_t);
    }

    return bits;
}

unsigned char * zip_double(points_t * points, uint16_t * csz, size_t * size)
{
    uint64_t * diff;
    uint64_t tdiff = 0;
    uint64_t mask = points->data->val.u;
    uint64_t vdiff = 0;
    uint8_t vstore = 0;
    uint64_t ts, val;
    size_t i, j;
    int tcount = 0;
    int vcount = 0;
    int shift, bcount;
    int vshift[raw_values_threshold];
    unsigned char * bits, *pt;

    for (i = points->len; --i;)
    {
        diff = &points->data[i].ts;
        *diff -= points->data[i-1].ts;
        tdiff |= *diff;
        vdiff |= (mask ^ points->data[i].val.u);
    }


    for (i = 0, mask = 0xff; i < 8; mask <<= 8, i++)
    {
        if (vdiff & mask)
        {
            vstore |= 1 << i;
            vshift[vcount] = i * 8;
            vcount += 1;
        }
        if (tdiff & mask)
        {
            tcount = i + 1;
        }
    }

    *csz = vstore;
    *csz <<= 8;
    *csz |= (uint8_t) tcount;

    bcount = tcount + vcount;
    *size = 16 + bcount * (points->len - 1);
    bits = (unsigned char *) malloc(*size);
    pt = bits;

    memcpy(pt, &points->data->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(pt, &points->data->val.u, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    tcount *= 8;

    for (i = 1; i < points->len; i++)
    {
        ts = points->data[i].ts;
        val = points->data[i].val.u;
        for (shift = tcount; (shift -= 8) >= 0; ++pt)
        {
            *pt = ts >> shift;
        }
        if (vcount < raw_values_threshold)
        {
            for (shift = 0; shift < vcount; ++shift, ++pt)
            {
                *pt = val >> vshift[shift];
            }
        }
        else
        {
            /* store raw values */
            memcpy(pt, &val, sizeof(uint64_t));
            pt += sizeof(uint64_t);
        }
    }

    return bits;
}

points_t * unzip_int(unsigned char * bits, uint16_t len, uint16_t csz)
{
    points_t * points;
    uint8_t vcount, tcount;
    uint64_t ts, tmp;
    int64_t val;
    size_t i, j;
    unsigned char * pt = bits;

    vcount = csz >> 8;
    tcount = csz;


    printf("tcount = %u\n", tcount);
    printf("vcount = %u\n", vcount);

    points = points_new(len, TP_INT);

    memcpy(&ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&tmp, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    points->data->ts = ts;
    points->data->val.u = tmp;
    val = points->data->val.i;


    for (i = 1; i < len; i++)
    {
        for (tmp = 0, j = 0; j < tcount; ++j, ++pt)
        {
            tmp |= *pt;
            tmp << 8;
        }
        ts += tmp;
        printf("ts: %lu\n", ts);
        points->data[i].ts = ts;

        if (vcount < 0xff)
        {
            for (tmp = 0, j = 0; j < vcount; ++j, ++pt)
            {
                tmp |= *pt;
                tmp << 8;
            }
            val += (tmp & 1) ? -(tmp >> 1) : (tmp >> 1);
            points->data[i].val.i = val;
        }
        else
        {
            printf("bla...");
            memcpy(&points->data[i].val.u, pt, sizeof(uint64_t));
            pt += sizeof(uint64_t);
        }

        printf("int: %ld\n", points->data[i].val.i);
    }

    return points;
}

points_t * unzip_double(unsigned char * bits, uint16_t len, uint16_t csz)
{
    points_t * points;
    int vshift[raw_values_threshold];
    uint8_t vstore, tcount;
    size_t i, c, j;
    unsigned char * pt = bits;
    uint64_t ts, tmp, mask, val;

    vstore = csz >> 8;
    tcount = csz;

    for (   i = 0, c = 0, mask = 0, tmp = 0xff;
            vstore;
            vstore >>= 1, ++i, tmp <<= 8)
    {
        if (vstore & 1)
        {
            vshift[c++] = i * 8;
            mask |= tmp;
        }
    }

    points = points_new(len, TP_DOUBLE);

    memcpy(&ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&val, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    points->data->ts = ts;
    points->data->val.u = val;

    val &= ^mask;

    printf("tcount: %u\n", tcount);
    printf("val: %u\n", val);
    printf("vstore: %u\n", vstore);

    for (i = 1; i < len; i++)
    {
        for (tmp = 0, j = 0; j < tcount; ++j, ++pt)
        {
            printf("pt val: %u\n", *pt);
            tmp <<= 8;
            tmp |= *pt;
        }
        printf("tmp: %lu\n", tmp);
        ts += tmp;
        printf("ts: %lu\n", ts);
        points->data[i].ts = ts;
        if (c < raw_values_threshold)
        {
            for (tmp = 0, j = 0; j < c; ++j, ++pt)
            {
                tmp |= ((uint64_t) *pt) << vshift[j];
            }
            tmp |= val;
            points->data[i].val.u = tmp;
        }
        else
        {
            memcpy(&points->data[i].val.u, pt, sizeof(uint64_t));
            pt += sizeof(uint64_t);
        }
        printf("double: %f\n", points->data[i].val.d);
    }

    return points;
}


int main()
{
    // srand(time(NULL));
    size_t sz = 10;
    points_t * points = points_new(sz, TP_DOUBLE);
    for (int i = 0; i < sz; i++)
    {
        int r = rand() % 60;
        uint64_t ts = 1511797596 + i*300 + r;
        cast_t cf;
        cf.d = 1.0;
        // if (r > 30)
        // {
        //     cf.d = 9223372036854775807;
        // }

        points_add_point(points, &ts, &cf);
    }

    uint16_t csz;
    size_t size;
    unsigned char * bits;
    points_t * upoints;

    if (points->tp == TP_DOUBLE)
    {
        bits = zip_double(points_copy(points), &csz, &size);
        upoints = unzip_double(bits, points->len, csz);
    }
    else
    {
        bits = zip_int(points_copy(points), &csz, &size);
        upoints = unzip_int(bits, points->len, csz);
    }

    printf("size: %lu\n", size);
    free(bits);

    for (size_t i = 0; i < points->len; i++)
    {
        printf("%lu - %lu\n", points->data[i].ts, upoints->data[i].ts);
        printf("%lu - %lu\n", points->data[i].val.u, upoints->data[i].val.u);
        assert(points->data[i].ts == upoints->data[i].ts);
        assert(points->data[i].val.u == upoints->data[i].val.u);
    }


    points_destroy(points);
    points_destroy(upoints);


    printf("Finished\n");
}
