#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
#include "scanner_internal.h"

static FILE *arquivo_scanner = NULL;

static void copy_field(char *destination, size_t capacity, const char *source)
{
    size_t length;
    if (capacity == 0) return;
    if (source == NULL) source = "";
    length = strlen(source);
    if (length >= capacity) length = capacity - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

int scanner_init(const char *nome_arquivo)
{
    if (nome_arquivo == NULL) return -1;
    if (arquivo_scanner != NULL) {
        fclose(arquivo_scanner);
        arquivo_scanner = NULL;
    }
    arquivo_scanner = fopen(nome_arquivo, "r");
    if (arquivo_scanner == NULL) return -1;
    yyrestart(arquivo_scanner);
    scanner_reset_context();
    return 0;
}

int scanner_next(Token *token)
{
    int tipo;
    const char *lexema;
    if (token == NULL) return UNDEF;
    memset(token, 0, sizeof *token);
    tipo = yylex();
    token->tipo = (TokenType)tipo;
    if (tipo == END_OF_FILE) {
        token->linha = linha_atual;
        token->coluna = coluna_atual;
    } else {
        token->linha = linha_token;
        token->coluna = coluna_token;
    }

    if (tipo == UNDEF) {
        copy_field(token->mensagem, sizeof token->mensagem,
                   microc_yylval.error_msg != NULL ?
                   microc_yylval.error_msg : "erro lexico nao especificado");
    } else if (tipo != END_OF_FILE) {
        lexema = microc_yylval.symbol != NULL ? microc_yylval.symbol : yytext;
        if (lexema != NULL && strlen(lexema) >= sizeof token->lexema) {
            token->tipo = UNDEF;
            copy_field(token->mensagem, sizeof token->mensagem,
                       "lexema excede o limite de armazenamento do token");
        } else {
            copy_field(token->lexema, sizeof token->lexema, lexema);
        }
    }

    free(microc_yylval.symbol);
    free(microc_yylval.error_msg);
    microc_yylval.symbol = NULL;
    microc_yylval.error_msg = NULL;
    return (int)token->tipo;
}

void scanner_close(void)
{
    if (arquivo_scanner != NULL) {
        fclose(arquivo_scanner);
        arquivo_scanner = NULL;
    }
    yyin = NULL;
}
