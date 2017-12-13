/*
 * shard.c - SiriDB Shard.

 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <ctree/ctree.h>
#include <imap/imap.h>
#include <limits.h>
#include <logger/logger.h>
#include <siri/db/series.h>
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <siri/optimize.h>
#include <siri/err.h>
#include <siri/file/pointer.h>
#include <siri/siri.h>
#include <slist/slist.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* max read buffer size used for reading from index file */
#define SIRIDB_SHARD_MAX_CHUNK_SZ 65536

/* growing with this block size */
#define SHARD_GROW_SZ 131072

/* shard schema (schemas below 20 are reserved for Python SiriDB) */
#define SIRIDB_SHARD_SHEMA 20

/*
 * Header schema layout
 *
 * Total Size 20
 * 0    (uint8_t)   SHEMA
 * 1    (uint64_t)  ID
 * 9    (uint64_t)  DURATION
 * 17   (uint16_t)  MAX_CHUCK_SZ
 * 19   (uint8_t)   TP
 * 20   (uint8_t)   TIME_PRECISION
 * 21   (uint8_t)   FLAGS
 *
 */
#define HEADER_SIZE 22
#define HEADER_SCHEMA 0
#define HEADER_ID 1
#define HEADER_DURATION 9
#define HEADER_MAX_CHUNK_SZ 17
#define HEADER_TP 19
#define HEADER_TIME_PRECISION 20
#define HEADER_FLAGS 21

/* 0    (uint32_t)  SERIES_ID
 * 4    (uint32_t)  START_TS
 * 8    (uint32_t)  END_TS
 * 12   (uint16_t)  LEN
 * 14   (uint16_t)  (OPTIONAL COMPRESSION INFO)
 */
#define IDX_NUM32_SZ 14  /* or 16 when compressed */

/* 0    (uint32_t)  SERIES_ID
 * 4    (uint64_t)  START_TS
 * 12   (uint64_t)  END_TS
 * 20   (uint16_t)  LEN
 * 22   (uint16_t)  (OPTIONAL COMPRESSION INFO)
 */
#define IDX_NUM64_SZ 22  /* or 24 when compressed */

/* 0    (uint32_t)  SERIES_ID
 * 4    (uint32_t)  START_TS
 * 8    (uint32_t)  END_TS
 * 12   (uint16_t)  LEN
 * 14   (uint16_t)  LOG_SZ
 */
#define IDX_LOG32_SZ 16

/* 0    (uint32_t)  SERIES_ID
 * 4    (uint64_t)  START_TS
 * 12   (uint64_t)  END_TS
 * 20   (uint16_t)  LEN
 * 22   (uint16_t)  LOG_SZ
 */
#define IDX_LOG64_SZ 24

#define SHARD_STATUS_SIZE 8

/*
 * Once a shard is created the chunk_size is saved (and after a restart loaded)
 * from the shard. Its not possible to shrink the chunk size for an existing
 * shard since we assume the index will not grow when optimizing. It is
 * possible to set a larger chunk size for an existing shard.
 *
 * New shards can be created using a lower max_chunk size.
 *
 * Max 65535 since uint16_t is used to store this value
 */
#define DEFAULT_MAX_CHUNK_SZ_NUM 800

static const siridb_shard_flags_repr_t flags_map[SHARD_STATUS_SIZE] = {
        {.repr="indexed", .flag=SIRIDB_SHARD_HAS_INDEX},
        {.repr="overlap", .flag=SIRIDB_SHARD_HAS_OVERLAP},
        {.repr="new-values", .flag=SIRIDB_SHARD_HAS_NEW_VALUES},
        {.repr="dropped-series", .flag=SIRIDB_SHARD_HAS_DROPPED_SERIES},
        {.repr="dropped", .flag=SIRIDB_SHARD_IS_REMOVED},
        {.repr="loading", .flag=SIRIDB_SHARD_IS_LOADING},
        {.repr="corrupt", .flag=SIRIDB_SHARD_IS_CORRUPT},
        {.repr="compressed", .flag=SIRIDB_SHARD_IS_COMPRESSED},
};

const char shard_type_map[2][7] = {
        "number",
        "log"
};

static ssize_t SHARD_apply_idx_num(
        siridb_t * siridb,
        siridb_shard_t * shard,
        char * pt,
        size_t pos,
        int is_num64);
static int SHARD_get_idx_num(
        siridb_t * siridb,
        siridb_shard_t * shard,
        int is_num64);
static int SHARD_load_idx_num(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp,
        int is_num64);
static inline int SHARD_init_fn(siridb_t * siridb, siridb_shard_t * shard);
static int SHARD_grow(siridb_shard_t * shard);
static int SHARD_write_header(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        FILE * fp);
static int SHARD_remove(siridb_shard_t * shard);

/*
 * Returns 0 if successful or -1 in case of an error.
 * When an error occurs, a SIGNAL can be raised in some cases but not for sure.
 */
