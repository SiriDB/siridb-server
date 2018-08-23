#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <siri/net/pipe.h>
#include <xpath/xpath.h>

#define PIPE_NAME_BUF_SZ SIRI_PATH_MAX

/*
 * Return a name for the connection if successful or NULL in case of a failure.
 *
 * The returned value is malloced and should be freed.
 */
char * sirinet_pipe_name(uv_pipe_t * client)
{
    size_t len = PIPE_NAME_BUF_SZ - 1;
    char * buffer = malloc(PIPE_NAME_BUF_SZ);

    if (buffer == NULL || uv_pipe_getsockname(client, buffer, &len))
    {
        free(buffer);
        return NULL;
    }

    buffer[len] = '\0';
    return buffer;

}
