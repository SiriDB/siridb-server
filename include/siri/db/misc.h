/*
 * misc.h - Miscellaneous functions used by SiriDB.
 */
#ifndef SIRIDB_MISC_H_
#define SIRIDB_MISC_H_

#include <inttypes.h>
#include <qpack/qpack.h>

#define siridb_misc_get_fn(__fn, __path, __filename)            \
    char __fn[strlen(__path) + strlen(__filename) + 1];         \
    sprintf(__fn, "%s%s", __path, __filename);

/* Schema File Check
 * Needs fn and unpacker, free unpacker in case of an error.
 * Returns current function with -1 in case of an error and
 * raises a SIGNAL in case of a memory error.
 */
#define siridb_misc_schema_check(__schema)                      \
    /* read and check schema */                                 \
    qp_obj_t qp_schema;                                         \
    if (!qp_is_array(qp_next(unpacker, NULL)) ||                \
            qp_next(unpacker, &qp_schema) != QP_INT64 ||        \
            qp_schema.via.int64 != __schema)                    \
    {                                                           \
        log_critical("Invalid schema detected in '%s'", fn);    \
        qp_unpacker_ff_free(unpacker);                          \
        return -1;                                              \
    }

qp_unpacker_t * siridb_misc_open_schema_file(uint8_t schema, const char * fn);


#endif  /* SIRIDB_MISC_H_ */
