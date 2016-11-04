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
#define _GNU_SOURCE
#include <qpack/qpack.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <logger/logger.h>
#include <assert.h>
#include <siri/err.h>


#define QPACK_MAX_FMT_SIZE 1024

#define QP_RESIZE(LEN)                                                  \
if (packer->len + LEN > packer->buffer_size)                            \
{                                                                       \
    packer->buffer_size =                                               \
            ((packer->len + LEN) / packer->alloc_size + 1)              \
            * packer->alloc_size;                                       \
    char * tmp = (char *) realloc(packer->buffer, packer->buffer_size); \
    if (tmp == NULL)                                                    \
    {                                                                   \
        ERR_ALLOC                                                       \
        packer->buffer_size = packer->len;                              \
        return -1;                                                      \
    }                                                                   \
    packer->buffer = tmp;                                               \
}

#define QP_PLAIN_OBJ(QP_TYPE)                       \
{                                                   \
    QP_RESIZE(1)                                    \
    packer->buffer[packer->len++] = QP_TYPE;        \
    return 0;                                       \
}

#define QP_PREPARE_RAW                                      \
    QP_RESIZE((5 + len))                                    \
    if (len < 100)                                          \
    {                                                       \
        packer->buffer[packer->len++] = 128 + len;          \
    }                                                       \
    else if (len < 256)                                     \
    {                                                       \
        uint8_t length = (uint8_t) len;                     \
        packer->buffer[packer->len++] = QP_RAW8;            \
        packer->buffer[packer->len++] = length;             \
    }                                                       \
    else if (len < 65536)                                   \
    {                                                       \
        uint16_t length = (uint16_t) len;                   \
        packer->buffer[packer->len++] = QP_RAW16;           \
        memcpy(packer->buffer + packer->len, &length, 2);   \
        packer->len += 2;                                   \
    }                                                       \
    else if (len < 4294967296)                              \
    {                                                       \
        uint32_t length = (uint32_t) len;                   \
        packer->buffer[packer->len++] = QP_RAW32;           \
        memcpy(packer->buffer + packer->len, &length, 4);   \
        packer->len += 4;                                   \
    }                                                       \
    else                                                    \
    {                                                       \
        packer->buffer[packer->len++] = QP_RAW64;           \
        memcpy(packer->buffer + packer->len, &len, 8);      \
        packer->len += 8;                                   \
    }

#define QP_UNPACK_RAW(uintx_t)                              \
{                                                           \
    QP_UNPACK_CHECK_SZ(sizeof(uintx_t))                     \
    size_t sz = (size_t) *((uintx_t *) unpacker->pt);       \
    QP_UNPACK_CHECK_SZ(sz)                                  \
    unpacker->pt += sizeof(uintx_t);                        \
    if (qp_obj != NULL)                                     \
    {                                                       \
        qp_obj->tp = QP_RAW;                                \
        qp_obj->via.raw = unpacker->pt;                     \
        qp_obj->len = sz;                                   \
    }                                                       \
    unpacker->pt += sz;                                     \
    return QP_RAW;                                          \
}

#define QP_UNPACK_INT(intx_t)                               \
{                                                           \
    QP_UNPACK_CHECK_SZ(sizeof(intx_t))                      \
    int64_t val = (int64_t) *((intx_t *) unpacker->pt);     \
    if (qp_obj != NULL)                                     \
    {                                                       \
        qp_obj->tp = QP_INT64;                              \
        qp_obj->via.int64 = val;                            \
    }                                                       \
    unpacker->pt += sizeof(intx_t);                         \
    return QP_INT64;                                        \
}

#define QP_UNPACK_CHECK_SZ(size)            \
if (unpacker->pt + size > unpacker->end)    \
{                                           \
    if  (qp_obj != NULL)                    \
    {                                       \
        qp_obj->tp = QP_ERR;                \
    }                                       \
    return QP_ERR;                          \
}

static qp_types_t QP_print_unpacker(
        qp_types_t tp,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj);

/*
 * Initialize unpacker object.
 */
