/*
 * qpjson.h - Convert between QPack and JSON
 */
#ifndef QPJSON_H_
#define QPJSON_H_

#include <stddef.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>

enum
{
    /* flags map to the API flags */
    QPJSON_FLAG_BEAUTIFY        =1<<2,
    QPJSON_FLAG_VALIDATE_UTF8   =1<<3,
};


yajl_gen_status qpjson_qp_to_json(
        const void * src,
        size_t src_n,
        unsigned char ** dst,
        size_t * dst_n,
        int flags);

yajl_status qpjson_json_to_qp(
        const void * src,
        size_t src_n,
        char ** dst,
        size_t * dst_n);


#endif  /* QPJSON_H_ */
