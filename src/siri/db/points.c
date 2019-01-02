/*
 * points.c - Array object for points.
 */
#include <siri/db/points.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <siri/err.h>
#include <unistd.h>
#include <string.h>
#include <xstr/xstr.h>

#define MAX_ITERATE_MERGE_COUNT 1000
#define POINTS_MAX_QSORT 250000
#define RAW_VALUES_THRESHOLD 7
#define DICT_SZ 0x3fff

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
static void POINTS_sort_while_merge(vec_t * plist, siridb_points_t * points);
static void POINTS_merge_and_sort(vec_t * plist, siridb_points_t * points);
static void POINTS_simple_sort(siridb_points_t * points);
static inline int POINTS_compare(const void * a, const void * b);
static void POINTS_highest_and_merge(vec_t * plist, siridb_points_t * points);
static size_t POINTS_strlen_check_ascii(const char * str, uint8_t * is_ascii);
static void POINTS_output_literal(
        size_t len,
        uint8_t * pt,
        uint8_t ** out,
        uint8_t is_ascii);
static void POINTS_output_match(
        size_t offset,
        size_t len,
        uint8_t ** out,
        uint8_t is_ascii);
static void POINTS_zip_str(
        uint8_t ** out,
        uint8_t ** pt,
        uint8_t * src,
        uint8_t * s,
        size_t n,
        uint8_t is_ascii);
static int POINTS_set_cinfo_size(uint16_t * cinfo, size_t * size);
inline static uint16_t POINTS_hash(uint32_t h);
static void POINTS_destroy(siridb_points_t * points);

static uint8_t * dictionary[DICT_SZ + 1];

void siridb_points_init(void)
{
    memset(dictionary, 0, sizeof(dictionary));
}

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
 * Resize points to a new size. Returns 0 when successful or -1 if failed.
 */
int siridb_points_resize(siridb_points_t * points, size_t n)
{
    assert( points->len <= n );
    siridb_point_t * tmp = realloc(points->data, sizeof(siridb_point_t) * n);
    if (tmp == NULL && n)
    {
        return -1;
    }
    points->data = tmp;
    return 0;
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
        if (points->tp == TP_STRING)
        {
            size_t i;
            for (i = 0; i < points->len; ++i)
            {
                (cpoints->data + i)->val.str =
                        strdup((points->data + i)->val.str);
            }
        }
    }
    return cpoints;
}

/*
 * Destroy points. (parsing NULL is NOT allowed)
 */
void siridb_points_free(siridb_points_t * points)
{
    if (points->tp == TP_STRING)
    {
        size_t i;
        for (i = 0; i < points->len; ++i)
        {
            free((points->data + i)->val.str);
        }
    }
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
        size_t i;
        siridb_point_t * point = points->data;
        switch (points->tp)
        {
        case TP_INT:
            for (i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_int64(packer, point->val.int64);
            }
            break;
        case TP_DOUBLE:
            for (i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_double(packer, point->val.real);
            }
            break;
        case TP_STRING:
            for (i = 0; i < points->len; i++, point++)
            {
                qp_add_type(packer, QP_ARRAY2);
                qp_add_int64(packer, (int64_t) point->ts);
                qp_add_string(packer, point->val.str);
            }
            break;
        }
    }
    qp_add_type(packer, QP_ARRAY_CLOSE);

    return siri_err;
}

void siridb_points_ts_correction(siridb_points_t * points, double factor)
{
    siridb_point_t * point = points->data;
    size_t i;
    for (i = 0; i < points->len; i++, point++)
    {
        point->ts *= factor;
    }
}

/*
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
int siridb_points_raw_pack(siridb_points_t * points, qp_packer_t * packer)
{
    int rc;
    size_t size;
    unsigned char * data;
    if (points->tp == TP_STRING)
    {
        uint16_t cinfo;
        data = siridb_points_zip_string(points, 0, points->len, &cinfo, &size);
    }
    else
    {
        data = (unsigned char *) points->data;
        size = points->len * sizeof(siridb_point_t);
    }

    rc = -(qp_add_type(packer, QP_ARRAY_OPEN) ||
            qp_add_int64(packer, (int64_t) points->tp) ||
            qp_add_int64(packer, (int64_t) points->len) ||
            qp_add_raw(packer, data, size) ||
            qp_add_type(packer, QP_ARRAY_CLOSE));

    if (points->tp == TP_STRING)
    {
        free(data);
    }

    return rc;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (err_msg is set when an error has occurred)
 * Use this function only when having at least two 'series' in the list.
 */
