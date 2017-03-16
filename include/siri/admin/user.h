/*
 * user.h - SiriDB Administrative User.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2017
 *
 */
#pragma once
#include <qpack/qpack.h>
#include <siri/siri.h>

typedef struct siri_s siri_t;

typedef struct siri_admin_user_s
{
    char * name;
    char * password; /* keeps an encrypted password */
} siri_admin_user_t;

int siri_admin_user_init(siri_t * siri);
void siri_admin_user_destroy(siri_t * siri);
int siri_admin_user_new(
        siri_t * siri,
        qp_obj_t * qp_name,
        qp_obj_t * qp_password,
        int is_encrypted);
int siri_admin_user_check(
        siri_t * siri,
        qp_obj_t * qp_name,
        qp_obj_t * qp_password);
int siri_admin_user_change_password(
        siri_t * siri,
        qp_obj_t * qp_name,
        qp_obj_t * qp_password);
int siri_admin_user_drop(siri_t * siri, qp_obj_t * qp_name);
int siri_admin_user_save(siri_t * siri);
