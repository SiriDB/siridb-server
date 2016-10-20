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
#include <stdlib.h>
#include <string.h>

static void KEYWORD_free(cleri_object_t * cl_object);

static cleri_node_t * KEYWORD_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

/*
 * Returns NULL in case an error has occurred.
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

/*
 * Returns a node or NULL. (result NULL can be also be caused by an error)
 */
static cleri_node_t * KEYWORD_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    size_t match_len;
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;

    if ((match_len = cleri_kwcache_match(pr, str)) < 0)
    {
    	cleri_err = -1; /* error occurred */
    	return NULL;
    }

    if (match_len == cl_obj->via.keyword->len &&
       (
           strncmp(cl_obj->via.keyword->keyword, str, match_len) == 0 ||
           (
               cl_obj->via.keyword->ign_case &&
               strncasecmp(cl_obj->via.keyword->keyword, str, match_len) == 0
           )
       ))
    {
        if ((node = cleri_node_new(cl_obj, str, match_len)) != NULL)
        {
            parent->len += node->len;
            cleri_children_add(parent->children, node);
        }
    }
    else
    {
        /* Update expecting */
        if (cleri_expecting_update(pr->expecting, cl_obj, str) == -1)
        {
            /* error occurred, node is already NULL */
        	cleri_err = -1;
        }
    }
    return node;
}
