#include <stdio.h>
#include <string.h>
#include "optional_nullable_greedy_parser.h"

typedef struct Input {
    int count;
    int position;
} Input;

static int next_token(ParserToken *token, void *context)
{
    Input *input = (Input *)context;
    memset(token, 0, sizeof *token);
    token->linha = 1;
    token->coluna = 1;
    token->type = input->position < input->count ?
                  PARSER_LITERAL_0 : PARSER_EOF;
    input->position++;
    return 0;
}

static int run(int count)
{
    Input input;
    input.count = count;
    input.position = 0;
    return parser_parse(next_token, &input);
}

int main(void)
{
    if (run(2) != 0) return 1;
    if (run(1) == 0) return 1;
    puts("opcional anulavel: escolha gulosa verificada");
    return 0;
}
