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
#include <stdlib.h>
#include <string.h>

static void TOKEN_free(cleri_object_t * cl_object);

static cleri_node_t * TOKEN_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

/*
 * Returns NULL in case an error has occurred.
 */
cleri_object_t * cleri_token(
        uint32_t gid,
        const char * token)
{
    cleri_object_t * cl_object = cleri_object_new(
            CLERI_TP_TOKEN,
            &TOKEN_free,
            &TOKEN_parse);


    if (cl_object == NULL)
    {
        return NULL;  /* signal is set */
    }

    cl_object->via.token =
            (cleri_token_t *) malloc(sizeof(cleri_token_t));

    if (cl_object->via.token == NULL)
    {
        free(cl_object);
        return NULL;
    }

    cl_object->via.token->gid = gid;
    cl_object->via.token->token = token;
    cl_object->via.token->len = strlen(token);

    return cl_object;
}

/*
 * Destroy token object.
 */
static void TOKEN_free(cleri_object_t * cl_object)
{
    free(cl_object->via.token);
}

/*
 * Returns a node or NULL. In case of an error cleri_err is set to -1.
 */
static cleri_node_t * TOKEN_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;
    if (strncmp(
            cl_obj->via.token->token,
            str,
            cl_obj->via.token->len) == 0)
    {
        if ((node = cleri_node_new(cl_obj, str, cl_obj->via.token->len)) != NULL)
        {
            parent->len += node->len;
            if (cleri_children_add(parent->children, node))
            {
				 /* error occurred, reverse changes set mg_node to NULL */
				cleri_err = -1;
				parent->len -= node->len;
				cleri_node_free(node);
				node = NULL;
            }
        }
        else
        {
        	cleri_err = -1;
        }
    }
    else if (cleri_expecting_update(pr->expecting, cl_obj, str) == -1)
    {
    	cleri_err = -1;
    }
    return node;
}

