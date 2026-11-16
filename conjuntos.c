#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "analysis.h"

static int add_terminal(Analysis *a, const char *name, int kind)
{
    size_t i;
    char **names;
    int *kinds;
    for (i = 0; i < a->terminal_count; ++i)
        if (a->terminal_kinds[i] == kind &&
            strcmp(a->terminal_names[i], name) == 0) return (int)i;
    names = (char **)realloc(a->terminal_names,
                             (a->terminal_count + 1) * sizeof *names);
    if (names == NULL) return -1;
    a->terminal_names = names;
    kinds = (int *)realloc(a->terminal_kinds,
                           (a->terminal_count + 1) * sizeof *kinds);
    if (kinds == NULL) return -1;
    a->terminal_kinds = kinds;
    a->terminal_names[a->terminal_count] = (char *)malloc(strlen(name) + 1);
    if (a->terminal_names[a->terminal_count] == NULL) return -1;
    strcpy(a->terminal_names[a->terminal_count], name);
    a->terminal_kinds[a->terminal_count] = kind;
    ++a->terminal_count;
    return (int)(a->terminal_count - 1);
}

static int count_exprs(const Expr *expr)
{
    size_t i;
    int count = 1;
    if (expr == NULL) return 0;
    if (expr->kind == EXPR_SEQUENCE || expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            count += count_exprs(expr->as.list.items[i]);
    } else if (expr->kind == EXPR_OPTIONAL || expr->kind == EXPR_REPETITION) {
        count += count_exprs(expr->as.child);
    }
    return count;
}

static void register_exprs(Analysis *a, Expr *expr, size_t *next)
{
    size_t i;
    if (expr == NULL) return;
    expr->analysis_id = *next;
    a->expr_nodes[*next] = expr;
    ++(*next);
    if (expr->kind == EXPR_SEQUENCE || expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            register_exprs(a, expr->as.list.items[i], next);
    } else if (expr->kind == EXPR_OPTIONAL || expr->kind == EXPR_REPETITION) {
        register_exprs(a, expr->as.child, next);
    }
}

static void collect_literals(Analysis *a, const Expr *expr)
{
    size_t i;
    if (expr == NULL) return;
    if (expr->kind == EXPR_SYMBOL) {
        if (expr->as.symbol.kind == SYMBOL_LITERAL)
            (void)add_terminal(a, expr->as.symbol.name, 1);
    } else if (expr->kind == EXPR_SEQUENCE || expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            collect_literals(a, expr->as.list.items[i]);
    } else {
        collect_literals(a, expr->as.child);
    }
}

static int rule_index(const Grammar *g, const char *name)
{
    size_t i;
    for (i = 0; i < g->rule_count; ++i)
        if (strcmp(g->rules[i].name, name) == 0) return (int)i;
    return -1;
}

static int expr_nullable_eval(const Grammar *g, Analysis *a, const Expr *e,
                              unsigned char *first)
{
    size_t i;
    int nullable;
    int index;
    const unsigned char *source;
    unsigned char *destination;
    size_t t;
    if (e == NULL) return 0;
    destination = first + e->analysis_id * a->terminal_count;
    memset(destination, 0, a->terminal_count);
    nullable = 0;
    switch (e->kind) {
    case EXPR_SYMBOL:
        if (e->as.symbol.kind == SYMBOL_LITERAL ||
            e->as.symbol.kind == SYMBOL_TERMINAL_NAME) {
            index = analysis_terminal_index(a, e->as.symbol.name,
                                            e->as.symbol.kind == SYMBOL_LITERAL ? 1 : 0);
            if (index >= 0) destination[index] = 1;
        } else {
            index = rule_index(g, e->as.symbol.name);
            if (index >= 0) {
                source = a->rule_first + (size_t)index * a->terminal_count;
                for (t = 0; t < a->terminal_count; ++t)
                    if (source[t]) destination[t] = 1;
                nullable = a->rule_nullable[index] != 0;
            }
        }
        break;
    case EXPR_SEQUENCE:
        nullable = 1;
        for (i = 0; i < e->as.list.count; ++i) {
            const Expr *child = e->as.list.items[i];
            int child_nullable = expr_nullable_eval(g, a, child, first);
            source = first + child->analysis_id * a->terminal_count;
            if (nullable) {
                for (t = 0; t < a->terminal_count; ++t)
                    if (source[t]) destination[t] = 1;
            }
            if (!child_nullable) nullable = 0;
        }
        break;
    case EXPR_ALTERNATIVE:
        nullable = 0;
        for (i = 0; i < e->as.list.count; ++i) {
            const Expr *child = e->as.list.items[i];
            if (expr_nullable_eval(g, a, child, first)) nullable = 1;
            source = first + child->analysis_id * a->terminal_count;
            for (t = 0; t < a->terminal_count; ++t)
                if (source[t]) destination[t] = 1;
        }
        break;
    case EXPR_OPTIONAL:
    case EXPR_REPETITION:
        (void)expr_nullable_eval(g, a, e->as.child, first);
        source = first + e->as.child->analysis_id * a->terminal_count;
        for (t = 0; t < a->terminal_count; ++t)
            if (source[t]) destination[t] = 1;
        nullable = 1;
        break;
    }
    a->expr_nullable[e->analysis_id] = (unsigned char)nullable;
    return nullable;
}

