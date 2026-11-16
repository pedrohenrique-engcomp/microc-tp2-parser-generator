#include <stdio.h>
#include <string.h>
#include "grammar.h"
#include "ebnf_parser.h"
#include "verificacao.h"
#include "analysis.h"
#include "ll1.h"
#include "codegen.h"

static void usage(const char *program)
{
    fprintf(stderr, "Uso: %s [opcoes] arquivo.ebnf\n", program);
    fprintf(stderr, "  --debug             imprime a gramatica reconstruida\n");
    fprintf(stderr, "  --sets              imprime anulavel, FIRST e FOLLOW\n");
    fprintf(stderr, "  --generate prefixo  gera prefixo.c e prefixo.h\n");
    fprintf(stderr, "  --help              mostra esta ajuda\n");
}

int main(int argc, char **argv)
{
    const char *filename;
    const char *output_prefix;
    int debug;
    int show_sets;
    int i;
    int status;
    int ll1_errors;
    Grammar grammar;
    Analysis analysis;
    char error[256];
    SourceLoc error_loc;

    filename = NULL;
    output_prefix = NULL;
    debug = 0;
    show_sets = 0;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--debug") == 0) debug = 1;
        else if (strcmp(argv[i], "--sets") == 0) show_sets = 1;
        else if (strcmp(argv[i], "--generate") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 2; }
            output_prefix = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]); return 0;
        } else if (filename == NULL) filename = argv[i];
        else { usage(argv[0]); return 2; }
    }
    if (filename == NULL) { usage(argv[0]); return 2; }

    grammar_init(&grammar);
    memset(&analysis, 0, sizeof analysis);
    status = ebnf_parse_file(filename, &grammar, error, sizeof error, &error_loc);
    if (status != 0) {
        fprintf(stderr, "%s:%d:%d: erro de leitura EBNF: %s\n",
                filename, error_loc.linha, error_loc.coluna, error);
        grammar_destroy(&grammar);
        return 1;
    }
    if (debug) {
        puts("[debug] gramatica reconstruida a partir da AST:");
        grammar_print(&grammar);
    }
    status = grammar_verify(&grammar);
    if (status != 0) {
        fprintf(stderr, "A gramatica possui %d erro(s) semantico(s); processamento interrompido.\n",
                status);
        grammar_destroy(&grammar);
        return 1;
    }
    if (analysis_compute(&grammar, &analysis) != 0) {
        fprintf(stderr, "erro: memoria insuficiente ao calcular anulavel/FIRST/FOLLOW\n");
        grammar_destroy(&grammar);
        return 1;
    }
    if (show_sets) analysis_print(&grammar, &analysis);

    ll1_errors = grammar_check_ll1(&grammar, &analysis);
    if (ll1_errors != 0) {
        analysis_destroy(&analysis);
        grammar_destroy(&grammar);
        return 1;
    }
    printf("Gramatica valida: %lu regra(s), %lu terminal(is).\n",
           (unsigned long)grammar.rule_count,
           (unsigned long)(analysis.terminal_count - 1));
    if (output_prefix != NULL) {
        if (codegen_parser(&grammar, &analysis, output_prefix) != 0) {
            analysis_destroy(&analysis);
            grammar_destroy(&grammar);
            return 1;
        }
    }
    analysis_destroy(&analysis);
    grammar_destroy(&grammar);
    return 0;
}
