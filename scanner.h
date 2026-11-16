#ifndef SCANNER_H
#define SCANNER_H

#include "tokens.h"

/* Abre a entrada Micro C. Retorna 0 em sucesso e -1 em erro. */
int scanner_init(const char *nome_arquivo);

/* Entrega exatamente um token por chamada. UNDEF representa erro léxico. */
int scanner_next(Token *token);

/* Fecha a entrada e libera o estado mantido pelo scanner. */
void scanner_close(void);

#endif