static int union_into(unsigned char *destination,
                      const unsigned char *source, size_t n)
{
    size_t i;
    int changed = 0;
    for (i = 0; i < n; ++i) {
        if (source[i] && !destination[i]) {
            destination[i] = 1;
            changed = 1;
        }
    }
    return changed;
}

static int add_follow(const Grammar *g, Analysis *a, const Expr *e,
                      const unsigned char *follow)
{
    size_t i;
    int changed = 0;
    int index;
    unsigned char *own;
    unsigned char *temporary;
    unsigned char *trailing;
    const unsigned char *child_first;
    int child_nullable;

    own = a->expr_follow + e->analysis_id * a->terminal_count;
    changed |= union_into(own, follow, a->terminal_count);
    if (e->kind == EXPR_SYMBOL) {
        if (e->as.symbol.kind == SYMBOL_NONTERMINAL) {
            index = rule_index(g, e->as.symbol.name);
            if (index >= 0)
                changed |= union_into(a->rule_follow + (size_t)index * a->terminal_count,
                                      follow, a->terminal_count);
        }
        return changed;
    }
    if (e->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < e->as.list.count; ++i)
            changed |= add_follow(g, a, e->as.list.items[i], follow);
        return changed;
    }
    if (e->kind == EXPR_OPTIONAL) {
        return changed | add_follow(g, a, e->as.child, follow);
    }
    if (e->kind == EXPR_REPETITION) {
        temporary = (unsigned char *)calloc(a->terminal_count, 1);
        if (temporary == NULL) return changed;
        child_first = analysis_expr_first(a, e->as.child);
        union_into(temporary, follow, a->terminal_count);
        union_into(temporary, child_first, a->terminal_count);
        changed |= add_follow(g, a, e->as.child, temporary);
        free(temporary);
        return changed;
    }
    if (e->kind == EXPR_SEQUENCE) {
        trailing = (unsigned char *)malloc(a->terminal_count);
        temporary = (unsigned char *)malloc(a->terminal_count);
        if (trailing == NULL || temporary == NULL) {
            free(trailing); free(temporary); return changed;
        }
        memcpy(trailing, follow, a->terminal_count);
        i = e->as.list.count;
        while (i > 0) {
            const Expr *child;
            --i;
            child = e->as.list.items[i];
            changed |= add_follow(g, a, child, trailing);
            memset(temporary, 0, a->terminal_count);
            child_first = analysis_expr_first(a, child);
            union_into(temporary, child_first, a->terminal_count);
            child_nullable = analysis_expr_nullable(a, child);
            if (child_nullable) union_into(temporary, trailing, a->terminal_count);
            memcpy(trailing, temporary, a->terminal_count);
        }
        free(trailing); free(temporary);
    }
    return changed;
}

