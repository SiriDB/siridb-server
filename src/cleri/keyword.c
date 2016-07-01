/*
 * keyword.c - cleri keyword element
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/keyword.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>
#include <siri/err.h>

static void KEYWORD_free(cleri_object_t * cl_object);

static cleri_node_t * KEYWORD_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

/*
 * Returns NULL and sets a signal in case an error has occurred.
 */
cleri_object_t * cleri_keyword(
        uint32_t gid,
        const char * keyword,
        int ign_case)
{
    cleri_object_t * cl_object = cleri_object_new(
            CLERI_TP_KEYWORD,
            &KEYWORD_free,
            &KEYWORD_parse);

    if (cl_object == NULL)
    {
        return NULL;  /* signal is set */
    }

    cl_object->via.keyword =
            (cleri_keyword_t *) malloc(sizeof(cleri_keyword_t));

    if (cl_object->via.tokens == NULL)
    {
        ERR_ALLOC
        free(cl_object);
        return NULL;
    }

    cl_object->via.keyword->gid = gid;
    cl_object->via.keyword->keyword = keyword;
    cl_object->via.keyword->ign_case = ign_case;
    cl_object->via.keyword->len = strlen(keyword);

    return cl_object;
}

/*
 * Destroy keyword object.
 */
static void KEYWORD_free(cleri_object_t * cl_object)
{
    free(cl_object->via.keyword);
}

static cleri_node_t * KEYWORD_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    size_t match_len;
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;

    match_len = cleri_kwcache_match(pr, str);
    if (match_len == cl_obj->via.keyword->len &&
       (
           strncmp(cl_obj->via.keyword->keyword, str, match_len) == 0 ||
           (
               cl_obj->via.keyword->ign_case &&
               strncasecmp(cl_obj->via.keyword->keyword, str, match_len) == 0
           )
       ))
    {
        node = cleri_node_new(cl_obj, str, match_len);
        parent->len += node->len;
        cleri_children_add(parent->children, node);
    }
    else
    {
        /* Update expecting */
        cleri_expecting_update(pr->expecting, cl_obj, str);
    }
    return node;
}
