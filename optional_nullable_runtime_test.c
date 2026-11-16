#include <stdio.h>
#include <string.h>
#include "optional_nullable_parser.h"

typedef struct Input {
    const int *types;
    int count;
    int position;
} Input;

static int next_token(ParserToken *token, void *context)
{
    Input *input = (Input *)context;
    memset(token, 0, sizeof *token);
    if (input->position >= input->count)
        token->type = PARSER_EOF;
    else
        token->type = input->types[input->position];
    input->position++;
    return 0;
}

static int check(const int *types, int count, int expected)
{
    Input input;
    int result;
    input.types = types;
    input.count = count;
    input.position = 0;
    result = parser_parse(next_token, &input);
    return result == expected;
}

int main(void)
{
    const int with_optional[] = {PARSER_LITERAL_0, PARSER_LITERAL_1, PARSER_EOF};
    const int without_optional[] = {PARSER_LITERAL_1, PARSER_EOF};
    const int missing_suffix[] = {PARSER_LITERAL_0, PARSER_EOF};

    if (!check(with_optional, 3, 0)) return 1;
    if (!check(without_optional, 2, 0)) return 1;
    if (!check(missing_suffix, 2, 1)) return 1;
    puts("3 casos de opcional anulavel passaram");
    return 0;
}
