/*
 * heartbeat.h - Heart-beat task SiriDB.
 *
 * There is one and only one heart-beat task thread running for SiriDB. For
 * this reason we do not need to parse data but we should only take care for
 * locks while writing data.
 */
#ifndef SIRI_HEARTBEAT_H_
#define SIRI_HEARTBEAT_H_

#include <siri/siri.h>

void siri_heartbeat_init(siri_t * siri);
void siri_heartbeat_stop(siri_t * siri);
void siri_heartbeat_force(void);

#endif  /* SIRI_HEARTBEAT_H_ */