int siridb_shard_load(siridb_t * siridb, uint64_t id)
{
    int is_num64;
    FILE * fp;
    off_t shard_sz;
    siridb_shard_t * shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));

    if (shard == NULL)
    {
        ERR_ALLOC
        return -1;  /* signal is raised */
    }
    shard->fp = siri_fp_new();
    if (shard->fp == NULL)
    {
        free(shard);
        return -1;  /* signal is raised */
    }
    shard->id = id;
    shard->ref = 1;
    shard->len = HEADER_SIZE;
    shard->replacing = NULL;
    if (SHARD_init_fn(siridb, shard) < 0)
    {
        ERR_ALLOC
        siridb_shard_decref(shard);
        return -1;  /* signal is raised */
    }


    log_info("Loading shard %" PRIu64, id);

    if ((fp = fopen(shard->fn, "r")) == NULL)
    {
        log_error("Cannot open shard file for reading: '%s'", shard->fn);
        siridb_shard_decref(shard);
        return -1;
    }

    if (fseeko(fp, 0, SEEK_END) ||
        (shard_sz = ftello(fp)) < (off_t) shard->len ||
        fseeko(fp, 0, SEEK_SET))
    {
        fclose(fp);
        log_critical("Index and/or shard corrupt: '%s'", shard->fn);
        siridb_shard_decref(shard);
        return -1;
    }

    shard->size = (size_t) shard_sz;

    char header[HEADER_SIZE];

    if (fread(&header, HEADER_SIZE, 1, fp) != 1)
    {
        /* cannot read header from shard file,
         * close file decrement reference shard and return -1
         */
        fclose(fp);
        log_critical("Missing header in shard file: '%s'", shard->fn);
        siridb_shard_decref(shard);
        return -1;
    }

    /* set shard type, flags and max_chunk_sz */
    shard->tp = (uint8_t) header[HEADER_TP];
    shard->flags = (uint8_t) header[HEADER_FLAGS] | SIRIDB_SHARD_IS_LOADING;
    shard->max_chunk_sz = *((uint16_t *) (header + HEADER_MAX_CHUNK_SZ));

    siridb_timep_t time_precision = (uint8_t) header[HEADER_TIME_PRECISION];

    if (siridb->time->precision != time_precision)
    {
        fclose(fp);
        log_critical(
                "Time precision from shard (%s) is not the same as "
                "database (%c). Skip loading '%c'",
                siridb_time_short_map[time_precision],
                siridb_time_short_map[siridb->time->precision],
                shard->fn);
        siridb_shard_decref(shard);
        return -1;
    }

    switch (shard->tp)
    {
    case SIRIDB_SHARD_TP_NUMBER:
        is_num64 = time_precision > SIRIDB_TIME_SECONDS;

        if (SHARD_get_idx_num(siridb, shard, is_num64))
        {
            fclose(fp);
            log_critical("Cannot read index for shard: '%s'", shard->fn);
            siridb_shard_decref(shard);
            return -1;
        }

        if (shard->size > shard->len)
        {
            if (fseeko(fp, (off_t) shard->len, SEEK_SET))
            {
                fclose(fp);
                log_critical("Seek error in: '%s'", shard->fn);
                siridb_shard_decref(shard);
                return -1;
            }

            SHARD_load_idx_num(siridb, shard, fp, is_num64);
        }
        break;

    case SIRIDB_SHARD_TP_LOG:
        /* TODO: implement string type */
        assert (0);
        break;

    default:
        fclose(fp);
        log_critical("Unknown type shard file: '%s'", shard->fn);
        siridb_shard_decref(shard);
        return -1;
    }

    if (fclose(fp))
    {
        log_critical("Cannot close shard file: '%s'", shard->fn);
        siridb_shard_decref(shard);
        return -1;
    }

    if (imap_set(siridb->shards, id, shard) == -1)
    {
        siridb_shard_decref(shard);
        return -1;
    }

    /* remove LOADING flag from shard status */
    shard->flags &= ~SIRIDB_SHARD_IS_LOADING;

    return 0;
}

/*
 * Create a new shard file and return a siridb_shard_t object.
 *
 * In case of an error the return value is NULL and a SIGNAL is raised.
 */
siridb_shard_t *  siridb_shard_create(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp,
        siridb_shard_t * replacing)
{
    siridb_shard_t * shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    if (shard == NULL)
    {
        ERR_ALLOC
        return NULL;
    }
    if ((shard->fp = siri_fp_new()) == NULL)
    {
        free(shard);
        return NULL;  /* signal is raised */
    }
    shard->id = id;
    shard->ref = 1;
    shard->tp = tp;
    shard->replacing = replacing;
    shard->len = shard->size = HEADER_SIZE;
    shard->max_chunk_sz = (replacing == NULL) ?
            DEFAULT_MAX_CHUNK_SZ_NUM : replacing->max_chunk_sz;

    FILE * fp;
    if (SHARD_init_fn(siridb, shard) < 0)
    {
        ERR_ALLOC
        siridb_shard_decref(shard);
        return NULL;
    }

    shard->flags =
            siri.cfg->shard_compression ? SIRIDB_SHARD_IS_COMPRESSED : 0;

    shard->flags |=
            (replacing == NULL || siri_optimize_create_idx(shard->fn)) ?
            SIRIDB_SHARD_OK : SIRIDB_SHARD_HAS_INDEX;

    if ((fp = fopen(shard->fn, "w")) == NULL)
    {
        ERR_FILE
        siridb_shard_decref(shard);
        log_critical("Cannot create shard file: '%s'", shard->fn);
        return NULL;
    }

    /* 0    (uint8_t)   SHEMA
     * 1    (uint64_t)  ID
     * 9    (uint64_t)  DURATION
     * 17   (uint16_t)  MAX_CHUNK_SZ
     * 19   (uint8_t)   TP
     * 20   (uint8_t)   TIME_PRECISION
     * 21   (uint8_t)   FLAGS
     */
    if (    fputc(SIRIDB_SHARD_SHEMA, fp) == EOF ||
            fwrite(&id, sizeof(uint64_t), 1, fp) != 1 ||
            fwrite(&duration, sizeof(uint64_t), 1, fp) != 1 ||
            fwrite(&shard->max_chunk_sz, sizeof(uint16_t), 1, fp) != 1 ||
            fputc(tp, fp) == EOF ||
            fputc(siridb->time->precision, fp) == EOF ||
            fputc(shard->flags, fp) == EOF)
    {
        ERR_FILE
        fclose(fp);
        siridb_shard_decref(shard);
        log_critical("Cannot write to shard file: '%s'", shard->fn);
        return NULL;
    }

    if (fclose(fp))
    {
        ERR_FILE
        siridb_shard_decref(shard);
        log_critical("Cannot close shard file: '%s'", shard->fn);
        return NULL;
    }

    if (imap_set(siridb->shards, id, shard) == -1)
    {
        siridb_shard_decref(shard);
        ERR_ALLOC
        return NULL;
    }

    /*
     * This is not critical at this point and it's hard to imagine this
     * fails if all the above was successful
     */
    siri_fopen(siri.fh, shard->fp, shard->fn, "r+");

    return shard;
}

