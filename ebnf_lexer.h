#ifndef EBNF_LEXER_H
#define EBNF_LEXER_H

#include <stddef.h>
#include "grammar.h"

typedef enum EbnfTokenKind {
    EBNF_EOF = 0,
    EBNF_IDENT,
    EBNF_STRING,
    EBNF_REGEX,
    EBNF_EQUAL,
    EBNF_SEMICOLON,
    EBNF_PIPE,
    EBNF_LBRACKET,
    EBNF_RBRACKET,
    EBNF_LBRACE,
    EBNF_RBRACE,
    EBNF_LPAREN,
    EBNF_RPAREN,
    EBNF_INVALID
} EbnfTokenKind;

typedef struct EbnfToken {
    EbnfTokenKind kind;
    char *text;
    SourceLoc loc;
} EbnfToken;

typedef struct EbnfLexer {
    char *source;
    size_t length;
    size_t position;
    int line;
    int column;
    const char *filename;
    char error[256];
    SourceLoc error_loc;
} EbnfLexer;

int ebnf_lexer_open(EbnfLexer *lexer, const char *filename);
void ebnf_lexer_close(EbnfLexer *lexer);
int ebnf_lexer_next(EbnfLexer *lexer, EbnfToken *token);
void ebnf_token_destroy(EbnfToken *token);
const char *ebnf_token_name(EbnfTokenKind kind);

#endif
