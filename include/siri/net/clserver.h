/*
 * clserver.h - TCP server for serving client requests.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#ifndef SIRINET_CLSERVER_H_
#define SIRINET_CLSERVER_H_

#include <uv.h>
#include <siri/siri.h>

typedef ssize_t (*sirinet_clserver_getfile)(char ** buffer, siridb_t * siridb);

int sirinet_clserver_init(siri_t * siri);

#endif  /* SIRINET_CLSERVER_H_ */
