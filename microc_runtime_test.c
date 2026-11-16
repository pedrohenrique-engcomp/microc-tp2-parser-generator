#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "microc_parser.h"

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

static int keyword(const char *s)
{
    if (strcmp(s, "int") == 0) return PARSER_T_INT;
    if (strcmp(s, "char") == 0) return PARSER_T_CHAR;
    if (strcmp(s, "main") == 0) return PARSER_T_MAIN;
    if (strcmp(s, "if") == 0) return PARSER_T_IF;
    if (strcmp(s, "else") == 0) return PARSER_T_ELSE;
    if (strcmp(s, "for") == 0) return PARSER_T_FOR;
    if (strcmp(s, "return") == 0) return PARSER_T_RETURN;
    if (strcmp(s, "print") == 0) return PARSER_T_PRINT;
    return PARSER_T_ID;
}

static int next_token(ParserToken *tok, void *context)
{
    Input *in = (Input *)context;
    size_t start, len;
    char c, two[3];
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
        tok->type = keyword(tok->lexema); return 0;
    }
    if (isdigit((unsigned char)c)) {
        start = in->pos;
        while (isdigit((unsigned char)in->text[in->pos])) step(in);
        len = in->pos - start;
        if (len >= sizeof tok->lexema) len = sizeof tok->lexema - 1;
        memcpy(tok->lexema, in->text + start, len); tok->lexema[len] = '\0';
        tok->type = PARSER_T_INTEGERCONST; return 0;
    }
    if (c == '"' || c == '\'') {
        char quote = c;
        start = in->pos; step(in);
        while (in->text[in->pos] != '\0' && in->text[in->pos] != quote) {
            if (in->text[in->pos] == '\\' && in->text[in->pos + 1] != '\0') step(in);
            step(in);
        }
        if (in->text[in->pos] == quote) step(in);
        len = in->pos - start;
        if (len >= sizeof tok->lexema) len = sizeof tok->lexema - 1;
        memcpy(tok->lexema, in->text + start, len); tok->lexema[len] = '\0';
        tok->type = quote == '"' ? PARSER_T_STRINGCONST : PARSER_T_CHARCONST;
        return 0;
    }
    two[0] = c; two[1] = in->text[in->pos + 1]; two[2] = '\0';
    if (strcmp(two, "==") == 0) { tok->type = PARSER_T_EQ; step(in); step(in); strcpy(tok->lexema, two); return 0; }
    if (strcmp(two, "!=") == 0) { tok->type = PARSER_T_NEQ; step(in); step(in); strcpy(tok->lexema, two); return 0; }
    if (strcmp(two, "<=") == 0) { tok->type = PARSER_T_LEQ; step(in); step(in); strcpy(tok->lexema, two); return 0; }
    if (strcmp(two, ">=") == 0) { tok->type = PARSER_T_GEQ; step(in); step(in); strcpy(tok->lexema, two); return 0; }
    if (strcmp(two, "&&") == 0) { tok->type = PARSER_T_AND; step(in); step(in); strcpy(tok->lexema, two); return 0; }
    if (strcmp(two, "||") == 0) { tok->type = PARSER_T_OR; step(in); step(in); strcpy(tok->lexema, two); return 0; }
    tok->lexema[0] = c; tok->lexema[1] = '\0'; step(in);
    switch (c) {
    case '+': tok->type = PARSER_T_PLUS; break;
    case '-': tok->type = PARSER_T_MINUS; break;
    case '*': tok->type = PARSER_T_MUL; break;
    case '/': tok->type = PARSER_T_DIV; break;
    case '%': tok->type = PARSER_T_MOD; break;
    case '<': tok->type = PARSER_T_LT; break;
    case '>': tok->type = PARSER_T_GT; break;
    case '!': tok->type = PARSER_T_NOT; break;
    case '=': tok->type = PARSER_T_ASSIGN; break;
    case ';': tok->type = PARSER_T_SEMICOLON; break;
    case ',': tok->type = PARSER_T_COMMA; break;
    case '(': tok->type = PARSER_T_LPAREN; break;
    case ')': tok->type = PARSER_T_RPAREN; break;
    case '{': tok->type = PARSER_T_LBRACE; break;
    case '}': tok->type = PARSER_T_RBRACE; break;
    case '[': tok->type = PARSER_T_LBRACKET; break;
    case ']': tok->type = PARSER_T_RBRACKET; break;
    default: tok->type = -1; strcpy(tok->mensagem, "caractere invalido"); break;
    }
    return 0;
}

static int check(const char *text, int accepted)
{
    Input in;
    int result;
    in.text = text; in.pos = 0; in.line = 1; in.col = 1;
    result = parser_parse(next_token, &in);
    if ((result == 0) != accepted) {
        fprintf(stderr, "Resultado inesperado para Micro C '%s': %d\n", text, result);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += check("int main() { int x; x = 1; }", 1);
    failures += check("int main() { char c; print(\"ok\"); return; }", 1);
    failures += check("int main() { int x; if (x < 3) x = x + 1; else x = 0; }", 1);
    failures += check("int main( { }", 0);
    failures += check("int main() { int ; }", 0);
    failures += check("int main() { x = ; }", 0);
    if (failures) return 1;
    puts("Parser Micro C: 6 casos passaram (3 aceitos, 3 rejeitados).");
    return 0;
}
