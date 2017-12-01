#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define TP_INT 0
#define TP_DOUBLE 1
#define RAW_VALUES_THRESHOLD 7


typedef union
{
    double d;
    int64_t int64;
    uint64_t uint64;
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


unsigned char * zip_int(
    points_t * points,
    uint_fast32_t start,
    uint_fast32_t end,
    uint16_t * cinfo,
    size_t * size)
{
    const uint64_t store_raw_mask = UINT64_C(0x8000000000000000);
    uint64_t tdiff = 0;
    uint64_t vdiff = 0;
    unsigned char * bits, *pt;
    uint64_t mask, val;
    size_t i = end - 2;
    point_t * point = points->data + i;
    uint64_t ts = (point + 1)->ts - point->ts;
    uint8_t vcount = 0;
    uint8_t tcount = 0;
    uint8_t shift = 0;
    int64_t a, b;

    a = (point - 1)->val.int64;
    b = point->val.int64;

    vdiff |= (uint64_t) (a > b) ? a - b : b - a;

    while (i-- > start)
    {
        point--;
        tdiff |= ts ^ ((point + 1)->ts - point->ts);
        a = b;
        b = point->val.int64;
        vdiff |= (uint64_t) (a > b) ? a - b : b - a;
    }

    vdiff <<= !(vdiff & store_raw_mask);

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

    switch (vcount)
    {
        case 1: *cinfo = 0x80; break;
        case 2: *cinfo = 0xc0; break;
        case 3: *cinfo = 0xe0; break;
        case 4: *cinfo = 0xf0; break;
        case 5: *cinfo = 0xf8; break;
        case 6: *cinfo = 0xfc; break;
        case 7: *cinfo = 0xfe; break;
        case 8: *cinfo = 0xff; break;
    }

    shift = (tcount > shift) ? 0 : shift;
    *cinfo <<= 8;
    *cinfo |= tcount | (shift << 4);
    *size = 16 + shift - tcount + (tcount + vcount) * (end - start - 1);
    bits = (unsigned char *) malloc(*size);
    if (bits == NULL)
    {
        return NULL;
    }
    pt = bits;

    memcpy(pt, &point->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(pt, &point->val.uint64, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (; shift-- > tcount; ++pt)
    {
        *pt = ts >> (shift * 8);
    }

    for (i = end, b = point->val.int64; --i > start;)
    {
        point++;
        ts = point->ts - (point - 1)->ts;

        for (shift = tcount; shift--; ++pt)
        {
            *pt = ts >> (shift * 8);
        }

        if (vcount == 8)
        {
            memcpy(pt, &point->val.uint64, sizeof(uint64_t));
            pt += sizeof(uint64_t);
            continue;
        }

        a = b;
        b = point->val.int64;

        if (a > b)
        {
            val = a - b;
            val <<= 1;
            val |= 1;
        }
        else
        {
            val = b - a;
            val <<= 1;
        };

        for (shift = vcount; shift--; ++pt)
        {
            *pt = (val >> (shift * 8)) & 0xff;
        }
    }

    return bits;
}

unsigned char * zip_double(
    points_t * points,
    uint_fast32_t start,
    uint_fast32_t end,
    uint16_t * cinfo,
    size_t * size)
{
    uint64_t tdiff = 0;
    uint64_t val;
    uint64_t mask = points->data->val.uint64;
    size_t i = end - 2;
    point_t * point = points->data + i;
    uint64_t ts = (point + 1)->ts - point->ts;
    uint64_t vdiff = mask ^ point->val.uint64;
    uint8_t tcount = 0;
    uint8_t vcount = 0;
    uint8_t vstore = 0;
    uint8_t shift = 0;
    int vshift[RAW_VALUES_THRESHOLD];
    unsigned char * bits, *pt;
    int * pshift;

    while (i-- > start)
    {
        vdiff |= mask ^ point->val.uint64;
        point--;
        tdiff |= ts ^ ((point + 1)->ts - point->ts);
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
    *cinfo = vstore;
    *cinfo <<= 8;
    *cinfo |= tcount | (shift << 4);
    *size = 16 + shift - tcount + (tcount + vcount) * (end - start - 1);
    bits = (unsigned char *) malloc(*size);
    if (bits == NULL)
    {
        return NULL;
    }
    pt = bits;

    memcpy(pt, &point->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(pt, &point->val.uint64, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (; shift-- > tcount; ++pt)
    {
        *pt = ts >> (shift * 8);
    }

    for (i = end; --i > start;)
    {
        point++;
        ts = point->ts - (point - 1)->ts;
        for (shift = tcount; shift--; ++pt)
        {
            *pt = ts >> (shift * 8);
        }

        val = point->val.uint64;

        if (vcount > RAW_VALUES_THRESHOLD)
        {
            memcpy(pt, &val, sizeof(uint64_t));
            pt += sizeof(uint64_t);
            continue;
        }

        for (pshift = vshift, shift = vcount; shift--; ++pshift, ++pt)
        {
            *pt = val >> *pshift;
        }
    }

    return bits;
}

size_t get_size_zipped(uint16_t cinfo, uint16_t len)
{
    uint8_t vcount = 0;
    uint8_t vstore = cinfo >> 8;
    uint8_t tcount = cinfo & 0xf;
    uint8_t tshift = cinfo & 0xf0;
    tshift >>= 4;

    for (; vstore; vstore &= vstore-1)
    {
        vcount++;
    }

    return 16 + tshift - tcount + (tcount + vcount) * (len - 1);
}

points_t * unzip_int(unsigned char * bits, uint16_t len, uint16_t cinfo)
{
    points_t * points;
    point_t * point;
    uint8_t vstore, tcount, tshift, j;
    uint64_t ts, tmp, mask;
    int64_t val;
    size_t i;
    unsigned char * pt = bits;

    vstore = cinfo >> 8;
    tcount = cinfo & 0xf;
    tshift = cinfo & 0xf0;
    tshift >>= 4;

    points = points_new(len, TP_INT);
    point = points->data;

    memcpy(&point->ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&point->val.uint64, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (mask = 0; tshift-- > tcount; ++pt)
    {
        mask |= *pt << (tshift * 8);
    }

    ts = point->ts;
    val = point->val.int64;

    for (i = len; --i;)
    {
        point++;
        for (tmp = 0, j = tcount; j--; ++pt)
        {
            tmp <<= 8;
            tmp |= *pt;
        }
        ts += tmp | mask;
        point->ts = ts;

        if (vstore != 0xff)
        {
            for (tmp = 0, j = vstore; j; j <<= 1, ++pt)
            {
                tmp <<= 8;
                tmp |= *pt;
            }
            val += (tmp & 1) ? -(tmp >> 1) : (tmp >> 1);
            point->val.int64 = val;
        }
        else
        {
            memcpy(&point->val.uint64, pt, sizeof(uint64_t));
            pt += sizeof(uint64_t);
        }
    }

    return points;
}


points_t * unzip_double(unsigned char * bits, uint16_t len, uint16_t cinfo)
{
    points_t * points;
    int vshift[RAW_VALUES_THRESHOLD];
    int * pshift;
    uint8_t vstore, tcount, tshift;
    size_t i, c, j;
    unsigned char * pt = bits;
    uint64_t ts, tmp, mask, val;
    point_t * point;

    vstore = cinfo >> 8;
    tcount = cinfo & 0xf;
    tshift = cinfo & 0xf0;
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
    memcpy(&point->val.uint64, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    ts = point->ts;
    val = point->val.uint64 & ~mask;

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

        if (c > RAW_VALUES_THRESHOLD)
        {
            memcpy(&point->val.uint64, pt, sizeof(uint64_t));
            pt += sizeof(uint64_t);
            continue;
        }

        for (tmp = 0, pshift = vshift, j = c; j--; ++pshift, ++pt)
        {
            tmp |= ((uint64_t) *pt) << *pshift;
        }
        tmp |= val;
        point->val.uint64 = tmp;
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
        cf.int64 = 1 + i + (30 - r);
        // if (r < 5)
        // {
        //     cf.int64 = 922337203685477507;
        // }
        points_add_point(points, &ts, &cf);
    }

    uint16_t cinfo;
    size_t size, tsize;
    unsigned char * bits;
    points_t * upoints;
    uint_fast32_t start = 2;
    uint_fast32_t end = points->len - 5;
    uint_fast32_t len = end - start;

    bits = zip_int(points, start, end, &cinfo, &size);
    upoints = unzip_int(bits, len, cinfo);

    printf("size: %lu (uncompressed: %lu)\n", size, len * 12);
    free(bits);

    for (size_t i = start; i < end; i++)
    {
        // printf("%lu - %lu\n", points->data[i].ts, upoints->data[i].ts);
        // printf("%lu - %lu\n", points->data[i].val.uint64, upoints->data[i].val.uint64);
        assert(points->data[i].ts == upoints->data[i-start].ts);
        assert(points->data[i].val.uint64 == upoints->data[i-start].val.uint64);
    }

    tsize = get_size_zipped(cinfo, len);
    // printf("size: %lu  get_size: %lu\n", size, tsize);
    assert(size == tsize);

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
        cf.d = 1.0;
        // if (r > 30)
        // {
        //     cf.d = -922337203685477507;
        // }

        points_add_point(points, &ts, &cf);
    }

    uint16_t cinfo;
    size_t size, tsize;
    unsigned char * bits;
    points_t * upoints;
    uint_fast32_t start = 10;
    uint_fast32_t end = points->len - 10;
    uint_fast32_t len = end - start;

    bits = zip_double(points, start, end, &cinfo, &size);
    upoints = unzip_double(bits, len, cinfo);

    printf("size: %lu (uncompressed: %lu)\n", size, len * 12);
    free(bits);

    for (size_t i = start; i < end; i++)
    {
        // printf("%lu - %lu\n", points->data[i].ts, upoints->data[i].ts);
        // printf("%lu - %lu\n", points->data[i].val.uint64, upoints->data[i].val.uint64);
        assert(points->data[i].ts == upoints->data[i-start].ts);
        assert(points->data[i].val.uint64 == upoints->data[i-start].val.uint64);
    }

    tsize = get_size_zipped(cinfo, len);
    // printf("size: %lu  get_size: %lu\n", size, tsize);
    assert(size == tsize);

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
