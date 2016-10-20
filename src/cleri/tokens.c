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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

static void TOKENS_free(cleri_object_t * cl_object);

static cleri_node_t * TOKENS_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static int TOKENS_list_append(
        cleri_tlist_t * tlist,
        const char * token,
        size_t len);

static void TOKENS_list_free(cleri_tlist_t * tlist);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
cleri_object_t * cleri_tokens(
        uint32_t gid,
        const char * tokens)
{
    size_t len;
    char * pt;
    cleri_object_t * cl_object;

    cl_object = cleri_object_new(
            CLERI_TP_TOKENS,
            &TOKENS_free,
            &TOKENS_parse);

    if (cl_object == NULL)
    {
        return NULL;  /* signal is set */
    }

    cl_object->via.tokens =
            (cleri_tokens_t *) malloc(sizeof(cleri_tokens_t));

    if (cl_object->via.tokens == NULL)
    {
        free(cl_object);
        return NULL;
    }

    cl_object->via.tokens->gid = gid;

    /* copy the sting twice, first one we set spaces to 0...*/
    cl_object->via.tokens->tokens = strdup(tokens);

    /* ...and this one we keep for showing the original */
    cl_object->via.tokens->spaced = strdup(tokens);

    cl_object->via.tokens->tlist =
            (cleri_tlist_t *) malloc(sizeof(cleri_tlist_t));

    if (    cl_object->via.tokens->tokens == NULL ||
            cl_object->via.tokens->spaced == NULL ||
            cl_object->via.tokens->tlist == NULL)
    {
        cleri_object_decref(cl_object);
        return NULL;
    }

    cl_object->via.tokens->tlist->token = NULL;
    cl_object->via.tokens->tlist->next = NULL;
    cl_object->via.tokens->tlist->len = 0;

    pt = cl_object->via.tokens->tokens;

    for (len = 0;; pt++)
    {
        if (!*pt || isspace(*pt))
        {
            if (len)
            {
                if (TOKENS_list_append(
                        cl_object->via.tokens->tlist,
                        pt - len,
                        len))
                {
                    cleri_object_decref(cl_object);
                    return NULL;
                }
                len = 0;
            }
            if (*pt)
                *pt = 0;
            else
            {
                break;
            }
        }
        else
        {
            len++;
        }
    }

#ifdef DEBUG
    /* check for empty token list */
    assert (cl_object->via.tokens->tlist->token != NULL);
#endif

    return cl_object;
}

/*
 * Destroy token object.
 */
static void TOKENS_free(cleri_object_t * cl_object)
{
    TOKENS_list_free(cl_object->via.tokens->tlist);
    free(cl_object->via.tokens->tokens);
    free(cl_object->via.tokens->spaced);
    free(cl_object->via.tokens);
}

/*
 * Returns a node or NULL. In case of an error cleri_err is set to -1.
 */
static cleri_node_t * TOKENS_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node = NULL;
    const char * str = parent->str + parent->len;
    cleri_tlist_t * tlist = cl_obj->via.tokens->tlist;

    /* we can trust that at least one token is in the list */
    for (; tlist != NULL; tlist = tlist->next)
    {
        if (strncmp(tlist->token, str, tlist->len) == 0)
        {
            if ((node = cleri_node_new(cl_obj, str, tlist->len)) != NULL)
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
            return node;
        }
    }
    if (cleri_expecting_update(pr->expecting, cl_obj, str) == -1)
    {
        cleri_err = -1;
    }
    return NULL;
}

/*
 * Returns 0 if successful and -1 in case an error occurred.
 * (the token list remains unchanged in case of an error)
 */
static int TOKENS_list_append(
        cleri_tlist_t * tlist,
        const char * token,
        size_t len)
{
    if (tlist->token == NULL)
    {
        tlist->token = token;
        tlist->len = len;
        return 0;
    }

    while (tlist->next != NULL)
    {
        tlist = tlist->next;
    }

    tlist->next = (cleri_tlist_t *) malloc(sizeof(cleri_tlist_t));
    if (tlist->next == NULL)
    {
        return -1;
    }
    tlist->next->len = len;
    tlist->next->token = token;
    tlist->next->next = NULL;

    return 0;
}

/*
 * Destroy token list. (NULL can be parsed tlist argument)
 */
static void TOKENS_list_free(cleri_tlist_t * tlist)
{
    cleri_tlist_t * next;
    while (tlist != NULL)
    {
        next = tlist->next;
        free(tlist);
        tlist = next;
    }
}

