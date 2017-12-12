/*
 * qpack.h - efficient binary serialization format
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 11-03-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define QP_SUGGESTED_SIZE 65536

typedef enum
{
    /*
     * Values with -##- will never be returned while unpacking. For example
     * a QP_INT8 (1 byte signed integer) will be returned as QP_INT64.
     */
    QP_END,             // at the end while unpacking
    QP_ERR,             // error
    QP_RAW,             // raw string
    /*
     * Both END and RAW are never actually packed but 0 and 1 are reserved
     * for positive signed integers.
     *
     * Fixed positive integers from 0 till 63       [  0...63  ]
     *
     * Fixed negative integers from -60 till -1     [ 64...123 ]
     *
     */
    QP_HOOK=124,        // Hook is not used by SiriDB
    QP_DOUBLE_N1=125,   // ## double value -1.0
    QP_DOUBLE_0,        // ## double value 0.0
    QP_DOUBLE_1,        // ## double value 1.0
    /*
     * Fixed raw strings lengths from 0 till 99     [ 128...227 ]
     */
    QP_RAW8=228,        // ## raw string with length < 1 byte
    QP_RAW16,           // ## raw string with length < 1 byte
    QP_RAW32,           // ## raw string with length < 1 byte
    QP_RAW64,           // ## raw string with length < 1 byte
    QP_INT8,            // ## 1 byte signed integer
    QP_INT16,           // ## 2 byte signed integer
    QP_INT32,           // ## 4 byte signed integer
    QP_INT64,           // 8 bytes signed integer
    QP_DOUBLE,          // 8 bytes double
    QP_ARRAY0,          // empty array
    QP_ARRAY1,          // array with 1 item
    QP_ARRAY2,          // array with 2 items
    QP_ARRAY3,          // array with 3 items
    QP_ARRAY4,          // array with 4 items
    QP_ARRAY5,          // array with 5 items
    QP_MAP0,            // empty map
    QP_MAP1,            // map with 1 item
    QP_MAP2,            // map with 2 items
    QP_MAP3,            // map with 3 items
    QP_MAP4,            // map with 4 items
    QP_MAP5,            // map with 5 items
    QP_TRUE,            // boolean true
    QP_FALSE,           // boolean false
    QP_NULL,            // null (none, nil)
    QP_ARRAY_OPEN,      // open a new array
    QP_MAP_OPEN,        // open a new map
    QP_ARRAY_CLOSE,     // close array
    QP_MAP_CLOSE,       // close map
} qp_types_t;

typedef union qp_via_u
{
    int64_t int64;
    uint64_t uint64;
    double real;
    unsigned char * raw;
} qp_via_t;

typedef struct qp_obj_s
{
    uint8_t tp;
    size_t len;
    qp_via_t via;
} qp_obj_t;

typedef struct qp_unpacker_s
{
    unsigned char * source; // can be NULL or a copy or the source
    unsigned char * pt;
    unsigned char * end;
} qp_unpacker_t;

typedef struct qp_packer_s
{
    size_t len;
    size_t buffer_size;
    size_t alloc_size;
    unsigned char * buffer;
} qp_packer_t;

typedef FILE qp_fpacker_t;
#define qp_open fopen    // returns NULL in case of an error
#define qp_close fclose   // 0 if successful, EOF in case of an error
#define qp_flush fflush   // 0 if successful, EOF in case of an error

/* packer: create, destroy and extend functions */
qp_packer_t * qp_packer_new(size_t alloc_size);
void qp_packer_free(qp_packer_t * packer);
int qp_packer_extend(qp_packer_t * packer, qp_packer_t * source);
int qp_packer_extend_fu(qp_packer_t * packer, qp_unpacker_t * unpacker);

/* unpacker: create and destroy functions */
void qp_unpacker_init(qp_unpacker_t * unpacker, unsigned char * pt, size_t len);
void qp_unpacker_ff_free(qp_unpacker_t * unpacker);
qp_unpacker_t * qp_unpacker_ff(const char * fn);

/* step functions to be used with an unpacker */
qp_types_t qp_next(qp_unpacker_t * unpacker, qp_obj_t * qp_obj);
qp_types_t qp_current(qp_unpacker_t * unpacker);
qp_types_t qp_skip_next(qp_unpacker_t * unpacker);

/* print function */
void qp_print(unsigned char * pt, size_t len);

/* Shortcut to print a packer object */
#define qp_packer_print(packer) \
    qp_print(packer->buffer, packer->len)

/* Shortcut to print an unpacker object */
#define qp_unpacker_print(unpacker) \
    qp_print(unpacker->pt, unpacker->end - unpacker->pt)

/* Test functions */
static inline int qp_is_array(qp_types_t tp)
{
    return tp == QP_ARRAY_OPEN || (tp >= QP_ARRAY0 && tp <= QP_ARRAY5);
}
static inline int qp_is_raw(qp_types_t tp)
{
    return tp == QP_RAW;
}
static inline int qp_is_map(qp_types_t tp)
{
    return tp == QP_MAP_OPEN || (tp >= QP_MAP0 && tp <= QP_MAP5);
}
static inline int qp_is_close(qp_types_t tp)
{
    return tp >= QP_ARRAY_CLOSE;
}
static inline int qp_is_int(qp_types_t tp)
{
    return tp == QP_INT64;
}
static inline int qp_is_double(qp_types_t tp)
{
    return tp == QP_DOUBLE;
}
static inline int qp_is_raw_term(qp_obj_t * qp_obj)
{
    return (qp_obj->tp == QP_RAW &&
            qp_obj->len &&
            qp_obj->via.raw[qp_obj->len - 1] == '\0');
}

/* Add to packer functions */
int qp_add_raw(qp_packer_t * packer, const unsigned char * raw, size_t len);
int qp_add_string(qp_packer_t * packer, const char * str);
int qp_add_string_term(qp_packer_t * packer, const char * str);

int qp_add_raw_term(qp_packer_t * packer, const unsigned char * raw, size_t len);
int qp_add_double(qp_packer_t * packer, double real);
int qp_add_int8(qp_packer_t * packer, int8_t integer);
int qp_add_int16(qp_packer_t * packer, int16_t integer);
int qp_add_int32(qp_packer_t * packer, int32_t integer);
int qp_add_int64(qp_packer_t * packer, int64_t integer);
int qp_add_true(qp_packer_t * packer);
int qp_add_false(qp_packer_t * packer);
int qp_add_null(qp_packer_t * packer);
int qp_add_type(qp_packer_t * packer, qp_types_t tp);
int qp_add_fmt(qp_packer_t * packer, const char * fmt, ...);
int qp_add_fmt_safe(qp_packer_t * packer, const char * fmt, ...);

/* Add to file-packer functions */
int qp_fadd_type(qp_fpacker_t * fpacker, qp_types_t tp);
int qp_fadd_raw(qp_fpacker_t * fpacker, const unsigned char * raw, size_t len);
int qp_fadd_string(qp_fpacker_t * fpacker, const char * str);
int qp_fadd_int8(qp_fpacker_t * fpacker, int8_t integer);
int qp_fadd_int16(qp_fpacker_t * fpacker, int16_t integer);
int qp_fadd_int32(qp_fpacker_t * fpacker, int32_t integer);
int qp_fadd_int64(qp_fpacker_t * fpacker, int64_t integer);
int qp_fadd_double(qp_fpacker_t * fpacker, double real);

/* creates a valid qpack buffer of length 3 holding an int16 type. */
#define QP_PACK_INT16(BUF__, N__) \
unsigned char BUF__[3];\
BUF__[0] = QP_INT16; \
memcpy(&BUF__[1], &N__, 2);


