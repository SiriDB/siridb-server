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

static void cleri_free_keyword(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_keyword(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_keyword(
        uint32_t gid,
        const char * keyword,
        int ign_case)
{
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_KEYWORD,
            &cleri_free_keyword,
            &cleri_parse_keyword);
    cl_object->cl_obj->keyword =
            (cleri_keyword_t *) malloc(sizeof(cleri_keyword_t));
    cl_object->cl_obj->keyword->gid = gid;
    cl_object->cl_obj->keyword->keyword = keyword;
    cl_object->cl_obj->keyword->ign_case = ign_case;
    cl_object->cl_obj->keyword->len = strlen(keyword);
    return cl_object;
}

static void cleri_free_keyword(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    free(cl_obj->cl_obj->keyword);
}

static cleri_node_t * cleri_parse_keyword(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    size_t match_len;
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;

    match_len = cleri_kwcache_match(pr, str);
    if (match_len == cl_obj->cl_obj->keyword->len &&
       (
           strncmp(cl_obj->cl_obj->keyword->keyword, str, match_len) == 0 ||
           (
               cl_obj->cl_obj->keyword->ign_case &&
               strncasecmp(cl_obj->cl_obj->keyword->keyword, str, match_len) == 0
           )
       ))
    {
        node = cleri_new_node(cl_obj, str, match_len);
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
