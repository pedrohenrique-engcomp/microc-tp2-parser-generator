#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"

static int make_filename(char *out, size_t size, const char *prefix,
                         const char *suffix)
{
    size_t prefix_length;
    size_t suffix_length;
    prefix_length = strlen(prefix);
    suffix_length = strlen(suffix);
    if (prefix_length + suffix_length + 1 > size) return -1;
    memcpy(out, prefix, prefix_length);
    memcpy(out + prefix_length, suffix, suffix_length + 1);
    return 0;
}

static void token_macro_name(char *out, size_t capacity,
                             const Analysis *a, size_t index)
{
    size_t i;
    size_t used;
    if (a->terminal_kinds[index] == 1) {
        sprintf(out, "PARSER_LITERAL_%lu", (unsigned long)index);
        return;
    }
    if (a->terminal_kinds[index] != 0) {
        sprintf(out, "PARSER_TOK_%lu", (unsigned long)index);
        return;
    }
    if (capacity < 10) {
        sprintf(out, "PARSER_TOK_%lu", (unsigned long)index);
        return;
    }
    strcpy(out, "PARSER_T_");
    used = strlen(out);
    if (strlen(a->terminal_names[index]) + used + 1 > capacity) {
        sprintf(out, "PARSER_TOK_%lu", (unsigned long)index);
        return;
    }
    for (i = 0; a->terminal_names[index][i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)a->terminal_names[index][i];
        out[used++] = (char)(isalnum(ch) ? ch : '_');
    }
    out[used] = '\0';
}

static void write_c_escaped_content(FILE *out, const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    while (*p != '\0') {
        switch (*p) {
        case '\\': fputs("\\\\", out); break;
        case '"':  fputs("\\\"", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (*p < 32 || *p == 127)
                fprintf(out, "\\%03o", (unsigned int)*p);
            else
                fputc(*p, out);
            break;
        }
        ++p;
    }
}

static void write_c_string_literal(FILE *out, const char *text)
{
    fputc('"', out);
    write_c_escaped_content(out, text);
    fputc('"', out);
}

static void write_terminal_display(FILE *out, const Analysis *a, size_t index)
{
    const unsigned char *p;
    char *display;
    size_t length;
    size_t used;
    if (a->terminal_kinds[index] != 1) {
        write_c_string_literal(out, a->terminal_names[index]);
        return;
    }

    /* Primeiro monta a forma EBNF legível, por exemplo `"\""`; depois
     * escapa essa mensagem como uma string C independente. */
    length = strlen(a->terminal_names[index]);
    display = (char *)malloc(length * 2 + 3);
    if (display == NULL) {
        write_c_string_literal(out, "<literal>");
        return;
    }
    used = 0;
    display[used++] = '"';
    p = (const unsigned char *)a->terminal_names[index];
    while (*p != '\0') {
        if (*p == '\\' || *p == '"') display[used++] = '\\';
        if (*p == '\n') display[used++] = 'n';
        else if (*p == '\r') display[used++] = 'r';
        else if (*p == '\t') display[used++] = 't';
        else display[used++] = (char)*p;
        ++p;
    }
    display[used++] = '"';
    display[used] = '\0';
    write_c_string_literal(out, display);
    free(display);
}

static void write_ebnf_literal(FILE *out, const char *text, int in_comment)
{
    const unsigned char *p = (const unsigned char *)text;
    fputc('"', out);
    while (*p != '\0') {
        if (in_comment && p[0] == '*' && p[1] == '/') {
            fputs("*\\/", out);
            p += 2;
            continue;
        }
        switch (*p) {
        case '\\': fputs("\\\\", out); break;
        case '"':  fputs("\\\"", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default: fputc(*p, out); break;
        }
        ++p;
    }
    fputc('"', out);
}

static void write_ebnf_expr(FILE *out, const Expr *expr)
{
    size_t i;
    if (expr == NULL) return;
    switch (expr->kind) {
    case EXPR_SYMBOL:
        if (expr->as.symbol.kind == SYMBOL_LITERAL)
            write_ebnf_literal(out, expr->as.symbol.name, 1);
        else
            fputs(expr->as.symbol.name, out);
        break;
    case EXPR_SEQUENCE:
        for (i = 0; i < expr->as.list.count; ++i) {
            const Expr *child = expr->as.list.items[i];
            if (i != 0) fputc(' ', out);
            if (child->kind == EXPR_ALTERNATIVE) fputc('(', out);
            write_ebnf_expr(out, child);
            if (child->kind == EXPR_ALTERNATIVE) fputc(')', out);
        }
        break;
    case EXPR_ALTERNATIVE:
        for (i = 0; i < expr->as.list.count; ++i) {
            if (i != 0) fputs(" | ", out);
            write_ebnf_expr(out, expr->as.list.items[i]);
        }
        break;
    case EXPR_OPTIONAL:
        fputc('[', out);
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) fputc(' ', out);
        write_ebnf_expr(out, expr->as.child);
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) fputc(' ', out);
        fputc(']', out);
        break;
    case EXPR_REPETITION:
        fputc('{', out);
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) fputc(' ', out);
        write_ebnf_expr(out, expr->as.child);
        if (expr->as.child->kind == EXPR_SEQUENCE ||
            expr->as.child->kind == EXPR_ALTERNATIVE) fputc(' ', out);
        fputc('}', out);
        break;
    }
}

