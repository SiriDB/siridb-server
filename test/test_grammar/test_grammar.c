#include "../test.h"
#include <siri/grammar/grammar.h>
#include <siri/grammar/gramp.h>

#define assert_valid(__g, __q)(_assert(_is_valid(__g, __q)))
#define assert_invalid(__g, __q)(_assert(!_is_valid(__g, __q)))

static int _is_valid(cleri_grammar_t * grammar, char * query)
{
    cleri_parse_t * pr = cleri_parse(grammar, query);
    _assert (pr);
    int is_valid = pr->is_valid;
    cleri_parse_free(pr);
    return is_valid;
}

int main()
{
    test_start("grammar");

    cleri_grammar_t * grammar = compile_grammar();

    assert_invalid(grammar, "select * from");
    assert_invalid(grammar, "list");

    assert_valid(grammar, "");
    assert_valid(grammar, "now - 1w");
    assert_valid(grammar, "help # with comments");
    assert_valid(grammar, "select * from *");
    assert_valid(grammar, "select * from 'series'");
    assert_valid(grammar, "select * from * after now-1d");
    assert_valid(grammar, "list series");
    assert_valid(grammar,
        "select mean(1h + 1m) from \"series-001\", \"series-002\", "
        "\"series-003\" between 1360152000 and 1360152000 + 1d merge as "
        "\"series\" using mean(1)");

    cleri_grammar_free(grammar);

    return test_end();
}
