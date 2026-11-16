/*
 * NOME = PEDRO HENRIQUE ACIOLY AMARAL  | MATRICULA = 202119050420
 * NOME = LUÍSA CARLA FERREIRA          | MATRICULA = 202019050193
 * microc.flex
 *
 * Scanner Micro C adaptado do TP1 para ser consumido pelo parser do TP2.
 * A especificacao de reconhecimento foi preservada; o main de depuracao
 * foi removido e a interface publica esta em include/scanner.h.
 *
 * Compilacao via Makefile do TP2: flex -> codigo C -> GCC ANSI C89.
 *
 * =====================================================================
 * DESCRIÇÃO
 * Acrescimos delimitados por:  >>>>> INICIO Tn/Cn ... <<<<< FIM Tn/Cn
 *
 *   T1  palavras reservadas    tabela estatica + strcmp apos o casamento
 *   T2  inteiros com sinal     token anterior via YY_DECL + yyless(1)
 *   T3  constantes de char     padrao com escape + padrao de erro
 *   T4  constantes de string   condicao %x STRING + buffer de montagem
 *   T5  operadores com prefixo regras separadas (casamento mais longo)
 *
 *   C1  <<EOF>> sem lista abrange todas as condicoes: escopo <INITIAL>
 *   C2  <COMMENT><<EOF>> sem BEGIN(INITIAL): laco infinito no main()
 *   C3  main() exibia yytext no lugar do valor semantico
 * =====================================================================
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
/* Tipos e interface interna compartilhados com scanner.c. */
#include "scanner_internal.h"

ScannerValue microc_yylval;
int linha_atual = 1;
int linha_token = 1;
int coluna_atual = 1;
int coluna_token = 1;
static int string_linha_inicio = 1;
static int string_coluna_inicio = 1;
extern char *yytext;
extern int yyleng;

static char *scanner_dup(const char *text) {
    size_t length;
    char *copy;
    if (text == NULL) return NULL;
    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (copy != NULL) memcpy(copy, text, length + 1);
    return copy;
}

static void scanner_update_position(void) {
    int i;
    linha_token = linha_atual;
    coluna_token = coluna_atual;
    for (i = 0; i < yyleng; ++i) {
        if (yytext[i] == '\n') {
            linha_atual++;
            coluna_atual = 1;
        } else {
            coluna_atual++;
        }
    }
}

#define YY_USER_ACTION scanner_update_position();

/* Funcao auxiliar para preencher microc_yylval.symbol com uma copia do
 * texto reconhecido (yytext). Sinta-se livre para usar/adaptar. */
static void guarda_lexema(void) {
    microc_yylval.symbol = scanner_dup(yytext);
}

/* T2 -- renomeia o scanner gerado; o involucro yylex()
 * (secao 4) registra o token anterior sem alterar regra alguma. */
#define YY_DECL int scanner_interno(void)
int scanner_interno(void);
/*T2*/

/* T1 -- tabela consultada apos o casamento de
 * {LETRA}{ALFANUM}*, que reconhece ID e reservada indistintamente. */
static TokenType classifica_identificador(const char *lexema) {
    static const struct { const char *texto; TokenType token; } reservadas[] = {
        {"main", MAIN},   {"if",   IF},   {"else",  ELSE},
        {"for",  FOR},    {"return", RETURN},
        {"int",  INT},    {"char", CHAR}, {"print", PRINT}
    };
    int i;
    int n = (int)(sizeof reservadas / sizeof reservadas[0]);

    for (i = 0; i < n; i++)                      /* busca sequencial */
        if (strcmp(lexema, reservadas[i].texto) == 0)
            return reservadas[i].token;

    return ID;
}
/*T1*/

/*T2 -- contexto a esquerda: o menos e binario quando o
 * token anterior pode encerrar uma expressao. */
static TokenType token_anterior = UNDEF;

