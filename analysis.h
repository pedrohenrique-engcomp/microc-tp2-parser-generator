#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "grammar.h"

typedef struct Analysis {
    size_t terminal_count;
    char **terminal_names;
    int *terminal_kinds; /* 0=nome de token, 1=literal, 2=fim de entrada */
    size_t expr_count;
    Expr **expr_nodes;
    unsigned char *expr_nullable;
    unsigned char *expr_first;
    unsigned char *expr_follow;
    unsigned char *rule_nullable;
    unsigned char *rule_first;
    unsigned char *rule_follow;
    size_t eof_index;
} Analysis;

int analysis_compute(const Grammar *grammar, Analysis *analysis);
void analysis_destroy(Analysis *analysis);
int analysis_terminal_index(const Analysis *analysis, const char *name, int kind);
int analysis_expr_nullable(const Analysis *analysis, const Expr *expr);
const unsigned char *analysis_expr_first(const Analysis *analysis, const Expr *expr);
const unsigned char *analysis_expr_follow(const Analysis *analysis, const Expr *expr);
const unsigned char *analysis_rule_first(const Analysis *analysis, size_t rule);
const unsigned char *analysis_rule_follow(const Analysis *analysis, size_t rule);
int analysis_rule_nullable(const Analysis *analysis, size_t rule);
void analysis_print(const Grammar *grammar, const Analysis *analysis);

#endif
