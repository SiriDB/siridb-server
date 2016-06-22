/*
 * qpack.c - efficient binary serialization format
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 11-03-2016
 *          ((l + n) // 4 + 1) * 4
 */
#include <qpack/qpack.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xmath/xmath.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <logger/logger.h>
#include <assert.h>

#define QPACK_MAX_FMT_SIZE 1024

#define QP_RESIZE(LEN)                                                  \
if (packer->len + LEN > packer->buffer_size)                            \
{                                                                       \
    packer->buffer_size =                                               \
            ((packer->len + LEN) / packer->alloc_size + 1)              \
            * packer->alloc_size;                                       \
    packer->buffer =                                                    \
            (char *) realloc(packer->buffer, packer->buffer_size);      \
}

#define QP_PLAIN_OBJ(QP_TYPE)                       \
{                                                   \
    QP_RESIZE(1)                                    \
    packer->buffer[packer->len++] = QP_TYPE;        \
}

#define QP_PREPARE_RAW                                      \
    QP_RESIZE((5 + len))                                    \
    if (len < 100)                                          \
        packer->buffer[packer->len++] = 128 + len;          \
    else if (len < 256)                                     \
    {                                                       \
        packer->buffer[packer->len++] = QP_RAW8;            \
        packer->buffer[packer->len++] = len;                \
    }                                                       \
    else if (len < 65536)                                   \
    {                                                       \
        packer->buffer[packer->len++] = QP_RAW16;           \
        memcpy(packer->buffer + packer->len, &len, 2);      \
        packer->len += 2;                                   \
    }                                                       \
    else if (len < 4294967296)                              \
    {                                                       \
        packer->buffer[packer->len++] = QP_RAW32;           \
        memcpy(packer->buffer + packer->len, &len, 4);      \
        packer->len += 4;                                   \
    }                                                       \
    else                                                    \
    {                                                       \
        packer->buffer[packer->len++] = QP_RAW64;           \
        memcpy(packer->buffer + packer->len, &len, 8);      \
        packer->len += 8;                                   \
    }

static qp_types_t print_unpacker(
        qp_types_t tp,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj);

qp_unpacker_t * qp_new_unpacker(const char * pt, size_t len)
{
    qp_unpacker_t * unpacker = (qp_unpacker_t *) malloc(sizeof(qp_unpacker_t));
    unpacker->source = NULL;
    unpacker->pt = pt;
    unpacker->end = pt + len;
    return unpacker;
}

qp_unpacker_t * qp_from_file_unpacker(const char * fn)
{
    FILE * fp;
    size_t size;
    qp_unpacker_t * unpacker;

    if (access(fn, R_OK) == -1)
        return NULL;

    fp = fopen(fn, "r");
    if (fp == NULL)
    {
        log_error("Could not read '%s'", fn);
        return NULL;
    }

    /* get the size */
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size == -1)
    {
        log_error("Cannot not read size of file '%s'", fn);
        unpacker = NULL;
    }
    else
    {
        unpacker = (qp_unpacker_t *) malloc(sizeof(qp_unpacker_t));
        unpacker->source = (char *) malloc(size);
        if (fread(unpacker->source, size, 1, fp) == 1)
        {
            unpacker->pt = unpacker->source;
            unpacker->end = unpacker->source + size;
        }
        else
        {
            log_error("Cannot not read from file '%s'", fn);
            qp_free_unpacker(unpacker);
            unpacker = NULL;
        }
    }

    fclose(fp);

    return unpacker;
}

void qp_free_object(qp_obj_t * qp_obj)
{
#ifdef DEBUG
    assert(qp_obj != NULL);
#endif
    free(qp_obj->via);
    free(qp_obj);
}

void qp_free_unpacker(qp_unpacker_t * unpacker)
{
#ifdef DEBUG
    assert(unpacker != NULL);
#endif
    free(unpacker->source);
    free(unpacker);
}

qp_obj_t * qp_new_object(void)
{
    qp_obj_t * qp_obj;
    qp_obj = (qp_obj_t *) malloc(sizeof(qp_obj_t));
    qp_obj->via = (qp_via_t *) malloc(sizeof(qp_via_t));
    return qp_obj;
}