static int menos_e_unario(void) {
    switch (token_anterior) {
        case ID: case INTEGERCONST: case CHARCONST:
        case RPAREN: case RBRACKET:
            return 0;                            /* binario */
        default:
            return 1;                            /* unario  */
    }
}
/*T2*/

/*T3 -- traducao de escapes, compartilhada com T4. */
static char traduz_escape(char indicador) {
    switch (indicador) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '0':  return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"':  return '"';
        default:   return indicador;
    }
}
/*T3*/

/*T4 -- buffer de montagem da string; o indicador de
 * estouro evita truncamento silencioso. */
#define TAM_MAX_STRING 4096

static char string_buffer[TAM_MAX_STRING];
static int  string_tamanho;
static int  string_estourou;

static void buffer_reinicia(void) {
    string_tamanho  = 0;
    string_estourou = 0;
}

static void buffer_acrescenta(char c) {
    if (string_tamanho < TAM_MAX_STRING - 1)
        string_buffer[string_tamanho++] = c;
    else
        string_estourou = 1;
}

static char *buffer_finaliza(void) {
    string_buffer[string_tamanho] = '\0';
    return scanner_dup(string_buffer);
}
/*T4*/

%}

/* -----------------------------------------------------------------------
 * 2. SECAO DE DEFINICOES
 * ------------------------------------------------------------------- */

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%x COMMENT

 /*T4 -- mesmo desenho de COMMENT; separa os tres erros. */
%x STRING
 /*T4*/

%%

 /* -----------------------------------------------------------------------
  * 3. SECAO DE REGRAS
  * --------------------------------------------------------------------- */

 /* --- Fim de arquivo -----------------------------------------------------
  * Tratada explicitamente (em vez de depender do retorno automatico 0
  * do flex), pois o token UNDEF tambem vale 0 no enum TokenType -- se
  * dependessemos do comportamento padrao, um erro lexico seria
  * confundido com o fim do arquivo pelo main() de teste abaixo. */

 /* [EDITADO] original era : <<EOF>> { return END_OF_FILE; }
  * Sem lista de condicoes a regra abrange todas e anula as de COMMENT
  * e STRING ("multiple <<EOF>> rules"). */
<INITIAL><<EOF>>    { return END_OF_FILE; }


 /* --- Espacos em branco e quebras de linha ---------------------------- */
\n                  { /* posicao atualizada por YY_USER_ACTION */ }
[ \t\r]+            { /* ignora espacos em branco */ }

 /* --- Comentarios ------------------------------------------------------
  * Estes ja estao implementados como exemplo de uso de estados (%x) e
  * de tratamento de erro via EOF dentro de um estado especial. */
"//".*              { /* comentario de linha: ignora ate o fim da linha */ }

"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { /* posicao atualizada por YY_USER_ACTION */ }
<COMMENT><<EOF>>    {
 /* [EDITADA] sem isto o main() reincide nesta regra em laco. */
                        BEGIN(INITIAL);

                        microc_yylval.error_msg = scanner_dup("EOF em comentario");
                        return UNDEF;
                    }
<COMMENT>.          { /* consome qualquer outro caractere dentro do comentario */ }

 /* Fechamento de comentario sem abertura correspondente. */
"*/"                {
                        microc_yylval.error_msg = scanner_dup("Comentario nao iniciado");
                        return UNDEF;
                    }

 /* --- Palavras reservadas e identificadores ----------------------------
  * O Flex reconhece o lexema completo; a tabela classifica palavras
  * reservadas e conserva o atributo apenas para identificadores. */
{LETRA}{ALFANUM}*   {
 /* T1 -- so ID carrega valor semantico. */
                        TokenType t = classifica_identificador(yytext);
                        if (t == ID) guarda_lexema();
                        return t;
 /*T1*/
                    }

 /* --- Constantes inteiras -----------------------------------------------
  * TODO(aluno): o padrao formal para um inteiro em Micro C e um ou mais
  * digitos, opcionalmente precedidos de um sinal de menos (numeros
  * negativos). A regra abaixo trata apenas inteiros sem sinal; use a
  * tecnica de lookahead discutida em aula (veja o operador MINUS mais
  * abaixo) para decidir quando um '-' faz parte do numero e quando ele
  * e, na verdade, o operador de subtracao. */

 /* T2 -- vence "-" pelo casamento mais longo; sendo
  * binario, yyless(1) devolve os digitos a entrada. */