/*
 * Call-back function used to validate shards in a where expression.
 *
 * Returns 0 or 1 (false or true).
 */
int siridb_shard_cexpr_cb(
        siridb_shard_view_t * vshard,
        cexpr_condition_t * cond)
{
    switch (cond->prop)
    {
    case CLERI_GID_K_SID:
        return cexpr_int_cmp(cond->operator, vshard->shard->id, cond->int64);
    case CLERI_GID_K_POOL:
        return cexpr_int_cmp(cond->operator, vshard->server->pool, cond->int64);
    case CLERI_GID_K_SIZE:
        return cexpr_int_cmp(cond->operator, vshard->shard->size, cond->int64);
    case CLERI_GID_K_START:
        return cexpr_int_cmp(cond->operator, vshard->start, cond->int64);
    case CLERI_GID_K_END:
        return cexpr_int_cmp(cond->operator, vshard->end, cond->int64);
    case CLERI_GID_K_TYPE:
        return cexpr_int_cmp(cond->operator, vshard->shard->tp, cond->int64);
    case CLERI_GID_K_SERVER:
        return cexpr_str_cmp(cond->operator, vshard->server->name, cond->str);
    case CLERI_GID_K_STATUS:
        {
            char buffer[SIRIDB_SHARD_STATUS_STR_MAX];
            siridb_shard_status(buffer, vshard->shard);
            return cexpr_str_cmp(cond->operator, buffer, cond->str);
        }
    }
    log_critical("Unexpected shard property received: %d", cond->prop);
    assert (0);
    return -1;
}

/*
 * Make sure 'str' is a pointer to a string which can hold at least
 * SIRIDB_SHARD_STR_MAX.
 */
int siridb_shard_status(char * str, siridb_shard_t * shard)
{
    char * pt = str;

    if (shard->replacing != NULL)
    {
        pt += sprintf(pt, "optimizing");
    }

    uint8_t flags = shard->flags;

    for (int i = 1; i < SHARD_STATUS_SIZE && flags; i++)
    {
        if ((flags & flags_map[i].flag) == flags_map[i].flag)
        {
            flags -= flags_map[i].flag;
            pt += (pt == str) ? sprintf(pt, "%s", flags_map[i].repr) :
                    sprintf(pt, " | %s", flags_map[i].repr);
        }
    }

    if (pt == str)
    {
        pt += sprintf(pt, "ok");
    }
    return pt - str;
}

/*
 * Writes an index and points to a shard. The return value is the position
 * where the points start in the shard file.
 *
 * If an error has occurred, EOF will be returned and a SIGNAL will be raised.
 */
long int siridb_shard_write_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard,
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        FILE * idx_fp,
        uint16_t * cinfo)
{
    FILE * fp;
    uint16_t len = end - start;
    size_t dsize;
    unsigned char * cdata = NULL;

    uint_fast32_t i;
    long int pos = EOF;
    int header_sz;

    if (shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, shard->fp, shard->fn, "r+"))
        {
            ERR_FILE
            log_critical("Cannot open file '%s'", shard->fn);
            return EOF;
        }
    }
    fp = shard->fp->fp;

    if (shard->flags & SIRIDB_SHARD_IS_COMPRESSED)
    {
        cdata = siridb_points_zip(points, start, end, cinfo, &dsize);
        if (cdata == NULL)
        {
            log_critical("Memory allocation error while compressing points");
            return -1;
        }
    }
    else if (series->tp == TP_STRING)
    {
        /* string.. */
    }
    else
    {
        /* no compression, ignore c-info */
        cinfo = NULL;
        dsize = (siridb->time->ts_sz + 8) * len;
    }

    if (shard->len > SHARD_GROW_SZ && (shard->len + dsize + 64 > shard->size))
    {
        SHARD_grow(shard);
    }

    if (fseeko(fp, shard->len, SEEK_SET))
    {
        log_critical("Seek error in: '%s'", shard->fn);
        return -1;
    }

    if (idx_fp == NULL || (shard->flags & SIRIDB_SHARD_HAS_NEW_VALUES))
    {
        header_sz = SHARD_write_header(
                siridb,
                series,
                points,
                start,
                end,
                cinfo,
                fp);
        pos = shard->len + header_sz;
    }
    else
    {
        /* the idx_fp is already at the end of the index file */
        header_sz = SHARD_write_header(
                siridb,
                series,
                points,
                start,
                end,
                cinfo,
                idx_fp);
        pos = shard->len;
    }

    if (header_sz < 0)
    {
        ERR_FILE
        log_critical(
                "Cannot write index header for shard id %" PRIu64,
                shard->id);
        free(cdata);
        return EOF;
    }

    if (cdata != NULL)
    {
        long int rc = fwrite(cdata, dsize, 1, fp);
        free(cdata);
        if (rc != 1)
        {
            ERR_FILE
            log_critical("Cannot write points to file '%s'", shard->fn);
            return EOF;
        }
    }
    else
    {
        for (i = start; i < end; i++)
        {
            if (fwrite(&points->data[i].ts, siridb->time->ts_sz, 1, fp) != 1 ||
                fwrite(&points->data[i].val, 8, 1, fp) != 1)
            {
                ERR_FILE
                log_critical("Cannot write points to file '%s'", shard->fn);
                return EOF;
            }
        }
    }

    if (fflush(fp))
    {
        ERR_FILE
        log_critical("Cannot write flush file '%s'", shard->fn);
        return EOF;
    }

    shard->len = pos + dsize;

    return pos;
}

