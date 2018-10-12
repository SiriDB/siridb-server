/*
 * presuf.c - Prefix and Suffix store.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/presuf.h>
#include <siri/err.h>
#include <siri/grammar/grammar.h>
#include <stdio.h>
#include <stdlib.h>
#include <xstr/xstr.h>
#include <string.h>

siridb_presuf_t * PRESUF_add(siridb_presuf_t ** presuf);
int PRESUF_is_equal(const char * a, const char * b);

static char * presuffed_name = NULL;
static size_t presuffed_size = 0;

/*
 * Memory will be allocated for 'presuffed_name' using malloc.
 * This function must be called once to free the memory.
 */
void siridb_presuf_cleanup(void)
{
    free(presuffed_name);
    presuffed_name = NULL;
    presuffed_size = 0;
}

/*
 * Destroy prefix-suffix object.
 */
void siridb_presuf_free(siridb_presuf_t * presuf)
{
    siridb_presuf_t * tmp;
    while (presuf != NULL)
    {
        tmp = presuf;
        presuf = presuf->prev;
        free(tmp->prefix);
        free(tmp->suffix);
        free(tmp);
    }
}

/*
 * Add prefix-suffix from a node.
 * Use 'siridb_presuf_is_unique' to check if the new node is unique.
 *
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 * (presuf remains unchanged in case of an error)
 */
siridb_presuf_t * siridb_presuf_add(
        siridb_presuf_t ** presuf,
        cleri_node_t * node)
{
    cleri_children_t * children = node->children->next;
    cleri_children_t * ps_children;

    siridb_presuf_t * nps = PRESUF_add(presuf);
    if (nps != NULL)
    {
        while (children != NULL)
        {
            ps_children = children->node->children->node->children;
            switch (ps_children->node->cl_obj->gid)
            {
            case CLERI_GID_K_PREFIX:
                nps->prefix =
                        (char *) malloc(ps_children->next->node->len + 1);
                if (nps->prefix != NULL)
                {
                    /* not critical if suffix is still NULL */
                    nps->len += xstr_extract_string(
                            nps->prefix,
                            ps_children->next->node->str,
                            ps_children->next->node->len);
                }
                break;
            case CLERI_GID_K_SUFFIX:
                nps->suffix =
                        (char *) malloc(ps_children->next->node->len + 1);
                if (nps->suffix != NULL)
                {
                    /* not critical if suffix is still NULL */
                    nps->len += xstr_extract_string(
                            nps->suffix,
                            ps_children->next->node->str,
                            ps_children->next->node->len);
                }
                break;
            default:
                assert (0);
                break;
            }
            children = children->next;
        }
    }
    return nps;
}

/*
 * Check if the last presuf is unique compared to the others.
 */
int siridb_presuf_is_unique(siridb_presuf_t * presuf)
{
    const char * prefix = presuf->prefix;
    const char * suffix = presuf->suffix;
    while (presuf->prev != NULL)
    {
        presuf = presuf->prev;
        if (    PRESUF_is_equal(prefix, presuf->prefix) &&
                PRESUF_is_equal(suffix, presuf->suffix))
        {
            return 0;  /* false, not unique */
        }
    }
    return 1;  /* true, unique */
}

const char * siridb_presuf_name(
        siridb_presuf_t * presuf,
        const char * name,
        size_t len)
{
    size_t size = len + presuf->len;
    if (size > presuffed_size)
    {
        char * tmp;
        tmp = (char *) realloc(presuffed_name, size);
        if (tmp == NULL)
        {
            ERR_ALLOC
            return NULL;
        }
        presuffed_size = size;
        presuffed_name = tmp;
    }
    sprintf(presuffed_name,
            "%s%s%s",
            (presuf->prefix == NULL) ? "" : presuf->prefix,
            name,
            (presuf->suffix == NULL) ? "" : presuf->suffix);
    return presuffed_name;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * In case of an error, the original presuf object remains unchanged,
 * when successful the returned object is the same as the presuf object.
 */
siridb_presuf_t * PRESUF_add(siridb_presuf_t ** presuf)
{
    siridb_presuf_t * newps =
            (siridb_presuf_t *) malloc(sizeof(siridb_presuf_t));
    if (newps == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        newps->prefix = NULL;
        newps->suffix = NULL;
        newps->len = 1;  /* we include the terminator char in the length */
        newps->prev = *presuf;
        *presuf = newps;
    }

    return newps;
}

/*
 * Like 'strcmp' but also accepts NULL.
 */
int PRESUF_is_equal(const char * a, const char * b)
{
    if ((a == NULL) ^ (b == NULL))
    {
        return 0;
    }
    return (a == NULL) || (strcmp(a, b) == 0);
}