void qp_unpacker_init(qp_unpacker_t * unpacker, char * pt, size_t len)
{
    unpacker->source = NULL;
    unpacker->pt = pt;
    unpacker->end = pt + len;
}

/*
 * Destroy unpacker object. (parsing NULL is not allowed)
 */
void qp_unpacker_ff_free(qp_unpacker_t * unpacker)
{
#ifdef DEBUG
    assert(unpacker != NULL);
#endif
    free(unpacker->source);
    free(unpacker);
}

/*
 * Returns a unpacker object or NULL in case an error occurred. The error
 * message will logged using at least log_error().
 *
 * In case and only in case of a memory (malloc) error, a signal will be raised.
 */
qp_unpacker_t * qp_unpacker_ff(const char * fn)
{
    FILE * fp;
    size_t size;
    qp_unpacker_t * unpacker;

    fp = fopen(fn, "r");
    if (fp == NULL)
    {
        log_error("Could not read '%s'", fn);
        return NULL;
    }

    /* get the size */
    if (
            fseeko(fp, 0, SEEK_END) ||
            (size = ftello(fp)) == -1 ||
            fseeko(fp, 0, SEEK_SET))
    {
        log_error("Cannot not read size of file '%s'", fn);
        unpacker = NULL;
    }
    else
    {
        unpacker = (qp_unpacker_t *) malloc(sizeof(qp_unpacker_t));
        if (unpacker == NULL)
        {
            ERR_ALLOC
        }
        else
        {
            unpacker->source = (char *) malloc(size);
            if (unpacker->source == NULL)
            {
                ERR_ALLOC
                qp_unpacker_ff_free(unpacker);
                unpacker = NULL;
            }
            else if (fread(unpacker->source, size, 1, fp) == 1)
            {
                unpacker->pt = unpacker->source;
                unpacker->end = unpacker->source + size;
            }
            else
            {
                log_error("Cannot not read from file '%s'", fn);
                qp_unpacker_ff_free(unpacker);
                unpacker = NULL;
            }
        }
    }

    if (fclose(fp))
    {
        /*
         * Not critical and should probably never happen because file was
         * open only for reading.
         */
        log_error("Could not close file: '%s'", fn);
    }

    return unpacker;
}

/*
 * Returns a new packer object or NULL in case of an error.
 */
qp_packer_t * qp_packer_new(size_t alloc_size)
{
    qp_packer_t * packer = (qp_packer_t *) malloc(sizeof(qp_packer_t));
    if (packer != NULL)
    {
    	packer->alloc_size = alloc_size;
        packer->buffer_size = packer->alloc_size;
        packer->len = 0;

        packer->buffer = (char *) malloc(packer->buffer_size);
        if (packer->buffer == NULL)
        {
            free(packer);
            packer = NULL;
        }
    }
    return packer;
}

/*
 * Destroy packer object. (parsing NULL is not allowed)
 */
void qp_packer_free(qp_packer_t * packer)
{
#ifdef DEBUG
    assert(packer != NULL);
#endif
    free(packer->buffer);
    free(packer);
}

/*
 * Extend packer with another packer (source).
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_packer_extend(qp_packer_t * packer, qp_packer_t * source)
{
    QP_RESIZE(source->len)
    memcpy(packer->buffer + packer->len, source->buffer, source->len);
    packer->len += source->len;
    return 0;
}

/*
 * Extend packer with data from an unpacker.
 * (only the object at the current position will be copied)
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_packer_extend_fu(qp_packer_t * packer, qp_unpacker_t * unpacker)
{
    /* mark the start of the object */
    const char * start = unpacker->pt;

    /* jump to the end of the current object */
    qp_skip_next(unpacker);

    /* get size of the total object */
    size_t size = unpacker->pt - start;

    /* write object to the packer */
    QP_RESIZE(size)
    memcpy(packer->buffer + packer->len, start, size);
    packer->len += size;
    return 0;
}

inline int qp_is_array(qp_types_t tp)
{
    return tp == QP_ARRAY_OPEN || (tp >= QP_ARRAY0 && tp <= QP_ARRAY5);
}

