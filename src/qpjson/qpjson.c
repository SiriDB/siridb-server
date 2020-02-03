/*
 * qpjson.c - Convert between QPack and JSON
 */
#include <assert.h>
#include <qpjson/qpjson.h>
#include <qpack/qpack.h>
#include <logger/logger.h>

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
    yajl_gen_status stat;

    if (src_n == 0)
    {
        *dst_n = 0;
        *dst = NULL;
        return yajl_gen_status_ok;
    }

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

static int reformat_null(void * ctx)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_null(pk);
}

static int reformat_boolean(void * ctx, int boolean)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == (boolean ? qp_add_true(pk) : qp_add_false(pk));
}

static int reformat_integer(void * ctx, long long i)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_int64(pk, i);
}

static int reformat_double(void * ctx, double d)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_double(pk, d);
}

static int reformat_string(void * ctx, const unsigned char * s, size_t n)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_raw(pk, s, n);
}

static int reformat_map_key(void * ctx, const unsigned char * s, size_t n)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_raw(pk, s, n);
}

static int reformat_start_map(void * ctx)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_type(pk, QP_MAP_OPEN);
}


static int reformat_end_map(void * ctx)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_type(pk, QP_MAP_CLOSE);
}

static int reformat_start_array(void * ctx)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_type(pk, QP_ARRAY_OPEN);
}

static int reformat_end_array(void * ctx)
{
    qp_packer_t * pk = (qp_packer_t *) ctx;
    return 0 == qp_add_type(pk, QP_ARRAY_CLOSE);
}

static void take_buffer(
        qp_packer_t * pk,
        char ** dst,
        size_t * dst_n)
{
    *dst = (char *) pk->buffer;
    *dst_n = pk->len;
    pk->buffer = NULL;
    pk->buffer_size = 0;
}

static yajl_callbacks qpjson__callbacks = {
    reformat_null,
    reformat_boolean,
    reformat_integer,
    reformat_double,
    NULL,
    reformat_string,
    reformat_start_map,
    reformat_map_key,
    reformat_end_map,
    reformat_start_array,
    reformat_end_array
};

yajl_status qpjson_json_to_qp(
        const void * src,
        size_t src_n,
        char ** dst,
        size_t * dst_n)
{
    yajl_handle hand;
    yajl_status stat = yajl_status_error;
    qp_packer_t * pk = qp_packer_new(src_n);

    if (pk == NULL)
        return stat;


    hand = yajl_alloc(&qpjson__callbacks, NULL, pk);
    if (!hand)
        goto fail1;

    stat = yajl_parse(hand, src, src_n);

    if (stat == yajl_status_ok)
        take_buffer(pk, dst, dst_n);

    yajl_free(hand);

fail1:
    qp_packer_free(pk);
    return stat;
}