qp_packer_t * qp_new_packer(size_t alloc_size)
{
    qp_packer_t * packer = (qp_packer_t *) malloc(sizeof(qp_packer_t));
    packer->alloc_size = alloc_size;
    packer->buffer_size = packer->alloc_size;
    packer->len = 0;
    packer->buffer = (char *) malloc(packer->buffer_size);
    return packer;
}

void qp_free_packer(qp_packer_t * packer)
{
#ifdef DEBUG
    assert(packer != NULL);
#endif
    free(packer->buffer);
    free(packer);
}

void qp_extend_packer(qp_packer_t * packer, qp_packer_t * source)
{
    QP_RESIZE(source->len)
    memcpy(packer->buffer + packer->len, source->buffer, source->len);
    packer->len += source->len;
}

inline int qp_is_array(qp_types_t tp)
{
    return tp == QP_ARRAY_OPEN || (tp >= QP_ARRAY0 && tp <= QP_ARRAY5);
}

inline int qp_is_map(qp_types_t tp)
{
    return tp == QP_MAP_OPEN || (tp >= QP_MAP0 && tp <= QP_MAP5);
}

void qp_print(const char * pt, size_t len)
{
    qp_unpacker_t * unpacker = qp_new_unpacker(pt, len);
    qp_obj_t * qp_obj = qp_new_object();
    print_unpacker(qp_next(unpacker, qp_obj), unpacker, qp_obj);
    printf("\n");
    qp_free_object(qp_obj);
    qp_free_unpacker(unpacker);
}

static qp_types_t print_unpacker(
        qp_types_t tp,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj)
{
    int count;
    int found;
    if (tp == QP_INT64)
        printf("%ld", qp_obj->via->int64);
    else if (tp == QP_DOUBLE)
        printf("%f", qp_obj->via->real);
    else if (tp == QP_RAW)
        printf("\"%.*s\"", (int) qp_obj->len,
                qp_obj->via->raw);
    else if (tp == QP_TRUE)
        printf("true");
    else if (tp == QP_FALSE)
        printf("false");
    else if (tp == QP_NULL)
        printf("null");
    else if (tp >= QP_ARRAY0 && tp <= QP_ARRAY5)
    {
        printf("[");
        count = tp - QP_ARRAY0;
        tp = qp_next(unpacker, qp_obj);
        for (found = 0; count-- && tp; found = 1)
        {
            if (found )
                printf(", ");
            tp = print_unpacker(tp, unpacker, qp_obj);
        }
        printf("]");
        return tp;
    }
    else if (tp >= QP_MAP0 && tp <= QP_MAP5)
    {
        printf("{");
        count = tp - QP_MAP0;
        tp = qp_next(unpacker, qp_obj);
        for (found = 0; count-- && tp; found = 1)
        {
            if (found )
                printf(", ");
            tp = print_unpacker(tp, unpacker, qp_obj);
            printf(": ");
            tp = print_unpacker(tp, unpacker, qp_obj);
        }
        printf("}");
        return tp;
    }
    else if (tp == QP_ARRAY_OPEN)
    {
        printf("[");
        tp = qp_next(unpacker, qp_obj);
        for (count = 0; tp && tp != QP_ARRAY_CLOSE; count = 1)
        {
            if (count)
                printf(", ");
            tp = print_unpacker(tp, unpacker, qp_obj);
        }
        printf("]");
    }
    else if (tp == QP_MAP_OPEN)
    {
        printf("{");
        tp = qp_next(unpacker, qp_obj);
        for (count = 0; tp && tp != QP_MAP_CLOSE; count = 1)
        {
            if (count)
                printf(", ");
            tp = print_unpacker(tp, unpacker, qp_obj);
            printf(": ");
            tp = print_unpacker(tp, unpacker, qp_obj);
        }
        printf("}");
    }
    return qp_next(unpacker, qp_obj);
}
void qp_add_fmt(qp_packer_t * packer, const char * fmt, ...)
{
    va_list args;
    char buffer[QPACK_MAX_FMT_SIZE];
    va_start(args, fmt);
    vsnprintf(buffer, QPACK_MAX_FMT_SIZE, fmt, args);
    va_end(args);
    qp_add_raw(packer, buffer, strlen(buffer));
}

void qp_add_raw(qp_packer_t * packer, const char * raw, size_t len)
{
    QP_PREPARE_RAW
    memcpy(packer->buffer + packer->len, raw, len);
    packer->len += len;
}

