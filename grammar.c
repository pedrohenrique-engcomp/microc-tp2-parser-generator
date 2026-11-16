#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grammar.h"

static char *dup_string(const char *text)
{
    size_t n;
    char *copy;
    if (text == NULL) return NULL;
    n = strlen(text);
    copy = (char *)malloc(n + 1);
    if (copy != NULL) memcpy(copy, text, n + 1);
    return copy;
}

void grammar_init(Grammar *grammar)
{
    memset(grammar, 0, sizeof *grammar);
}

void expr_destroy(Expr *expr)
{
    size_t i;
    if (expr == NULL) return;
    if (expr->kind == EXPR_SYMBOL) {
        free(expr->as.symbol.name);
    } else if (expr->kind == EXPR_SEQUENCE ||
               expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            expr_destroy(expr->as.list.items[i]);
        free(expr->as.list.items);
    } else {
        expr_destroy(expr->as.child);
    }
    free(expr);
}

void grammar_destroy(Grammar *grammar)
{
    size_t i;
    if (grammar == NULL) return;
    free(grammar->filename);
    for (i = 0; i < grammar->token_count; ++i) {
        free(grammar->tokens[i].name);
        free(grammar->tokens[i].regex);
    }
    free(grammar->tokens);
    free(grammar->skip_regex);
    for (i = 0; i < grammar->rule_count; ++i) {
        free(grammar->rules[i].name);
        expr_destroy(grammar->rules[i].expr);
    }
    free(grammar->rules);
    memset(grammar, 0, sizeof *grammar);
}

static int reserve_tokens(Grammar *grammar)
{
    size_t capacity;
    TokenDecl *next;
    if (grammar->token_count < grammar->token_capacity) return 0;
    capacity = grammar->token_capacity == 0 ? 8 : grammar->token_capacity * 2;
    next = (TokenDecl *)realloc(grammar->tokens, capacity * sizeof *next);
    if (next == NULL) return -1;
    grammar->tokens = next;
    grammar->token_capacity = capacity;
    return 0;
}

static int reserve_rules(Grammar *grammar)
{
    size_t capacity;
    Rule *next;
    if (grammar->rule_count < grammar->rule_capacity) return 0;
    capacity = grammar->rule_capacity == 0 ? 8 : grammar->rule_capacity * 2;
    next = (Rule *)realloc(grammar->rules, capacity * sizeof *next);
    if (next == NULL) return -1;
    grammar->rules = next;
    grammar->rule_capacity = capacity;
    return 0;
}

int grammar_add_token(Grammar *grammar, const char *name,
                      const char *regex, SourceLoc loc)
{
    TokenDecl item;
    if (reserve_tokens(grammar) != 0) return -1;
    item.name = dup_string(name);
    item.regex = dup_string(regex);
    item.loc = loc;
    if (item.name == NULL || item.regex == NULL) {
        free(item.name);
        free(item.regex);
        return -1;
    }
    grammar->tokens[grammar->token_count++] = item;
    return 0;
}

int grammar_add_rule(Grammar *grammar, const char *name,
                     Expr *expr, SourceLoc loc)
{
    Rule item;
    if (reserve_rules(grammar) != 0) return -1;
    item.name = dup_string(name);
    if (item.name == NULL) return -1;
    item.expr = expr;
    item.loc = loc;
    item.reachable = 0;
    item.productive = 0;
    item.nullable = 0;
    grammar->rules[grammar->rule_count++] = item;
    return 0;
}

static void print_escaped(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    putchar('"');
    while (*p != '\0') {
        if (*p == '\\' || *p == '"') putchar('\\');
        if (*p == '\n') { fputs("\\n", stdout); ++p; continue; }
        if (*p == '\r') { fputs("\\r", stdout); ++p; continue; }
        if (*p == '\t') { fputs("\\t", stdout); ++p; continue; }
        putchar(*p++);
    }
    putchar('"');
}

static void print_expr(const Expr *expr)
{
    size_t i;
    if (expr == NULL) return;
    switch (expr->kind) {
    case EXPR_SYMBOL:
        if (expr->as.symbol.kind == SYMBOL_LITERAL)
            print_escaped(expr->as.symbol.name);
        else
            fputs(expr->as.symbol.name, stdout);
        break;
    case EXPR_SEQUENCE:
        for (i = 0; i < expr->as.list.count; ++i) {
            const Expr *child;
            if (i != 0) putchar(' ');
            child = expr->as.list.items[i];
            if (child->kind == EXPR_ALTERNATIVE) putchar('(');
            print_expr(child);
            if (child->kind == EXPR_ALTERNATIVE) putchar(')');
        }
        break;
    case EXPR_ALTERNATIVE:
        for (i = 0; i < expr->as.list.count; ++i) {
            if (i != 0) fputs(" | ", stdout);
            print_expr(expr->as.list.items[i]);
        }
        break;
    case EXPR_OPTIONAL:
        putchar('[');
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) putchar(' ');
        print_expr(expr->as.child);
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) putchar(' ');
        putchar(']');
        break;
    case EXPR_REPETITION:
        putchar('{');
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) putchar(' ');
        print_expr(expr->as.child);
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) putchar(' ');
        putchar('}');
        break;
    }
}

void grammar_print(const Grammar *grammar)
{
    size_t i;
    puts("TOKENS");
    for (i = 0; i < grammar->token_count; ++i)
        printf("  %s = /%s/;\n", grammar->tokens[i].name,
               grammar->tokens[i].regex);
    if (grammar->skip_regex != NULL)
        printf("  SKIP = /%s/;\n", grammar->skip_regex);
    puts("RULES");
    for (i = 0; i < grammar->rule_count; ++i) {
        printf("  %s = ", grammar->rules[i].name);
        print_expr(grammar->rules[i].expr);
        puts(";");
    }
}