static void write_rule_comment(FILE *out, const Rule *rule)
{
    fputs("/* Regra EBNF de origem:\n * ", out);
    fputs(rule->name, out);
    fputs(" = ", out);
    write_ebnf_expr(out, rule->expr);
    fputs(";\n */\n", out);
}

static int write_header(FILE *h, const Grammar *g, const Analysis *a)
{
    size_t i;
    (void)g;
    fputs("#ifndef PARSER_GENERATED_H\n#define PARSER_GENERATED_H\n\n", h);
    fputs("#include <stddef.h>\n\n", h);
    fputs("enum ParserTokenKind {\n    PARSER_EOF = 0", h);
    for (i = 0; i < a->terminal_count; ++i) {
        if (i == a->eof_index) continue;
        fprintf(h, ",\n    PARSER_TOK_%lu = %lu",
                (unsigned long)i, (unsigned long)(i + 1));
    }
    fputc('\n', h);
    fputs("};\n\n", h);
    for (i = 0; i < a->terminal_count; ++i) {
        char macro[256];
        if (i == a->eof_index || a->terminal_kinds[i] == 2) continue;
        token_macro_name(macro, sizeof macro, a, i);
        fprintf(h, "#define %s PARSER_TOK_%lu\n", macro, (unsigned long)i);
    }
    fputs("\ntypedef struct ParserToken {\n"
          "    int type;\n"
          "    char lexema[4096];\n"
          "    char mensagem[256];\n"
          "    int linha;\n"
          "    int coluna;\n"
          "} ParserToken;\n\n"
          "typedef int (*ParserNextTokenFn)(ParserToken *token, void *context);\n\n"
          "/* Retorna 0 se a entrada pertence a gramatica; 1 em erro. */\n"
          "int parser_parse(ParserNextTokenFn next_token, void *context);\n"
          "const char *parser_token_name(int type);\n\n"
          "#endif\n", h);
    return ferror(h) ? -1 : 0;
}

static void emit_error_set(FILE *c, const Analysis *a,
                           const unsigned char *set, const char *indent)
{
    size_t i;
    int n = 0;
    fprintf(c, "%s{\n%s    int expected[%lu];\n", indent, indent,
            (unsigned long)(a->terminal_count + 1));
    for (i = 0; i < a->terminal_count; ++i) {
        if (!set[i]) continue;
        if (i == a->eof_index)
            fprintf(c, "%s    expected[%d] = PARSER_EOF;\n", indent, n);
        else {
            char macro[256];
            token_macro_name(macro, sizeof macro, a, i);
            fprintf(c, "%s    expected[%d] = %s;\n", indent, n, macro);
        }
        ++n;
    }
    fprintf(c, "%s    parser_error_set(expected, %d);\n%s}\n", indent, n, indent);
}

