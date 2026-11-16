#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ebnf_parser.h"

typedef struct Parser {
    EbnfLexer lexer;
    EbnfToken current;
    Grammar *grammar;
    char error[256];
    SourceLoc error_loc;
    int failed;
} Parser;

static char *dup_text(const char *text)
{
    size_t n;
    char *copy;
    if (text == NULL) return NULL;
    n = strlen(text);
    copy = (char *)malloc(n + 1);
    if (copy != NULL) memcpy(copy, text, n + 1);
    return copy;
}

static void parser_error(Parser *parser, SourceLoc loc, const char *message)
{
    if (parser->failed) return;
    parser->failed = 1;
    parser->error_loc = loc;
    strncpy(parser->error, message, sizeof parser->error - 1);
    parser->error[sizeof parser->error - 1] = '\0';
}

static int next_token(Parser *parser)
{
    ebnf_token_destroy(&parser->current);
    if (ebnf_lexer_next(&parser->lexer, &parser->current) != 0) {
        parser_error(parser, parser->lexer.error_loc, parser->lexer.error);
        return -1;
    }
    return 0;
}

static int expect(Parser *parser, EbnfTokenKind kind, const char *message)
{
    if (parser->current.kind != kind) {
        parser_error(parser, parser->current.loc, message);
        return -1;
    }
    return next_token(parser);
}

static Expr *new_expr(ExprKind kind, SourceLoc loc)
{
    Expr *expr = (Expr *)calloc(1, sizeof *expr);
    if (expr == NULL) return NULL;
    expr->kind = kind;
    expr->loc = loc;
    return expr;
}

static int list_append(Expr *expr, Expr *child)
{
    Expr **next;
    size_t capacity;
    if (expr->as.list.count >= expr->as.list.capacity) {
        capacity = expr->as.list.capacity == 0 ? 4 : expr->as.list.capacity * 2;
        next = (Expr **)realloc(expr->as.list.items, capacity * sizeof *next);
        if (next == NULL) return -1;
        expr->as.list.items = next;
        expr->as.list.capacity = capacity;
    }
    expr->as.list.items[expr->as.list.count++] = child;
    return 0;
}

static int starts_primary(EbnfTokenKind kind)
{
    return kind == EBNF_IDENT || kind == EBNF_STRING ||
           kind == EBNF_LBRACKET || kind == EBNF_LBRACE ||
           kind == EBNF_LPAREN;
}

static Expr *parse_alternative(Parser *parser);

static Expr *parse_primary(Parser *parser)
{
    Expr *expr;
    Expr *child;
    SourceLoc loc;
    SymbolKind symbol_kind;
    char *name;
    EbnfTokenKind closing;

    loc = parser->current.loc;
    if (parser->current.kind == EBNF_IDENT ||
        parser->current.kind == EBNF_STRING) {
        name = dup_text(parser->current.text);
        if (name == NULL) {
            parser_error(parser, loc, "memoria insuficiente");
            return NULL;
        }
        if (parser->current.kind == EBNF_STRING) {
            symbol_kind = SYMBOL_LITERAL;
        } else if (isupper((unsigned char)name[0])) {
            symbol_kind = SYMBOL_TERMINAL_NAME;
        } else {
            symbol_kind = SYMBOL_NONTERMINAL;
        }
        if (next_token(parser) != 0) {
            free(name);
            return NULL;
        }
        expr = new_expr(EXPR_SYMBOL, loc);
        if (expr == NULL) {
            free(name);
            parser_error(parser, loc, "memoria insuficiente");
            return NULL;
        }
        expr->as.symbol.kind = symbol_kind;
        expr->as.symbol.name = name;
        return expr;
    }

    if (parser->current.kind == EBNF_LBRACKET ||
        parser->current.kind == EBNF_LBRACE ||
        parser->current.kind == EBNF_LPAREN) {
        if (parser->current.kind == EBNF_LBRACKET) closing = EBNF_RBRACKET;
        else if (parser->current.kind == EBNF_LBRACE) closing = EBNF_RBRACE;
        else closing = EBNF_RPAREN;
        if (next_token(parser) != 0) return NULL;
        if (!starts_primary(parser->current.kind)) {
            parser_error(parser, parser->current.loc,
                         "construcao EBNF vazia nao suportada");
            return NULL;
        }
        child = parse_alternative(parser);
        if (child == NULL) return NULL;
        if (parser->current.kind != closing) {
            expr_destroy(child);
            parser_error(parser, parser->current.loc,
                         "delimitador de fechamento ausente");
            return NULL;
        }
        if (next_token(parser) != 0) {
            expr_destroy(child);
            return NULL;
        }
        if (closing == EBNF_RPAREN) return child;
        expr = new_expr(closing == EBNF_RBRACKET ? EXPR_OPTIONAL : EXPR_REPETITION,
                        loc);
        if (expr == NULL) {
            expr_destroy(child);
            parser_error(parser, loc, "memoria insuficiente");
            return NULL;
        }
        expr->as.child = child;
        return expr;
    }
    parser_error(parser, parser->current.loc, "simbolo esperado na expressao EBNF");
    return NULL;
}

