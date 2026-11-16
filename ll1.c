#include <stdio.h>
#include <stdlib.h>
#include "ll1.h"

static void print_set(const Analysis *a, const unsigned char *set)
{
    size_t i;
    const unsigned char *p;
    int first_item = 1;
    fputc('{', stderr);
    for (i = 0; i < a->terminal_count; ++i) {
        if (!set[i]) continue;
        if (!first_item) fputs(", ", stderr);
        if (a->terminal_kinds[i] == 1) {
            fputc('"', stderr);
            p = (const unsigned char *)a->terminal_names[i];
            while (*p != '\0') {
                if (*p == '\\' || *p == '"') fputc('\\', stderr);
                if (*p == '\n') fputs("\\n", stderr);
                else if (*p == '\r') fputs("\\r", stderr);
                else if (*p == '\t') fputs("\\t", stderr);
                else fputc(*p, stderr);
                ++p;
            }
            fputc('"', stderr);
        } else if (i == a->eof_index) {
            fputs("fim de arquivo", stderr);
        } else {
            fputs(a->terminal_names[i], stderr);
        }
        first_item = 0;
    }
    fputc('}', stderr);
}

static void intersection(const Analysis *a, const unsigned char *left,
                         const unsigned char *right, unsigned char *result)
{
    size_t i;
    for (i = 0; i < a->terminal_count; ++i)
        result[i] = (unsigned char)(left[i] && right[i]);
}

static int nonempty(const Analysis *a, const unsigned char *set)
{
    size_t i;
    for (i = 0; i < a->terminal_count; ++i) if (set[i]) return 1;
    return 0;
}

static int check_expr(const Grammar *g, const Analysis *a, const Expr *e,
                       const char *rule_name)
{
    size_t i, j, nullable_count, nullable_index;
    int errors = 0;
    unsigned char *common;
    const unsigned char *first_i;
    const unsigned char *first_j;
    const unsigned char *follow;
    const char *file;

    if (e == NULL) return 0;
    file = g->filename != NULL ? g->filename : "<gramatica>";
    if (e->kind == EXPR_ALTERNATIVE) {
        nullable_count = 0;
        nullable_index = 0;
        for (i = 0; i < e->as.list.count; ++i) {
            if (analysis_expr_nullable(a, e->as.list.items[i])) {
                ++nullable_count;
                nullable_index = i;
            }
        }
        for (i = 0; i < e->as.list.count; ++i) {
            first_i = analysis_expr_first(a, e->as.list.items[i]);
            for (j = i + 1; j < e->as.list.count; ++j) {
                first_j = analysis_expr_first(a, e->as.list.items[j]);
                common = (unsigned char *)calloc(a->terminal_count, 1);
                if (common == NULL) return errors + 1;
                intersection(a, first_i, first_j, common);
                if (nonempty(a, common)) {
                    fprintf(stderr, "%s:%d:%d: erro: conflito LL(1) em regra '%s' entre alternativas %lu e %lu: FIRST em comum ",
                            file, e->loc.linha, e->loc.coluna, rule_name,
                            (unsigned long)(i + 1), (unsigned long)(j + 1));
                    print_set(a, common);
                    fputc('\n', stderr);
                    ++errors;
                }
                free(common);
            }
        }
        if (nullable_count > 1) {
            fprintf(stderr, "%s:%d:%d: erro: mais de uma alternativa anulavel em regra '%s'\n",
                    file, e->loc.linha, e->loc.coluna, rule_name);
            ++errors;
        } else if (nullable_count == 1) {
            follow = analysis_expr_follow(a, e);
            for (i = 0; i < e->as.list.count; ++i) {
                if (i == nullable_index) continue;
                first_i = analysis_expr_first(a, e->as.list.items[i]);
                common = (unsigned char *)calloc(a->terminal_count, 1);
                if (common == NULL) return errors + 1;
                intersection(a, follow, first_i, common);
                if (nonempty(a, common)) {
                    fprintf(stderr, "%s:%d:%d: erro: conflito LL(1) em regra '%s': FOLLOW da alternativa anulavel intersecta FIRST da alternativa %lu: ",
                            file, e->loc.linha, e->loc.coluna, rule_name,
                            (unsigned long)(i + 1));
                    print_set(a, common);
                    fputc('\n', stderr);
                    ++errors;
                }
                free(common);
            }
        }
        for (i = 0; i < e->as.list.count; ++i)
            errors += check_expr(g, a, e->as.list.items[i], rule_name);
    } else if (e->kind == EXPR_OPTIONAL || e->kind == EXPR_REPETITION) {
        if (e->kind == EXPR_REPETITION &&
            analysis_expr_nullable(a, e->as.child)) {
            fprintf(stderr, "%s:%d:%d: erro: em regra '%s', corpo de repeticao e anulavel; o while poderia nao consumir tokens\n",
                    file, e->loc.linha, e->loc.coluna, rule_name);
            ++errors;
        } else {
            /* Um opcional cujo corpo e anulavel nao cria repeticao infinita:
             * ele e tentado uma unica vez. A selecao continua sendo feita
             * por FIRST, e a intersecao com FOLLOW permanece um aviso de
             * escolha gulosa. Para repeticoes, corpo anulavel e erro. */
            first_i = analysis_expr_first(a, e->as.child);
            follow = analysis_expr_follow(a, e);
            common = (unsigned char *)calloc(a->terminal_count, 1);
            if (common == NULL) return errors + 1;
            intersection(a, first_i, follow, common);
            if (nonempty(a, common)) {
                fprintf(stderr, "%s:%d:%d: aviso: em regra '%s', FIRST do corpo intersecta FOLLOW da construcao; sera aplicada escolha gulosa ",
                        file, e->loc.linha, e->loc.coluna, rule_name);
                print_set(a, common);
                fputc('\n', stderr);
            }
            free(common);
        }
        errors += check_expr(g, a, e->as.child, rule_name);
    } else if (e->kind == EXPR_SEQUENCE) {
        for (i = 0; i < e->as.list.count; ++i)
            errors += check_expr(g, a, e->as.list.items[i], rule_name);
    }
    return errors;
}

int grammar_check_ll1(const Grammar *grammar, const Analysis *analysis)
{
    size_t i;
    int errors = 0;
    for (i = 0; i < grammar->rule_count; ++i)
        errors += check_expr(grammar, analysis, grammar->rules[i].expr,
                             grammar->rules[i].name);
    if (errors != 0)
        fprintf(stderr, "%s: a gramatica nao e LL(1); nenhum codigo foi gerado\n",
                grammar->filename != NULL ? grammar->filename : "<gramatica>");
    return errors;
}
