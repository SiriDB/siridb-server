/*
 * token.c - cleri token element. note that one single char will parse
 *           slightly faster compared to tokens containing more characters.
 *           (be careful a token should not match the keyword regular
 *           expression)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/token.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>

static void cleri_free_token(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_token(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_token(
        uint32_t gid,
        const char * token)
{
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_TOKEN,
            &cleri_free_token,
            &cleri_parse_token);
    cl_object->cl_obj->token =
            (cleri_token_t *) malloc(sizeof(cleri_token_t));
    cl_object->cl_obj->token->gid = gid;
    cl_object->cl_obj->token->token = token;
    cl_object->cl_obj->token->len = strlen(token);
    return cl_object;
}

static void cleri_free_token(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    free(cl_obj->cl_obj->token);
}

static cleri_node_t * cleri_parse_token(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;
    if (strncmp(
            cl_obj->cl_obj->token->token,
            str,
            cl_obj->cl_obj->token->len) == 0)
    {
        node = cleri_new_node(cl_obj, str, cl_obj->cl_obj->token->len);
        parent->len += node->len;
        cleri_children_add(parent->children, node);
    }
    else
    {
        cleri_expecting_update(pr->expecting, cl_obj, str);
    }
    return node;
}

