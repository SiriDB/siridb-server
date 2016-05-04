/*
 * user.c - contains functions for a SiriDB database member.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */

#include <siri/db/user.h>
#include <siri/grammar/grammar.h>

siridb_user_t * siridb_user_new(void)
{
    siridb_user_t * user = (siridb_user_t *) malloc(sizeof(siridb_user_t));
    user->access_bit = 0;
    user->password = NULL;
    user->username = NULL;
    return user;
}

void siridb_user_free(siridb_user_t * user)
{
    if (user != NULL)
    {
        free(user->username);
        free(user->password);
        free(user);
    }
}

void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop)
{
    switch (prop)
    {
    case CLERI_GID_K_USER:
        qp_add_string(packer, user->username);
        break;
    case CLERI_GID_K_ACCESS:
        qp_add_string(packer, "dummy");
        break;
    }
}
