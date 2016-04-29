/*
 * tokens.c - cleri tokens element. (like token but can contain more tokens
 *            in one element)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/tokens.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static void cleri_free_tokens(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_tokens(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static void cleri_tlist_add(
        cleri_tlist_t * tlist,
        const char * token,
        size_t len);

static void cleri_free_tlist(cleri_tlist_t * tlist);

cleri_object_t * cleri_tokens(
        uint32_t gid,
        const char * tokens)
{
    size_t len;
    char * pt;
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_TOKENS,
            &cleri_free_tokens,
            &cleri_parse_tokens);
    cl_object->cl_obj->tokens =
            (cleri_tokens_t *) malloc(sizeof(cleri_tokens_t));
    cl_object->cl_obj->tokens->gid = gid;


    /* copy the sting twice, first one we set spaces to 0...*/
    cl_object->cl_obj->tokens->tokens = strdup(tokens);

    /* ...and this one we keep for showing the original */
    cl_object->cl_obj->tokens->spaced = strdup(tokens);

    cl_object->cl_obj->tokens->tlist =
            (cleri_tlist_t *) malloc(sizeof(cleri_tlist_t));
    cl_object->cl_obj->tokens->tlist->token = NULL;
    cl_object->cl_obj->tokens->tlist->next = NULL;
    cl_object->cl_obj->tokens->tlist->len = 0;

    pt = cl_object->cl_obj->tokens->tokens;

    for (len = 0;; pt++)
    {
        if (!*pt || isspace(*pt))
        {
            if (len)
            {
                cleri_tlist_add(
                        cl_object->cl_obj->tokens->tlist,
                        pt - len,
                        len);
                len = 0;
            }
            if (*pt)
                *pt = 0;
            else
                break;
        }
        else
            len++;
    }

    if (cl_object->cl_obj->tokens->tlist->token == NULL)
    {
        printf("Got an empty tokens object!");
        exit(EXIT_FAILURE);
    }

    return cl_object;
}

static void cleri_free_tokens(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_tlist(cl_obj->cl_obj->tokens->tlist);
    free(cl_obj->cl_obj->tokens->tokens);
    free(cl_obj->cl_obj->tokens->spaced);
    free(cl_obj->cl_obj->tokens);
}

static cleri_node_t * cleri_parse_tokens(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;
    cleri_tlist_t * tlist = cl_obj->cl_obj->tokens->tlist;

    /* we can trust that at least one token is in the list */
    for (; tlist != NULL; tlist = tlist->next)
    {
        if (strncmp(tlist->token, str, tlist->len) == 0)
        {
            node = cleri_new_node(cl_obj, str, tlist->len);
            parent->len += node->len;
            cleri_children_add(parent->children, node);
            return node;
        }
    }
    cleri_expecting_update(pr->expecting, cl_obj, str);
    return NULL;
}

static void cleri_tlist_add(
        cleri_tlist_t * tlist,
        const char * token,
        size_t len)
{
    if (tlist->token == NULL)
    {
        tlist->token = token;
        tlist->len = len;
        return;
    }
    while (tlist->next != NULL)
        tlist = tlist->next;

    tlist->next = (cleri_tlist_t *) malloc(sizeof(cleri_tlist_t));
    tlist->next->len = len;
    tlist->next->token = token;
    tlist->next->next = NULL;
}

static void cleri_free_tlist(cleri_tlist_t * tlist)
{
    cleri_tlist_t * next;
    while (tlist != NULL)
    {
        next = tlist->next;
        free(tlist);
        tlist = next;
    }
}

