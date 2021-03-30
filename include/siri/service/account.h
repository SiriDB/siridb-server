/*
 * account.h - SiriDB Service Account.
 */
#ifndef SIRI_SERVICE_ACCOUNT_H_
#define SIRI_SERVICE_ACCOUNT_H_

typedef struct siri_service_account_s siri_service_account_t;

#include <qpack/qpack.h>
#include <siri/siri.h>
#include <stdbool.h>

int siri_service_account_init(siri_t * siri);
void siri_service_account_destroy(siri_t * siri);
int siri_service_account_new(
        siri_t * siri,
        qp_obj_t * qp_account,
        qp_obj_t * qp_password,
        int is_encrypted,
        char * err_msg);
int siri_service_account_check(
        siri_t * siri,
        qp_obj_t * qp_account,
        qp_obj_t * qp_password,
        char * err_msg);
int siri_service_account_change_password(
        siri_t * siri,
        qp_obj_t * qp_account,
        qp_obj_t * qp_password,
        char * err_msg);
bool siri_service_account_check_basic(
        siri_t * siri,
        const char * data,
        size_t n);
int siri_service_account_drop(
        siri_t * siri,
        qp_obj_t * qp_account,
        char * err_msg);
int siri_service_account_save(siri_t * siri, char * err_msg);

struct siri_service_account_s
{
    char * account;
    char * password; /* keeps an encrypted password */
};

#endif  /* SIRI_SERVICE_ACCOUNT_H_ */
