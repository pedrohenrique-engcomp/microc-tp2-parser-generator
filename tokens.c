#include "tokens.h"

const char *token_nome(TokenType tipo)
{
    static const char *nomes[] = {
        "UNDEF", "ID", "END_OF_FILE",
        "INTEGERCONST", "CHARCONST", "STRINGCONST",
        "PLUS", "MINUS", "MUL", "DIV", "MOD",
        "EQ", "NEQ", "LT", "GT", "LEQ", "GEQ", "AND", "OR", "NOT",
        "ASSIGN", "SEMICOLON", "COMMA", "LPAREN", "RPAREN",
        "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
        "MAIN", "IF", "ELSE", "FOR", "RETURN", "INT", "CHAR", "PRINT"
    };
    int quantidade = (int)(sizeof nomes / sizeof nomes[0]);

    if ((int)tipo < 0 || (int)tipo >= quantidade)
        return "TOKEN_DESCONHECIDO";
    return nomes[(int)tipo];
}