inline int qp_is_map(qp_types_t tp)
{
    return tp == QP_MAP_OPEN || (tp >= QP_MAP0 && tp <= QP_MAP5);
}

inline int qp_is_close(qp_types_t tp)
{
    return tp >= QP_ARRAY_CLOSE;
}

inline int qp_is_raw(qp_types_t tp)
{
    return tp == QP_RAW;
}

inline int qp_is_int(qp_types_t tp)
{
    return tp == QP_INT64;
}

inline int qp_is_double(qp_types_t tp)
{
    return tp == QP_DOUBLE;
}

inline int qp_is_raw_term(qp_obj_t * qp_obj)
{
    return (qp_obj->tp == QP_RAW &&
            qp_obj->len &&
            qp_obj->via.raw[qp_obj->len - 1] == '\0');
}

/*
 * Print qpack content.
 */
void qp_print(char * pt, size_t len)
{
    qp_obj_t qp_obj;
    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, pt, len);
    QP_print_unpacker(qp_next(&unpacker, &qp_obj), &unpacker, &qp_obj);
    printf("\n");
}

/*
 * This is like qp_next(unpacker, NULL) but in case of a map or array the
 * total object is skipped. The return type can be used to check what the
 * skipped object was.
 */
qp_types_t qp_skip_next(qp_unpacker_t * unpacker)
{
    qp_types_t tp = qp_next(unpacker, NULL);
    int count;
    switch (tp)
    {

    case QP_ARRAY0:
    case QP_ARRAY1:
    case QP_ARRAY2:
    case QP_ARRAY3:
    case QP_ARRAY4:
    case QP_ARRAY5:
        count = tp - QP_ARRAY0;
        while (count--)
        {
            qp_skip_next(unpacker);
        }
        return tp;
    case QP_MAP0:
    case QP_MAP1:
    case QP_MAP2:
    case QP_MAP3:
    case QP_MAP4:
    case QP_MAP5:
        count = (tp - QP_MAP0) * 2;
        while (count--)
        {
            qp_skip_next(unpacker);
        }
        return tp;
    case QP_ARRAY_OPEN:
        while (tp && tp != QP_ARRAY_CLOSE)
        {
            tp = qp_skip_next(unpacker);
        }
        return QP_ARRAY_OPEN;
    case QP_MAP_OPEN:
        /* read first key or end or close */
        tp = qp_skip_next(unpacker);
        while (tp && tp != QP_MAP_CLOSE)
        {
            /* read value */
            qp_skip_next(unpacker);

            /* read next key or end or close */
            tp = qp_skip_next(unpacker);
        }
        return QP_MAP_OPEN;
    default:
        return tp;
    }
}

/*
 * This function will not add more than QPACK_MAX_FMT_SIZE and will still
 * return 0 in case longer strings are parsed.
 *
 * Use qp_add_fmt_safe() in case you want to add longer or unknown length.
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_fmt(qp_packer_t * packer, const char * fmt, ...)
{
    va_list args;
    char buffer[QPACK_MAX_FMT_SIZE];
    va_start(args, fmt);
    vsnprintf(buffer, QPACK_MAX_FMT_SIZE, fmt, args);
    va_end(args);
    return qp_add_raw(packer, buffer, strlen(buffer));
}

/*
 * Like qp_add_fmt() but works for any length.
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_fmt_safe(qp_packer_t * packer, const char * fmt, ...)
{
    int rc;
    va_list args;
    char * buffer;

    va_start(args, fmt);
    rc = vasprintf(&buffer, fmt, args);
    va_end(args);

    if (rc == -1)
    {
        ERR_ALLOC
        return -1;
    }

    rc = qp_add_raw(packer, buffer, strlen(buffer));
    free(buffer);
    return rc;
}

/*
 * Adds a raw string to the packer fixed to len chars.
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_raw(qp_packer_t * packer, const char * raw, size_t len)
{
    QP_PREPARE_RAW
    memcpy(packer->buffer + packer->len, raw, len);
    packer->len += len;
    return 0;
}

/*
 * Adds a raw string to the packer and appends a terminator (0) so the written
 * length is len + 1
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_raw_term(qp_packer_t * packer, const char * raw, size_t len_raw)
{
    /* add one because 0 (terminator) should be included with the length */
    size_t len = len_raw + 1;
    QP_PREPARE_RAW

    /* now take 1 because we want to copy one less than the new len */
    memcpy(packer->buffer + packer->len, raw, len_raw);
    packer->len += len;
    packer->buffer[packer->len - 1] = '\0';
    return 0;
}

