/*
 * groups.h - Groups (saved regular expressions).
 *
 * Info groups->mutex:
 *
 *  Main thread:
 *      groups->groups :    read (no lock)      write (lock)
 *      groups->nseries :   read (lock)         write (lock)
 *      groups->ngroups :   read (lock)         write (lock)
 *      group->series :     read (lock)         write (not allowed)

 *  Other threads:
 *      groups->groups :    read (lock)         write (not allowed)
 *      groups->nseries :   read (lock)         write (lock)
 *      groups->ngroups :   read (lock)         write (lock)
 *
 *  Group thread:
 *      group->series :     read (no lock)      write (lock)
 *
 *  Note:   One exception to 'not allowed' are the free functions
 *          since they only run when no other references to the object exist.
 */
#ifndef SIRIDB_GROUPS_H_
#define SIRIDB_GROUPS_H_

typedef enum
{
    GROUPS_RUNNING,
    GROUPS_STOPPING,
    GROUPS_CLOSED
} siridb_groups_status_t;

typedef struct siridb_groups_s siridb_groups_t;

#define GROUPS_FLAG_DROPPED_SERIES 1

#include <ctree/ctree.h>
#include <vec/vec.h>
#include <uv.h>
#include <siri/db/db.h>
#include <siri/net/pkg.h>

siridb_groups_t * siridb_groups_new(siridb_t * siridb);
void siridb_groups_start(siridb_groups_t * groups);
int siridb_groups_save(siridb_groups_t * groups);
ssize_t siridb_groups_get_file(char ** buffer, siridb_t * siridb);
void siridb_groups_init_nseries(siridb_groups_t * groups);
sirinet_pkg_t * siridb_groups_pkg(siridb_groups_t * groups, uint16_t pid);
int siridb_groups_drop_group(
        siridb_groups_t * groups,
        const char * name,
        char * err_msg);
void siridb_groups_destroy(siridb_groups_t * groups);
void siridb_groups_decref(siridb_groups_t * groups);
int siridb_groups_add_group(
        siridb_groups_t * groups,
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg);
int siridb_groups_add_series(
        siridb_groups_t * groups,
        siridb_series_t * series);

struct siridb_groups_s
{
    uint8_t status;
    uint8_t flags;
    uint8_t ref;
    char * fn;
    ct_t * groups;
    vec_t * nseries;  /* list of series we need to assign to groups */
    vec_t * ngroups;  /* list of groups which need initialization */
    uv_mutex_t mutex;
    uv_work_t work;
};
#endif  /* SIRIDB_GROUPS_H_ */
