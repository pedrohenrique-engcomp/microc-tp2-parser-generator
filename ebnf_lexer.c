#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ebnf_lexer.h"

static void lexer_advance(EbnfLexer *lexer)
{
    if (lexer->position >= lexer->length) return;
    if (lexer->source[lexer->position] == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    lexer->position++;
}

static char *copy_text(const char *text, size_t length)
{
    char *result = (char *)malloc(length + 1);
    if (result == NULL) return NULL;
    memcpy(result, text, length);
    result[length] = '\0';
    return result;
}

static int append_char(char **buffer, size_t *length, size_t *capacity,
                       char value)
{
    char *next;
    size_t new_capacity;
    if (*length + 1 >= *capacity) {
        new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        next = (char *)realloc(*buffer, new_capacity);
        if (next == NULL) return -1;
        *buffer = next;
        *capacity = new_capacity;
    }
    (*buffer)[(*length)++] = value;
    (*buffer)[*length] = '\0';
    return 0;
}

static char decode_escape(char value)
{
    switch (value) {
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case '0': return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    default: return value;
    }
}

static int lexer_fail(EbnfLexer *lexer, SourceLoc loc, const char *message)
{
    lexer->error_loc = loc;
    strncpy(lexer->error, message, sizeof lexer->error - 1);
    lexer->error[sizeof lexer->error - 1] = '\0';
    return -1;
}

static int skip_space_and_comments(EbnfLexer *lexer)
{
    int again;
    SourceLoc start;
    do {
        again = 0;
        while (lexer->position < lexer->length &&
               isspace((unsigned char)lexer->source[lexer->position]))
            lexer_advance(lexer);
        if (lexer->position + 1 < lexer->length &&
            lexer->source[lexer->position] == '(' &&
            lexer->source[lexer->position + 1] == '*') {
            start.linha = lexer->line;
            start.coluna = lexer->column;
            lexer_advance(lexer);
            lexer_advance(lexer);
            while (lexer->position + 1 < lexer->length &&
                   !(lexer->source[lexer->position] == '*' &&
                     lexer->source[lexer->position + 1] == ')'))
                lexer_advance(lexer);
            if (lexer->position + 1 >= lexer->length)
                return lexer_fail(lexer, start, "comentario EBNF nao terminado");
            lexer_advance(lexer);
            lexer_advance(lexer);
            again = 1;
        }
    } while (again);
    return 0;
}

int ebnf_lexer_open(EbnfLexer *lexer, const char *filename)
{
    FILE *file;
    long length;
    size_t read_count;
    memset(lexer, 0, sizeof *lexer);
    lexer->line = 1;
    lexer->column = 1;
    lexer->error_loc.linha = 1;
    lexer->error_loc.coluna = 1;
    file = fopen(filename, "rb");
    if (file == NULL) {
        strncpy(lexer->error, "nao foi possivel abrir o arquivo", sizeof lexer->error - 1);
        return -1;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        strncpy(lexer->error, "nao foi possivel ler o arquivo", sizeof lexer->error - 1);
        return -1;
    }
    lexer->source = (char *)malloc((size_t)length + 1);
    if (lexer->source == NULL) {
        fclose(file);
        strncpy(lexer->error, "memoria insuficiente", sizeof lexer->error - 1);
        return -1;
    }
    read_count = fread(lexer->source, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(lexer->source);
        lexer->source = NULL;
        strncpy(lexer->error, "falha durante a leitura do arquivo", sizeof lexer->error - 1);
        return -1;
    }
    lexer->source[read_count] = '\0';
    lexer->length = read_count;
    lexer->line = 1;
    lexer->column = 1;
    lexer->filename = filename;
    return 0;
}

void ebnf_lexer_close(EbnfLexer *lexer)
{
    free(lexer->source);
    memset(lexer, 0, sizeof *lexer);
}

void ebnf_token_destroy(EbnfToken *token)
{
    free(token->text);
    token->text = NULL;
}

const char *ebnf_token_name(EbnfTokenKind kind)
{
    switch (kind) {
    case EBNF_EOF: return "fim de arquivo";
    case EBNF_IDENT: return "identificador";
    case EBNF_STRING: return "literal";
    case EBNF_REGEX: return "expressao regular";
    case EBNF_EQUAL: return "'='";
    case EBNF_SEMICOLON: return "';'";
    case EBNF_PIPE: return "'|'";
    case EBNF_LBRACKET: return "'['";
    case EBNF_RBRACKET: return "']'";
    case EBNF_LBRACE: return "'{'";
    case EBNF_RBRACE: return "'}'";
    case EBNF_LPAREN: return "'('";
    case EBNF_RPAREN: return "')'";
    default: return "token invalido";
    }
}

int ebnf_lexer_next(EbnfLexer *lexer, EbnfToken *token)
{
    char c;
    size_t start;
    char quote;
    char *buffer;
    size_t length;
    size_t capacity;
    int escaped;

    memset(token, 0, sizeof *token);
    if (skip_space_and_comments(lexer) != 0) return -1;
    token->loc.linha = lexer->line;
    token->loc.coluna = lexer->column;
    if (lexer->position >= lexer->length) {
        token->kind = EBNF_EOF;
        return 0;
    }
    c = lexer->source[lexer->position];
    if (isalpha((unsigned char)c) || c == '_') {
        start = lexer->position;
        while (lexer->position < lexer->length) {
            c = lexer->source[lexer->position];
            if (!isalnum((unsigned char)c) && c != '_') break;
            lexer_advance(lexer);
        }
        token->kind = EBNF_IDENT;
        token->text = copy_text(lexer->source + start, lexer->position - start);
        return token->text == NULL ? lexer_fail(lexer, token->loc, "memoria insuficiente") : 0;
    }
    if (c == '/' ) {
        lexer_advance(lexer);
        buffer = NULL; length = 0; capacity = 0; escaped = 0;
        while (lexer->position < lexer->length) {
            c = lexer->source[lexer->position];
            if (c == '/' && !escaped) break;
            if (append_char(&buffer, &length, &capacity, c) != 0) {
                free(buffer);
                return lexer_fail(lexer, token->loc, "memoria insuficiente");
            }
            if (c == '\\' && !escaped) escaped = 1;
            else escaped = 0;
            lexer_advance(lexer);
        }
        if (lexer->position >= lexer->length) {
            free(buffer);
            return lexer_fail(lexer, token->loc, "expressao regular sem '/' final");
        }
        lexer_advance(lexer);
        if (buffer == NULL) buffer = copy_text("", 0);
        token->kind = EBNF_REGEX;
        token->text = buffer;
        return token->text == NULL ? lexer_fail(lexer, token->loc, "memoria insuficiente") : 0;
    }
    if (c == '\'' || c == '"') {
        quote = c;
        lexer_advance(lexer);
        buffer = NULL; length = 0; capacity = 0;
        while (lexer->position < lexer->length &&
               lexer->source[lexer->position] != quote) {
            c = lexer->source[lexer->position];
            if (c == '\n' || c == '\r') {
                free(buffer);
                return lexer_fail(lexer, token->loc, "literal EBNF atravessa linha sem escape");
            }
            lexer_advance(lexer);
            if (c == '\\') {
                if (lexer->position >= lexer->length) {
                    free(buffer);
                    return lexer_fail(lexer, token->loc, "escape incompleto em literal");
                }
                c = decode_escape(lexer->source[lexer->position]);
                lexer_advance(lexer);
            }
            if (append_char(&buffer, &length, &capacity, c) != 0) {
                free(buffer);
                return lexer_fail(lexer, token->loc, "memoria insuficiente");
            }
        }
        if (lexer->position >= lexer->length) {
            free(buffer);
            return lexer_fail(lexer, token->loc, "literal EBNF sem fechamento");
        }
        lexer_advance(lexer);
        if (buffer == NULL) buffer = copy_text("", 0);
        token->kind = EBNF_STRING;
        token->text = buffer;
        return token->text == NULL ? lexer_fail(lexer, token->loc, "memoria insuficiente") : 0;
    }
    lexer_advance(lexer);
    switch (c) {
    case '=': token->kind = EBNF_EQUAL; break;
    case ';': token->kind = EBNF_SEMICOLON; break;
    case '|': token->kind = EBNF_PIPE; break;
    case '[': token->kind = EBNF_LBRACKET; break;
    case ']': token->kind = EBNF_RBRACKET; break;
    case '{': token->kind = EBNF_LBRACE; break;
    case '}': token->kind = EBNF_RBRACE; break;
    case '(': token->kind = EBNF_LPAREN; break;
    case ')': token->kind = EBNF_RPAREN; break;
    default:
        token->kind = EBNF_INVALID;
        return lexer_fail(lexer, token->loc, "caractere inesperado na EBNF");
    }
    return 0;
}
