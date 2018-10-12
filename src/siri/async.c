/*
 * async.c - SiriDB async wrapper.
 */
#include <logger/logger.h>
#include <siri/async.h>

/*
 * Used as uv_close_cb for closing  uv_async_t
 */
void siri_async_close(uv_handle_t * handle)
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


