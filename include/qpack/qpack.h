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

#define QP_SUGGESTED_SIZE 65536

typedef enum
{
    /*
     * Values with -##- will never be returned while unpacking. For example
     * a QP_INT8 (1 byte signed integer) will be returned as QP_INT64.
     */
    QP_ERR=-1,          // when an error occurred
    QP_END,             // at the end while unpacking
    QP_RAW,             // raw string
    /*
     * Both END and RAW are never actually packed but 0 and 1 are reseverd
     * for positive signed integers.
     *
     * Fixed positive integers from 0 till 63       [  0...63  ]
     *
     * Fixed negative integers from -61 till -1     [ 64...124 ]
     *
     */
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
    double real;
    const char * raw;
} qp_via_t;

typedef struct qp_obj_s
{
    uint8_t tp;
    size_t len;
    qp_via_t * via;
} qp_obj_t;

typedef struct qp_unpacker_s
{
    char * source; // can be NULL or a copy or the source
    const char * pt;
    const char * end;
} qp_unpacker_t;

typedef struct qp_packer_s
{
    size_t len;
    size_t buffer_size;
    size_t alloc_size;
    char * buffer;
} qp_packer_t;

typedef FILE qp_fpacker_t;
#define qp_open fopen
#define qp_close fclose
#define qp_flush fflush

qp_packer_t * qp_new_packer(size_t alloc_size);
qp_obj_t * qp_new_object(void);

void qp_free_packer(qp_packer_t * packer);

void qp_extend_packer(qp_packer_t * packer, qp_packer_t * source);

/* extend from unpacker (only the current position will be copied) */
void qp_extend_from_unpacker(qp_packer_t * packer, qp_unpacker_t * unpacker);

qp_unpacker_t * qp_new_unpacker(const char * pt, size_t len);

qp_unpacker_t * qp_from_file_unpacker(const char * fn);


void qp_free_unpacker(qp_unpacker_t * unpacker);

/* Its fine to reuse the same object without calling free in between.
 * (qp_obj may also be NULL)
 */
qp_types_t qp_next(qp_unpacker_t * unpacker, qp_obj_t * qp_obj);

void qp_free_object(qp_obj_t * qp_obj);

void qp_print(const char * pt, size_t len);

/* Shortcut to print a packer object */
#define qp_print_packer(packer) \
    qp_print(packer->buffer, packer->len)

/* Shortcut to print an unpacker object */
#define qp_print_unpacker(unpacker) \
    qp_print(unpacker->source, unpacker->end - unpacker->source)

/* Test functions */
extern int qp_is_array(qp_types_t tp);
extern int qp_is_map(qp_types_t tp);

/* Adds a raw string to the packer fixed to len chars */
void qp_add_raw(qp_packer_t * packer, const char * raw, size_t len);

/* Adds a raw string to the packer and appends a terminator (0) so the written
 * length is len + 1 */
void qp_add_raw_term(qp_packer_t * packer, const char * raw, size_t len);

/* Adds a 0 terminated string to the packer but note that the terminator itself
 * will NOT be written. (Use qp_add_string_term() instead if you want the
 * destination to be 0 terminated */
void qp_add_string(qp_packer_t * packer, const char * str);

/* Like qp_add_string() but includes the 0 terminator. */
void qp_add_string_term(qp_packer_t * packer, const char * str);

void qp_add_double(qp_packer_t * packer, double real);
void qp_add_int8(qp_packer_t * packer, int8_t integer);
void qp_add_int16(qp_packer_t * packer, int16_t integer);
void qp_add_int32(qp_packer_t * packer, int32_t integer);
void qp_add_int64(qp_packer_t * packer, int64_t integer);
void qp_add_true(qp_packer_t * packer);
void qp_add_false(qp_packer_t * packer);
void qp_add_null(qp_packer_t * packer);

void qp_add_type(qp_packer_t * packer, qp_types_t tp);

/* adds a format string to the packer, but take in account that only
 * QPACK_MAX_FMT_SIZE characters are supported. (rest will be cut off)
 */
void qp_add_fmt(qp_packer_t * packer, const char * fmt, ...);

int qp_fadd_type(qp_fpacker_t * fpacker, qp_types_t tp);
int qp_fadd_raw(qp_fpacker_t * fpacker, const char * raw, size_t len);
int qp_fadd_string(qp_fpacker_t * fpacker, const char * str);
int qp_fadd_int8(qp_fpacker_t * fpacker, int8_t integer);
int qp_fadd_int16(qp_fpacker_t * fpacker, int16_t integer);
int qp_fadd_int32(qp_fpacker_t * fpacker, int32_t integer);
int qp_fadd_int64(qp_fpacker_t * fpacker, int64_t integer);

/* creates a valid qpack buffer of length 3 holding an int16 type. */
#define QP_PACK_INT16(buffer, n) \
char buffer[3];\
buffer[0] = QP_INT16; \
memcpy(&buffer[1], &n, 2);

