#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "verificacao.h"

static int find_rule(const Grammar *grammar, const char *name)
{
    size_t i;
    for (i = 0; i < grammar->rule_count; ++i)
        if (strcmp(grammar->rules[i].name, name) == 0) return (int)i;
    return -1;
}

static int find_token(const Grammar *grammar, const char *name)
{
    size_t i;
    for (i = 0; i < grammar->token_count; ++i)
        if (strcmp(grammar->tokens[i].name, name) == 0) return (int)i;
    return -1;
}

static void report_named_error(const Grammar *grammar, SourceLoc loc,
                               const char *prefix, const char *name,
                               const char *suffix)
{
    fprintf(stderr, "%s:%d:%d: erro: %s'%s'%s\n",
            grammar->filename != NULL ? grammar->filename : "<gramatica>",
            loc.linha, loc.coluna, prefix, name, suffix);
}

static void report_named_warning(const Grammar *grammar, SourceLoc loc,
                                 const char *prefix, const char *name,
                                 const char *suffix)
{
    fprintf(stderr, "%s:%d:%d: aviso: %s'%s'%s\n",
            grammar->filename != NULL ? grammar->filename : "<gramatica>",
            loc.linha, loc.coluna, prefix, name, suffix);
}

static int check_references(const Grammar *grammar, const Expr *expr)
{
    size_t i;
    int errors = 0;
    int index;
    if (expr == NULL) return 0;
    if (expr->kind == EXPR_SYMBOL) {
        if (expr->as.symbol.kind == SYMBOL_NONTERMINAL) {
            index = find_rule(grammar, expr->as.symbol.name);
            if (index < 0) {
                report_named_error(grammar, expr->loc,
                                    "nao terminal ", expr->as.symbol.name,
                                    " foi usado mas nao foi definido");
                return 1;
            }
        } else if (expr->as.symbol.kind == SYMBOL_TERMINAL_NAME) {
            index = find_token(grammar, expr->as.symbol.name);
            if (index < 0) {
                report_named_error(grammar, expr->loc,
                                    "terminal ", expr->as.symbol.name,
                                    " nao foi declarado em TOKENS");
                return 1;
            }
        }
        return 0;
    }
    if (expr->kind == EXPR_SEQUENCE || expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            errors += check_references(grammar, expr->as.list.items[i]);
    } else {
        errors += check_references(grammar, expr->as.child);
    }
    return errors;
}

static void mark_expr_references(Grammar *grammar, const Expr *expr);

static void mark_rule_reachable(Grammar *grammar, int index)
{
    if (index < 0 || grammar->rules[index].reachable) return;
    grammar->rules[index].reachable = 1;
    mark_expr_references(grammar, grammar->rules[index].expr);
}

static void mark_expr_references(Grammar *grammar, const Expr *expr)
{
    size_t i;
    int index;
    if (expr == NULL) return;
    if (expr->kind == EXPR_SYMBOL) {
        if (expr->as.symbol.kind == SYMBOL_NONTERMINAL) {
            index = find_rule(grammar, expr->as.symbol.name);
            mark_rule_reachable(grammar, index);
        }
    } else if (expr->kind == EXPR_SEQUENCE || expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            mark_expr_references(grammar, expr->as.list.items[i]);
    } else {
        mark_expr_references(grammar, expr->as.child);
    }
}

static int expr_productive(const Grammar *grammar, const Expr *expr)
{
    size_t i;
    int index;
    if (expr == NULL) return 0;
    if (expr->kind == EXPR_SYMBOL) {
        if (expr->as.symbol.kind != SYMBOL_NONTERMINAL) return 1;
        index = find_rule(grammar, expr->as.symbol.name);
        return index >= 0 ? grammar->rules[index].productive : 0;
    }
    if (expr->kind == EXPR_OPTIONAL || expr->kind == EXPR_REPETITION)
        return 1;
    if (expr->kind == EXPR_SEQUENCE) {
        for (i = 0; i < expr->as.list.count; ++i)
            if (!expr_productive(grammar, expr->as.list.items[i])) return 0;
        return 1;
    }
    for (i = 0; i < expr->as.list.count; ++i)
        if (expr_productive(grammar, expr->as.list.items[i])) return 1;
    return 0;
}

static int expr_nullable(const Grammar *grammar, const Expr *expr)
{
    size_t i;
    int index;
    if (expr == NULL) return 0;
    if (expr->kind == EXPR_SYMBOL) {
        if (expr->as.symbol.kind != SYMBOL_NONTERMINAL) return 0;
        index = find_rule(grammar, expr->as.symbol.name);
        return index >= 0 ? grammar->rules[index].nullable : 0;
    }
    if (expr->kind == EXPR_OPTIONAL || expr->kind == EXPR_REPETITION)
        return 1;
    if (expr->kind == EXPR_SEQUENCE) {
        for (i = 0; i < expr->as.list.count; ++i)
            if (!expr_nullable(grammar, expr->as.list.items[i])) return 0;
        return 1;
    }
    for (i = 0; i < expr->as.list.count; ++i)
        if (expr_nullable(grammar, expr->as.list.items[i])) return 1;
    return 0;
}

static void leading_edges(const Grammar *grammar, const Expr *expr,
                          int source, unsigned char *edges, size_t n);

static void add_leading_symbol(const Grammar *grammar, const Expr *expr,
                               int source, unsigned char *edges, size_t n)
{
    int target;
    if (expr == NULL) return;
    if (expr->kind == EXPR_SYMBOL) {
        if (expr->as.symbol.kind == SYMBOL_NONTERMINAL) {
            target = find_rule(grammar, expr->as.symbol.name);
            if (target >= 0 && (size_t)target < n)
                edges[(size_t)source * n + (size_t)target] = 1;
        }
        return;
    }
    leading_edges(grammar, expr, source, edges, n);
}

