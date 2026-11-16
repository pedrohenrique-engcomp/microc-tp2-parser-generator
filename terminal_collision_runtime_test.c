#include <stdio.h>
#include <string.h>
#include "terminal_collision_parser.h"

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

static int check(const int *types, int count)
{
    Input input;
    input.types = types;
    input.count = count;
    input.position = 0;
    return parser_parse(next_token, &input) == 0;
}

int main(void)
{
    const int named_token[] = {PARSER_T_ID, PARSER_EOF};
    const int literal_token[] = {PARSER_LITERAL_1, PARSER_EOF};
    if (!check(named_token, 2)) return 1;
    if (!check(literal_token, 2)) return 1;
    puts("colisao entre ID nomeado e literal ID resolvida");
    return 0;
}
