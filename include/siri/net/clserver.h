/*
 * clserver.h - Listen for client requests.
 */
#ifndef SIRINET_CLSERVER_H_
#define SIRINET_CLSERVER_H_

#include <uv.h>
#include <siri/siri.h>

typedef ssize_t (*sirinet_clserver_getfile)(char ** buffer, siridb_t * siridb);

int sirinet_clserver_init(siri_t * siri);

#endif  /* SIRINET_CLSERVER_H_ */