/*
 * Returns 0 if successful or -1 in case of an error. SiriDB might recover
 * from this error so we do not consider this critical.
 */
int siridb_shard_get_points_num32(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    uint32_t * temp,* pt;
    size_t len = points->len + idx->len;

    if (idx->shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, idx->shard->fp, idx->shard->fn, "r+"))
        {
            log_critical(
                    "Cannot open file '%s', skip reading points",
                    idx->shard->fn);
            return -1;
        }
    }

    temp = (uint32_t *) malloc(sizeof(uint32_t) * idx->len * 3);
    if (temp == NULL)
    {
        log_critical("Memory allocation error");
        return -1;
    }

    if (fseeko(idx->shard->fp->fp, idx->pos, SEEK_SET) ||
        fread(
            temp,
            12,  // NUM32 point size
            idx->len,
            idx->shard->fp->fp) != idx->len)
    {
        if (idx->shard->flags & SIRIDB_SHARD_IS_CORRUPT)
        {
            log_error("Cannot read from shard id %" PRIu64, idx->shard->id);
        }
        else
        {
            log_critical(
                    "Cannot read from shard id %" PRIu64
                    ". The next optimize cycle "
                    "will fix this shard but you might loose some data.",
                    idx->shard->id);
            idx->shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
        }
        free(temp);
        return -1;
    }

    /* set pointer to start */
    pt = temp;

    /* crop from start if needed */
    if (start_ts != NULL)
    {
        for (; *pt < *start_ts; pt += 3, len--);
    }

    /* crop from end if needed */
    if (end_ts != NULL)
    {
        for (   uint32_t * p = temp + 3 * (idx->len - 1);
                *p >= *end_ts;
                p -= 3, len--);
    }

    if (    has_overlap &&
            points->len &&
            (idx->shard->flags & SIRIDB_SHARD_HAS_OVERLAP))
    {
        for (uint64_t ts; points->len < len; pt += 3)
        {
            ts = (uint64_t) *pt;
            siridb_points_add_point(points, &ts, ((qp_via_t *) (pt + 1)));
        }
    }
    else
    {
        for (; points->len < len; points->len++, pt += 3)
        {
            points->data[points->len].ts = (uint64_t) *pt;
            points->data[points->len].val = *((qp_via_t *) (pt + 1));
        }
    }

    free(temp);
    return 0;
}

/*
 * COPY from siridb_shard_get_points_num32
 */
int siridb_shard_get_points_num64(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    uint64_t * temp, * pt;
    size_t len = points->len + idx->len;

    if (idx->shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, idx->shard->fp, idx->shard->fn, "r+"))
        {
            log_critical(
                    "Cannot open file '%s', skip reading points",
                    idx->shard->fn);
            return -1;
        }
    }

    temp = (uint64_t *) malloc(sizeof(uint64_t) * idx->len * 2);
    if (temp == NULL)
    {
        log_critical("Memory allocation error");
        return -1;
    }

    if (fseeko(idx->shard->fp->fp, idx->pos, SEEK_SET) ||
        fread(
            temp,
            16,  // NUM64 point size
            idx->len,
            idx->shard->fp->fp) != idx->len)
    {
        if (idx->shard->flags & SIRIDB_SHARD_IS_CORRUPT)
        {
            log_error("Cannot read from shard id %" PRIu64, idx->shard->id);
        }
        else
        {
            log_critical(
                    "Cannot read from shard id %" PRIu64
                    ". The next optimize cycle "
                    "will fix this shard but you might loose some data.",
                    idx->shard->id);
            idx->shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
        }
        free(temp);
        return -1;
    }

    /* set pointer to start */
    pt = temp;

    /* crop from start if needed */
    if (start_ts != NULL)
    {
        for (; *pt < *start_ts; pt += 2, len--);
    }

    /* crop from end if needed */
    if (end_ts != NULL)
    {
        for (   uint64_t * p = temp + 2 * (idx->len - 1);
                *p >= *end_ts;
                p -= 2, len--);
    }

    if (    has_overlap &&
            points->len &&
            (idx->shard->flags & SIRIDB_SHARD_HAS_OVERLAP))
    {
        for (; points->len < len; pt += 2)
        {
            siridb_points_add_point(points, pt, ((qp_via_t *) (pt + 1)));
        }
    }
    else
    {
        for (; points->len < len; points->len++, pt += 2)
        {
            points->data[points->len].ts = *pt;
            points->data[points->len].val = *((qp_via_t *) (pt + 1));
        }
    }

    free(temp);
    return 0;
}

/*
 * Returns 0 if successful or -1 in case of an error. SiriDB might recover
 * from this error so we do not consider this critical.
 */
