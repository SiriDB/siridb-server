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
#include <unistd.h>
#include <string.h>

#define MAX_ITERATE_MERGE_COUNT 1000
#define POINTS_MAX_QSORT 250000
#define RAW_VALUES_THRESHOLD 7
#define ZIP_THRESHOLD 5

static unsigned char * POINTS_zip_raw(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size);
static void POINTS_unzip_raw(
        siridb_points_t * points,
        unsigned char * bits,
        uint16_t len,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
static void POINTS_sort_while_merge(slist_t * plist, siridb_points_t * points);
static void POINTS_merge_and_sort(slist_t * plist, siridb_points_t * points);
static void POINTS_simple_sort(siridb_points_t * points);
static inline int POINTS_compare(const void * a, const void * b);
static void POINTS_highest_and_merge(slist_t * plist, siridb_points_t * points);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_points_t * siridb_points_new(size_t size, points_tp tp)
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
        points->content = NULL;
        points->data =
                (siridb_point_t *) malloc(sizeof(siridb_point_t) * size);
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
 * Returns a copy of points or NULL in case of an error. NULL is also returned
 * if points is NULL.
 */
siridb_points_t * siridb_points_copy(siridb_points_t * points)
{
    if (points == NULL)
    {
        return NULL;
    }
    siridb_points_t * cpoints =
            (siridb_points_t *) malloc(sizeof(siridb_points_t));
    if (cpoints != NULL)
    {
        size_t sz = sizeof(siridb_point_t) * points->len;
        cpoints->len = points->len;
        cpoints->tp = points->tp;
        cpoints->content = NULL;
        cpoints->data = (siridb_point_t *) malloc(sz);
        if (cpoints->data == NULL)
        {
            free(cpoints);
            cpoints = NULL;
        }
        else
        {
            memcpy(cpoints->data, points->data, sz);
        }
    }
    return cpoints;
}

/*
 * Destroy points. (parsing NULL is NOT allowed)
 */
void siridb_points_free(siridb_points_t * points)
{
    free(points->content);
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
        siridb_points_t *__restrict points,
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
        siridb_point_t * point = points->data;
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

void siridb_points_ts_correction(siridb_points_t * points, double factor)
{
    siridb_point_t * point = points->data;
    for (size_t i = 0; i < points->len; i++, point++)
    {
        point->ts *= factor;
    }
}

/*
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
int siridb_points_raw_pack(siridb_points_t * points, qp_packer_t * packer)
{
    return -(qp_add_type(packer, QP_ARRAY_OPEN) ||
            qp_add_int8(packer, points->tp) ||
            qp_add_int32(packer, points->len) ||
            qp_add_raw(
                packer,
                (unsigned char *) points->data,
                points->len * sizeof(siridb_point_t)) ||
            qp_add_type(packer, QP_ARRAY_CLOSE));
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
        usleep(1000);

        /*
         * When both series from type double and type integer are merged
         * we need to promote the integer series to double.
         */
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
                    usleep(100);
                }
            }
        }

        points->len = n;

        /*
         * Select a merge method depending on the ratio series and points.
         */
        if (plist->len < 4)
        {
            POINTS_sort_while_merge(plist, points);
        }
        else if (n == plist->len || n < POINTS_MAX_QSORT)
        {
            POINTS_merge_and_sort(plist, points);
        }
        else
        {
            POINTS_highest_and_merge(plist, points);
        }
    }
    return points;
}

/*
 * Return NULL in case an error has occurred. This function destroys the
 * original points.
 */