static void leading_edges(const Grammar *grammar, const Expr *expr,
                          int source, unsigned char *edges, size_t n)
{
    size_t i;
    if (expr == NULL) return;
    if (expr->kind == EXPR_SYMBOL) {
        add_leading_symbol(grammar, expr, source, edges, n);
    } else if (expr->kind == EXPR_SEQUENCE) {
        for (i = 0; i < expr->as.list.count; ++i) {
            add_leading_symbol(grammar, expr->as.list.items[i], source, edges, n);
            if (!expr_nullable(grammar, expr->as.list.items[i])) break;
        }
    } else if (expr->kind == EXPR_ALTERNATIVE) {
        for (i = 0; i < expr->as.list.count; ++i)
            add_leading_symbol(grammar, expr->as.list.items[i], source, edges, n);
    } else {
        add_leading_symbol(grammar, expr->as.child, source, edges, n);
    }
}

static int check_left_recursion(Grammar *grammar)
{
    size_t i, j, k;
    size_t n = grammar->rule_count;
    unsigned char *edges;
    int found = 0;
    int changed;

    if (n == 0) return 0;

    /* O cálculo de anulabilidade é necessário para construir as arestas
     * de prefixo: uma regra pode aparecer após símbolos anuláveis. */
    do {
        changed = 0;
        for (i = 0; i < n; ++i) {
            if (!grammar->rules[i].nullable &&
                expr_nullable(grammar, grammar->rules[i].expr)) {
                grammar->rules[i].nullable = 1;
                changed = 1;
            }
        }
    } while (changed);

    if (n > (size_t)-1 / n) {
        fprintf(stderr, "erro: numero de regras excede o limite para verificar recursao a esquerda\n");
        return 1;
    }
    edges = (unsigned char *)calloc(n * n, sizeof *edges);
    if (edges == NULL) {
        fprintf(stderr, "erro: memoria insuficiente ao verificar recursao a esquerda\n");
        return 1;
    }

    for (i = 0; i < n; ++i)
        leading_edges(grammar, grammar->rules[i].expr, (int)i, edges, n);

    /* Fecho transitivo de Warshall. Uma diagonal verdadeira significa que
     * a regra alcança a si mesma por uma ou mais chamadas em prefixo. Ao
     * contrário do DFS anterior, cada regra é avaliada independentemente,
     * e nenhum estado parcial de busca contamina a próxima verificação. */
    for (k = 0; k < n; ++k) {
        for (i = 0; i < n; ++i) {
            if (!edges[i * n + k]) continue;
            for (j = 0; j < n; ++j) {
                if (edges[k * n + j]) edges[i * n + j] = 1;
            }
        }
    }

    for (i = 0; i < n; ++i) {
        if (edges[i * n + i]) {
            report_named_error(grammar, grammar->rules[i].loc,
                                "recursao a esquerda direta ou indireta em ",
                                grammar->rules[i].name, "");
            found++;
        }
    }
    free(edges);
    return found;
}

int grammar_verify(Grammar *grammar)
{
    size_t i;
    size_t j;
    int errors = 0;
    int changed;

    if (grammar->rule_count == 0) {
        fprintf(stderr, "erro: gramatica sem regras\n");
        return 1;
    }
    for (i = 0; i < grammar->token_count; ++i) {
        if (!isupper((unsigned char)grammar->tokens[i].name[0])) {
            report_named_error(grammar, grammar->tokens[i].loc,
                                "nome de terminal ", grammar->tokens[i].name,
                                " deve comecar com maiuscula");
            errors++;
        }
        for (j = i + 1; j < grammar->token_count; ++j) {
            if (strcmp(grammar->tokens[i].name, grammar->tokens[j].name) == 0) {
                report_named_error(grammar, grammar->tokens[j].loc,
                                    "terminal ", grammar->tokens[i].name,
                                    " declarado mais de uma vez");
                errors++;
            }
        }
    }
    for (i = 0; i < grammar->rule_count; ++i) {
        if (!islower((unsigned char)grammar->rules[i].name[0])) {
            report_named_error(grammar, grammar->rules[i].loc,
                                "nome de nao terminal ", grammar->rules[i].name,
                                " deve comecar com minuscula");
            errors++;
        }
        for (j = i + 1; j < grammar->rule_count; ++j) {
            if (strcmp(grammar->rules[i].name, grammar->rules[j].name) == 0) {
                report_named_error(grammar, grammar->rules[j].loc,
                                    "nao terminal ", grammar->rules[i].name,
                                    " definido mais de uma vez");
                errors++;
            }
        }
        errors += check_references(grammar, grammar->rules[i].expr);
    }
    if (errors != 0) return errors;

    mark_rule_reachable(grammar, 0);
    for (i = 0; i < grammar->rule_count; ++i) {
        if (!grammar->rules[i].reachable) {
            report_named_warning(grammar, grammar->rules[i].loc,
                                  "nao terminal ", grammar->rules[i].name,
                                  " e inalcançavel a partir do simbolo inicial");
        }
    }

    do {
        changed = 0;
        for (i = 0; i < grammar->rule_count; ++i) {
            if (!grammar->rules[i].productive &&
                expr_productive(grammar, grammar->rules[i].expr)) {
                grammar->rules[i].productive = 1;
                changed = 1;
            }
        }
    } while (changed);
    for (i = 0; i < grammar->rule_count; ++i) {
        if (!grammar->rules[i].productive) {
            report_named_error(grammar, grammar->rules[i].loc,
                                "nao terminal ", grammar->rules[i].name,
                                " e improdutivo");
            errors++;
        }
    }
    if (errors != 0) return errors;
    errors += check_left_recursion(grammar);
    return errors;
}
