#include <stdio.h>
#include <string.h>
#include "literal_dollar_parser.h"

typedef struct Input {
    int position;
} Input;

static int next_token(ParserToken *token, void *context)
{
    Input *input = (Input *)context;
    memset(token, 0, sizeof *token);
    token->type = input->position++ == 0 ? PARSER_LITERAL_0 : PARSER_EOF;
    return 0;
}

int main(void)
{
    Input input;
    input.position = 0;
    if (parser_parse(next_token, &input) != 0) return 1;
    puts("literal '$' distinguido do fim de arquivo");
    return 0;
}