void qp_add_raw_term(qp_packer_t * packer, const char * raw, size_t len_raw)
{
    /* add one because 0 (terminator) should be included with the length */
    size_t len = len_raw + 1;
    QP_PREPARE_RAW

    /* now take 1 because we want to copy one less than the new len */
    memcpy(packer->buffer + packer->len, raw, len_raw);
    packer->len += len;
    packer->buffer[packer->len - 1] = 0;
}

void qp_add_string(qp_packer_t * packer, const char * str)
{
    qp_add_raw(packer, str, strlen(str));
}

void qp_add_string_term(qp_packer_t * packer, const char * str)
{
    qp_add_raw(packer, str, strlen(str) + 1);
}

void qp_add_double(qp_packer_t * packer, double real)
{
    QP_RESIZE(9)
    if (real == 0.0)
        packer->buffer[packer->len++] = QP_DOUBLE_0;
    else if (real == 1.0)
        packer->buffer[packer->len++] = QP_DOUBLE_1;
    else if (real == -1.0)
        packer->buffer[packer->len++] = QP_DOUBLE_N1;
    else
    {
        packer->buffer[packer->len++] = QP_DOUBLE;
        memcpy(packer->buffer + packer->len, &real, 8);
        packer->len += 8;
    }
}
void qp_add_int8(qp_packer_t * packer, int8_t integer)
{
    QP_RESIZE(2)
    if (integer >= 0 && integer < 64)
        packer->buffer[packer->len++] = integer;
    else if (integer > -64 && integer < 0)
        packer->buffer[packer->len++] = 63 - integer;
    else
    {
        packer->buffer[packer->len++] = QP_INT8;
        packer->buffer[packer->len++] = integer;
    }
}
void qp_add_int16(qp_packer_t * packer, int16_t integer)
{
    QP_RESIZE(3)
    packer->buffer[packer->len++] = QP_INT16;
    memcpy(packer->buffer + packer->len, &integer, 2);
    packer->len += 2;
}
void qp_add_int32(qp_packer_t * packer, int32_t integer)
{
    QP_RESIZE(5)
    packer->buffer[packer->len++] = QP_INT32;
    memcpy(packer->buffer + packer->len, &integer, 4);
    packer->len += 4;
}
void qp_add_int64(qp_packer_t * packer, int64_t integer)
{
    QP_RESIZE(9)
    packer->buffer[packer->len++] = QP_INT64;
    memcpy(packer->buffer + packer->len, &integer, 8);
    packer->len += 8;
}

void qp_add_true(qp_packer_t * packer) QP_PLAIN_OBJ(QP_TRUE)
void qp_add_false(qp_packer_t * packer) QP_PLAIN_OBJ(QP_FALSE)
void qp_add_null(qp_packer_t * packer) QP_PLAIN_OBJ(QP_NULL)

void qp_add_type(qp_packer_t * packer, qp_types_t tp)
{
#ifdef DEBUG
    assert(tp >= QP_ARRAY0 && tp <= QP_MAP_CLOSE);
#endif

    QP_RESIZE(1)
    packer->buffer[packer->len++] = tp;
}

int qp_fadd_type(qp_fpacker_t * fpacker, qp_types_t tp)
{
#ifdef DEBUG
    assert(tp >= QP_ARRAY0 && tp <= QP_MAP_CLOSE);
#endif

    return (fputc(tp, fpacker) == tp) ? 0 : -1;
}

int qp_fadd_raw(qp_fpacker_t * fpacker, const char * raw, size_t len)
{
    if (len < 100)
        fputc(128 + len, fpacker);
    else if (len < 256)
    {
        fputc(QP_RAW8, fpacker);
        fputc(len, fpacker);
    }
    else if (len < 65536)
    {
        fputc(QP_RAW16, fpacker);
        fwrite(&len, sizeof(uint16_t), 1, fpacker);
    }
    else if (len < 4294967296)
    {
        fputc(QP_RAW32, fpacker);
        fwrite(&len, sizeof(uint32_t), 1, fpacker);
    }
    else
    {
        fputc(QP_RAW64, fpacker);
        fwrite(&len, sizeof(uint64_t), 1, fpacker);
    }
    return (fwrite(raw, len, 1, fpacker) == 1) ? 0 : -1;
}

