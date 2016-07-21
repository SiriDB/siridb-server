/*
 * async.c - SiriDB async wrapper
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 21-07-2016
 *
 */
#include <siri/async.h>

inline void siri_async_incref(uv_async_t * handle)
{
    ((siri_async_t *) handle->data)->ref++;
}

/*
 * Used as uv_close_cb for closing  uv_async_t
 */
inline void siri_async_close(uv_handle_t * handle)
{
    siri_async_decref((uv_async_t **) &handle);
}

/*
 * Decrement reference counter for handle. If zero is reached the handle
 * will be freed and handle will be set to NULL.
 */
void siri_async_decref(uv_async_t ** handle)
{
    siri_async_t * shandle = (*handle)->data;
    if (!--shandle->ref)
    {
        shandle->free_cb((uv_handle_t *) *handle);
        *handle = NULL;
    }
}