int siridb_shard_get_points_num_compressed(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    unsigned char * bits;
    size_t size = siridb_points_get_size_zipped(idx->cinfo, idx->len);

    if (idx->shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, idx->shard->fp, idx->shard->fn, "r+"))
        {
            log_critical(
                    "Cannot open file '%s', skip reading points",
                    idx->shard->fn);
            return -1;
        }
    }

    bits = (unsigned char *) malloc(size);
    if (bits == NULL)
    {
        log_critical("Memory allocation error");
        return -1;
    }

    if (fseeko(idx->shard->fp->fp, idx->pos, SEEK_SET) ||
        fread(bits, size, 1, idx->shard->fp->fp) != 1)
    {
        if (idx->shard->flags & SIRIDB_SHARD_IS_CORRUPT)
        {
            log_error("Cannot read from shard id %" PRIu64, idx->shard->id);
        }
        else
        {
            log_critical(
                    "Cannot read from shard id %" PRIu64
                    ". The next optimize cycle "
                    "will fix this shard but you might loose some data.",
                    idx->shard->id);
            idx->shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
        }
        free(bits);
        return -1;
    }

    switch (points->tp)
    {
    case TP_INT:
        siridb_points_unzip_int(
            points,
            bits,
            idx->len,
            idx->cinfo,
            start_ts,
            end_ts,
            has_overlap && (idx->shard->flags & SIRIDB_SHARD_HAS_OVERLAP));
    break;
    case TP_DOUBLE:
        siridb_points_unzip_double(
            points,
            bits,
            idx->len,
            idx->cinfo,
            start_ts,
            end_ts,
            has_overlap && (idx->shard->flags & SIRIDB_SHARD_HAS_OVERLAP));
    break;
    case TP_STRING: assert(0);
    }

    free(bits);
    return 0;
}

/*
 * COPY from siridb_shard_get_points_num32
 */
int siridb_shard_get_points_log32(
        siridb_points_t * points __attribute__((unused)),
        idx_t * idx __attribute__((unused)),
        uint64_t * start_ts __attribute__((unused)),
        uint64_t * end_ts __attribute__((unused)),
        uint8_t has_overlap __attribute__((unused)))
{
    return -1;  /* dummy function */
}

/*
 * COPY from siridb_shard_get_points_num32
 */
int siridb_shard_get_points_log64(
        siridb_points_t * points __attribute__((unused)),
        idx_t * idx __attribute__((unused)),
        uint64_t * start_ts __attribute__((unused)),
        uint64_t * end_ts __attribute__((unused)),
        uint8_t has_overlap __attribute__((unused)))
{
    return -1;  /* dummy function */
}

/*
 * This function will be called from the 'optimize' thread.
 *
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
int siridb_shard_optimize(siridb_shard_t * shard, siridb_t * siridb)
{
    int rc = 0;
    siridb_shard_t * new_shard = NULL;
    uint64_t duration = (shard->tp == SIRIDB_SHARD_TP_NUMBER) ?
            siridb->duration_num : siridb->duration_log;
    siridb_series_t * series;

    uv_mutex_lock(&siridb->shards_mutex);

    /* In case the shard is not removed, it must be the shard inside the imap
     * because we check and replace the shard within the shards_mutex lock.
     * If the shard is marked as removed we can simply skip the optimize.
     */
    if (~shard->flags & SIRIDB_SHARD_IS_REMOVED)
    {
        if ((new_shard = siridb_shard_create(
            siridb,
            shard->id,
            duration,
            shard->tp,
            shard)) == NULL)
        {
            /* signal is raised */
            log_critical(
                    "Cannot create shard id '%" PRIu64 "' for optimizing.",
                    shard->id);
            rc = -1;
        }
        else
        {
            siridb_shard_incref(new_shard);
        }
    }
    else
    {
        log_warning(
                "Skip optimizing shard id '%" PRIu64 "' "
                "because the shard is probably dropped.",
                shard->id);
    }

    uv_mutex_unlock(&siridb->shards_mutex);

    if (new_shard == NULL)
    {
        /*
         * Creating the new shard has failed or the shard is dropped.
         * We exit here so the mutex is is unlocked.
         * (a signal might have been raised)
         */
        return rc;
    }

    /* at this point the references should be as following (unless dropped):
     *  shard->ref (=>2)
     *      - simple list
     *      - new_shard->replacing
     *  new_shard->ref (=>2)
     *      - siridb->shards
     *      - this method
     */

    sleep(1);

    uv_mutex_lock(&siridb->series_mutex);

    slist_t * slist = imap_2slist_ref(siridb->series_map);

    uv_mutex_unlock(&siridb->series_mutex);

    if (slist == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    sleep(1);

    for (size_t i = 0; i < slist->len; i++)
    {
        /* its possible that another database is paused, but we wait anyway */
        if (siri.optimize->pause)
        {
            siri_optimize_wait();
        }

        series = slist->data[i];

        if (    !siri_err &&
                siri.optimize->status != SIRI_OPTIMIZE_CANCELLED &&
                shard->id % siridb->duration_num == series->mask &&
                (~series->flags & SIRIDB_SERIES_IS_DROPPED) &&
                (~new_shard->flags & SIRIDB_SHARD_IS_REMOVED))
        {
            uv_mutex_lock(&siridb->series_mutex);

            if (    (~new_shard->flags & SIRIDB_SHARD_IS_REMOVED) &&
                    siridb_series_optimize_shard(
                        siridb,
                        series,
                        new_shard))
            {
                log_critical(
                        "Optimizing shard '%s' has failed due to a critical "
                        "error", shard->fn);
            }

            uv_mutex_unlock(&siridb->series_mutex);

            /* make this sleep depending on the active_tasks
             * (50ms per active task) */
            usleep( 50000 * siridb->active_tasks + 100 );
        }

        siridb_series_decref(series);
    }

    slist_free(slist);

    if (new_shard->flags & SIRIDB_SHARD_IS_REMOVED)
    {
        log_warning(
                "Cancel optimizing shard '%s' because the shard is dropped",
                new_shard->fn);
        siridb_shard_decref(new_shard);
        return siri_err;
    }

    if (siri_err || siri.optimize->status == SIRI_OPTIMIZE_CANCELLED)
    {
        /*
         * Error occurred or the optimize task is cancelled. By decrementing
         * only the reference counter for the new_shard we keep this shard as
         * if it is still optimizing so remaining points can still be written.
         */
        siridb_shard_decref(new_shard);
        return siri_err;
    }

    sleep(1);

    uv_mutex_lock(&siridb->series_mutex);

    /* make sure both shards files are closed */
    siri_fp_close(new_shard->replacing->fp);
    siri_fp_close(new_shard->fp);

    /*
     * Closing files or writing to the new shard might have produced
     * critical errors. This seems to be a good point to check for errors.
     */
    if (siri_err || (new_shard->flags & SIRIDB_SHARD_IS_REMOVED))
    {
        if (new_shard->flags & SIRIDB_SHARD_IS_REMOVED)
        {
            log_warning(
                    "Cancel optimizing shard '%s' because the shard is dropped",
                    new_shard->fn);
        }
    }
    else
    {
        /* remove the old shard file, this is not critical */
        unlink(new_shard->replacing->fn);

        /* rename the temporary files to the correct file names */
        if (rename(new_shard->fn, new_shard->replacing->fn) ||
            siri_optimize_finish_idx(
                new_shard->replacing->fn,
                new_shard->replacing->flags & SIRIDB_SHARD_HAS_INDEX))
        {
            log_critical(
                    "Could not rename file '%s' to '%s'",
                    new_shard->fn,
                    new_shard->replacing->fn);
            ERR_FILE
        }
        else
        {
            /* free the original allocated memory and set the new filename */
            free(new_shard->fn);
            new_shard->fn = new_shard->replacing->fn;
            new_shard->replacing->fn = NULL;

            /* decrement reference to old shard and set
             * new_shard->replacing to NULL
             */
            siridb_shard_decref(new_shard->replacing);
            new_shard->replacing = NULL;
        }
    }

    uv_mutex_unlock(&siridb->series_mutex);

    /* can raise an error only if the shard is dropped, in any other case we
     * still have a reference left and an error cannot be raised.
     */
    siridb_shard_decref(new_shard);

    sleep(1);

    return siri_err;
}