int analysis_compute(const Grammar *grammar, Analysis *a)
{
    size_t i;
    size_t next;
    size_t bytes;
    int changed;

    memset(a, 0, sizeof *a);
    for (i = 0; i < grammar->token_count; ++i)
        if (add_terminal(a, grammar->tokens[i].name, 0) < 0) goto memory_error;
    for (i = 0; i < grammar->rule_count; ++i)
        collect_literals(a, grammar->rules[i].expr);
    a->eof_index = (size_t)add_terminal(a, "$", 2);
    if (a->terminal_count == 0) goto memory_error;

    for (i = 0; i < grammar->rule_count; ++i)
        a->expr_count += (size_t)count_exprs(grammar->rules[i].expr);
    a->expr_nodes = (Expr **)calloc(a->expr_count, sizeof *a->expr_nodes);
    a->expr_nullable = (unsigned char *)calloc(a->expr_count, 1);
    if (a->terminal_count != 0 && a->expr_count > ((size_t)-1) / a->terminal_count)
        goto memory_error;
    bytes = a->expr_count * a->terminal_count;
    a->expr_first = (unsigned char *)calloc(bytes, 1);
    a->expr_follow = (unsigned char *)calloc(bytes, 1);
    a->rule_nullable = (unsigned char *)calloc(grammar->rule_count, 1);
    if (grammar->rule_count != 0 && a->terminal_count > ((size_t)-1) / grammar->rule_count)
        goto memory_error;
    bytes = grammar->rule_count * a->terminal_count;
    a->rule_first = (unsigned char *)calloc(bytes, 1);
    a->rule_follow = (unsigned char *)calloc(bytes, 1);
    if (a->expr_nodes == NULL || a->expr_nullable == NULL ||
        a->expr_first == NULL || a->expr_follow == NULL ||
        a->rule_nullable == NULL || a->rule_first == NULL ||
        a->rule_follow == NULL) goto memory_error;
    next = 0;
    for (i = 0; i < grammar->rule_count; ++i)
        register_exprs(a, grammar->rules[i].expr, &next);

    do {
        changed = 0;
        for (i = 0; i < grammar->rule_count; ++i) {
            int nullable = expr_nullable_eval(grammar, a, grammar->rules[i].expr,
                                              a->expr_first);
            const unsigned char *first = analysis_expr_first(a, grammar->rules[i].expr);
            if (nullable && !a->rule_nullable[i]) {
                a->rule_nullable[i] = 1;
                changed = 1;
            }
            changed |= union_into(a->rule_first + i * a->terminal_count,
                                  first, a->terminal_count);
        }
    } while (changed);

    /* FOLLOW do simbolo inicial contem o marcador de fim de entrada. */
    a->rule_follow[0 * a->terminal_count + a->eof_index] = 1;
    do {
        changed = 0;
        for (i = 0; i < grammar->rule_count; ++i)
            changed |= add_follow(grammar, a, grammar->rules[i].expr,
                                  a->rule_follow + i * a->terminal_count);
    } while (changed);
    return 0;

memory_error:
    analysis_destroy(a);
    return -1;
}

void analysis_destroy(Analysis *a)
{
    size_t i;
    if (a == NULL) return;
    for (i = 0; i < a->terminal_count; ++i) free(a->terminal_names[i]);
    free(a->terminal_names);
    free(a->terminal_kinds);
    free(a->expr_nodes);
    free(a->expr_nullable);
    free(a->expr_first);
    free(a->expr_follow);
    free(a->rule_nullable);
    free(a->rule_first);
    free(a->rule_follow);
    memset(a, 0, sizeof *a);
}

int analysis_terminal_index(const Analysis *a, const char *name, int kind)
{
    size_t i;
    for (i = 0; i < a->terminal_count; ++i) {
        if (a->terminal_kinds[i] == kind &&
            strcmp(a->terminal_names[i], name) == 0)
            return (int)i;
    }
    return -1;
}

int analysis_expr_nullable(const Analysis *a, const Expr *e)
{
    return e != NULL ? a->expr_nullable[e->analysis_id] : 0;
}

const unsigned char *analysis_expr_first(const Analysis *a, const Expr *e)
{
    return e != NULL ? a->expr_first + e->analysis_id * a->terminal_count : NULL;
}

const unsigned char *analysis_expr_follow(const Analysis *a, const Expr *e)
{
    return e != NULL ? a->expr_follow + e->analysis_id * a->terminal_count : NULL;
}

const unsigned char *analysis_rule_first(const Analysis *a, size_t rule)
{
    return a->rule_first + rule * a->terminal_count;
}

const unsigned char *analysis_rule_follow(const Analysis *a, size_t rule)
{
    return a->rule_follow + rule * a->terminal_count;
}

int analysis_rule_nullable(const Analysis *a, size_t rule)
{
    return a->rule_nullable[rule] != 0;
}

void analysis_print(const Grammar *g, const Analysis *a)
{
    size_t i, t;
    int first_item;
    puts("CONJUNTOS DOS NAO TERMINAIS");
    for (i = 0; i < g->rule_count; ++i) {
        printf("%s: anulavel=%s FIRST={", g->rules[i].name,
               analysis_rule_nullable(a, i) ? "sim" : "nao");
        first_item = 1;
        for (t = 0; t < a->terminal_count; ++t) {
            if (analysis_rule_first(a, i)[t]) {
                printf("%s", first_item ? "" : ", ");
                if (a->terminal_kinds[t] == 1)
                    printf("\"%s\"", a->terminal_names[t]);
                else
                    printf("%s", a->terminal_names[t]);
                first_item = 0;
            }
        }
        fputs("} FOLLOW={", stdout);
        first_item = 1;
        for (t = 0; t < a->terminal_count; ++t) {
            if (analysis_rule_follow(a, i)[t]) {
                printf("%s", first_item ? "" : ", ");
                if (a->terminal_kinds[t] == 1)
                    printf("\"%s\"", a->terminal_names[t]);
                else
                    printf("%s", a->terminal_names[t]);
                first_item = 0;
            }
        }
        puts("}");
    }
}
