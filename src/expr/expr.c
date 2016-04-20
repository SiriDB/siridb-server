/*
 * expr.c - Parse expression
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-04-2016
 *
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


int64_t expr_parser(const char * expr, size_t len)
{
    int64_t result;
    char * pt;
    char expression[len + 1];

    /* remove spaces */
    for (pt = expression; len; len--, expr++)
    {
        if (isspace(*expr))
            continue;
        *pt = *expr;
        pt++;
    }
    /* write terminator */
    *pt = 0;

    /* reset pointer to start */
    pt = expression;

    /* get expression result */
    result = expr_expression((const char **) &pt);

    return result;
}

static int64_t expr_number(const char ** expression)
{
    int result = *(*expression)++ - '0';
    while (**expression >= '0' && **expression <= '9')
    {
        result = 10 * result + *(*expression)++ - '0';
    }
    return result;
}

static int64_t expr_factor(const char ** expression)
{
    if (**expression >= '0' && **expression <= '9')
        return expr_number(expression);
    else if (**expression == '(')
    {
        (*expression)++; // '('
        int result = expr_expression(expression);
        (*expression)++; // ')'
        return result;
    }
    else if (**expression == '-')
    {
        (*expression)++;
        return -expr_factor(expression);
    }
    return 0; // error
}

static int64_t expr_term(const char ** expression)
{
    int result = expr_factor(expression);
    int i = 1;
    while (i)
        switch (*(*expression)++)
        {
        case '*':
            result *= expr_factor(expression); break;
        case '%':
            result %= expr_factor(expression); break;
        case '/':
            result /= expr_factor(expression); break;
        default:
            i = 0;
        }
    return result;
}

static int64_t expr_expression(const char ** expression)
{
    int64_t result = expr_term(expression);
    while (**expression == '+' || **expression == '-')
        if (*(*expression)++ == '+')
            result += expr_term(expression);
        else
            result -= expr_term(expression);
    return result;
}
