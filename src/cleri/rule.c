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
#include <logger/logger.h>
#include <stdlib.h>

static void cleri_free_rule(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_rule(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static void cleri_free_rule_tested(cleri_rule_tested_t * tested);

cleri_object_t * cleri_rule(uint32_t gid, cleri_object_t * cl_obj)
{
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_RULE,
            &cleri_free_rule,
            &cleri_parse_rule);
    cl_object->cl_obj->rule =
            (cleri_rule_t *) malloc(sizeof(cleri_rule_t));
    cl_object->cl_obj->rule->gid = gid;
    cl_object->cl_obj->rule->cl_obj = cl_obj;
    return cl_object;
}

int cleri_init_rule_tested(
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
        return 1;
    }

    while ((*target) != NULL)
    {
        if ((*target)->str == str)
            return 0;
        prev = (*target);
        (*target) = (*target)->next;
    }
    (*target) = prev->next =
            (cleri_rule_tested_t *) malloc(sizeof(cleri_rule_tested_t));
    (*target)->str = str;
    (*target)->node = NULL;
    (*target)->next = NULL;

    return 1;
}

static void cleri_free_rule(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_object(grammar, cl_obj->cl_obj->optional->cl_obj);
    free(cl_obj->cl_obj->optional);
}

static cleri_node_t * cleri_parse_rule(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node;
    cleri_node_t * rnode;
    node = cleri_new_node(cl_obj, parent->str + parent->len, 0);

    rule = (cleri_rule_store_t *) malloc(sizeof(cleri_rule_store_t));
    rule->tested = (cleri_rule_tested_t *) malloc(sizeof(cleri_rule_tested_t));
    rule->tested->str = NULL;
    rule->tested->node = NULL;
    rule->tested->next = NULL;
    rule->root_obj = cl_obj->cl_obj->rule->cl_obj;

    rnode = cleri_walk(
            pr,
            node,
            rule->root_obj,
            rule,
            CLERI_EXP_MODE_REQUIRED);


    if (rnode == NULL)
    {
        cleri_free_node(node);
        node = NULL;
    }
    else
    {
        parent->len += node->len;
        cleri_children_add(parent->children, node);
    }

    /* cleanup rule */
    cleri_free_rule_tested(rule->tested);
    free(rule);

    return node;
}

static void cleri_free_rule_tested(cleri_rule_tested_t * tested)
{
    cleri_rule_tested_t * next;
    while (tested != NULL)
    {
        next = tested->next;
        free(tested);
        tested = next;
    }
}

