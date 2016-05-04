/*
 * user.h - contains functions for a SiriDB database member.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#pragma once

#include <qpack/qpack.h>
#include <inttypes.h>
#include <siri/db/db.h>

typedef struct siridb_s siridb_t;

typedef struct siridb_user_s
{
    char * username;
    char * password; /* keeps an encrypted password */
    uint32_t access_bit;
} siridb_user_t;

siridb_user_t * siridb_user_new(void);
void siridb_user_free(siridb_user_t * user);
void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop);
