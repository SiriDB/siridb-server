/*
 * pipe.h - Named Pipe support.
 */
#ifndef SIRINET_PIPE_H_
#define SIRINET_PIPE_H_

#include <uv.h>

char * sirinet_pipe_name(uv_pipe_t * client);
void sirinet_pipe_unlink(uv_pipe_t * client);

#endif  /* SIRINET_PIPE_H_ */