/*
 * Adds a 0 terminated string to the packer but note that the terminator itself
 * will NOT be written. (Use qp_add_string_term() instead if you want the
 * destination to be 0 terminated
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
inline int qp_add_string(qp_packer_t * packer, const char * str)
{
    return qp_add_raw(packer, str, strlen(str));
}

/*
 * Like qp_add_string() but includes the 0 terminator.
 *
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
inline int qp_add_string_term(qp_packer_t * packer, const char * str)
{
    return qp_add_raw(packer, str, strlen(str) + 1);
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_double(qp_packer_t * packer, double real)
{
    QP_RESIZE(9)
    if (real == 0.0)
    {
        packer->buffer[packer->len++] = QP_DOUBLE_0;
    }
    else if (real == 1.0)
    {
        packer->buffer[packer->len++] = QP_DOUBLE_1;
    }
    else if (real == -1.0)
    {
        packer->buffer[packer->len++] = QP_DOUBLE_N1;
    }
    else
    {
        packer->buffer[packer->len++] = QP_DOUBLE;
        memcpy(packer->buffer + packer->len, &real, sizeof(double));
        packer->len += sizeof(double);
    }
    return 0;
}

#define QP_ADD_INT8(packer, integer) \
{                                                               \
    QP_RESIZE(2)                                                \
    if (integer >= 0 && integer < 64)                           \
    {                                                           \
        packer->buffer[packer->len++] = integer;                \
    }                                                           \
    else if (integer >= -60 && integer < 0)                     \
    {                                                           \
        packer->buffer[packer->len++] = 63 - integer;           \
    }                                                           \
    else                                                        \
    {                                                           \
        packer->buffer[packer->len++] = QP_INT8;                \
        packer->buffer[packer->len++] = integer;                \
    }                                                           \
    return 0;                                                   \
}
/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_int8(qp_packer_t * packer, int8_t integer)
    QP_ADD_INT8(packer, integer)

#define QP_ADD_INT16(packer, integer)                                   \
{                                                                       \
    QP_RESIZE(3)                                                        \
    packer->buffer[packer->len++] = QP_INT16;                           \
    memcpy(packer->buffer + packer->len, &integer, sizeof(int16_t));    \
    packer->len += sizeof(int16_t);                                     \
    return 0;                                                           \
}
/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_int16(qp_packer_t * packer, int16_t integer)
{
    int8_t i8;
    if ((i8 = (int8_t) integer) == integer)
        QP_ADD_INT8(packer, i8)

    QP_ADD_INT16(packer, integer)
}

#define QP_ADD_INT32(packer, integer)                                   \
{                                                                       \
    QP_RESIZE(5)                                                        \
    packer->buffer[packer->len++] = QP_INT32;                           \
    memcpy(packer->buffer + packer->len, &integer, sizeof(int32_t));    \
    packer->len += sizeof(int32_t);                                     \
    return 0;                                                           \
}
/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_int32(qp_packer_t * packer, int32_t integer)
{
    int8_t i8;
    if ((i8 = (int8_t) integer) == integer)
        QP_ADD_INT8(packer, i8)

    int16_t i16;
    if ((i16 = (int16_t) integer) == integer)
        QP_ADD_INT16(packer, i16)

    QP_ADD_INT32(packer, integer)
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_int64(qp_packer_t * packer, int64_t integer)
{
    int8_t i8;
    if ((i8 = (int8_t) integer) == integer)
        QP_ADD_INT8(packer, i8)

    int16_t i16;
    if ((i16 = (int16_t) integer) == integer)
        QP_ADD_INT16(packer, i16)

    int32_t i32;
    if ((i32 = (int32_t) integer) == integer)
        QP_ADD_INT32(packer, i32)


    QP_RESIZE(9)
    packer->buffer[packer->len++] = QP_INT64;
    memcpy(packer->buffer + packer->len, &integer, sizeof(int64_t));
    packer->len += sizeof(int64_t);
    return 0;
}

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_true(qp_packer_t * packer) QP_PLAIN_OBJ(QP_TRUE)

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_false(qp_packer_t * packer) QP_PLAIN_OBJ(QP_FALSE)

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_null(qp_packer_t * packer) QP_PLAIN_OBJ(QP_NULL)

/*
 * Returns 0 if successful; -1 and a SIGNAL is raised in case an error occurred.
 */
