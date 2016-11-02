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
    uint16_t ref;
    uint16_t pad0;
    uint32_t access_bit;
    char * name;
    char * password; /* keeps an encrypted password */
} siridb_user_t;

siridb_user_t * siridb_user_new(void);
void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop);
int siridb_user_set_name(
        siridb_t * siridb,
        siridb_user_t * user,
        const char * name,
        char * err_msg);
int siridb_user_set_password(
        siridb_user_t * user,
        const char * password,
        char * err_msg);

int siridb_user_check_access(
        siridb_user_t * user,
        uint32_t access_bit,
        char * err_msg);

void siridb__user_free(siridb_user_t * user);

int siridb_user_cexpr_cb(siridb_user_t * user, cexpr_condition_t * cond);

/*
 * Increment the user reference counter.
 */
#define siridb_user_incref(user) user->ref++

/*
 * Decrement user reference counter and free the user when zero is reached.
 */
#define siridb_user_decref(user__) \
	if (!--user__->ref) siridb__user_free(user__)
