/*
 * backup.h - Set SiriDB in backup mode.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 27-09-2016
 *
 */
#pragma once

#include <siri/siri.h>
#include <siri/db/db.h>

typedef struct siri_s siri_t;
typedef struct siridb_s siridb_t;

void siri_backup_init(siri_t * siri);
void siri_backup_destroy(siri_t * siri);
int siri_backup_enable(siri_t * siri, siridb_t * siridb);
int siri_backup_disable(siri_t * siri, siridb_t * siridb);
