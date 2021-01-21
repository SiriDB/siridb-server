/*
 * tags.h - Tag (tagged series).
 */
#ifndef SIRIDB_TAGS_H_
#define SIRIDB_TAGS_H_

typedef struct siridb_tags_s siridb_tags_t;

#include <inttypes.h>
#include <ctree/ctree.h>
#include <vec/vec.h>
#include <uv.h>
#include <siri/db/db.h>
#include <siri/db/tag.h>

#define SIRIDB_TAGS_PATH "tags/"

enum
{
    TAGS_FLAG_DROPPED_SERIES    = 1<<0,
    TAGS_FLAG_REQUIRE_SAVE      = 1<<1,
};

struct siridb_tags_s
{
    uint16_t flags;
    uint16_t ref;
    uint32_t pad0;
    uint64_t next_id;
    char * path;
    ct_t * tags;
    uv_mutex_t mutex;
};

int siridb_tags_init(siridb_t * siridb);
int siridb_tags_drop_tag(
        siridb_tags_t * tags,
        const char * name,
        char * err_msg);
siridb_tag_t * siridb_tags_add_n(
        siridb_tags_t * tags,
        const char * name,
        size_t name_len);
siridb_tag_t * siridb_tags_add(siridb_tags_t * tags, const char * name);
void siridb_tags_dropped_series(siridb_tags_t * tags);
void siridb_tags_save(siridb_tags_t * tags);
void siridb_tags_init_nseries(siridb_tags_t * tags);
sirinet_pkg_t * siridb_tags_pkg(siridb_tags_t * tags, uint16_t pid);
sirinet_pkg_t * siridb_tags_series(siridb_series_t * series);
sirinet_pkg_t * siridb_tags_empty(siridb_tags_t * tags);
void siridb__tags_free(siridb_tags_t * tags);

#define siridb_tags_set_require_save(__tags, __tag) \
do{                                                 \
    (__tags)->flags |= TAGS_FLAG_REQUIRE_SAVE;      \
    (__tag)->flags |= TAG_FLAG_REQUIRE_SAVE;        \
}while(0)


#define siridb_tags_incref(tags__) __atomic_add_fetch(&(tags__)->ref, 1, __ATOMIC_SEQ_CST)
#define siridb_tags_decref(tags__) \
        if (!__atomic_sub_fetch(&(tags__)->ref, 1, __ATOMIC_SEQ_CST)) siridb__tags_free(tags__)

#endif  /* SIRIDB_TAGS_H_ */
