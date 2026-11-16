#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "parser.h"

typedef struct TestInput {
    const char *text;
    size_t position;
    int line;
    int column;
} TestInput;

static void advance_char(TestInput *input)
{
    if (input->text[input->position] == '\n') {
        input->line++;
        input->column = 1;
    } else {
        input->column++;
    }
    input->position++;
}

static int test_next(ParserToken *token, void *context)
{
    TestInput *input = (TestInput *)context;
    size_t start;
    size_t length;
    char c;
    memset(token, 0, sizeof *token);
    while ((c = input->text[input->position]) != '\0' && isspace((unsigned char)c))
        advance_char(input);
    token->linha = input->line;
    token->coluna = input->column;
    c = input->text[input->position];
    if (c == '\0') {
        token->type = PARSER_EOF;
        return 0;
    }
    if (isdigit((unsigned char)c)) {
        start = input->position;
        while (isdigit((unsigned char)input->text[input->position])) advance_char(input);
        length = input->position - start;
        memcpy(token->lexema, input->text + start, length);
        token->lexema[length] = '\0';
        token->type = PARSER_T_INTEGERCONST;
        return 0;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        start = input->position;
        while (isalnum((unsigned char)input->text[input->position]) ||
               input->text[input->position] == '_') advance_char(input);
        length = input->position - start;
        memcpy(token->lexema, input->text + start, length);
        token->lexema[length] = '\0';
        token->type = PARSER_T_ID;
        return 0;
    }
    token->lexema[0] = c;
    token->lexema[1] = '\0';
    advance_char(input);
    switch (c) {
    case '+': token->type = PARSER_T_PLUS; break;
    case '-': token->type = PARSER_T_MINUS; break;
    case '*': token->type = PARSER_T_MUL; break;
    case '/': token->type = PARSER_T_DIV; break;
    case '(': token->type = PARSER_T_LPAREN; break;
    case ')': token->type = PARSER_T_RPAREN; break;
    default:
        token->type = -1;
        strcpy(token->mensagem, "caractere invalido no scanner de teste");
        break;
    }
    return 0;
}

static int run_case(const char *text, int should_accept)
{
    TestInput input;
    int result;
    input.text = text;
    input.position = 0;
    input.line = 1;
    input.column = 1;
    result = parser_parse(test_next, &input);
    if ((result == 0) != should_accept) {
        fprintf(stderr, "Caso inesperado: '%s' (resultado=%d, esperado=%d)\n",
                text, result, should_accept);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += run_case("2 * x", 1);
    failures += run_case("(12 + 3) / abc - 4", 1);
    failures += run_case("identificador_2", 1);
    failures += run_case("2 + * 5", 0);
    failures += run_case("(2 + 3", 0);
    failures += run_case("2 + 3 x", 0);
    if (failures != 0) return 1;
    puts("Parser gerado: 6 casos de execucao passaram (3 aceitos, 3 rejeitados).");
    return 0;
}
