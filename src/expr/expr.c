/*
 * expr.c - For parsing expressions.
 */
#include <stdio.h>
#include <ctype.h>
#include <expr/expr.h>
#include <stdlib.h>

static int64_t expr_expression(const char ** expression);
static int64_t expr_number(const char ** expression);
static int64_t expr_factor(const char ** expression);
static int64_t expr_term(const char ** expression);
static int64_t expr_expression(const char ** expression);

static int expr_err = 0;

int expr_parse(int64_t * result, const char * expr)
{
    expr_err = 0;
    *result = expr_expression(&expr);
    return (expr_err) ? expr_err : 0;
}

static int64_t expr_number(const char ** expression)
{
    int64_t result = *(*expression)++ - '0';
    while (**expression >= '0' && **expression <= '9')
    {
        result = 10 * result + *(*expression)++ - '0';
    }

    return result;
}

static int64_t expr_factor(const char ** expression)
{
    if (**expression >= '0' && **expression <= '9')
    {
        return expr_number(expression);
    }
    else if (**expression == '(')
    {
        (*expression)++; /* '('  */
        int64_t result = expr_expression(expression);
        (*expression)++; /* ')'  */
        return result;
    }
    else if (**expression == '-')
    {
        (*expression)++;
        return -expr_factor(expression);
    }
    return 0; /* error  */
}

static int64_t expr_term(const char ** expression)
{
    int64_t result = expr_factor(expression);
    int64_t temp;
    int i;

    for (i = 1; i;)
        switch (*(*expression))
        {
        case '*':
            (*expression)++;
            result *= expr_factor(expression);
            break;
        case '%':
            (*expression)++;
            result %= ((temp = expr_factor(expression))) ?
                    temp : (expr_err = EXPR_MODULO_BY_ZERO);
            break;
        case '/':
            (*expression)++;
            result /= ((temp = expr_factor(expression))) ?
                    temp : (expr_err = EXPR_DIVISION_BY_ZERO);
            break;
        default:
            i = 0;
        }
    return result;
}

static int64_t expr_expression(const char ** expression)
{
    int64_t result = expr_term(expression);
    while (**expression == '+' || **expression == '-')
    {
        if (*(*expression)++ == '+')
        {
            result += expr_term(expression);
        }
        else
        {
            result -= expr_term(expression);
        }
    }
    return result;
}
