/*
 * user.c - SiriDB Administrative User.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2017
 *
 */
#include <siri/admin/user.h>
#include <stddef.h>

#define SIRI_ADMIN_USER_SCHEMA 1
#define FILENAME ".users.dat"

typedef struct siri_admin_user_s
{
    char * name;
    char * password; /* keeps an encrypted password */
} siri_admin_user_t;

static int USER_free(void * user, void * args);

int siri_admin_user_init(siri_t * siri)
{
    qp_unpacker_t * unpacker;
    qp_obj_t qp_schema;
    char fn[strlen(siri->cfg->default_db_path) + strlen(FILENAME) + 1];

    /* initialize linked list */
    siri->users = llist_new();
    if (siri->users == NULL)
    {
        return -1;
    }

    /* make filename */
    sprintf(fn, "%s%s", siri->cfg->default_db_path, FILENAME);

    if (!xpath_file_exist(fn))
    {
        /* we do not have a user access file, lets create the first user */
        return siri_admin_user_new(siri, "iris", "siri");
    }

    if ((unpacker = qp_unpacker_ff(fn)) == NULL)
    {
        return -1;  /* a signal is raised is case of a memory error */
    }

    if (    !qp_is_array(qp_next(unpacker, NULL)) ||
            qp_next(unpacker, &qp_schema) != QP_INT64 ||
            qp_schema.via.int64 != SIRI_ADMIN_USER_SCHEMA)
    {
        log_critical("Invalid schema detected in '%s'", fn);
        qp_unpacker_ff_free(unpacker);
        return -1;
    }

    return 0;
}

void siri_admin_user_destroy(siri_t * siri)
{
    if (siri->users != NULL)
    {
        llist_free_cb(siri->users, USER_free, NULL);
    }
}

static int USER_free(siri_admin_user_t * user, void * args)
{
    free(user->name);
    free(user->password);
    free(user);
    return 0;
}