unsigned char * siridb_points_zip_int(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size)
{
    if (end - start < ZIP_THRESHOLD)
    {
        return POINTS_zip_raw(points, start, end, cinfo, size);
    }
    const uint64_t store_raw_mask = UINT64_C(0x8000000000000000);
    uint64_t tdiff = 0;
    uint64_t vdiff = 0;
    unsigned char * bits, *pt;
    uint64_t mask, val;
    size_t i = end - 2;
    siridb_point_t * point = points->data + i;
    uint64_t ts = (point + 1)->ts - point->ts;
    uint8_t vcount = 0;
    uint8_t tcount = 0;
    uint8_t shift = 0;
    int64_t a, b;

    a = (point + 1)->val.int64;
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

    shift = (tcount > shift) ? tcount : shift;
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

/*
 * Return NULL in case an error has occurred. This function destroys the
 * original points.
 */
unsigned char * siridb_points_zip_double(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size)
{
    if (end - start < ZIP_THRESHOLD)
    {
        return POINTS_zip_raw(points, start, end, cinfo, size);
    }
    uint64_t tdiff = 0;
    uint64_t val;
    size_t i = end - 2;
    siridb_point_t * point = points->data + i;
    uint64_t ts = (point + 1)->ts - point->ts;
    uint64_t mask = (point + 1)->val.uint64;
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
        point--;
        vdiff |= mask ^ point->val.uint64;
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
    shift = (tcount > shift) ? tcount : shift;
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

void siridb_points_unzip_int(
        siridb_points_t * points,
        unsigned char * bits,
        uint16_t len,
        uint16_t cinfo,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    if (len < ZIP_THRESHOLD)
    {
        return POINTS_unzip_raw(
                points, bits, len, start_ts, end_ts, has_overlap);
    }
    uint8_t vstore, tcount, tshift, j;
    uint64_t ts, tmp, mask;
    int64_t val;
    size_t i;
    unsigned char * pt = bits;
    siridb_point_t * point = points->data + points->len;

    vstore = cinfo >> 8;
    tcount = cinfo & 0xf;
    tshift = cinfo & 0xf0;
    tshift >>= 4;

    memcpy(&point->ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&point->val.uint64, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    for (mask = 0; tshift-- > tcount; ++pt)
    {
        mask |= ((uint64_t) *pt) << (tshift * 8);
    }

    ts = point->ts;
    val = point->val.int64;

    for (i = len; (end_ts == NULL || ts < *end_ts) && --i; )
    {
        if (start_ts != NULL && ts < *start_ts)
        {
            --len;
        }
        else
        {
            ++point;
        }

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

    if (has_overlap && points->len)
    {
        qp_via_t v;
        point = points->data + points->len;
        for (i = len - i; i--; ++point)
        {
            ts = point->ts;
            v = point->val;
            siridb_points_add_point(points, &ts, &v);
        }
    }
    else
    {
        points->len += len - i;
    }
}

void siridb_points_unzip_double(
        siridb_points_t * points,
        unsigned char * bits,
        uint16_t len,
        uint16_t cinfo,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    if (len < ZIP_THRESHOLD)
    {
        return POINTS_unzip_raw(
                points, bits, len, start_ts, end_ts, has_overlap);
    }
    int vshift[RAW_VALUES_THRESHOLD];
    int * pshift;
    uint8_t vstore, tcount, tshift;
    size_t i, c, j;
    unsigned char * pt = bits;
    uint64_t ts, tmp, mask, val;
    siridb_point_t * point = points->data + points->len;

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

    memcpy(&point->ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);
    memcpy(&point->val.uint64, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    ts = point->ts;
    val = point->val.uint64 & ~mask;

    for (mask = 0; tshift-- > tcount; ++pt)
    {
        mask |= ((uint64_t) *pt) << (tshift * 8);
    }

    for (i = len; (end_ts == NULL || ts < *end_ts) && --i; )
    {
        if (start_ts != NULL && ts < *start_ts)
        {
            --len;
        }
        else
        {
            ++point;
        }

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

    if (has_overlap && points->len)
    {
        qp_via_t v;
        point = points->data + points->len;
        for (i = len - i; i--; ++point)
        {
            ts = point->ts;
            v = point->val;
            siridb_points_add_point(points, &ts, &v);
        }
    }
    else
    {
        points->len += len - i;
    }
}

size_t siridb_points_get_size_zipped(uint16_t cinfo, uint16_t len)
{
    if (len < ZIP_THRESHOLD)
    {
        return len * 16;
    }
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

static unsigned char * POINTS_zip_raw(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size)
{
    uint_fast32_t n = end - start;
    *size = n * 16;
    *cinfo = 0xffff;
    unsigned char * bits = (unsigned char *) malloc(*size);
    if (bits == NULL)
    {
        return NULL;
    }
    unsigned char * pt = bits;
    for (uint_fast32_t i = start; i < end; ++i)
    {
        memcpy(pt, &points->data[i].ts, sizeof(uint64_t));
        pt += sizeof(uint64_t);
        memcpy(pt, &points->data[i].val, sizeof(uint64_t));
        pt += sizeof(uint64_t);
    }
    return bits;
}

static void POINTS_unzip_raw(
        siridb_points_t * points,
        unsigned char * bits,
        uint16_t len,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    unsigned char * pt = bits;
    siridb_point_t p;

    while (len--)
    {
        memcpy(&p.ts, pt, sizeof(uint64_t));
        pt += sizeof(uint64_t);
        memcpy(&p.val, pt, sizeof(uint64_t));
        pt += sizeof(uint64_t);
        if ((start_ts == NULL || p.ts >= *start_ts) &&
            (end_ts == NULL || p.ts < *end_ts))
        {
            if (has_overlap)
            {
                siridb_points_add_point(points, &p.ts, &p.val);
            }
            else
            {
                memcpy(points->data + points->len++, &p,
                        sizeof(siridb_point_t));
            }
        }
    }
}

/*
 * This merge method works best when having both a lot of series and having
 * any number of points for each series.
 */
static void POINTS_highest_and_merge(slist_t * plist, siridb_points_t * points)
{
    siridb_points_t ** m = NULL;
    size_t i, n, l = 0;
    siridb_points_t ** tpts;
    uint64_t highest = 0;
    n = points->len;

    while (plist->len)
    {
        if (m != NULL)
        {
            tpts = m;
            m = NULL;
            for (; l < plist->len; l++, tpts++)
            {
                if (((*tpts)->data + (*tpts)->len)->ts == highest)
                {
                    m = tpts;
                    break;
                }
            }
        }

        if (m == NULL)
        {
            m = (siridb_points_t **) plist->data;

            for (i = 1, tpts = m + 1; i < plist->len; i++, tpts++)
            {
                if (((*tpts)->data + (*tpts)->len)->ts >
                    ((*m)->data + (*m)->len)->ts)
                {
                    m = tpts;
                    l = i;
                    highest = ((*m)->data + (*m)->len)->ts;
                }
            }
        }

        points->data[--n] = (*m)->data[(*m)->len];

        if ((*m)->len--)
        {
            m++;
            l++;
        }
        else
        {
            siridb_points_free(*m);

            if (--plist->len)
            {
                *m = (siridb_points_t *) plist->data[plist->len];
            }
        }

        if (!(n % MAX_ITERATE_MERGE_COUNT))
        {
            usleep(3000);
        }
    }
#ifdef DEBUG
    /* size should be exactly zero */
    assert (n == 0);
#endif
}


/*
 * This merge method works best for only a few series with possible a lot
 * of points.
 */
static void POINTS_sort_while_merge(slist_t * plist, siridb_points_t * points)
{
    siridb_points_t ** m;
    size_t i, n;
    siridb_points_t ** tpts;
    n = points->len;

    while (plist->len)
    {
        m = (siridb_points_t **) plist->data;

        for (i = 1, tpts = m + 1; i < plist->len; i++, tpts++)
        {
            if (((*tpts)->data + (*tpts)->len)->ts >
                ((*m)->data + (*m)->len)->ts)
            {
                m = tpts;
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

        if (!(n % MAX_ITERATE_MERGE_COUNT))
        {
            usleep(3000);
        }
    }
#ifdef DEBUG
    /* size should be exactly zero */
    assert (n == 0);
#endif
}

/*
 * This merge method works best when having a lot of series with only one point
 * or when the total number of points it not so high.
 */
static void POINTS_merge_and_sort(slist_t * plist, siridb_points_t * points)
{
    siridb_points_t ** m;
    size_t i, n;
    n = points->len;
    uint8_t use_simple_sort = n == plist->len;

    while (plist->len)
    {
        for (i = 0; i < plist->len; i++)
        {
            m = (siridb_points_t **) &plist->data[i];

            points->data[--n] = (*m)->data[(*m)->len];

            if (!(*m)->len--)
            {
                siridb_points_free(*m);
                if (--plist->len)
                {
                    *m = (siridb_points_t *) plist->data[plist->len];
                }
            }

            if (!(n % MAX_ITERATE_MERGE_COUNT))
            {
                usleep(3000);
            }
        }
    }

#ifdef DEBUG
    /* size should be exactly zero */
    assert (n == 0);
#endif

    usleep(5000);

    if (use_simple_sort)
    {
        POINTS_simple_sort(points);
    }
    else
    {
        qsort(  points->data,
                points->len,
                sizeof(siridb_point_t),
                &POINTS_compare);
    }
}

static inline int POINTS_compare(const void * a, const void * b)
{
    return (((siridb_point_t *) a)->ts - ((siridb_point_t *) b)->ts);
}
/*
 * Sort points by time-stamp.
 *
 * Warning: this function should only be used in another thread.
 */
static void POINTS_simple_sort(siridb_points_t * points)
{
    if (points->len < 2)
    {
        return;  /* nothing to do... */
    }
    size_t n = 0;
    size_t i;
    siridb_point_t * pa, * pb;
    siridb_point_t tmp;
    while(++n < points->len)
    {
        pa = points->data + n - 1;
        pb = points->data + n;
        if (pa->ts > pb->ts)
        {
            tmp = points->data[n];
            i = n;
            do
            {
                points->data[i] = points->data[i - 1];
            }
            while (--i && points->data[i - 1].ts > tmp.ts);
            points->data[i] = tmp;

            usleep(100);
        }
    }
}
