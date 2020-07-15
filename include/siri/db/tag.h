/*
 * tag.h - Tag (tag series).
 */
#ifndef SIRIDB_TAG_H_
#define SIRIDB_TAG_H_

typedef struct siridb_tag_s siridb_tag_t;

enum
{
    TAG_FLAG_CLEANUP        = 1<<0,
    TAG_FLAG_REQUIRE_SAVE   = 1<<1,
};

#include <inttypes.h>
#include <imap/imap.h>
#include <siri/db/db.h>

siridb_tag_t * siridb_tag_new(uint32_t id, const char * tags_path);
void siridb__tag_decref(siridb_tag_t * tag);
void siridb__tag_free(siridb_tag_t * tag);
int siridb_tag_is_valid_fn(const char * fn);
siridb_tag_t * siridb_tag_load(siridb_t * siridb, const char * fn);
int siridb_tag_save(siridb_tag_t * tag);
int siridb_tag_is_remote_prop(uint32_t prop);
void siridb_tag_prop(siridb_tag_t * tag, qp_packer_t * packer, int prop);
int siridb_tag_cexpr_cb(siridb_tag_t * tag, cexpr_condition_t * cond);


#define siridb_tag_incref(tag__) (tag__)->ref++
#define siridb_tag_decref(tag__) \
        if (!--(tag__)->ref) siridb__tag_free(tag__)

struct siridb_tag_s
{
    uint16_t ref;
    uint16_t flags;
    uint32_t n;
    char * name;
    imap_t * series;
};


#endif  /* SIRIDB_TAG_H_ */
