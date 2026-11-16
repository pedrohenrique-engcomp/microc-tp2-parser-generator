#ifndef TOKENS_H
#define TOKENS_H

/* Vocabulário compartilhado pelo scanner do TP1 e pelo parser gerado. */
typedef enum {
    UNDEF = 0,
    ID,
    END_OF_FILE,
    INTEGERCONST,
    CHARCONST,
    STRINGCONST,
    PLUS,
    MINUS,
    MUL,
    DIV,
    MOD,
    EQ,
    NEQ,
    LT,
    GT,
    LEQ,
    GEQ,
    AND,
    OR,
    NOT,
    ASSIGN,
    SEMICOLON,
    COMMA,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    MAIN,
    IF,
    ELSE,
    FOR,
    RETURN,
    INT,
    CHAR,
    PRINT
} TokenType;

#define TOKEN_LEXEME_MAX 4096
#define TOKEN_ERROR_MAX   256

/* Token transporta tipo, lexema, mensagem léxica e posição de origem. */
typedef struct Token {
    TokenType tipo;
    char lexema[TOKEN_LEXEME_MAX];
    char mensagem[TOKEN_ERROR_MAX];
    int linha;
    int coluna;
} Token;

const char *token_nome(TokenType tipo);

#endif
