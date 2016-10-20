/*
 * this.c - cleri THIS element. there should be only one single instance
 *          of this which can even be shared over different grammars.
 *          Always use this element using its constant CLERI_THIS and
 *          somewhere within a prio element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/expecting.h>
#include <cleri/this.h>
#include <stdlib.h>
#include <assert.h>

static cleri_node_t * cleri_parse_this(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static cleri_dummy_t cleri_dummy = {.gid=0};

static cleri_object_t cleri_this = {
        .tp=CLERI_TP_THIS,
        .free_object=NULL,
        .parse_object=&cleri_parse_this,
        .via={.dummy=(cleri_dummy_t *) &cleri_dummy}};

cleri_object_t * CLERI_THIS = &cleri_this;

/*
 * Returns a node or NULL. In case of an error cleri_err is set to -1.
 */
static cleri_node_t * cleri_parse_this(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node;
    cleri_rule_tested_t * tested;
    const char * str = parent->str + parent->len;

    switch (cleri_rule_init(&tested, rule->tested, str))
    {
    case CLERI_RULE_TRUE:
        if ((node = cleri_node_new(cl_obj, str, 0)) == NULL)
        {
        	cleri_err = -1;
            return NULL;
        }
        tested->node = cleri__parser_walk(
                pr,
                node,
                rule->root_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);

        if (tested->node == NULL)
        {
            cleri_node_free(node);
            return NULL;
        }
        break;
    case CLERI_RULE_FALSE:
        node = tested->node;
        if (node == NULL)
        {
            return NULL;
        }
        node->ref++;
        break;
    case CLERI_RULE_ERROR:
    	cleri_err = -1;
        return NULL;

    default:
        assert (0);
        node = NULL;
    }

    parent->len += tested->node->len;
    if (cleri_children_add(parent->children, node))
    {
		 /* error occurred, reverse changes set mg_node to NULL */
		cleri_err = -1;
		parent->len -=  tested->node->len;
		cleri_node_free(node);
		node = NULL;
    }
    return node;
}
