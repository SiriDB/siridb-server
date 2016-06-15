/*
 * cexpr.c - Conditional expressions.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-06-2016
 *
 */
#include <cexpr/cexpr.h>
#include <assert.h>
#include <stddef.h>
#include <siri/grammar/grammar.h>
#include <logger/logger.h>
#include <siri/db/series.h>
#include <strextra/strextra.h>

#define VIA_NULL 0
#define VIA_CEXPR 1
#define VIA_COND 2

#define EXPECTING_PROP 0
#define EXPECTING_OPERATOR 1
#define EXPECTING_VAL 2
#define EXPECTING_NEXT 3

/* make sure the condition is set to NULL because we will free
 * the pointer in case CEXPR_MAX_CURLY_DEPT occurs.
 */
#define SET_CONDITION_AND_RETURN                \
    CEXPR_push_condition(cexpr, *condition);    \
    *condition = NULL;                          \
    (*expecting) = EXPECTING_NEXT;              \
    return cexpr;

static cexpr_t * CEXPR_new(void);
static cexpr_condition_t * CEXPR_condition_new(void);
static void CEXPR_condition_free(cexpr_condition_t * cond);
static void CEXPR_push_condition(cexpr_t * cexpr, cexpr_condition_t * cond);
static cexpr_t * CEXPR_push_and(cexpr_t * cexpr);
static cexpr_t * CEXPR_push_or(cexpr_t * cexpr);
static cexpr_t * CEXPR_open_curly(cexpr_t * cexpr, cexpr_list_t * list);
static cexpr_t * CEXPR_close_curly(cexpr_t * cexpr, cexpr_list_t * list);

static cexpr_t * CEXPR_walk_node(
        cleri_node_t * node,
        cexpr_t * cexpr,
        cexpr_list_t * list,
        cexpr_condition_t ** condition,
        int * expecting);

cexpr_t * cexpr_from_node(cleri_node_t * node)
{
    cexpr_t * cexpr = CEXPR_new();
    int expecting = EXPECTING_PROP;
    cexpr_condition_t * condition = CEXPR_condition_new();

    /* create a list, we only need this list while building an expression */
    cexpr_list_t list;
    list.len = 0;

    /* by simply wrapping the expression between curly brackets we make sure
     * we start validating at the correct start.
     */
    CEXPR_open_curly(cexpr, &list);

    /* build the expression */
    cexpr = CEXPR_walk_node(node, cexpr, &list, &condition, &expecting);

    /* test if successful */
    if (cexpr != NULL)
    {
        /* successful */
        cexpr = CEXPR_close_curly(cexpr, &list);
#ifdef DEBUG
        assert (list.len == 0 && condition == NULL);
#endif
    }
    else if (condition != NULL)
    {
        /* CEXPR_MAX_CURLY_DEPT, logging is done by the listener */
        CEXPR_condition_free(condition);
    }

    return cexpr;
}


int cexpr_icmp(cexpr_operator_t operator, int64_t a, int64_t b)
{
    switch (operator)
    {
    case CEXPR_EQ:
        return a == b;
    case CEXPR_NE:
        return a != b;
    case CEXPR_GT:
        return a > b;
    case CEXPR_LT:
        return a < b;
    case CEXPR_GE:
        return a >= b;
    case CEXPR_LE:
        return a <= b;
    default:
        log_critical("Got an unexpected operator (integer type): %d", operator);
        assert (0);
    }
    return -1;
}

int cexpr_run(cexpr_t * cexpr, cexpr_cb_t cb, void * obj)
{
    /* should return either 1 or 0. (true or false) */

    switch (cexpr->operator)
    {
    case CEXPR_AND:
#ifdef DEBUG
        /* tp_a cannot be VIA_NULL, but tp_b can */
        assert (cexpr->tp_a != VIA_NULL);
#endif
        return  ((cexpr->tp_a == VIA_CEXPR) ?
                    cexpr_run(cexpr->via_a.cexpr, cb, obj) :
                    cb(obj, cexpr->via_a.cond)) &&
                (cexpr->tp_b == VIA_NULL || ((cexpr->tp_b == VIA_CEXPR) ?
                    cexpr_run(cexpr->via_b.cexpr, cb, obj) :
                    cb(obj, cexpr->via_b.cond)));
    case CEXPR_OR:
#ifdef DEBUG
        /* both tp_a and tp_b can NEVER be VIA_NULL */
        assert (cexpr->tp_a != VIA_NULL && cexpr->tp_b != VIA_NULL);
#endif
        return  ((cexpr->tp_a == VIA_CEXPR) ?
                    cexpr_run(cexpr->via_a.cexpr, cb, obj) :
                    cb(obj, cexpr->via_a.cond)) ||
                ((cexpr->tp_b == VIA_CEXPR) ?
                    cexpr_run(cexpr->via_b.cexpr, cb, obj) :
                    cb(obj, cexpr->via_b.cond));
    default:
        log_critical("operator must be AND or OR, got: %d", cexpr->operator);
        assert (0);
    }
    return -1; /* this should NEVER happen */
}

