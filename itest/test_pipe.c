#include <uv.h>
#include <stdio.h>
#include <stdlib.h>

void OnConnect(uv_connect_t * connect, int status)
{
    printf("Hi!");
}

int main()
{
    uv_loop_t * loop = uv_default_loop();
    uv_pipe_t * handle = malloc(sizeof(uv_pipe_t));
    uv_connect_t * connect = malloc(sizeof(uv_connect_t));

    uv_pipe_init(loop, handle, 0);
    uv_pipe_open(handle, socket(PF_UNIX, SOCK_STREAM, 0));
    uv_pipe_connect(connect, handle, "/tmp/siridb_pipe_test.sock", OnConnect);

    uv_run(loop, UV_RUN_DEFAULT);
}