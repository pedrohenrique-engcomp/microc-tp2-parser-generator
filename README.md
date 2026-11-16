# TP2 — Gerador de analisadores sintáticos descendentes recursivos

Implementação própria em C89/C90 de um gerador que lê gramáticas EBNF,
verifica propriedades semânticas e LL(1), calcula anulável/FIRST/FOLLOW e
gera um parser descendente recursivo em C.

## Requisitos

- GCC ou compilador C compatível com C89;
- `make`;
- `flex` somente para construir o scanner Micro C reaproveitado do TP1 e o
  executável final integrado.

O gerador e os parsers gerados são compilados com:

```sh
-std=c89 -pedantic -Wall -Wextra
```

## Compilar e executar o gerador

```sh
make
./gerador --help
./gerador --debug examples/expressoes.ebnf
./gerador --sets examples/expressoes.ebnf
./gerador --generate build/parser examples/expressoes.ebnf
```

A última opção produz `build/parser.c` e `build/parser.h`. O parser gerado
recebe tokens por demanda através da função callback `ParserNextTokenFn`.
O contrato está declarado no header gerado.

## Testes automatizados

```sh
make test
```

A bateria automatizada executa:

- três gramáticas válidas: expressões aritméticas, listas de identificadores
  e a gramática sintática de Micro C;
- nove entradas aceitas e nove rejeitadas (três de cada gramática);
- doze gramáticas inválidas; cada caso confere também o motivo esperado da
  rejeição, incluindo recursão direta/indireta e conflito envolvendo literal;
- quatro casos adicionais de execução para uma alternativa anulável, com
  escolha correta por FIRST ou FOLLOW;
- casos para opcionais cujo corpo é anulável (permitidos, com teste de
  escolha gulosa) e repetições cujo corpo é anulável (rejeitadas por risco de
  laço sem consumo);
- casos de colisão entre token nomeado e literal de mesmo texto (`ID` e
  `"ID"`), além de literal `"$"` separado do marcador de fim de arquivo;
- geração e compilação dos parsers em C89 estrito, incluindo testes de escape
  de aspas/barra invertida e omissão de regras inalcançáveis;
- compilação C89 da interface do scanner e da ponte entre o scanner TP1 e o
  parser gerado.

Os testes de parser usam callbacks de scanner determinísticos, para que a
bateria possa ser executada mesmo em ambientes sem Flex. Os casos rejeitados
produzem diagnósticos esperados, capturados em `build/test/*_errors.txt`.

A identidade interna de terminal agora é o par **(nome, categoria)**. Assim, o token nomeado `ID` não colide com o literal `"ID"`, e um literal `"$"` não se confunde com o marcador interno de fim de arquivo. Opcionais com corpo anulável são permitidos quando a análise preditiva consegue decidir por FIRST/FOLLOW; repetições com corpo anulável continuam sendo erro, pois poderiam não consumir entrada e formar um laço infinito.

**Observação sobre a regra de opcional anulável:** esta implementação segue a avaliação técnica mais recente que solicitou permitir `[ α ]` quando `α` é anulável, desde que a escolha por FIRST/FOLLOW seja segura. O texto do enunciado transcrito anteriormente na conversa, porém, afirma que o corpo de `[ α ]` e `{ α }` não deve ser anulável. Há uma divergência entre essas duas orientações; confirmem com o professor qual regra deve prevalecer na correção. Para `{ α }`, o corpo anulável continua sendo rejeitado para evitar um `while` sem consumo.

## Integração com o scanner Micro C do TP1

A especificação Flex adaptada está em `src/scanner/microc.flex`. A interface
pública está em `include/scanner.h` e `include/tokens.h`; a função
`scanner_next()` devolve um `Token` contendo tipo, lexema, linha, coluna e
mensagem léxica. `src/scanner/microc_parser_driver.c` converte os tipos do
TP1 nos tipos do header gerado pelo parser Micro C.

Com Flex instalado, construa o executável integrado:

```sh
make microc-parser
./build/microc-parser caminho/para/programa.mc
```

`make scanner` constrói somente `build/libmicrocscanner.a`.
`make microc-parser` também gera o parser a partir de
`examples/microc.ebnf`, compila a ponte e liga tudo ao scanner Flex.

**Limitação de validação deste pacote:** o ambiente em que esta versão foi
montada não disponibiliza o executável Flex. A tentativa de executar
`make microc-parser` terminou com `make: flex: No such file or directory`.
Portanto `make test` valida a implementação do gerador, a geração/execução dos
parsers com scanners de teste e a compilação C89 da ponte, mas a geração do
scanner Flex e a ligação/execução final com arquivos `.mc` ainda precisam ser
executadas em uma máquina com Flex instalado.

## Formato EBNF suportado

```ebnf
TOKENS
NUM = /[0-9]+/;
ID = /[a-zA-Z_][a-zA-Z0-9_]*/;
SKIP = /[ \t\r\n]+/;

RULES
expr = term { ("+" | "-") term };
term = factor { ("*" | "/") factor };
factor = NUM | ID | "(" expr ")";
```

- A primeira regra em `RULES` é o símbolo inicial.
- Terminais nomeados começam com maiúscula; não terminais começam com
  minúscula.
- Literais entre aspas são terminais fixos.
- `SKIP` é reconhecido e armazenado como diretiva léxica da gramática, mas
  esta versão não gera um analisador léxico a partir das expressões regulares.
  O parser gerado consome tokens de um scanner externo. Para Micro C, esse
  scanner externo é o Flex adaptado do TP1.
- Comentários de gramática usam `(* ... *)`.

## Organização

```text
include/       interfaces públicas e estruturas
src/core/      estrutura da gramática e tokens
src/ebnf/      lexer e parser da notação EBNF
src/analysis/  verificações semânticas, conjuntos e LL(1)
src/codegen/   geração de parser.c/parser.h
src/scanner/   adaptação do scanner Flex do TP1 e ponte para o parser
examples/      gramáticas de expressões, listas e Micro C
tests/         testes de aceitação/rejeição e gramáticas inválidas
```

## Limpeza

```sh
make clean
```

## Registro de uso de IA e pendências do enunciado

O arquivo `USO_IA.md` registra o apoio de IA utilizado, as atividades em que
foi empregado e os limites da validação realizada. Esta versão ainda não gera
automaticamente um scanner para gramáticas arbitrárias a partir de `TOKENS` e
`SKIP`, e não implementa as opções `--tree` e `--tokens` citadas no enunciado.
A bateria original do código-base da disciplina também precisa ser recebida e
executada antes de declarar compatibilidade com os testes oficiais.
