/*
 * misc.c - Miscellaneous functions used by SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-08-2016
 *
 */
#include <siri/db/misc.h>
#include <stdlib.h>
#include <siri/err.h>

qp_unpacker_t * siridb_misc_open_schema_file(uint8_t schema, const char * fn)
{
    qp_unpacker_t * unpacker = qp_unpacker_from_file(fn);
    if (unpacker != NULL)
    {
        qp_obj_t * qp_schema = qp_object_new();
        if (qp_schema == NULL)
        {
            qp_unpacker_free(unpacker);
            unpacker = NULL;  /* signal is raised */
        }
        else
        {
            if (    !qp_is_array(qp_next(unpacker, NULL)) ||
                    qp_next(unpacker, qp_schema) != QP_INT64 ||
                    qp_schema->via->int64 != schema)
            {
                log_critical("Invalid schema detected in '%s'", fn);
                qp_unpacker_free(unpacker);
                unpacker = NULL;
            }

            /* finished schema check, free schema object */
            qp_object_free(qp_schema);
        }
    }

    return unpacker;
}
