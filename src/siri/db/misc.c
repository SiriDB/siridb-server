/*
 * misc.c - Miscellaneous functions used by SiriDB.
 */
#include <siri/db/misc.h>
#include <stdlib.h>
#include <siri/err.h>

qp_unpacker_t * siridb_misc_open_schema_file(uint8_t schema, const char * fn)
{
    qp_unpacker_t * unpacker = qp_unpacker_ff(fn);
    if (unpacker != NULL)
    {
        qp_obj_t qp_schema;
        if (    !qp_is_array(qp_next(unpacker, NULL)) ||
                qp_next(unpacker, &qp_schema) != QP_INT64 ||
                qp_schema.via.int64 != schema)
        {
            log_critical("Invalid schema detected in '%s'", fn);
            qp_unpacker_ff_free(unpacker);
            unpacker = NULL;
        }
    }

    return unpacker;
}
