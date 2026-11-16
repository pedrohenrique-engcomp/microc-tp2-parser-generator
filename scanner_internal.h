#ifndef SCANNER_INTERNAL_H
#define SCANNER_INTERNAL_H

#include <stdio.h>

typedef struct {
    char *symbol;
    char *error_msg;
} ScannerValue;

extern ScannerValue microc_yylval;
extern int linha_atual;
extern int coluna_atual;
extern int linha_token;
extern int coluna_token;
extern FILE *yyin;
extern char *yytext;

int yylex(void);
void yyrestart(FILE *input_file);
void scanner_reset_context(void);

#endif
