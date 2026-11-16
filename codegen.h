#ifndef CODEGEN_H
#define CODEGEN_H

#include "analysis.h"

/* Gera <prefix>.c e <prefix>.h. Retorna 0 em sucesso. */
int codegen_parser(const Grammar *grammar, const Analysis *analysis,
                   const char *prefix);

#endif