/*
 * This function can be used instead of the macro function when needed as
 * callback.
 *
 * Decrement the reference counter, when 0 the shard will be destroyed.
 *
 * In case the shard will be destroyed and flag SIRIDB_SHARD_WILL_BE_REMOVED
 * is set, the file will be removed.
 *
 * A signal can be raised in case closing the shard file fails.
 */
void siridb__shard_decref(siridb_shard_t * shard)
{
    if (!--shard->ref)
    {
        siridb__shard_free(shard);
    }
}

void siridb_shard_drop(siridb_shard_t * shard, siridb_t * siridb)
{
    siridb_series_t * series;
    siridb_shard_t * pop_shard;
    int optimizing = 0;

    uv_mutex_lock(&siridb->series_mutex);
    uv_mutex_lock(&siridb->shards_mutex);

    pop_shard = (siridb_shard_t *) imap_pop(siridb->shards, shard->id);

    /*
     * When optimizing, 'pop_shard' is always the new shard and 'shard'
     * will be set to the old one.
     */
    if (pop_shard != NULL && (~pop_shard->flags & SIRIDB_SHARD_IS_REMOVED))
    {
        pop_shard->flags |= SIRIDB_SHARD_IS_REMOVED;
        SHARD_remove(pop_shard);

        if (shard != pop_shard)
        {
            optimizing = 1;
        }
        else if (shard->replacing != NULL)
        {
            optimizing = 1;
            shard = shard->replacing;
        }
    }
    else
    {
        log_warning("Shard id '%" PRIu64 "' is already dropped", shard->id);
    }

    uv_mutex_unlock(&siridb->shards_mutex);

    /*
     * We need a series mutex here since we depend on the series index
     * and we create a copy since series might be removed when the length
     * of series is zero after removing the shard
     */

    /*
     * Create a copy since series might be removed and when optimizing we need
     * to remove indexes for both the old and new shard. Since a series might
     * be dropped by the first call to remove shard, we need an extra reference
     * for each series.
     */
    if (optimizing)
    {
        slist_t * slist = imap_2slist_ref(siridb->series_map);

        if (slist == NULL)
        {
            ERR_ALLOC
        }
        else for (size_t i = 0; i < slist->len; i++)
        {
            series = (siridb_series_t *) slist->data[i];
            if (shard->id % siridb->duration_num == series->mask)
            {
                siridb_series_remove_shard(siridb, series, shard);
                siridb_series_remove_shard(siridb, series, pop_shard);
            }
            siridb_series_decref(series);
        }

        slist_free(slist);
    }
    else
    {
        slist_t * slist = imap_2slist(siridb->series_map);

        if (slist == NULL)
        {
            ERR_ALLOC
        }
        else for (size_t i = 0; i < slist->len; i++)
        {
            series = (siridb_series_t *) slist->data[i];
            if (shard->id % siridb->duration_num == series->mask)
            {
                siridb_series_remove_shard(siridb, series, shard);
            }
        }
        slist_free(slist);
    }

    if (pop_shard != NULL)
    {
        siridb_shard_decref(pop_shard);
    }

    uv_mutex_unlock(&siridb->series_mutex);
}

