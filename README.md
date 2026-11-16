# Gerador de Analisadores Sintáticos — Micro C (TP2)

Ferramenta que recebe uma gramática escrita em EBNF, verifica suas propriedades sintáticas e gera automaticamente o código-fonte de um analisador sintático descendente recursivo em C padrão ANSI (C89/C90).

Segunda etapa do Trabalho de Compiladores I, ministrado pelo professor Amaury Castro Júnior — FACOM/UFMS. O projeto dá continuidade ao TP1: o scanner de Micro C é adaptado para fornecer tokens sob demanda ao parser gerado.

| **Autor**                    | **Matrícula** |
| ---------------------------- | ------------- |
| Pedro Henrique Acioly Amaral | 202119050420  |
| Luísa Carla Ferreira         | 202019050193  |

## O que faz

O gerador lê uma gramática EBNF e executa as etapas necessárias para produzir um parser descendente recursivo:

1. Leitura da seção `TOKENS` e das produções da seção `RULES`.
2. Construção de uma representação interna em árvore (AST da gramática).
3. Verificações semânticas dos símbolos e das produções.
4. Cálculo de anulabilidade, FIRST e FOLLOW.
5. Verificação das condições LL(1), com diagnóstico de conflitos.
6. Geração de `parser.c` e `parser.h`.

Cada não terminal da gramática é traduzido em uma função de análise. O parser gerado consome tokens por demanda por meio de uma função callback, definida na interface do header gerado.

## Requisitos

Para compilar o gerador e executar os testes locais:

- GCC ou outro compilador C compatível com ANSI C89/C90;
- GNU Make.

O Flex é necessário somente para gerar e compilar o scanner Micro C adaptado do TP1 e montar o executável integrado.

A compilação do gerador e dos parsers gerados utiliza:

```bash
-std=c89 -pedantic -Wall -Wextra
```

## Compilação e uso

Na raiz do repositório:

```bash
make
```

Consultar as opções disponíveis:

```bash
./gerador --help
```

Ler e reconstruir a gramática no terminal:

```bash
./gerador --debug examples/expressoes.ebnf
```

Exibir anulabilidade, FIRST e FOLLOW:

```bash
./gerador --sets examples/expressoes.ebnf
```

Gerar os arquivos do parser:

```bash
./gerador --generate build/parser examples/expressoes.ebnf
```

O comando acima produz:

```text
build/parser.c
build/parser.h
```

O parser gerado não incorpora um scanner genérico. Ele recebe tokens de uma função callback. A assinatura e os tipos necessários ficam declarados no `parser.h` gerado.

## Formato EBNF aceito

As gramáticas são organizadas em duas seções: `TOKENS`, para declarar terminais e expressões regulares, e `RULES`, para declarar as produções. A primeira regra de `RULES` é considerada o símbolo inicial.

Exemplo de gramática de expressões aritméticas:

```ebnf
(* Gramática de expressões aritméticas *)
TOKENS
NUM = /[0-9]+/;
ID = /[a-zA-Z_][a-zA-Z0-9_]*/;
SKIP = /[ \t\r\n]+/;

RULES
expr = term { ("+" | "-") term };
term = factor { ("*" | "/") factor };
factor = NUM | ID | "(" expr ")";
```

Construções suportadas nas produções:

| Construção | Sintaxe | Significado |
|---|---|---|
| Alternativa | `α | β` | reconhece `α` ou `β` |
| Sequência | `α β` | reconhece `α` seguida de `β` |
| Opcional | `[ α ]` | reconhece zero ou uma ocorrência |
| Repetição | `{ α }` | reconhece zero ou mais ocorrências |
| Agrupamento | `( α )` | agrupa uma subexpressão |
| Literal | `"while"` ou `'+'` | terminal literal |

Terminais nomeados devem começar com letra maiúscula; não terminais devem começar com letra minúscula. Comentários da gramática usam `(* ... *)`.

**Observação sobre `TOKENS` e `SKIP`:** nesta versão, as expressões regulares são lidas e armazenadas na representação da gramática, mas ainda não são transformadas em AFN/AFD nem em código de scanner. Portanto, o parser gerado utiliza um scanner externo. Para a gramática de Micro C, esse scanner é o Flex adaptado do TP1.

## Verificações implementadas

O gerador verifica, entre outros pontos:

- Não terminais utilizados, mas não definidos.
- Definições duplicadas de não terminais e terminais.
- Terminais nomeados que não foram declarados em `TOKENS`.
- Não terminais inalcançáveis a partir do símbolo inicial (aviso).
- Não terminais improdutivos.
- Recursão à esquerda direta e indireta, inclusive quando há prefixos anuláveis.
- Conflitos LL(1) entre alternativas e condições de seleção de opcionais e repetições.

