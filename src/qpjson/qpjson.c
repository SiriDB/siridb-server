/*
 * qpjson.c - Convert between QPack and JSON
 */
#include <assert.h>
#include <qpjson/qpjson.h>
#include <qpack/qpack.h>

static yajl_gen_status qp__to_json(yajl_gen g, qp_unpacker_t * up, qp_obj_t * obj)
{
    switch (obj->tp)
    {
    case QP_ERR:
    case QP_ARRAY_CLOSE:
    case QP_MAP_CLOSE:
    case QP_END:
        /* END, ARRAY_CLOSE and MAP_CLOSE should never occur since they can
         * only happen after "*OPEN", and this case is handled in the *OPEN
         * parts itself
         */
        return yajl_gen_in_error_state;
    case QP_INT64:
        return yajl_gen_integer(g, obj->via.int64);
    case QP_DOUBLE:
        return yajl_gen_double(g, obj->via.real);
    case QP_RAW:
        return yajl_gen_string(
                g, (unsigned char *) obj->via.raw, obj->len);
    case QP_TRUE:
        return yajl_gen_bool(g, 1 /* true */);
    case QP_FALSE:
        return yajl_gen_bool(g, 0 /* false */);
    case QP_NULL:
        return yajl_gen_null(g);
    case QP_ARRAY0:
    case QP_ARRAY1:
    case QP_ARRAY2:
    case QP_ARRAY3:
    case QP_ARRAY4:
    case QP_ARRAY5:
    {
        size_t i = obj->tp - QP_ARRAY0;

        yajl_gen_status stat;
        if ((stat = yajl_gen_array_open(g)) != yajl_gen_status_ok)
            return stat;

        while (i--)
        {
            qp_next(up, obj);
            if ((stat = qp__to_json(g, up, obj)) != yajl_gen_status_ok)
                return stat;
        }

        return yajl_gen_array_close(g);
    }
    case QP_MAP0:
    case QP_MAP1:
    case QP_MAP2:
    case QP_MAP3:
    case QP_MAP4:
    case QP_MAP5:
    {
        size_t i = obj->tp - QP_MAP0;
        yajl_gen_status stat;
        if ((stat = yajl_gen_map_open(g)) != yajl_gen_status_ok)
            return stat;

        while (i--)
        {
            qp_next(up, obj);
            if ((stat = qp__to_json(g, up, obj)) != yajl_gen_status_ok)
                return stat;

            qp_next(up, obj);
            if ((stat = qp__to_json(g, up, obj)) != yajl_gen_status_ok)
                return stat;
        }

        return yajl_gen_map_close(g);
    }
    case QP_ARRAY_OPEN:
    {
        yajl_gen_status stat;
        if ((stat = yajl_gen_array_open(g)) != yajl_gen_status_ok)
            return stat;

        while(qp_next(up, obj) && obj->tp != QP_ARRAY_CLOSE)
        {
            if ((stat = qp__to_json(g, up, obj)) != yajl_gen_status_ok)
                return stat;
        }

        return yajl_gen_array_close(g);
    }
    case QP_MAP_OPEN:
    {
        yajl_gen_status stat;
        if ((stat = yajl_gen_map_open(g)) != yajl_gen_status_ok)
            return stat;

        while(qp_next(up, obj) && obj->tp != QP_MAP_CLOSE)
        {
            if ((stat = qp__to_json(g, up, obj)) != yajl_gen_status_ok)
                return stat;

            qp_next(up, obj);
            if ((stat = qp__to_json(g, up, obj)) != yajl_gen_status_ok)
                return stat;
        }
        return yajl_gen_map_close(g);
    }
    }
    return yajl_gen_in_error_state;
}


yajl_gen_status qpjson_qp_to_json(
        void * src,
        size_t src_n,
        unsigned char ** dst,
        size_t * dst_n,
        int flags)
{
    qp_unpacker_t up;
    qp_obj_t obj;
    yajl_gen g;
    yajl_status stat;

    assert (src_n);
    qp_unpacker_init(&up, src, src_n);

    g = yajl_gen_alloc(NULL);
    if (!g)
        return yajl_gen_in_error_state;

    yajl_gen_config(g, yajl_gen_beautify, flags & QPJSON_FLAG_BEAUTIFY);
    yajl_gen_config(g, yajl_gen_validate_utf8, flags & QPJSON_FLAG_VALIDATE_UTF8);

    qp_next(&up, &obj);

    assert (obj.tp != QP_END);

    if ((stat = qp__to_json(g, &up, &obj)) == yajl_status_ok)
    {
        const unsigned char * tmp;
        yajl_gen_get_buf(g, &tmp, dst_n);
        *dst = malloc(*dst_n);
        if (*dst)
            memcpy(*dst, tmp, *dst_n);
        else
            stat = yajl_gen_in_error_state;
    }

    yajl_gen_free(g);
    return stat;
}


yajl_status qpjson_json_to_qp(
        const void * src,
        size_t src_n,
        char ** dst,
        size_t * dst_n)
{
    yajl_handle hand;
    yajl_status stat = yajl_status_error;
    qp_packer_t * packer = qp_packer_new(src_n);
    if (!packer)
        return stat;

//    hand = yajl_alloc(&callbacks, NULL, c);
//    if (!hand)
//        goto fail1;
//
//    stat = yajl_parse(hand, src, src_n);
//    if (stat == yajl_status_ok)
//        take_buffer(&buffer, dst, dst_n);
//    yajl_free(hand);

    qp_packer_free(packer);
    return stat;
}
