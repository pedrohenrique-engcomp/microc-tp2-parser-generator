#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "lista_parser.h"

typedef struct Input {
    const char *text;
    size_t pos;
    int line;
    int col;
} Input;

static void step(Input *in)
{
    if (in->text[in->pos] == '\n') { ++in->line; in->col = 1; }
    else ++in->col;
    ++in->pos;
}

static int next_token(ParserToken *tok, void *context)
{
    Input *in = (Input *)context;
    size_t start, len;
    char c;
    memset(tok, 0, sizeof *tok);
    while ((c = in->text[in->pos]) != '\0' && isspace((unsigned char)c)) step(in);
    tok->linha = in->line; tok->coluna = in->col;
    c = in->text[in->pos];
    if (c == '\0') { tok->type = PARSER_EOF; return 0; }
    if (isalpha((unsigned char)c) || c == '_') {
        start = in->pos;
        while (isalnum((unsigned char)in->text[in->pos]) || in->text[in->pos] == '_') step(in);
        len = in->pos - start;
        if (len >= sizeof tok->lexema) len = sizeof tok->lexema - 1;
        memcpy(tok->lexema, in->text + start, len); tok->lexema[len] = '\0';
        tok->type = PARSER_T_ID; return 0;
    }
    if (c == ',') {
        tok->type = PARSER_LITERAL_1; tok->lexema[0] = ','; tok->lexema[1] = '\0'; step(in); return 0;
    }
    tok->type = -1; strcpy(tok->mensagem, "caractere invalido"); return 0;
}

static int check(const char *text, int accepted)
{
    Input in;
    int result;
    in.text = text; in.pos = 0; in.line = 1; in.col = 1;
    result = parser_parse(next_token, &in);
    if ((result == 0) != accepted) {
        fprintf(stderr, "Resultado inesperado para '%s': %d\n", text, result);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += check("alpha", 1);
    failures += check("alpha, beta", 1);
    failures += check("a_1,b2,gamma_3", 1);
    failures += check("", 0);
    failures += check(",alpha", 0);
    failures += check("alpha,", 0);
    if (failures) return 1;
    puts("Parser lista: 6 casos passaram (3 aceitos, 3 rejeitados).");
    return 0;
}