-{DIGIT}+           {
                        int tamanho_casado = yyleng;
                        if (!menos_e_unario()) {
                            yyless(1);
                            coluna_atual -= tamanho_casado - 1;
                            return MINUS;
                        }
                        guarda_lexema();
                        return INTEGERCONST;
                    }
 /*T2*/

{DIGIT}+            {
                        guarda_lexema();
                        return INTEGERCONST;
                    }

 /* --- Constantes de caractere --------------------------------------------
  * Reconhece um caractere ou escape e sinaliza constantes malformadas. */

 /*T3 -- aspa, um caractere ou um escape, aspa. */
'([^'\\\n]|\\[nt0r\\'\"])'  {
                        /* yytext[1]: conteudo; se for barra, o indicador
                         * esta em yytext[2]. */
                        char valor = (yytext[1] == '\\')
                                   ? traduz_escape(yytext[2])
                                   : yytext[1];
                        char lexema[2];
                        lexema[0] = valor;
                        lexema[1] = '\0';
                        microc_yylval.symbol = scanner_dup(lexema);
                        return CHARCONST;
                    }

 /* Nao fechada, ou mais de um caractere. Empata com o padrao acima em
  * 'a' e perde por precedencia textual. */
'[^'\n]*'?          {
                        microc_yylval.error_msg = scanner_dup("Constante de caractere invalida");
                        return UNDEF;
                    }
 /*T3*/

 /* --- Constantes de string -------------------------------------------
  * TODO(aluno): reconhecer o padrao "[^"\n]*" (uma ou mais aspas
  * duplas delimitando o conteudo da string) e devolver STRINGCONST.
  * Voce deve tratar os seguintes erros (veja o enunciado, Secao 4.1):
  *   - EOF antes do fechamento da string ("EOF em string")
  *   - quebra de linha nao escapada dentro da string
  *     ("String nao terminada")
  *   - caractere nulo dentro da string
  *     ("String contem caractere nulo")
  * Alem disso, converta as sequencias de escape (\n, \t, \\, \", \0)
  * para os caracteres correspondentes antes de armazenar o lexema. */

 /*T4 -- entrada, saida, escapes, tres erros, coringa. */
\"                  {
                        string_linha_inicio = linha_token;
                        string_coluna_inicio = coluna_token;
                        BEGIN(STRING);
                        buffer_reinicia();
                    }

<STRING>\"          {
                        linha_token = string_linha_inicio;
                        coluna_token = string_coluna_inicio;
                        BEGIN(INITIAL);
                        if (string_estourou) {
                            microc_yylval.error_msg = scanner_dup(
                                "String excede o tamanho maximo");
                            return UNDEF;
                        }
                        microc_yylval.symbol = buffer_finaliza();
                        return STRINGCONST;
                    }

 /* "\\n" casa barra + letra n; "\n" casaria quebra de linha. */
<STRING>\\n         { buffer_acrescenta(traduz_escape('n'));  }
<STRING>\\t         { buffer_acrescenta(traduz_escape('t'));  }
<STRING>\\\\        { buffer_acrescenta(traduz_escape('\\')); }
<STRING>\\\"        { buffer_acrescenta(traduz_escape('"'));  }

<STRING>\\0         {
                        linha_token = string_linha_inicio;
                        coluna_token = string_coluna_inicio;
                        BEGIN(INITIAL);
                        microc_yylval.error_msg = scanner_dup("String contem caractere nulo");
                        return UNDEF;
                    }