void cexpr_free(cexpr_t * cexpr)
{
    log_debug("Free expression!");
    switch (cexpr->tp_a)
    {
    case VIA_CEXPR: cexpr_free(cexpr->via_a.cexpr); break;
    case VIA_COND: CEXPR_condition_free(cexpr->via_a.cond); break;
    }

    switch (cexpr->tp_b)
    {
    case VIA_CEXPR: cexpr_free(cexpr->via_b.cexpr); break;
    case VIA_COND: CEXPR_condition_free(cexpr->via_b.cond); break;
    }

    free(cexpr);
}

static cexpr_t * CEXPR_walk_node(
        cleri_node_t * node,
        cexpr_t * cexpr,
        cexpr_list_t * list,
        cexpr_condition_t ** condition,
        int * expecting)
{
    switch (*expecting)
    {
    case EXPECTING_PROP:
        switch (node->cl_obj->tp)
        {
        case CLERI_TP_TOKEN:
            /* this must be an open curly */
            return CEXPR_open_curly(cexpr, list);

        case CLERI_TP_KEYWORD:
            /* this is a property we are looking for */
            (*condition)->prop = node->cl_obj->cl_obj->keyword->gid;
            (*expecting) = EXPECTING_OPERATOR;
            return cexpr;

        default:
            /* this is probably a CHOICE */
            break;
        }
        break;
    case EXPECTING_OPERATOR:
        switch (node->cl_obj->tp)
        {
        case CLERI_TP_TOKENS:
            /* this one of the following operators:
             *      ==  <  <=  >  >=  !=  ~  !~
             */
            if (node->len == 1)
            {
                switch (*node->str)
                {
                case '>': (*condition)->operator = CEXPR_GT; break;
                case '<': (*condition)->operator = CEXPR_LT; break;
                case '~': (*condition)->operator = CEXPR_IN; break;
                }
            }
            else
            {
#ifdef DEBUG
                assert (node->len == 2);
#endif
                switch (*node->str)
                {
                case '=': (*condition)->operator = CEXPR_EQ; break;
                case '!': (*condition)->operator = (*(node->str + 1) == '=') ?
                            CEXPR_NE : CEXPR_NI; break;
                case '>': (*condition)->operator = CEXPR_GE; break;
                case '<': (*condition)->operator = CEXPR_LE; break;
                }
            }
            (*expecting) = EXPECTING_VAL;
            return cexpr;

        default:
            log_critical(
                    "Got an unexpected operator type: %d", node->cl_obj->tp);
            assert (0);
        }
        break;
    case EXPECTING_VAL:
        switch (node->cl_obj->tp)
        {
        case CLERI_TP_RULE:
            /* this is an integer or time expression, we can set the result
             * and the condition.
             */
            (*condition)->int64 = node->result;
            SET_CONDITION_AND_RETURN
        case CLERI_TP_CHOICE:
            /* in case of a string, set the value and return */
            if (node->cl_obj->cl_obj->choice->gid == CLERI_GID_STRING)
            {
                (*condition)->str = (char *) malloc(node->len -1);
                strx_extract_string((*condition)->str, node->str, node->len);
                SET_CONDITION_AND_RETURN
            }
            /* can be a choice between keywords, in that case just wait */
            break;
        case CLERI_TP_KEYWORD:
            /* for some keywords we can do some work to speed up checks */
            switch (node->cl_obj->cl_obj->keyword->gid)
            {
            case CLERI_GID_K_TRUE:
                (*condition)->int64 = 1; break;
            case CLERI_GID_K_FALSE:
                (*condition)->int64 = 0; break;
            case CLERI_GID_K_INTEGER:
                (*condition)->int64 = SIRIDB_SERIES_TP_INT; break;
            case CLERI_GID_K_FLOAT:
                (*condition)->int64 = SIRIDB_SERIES_TP_DOUBLE; break;
            case CLERI_GID_K_STRING:
                (*condition)->int64 = SIRIDB_SERIES_TP_STRING; break;
            default:
                (*condition)->int64 = node->cl_obj->cl_obj->keyword->gid;
            }
            SET_CONDITION_AND_RETURN
        default:
            log_critical(
                    "Got an unexpected value type: %d", node->cl_obj->tp);
            assert (0);
        }
        break;

    case EXPECTING_NEXT:
        /* this can be 'and', 'or', or an closing curly */
        switch (node->cl_obj->tp)
        {
        case CLERI_TP_KEYWORD:
            switch (node->cl_obj->cl_obj->keyword->gid)
            {
            case CLERI_GID_K_AND:
                cexpr = CEXPR_push_and(cexpr);
                break;
            case CLERI_GID_K_OR:
                cexpr = CEXPR_push_or(cexpr);
                break;
            default:
                log_critical(
                    "Only 'and' or 'or' keywords are expected, got type: %ld",
                    node->cl_obj->cl_obj->keyword->gid);
                assert (0);
            }
            *condition = CEXPR_condition_new();
            (*expecting) = EXPECTING_PROP;
            return cexpr;

        case CLERI_TP_TOKEN:
            return CEXPR_close_curly(cexpr, list);

        default:
            log_critical(
                    "Only and/or/closing curly are expected. got type: %d",
                    node->cl_obj->tp);
            assert (0);
        }
        break;
    }

    cleri_children_t * current = node->children;

    while (current != NULL && current->node != NULL)
    {
        cexpr = CEXPR_walk_node(
                current->node,
                cexpr,
                list,
                condition,
                expecting);
        if (cexpr == NULL)
        {
            return NULL;
        }
        current = current->next;
    }

    return cexpr;
}


