/*
 * vec.c - Vector List.
 */
#include <logger/logger.h>
#include <vec/vec.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define VEC_MAX_SZ 512

/*
 * Returns NULL in case an error has occurred.
 *
 * In case the size is unknown, VEC_DEFAULT_SIZE is recommended since in this
 * case we can do a re-allocation with multiples of 64K.
 */
vec_t * vec_new(size_t size)
{
    /* sizeof(vec_t) is 16 bytes, only for len and size and data[] is
     * excluded.
     */
    vec_t * vec = (vec_t *)
            malloc(sizeof(vec_t) + sizeof(void *) * size);

    if (vec == NULL)
    {
        return NULL;
    }
    vec->size = size;
    vec->len = 0;
    return vec;
}

/*
 * Returns NULL in case an error has occurred.
 */
vec_t * vec_copy(vec_t * source)
{
    size_t size = sizeof(vec_t) + sizeof(void *) * source->size;
    vec_t * vec = (vec_t *) malloc(size);
    if (vec == NULL)
    {
        return NULL;
    }
    memcpy(vec, source, size);
    return vec;
}

/*
 * Returns 0 if successful or -1 in case of an error.
 * (in case of an error the list is unchanged)
 */
int vec_append_safe(vec_t ** vec, void * data)
{
    if ((*vec)->len == (*vec)->size)
    {
        vec_t * tmp;

        size_t sz = (*vec)->size;

        /* double the size when > 0 and <  VEC_MAX_SZ */
        (*vec)->size = (sz >= VEC_DEFAULT_SIZE) ?
                (sz <= VEC_MAX_SZ) ?
                        sz * 2 : sz + VEC_MAX_SZ : VEC_DEFAULT_SIZE;

        tmp = (vec_t *) realloc(
                *vec,
                sizeof(vec_t) + sizeof(void *) * (*vec)->size);

        if (tmp == NULL)
        {
            /* an error has occurred */
            (*vec)->size = sz;
            return -1;
        }

        /* overwrite the original value with the new one */
        *vec = tmp;
    }

    vec_append((*vec), data);

    return 0;
}

/*
 * Compact memory used for the vec object when the list has more than
 * VEC_DEFAULT_SIZE free space. After the compact the list has
 * a size which is VEC_DEFAULT_SIZE greater than its length.
 */
void vec_compact(vec_t ** vec)
{
    size_t sz = (*vec)->size;

    if (sz - (*vec)->len > VEC_DEFAULT_SIZE)
    {
        vec_t * tmp;

        (*vec)->size = (*vec)->len + VEC_DEFAULT_SIZE;

        tmp = (vec_t *) realloc(
                *vec,
                sizeof(vec_t) + sizeof(void *) * (*vec)->size);

        if (tmp == NULL)
        {
            /* an error has occurred; log and restore size */
            log_error("Error has occurred while re-allocating less space");
            (*vec)->size = sz;
        }
        else
        {
            /* overwrite the original value with the new one */
            *vec = tmp;
        }
    }
}
