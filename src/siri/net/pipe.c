/*
 * pipe.c - Named Pipe support.
 */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <siri/net/pipe.h>
#include <siri/siri.h>
#include <xpath/xpath.h>
#include <logger/logger.h>

#define PIPE_NAME_BUF_SZ XPATH_MAX

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

/*
 * Cleanup socket (pipe) file. (Unused)
 */
void sirinet_pipe_unlink(uv_pipe_t * client)
{
    char * pipe_name = sirinet_pipe_name(client);
    if (pipe_name != NULL)
    {
        log_debug("Unlink named pipe: '%s'", pipe_name);
        uv_fs_t * req = malloc(sizeof(uv_fs_t));
        if (req != NULL)
        {
            uv_fs_unlink(siri.loop, req, pipe_name, (uv_fs_cb) free);
        }
        free(pipe_name);
    }
}
