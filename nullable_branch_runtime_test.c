#include <stdio.h>
#include <string.h>
#include "nullable_branch_parser.h"

typedef struct Input {
    const int *types;
    int count;
    int position;
} Input;

static int next_token(ParserToken *token, void *context)
{
    Input *input = (Input *)context;
    memset(token, 0, sizeof *token);
    if (input->position >= input->count) {
        token->type = PARSER_EOF;
    } else {
        token->type = input->types[input->position];
    }
    input->position++;
    return 0;
}

static int run_case(const int *types, int count, int expected)
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
    const int with_y[] = {PARSER_T_Y, PARSER_T_Z, PARSER_EOF};
    const int without_y[] = {PARSER_T_Z, PARSER_EOF};
    const int with_x[] = {PARSER_T_X, PARSER_T_Z, PARSER_EOF};
    const int missing_z[] = {PARSER_T_X, PARSER_EOF};

    if (!run_case(with_y, 3, 0)) return 1;
    if (!run_case(without_y, 2, 0)) return 1;
    if (!run_case(with_x, 3, 0)) return 1;
    if (!run_case(missing_z, 2, 1)) return 1;
    puts("4 casos da alternativa anulavel passaram");
    return 0;
}