static Expr *parse_sequence(Parser *parser)
{
    Expr *sequence;
    Expr *child;
    SourceLoc loc;
    loc = parser->current.loc;
    if (!starts_primary(parser->current.kind)) {
        parser_error(parser, parser->current.loc, "sequencia EBNF vazia");
        return NULL;
    }
    child = parse_primary(parser);
    if (child == NULL) return NULL;
    if (!starts_primary(parser->current.kind)) return child;

    sequence = new_expr(EXPR_SEQUENCE, loc);
    if (sequence == NULL) {
        expr_destroy(child);
        parser_error(parser, loc, "memoria insuficiente");
        return NULL;
    }
    if (list_append(sequence, child) != 0) {
        expr_destroy(child);
        free(sequence);
        parser_error(parser, loc, "memoria insuficiente");
        return NULL;
    }
    while (starts_primary(parser->current.kind)) {
        child = parse_primary(parser);
        if (child == NULL) {
            expr_destroy(sequence);
            return NULL;
        }
        if (list_append(sequence, child) != 0) {
            expr_destroy(child);
            expr_destroy(sequence);
            parser_error(parser, loc, "memoria insuficiente");
            return NULL;
        }
    }
    return sequence;
}

static Expr *parse_alternative(Parser *parser)
{
    Expr *alternative;
    Expr *sequence;
    SourceLoc loc;
    loc = parser->current.loc;
    sequence = parse_sequence(parser);
    if (sequence == NULL) return NULL;
    if (parser->current.kind != EBNF_PIPE) return sequence;

    alternative = new_expr(EXPR_ALTERNATIVE, loc);
    if (alternative == NULL) {
        expr_destroy(sequence);
        parser_error(parser, loc, "memoria insuficiente");
        return NULL;
    }
    if (list_append(alternative, sequence) != 0) {
        expr_destroy(sequence);
        free(alternative);
        parser_error(parser, loc, "memoria insuficiente");
        return NULL;
    }
    while (parser->current.kind == EBNF_PIPE) {
        if (next_token(parser) != 0) {
            expr_destroy(alternative);
            return NULL;
        }
        sequence = parse_sequence(parser);
        if (sequence == NULL) {
            expr_destroy(alternative);
            return NULL;
        }
        if (list_append(alternative, sequence) != 0) {
            expr_destroy(sequence);
            expr_destroy(alternative);
            parser_error(parser, loc, "memoria insuficiente");
            return NULL;
        }
    }
    return alternative;
}

