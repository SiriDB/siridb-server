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

int zip_int_val(int64_t * a, int64_t * b, uint64_t * v)
{
    const uint64_t store_raw_mask = UINT64_C(0xff80000000000000);
    uint64_t * diff = (uint64_t *) b;

    if (*a > *b)
    {
        *diff = *a - *b;
        if (*diff & store_raw_mask)
        {
            *b = *a - *diff;
            *v |= store_raw_mask;
            return 1;
        }
        *diff <<= 1;
        *diff |= 1;
    }
    else
    {
        *diff = *b - *a;
        if (*diff & store_raw_mask)
        {
            *b = *diff + *a;
            *v |= store_raw_mask;
            return 1;
        }
        *diff <<= 1;
    }

    *v |= *diff;
    return 0;
}

unsigned char * zip_int(points_t * points, uint16_t * csz, size_t * size)
{

    uint64_t * diff;
    int64_t * a, * b;
    int store_raw_diff = 0;
    uint64_t tdiff = 0;
    uint64_t vdiff = 0;
    unsigned char * bits, *pt;
    uint64_t mask, ts, val;
    size_t i = points->len - 1;
    point_t * point = points->data + i;
    uint8_t vcount = 0;
    uint8_t tcount = 0;
    uint8_t shift = 0;

    i--;
    diff = &point->ts;
    a = &point->val.i;
    point--;
    *diff -= point->ts;
    ts = *diff;

    b = a;
    a = &point->val.i;

    if (zip_int_val(a, b, &vdiff))
    {
        store_raw_diff = points->len - i - 1;
    }

    for (;i--;)
    {
        diff = &point->ts;
        point--;
        *diff -= point->ts;

        tdiff |= ts ^ *diff;

        if (store_raw_diff)
        {
            continue;
        }

        b = a;
        a = &point->val.i;

        if (zip_int_val(a, b, &vdiff))
        {
            store_raw_diff = points->len - i - 1;
        }
    }

    for (i = 1, mask = 0xff; i <= 8; mask <<= 8, i++)
    {
        if (vdiff & mask)
        {
            vcount = i;
        }
        if (tdiff & mask)
        {
            tcount = i;
        }
        else if (ts & mask)
        {
            shift = i;
        }
    }
    shift = (tcount > shift) ? 0 : shift;
    *csz = (uint8_t) store_raw_diff ? 0xff : vcount;
    *csz <<= 8;
    *csz |= tcount | (shift << 4);
    *size = 16 + shift - tcount + (tcount + vcount) * (points->len - 1);
    bits = (unsigned char *) malloc(*size);
    pt = bits;

    memcpy(pt, &point->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(pt, &point->val.u, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (; shift-- > tcount; ++pt)
    {
        *pt = ts >> (shift * 8);
    }

    for (i = points->len; --i;)
    {
        point++;
        ts = point->ts;
        val = point->val.u;

        for (shift = tcount; shift--; ++pt)
        {
            *pt = ts >> (shift * 8);
        }

        if (store_raw_diff)
        {
            if (i < store_raw_diff)
            {
                if (val & 1)
                {
                    val >>= 1;
                    point->val.i = (point-1)->val.i - val;
                }
                else
                {
                    val >>= 1;
                    point->val.i = val + (point-1)->val.i;
                }
                val = point->val.u;
            }

            memcpy(pt, &val, sizeof(uint64_t));
            pt += sizeof(uint64_t);
            continue;
        }

        for (shift = vcount; shift--; ++pt)
        {
            *pt = val >> (shift * 8);
        }
    }

    return bits;
}

unsigned char * zip_double(points_t * points, uint16_t * csz, size_t * size)
{
    uint64_t * diff;
    uint64_t tdiff = 0;
    uint64_t ts, val;
    uint64_t mask = points->data->val.u;
    size_t i = points->len - 1;
    point_t * point = points->data + i;
    uint64_t vdiff = mask ^ point->val.u;
    uint8_t tcount = 0;
    uint8_t vcount = 0;
    uint8_t vstore = 0;
    uint8_t shift = 0;
    int vshift[raw_values_threshold];
    unsigned char * bits, *pt;
    int * pshift;

    i--;
    diff = &point->ts;
    point--;
    *diff -= point->ts;
    ts = *diff;

    while (i--)
    {
        vdiff |= mask ^ point->val.u;
        diff = &point->ts;
        point--;
        *diff -= point->ts;
        tdiff |= ts ^ *diff;
    }

    for (i = 0, mask = 0xff; i < 8; mask <<= 8, i++)
    {
        if (vdiff & mask)
        {
            vstore |= 1 << i;
            vshift[vcount++] = i * 8;
        }
        if (tdiff & mask)
        {
            tcount = i + 1;
        }
        else if (ts & mask)
        {
            shift = i + 1;
        }
    }
    shift = (tcount > shift) ? 0 : shift;
    *csz = vstore;
    *csz <<= 8;
    *csz |= tcount | (shift << 4);
    *size = 16 + shift - tcount + (tcount + vcount) * (points->len - 1);
    bits = (unsigned char *) malloc(*size);
    pt = bits;

    memcpy(pt, &point->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(pt, &point->val.u, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (; shift-- > tcount; ++pt)
    {
        *pt = ts >> (shift * 8);
    }

    for (i = points->len; --i;)
    {
        point++;
        ts = point->ts;
        for (shift = tcount; shift--; ++pt)
        {
            *pt = ts >> (shift * 8);
        }
        val = point->val.u;
        if (vcount <= raw_values_threshold)
        {
            for (pshift = vshift, shift = vcount; shift--; ++pshift, ++pt)
            {
                *pt = val >> *pshift;
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
    point_t * point;
    uint8_t vcount, tcount, tshift;
    uint64_t ts, tmp, mask;
    int64_t val;
    size_t i, j;
    unsigned char * pt = bits;

    vcount = csz >> 8;
    tcount = csz & 0xf;
    tshift = csz & 0xf0;
    tshift >>= 4;

    points = points_new(len, TP_INT);
    point = points->data;

    memcpy(&point->ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&point->val.u, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (mask = 0; tshift-- > tcount; ++pt)
    {
        mask |= *pt << (tshift * 8);
    }

    ts = point->ts;
    val = point->val.i;

    for (i = len; --i;)
    {
        point++;
        for (tmp = 0, j = 0; j < tcount; ++j, ++pt)
        {
            tmp <<= 8;
            tmp |= *pt;
        }
        ts += tmp | mask;
        point->ts = ts;

        if (vcount != 0xff)
        {
            for (tmp = 0, j = 0; j < vcount; ++j, ++pt)
            {
                tmp <<= 8;
                tmp |= *pt;
            }
            val += (tmp & 1) ? -(tmp >> 1) : (tmp >> 1);
            point->val.i = val;
        }
        else
        {
            memcpy(&point->val.u, pt, sizeof(uint64_t));
            pt += sizeof(uint64_t);
        }
    }

    return points;
}

points_t * unzip_double(unsigned char * bits, uint16_t len, uint16_t csz)
{
    points_t * points;
    int vshift[raw_values_threshold];
    int * pshift;
    uint8_t vstore, tcount, tshift;
    size_t i, c, j;
    unsigned char * pt = bits;
    uint64_t ts, tmp, mask, val;
    point_t * point;

    vstore = csz >> 8;
    tcount = csz & 0xf;
    tshift = csz & 0xf0;
    tshift >>= 4;

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
    point = points->data;

    memcpy(&point->ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&point->val.u, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    ts = point->ts;
    val = point->val.u & ~mask;

    for (mask = 0; tshift-- > tcount; ++pt)
    {
        mask |= *pt << (tshift * 8);
    }

    for (i = len; --i;)
    {
        point++;
        for (tmp = 0, j = tcount; j--; ++pt)
        {
            tmp <<= 8;
            tmp |= *pt;
        }
        ts += mask | tmp;
        point->ts = ts;

        if (c <= raw_values_threshold)
        {
            for (tmp = 0, pshift = vshift, j = c; j--; ++pshift, ++pt)
            {
                tmp |= ((uint64_t) *pt) << *pshift;
            }
            tmp |= val;
            point->val.u = tmp;
        }
        else
        {
            memcpy(&point->val.u, pt, sizeof(uint64_t));
            pt += sizeof(uint64_t);
        }
    }

    return points;
}

void test_int()
{
    size_t sz = 10;
    points_t * points = points_new(sz, TP_INT);
    uint64_t ts = 1511797596;
    for (int i = 0; i < sz; i++)
    {
        int r = rand() % 60;
        ts += 300 + r;
        cast_t cf;
        cf.i = 1 + (r - 30) * r;
        // if (r > 30)
        // {
        //     cf.i = -922337203685477507;
        // }
        points_add_point(points, &ts, &cf);
    }

    uint16_t csz;
    size_t size;
    unsigned char * bits;
    points_t * upoints;

    bits = zip_int(points_copy(points), &csz, &size);
    upoints = unzip_int(bits, points->len, csz);

    printf("size: %lu (uncompressed: %d)\n", size, points->len * 12);
    free(bits);

    for (size_t i = 0; i < points->len; i++)
    {
        // printf("%lu - %lu\n", points->data[i].ts, upoints->data[i].ts);
        // printf("%lu - %lu\n", points->data[i].val.u, upoints->data[i].val.u);
        assert(points->data[i].ts == upoints->data[i].ts);
        assert(points->data[i].val.u == upoints->data[i].val.u);
    }

    points_destroy(points);
    points_destroy(upoints);

    printf("Finished int test\n");
}

void test_double()
{
    size_t sz = 200;
    points_t * points = points_new(sz, TP_DOUBLE);
    uint64_t ts = 1511797596;
    for (int i = 0; i < sz; i++)
    {
        int r = rand() % 60;
        ts += 300 + r;
        cast_t cf;
        cf.d = 1.0 + 100 * i;
        // if (r > 30)
        // {
        //     cf.d = -922337203685477507;
        // }

        points_add_point(points, &ts, &cf);
    }

    uint16_t csz;
    size_t size;
    unsigned char * bits;
    points_t * upoints;

    bits = zip_double(points_copy(points), &csz, &size);
    upoints = unzip_double(bits, points->len, csz);

    printf("size: %lu (uncompressed: %d)\n", size, points->len * 12);
    free(bits);

    for (size_t i = 0; i < points->len; i++)
    {
        // printf("%lu - %lu\n", points->data[i].ts, upoints->data[i].ts);
        // printf("%lu - %lu\n", points->data[i].val.u, upoints->data[i].val.u);
        assert(points->data[i].ts == upoints->data[i].ts);
        assert(points->data[i].val.u == upoints->data[i].val.u);
    }

    points_destroy(points);
    points_destroy(upoints);


    printf("Finished double test\n");
}

int main()
{
    srand(time(NULL));
    test_int();
    test_double();
}