static cexpr_t * CEXPR_new(void)
{
    cexpr_t * cexpr = (cexpr_t *) malloc(sizeof(cexpr_t));
    cexpr->operator = CEXPR_AND;
    cexpr->tp_a = VIA_NULL;
    cexpr->tp_b = VIA_NULL;
    cexpr->via_a.cexpr = NULL;
    return cexpr;
}

static cexpr_condition_t * CEXPR_condition_new(void)
{
    cexpr_condition_t * condition =
            (cexpr_condition_t *) malloc(sizeof(cexpr_condition_t));
    condition->int64 = 0;
    condition->str = NULL;
    return condition;
}

static void CEXPR_condition_free(cexpr_condition_t * cond)
{
    log_debug("Free condition!");
    free(cond->str);
    free(cond);
}

static cexpr_t * CEXPR_push_and(cexpr_t * cexpr)
{
    if (cexpr->tp_b == VIA_NULL)
    {
        return cexpr;
    }
    cexpr_t * new_cexpr = CEXPR_new();
    new_cexpr->tp_a = cexpr->tp_b;
    new_cexpr->via_a = cexpr->via_b;
    cexpr->tp_b = VIA_CEXPR;
    cexpr->via_b.cexpr = new_cexpr;
    return new_cexpr;
}

static cexpr_t * CEXPR_push_or(cexpr_t * cexpr)
{
    if (cexpr->tp_b == VIA_NULL)
    {
        return cexpr;
    }
    cexpr_t * new_cexpr = CEXPR_new();
    new_cexpr->operator = CEXPR_OR;
    new_cexpr->tp_a = VIA_CEXPR;
    new_cexpr->via_a.cexpr = cexpr;
    return new_cexpr;
}

static void CEXPR_push_condition(cexpr_t * cexpr, cexpr_condition_t * cond)
{
    if (cexpr->tp_a == VIA_NULL)
    {
        cexpr->tp_a = VIA_COND;
        cexpr->via_a.cond = cond;
    }
    else
    {
#ifdef DEBUG
        assert(cexpr->tp_b == VIA_NULL);
#endif
        cexpr->tp_b = VIA_COND;
        cexpr->via_b.cond = cond;
    }
}

static cexpr_t * CEXPR_open_curly(cexpr_t * cexpr, cexpr_list_t * list)
{
    if (list->len == CEXPR_MAX_CURLY_DEPT)
    {
        /* max expression depth reached */
        cexpr_free(cexpr);
        return NULL;
    }

    /* save current cexpr in depth. */
    list->cexpr[list->len] = cexpr;

    /* create new expression */
    cexpr_t * new_cexpr = CEXPR_new();

    if (cexpr->tp_a == VIA_NULL)
    {
        cexpr->tp_a = VIA_CEXPR;
        cexpr->via_a.cexpr = new_cexpr;
    }
    else
    {
#ifdef DEBUG
        assert(cexpr->tp_b == VIA_NULL);
#endif
        cexpr->tp_b = VIA_CEXPR;
        cexpr->via_b.cexpr = new_cexpr;
    }
    list->len++;
    return new_cexpr;
}

static cexpr_t * CEXPR_close_curly(cexpr_t * cexpr, cexpr_list_t * list)
{
#ifdef DEBUG
    assert (list->len > 0);
#endif
    list->len--;
    return list->cexpr[list->len];
}
