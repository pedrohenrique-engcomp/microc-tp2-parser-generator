#ifndef EBNF_PARSER_H
#define EBNF_PARSER_H

#include "ebnf_lexer.h"
#include "grammar.h"

int ebnf_parse_file(const char *filename, Grammar *grammar,
                    char *error, size_t error_size, SourceLoc *error_loc);

#endif
