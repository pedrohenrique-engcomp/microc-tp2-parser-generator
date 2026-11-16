#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stddef.h>

typedef struct SourceLoc {
    int linha;
    int coluna;
} SourceLoc;

typedef enum ExprKind {
    EXPR_SYMBOL,
    EXPR_SEQUENCE,
    EXPR_ALTERNATIVE,
    EXPR_OPTIONAL,
    EXPR_REPETITION
} ExprKind;

typedef enum SymbolKind {
    SYMBOL_TERMINAL_NAME,
    SYMBOL_NONTERMINAL,
    SYMBOL_LITERAL
} SymbolKind;

typedef struct Expr Expr;

typedef struct ExprList {
    Expr **items;
    size_t count;
    size_t capacity;
} ExprList;

struct Expr {
    ExprKind kind;
    size_t analysis_id;
    SourceLoc loc;
    union {
        struct {
            SymbolKind kind;
            char *name;
        } symbol;
        ExprList list;
        Expr *child;
    } as;
};

typedef struct TokenDecl {
    char *name;
    char *regex;
    SourceLoc loc;
} TokenDecl;

typedef struct Rule {
    char *name;
    Expr *expr;
    SourceLoc loc;
    int reachable;
    int productive;
    int nullable;
} Rule;

typedef struct Grammar {
    char *filename;
    TokenDecl *tokens;
    size_t token_count;
    size_t token_capacity;
    char *skip_regex;
    SourceLoc skip_loc;
    Rule *rules;
    size_t rule_count;
    size_t rule_capacity;
} Grammar;

void grammar_init(Grammar *grammar);
void grammar_destroy(Grammar *grammar);
void expr_destroy(Expr *expr);
int grammar_add_token(Grammar *grammar, const char *name,
                      const char *regex, SourceLoc loc);
int grammar_add_rule(Grammar *grammar, const char *name,
                     Expr *expr, SourceLoc loc);
void grammar_print(const Grammar *grammar);

#endif
