/*
 * qpjson.c - Convert between QPack and JSON
 */

#include <qpjson/qpjson.h>

yajl_gen_status mpjson_mp_to_json(
        const void * src,
        size_t src_n,
        unsigned char ** dst,
        size_t * dst_n,
        int flags)