int qp_add_type(qp_packer_t * packer, qp_types_t tp)
{
#ifdef DEBUG
    assert(tp >= QP_ARRAY0 && tp <= QP_MAP_CLOSE);
#endif

    QP_RESIZE(1)
    packer->buffer[packer->len++] = tp;
    return 0;
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_type(qp_fpacker_t * fpacker, qp_types_t tp)
{
#ifdef DEBUG
    assert(tp >= QP_ARRAY0 && tp <= QP_MAP_CLOSE);
#endif

    return (fputc(tp, fpacker) == tp) ? 0 : EOF;
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_raw(qp_fpacker_t * fpacker, const char * raw, size_t len)
{
    if (len < 100)
    {
        if (fputc(128 + len, fpacker) == EOF)
        {
            return EOF;
        }
    }
    else if (len < 256)
    {
        if (    fputc(QP_RAW8, fpacker) == EOF ||
                fputc(len, fpacker) == EOF)
        {
            return EOF;
        }
    }
    else if (len < 65536)
    {
        if (    fputc(QP_RAW16, fpacker) == EOF ||
                fwrite(&len, sizeof(uint16_t), 1, fpacker) != 1)
        {
            return EOF;
        }
    }
    else if (len < 4294967296)
    {
        if (    fputc(QP_RAW32, fpacker) == EOF ||
                fwrite(&len, sizeof(uint32_t), 1, fpacker) != 1)
        {
            return EOF;
        }
    }
    else
    {
        if (    fputc(QP_RAW64, fpacker) == EOF ||
                fwrite(&len, sizeof(uint64_t), 1, fpacker) != 1)
        {
            return EOF;
        }
    }
    return (fwrite(raw, len, 1, fpacker) == 1) ? 0 : EOF;
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_string(qp_fpacker_t * fpacker, const char * str)
{
    return qp_fadd_raw(fpacker, str, strlen(str));
}

#define QP_FADD_INT8(fpacker, integer)                                      \
{                                                                           \
    if (integer >= 0 && integer < 64)                                       \
    {                                                                       \
        return (fputc(integer, fpacker) != EOF) ? 0 : EOF;                  \
    }                                                                       \
    if (integer >= -60 && integer < 0)                                      \
    {                                                                       \
        return (fputc(63 - integer, fpacker) != EOF) ? 0 : EOF;             \
    }                                                                          \
    return (fputc(QP_INT8, fpacker) != EOF && fputc(integer, fpacker) != EOF)  \
            ? 0 : EOF;                                                         \
}
/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_int8(qp_fpacker_t * fpacker, int8_t integer)
    QP_FADD_INT8(fpacker, integer)

#define QP_FADD_INT16(fpacker, integer)                                     \
{                                                                           \
    return (fputc(QP_INT16, fpacker) != EOF &&                              \
            fwrite(&integer, sizeof(int16_t), 1, fpacker) == 1) ? 0 : EOF;  \
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_int16(qp_fpacker_t * fpacker, int16_t integer)
{
    int8_t i8;
    if ((i8 = (int8_t) integer) == integer)
        QP_FADD_INT8(fpacker, i8)

    QP_FADD_INT16(fpacker, integer)
}

#define QP_FADD_INT32(fpacker, integer)                                     \
{                                                                           \
    return (fputc(QP_INT32, fpacker) != EOF &&                              \
            fwrite(&integer, sizeof(int32_t), 1, fpacker) == 1) ? 0 : EOF;  \
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_int32(qp_fpacker_t * fpacker, int32_t integer)
{
    int8_t i8;
    if ((i8 = (int8_t) integer) == integer)
        QP_FADD_INT8(fpacker, i8)

    int16_t i16;
    if ((i16 = (int16_t) integer) == integer)
        QP_FADD_INT16(fpacker, i16)

    QP_FADD_INT32(fpacker, integer)
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_int64(qp_fpacker_t * fpacker, int64_t integer)
{
    int8_t i8;
    if ((i8 = (int8_t) integer) == integer)
        QP_FADD_INT8(fpacker, i8)

    int16_t i16;
    if ((i16 = (int16_t) integer) == integer)
        QP_FADD_INT16(fpacker, i16)

    int32_t i32;
    if ((i32 = (int32_t) integer) == integer)
        QP_FADD_INT32(fpacker, i32)

    return (fputc(QP_INT64, fpacker) != EOF &&
            fwrite(&integer, sizeof(int64_t), 1, fpacker) == 1) ? 0 : EOF;
}

/*
 * Returns 0 if successful and EOF in case an error occurred.
 */
int qp_fadd_double(qp_fpacker_t * fpacker, double real)
{
    if (real == 0.0)
    {
        return (fputc(QP_DOUBLE_0, fpacker) != EOF) ? 0 : EOF;
    }
    else if (real == 1.0)
    {
        return (fputc(QP_DOUBLE_1, fpacker) != EOF) ? 0 : EOF;
    }
    else if (real == -1.0)
    {
        return (fputc(QP_DOUBLE_N1, fpacker) != EOF) ? 0 : EOF;
    }
    else
    {
        return (fputc(QP_DOUBLE, fpacker) != EOF &&
                fwrite(&real, sizeof(double), 1, fpacker) == 1) ? 0 : EOF;
    }
    return 0;
}

/*
 * Jump to the next object. If 'qp_obj' is not NULL, the object will be stored
 * in qp_obj so you can use it later.
 *
 * Returns one of the following: (these are the ONLY possible return values)
 *
 *  QP_END, QP_RAW, QP_INT64, QP_DOUBLE
 *  QP_TRUE, QP_FALSE, QP_NULL, QP_ARRAY0..5, QP_MAP0..5,
 *  QP_ARRAY_OPEN, QP_ARRAY_CLOSE, QP_MAP_OPEN, QP_MAP_CLOSE
 *
 * Its fine to reuse the same object without calling free in between.
 */
qp_types_t qp_next(qp_unpacker_t * unpacker, qp_obj_t * qp_obj)
{
    uint8_t tp;

    if (unpacker->pt >= unpacker->end)
    {
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_END;
        }
        return QP_END;
    }

    tp = *unpacker->pt;
    unpacker->pt++;

    switch(tp)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_INT64;
            qp_obj->via.int64 = (int64_t) tp;
        }
        return QP_INT64;

    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
    case 98:
    case 99:
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
    case 108:
    case 109:
    case 110:
    case 111:
    case 112:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 121:
    case 122:
    case 123:
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_INT64;
            qp_obj->via.int64 = (int64_t) 63 - tp;
        }
        return QP_INT64;

    case 124:
#ifdef DEBUG
        /* Object hooks are not supported yet */
        assert (0);
#endif
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_HOOK;
        }
        return QP_HOOK;

    case 125:
    case 126:
    case 127:
        /* unpack fixed doubles -1.0, 0.0 or 1.0 */
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_DOUBLE;
            qp_obj->via.real = (double) (tp - 126);
        }
        return QP_DOUBLE;

    case 128:
    case 129:
    case 130:
    case 131:
    case 132:
    case 133:
    case 134:
    case 135:
    case 136:
    case 137:
    case 138:
    case 139:
    case 140:
    case 141:
    case 142:
    case 143:
    case 144:
    case 145:
    case 146:
    case 147:
    case 148:
    case 149:
    case 150:
    case 151:
    case 152:
    case 153:
    case 154:
    case 155:
    case 156:
    case 157:
    case 158:
    case 159:
    case 160:
    case 161:
    case 162:
    case 163:
    case 164:
    case 165:
    case 166:
    case 167:
    case 168:
    case 169:
    case 170:
    case 171:
    case 172:
    case 173:
    case 174:
    case 175:
    case 176:
    case 177:
    case 178:
    case 179:
    case 180:
    case 181:
    case 182:
    case 183:
    case 184:
    case 185:
    case 186:
    case 187:
    case 188:
    case 189:
    case 190:
    case 191:
    case 192:
    case 193:
    case 194:
    case 195:
    case 196:
    case 197:
    case 198:
    case 199:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
    case 208:
    case 209:
    case 210:
    case 211:
    case 212:
    case 213:
    case 214:
    case 215:
    case 216:
    case 217:
    case 218:
    case 219:
    case 220:
    case 221:
    case 222:
    case 223:
    case 224:
    case 225:
    case 226:
    case 227:
        /* unpack fixed sized raw strings */
        {
            size_t size = tp - 128;
            QP_UNPACK_CHECK_SZ(size)

            if (qp_obj != NULL)
            {
                qp_obj->tp = QP_RAW;
                qp_obj->via.raw = unpacker->pt;
                qp_obj->len = size;
            }
            unpacker->pt += size;
            return QP_RAW;
        }
    case 228:
        QP_UNPACK_RAW(uint8_t)
    case 229:
        QP_UNPACK_RAW(uint16_t)
    case 230:
        QP_UNPACK_RAW(uint32_t)
    case 231:
        QP_UNPACK_RAW(uint64_t)

    case 232:
        QP_UNPACK_INT(int8_t)
    case 233:
        QP_UNPACK_INT(int16_t)
    case 234:
        QP_UNPACK_INT(int32_t)
    case 235:
        QP_UNPACK_INT(int64_t)

    case 236:
        QP_UNPACK_CHECK_SZ(sizeof(double))
        if (qp_obj != NULL)
        {
            qp_obj->tp = QP_DOUBLE;
            qp_obj->via.real = (double) *((double *) unpacker->pt);
        }
        unpacker->pt += sizeof(double);
        return QP_DOUBLE;

    case 237:
    case 238:
    case 239:
    case 240:
    case 241:
    case 242:
    case 243:
    case 244:
    case 245:
    case 246:
    case 247:
    case 248:
    case 249:
    case 250:
    case 251:
    case 252:
    case 253:
    case 254:
    case 255:
        if (qp_obj != NULL)
        {
            qp_obj->tp = tp;
        }
        return tp;
    }

    assert (0);
    return -1;
}