/*
 * NEVER call this function but call siridb_shard_decref instead.
 *
 * Destroys a shard.
 * When flag SIRIDB_SHARD_WILL_BE_REMOVED is set, the file will be removed.
 *
 * A signal can be raised in case closing the shard file fails.
 */
void siridb__shard_free(siridb_shard_t * shard)
{
    if (shard->replacing != NULL)
    {
        /* in case shard->replacing is set we also need to free this shard */
        siridb_shard_decref(shard->replacing);
    }

    /* this will close the file, even when other references exist */
    siri_fp_decref(shard->fp);

#ifdef DEBUG
    log_debug("Free shard id: %" PRIu64, shard->id);
#endif

    free(shard->fn);
    free(shard);
}

/*
 * Returns 0 when successful or a negative value in case of an error.
 */
static int SHARD_remove(siridb_shard_t * shard)
{
    int rc = 0;

    if (shard->replacing != NULL)
    {
        rc = SHARD_remove(shard->replacing);
    }
    else if (shard->flags & SIRIDB_SHARD_HAS_INDEX)
    {
        /* We never have to delete the temporary (__) index file since this is
         * the responsibility for the optimize task. This index file can never
         * be in use by SiriDB. In case deletion has failed, nothing will
         * happen because the file will not be read at startup.
         */
        siridb_shard_idx_file(buffer, shard->fn);

        /* unlink the index file */
        if (unlink(buffer))
        {
            /* not a real issue, only a warning since this is not expected */
            log_warning("Removing index file failed: %s", buffer);
        }
        else
        {
            log_info("Index file removed: %s", buffer);
        }
    }

    siri_fp_close(shard->fp);

    rc += unlink(shard->fn);

    if (rc == 0)
    {
        log_info("Shard file removed: %s", shard->fn);
    }
    else if (shard->fn != NULL)
    {
        log_critical(
                "Removing shard file failed: %s (error code: %d)",
                shard->fn,
                rc);
    }

    return rc;
}

/*
 * This function applies the index on the appropriate series. In case the
 * series is not found, a log line will be displayed if this is the first
 * one in the shard which is not found. The next series which cannot be found
 * is simply ignored. In case the series id is not possible (invalid id),
 * then an log error is displayed and the return value will be -1.
 */
static ssize_t SHARD_apply_idx_num(
        siridb_t * siridb,
        siridb_shard_t * shard,
        char * pt,
        size_t pos,
        int is_num64)
{
    ssize_t size;
    uint16_t len;
    uint32_t series_id;
    siridb_series_t * series;
    uint16_t cinfo = 0;

    series_id = *((uint32_t *) pt);
    if (series_id == 0)
    {
        return 0;
    }
    len = *((uint16_t *) (pt + (is_num64 ? 20 : 12)));  // LEN POS IN INDEX
    series = imap_get(siridb->series_map, series_id);

    if (shard->flags & SIRIDB_SHARD_IS_COMPRESSED)
    {
        cinfo = *((uint16_t *)(pt + (is_num64 ? IDX_NUM64_SZ : IDX_NUM32_SZ)));
        size = (ssize_t) siridb_points_get_size_zipped(cinfo, len);
    }
    else
    {
        size = len * (is_num64 ? 16 : 12);
    }

    if (series == NULL)
    {
        if (!series_id || series_id > siridb->max_series_id)
        {
            log_error(
                    "Unexpected Series ID %" PRIu32
                    " is found in shard %" PRIu64 " (%s) at "
                    "position %ld. This indicates that this shard is "
                    "probably corrupt. The next optimize cycle will most "
                    "likely fix this shard but you might loose some data.",
                    series_id,
                    shard->id,
                    shard->fn,
                    pos);
            shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
            return -1;
        }

        /* this shard has remove series, make sure the flag is set */
        if (!(shard->flags & SIRIDB_SHARD_HAS_DROPPED_SERIES))
        {
            log_debug(
                    "At least Series ID %" PRIu32
                    " is found in shard %" PRIu64 " (%s) but "
                    "does not exist anymore. We will remove the series on "
                    "the next optimize cycle.",
                    series_id,
                    shard->id,
                    shard->fn);
            shard->flags |= SIRIDB_SHARD_HAS_DROPPED_SERIES;
        }
    }
    else
    {
        if (siridb_series_add_idx(
                series,
                shard,
                is_num64 ? // START_TS IN HEADER
                        (uint64_t) *((uint64_t *) (pt + 4)) :
                        (uint64_t) *((uint32_t *) (pt + 4)),
                is_num64 ? // END_TS IN HEADER
                        (uint64_t) *((uint64_t *) (pt + 12)) :
                        (uint64_t) *((uint32_t *) (pt + 8)),
                (uint32_t) pos,
                len,
                cinfo) == 0)
        {
            /* update the series length property */
            series->length += len;
        }
        else
        {
            /* signal is raised */
            log_critical("Cannot load index for Series ID %u", series->id);
        }
    }

    return size;
}

/*
 * Read an index file for a shard in case the shard has flag
 * SIRIDB_SHARD_HAS_INDEX set. Returns 0 in case the index was read successful
 * and if the flag was not set. Returns a negative value in case of an error.
 *
 * Member shard->len will be updated according the index.
 */