static int parse_tokens_section(Parser *parser)
{
    char *name;
    char *regex;
    SourceLoc loc;
    if (next_token(parser) != 0) return -1;
    while (parser->current.kind == EBNF_IDENT &&
           strcmp(parser->current.text, "RULES") != 0) {
        loc = parser->current.loc;
        name = dup_text(parser->current.text);
        if (name == NULL) {
            parser_error(parser, loc, "memoria insuficiente");
            return -1;
        }
        if (next_token(parser) != 0) { free(name); return -1; }
        if (expect(parser, EBNF_EQUAL, "'=' esperado na declaracao de token") != 0) {
            free(name); return -1;
        }
        if (parser->current.kind != EBNF_REGEX) {
            free(name);
            parser_error(parser, parser->current.loc,
                         "expressao regular esperada depois de '='");
            return -1;
        }
        regex = dup_text(parser->current.text);
        if (regex == NULL) {
            free(name);
            parser_error(parser, parser->current.loc, "memoria insuficiente");
            return -1;
        }
        if (next_token(parser) != 0 ||
            expect(parser, EBNF_SEMICOLON, "';' esperado apos declaracao de token") != 0) {
            free(name); free(regex); return -1;
        }
        if (strcmp(name, "SKIP") == 0) {
            if (parser->grammar->skip_regex != NULL) {
                free(name); free(regex);
                parser_error(parser, loc, "SKIP declarado mais de uma vez");
                return -1;
            }
            parser->grammar->skip_regex = regex;
            parser->grammar->skip_loc = loc;
            free(name);
        } else {
            if (grammar_add_token(parser->grammar, name, regex, loc) != 0) {
                free(name); free(regex);
                parser_error(parser, loc, "memoria insuficiente");
                return -1;
            }
            free(name);
            free(regex);
        }
    }
    if (parser->current.kind != EBNF_IDENT ||
        strcmp(parser->current.text, "RULES") != 0) {
        parser_error(parser, parser->current.loc, "secao RULES esperada apos TOKENS");
        return -1;
    }
    return next_token(parser);
}

static int parse_rules_section(Parser *parser)
{
    char *name;
    Expr *expr;
    SourceLoc loc;
    while (parser->current.kind != EBNF_EOF) {
        if (parser->current.kind != EBNF_IDENT) {
            parser_error(parser, parser->current.loc, "nome de regra esperado");
            return -1;
        }
        loc = parser->current.loc;
        name = dup_text(parser->current.text);
        if (name == NULL) {
            parser_error(parser, loc, "memoria insuficiente");
            return -1;
        }
        if (next_token(parser) != 0 ||
            expect(parser, EBNF_EQUAL, "'=' esperado depois do nome da regra") != 0) {
            free(name); return -1;
        }
        expr = parse_alternative(parser);
        if (expr == NULL) { free(name); return -1; }
        if (expect(parser, EBNF_SEMICOLON, "';' esperado ao final da regra") != 0) {
            free(name); expr_destroy(expr); return -1;
        }
        if (grammar_add_rule(parser->grammar, name, expr, loc) != 0) {
            free(name); expr_destroy(expr);
            parser_error(parser, loc, "memoria insuficiente");
            return -1;
        }
        free(name);
    }
    if (parser->grammar->rule_count == 0) {
        parser_error(parser, parser->current.loc, "a gramatica nao possui regras");
        return -1;
    }
    return 0;
}

int ebnf_parse_file(const char *filename, Grammar *grammar,
                    char *error, size_t error_size, SourceLoc *error_loc)
{
    Parser parser;
    int result;
    memset(&parser, 0, sizeof parser);
    parser.grammar = grammar;
    if (ebnf_lexer_open(&parser.lexer, filename) != 0) {
        if (error_size > 0) {
            strncpy(error, parser.lexer.error, error_size - 1);
            error[error_size - 1] = '\0';
        }
        if (error_loc != NULL) *error_loc = parser.lexer.error_loc;
        ebnf_lexer_close(&parser.lexer);
        return -1;
    }
    grammar->filename = dup_text(filename);
    if (grammar->filename == NULL) {
        ebnf_lexer_close(&parser.lexer);
        if (error_size > 0) strncpy(error, "memoria insuficiente", error_size - 1);
        return -1;
    }
    if (next_token(&parser) != 0) {
        result = -1;
    } else if (parser.current.kind != EBNF_IDENT ||
               strcmp(parser.current.text, "TOKENS") != 0) {
        parser_error(&parser, parser.current.loc, "secao TOKENS esperada no inicio do arquivo");
        result = -1;
    } else if (parse_tokens_section(&parser) != 0) {
        result = -1;
    } else {
        result = parse_rules_section(&parser);
    }
    if (result != 0) {
        if (error_size > 0) {
            strncpy(error, parser.error, error_size - 1);
            error[error_size - 1] = '\0';
        }
        if (error_loc != NULL) *error_loc = parser.error_loc;
    }
    ebnf_token_destroy(&parser.current);
    ebnf_lexer_close(&parser.lexer);
    return result;
}
