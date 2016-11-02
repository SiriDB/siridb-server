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
#include <siri/db/shard.h>
#include <siri/db/access.h>
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
static cexpr_t * CEXPR_push_or(cexpr_t * cexpr, cexpr_list_t * list);
static cexpr_t * CEXPR_open_curly(cexpr_t * cexpr, cexpr_list_t * list);
static cexpr_t * CEXPR_close_curly(cexpr_list_t * list);

static cexpr_t * CEXPR_walk_node(
        cleri_node_t * node,
        cexpr_t * cexpr,
        cexpr_list_t * list,
        cexpr_condition_t ** condition,
        int * expecting);

cexpr_t * cexpr_from_node(cleri_node_t * node)
{
	cexpr_t * tmp;
	cexpr_t * cexpr = CEXPR_new();
    if (cexpr == NULL)
    {
    	return NULL;
    }

    int expecting = EXPECTING_PROP;
    cexpr_condition_t * condition = CEXPR_condition_new();

    if (condition == NULL)
    {
    	free(cexpr);
    	return NULL;
    }

    /* create a list, we only need this list while building an expression */
    cexpr_list_t list;
    list.len = 0;

    /* by simply wrapping the expression between curly brackets we make sure
     * we start validating at the correct start.
     */
    tmp = CEXPR_open_curly(cexpr, &list);

    if (tmp == NULL)
    {
    	free(cexpr);
    	free(condition);
    	return NULL;
    }

    /* build the expression */
    cexpr = CEXPR_walk_node(node, tmp, &list, &condition, &expecting);

    /* test if successful */
    if (cexpr != NULL)
    {
        /* successful */
        cexpr = CEXPR_close_curly(&list);
#ifdef DEBUG
        assert (list.len == 0 && condition == NULL);
#endif
    }
    else if (condition != NULL)
    {
        /* CEXPR_MAX_CURLY_DEPT, logging is done by the listener */
        cexpr_free(list.cexpr[0]);
        CEXPR_condition_free(condition);
    }

    return cexpr;
}

/*
 * Returns 0 or 1 (false or true).
 */
int cexpr_int_cmp(
        const cexpr_operator_t operator,
        const int64_t a,
        const int64_t b)
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
        log_critical("Got an unexpected operator (int type): %d", operator);
        assert (0);
    }
    /* we should NEVER get here */
    return -1;
}

/*
 * Returns 0 or 1 (false or true).
 */
int cexpr_double_cmp(
        const cexpr_operator_t operator,
        const double a,
        const double b)
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
        log_critical("Got an unexpected operator (int type): %d", operator);
        assert (0);
    }
    /* we should NEVER get here */
    return -1;
}

/*
 * Returns 0 or 1 (false or true).
 */
int cexpr_str_cmp(
        const cexpr_operator_t operator,
        const char * a,
        const char * b)
{
    /* both a and b MUST be terminated strings */

#ifdef DEBUG
    assert (a != NULL && b != NULL);
#endif

    switch (operator)
    {
    case CEXPR_EQ:
        return strcmp(a, b) == 0;
    case CEXPR_NE:
        return strcmp(a, b) != 0;
    case CEXPR_GT:
        return strcmp(a, b) > 0;
    case CEXPR_LT:
        return strcmp(a, b) < 0;
    case CEXPR_GE:
        return strcmp(a, b) >= 0;
    case CEXPR_LE:
        return strcmp(a, b) <= 0;
    case CEXPR_IN:
        return strstr(a, b) != NULL;
    case CEXPR_NI:
        return strstr(a, b) == NULL;
    default:
        log_critical("Got an unexpected operator (string type): %d", operator);
        assert (0);
    }
    /* we should NEVER get here */
    return -1;
}

/*
 * Returns 0 or 1 (false or true).
 */
int cexpr_bool_cmp(
        const cexpr_operator_t operator,
        const int64_t a,
        const int64_t b)
{
#ifdef DEBUG
    assert ((a == 0 || a == 1) && (b == 0 || b == 1));
#endif
    switch (operator)
    {
    case CEXPR_EQ:
        return a == b;
    case CEXPR_NE:
        return a != b;
    default:
        log_critical("Got an unexpected operator (boolean type): %d", operator);
        assert (0);
    }
    /* we should NEVER get here */
    return -1;
}

/*
 * Returns 0 or 1 (false or true).
 */