static int SHARD_get_idx_num(
        siridb_t * siridb,
        siridb_shard_t * shard,
        int is_num64)
{
    const unsigned int idx_sz = (shard->flags & SIRIDB_SHARD_IS_COMPRESSED) ?
            (is_num64 ? 24 : 16) :
            (is_num64 ? IDX_NUM64_SZ : IDX_NUM32_SZ);
    size_t i, n;
    char * data, * pt;
    ssize_t size;
    FILE * fp;

    if (~shard->flags & SIRIDB_SHARD_HAS_INDEX)
    {
        return 0;
    }

    n = SIRIDB_SHARD_MAX_CHUNK_SZ / idx_sz;
    data = (char *) malloc(n * idx_sz);

    if (data == NULL)
    {
        log_critical("Memory allocation error");
        free(data);
        return -1;
    }

    /* get the index file name */
    siridb_shard_idx_file(fn, shard->fn);

    fp = fopen(fn, "r");
    if (fp == NULL)
    {
        log_critical("Cannot open index file for reading: '%s'", fn);
        free(data);
        return -1;
    }

    while ((n = fread(data, idx_sz, n, fp)))
    {
        pt = data;
        for (i = 0; i < n; i++, pt += idx_sz)
        {
            size = SHARD_apply_idx_num(
                    siridb,
                    shard,
                    pt,
                    shard->len,
                    is_num64);

            if (size < 0)
            {
                log_critical("Error while reading index file: '%s'", fn);
                fclose(fp);
                free(data);
                return -1;
            }

            shard->len += size;
        }
    }

    fclose(fp);
    free(data);

    return 0;
}

/*
 * Returns 0 if successful or -1 in case of an error.
 *
 * A SIGNAL might be raised in case of memory errors. We mark the shard as
 * corrupt in case of disk errors and try to recover on the next optimize
 * cycle.
 */
static int SHARD_load_idx_num(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp,
        int is_num64)
{
    const unsigned int idx_sz = (shard->flags & SIRIDB_SHARD_IS_COMPRESSED) ?
            is_num64 ? 24 : 16 :
            is_num64 ? IDX_NUM64_SZ : IDX_NUM32_SZ;

    char idx[idx_sz];
    ssize_t sz;
    int rc;
    size_t size, pos;

    while ((size = fread(&idx, 1, idx_sz, fp)) == idx_sz)
    {
        pos = shard->len + idx_sz;

        sz = SHARD_apply_idx_num(siridb, shard, idx, pos, is_num64);
        if (sz == 0)
        {
            break;
        }
        else if (sz < 0)
        {
            return -1;
        }

        rc = fseeko(fp, sz, SEEK_CUR);  // 16 = NUM64 point size
        if (rc != 0)
        {
            log_error(
                "Seek error in shard %" PRIu64 " (%s) at position %ld. "
                "Mark this shard as corrupt. The next optimize cycle will "
                "most likely fix this shard but you might loose some data.",
                shard->id,
                shard->fn,
                pos);
            shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
            return -1;
        }

        shard->flags |= SIRIDB_SHARD_HAS_NEW_VALUES;
        shard->len = pos + sz;
    }

    return siri_err;
}

/*
 * Set shard->fn to the correct file name.
 *
 * Returns the length of 'fn' or a negative value in case of an error.
 */
static inline int SHARD_init_fn(siridb_t * siridb, siridb_shard_t * shard)
{
     return asprintf(
             &shard->fn,
             "%s%s%s%" PRIu64 "%s",
             siridb->dbpath,
             SIRIDB_SHARDS_PATH,
             (shard->replacing == NULL) ? "" : "__",
             shard->id,
             ".sdb");
}

/*
 * Write a header for a chunk of points. The header can be written to argument
 * fp which should be a pointer to the index, or the shard file.
 *
 * In case of an error the function return EOF, otherwise the size which is
 * written.
 */
static int SHARD_write_header(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        uint16_t * cinfo,
        FILE * fp)
{
    uint16_t len = end - start;
    int size = EOF;

    if (fwrite(&series->id, sizeof(uint32_t), 1, fp) != 1)
    {
        return EOF;
    }

    switch (siridb->time->ts_sz)
    {
    case sizeof(uint32_t):
        {
            uint32_t start_ts = (uint32_t) points->data[start].ts;
            uint32_t end_ts = (uint32_t) points->data[end - 1].ts;
            if (fwrite(&start_ts, sizeof(uint32_t), 1, fp) != 1 ||
                fwrite(&end_ts, sizeof(uint32_t), 1, fp) != 1)
            {
                return EOF;
            }
        }
        /* TODO: this is not LOG compatible */
        size = IDX_NUM32_SZ;
        break;

    case sizeof(uint64_t):
        if (fwrite(&points->data[start].ts, sizeof(uint64_t), 1, fp) != 1 ||
            fwrite(&points->data[end - 1].ts, sizeof(uint64_t), 1, fp) != 1)
        {
            return EOF;
        }
        /* TODO: this is not LOG compatible */
        size = IDX_NUM64_SZ;
        break;

    default:
        assert (0);
        break;
    }


    if (fwrite(&len, sizeof(uint16_t), 1, fp) != 1)
    {
        return EOF;
    }

    if (cinfo != NULL)
    {
        size += sizeof(uint16_t);
        if (fwrite(cinfo, sizeof(uint16_t), 1, fp) != 1)
        {
            return EOF;
        }
    }

    return size;
}


static int SHARD_grow(siridb_shard_t * shard)
{
    assert (shard->fp);

    int buffer_fd = fileno(shard->fp->fp);

    shard->size = ((size_t) (shard->size / SHARD_GROW_SZ) + 2) * SHARD_GROW_SZ;

    if (buffer_fd == -1)
    {
        log_error("Cannot get file descriptor for '%s'", shard->fn);
        return -1;
    }

    if (ftruncate(buffer_fd, (off_t) shard->size) || fsync(buffer_fd))
    {
        log_error("Cannot truncate shard file: '%s'", shard->fn);
        return -1;
    }

    return 0;
}