siridb_points_t * siridb_points_merge(vec_t * plist, char * err_msg)
{
    assert (plist->len >= 2);
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
            POINTS_destroy(plist->data[i]);

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

    /* we have at least one series left */
    assert (plist->len >= 1);

    if (plist->len == 1)
    {
        /*
         * Return the only left points since there is nothing to merge as set
         * list length to 0. We do have to restore the points length since
         * this is decremented by one.
         */
        points = (siridb_points_t *) vec_pop(plist);
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
    if (end - start < POINTS_ZIP_THRESHOLD)
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

    *cinfo = 0xff00 >> vcount;
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

unsigned char * siridb_points_zip_string(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size)
{
    uint_fast32_t n = end - start;
    if (n < POINTS_ZIP_THRESHOLD)
    {
        *size = sizeof(uint64_t);
        return siridb_points_raw_string(points, start, end, cinfo, size);
    }
    size_t * sizes = (size_t *) malloc(sizeof(size_t) * n);
    if (sizes == NULL)
    {
        return NULL;
    }
    uint8_t is_ascii = 0x80;
    uint64_t tdiff = 0;
    uint8_t * src, * out, * pt, * spt, * sout;
    uint64_t mask;
    size_t sz, m = n, i = end - 2;
    uint32_t sz_src = 0;
    siridb_point_t * point = points->data + i;
    uint64_t ts = (point + 1)->ts - point->ts;
    uint8_t tinfo = 0;
    uint8_t shift = 0;
    uint8_t ibit;

    sz = POINTS_strlen_check_ascii((point + 1)->val.str, &is_ascii);
    sizes[--m] = sz;
    sz_src += sz;
    sz = POINTS_strlen_check_ascii((point)->val.str, &is_ascii);
    sizes[--m] = sz;
    sz_src += sz;

    while (i-- > start)
    {
        point--;
        sz = POINTS_strlen_check_ascii(point->val.str, &is_ascii);
        sizes[--m] = sz;
        sz_src += sz;
        tdiff |= ts ^ ((point + 1)->ts - point->ts);
    }

    for (i = 1, mask = 0xff; i <= 8; mask <<= 8, i++)
    {
        if (tdiff & mask)
        {
            tinfo = i;
        }
        else if (ts & mask)
        {
            shift = i;
        }
    }

    shift = tinfo > shift ? tinfo : shift;
    ibit = tinfo | (shift << 4);
    /* calculate time-stamps size */
    sz = 13 + shift + tinfo*(n - 2);

    src = (uint8_t *) malloc(sz_src);
    out = (uint8_t *) malloc(sz + sz_src + (is_ascii ? 0 : (n * 8)));
    if (src == NULL || out == NULL)
    {
        goto failed;
    }
    pt = out;
    sout = out + sz;
    spt = src;

    memcpy(pt, &ibit, sizeof(uint8_t));
    pt++;
    memcpy(pt, &sz_src, sizeof(uint32_t));
    pt += sizeof(uint32_t);
    memcpy(pt, &point->ts, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    POINTS_zip_str(&sout, &spt, src, point->val.raw, sizes[m++], is_ascii);

    for (; shift-- > tinfo; ++pt)
    {
        *pt = ts >> (shift * 8);
    }
    for (i = end; --i > start;)
    {
        point++;
        ts = point->ts - (point - 1)->ts;

        for (shift = tinfo; shift--; ++pt)
        {
            *pt = ts >> (shift * 8);
        }

        POINTS_zip_str(&sout, &spt, src, point->val.raw, sizes[m++], is_ascii);
    }
    sz = *size = sout - out;

    if (POINTS_set_cinfo_size(cinfo, size))
    {
        goto failed;
    }

    if (*size > sz)
    {
        sout = (uint8_t *) realloc(out, *size);
        if (sout == NULL)
        {
            goto failed;

        }
        pt = out = sout;
        sout += *size;
        pt += sz;
        for (; pt < sout; ++pt)
        {
            *pt = '\0';
        }
    }

    free(sizes);
    free(src);
    return out;

failed:
    free(sizes);
    free(src);
    free(out);
    return NULL;
}

unsigned char * siridb_points_raw_string(
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        size_t * size)
{
    size_t ts_sz = *size;
    *size = 0;

    uint_fast32_t n = end - start;
    size_t * sizes = (size_t *) malloc(sizeof(size_t) * n);
    size_t * psz = sizes;
    unsigned char * pdata;
    unsigned char * cdata;
    uint_fast32_t i;

    if (sizes == NULL)
    {
        return NULL;
    }

    for (i = start; i < end; ++i, ++psz)
    {
        *psz = strlen(points->data[i].val.str) + 1;
        *size += *psz;
    }

    if (POINTS_set_cinfo_size(cinfo, size))
    {
        free(sizes);
        return NULL;
    }

    /* when uncompressed, time-stamp size is not included */
    *size += n * ts_sz;

    cdata = (unsigned char *) calloc(*size, sizeof(unsigned char));
    if (cdata == NULL)
    {
        free(sizes);
        return NULL;
    }

    pdata = cdata;
    psz = sizes;

    switch (ts_sz)
    {
    case sizeof(uint32_t):
        for (i = start; i < end; ++i)
        {
            uint32_t ts = points->data[i].ts;
            memcpy(pdata, &ts, sizeof(uint32_t));
            pdata += sizeof(uint32_t);
        }
        break;
    case sizeof(uint64_t):
        for (i = start; i < end; ++i)
        {
            memcpy(pdata, &points->data[i].ts, sizeof(uint64_t));
            pdata += sizeof(uint64_t);
        }
        break;
    default:
        assert(0);
    }

    for (i = start; i < end; ++i, ++psz)
    {
        memcpy(pdata, points->data[i].val.str, *psz);
        pdata += *psz;
    }

    free(sizes);

    return cdata;
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
    if (end - start < POINTS_ZIP_THRESHOLD)
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
    if (len < POINTS_ZIP_THRESHOLD)
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
    if (len < POINTS_ZIP_THRESHOLD)
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

static size_t POINTS_dec_len(uint8_t **pt)
{
    size_t sz = 0;
    uint8_t shift = 6;
    uint8_t * b = *pt;

    (*pt)++;
    sz += *b & 0x3f;

    if ((*b & 0x40) == 0)
    {
        return sz;
    }

    do
    {
        b = *pt;
        (*pt)++;
        sz |= (*b & 0x7f) << shift;
        shift += 7;
    } while ((*b & 0x80) != 0);

    return sz;
}

static int POINTS_unpack_string(
        siridb_point_t * point,
        size_t n,
        size_t offset,
        uint8_t * src,
        uint8_t * buf)
{
    assert (n);
    uint8_t * pt = src;
    uint8_t * sbuf = buf;
    uint8_t is_ascii = (0x80 & (*pt)) ^ 0x80;
    size_t i = 0;
    while (1)
    {
        if (is_ascii ^ ((*pt) & 0x80))
        {
            /* literal */
            size_t len = (is_ascii) ? 0 : POINTS_dec_len(&pt);

            do
            {
                *buf = *pt;
                ++pt;
                if (!*buf)
                {
                    if (i >= offset)
                    {
                        point->val.str = strdup((char *)sbuf);
                        if (point->val.str == NULL)
                        {
                            return -1;
                        }
                        ++point;
                        if (!--n)
                        {
                            return 0;
                        }
                    }
                    ++i;
                    ++buf;
                    sbuf = buf;
                    break;
                }
                ++buf;
            }
            while ((is_ascii && ((~(*pt)) & 0x80)) || ((!is_ascii) && --len));
        }
        else
        {
            /* match */
            size_t off = POINTS_dec_len(&pt);
            size_t len = POINTS_dec_len(&pt);
            while (len--)
            {
              *buf = *(buf - off);
              if (!*buf)
              {
                  if (i >= offset)
                  {
                      point->val.str = strdup((char *)sbuf);
                      if (point->val.str == NULL)
                      {
                          return -1;
                      }
                      ++point;
                      if (!--n)
                      {
                          return 0;
                      }
                  }
                  ++i;
                  ++buf;
                  sbuf = buf;
              }
              else
              {
                  ++buf;
              }
            }
        }
    }
}

int siridb_points_unzip_string_raw(
        siridb_points_t * points,
        uint8_t * bits,
        uint16_t len)
{
    uint64_t * tpt;
    tpt = (uint64_t *) bits;
    char * cpt = (char *) (bits + (len * sizeof(uint64_t)));

    for (; points->len < len; ++points->len, ++tpt)
    {
        size_t slen;
        points->data[points->len].ts = *tpt;
        points->data[points->len].val.str = xstr_dup(cpt, &slen);
        cpt += slen + 1;
    }
    return 0;
}

int siridb_points_unzip_string(
        siridb_points_t * points,
        uint8_t * bits,
        uint16_t len,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    uint64_t ts, tmp, mask;
    siridb_point_t * point = points->data + points->len;
    uint32_t src_sz;
    uint8_t * buf, * pt = bits;
    uint8_t j, tcount, tshift = *pt;
    size_t i, offset;

    tcount =  *pt & 0xf;
    tshift = *pt & 0xf0;
    tshift >>= 4;
    pt++;
    offset = 13 + tshift + (tcount * (len - 2));

    memcpy(&src_sz, pt, sizeof(uint32_t));
    pt += sizeof(uint32_t);
    memcpy(&point->ts, pt, sizeof(uint64_t));
    pt += sizeof(uint64_t);

    buf = (uint8_t *) malloc(src_sz);
    if (buf == NULL)
    {
        return -1;
    }

    for (mask = 0; tshift-- > tcount; ++pt)
    {
        mask |= ((uint64_t) *pt) << (tshift * 8);
    }

    ts = point->ts;
    uint16_t n = len;

    for (i = n; (end_ts == NULL || ts < *end_ts) && --i; )
    {
        if (start_ts != NULL && ts < *start_ts)
        {
            --n;
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
    }

    n -= i;

    if (POINTS_unpack_string(
            points->data + points->len, n, i, bits + offset, buf))
    {
        free(buf);
        return -1;
    }

    if (has_overlap && points->len)
    {
        qp_via_t v;
        point = points->data + points->len;
        for (i = n; i--; ++point)
        {
            ts = point->ts;
            v = point->val;
            siridb_points_add_point(points, &ts, &v);
        }
    }
    else
    {
        points->len += n;
    }

    free(buf);
    return 0;
}

size_t siridb_points_get_size_zipped(uint16_t cinfo, uint16_t len)
{
    if (len < POINTS_ZIP_THRESHOLD)
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
    uint_fast32_t i;
    unsigned char * bits;
    unsigned char * pt;

    *size = n * 16;
    *cinfo = 0xffff;

    bits = (unsigned char *) malloc(*size);
    if (bits == NULL)
    {
        return NULL;
    }

    pt = bits;
    for (i = start; i < end; ++i)
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
static void POINTS_highest_and_merge(vec_t * plist, siridb_points_t * points)
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
            POINTS_destroy(*m);

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
    /* size should be exactly zero */
    assert (n == 0);
}


/*
 * This merge method works best for only a few series with possible a lot
 * of points.
 */
static void POINTS_sort_while_merge(vec_t * plist, siridb_points_t * points)
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
            POINTS_destroy(*m);

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
    /* size should be exactly zero */
    assert (n == 0);
}

/*
 * This merge method works best when having a lot of series with only one point
 * or when the total number of points it not so high.
 */
static void POINTS_merge_and_sort(vec_t * plist, siridb_points_t * points)
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
                POINTS_destroy(*m);
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
    /* size should be exactly zero */
    assert (n == 0);

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
    return (((siridb_point_t *) a)->ts < ((siridb_point_t *) b)->ts)
        ? -1
        : (((siridb_point_t *) a)->ts > ((siridb_point_t *) b)->ts)
        ? 1
        : 0;
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

static size_t POINTS_strlen_check_ascii(const char * str, uint8_t * is_ascii)
{
    size_t sz;
    if (*is_ascii)
    {
        const char * pt = str;
        for (; *pt; ++pt)
        {
            *is_ascii &= (*pt) ^ 0x80;
        }
        sz = pt - str + 1;
    }
    else
    {
        sz = strlen(str) + 1;
    }
    return sz;
}

static void POINTS_output_literal(
        size_t len,
        uint8_t * pt,
        uint8_t ** out,
        uint8_t is_ascii)
{
    if (!is_ascii)
    {
        size_t n = len;
        **out = 0x80 | ((n > 0x3f) << 6) | (n & 0x3f);
        (*out)++;
        n >>= 6;

        while (n)
        {
            **out = ((n > 0x7f) << 7) | (n & 0x7f);
            (*out)++;
            n >>= 7;
        }
    }

    while (len--)
    {
        **out = *pt;
        (*out)++;
        pt++;
    }
}

static void POINTS_output_match(
        size_t offset,
        size_t len,
        uint8_t ** out,
        uint8_t is_ascii)
{
    **out = is_ascii | ((offset > 0x3f) << 6) | (offset & 0x3f);
    (*out)++;
    offset >>= 6;

    while (offset)
    {
        **out = ((offset > 0x7f) << 7) | (offset & 0x7f);
        (*out)++;
        offset >>= 7;
    }

    **out = is_ascii | ((len > 0x3f) << 6) | (len & 0x3f);
    (*out)++;
    len >>= 6;

    while (len)
    {
        **out = ((len > 0x7f) << 7) | (len & 0x7f);
        (*out)++;
        len >>= 7;
    }
}

static void POINTS_zip_str(
        uint8_t ** out,
        uint8_t ** pt,
        uint8_t * src,
        uint8_t * s,
        size_t n,
        uint8_t is_ascii)
{
    uint8_t * match;
    uint8_t * literal = *pt;
    memcpy(*pt, s, n);
    uint8_t * end = (*pt) + n;
    uint8_t * tend = end - sizeof(uint32_t);
    while (*pt < tend)
    {
        uint32_t * inp = (uint32_t *) *pt;
        uint16_t idx = POINTS_hash(*inp);
        match = dictionary[POINTS_hash(idx)];
        dictionary[idx] = *pt;
        if (match >= src && match < *pt && *((uint32_t *) match) == *inp)
        {
            if (literal < *pt)
            {
                POINTS_output_literal((*pt) - literal, literal, out, is_ascii);
            }

            uint8_t * p = (*pt) + 4;
            match += 4;
            while (p < end && *match == *p)
            {
                ++match;
                ++p;
            }

            POINTS_output_match(p - match, p - (*pt), out, is_ascii);
            literal = *pt = p;
        }
        else
        {
            ++(*pt);
        }
    }

    if (literal < end)
    {
        POINTS_output_literal(end - literal, literal, out, is_ascii);
    }
    *pt = end;
}

static int POINTS_set_cinfo_size(uint16_t * cinfo, size_t * size)
{
    if (*size >= 0x8000)
    {
        if (*size > 0x2000000)
        {
            log_critical("Size too large: %zu", *size);
            return -1;
        }
        *size = (!!((*size) & 0x3ff)) + ((*size) >> 10);
        *cinfo = (*size) | 0x8000;
        *size <<= 10;
    }
    else
    {
        *cinfo = *size;
    }
    return 0;
}

inline static uint16_t POINTS_hash(uint32_t h)
{
    return ((h >> 17) ^ (h & 0xffff)) & DICT_SZ;
}

/*
 * Destroy points. (parsing NULL is NOT allowed)
 */
static void POINTS_destroy(siridb_points_t * points)
{
    free(points->data);
    free(points);
}
