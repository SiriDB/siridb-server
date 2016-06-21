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
#include <siri/db/access.h>
#include <cexpr/cexpr.h>

typedef struct siridb_s siridb_t;

typedef struct siridb_user_s
{
    char * username;
    char * password; /* keeps an encrypted password */
    siridb_access_t access_bit;
    uint16_t ref;
} siridb_user_t;

siridb_user_t * siridb_user_new(void);
void siridb_user_incref(siridb_user_t * user);
void siridb_user_decref(siridb_user_t * user);
void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop);
int siridb_user_set_password(
        siridb_user_t * user,
        const char * password,
        char * err_msg);

int siridb_user_check_access(
        siridb_user_t * user,
        siridb_access_t access_bit,
        char * err_msg);

int siridb_user_cexpr_cb(siridb_user_t * user, cexpr_condition_t * cond);