<STRING>\n          {
                        /* A quebra de linha ja foi contabilizada pelo hook. */
                        linha_token = string_linha_inicio;
                        coluna_token = string_coluna_inicio;
                        BEGIN(INITIAL);
                        microc_yylval.error_msg = scanner_dup("String nao terminada");
                        return UNDEF;
                    }

<STRING><<EOF>>     {
                        linha_token = string_linha_inicio;
                        coluna_token = string_coluna_inicio;
                        BEGIN(INITIAL);
                        microc_yylval.error_msg = scanner_dup("EOF em string");
                        return UNDEF;
                    }

<STRING>.           { buffer_acrescenta(yytext[0]); }
 /*T4*/

 /* --- Operadores relacionais e logicos ---------------------------------
  * O caso de '=' esta implementado como EXEMPLO do uso de lookahead
  * (yytext mostra o que foi casado; voce pode usar input()/unput() ou,
  * de forma mais simples em flex, escrever as duas alternativas como
  * regras separadas, deixando o proprio flex escolher o casamento mais
  * longo -- veja a explicacao na Secao 2 do enunciado). */
"=="                { return EQ; }
"="                 { return ASSIGN; }

 /* TODO(aluno): complete os demais operadores que compartilham prefixo,
  * seguindo o mesmo padrao do exemplo acima:
  *   !=  e  !      ->  NEQ, NOT
  *   <=  e  <      ->  LEQ, LT
  *   >=  e  >      ->  GEQ, GT
  *   &&             ->  AND
  *   ||             ->  OR
  */

 /*T5 -- casamento mais longo resolve cada par; "&" e "|"
  * isolados caem no padrao "." e viram UNDEF. */
"!="                { return NEQ; }
"!"                 { return NOT; }
"<="                { return LEQ; }
"<"                 { return LT;  }
">="                { return GEQ; }
">"                 { return GT;  }
"&&"                { return AND; }
"||"                { return OR;  }
 /*T5*/

 /* --- Operadores aritmeticos e simbolos de pontuacao (ja prontos) ------ */
"+"                 { return PLUS; }
"-"                 { return MINUS; }
"*"                 { return MUL; }
"/"                 { return DIV; }
"%"                 { return MOD; }
";"                 { return SEMICOLON; }
","                 { return COMMA; }
"("                 { return LPAREN; }
")"                 { return RPAREN; }
"{"                 { return LBRACE; }
"}"                 { return RBRACE; }
"["                 { return LBRACKET; }
"]"                 { return RBRACKET; }

 /* --- Caractere invalido -------------------------------------------------
  * Casa com qualquer caractere que nao tenha correspondido a nenhuma
  * regra anterior. Deve ser SEMPRE a ultima regra do arquivo. */
.                   {
                        microc_yylval.error_msg = scanner_dup(yytext);
                        return UNDEF;
                    }

%%

/* -----------------------------------------------------------------------
 * 4. SUB-ROTINAS DO USUARIO
 * ------------------------------------------------------------------- */

/* yywrap: informa ao flex que, ao atingir o EOF, a leitura deve
 * simplesmente parar (nao ha um proximo arquivo a processar). */
int yywrap(void) {
    return 1;
}

/*T2 -- preserva a interface yylex() e registra o token
 * devolvido, consultado por menos_e_unario() na chamada seguinte. */
int yylex(void) {
    int tipo = scanner_interno();
    token_anterior = (TokenType) tipo;
    return tipo;
}
/*T2*/

/* Reinicia o contexto entre arquivos analisados no mesmo processo. */
void scanner_reset_context(void) {
    token_anterior = UNDEF;
    linha_atual = 1;
    linha_token = 1;
    coluna_atual = 1;
    coluna_token = 1;
    string_linha_inicio = 1;
    string_coluna_inicio = 1;
    microc_yylval.symbol = NULL;
    microc_yylval.error_msg = NULL;
    BEGIN(INITIAL);
}