/*
 * Returns one of the following: (these are the ONLY possible return values)
 *
 *  QP_END, QP_HOOK, QP_RAW, QP_INT64, QP_DOUBLE
 *  QP_TRUE, QP_FALSE, QP_NULL, QP_ARRAY0..5, QP_MAP0..5,
 *  QP_ARRAY_OPEN, QP_ARRAY_CLOSE, QP_MAP_OPEN, QP_MAP_CLOSE
 */
qp_types_t qp_current(qp_unpacker_t * unpacker)
{
    if (unpacker->pt >= unpacker->end)
    {
        return QP_END;
    }

    switch ((uint8_t) *unpacker->pt)
    {
    case 124:
        return QP_HOOK;
    case 125:
    case 126:
    case 127:
        return QP_DOUBLE;
    case 128:
    case 129:
    case 130:
    case 131:
    case 132:
    case 133:
    case 134:
    case 135:
    case 136:
    case 137:
    case 138:
    case 139:
    case 140:
    case 141:
    case 142:
    case 143:
    case 144:
    case 145:
    case 146:
    case 147:
    case 148:
    case 149:
    case 150:
    case 151:
    case 152:
    case 153:
    case 154:
    case 155:
    case 156:
    case 157:
    case 158:
    case 159:
    case 160:
    case 161:
    case 162:
    case 163:
    case 164:
    case 165:
    case 166:
    case 167:
    case 168:
    case 169:
    case 170:
    case 171:
    case 172:
    case 173:
    case 174:
    case 175:
    case 176:
    case 177:
    case 178:
    case 179:
    case 180:
    case 181:
    case 182:
    case 183:
    case 184:
    case 185:
    case 186:
    case 187:
    case 188:
    case 189:
    case 190:
    case 191:
    case 192:
    case 193:
    case 194:
    case 195:
    case 196:
    case 197:
    case 198:
    case 199:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
    case 208:
    case 209:
    case 210:
    case 211:
    case 212:
    case 213:
    case 214:
    case 215:
    case 216:
    case 217:
    case 218:
    case 219:
    case 220:
    case 221:
    case 222:
    case 223:
    case 224:
    case 225:
    case 226:
    case 227:
    case 228:
    case 229:
    case 230:
    case 231:
        return QP_RAW;
    case 232:
    case 233:
    case 234:
    case 235:
        return QP_INT64;
    case 236:
        return QP_DOUBLE;
    case 237:
    case 238:
    case 239:
    case 240:
    case 241:
    case 242:
    case 243:
    case 244:
    case 245:
    case 246:
    case 247:
    case 248:
    case 249:
    case 250:
    case 251:
    case 252:
    case 253:
    case 254:
    case 255:
        return (uint8_t) *unpacker->pt;
    default:
        return QP_INT64;
    }
}