static void emit_expr(FILE *c, const Grammar *g, const Analysis *a,
                      const Expr *e, int depth)
{
    size_t i, j;
    const unsigned char *first;
    const unsigned char *follow;
    unsigned char *expected;
    int has_nullable;
    int nullable_index;
    int first_term;
    char indent[64];
    int k;
    (void)g;
    if (depth > 40) return;
    if (depth * 4 >= (int)sizeof indent) depth = 10;
    for (k = 0; k < depth * 4; ++k) indent[k] = ' ';
    indent[depth * 4] = '\0';

    if (e->kind == EXPR_SYMBOL) {
        if (e->as.symbol.kind == SYMBOL_NONTERMINAL) {
            fprintf(c, "%sif (!parse_%s()) return 0;\n", indent,
                    e->as.symbol.name);
        } else {
            int ti = analysis_terminal_index(a, e->as.symbol.name,
                                               e->as.symbol.kind == SYMBOL_LITERAL ? 1 : 0);
            if (ti >= 0) {
                char macro[256];
                token_macro_name(macro, sizeof macro, a, (size_t)ti);
                fprintf(c, "%sif (!parser_match(%s)) return 0;\n",
                        indent, macro);
            }
        }
        return;
    }
    if (e->kind == EXPR_SEQUENCE) {
        for (i = 0; i < e->as.list.count; ++i)
            emit_expr(c, g, a, e->as.list.items[i], depth);
        return;
    }
    if (e->kind == EXPR_OPTIONAL || e->kind == EXPR_REPETITION) {
        first = analysis_expr_first(a, e->as.child);
        if (e->kind == EXPR_OPTIONAL)
            fprintf(c, "%sif (", indent);
        else
            fprintf(c, "%swhile (", indent);
        first_term = 0;
        for (i = 0; i < a->terminal_count; ++i) {
            if (!first[i]) continue;
            if (i == a->eof_index) {
                fprintf(c, "%sparser_current.type == PARSER_EOF",
                        first_term ? " || " : "");
            } else {
                char macro[256];
                token_macro_name(macro, sizeof macro, a, i);
                fprintf(c, "%sparser_current.type == %s",
                        first_term ? " || " : "", macro);
            }
            first_term = 1;
        }
        if (!first_term) fputs("0", c);
        fputs(") {\n", c);
        emit_expr(c, g, a, e->as.child, depth + 1);
        fprintf(c, "%s}\n", indent);
        return;
    }
    if (e->kind == EXPR_ALTERNATIVE) {
        expected = (unsigned char *)calloc(a->terminal_count, 1);
        if (expected == NULL) return;
        has_nullable = 0;
        nullable_index = -1;
        for (i = 0; i < e->as.list.count; ++i) {
            first = analysis_expr_first(a, e->as.list.items[i]);
            for (j = 0; j < a->terminal_count; ++j)
                if (first[j]) expected[j] = 1;
            if (analysis_expr_nullable(a, e->as.list.items[i])) {
                has_nullable = 1;
                nullable_index = (int)i;
            }
        }
        follow = analysis_expr_follow(a, e);
        if (has_nullable) {
            for (j = 0; j < a->terminal_count; ++j)
                if (follow[j]) expected[j] = 1;
        }

        /* Uma alternativa anulavel pode ser escolhida tanto por FIRST
         * quanto por FOLLOW. Unimos as duas condicoes e emitimos o corpo
         * uma unica vez, evitando blocos duplicados. */
        for (i = 0; i < e->as.list.count; ++i) {
            const Expr *branch = e->as.list.items[i];
            first = analysis_expr_first(a, branch);
            if (i == 0) fprintf(c, "%sif (", indent);
            else fprintf(c, "%selse if (", indent);
            first_term = 0;
            for (j = 0; j < a->terminal_count; ++j) {
                int include_terminal = first[j] ||
                    ((int)i == nullable_index && has_nullable && follow[j]);
                if (!include_terminal) continue;
                if (j == a->eof_index) {
                    fprintf(c, "%sparser_current.type == PARSER_EOF",
                            first_term ? " || " : "");
                } else {
                    char macro[256];
                    token_macro_name(macro, sizeof macro, a, j);
                    fprintf(c, "%sparser_current.type == %s",
                            first_term ? " || " : "", macro);
                }
                first_term = 1;
            }
            if (!first_term) fputs("0", c);
            fputs(") {\n", c);
            emit_expr(c, g, a, branch, depth + 1);
            fprintf(c, "%s}\n", indent);
        }
        fprintf(c, "%selse {\n", indent);
        /* O nível interno do bloco de erro corresponde ao bloco else. */
        {
            char error_indent[64];
            size_t indent_length = strlen(indent);
            if (indent_length + 4 >= sizeof error_indent)
                indent_length = sizeof error_indent - 5;
            memcpy(error_indent, indent, indent_length);
            memset(error_indent + indent_length, ' ', 4);
            error_indent[indent_length + 4] = '\0';
            emit_error_set(c, a, expected, error_indent);
        }
        fprintf(c, "%s    return 0;\n%s}\n", indent, indent);
        free(expected);
    }
}

