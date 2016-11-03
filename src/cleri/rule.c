/*
 * rule.c - cleri regular rule element. (do not directly use this element but
 *          create a 'prio' instead which will be wrapped by a rule element)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/rule.h>
#include <stdlib.h>

static void RULE_free(cleri_object_t * cl_object);
static cleri_node_t * RULE_parse(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);
static void RULE_tested_free(cleri_rule_tested_t * tested);

/*
 * Returns NULL in case an error has occurred.
 */
cleri_object_t * cleri_rule(uint32_t gid, cleri_object_t * cl_obj)
{
    if (cl_obj == NULL)
    {
        return NULL;
    }

    cleri_object_t * cl_object = cleri_object_new(
            CLERI_TP_RULE,
            &RULE_free,
            &RULE_parse);

    if (cl_object != NULL)
    {
		cl_object->via.rule =
				(cleri_rule_t *) malloc(sizeof(cleri_rule_t));

		if (cl_object->via.rule == NULL)
		{
			free(cl_object);
			cl_object = NULL;
		}
		else
		{
			cl_object->via.rule->gid = gid;
			cl_object->via.rule->cl_obj = cl_obj;
			cleri_object_incref(cl_obj);
		}
    }

    return cl_object;
}

/*
 * Initialize a rule and return the test result.
 * Result can be either CLERI_RULE_TRUE, CLERI_RULE_FALSE or CLERI_RULE_ERROR.
 *
 *  - CLERI_RULE_TRUE: a new test is created
 *  - CLERI_RULE_FALSE: no new test is created
 *  - CLERI_RULE_ERROR: an error occurred
 */
cleri_rule_test_t cleri_rule_init(
        cleri_rule_tested_t ** target,
        cleri_rule_tested_t * tested,
        const char * str)
{
    /*
     * return true (1) when a new test is created, false (0) when not.
     */
    cleri_rule_tested_t * prev;

    (*target) = tested;

    if ((*target)->str == NULL)
    {
        (*target)->str = str;
        return CLERI_RULE_TRUE;
    }

    while ((*target) != NULL)
    {
        if ((*target)->str == str)
        {
            return CLERI_RULE_FALSE;
        }
        prev = (*target);
        (*target) = (*target)->next;
    }
    *target = prev->next =
            (cleri_rule_tested_t *) malloc(sizeof(cleri_rule_tested_t));

    if (*target == NULL)
    {
        return CLERI_RULE_ERROR;
    }
    (*target)->str = str;
    (*target)->node = NULL;
    (*target)->next = NULL;

    return CLERI_RULE_TRUE;
}

static void RULE_free(cleri_object_t * cl_object)
{
    cleri_object_decref(cl_object->via.rule->cl_obj);
    free(cl_object->via.rule);
}

/*
 * Returns a node or NULL. In case of an error cleri_err is set to -1.
 */
static cleri_node_t * RULE_parse(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * __rule)
{
    cleri_node_t * node;
    cleri_node_t * rnode;
    cleri_rule_store_t nrule;

    if ((node = cleri_node_new(cl_obj, parent->str + parent->len, 0)) == NULL)
    {
    	pr->is_valid = -1;
        return NULL;
    }

    nrule.depth = 0;
    nrule.tested = (cleri_rule_tested_t *) malloc(sizeof(cleri_rule_tested_t));

    if (nrule.tested == NULL)
    {
    	pr->is_valid = -1;
        cleri_node_free(node);
        return NULL;
    }

    nrule.tested->str = NULL;
    nrule.tested->node = NULL;
    nrule.tested->next = NULL;
    nrule.root_obj = cl_obj->via.rule->cl_obj;

    rnode = cleri__parse_walk(
            pr,
            node,
			nrule.root_obj,
            &nrule,
            CLERI_EXP_MODE_REQUIRED);


    if (rnode == NULL)
    {
        cleri_node_free(node);
        node = NULL;
    }
    else
    {
        parent->len += node->len;
        if (cleri_children_add(parent->children, node))
        {
			 /* error occurred, reverse changes set mg_node to NULL */
        	pr->is_valid = -1;
			parent->len -= node->len;
			cleri_node_free(node);
			node = NULL;
        }
    }

    /* cleanup rule */
    RULE_tested_free(nrule.tested);

    return node;
}

/*
 * Cleanup rule tested
 */
static void RULE_tested_free(cleri_rule_tested_t * tested)
{
    cleri_rule_tested_t * next;
    while (tested != NULL)
    {
        next = tested->next;
        free(tested);
        tested = next;
    }
}