Anulabilidade, FIRST e FOLLOW são calculados para as regras e para as subexpressões da árvore EBNF. A opção `--sets` apresenta esses conjuntos.

**Tratamento de expressões anuláveis:** o código atual permite opcional cujo corpo é anulável quando a seleção pode ser resolvida por FIRST/FOLLOW, emitindo aviso e adotando escolha gulosa quando há interseção. Repetições cujo corpo é anulável são rejeitadas para evitar uma repetição que não consome tokens. Essa decisão deve ser conferida com o professor, pois o texto do enunciado fornecido na disciplina também descreve a exigência de corpo não anulável para opcionais.

## Integração com o scanner Micro C do TP1

O scanner original do TP1 está no repositório:

https://github.com/pedrohenrique-engcomp/microc-scanner

Uma cópia adaptada da especificação Flex está em `src/scanner/microc.flex`. A interface pública está em `include/scanner.h` e `include/tokens.h`; o adaptador `src/scanner/scanner.c` expõe a operação `scanner_next()`, que entrega um token por chamada. O arquivo `src/scanner/microc_parser_driver.c` converte os tokens do TP1 para os tipos esperados pelo parser gerado para Micro C.

Com Flex instalado, gerar e compilar o executável integrado:

```bash
make microc-parser
```

Executar a análise sintática de um programa:

```bash
./build/microc-parser caminho/para/programa.mc
```

Para construir apenas a biblioteca do scanner:

```bash
make scanner
```

A integração Flex deve ser validada no ambiente de entrega. Na última validação deste projeto, o alvo `make microc-parser` foi tentado, mas não pôde concluir porque o executável `flex` não estava instalado. A compilação isolada da interface C do scanner e do driver foi realizada; isso não equivale a uma validação ponta a ponta do scanner gerado pelo Flex.

## Testes

Executar a bateria local:

```bash
make test
```

A bateria inclui três gramáticas válidas — expressões aritméticas, listas de identificadores e gramática sintática de Micro C —, casos de programas aceitos e rejeitados, gramáticas inválidas e testes de regressão para:

- Recursão à esquerda direta e indireta.
- Alternativas anuláveis e escolha por FIRST/FOLLOW.
- Colisão entre nome de token e literal de mesmo texto, como `ID` e `"ID"`.
- Distinção entre o literal `"$"` e o marcador de fim de arquivo.
- Literais contendo aspas e barra invertida no código C gerado.
- Omissão de regras inalcançáveis no parser gerado.
- Conferência do motivo esperado nos diagnósticos de gramáticas inválidas.

Os testes do parser utilizam callbacks determinísticos, de modo que a bateria local do gerador não depende do Flex. A saída e os arquivos auxiliares de teste são criados em `build/test/`.

A bateria original de 17 testes do código-base da disciplina ainda precisa ser executada para confirmar compatibilidade com os testes oficiais.

## Organização do repositório

```text
include/       interfaces e estruturas compartilhadas
src/
  core/        estruturas da gramática e tipos de tokens
  ebnf/        analisador léxico e sintático da EBNF
  analysis/    verificações semânticas, conjuntos e LL(1)
  codegen/     geração de parser.c e parser.h
  scanner/     scanner Flex adaptado do TP1 e adaptador de tokens
examples/      gramáticas de expressões, listas e Micro C
tests/         gramáticas de teste e testes de execução
Makefile       compilação e execução dos testes
README.md      documentação do projeto
USO_IA.md      registro de utilização de ferramentas de IA
TEST_REPORT.md relatório da validação local
```

## Limitações conhecidas e próximas etapas

Esta versão implementa a leitura da EBNF, a representação da gramática, as verificações semânticas, os conjuntos anulável/FIRST/FOLLOW, a análise LL(1) e a geração do parser descendente recursivo.

Ainda estão pendentes em relação ao enunciado:

- Geração de um analisador léxico próprio para gramáticas arbitrárias, com conversão de expressões regulares em AFN, AFD e código C.
- Opções `--tree` e `--tokens`.
- Validação ponta a ponta da integração com Flex no ambiente de entrega.
- Execução da bateria original de testes fornecida com o código-base da disciplina.

Os extras de recuperação após erro, construção de árvore sintática abstrata, correção automática de conflitos e descrição da própria EBNF em EBNF também não são declarados como implementados nesta versão.

## Limpeza dos arquivos gerados

```bash
make clean
```

## Registro de uso de IA

O arquivo `USO_IA.md` registra as ferramentas de IA utilizadas como apoio, as atividades em que foram empregadas e as limitações da validação. O uso de IA não substitui a revisão, a compreensão nem a responsabilidade dos integrantes sobre o código entregue.ecisa ser recebida e
executada antes de declarar compatibilidade com os testes oficiais.