static int write_parser(FILE *c, const Grammar *g, const Analysis *a,
                        const char *header_basename)
{
    size_t i, j;
    fputs("#include <stdio.h>\n#include <string.h>\n#include \"", c);
    fprintf(c, "%s", header_basename);
    fputs("\"\n\n", c);
    fputs("static ParserToken parser_current;\n"
          "static ParserNextTokenFn parser_next_fn;\n"
          "static void *parser_context;\n"
          "static int parser_failed;\n\n", c);
    fputs("const char *parser_token_name(int type)\n{\n    switch (type) {\n"
          "    case PARSER_EOF: return \"fim de arquivo\";\n", c);
    for (i = 0; i < a->terminal_count; ++i) {
        char macro[256];
        if (i == a->eof_index) continue;
        token_macro_name(macro, sizeof macro, a, i);
        fprintf(c, "    case %s: return ", macro);
        write_terminal_display(c, a, i);
        fputs(";\n", c);
    }
    fputs("    default: return \"token desconhecido\";\n    }\n}\n\n", c);
    fputs("static void parser_error_set(const int *expected, int count)\n{\n"
          "    int i;\n"
          "    if (parser_failed) return;\n"
          "    parser_failed = 1;\n"
          "    fprintf(stderr, \"erro sintatico na linha %d, coluna %d: encontrado %s; esperado {\",\n"
          "            parser_current.linha, parser_current.coluna,\n"
          "            parser_current.type == PARSER_EOF ? \"fim de arquivo\" :\n"
          "            (parser_current.lexema[0] ? parser_current.lexema : parser_token_name(parser_current.type)));\n", c);
    fputs("    for (i = 0; i < count; ++i)\n"
          "        fprintf(stderr, \"%s%s\", i ? \", \" : \"\", parser_token_name(expected[i]));\n"
          "    fputs(\"}\\n\", stderr);\n}\n\n", c);
    fputs("static int parser_advance(void)\n{\n"
          "    int status;\n"
          "    memset(&parser_current, 0, sizeof parser_current);\n"
          "    status = parser_next_fn(&parser_current, parser_context);\n"
          "    if (status != 0) {\n"
          "        fprintf(stderr, \"erro: falha ao obter proximo token\\n\");\n"
          "        parser_failed = 1; return 0;\n"
          "    }\n", c);
    fputs("    if (parser_current.type < 0) {\n"
          "        fprintf(stderr, \"erro lexico na linha %d, coluna %d: %s\\n\",\n"
          "                parser_current.linha, parser_current.coluna,\n"
          "                parser_current.mensagem[0] ? parser_current.mensagem : \"token invalido\");\n"
          "        parser_failed = 1; return 0;\n"
          "    }\n"
          "    return 1;\n}\n\n", c);
    fputs("static int parser_match(int expected)\n{\n"
          "    int set[1];\n"
          "    if (parser_current.type != expected) {\n"
          "        set[0] = expected; parser_error_set(set, 1); return 0;\n"
          "    }\n"
          "    return parser_advance();\n}\n\n", c);
    for (i = 0; i < g->rule_count; ++i) {
        if (!g->rules[i].reachable) continue;
        fprintf(c, "static int parse_%s(void);\n", g->rules[i].name);
    }
    fputc('\n', c);
    for (i = 0; i < g->rule_count; ++i) {
        if (!g->rules[i].reachable) continue;
        write_rule_comment(c, &g->rules[i]);
        fprintf(c, "static int parse_%s(void)\n{\n", g->rules[i].name);
        emit_expr(c, g, a, g->rules[i].expr, 1);
        fputs("    return !parser_failed;\n}\n\n", c);
    }
    fputs("int parser_parse(ParserNextTokenFn next_token, void *context)\n{\n"
          "    if (next_token == NULL) return 1;\n"
          "    parser_next_fn = next_token; parser_context = context;\n"
          "    parser_failed = 0;\n"
          "    if (!parser_advance()) return 1;\n", c);
    fprintf(c, "    if (!parse_%s()) return 1;\n", g->rules[0].name);
    fputs("    if (parser_current.type != PARSER_EOF) {\n"
          "        int expected[1]; expected[0] = PARSER_EOF;\n"
          "        parser_error_set(expected, 1); return 1;\n"
          "    }\n"
          "    return parser_failed ? 1 : 0;\n}\n", c);
    (void)j;
    return ferror(c) ? -1 : 0;
}

int codegen_parser(const Grammar *grammar, const Analysis *analysis,
                   const char *prefix)
{
    char header_name[1024];
    char source_name[1024];
    FILE *h;
    FILE *c;
    const char *header_basename;
    if (make_filename(header_name, sizeof header_name, prefix, ".h") != 0 ||
        make_filename(source_name, sizeof source_name, prefix, ".c") != 0) {
        fprintf(stderr, "erro: prefixo de saida muito longo\n");
        return -1;
    }
    header_basename = strrchr(header_name, '/');
    header_basename = header_basename != NULL ? header_basename + 1 : header_name;
    h = fopen(header_name, "w");
    if (h == NULL) {
        fprintf(stderr, "erro: nao foi possivel criar %s\n", header_name);
        return -1;
    }
    c = fopen(source_name, "w");
    if (c == NULL) {
        fprintf(stderr, "erro: nao foi possivel criar %s\n", source_name);
        fclose(h);
        return -1;
    }
    if (write_header(h, grammar, analysis) != 0 ||
        write_parser(c, grammar, analysis, header_basename) != 0) {
        fprintf(stderr, "erro: falha ao escrever os arquivos gerados\n");
        fclose(h); fclose(c);
        return -1;
    }
    if (fclose(h) != 0 || fclose(c) != 0) return -1;
    printf("Parser gerado: %s e %s\n", source_name, header_name);
    return 0;
}
