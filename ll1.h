#ifndef LL1_H
#define LL1_H

#include "analysis.h"

/* Retorna quantidade de erros LL(1). Conflitos em opcional/repeticao
 * sao avisos e nao impedem a geracao gulosa do parser. */
int grammar_check_ll1(const Grammar *grammar, const Analysis *analysis);

#endif