int qp_fadd_string(qp_fpacker_t * fpacker, const char * str)
{
    return qp_fadd_raw(fpacker, str, strlen(str));
}

int qp_fadd_int8(qp_fpacker_t * fpacker, int8_t integer)
{
    if (integer >= 0 && integer < 64)
        return (fputc(integer, fpacker) != EOF) ? 0 : -1;

    if (integer > -64 && integer < 0)
        return (fputc(63 - integer, fpacker) != EOF) ? 0 : -1;

    return (fputc(QP_INT8, fpacker) != EOF && fputc(integer, fpacker) != EOF)
            ? 0 : -1;
}

int qp_fadd_int16(qp_fpacker_t * fpacker, int16_t integer)
{
    return (fputc(QP_INT16, fpacker) != EOF &&
            fwrite(&integer, sizeof(int16_t), 1, fpacker) == 1) ? 0 : -1;
}

int qp_fadd_int32(qp_fpacker_t * fpacker, int32_t integer)
{
    return (fputc(QP_INT32, fpacker) != EOF &&
            fwrite(&integer, sizeof(int32_t), 1, fpacker) == 1) ? 0 : -1;
}

int qp_fadd_int64(qp_fpacker_t * fpacker, int64_t integer)
{
    return (fputc(QP_INT64, fpacker) != EOF &&
            fwrite(&integer, sizeof(int64_t), 1, fpacker) == 1) ? 0 : -1;
}

qp_types_t qp_next(qp_unpacker_t * unpacker, qp_obj_t * qp_obj)
{
    uint_fast8_t tp;
    if (unpacker->pt >= unpacker->end)
        return QP_END;

    tp = *unpacker->pt;

    /* unpack specials like array, map, boolean, null etc */
    if (tp > 236)
    {
        if (qp_obj != NULL)
            qp_obj->tp = *unpacker->pt;
        unpacker->pt++;
        return tp;
    }

    unpacker->pt++;

    /* unpack fixed positive or negative integers */
    if (tp < 125)
    {
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_INT64;
            qp_obj->via->int64 =  (tp < 64) ?
                    (int64_t) tp :
                    (int64_t) 63 - tp;
        }
        return QP_INT64;
    }

    /* unpack fixed doubles -1.0, 0.0 or 1.0 */
    if (tp < 128)
    {
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_DOUBLE;
            qp_obj->via->real = (double) (tp - 126);
        }
        return QP_DOUBLE;
    }

    /* unpack fixed sized raw strings */
    if (tp < 228)
    {
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_RAW;
            qp_obj->via->raw = unpacker->pt;
            qp_obj->len = (size_t) (tp - 128);
        }
        unpacker->pt += tp - 128;
        return QP_RAW;
    }

    /* unpack raw strings */
    if (tp < 232)
    {
        size_t sz = (tp == QP_RAW8)   ? (size_t) *((uint8_t *) unpacker->pt):
                    (tp == QP_RAW16)  ? (size_t) *((uint16_t *) unpacker->pt):
                    (tp == QP_RAW32)  ? (size_t) *((uint32_t *) unpacker->pt):
                                  (size_t) *((uint64_t *) unpacker->pt);

        unpacker->pt += xmath_ipow(2, -QP_RAW8 + tp);
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_RAW;
            qp_obj->via->raw = unpacker->pt;
            qp_obj->len = sz;
        }
        unpacker->pt += sz;

        return QP_RAW;
    }

    /* unpack integer values */
    if (tp < 236)
    {
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_INT64;
            qp_obj->via->int64 =
                    (tp == QP_INT8)  ? (int64_t) *((int8_t *) unpacker->pt):
                    (tp == QP_INT16) ? (int64_t) *((int16_t *) unpacker->pt):
                    (tp == QP_INT32) ? (int64_t) *((int32_t *) unpacker->pt):
                                  (int64_t) *((int64_t *) unpacker->pt);
        }
        unpacker->pt += xmath_ipow(2, -QP_INT8 + tp);
        return QP_INT64;
    }

    if (tp == QP_DOUBLE)
    {
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_DOUBLE;
            qp_obj->via->real =
                    (double) *((double *) unpacker->pt);
        }
        unpacker->pt += 8;
        return QP_DOUBLE;
    }

    // error
    return QP_ERR;
}
