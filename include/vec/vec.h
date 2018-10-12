/*
 * vec.h - Vector List.
 */
#ifndef VEC_H_
#define VEC_H_

#define VEC_DEFAULT_SIZE 8

typedef struct vec_s vec_t;
typedef struct vec_object_s vec_object_t;

#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>

vec_t * vec_new(size_t size);
vec_t * vec_copy(vec_t * source);
void vec_compact(vec_t ** vec);
int vec_append_safe(vec_t ** vec, void * data);

/*
 * Expects the object to have a object->ref (uint_xxx_t) on top of the
 * objects definition.
 */
#define vec_object_incref(object) ((vec_object_t * ) object)->ref++

/*
 * Expects the object to have a object->ref (uint_xxx_t) on top of the
 * objects definition.
 *
 * WARNING: Be careful using 'vec_object_decref' only when being sure
 *          there are still references left on the object since an object
 *          probably needs specific cleanup tasks.
 */
#define vec_object_decref(object) ((vec_object_t * ) object)->ref--

/*
 * Append data to the list. This functions assumes the list can hold the new
 * data is therefore not safe.
 */
#define vec_append(vec, _data) vec->data[vec->len++] = _data

/*
 * Destroy the vector.
 */
#define vec_free(vec) free(vec)

/*
 * Pop the last item from the list
 */
#define vec_pop(vec) vec->data[--vec->len]

struct vec_object_s
{
    uint32_t ref;
};

struct vec_s
{
    size_t size;
    size_t len;
    void * data[];
};

#endif  /* VEC_H_ */
