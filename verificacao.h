#ifndef VERIFICACAO_H
#define VERIFICACAO_H

#include "grammar.h"

/* Retorna 0 se nao houver erros semanticos; avisos nao alteram o retorno. */
int grammar_verify(Grammar *grammar);

#endif
