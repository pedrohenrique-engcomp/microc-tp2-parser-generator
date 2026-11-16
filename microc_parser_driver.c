#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "scanner.h"

static int adapt_microc_token(ParserToken *destination, void *context)
{
    Token source;
    (void)context;
    (void)scanner_next(&source);
    memset(destination, 0, sizeof *destination);
    destination->linha = source.linha;
    destination->coluna = source.coluna;
    strcpy(destination->lexema, source.lexema);
    switch (source.tipo) {
    case END_OF_FILE: destination->type = PARSER_EOF; break;
    case ID: destination->type = PARSER_T_ID; break;
    case INTEGERCONST: destination->type = PARSER_T_INTEGERCONST; break;
    case CHARCONST: destination->type = PARSER_T_CHARCONST; break;
    case STRINGCONST: destination->type = PARSER_T_STRINGCONST; break;
    case PLUS: destination->type = PARSER_T_PLUS; break;
    case MINUS: destination->type = PARSER_T_MINUS; break;
    case MUL: destination->type = PARSER_T_MUL; break;
    case DIV: destination->type = PARSER_T_DIV; break;
    case MOD: destination->type = PARSER_T_MOD; break;
    case EQ: destination->type = PARSER_T_EQ; break;
    case NEQ: destination->type = PARSER_T_NEQ; break;
    case LT: destination->type = PARSER_T_LT; break;
    case GT: destination->type = PARSER_T_GT; break;
    case LEQ: destination->type = PARSER_T_LEQ; break;
    case GEQ: destination->type = PARSER_T_GEQ; break;
    case AND: destination->type = PARSER_T_AND; break;
    case OR: destination->type = PARSER_T_OR; break;
    case NOT: destination->type = PARSER_T_NOT; break;
    case ASSIGN: destination->type = PARSER_T_ASSIGN; break;
    case SEMICOLON: destination->type = PARSER_T_SEMICOLON; break;
    case COMMA: destination->type = PARSER_T_COMMA; break;
    case LPAREN: destination->type = PARSER_T_LPAREN; break;
    case RPAREN: destination->type = PARSER_T_RPAREN; break;
    case LBRACE: destination->type = PARSER_T_LBRACE; break;
    case RBRACE: destination->type = PARSER_T_RBRACE; break;
    case LBRACKET: destination->type = PARSER_T_LBRACKET; break;
    case RBRACKET: destination->type = PARSER_T_RBRACKET; break;
    case MAIN: destination->type = PARSER_T_MAIN; break;
    case IF: destination->type = PARSER_T_IF; break;
    case ELSE: destination->type = PARSER_T_ELSE; break;
    case FOR: destination->type = PARSER_T_FOR; break;
    case RETURN: destination->type = PARSER_T_RETURN; break;
    case INT: destination->type = PARSER_T_INT; break;
    case CHAR: destination->type = PARSER_T_CHAR; break;
    case PRINT: destination->type = PARSER_T_PRINT; break;
    case UNDEF:
    default:
        destination->type = -1;
        strcpy(destination->mensagem, source.mensagem[0] != '\0' ?
               source.mensagem : "token lexico indefinido");
        break;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    if (argc != 2) {
        fprintf(stderr, "Uso: %s arquivo.mc\n", argv[0]);
        return 2;
    }
    if (scanner_init(argv[1]) != 0) {
        fprintf(stderr, "erro: nao foi possivel abrir %s\n", argv[1]);
        return 2;
    }
    result = parser_parse(adapt_microc_token, NULL);
    scanner_close();
    if (result == 0) {
        puts("Analise sintatica concluida: programa Micro C valido.");
        return 0;
    }
    return 1;
}