/*
 * Recursive function for printing qpack data. (this function uses an unpacker
 * object but its also used when using qp_packer_print().
 */
static qp_types_t QP_print_unpacker(
        qp_types_t tp,
        qp_unpacker_t * unpacker,
        qp_obj_t * qp_obj)
{
    int count;
    int found;
    switch (tp)
    {
    case QP_INT64:
        printf("%lld", (long long) qp_obj->via.int64);
        break;
    case QP_DOUBLE:
        printf("%f", qp_obj->via.real);
        break;
    case QP_RAW:
        printf("\"%.*s\"", (int) qp_obj->len, qp_obj->via.raw);
        break;
    case QP_TRUE:
        printf("true");
        break;
    case QP_FALSE:
        printf("false");
        break;
    case QP_NULL:
        printf("null");
        break;
    case QP_ARRAY0:
    case QP_ARRAY1:
    case QP_ARRAY2:
    case QP_ARRAY3:
    case QP_ARRAY4:
    case QP_ARRAY5:
        printf("[");
        count = tp - QP_ARRAY0;
        tp = qp_next(unpacker, qp_obj);
        for (found = 0; count-- && tp; found = 1)
        {
            if (found )
            {
                printf(", ");
            }
            tp = QP_print_unpacker(tp, unpacker, qp_obj);
        }
        printf("]");
        return tp;
    case QP_MAP0:
    case QP_MAP1:
    case QP_MAP2:
    case QP_MAP3:
    case QP_MAP4:
    case QP_MAP5:
        printf("{");
        count = tp - QP_MAP0;
        tp = qp_next(unpacker, qp_obj);
        for (found = 0; count-- && tp; found = 1)
        {
            if (found )
            {
                printf(", ");
            }
            tp = QP_print_unpacker(tp, unpacker, qp_obj);
            printf(": ");
            tp = QP_print_unpacker(tp, unpacker, qp_obj);
        }
        printf("}");
        return tp;
    case QP_ARRAY_OPEN:
        printf("[");
        tp = qp_next(unpacker, qp_obj);
        for (count = 0; tp && tp != QP_ARRAY_CLOSE; count = 1)
        {
            if (count)
            {
                printf(", ");
            }
            tp = QP_print_unpacker(tp, unpacker, qp_obj);
        }
        printf("]");
        break;
    case QP_MAP_OPEN:
        printf("{");
        tp = qp_next(unpacker, qp_obj);
        for (count = 0; tp && tp != QP_MAP_CLOSE; count = 1)
        {
            if (count)
            {
                printf(", ");
            }
            tp = QP_print_unpacker(tp, unpacker, qp_obj);
            printf(": ");
            tp = QP_print_unpacker(tp, unpacker, qp_obj);
        }
        printf("}");
        break;
    default:
        break;
    }
    return qp_next(unpacker, qp_obj);
}