int cexpr_run(cexpr_t * cexpr, cexpr_cb_t cb, void * obj)
{
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

int cexpr_contains(cexpr_t * cexpr, cexpr_cb_prop_t cb)
{
    /* should return either 1 or 0. (true or false) */

    if (cexpr->tp_a == VIA_CEXPR && cexpr_contains(cexpr->via_a.cexpr, cb))
    {
        return 1;
    }
    if (cexpr->tp_b == VIA_CEXPR && cexpr_contains(cexpr->via_b.cexpr, cb))
    {
        return 1;
    }
    if (cexpr->tp_a == VIA_COND && cb(cexpr->via_a.cond->prop))
    {
        return 1;
    }
    if (cexpr->tp_b == VIA_COND && cb(cexpr->via_b.cond->prop))
    {
        return 1;
    }
    return 0;
}

void cexpr_free(cexpr_t * cexpr)
{
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

/*
 * Get operator from cleri node
 */
cexpr_operator_t cexpr_operator_fn(cleri_node_t * node)
{
    if (node->len == 1)
    {
        switch (*node->str)
        {
        case '>': return CEXPR_GT;
        case '<': return CEXPR_LT;
        case '~': return CEXPR_IN;
        }
    }
    else
    {
#ifdef DEBUG
        assert (node->len == 2);
#endif
        switch (*node->str)
        {
        case '=': return CEXPR_EQ;
        case '!': return (*(node->str + 1) == '=') ?
                    CEXPR_NE : CEXPR_NI;
        case '>': return CEXPR_GE;
        case '<': return CEXPR_LE;
        }
    }

    assert (0);
    return 0;
}

/*
 * Return NULL in case of an error. (this can be when max_curly depth is
 * reached or when a memory allocation error has occurred.
 */
static cexpr_t * CEXPR_walk_node(
        cleri_node_t * node,
        cexpr_t * cexpr,
        cexpr_list_t * list,
        cexpr_condition_t ** condition,
        int * expecting)
{
	cexpr_condition_t * tmp_condition;
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
            (*condition)->prop = node->cl_obj->via.keyword->gid;
            (*expecting) = EXPECTING_OPERATOR;
            return cexpr;

        default:
            /* this is probably a CHOICE */
            break;
        }
        /* its fine to get here, not all cases are captured */
        break;

    case EXPECTING_OPERATOR:
        switch (node->cl_obj->tp)
        {
        case CLERI_TP_TOKENS:
            /* this one of the following operators:
             *      ==  <  <=  >  >=  !=  ~  !~
             */
            (*condition)->operator = cexpr_operator_fn(node);
            (*expecting) = EXPECTING_VAL;
            return cexpr;

        default:
            log_critical(
                    "Got an unexpected operator type: %d", node->cl_obj->tp);
        }
        assert (0);
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
            if (node->cl_obj->via.choice->gid == CLERI_GID_STRING)
            {
                (*condition)->str = (char *) malloc(node->len -1);
                if ((*condition)->str == NULL)
                {
                	return NULL;
                }
                strx_extract_string((*condition)->str, node->str, node->len);
                SET_CONDITION_AND_RETURN
            }
            /* can be a choice between keywords, in that case just wait */
            break;
        case CLERI_TP_KEYWORD:
            /* for some keywords we can do some work to speed up checks */
            switch (node->cl_obj->via.keyword->gid)
            {
            /* map boolean types */
            case CLERI_GID_K_TRUE:
                (*condition)->int64 = 1; break;
            case CLERI_GID_K_FALSE:
                (*condition)->int64 = 0; break;

            /* map series types */
            case CLERI_GID_K_INTEGER:
                (*condition)->int64 = TP_INT; break;
            case CLERI_GID_K_FLOAT:
                (*condition)->int64 = TP_DOUBLE; break;
            case CLERI_GID_K_STRING:
                (*condition)->int64 = TP_STRING; break;

            /* map shard types */
            case CLERI_GID_K_NUMBER:
                (*condition)->int64 = SIRIDB_SHARD_TP_NUMBER; break;
            case CLERI_GID_K_LOG:
                (*condition)->int64 = SIRIDB_SHARD_TP_LOG; break;

            /* map access types */
            case CLERI_GID_K_SHOW:
                (*condition)->int64 = SIRIDB_ACCESS_SHOW; break;
            case CLERI_GID_K_COUNT:
                (*condition)->int64 = SIRIDB_ACCESS_COUNT; break;
            case CLERI_GID_K_LIST:
                (*condition)->int64 = SIRIDB_ACCESS_LIST; break;
            case CLERI_GID_K_SELECT:
                (*condition)->int64 = SIRIDB_ACCESS_SELECT; break;
            case CLERI_GID_K_INSERT:
                (*condition)->int64 = SIRIDB_ACCESS_INSERT; break;
            case CLERI_GID_K_CREATE:
                (*condition)->int64 = SIRIDB_ACCESS_CREATE; break;
            case CLERI_GID_K_ALTER:
                (*condition)->int64 = SIRIDB_ACCESS_ALTER; break;
            case CLERI_GID_K_DROP:
                (*condition)->int64 = SIRIDB_ACCESS_DROP; break;
            case CLERI_GID_K_GRANT:
                (*condition)->int64 = SIRIDB_ACCESS_GRANT; break;
            case CLERI_GID_K_REVOKE:
                (*condition)->int64 = SIRIDB_ACCESS_REVOKE; break;

            /* map access profiles */
            case CLERI_GID_K_READ:
                (*condition)->int64 = SIRIDB_ACCESS_PROFILE_READ; break;
            case CLERI_GID_K_WRITE:
                (*condition)->int64 = SIRIDB_ACCESS_PROFILE_WRITE; break;
            case CLERI_GID_K_MODIFY:
                (*condition)->int64 = SIRIDB_ACCESS_PROFILE_MODIFY; break;
            case CLERI_GID_K_FULL:
                (*condition)->int64 = SIRIDB_ACCESS_PROFILE_FULL; break;

            /* map log levels */
            case CLERI_GID_K_DEBUG:
                (*condition)->int64 = LOGGER_DEBUG; break;
            case CLERI_GID_K_INFO:
                (*condition)->int64 = LOGGER_INFO; break;
            case CLERI_GID_K_WARNING:
                (*condition)->int64 = LOGGER_WARNING; break;
            case CLERI_GID_K_ERROR:
                (*condition)->int64 = LOGGER_ERROR; break;
            case CLERI_GID_K_CRITICAL:
                (*condition)->int64 = LOGGER_CRITICAL; break;

            default:
                (*condition)->int64 = node->cl_obj->via.keyword->gid;
            }
            SET_CONDITION_AND_RETURN
        default:
            log_critical(
                    "Got an unexpected value type: %d", node->cl_obj->tp);
            assert (0);
        }
        /* we allow to get here, not all cases return */
        break;

    case EXPECTING_NEXT:
        /* this can be 'and', 'or', or an closing curly */
        switch (node->cl_obj->tp)
        {
        case CLERI_TP_KEYWORD:
            switch (node->cl_obj->via.keyword->gid)
            {
            case CLERI_GID_K_AND:
                cexpr = CEXPR_push_and(cexpr);
                if (cexpr == NULL)
                {
                	return NULL;
                }
                break;
            case CLERI_GID_K_OR:
                cexpr = CEXPR_push_or(cexpr, list);
                if (cexpr == NULL)
                {
                	return NULL;
                }
                break;
            default:
                log_critical(
                    "Only 'and' or 'or' keywords are expected, got type: %lu",
                    node->cl_obj->via.keyword->gid);
                assert (0);
            }
            tmp_condition = CEXPR_condition_new();
            if (tmp_condition == NULL)
            {
            	return NULL;
            }
            *condition = tmp_condition;
            *expecting = EXPECTING_PROP;
            return cexpr;

        case CLERI_TP_TOKEN:
            return CEXPR_close_curly(list);

        default:
            log_critical(
                    "Only and/or/closing curly are expected. got type: %d",
                    node->cl_obj->tp);
        }
        /* we must NEVER get here */
        assert (0);
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

/*
 * Returns NULL in case of an error
 */
static cexpr_t * CEXPR_new(void)
{
    cexpr_t * cexpr = (cexpr_t *) malloc(sizeof(cexpr_t));
    if (cexpr != NULL)
    {
		cexpr->operator = CEXPR_AND;
		cexpr->tp_a = VIA_NULL;
		cexpr->tp_b = VIA_NULL;
		cexpr->via_a.cexpr = NULL;
    }
    return cexpr;
}

/*
 * Returns NULL in case of an error
 */
static cexpr_condition_t * CEXPR_condition_new(void)
{
    cexpr_condition_t * condition =
            (cexpr_condition_t *) malloc(sizeof(cexpr_condition_t));

    if (condition != NULL)
    {
		condition->int64 = 0;
		condition->str = NULL;
    }

    return condition;
}

static void CEXPR_condition_free(cexpr_condition_t * cond)
{
    free(cond->str);
    free(cond);
}

/*
 * Returns NULL in case or an error
 */
static cexpr_t * CEXPR_push_and(cexpr_t * cexpr)
{
    if (cexpr->tp_b == VIA_NULL)
    {
        return cexpr;
    }
    cexpr_t * new_cexpr = CEXPR_new();

    if (new_cexpr != NULL)
    {
		new_cexpr->tp_a = cexpr->tp_b;
		new_cexpr->via_a = cexpr->via_b;
		cexpr->tp_b = VIA_CEXPR;
		cexpr->via_b.cexpr = new_cexpr;
    }
    return new_cexpr;
}

/*
 * Returns NULL in case or an error
 */
static cexpr_t * CEXPR_push_or(cexpr_t * cexpr, cexpr_list_t * list)
{
    if (cexpr->tp_b == VIA_NULL)
    {
        /* this can happen only at the first condition */
        cexpr->operator = CEXPR_OR;
        return cexpr;
    }

    cexpr_t * new_cexpr = CEXPR_new();

    if (new_cexpr != NULL)
    {
		size_t selected = list->len - 1;
		cexpr = list->cexpr[selected];


		new_cexpr->operator = CEXPR_OR;
		new_cexpr->tp_a = VIA_CEXPR;
		new_cexpr->via_a.cexpr = cexpr;

		list->cexpr[selected] = new_cexpr;
    }

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
    if (list->len == CEXPR_MAX_CURLY_DEPTH)
    {
        /* max expression depth reached */
        return NULL;
    }

    /* create new expression */
    cexpr_t * new_cexpr = CEXPR_new();

    if (new_cexpr == NULL)
    {
    	/* memory allocation error */
    	return NULL;
    }


    /* save current cexpr in depth. */
    list->cexpr[list->len] = cexpr;

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

static cexpr_t * CEXPR_close_curly(cexpr_list_t * list)
{
#ifdef DEBUG
    assert (list->len > 0);
#endif
    list->len--;
    return list->cexpr[list->len];
}
