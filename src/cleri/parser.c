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

cleri_parse_result_t * cleri_parse(cleri_grammar_t * grammar, const char * str)
{
    cleri_parse_result_t * pr;
    cleri_node_t * rnode;
    const char * end;
    bool at_end;

    /* prepare parsing */
    pr = (cleri_parse_result_t *) malloc(sizeof(cleri_parse_result_t));
    pr->str = str;
    pr->tree = cleri_new_node(NULL, str, 0);
    pr->kwcache = cleri_new_kwcache();
    pr->expecting = cleri_new_expecting(str);
    pr->re_keywords = grammar->re_keywords;
    pr->re_kw_extra = grammar->re_kw_extra;

    /* do the actual parsing */
    rnode = cleri_walk(
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
        cleri_expecting_update(
                pr->expecting,
                CLERI_END_OF_STATEMENT,
                end);
    }

    /* append optional to required and set optional to NULL */
    cleri_combine_expecting(pr->expecting);

    return pr;
}

void cleri_free_parse_result(cleri_parse_result_t * pr)
{
    cleri_free_node(pr->tree);
    cleri_free_expecting(pr->expecting);
    cleri_free_kwcache(pr->kwcache);
    free(pr);
}

cleri_node_t * cleri_walk(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule,
        int mode)
{
    /* set parent len to next none white space char */
    while (isspace(*(parent->str + parent->len)))
        parent->len++;

    /* set expecting mode */
    cleri_expecting_set_mode(pr->expecting, parent->str, mode);

    /* note that the actual node is returned or NULL but we do not
     * actually need the node. (boolean true/false would be enough)
     */
    return (cl_obj->parse_object == NULL) ? NULL :
            (*cl_obj->parse_object)(pr, parent, cl_obj, rule);
}
