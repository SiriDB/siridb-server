/*
 * parser.c - this contains the start for parsing a string to a grammar.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/parser.h>
#include <logger/logger.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <strextra/strextra.h>
#include <siri/err.h>

/*
 * Returns NULL and sets a signal in case an error has occurred.
 */
cleri_parser_t * cleri_parser_new(cleri_grammar_t * grammar, const char * str)
{
    cleri_parser_t * pr;
    cleri_node_t * rnode;
    const char * end;
    bool at_end;

    /* prepare parsing */
    pr = (cleri_parser_t *) malloc(sizeof(cleri_parser_t));
    if (pr == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    pr->str = str;
    pr->tree = NULL;
    pr->kwcache = NULL;
    pr->expecting = NULL;

    if (    (pr->tree = cleri_node_new(NULL, str, 0)) == NULL ||
            (pr->kwcache = cleri_kwcache_new()) == NULL ||
            (pr->expecting = cleri_expecting_new(str)) == NULL)
    {
        cleri_parser_free(pr);
        return NULL;
    }

    pr->re_keywords = grammar->re_keywords;
    pr->re_kw_extra = grammar->re_kw_extra;

    /* do the actual parsing */
    rnode = cleri__parser_walk(
            pr,
            pr->tree,
            grammar->start,
            NULL,
            CLERI_EXP_MODE_REQUIRED);

    /* process the parse result */
    end = pr->tree->str + pr->tree->len;
    at_end = strx_is_empty(end);
    pr->is_valid = rnode != NULL && at_end;
    pr->pos = (pr->is_valid) ? pr->tree->len : pr->expecting->str - pr->str;

    if (!at_end && pr->expecting->required->cl_obj == NULL)
    {
        cleri_expecting_set_mode(
                pr->expecting,
                end,
                CLERI_EXP_MODE_REQUIRED);
        if (cleri_expecting_update(
                pr->expecting,
                CLERI_END_OF_STATEMENT,
                end) == -1)
        {
            ERR_ALLOC
        }
    }

    cleri_expecting_combine(pr->expecting);

    if (siri_err)
    {
        cleri_parser_free(pr);
        pr = NULL;
    }

    return pr;
}

/*
 * Destroy parser. (parsing NULL is allowed)
 */
void cleri_parser_free(cleri_parser_t * pr)
{
    cleri_node_free(pr->tree);
    cleri_kwcache_free(pr->kwcache);
    if (pr->expecting != NULL)
    {
        cleri_expecting_free(pr->expecting);
    }
    free(pr);
}

/*
 * Walk a parser object.
 * (recursive function, called from each parse_object function)
 * Returns a node or NULL. In case of errors, a signal is set.
 */
cleri_node_t * cleri__parser_walk(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule,
        int mode)
{
    /* set parent len to next none white space char */
    while (isspace(*(parent->str + parent->len)))
    {
        parent->len++;
    }

    /* set expecting mode */
    if (cleri_expecting_set_mode(pr->expecting, parent->str, mode) == -1)
    {
        return NULL;
    }

    /* note that the actual node is returned or NULL but we do not
     * actually need the node. (boolean true/false would be enough)
     */
    return (cl_obj->parse_object == NULL) ? NULL :
            (*cl_obj->parse_object)(pr, parent, cl_obj, rule);
}
