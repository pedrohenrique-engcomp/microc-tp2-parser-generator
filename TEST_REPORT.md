# Relatório de validação — TP2 Parser Generator

**Data desta validação:** 16/11/2026  
**Compilador:** GCC 14.2.0  
**Sistema de build:** GNU Make 4.4.1  
**Padrão exigido:** C89/C90 (`-std=c89 -pedantic`)  
**Avisos tratados como erros:** `-Wall -Wextra -Werror`

## 1. Compilação do gerador

Comando executado:

```sh
make clean
make CFLAGS='-std=c89 -pedantic -Wall -Wextra -Werror -O2' -j2
```

**Resultado:** compilação concluída sem avisos ou erros.

## 2. Testes funcionais

Comando executado:

```sh
make test
```

**Resultado:** todos os testes da bateria local passaram.

| Verificação | Resultado |
|---|---:|
| Gramáticas válidas principais | 3 |
| Entradas aceitas | 9 |
| Entradas rejeitadas pelos parsers | 9 |
| Casos adicionais de alternativa anulável (FIRST/FOLLOW) | 4 |
| Casos de opcional anulável | 3 + escolha gulosa |
| Casos de colisão entre token/literal/EOF | 3 |
| Gramáticas inválidas com motivo esperado conferido | 12 |
| Compilação dos parsers gerados em C89 com `-Werror` | Passou |
| Compilação C89 da ponte e do adaptador do scanner | Passou |
| GCC `-fanalyzer` nos módulos C | Sem avisos |
| AddressSanitizer + UBSan com leak detection | Passou |

A bateria local cobre expressões aritméticas, lista de identificadores e um subconjunto sintático de Micro C. As doze gramáticas inválidas conferem a mensagem correspondente, em vez de apenas verificar o código de saída. Há casos específicos para recursão direta e indireta e para conflito LL(1) com literal.

Foram acrescentados testes de regressão para os seguintes pontos da revisão:

- a recursão direta `programa = lista; lista = lista COMMA X | X;` identifica `lista`, não `programa`;
- a recursão indireta identifica os membros do ciclo (`a` e `b`);
- regras inalcançáveis continuam gerando aviso, mas não são emitidas como funções `static` sem uso;
- literais contendo aspas, barra invertida e a sequência `*/` não quebram o C gerado;
- o comentário do parser gerado inclui a produção EBNF completa;
- a alternativa anulável é selecionada por FIRST ou FOLLOW e o corpo é emitido uma única vez;
- opcional com corpo anulável é permitido quando pode ser decidido pelo lookahead; a interseção FIRST/FOLLOW gera aviso e o teste confirma a escolha gulosa; repetição com corpo anulável é rejeitada para evitar laço sem consumo;
- terminal nomeado `ID` e literal `"ID"` possuem índices distintos; literal `"$"` também é distinto do marcador interno de EOF;
- mensagens LL(1) identificam a regra e imprimem literais entre aspas.

A bateria também foi repetida com AddressSanitizer e UndefinedBehaviorSanitizer
(`-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=1`), sem relatórios
dos sanitizadores. Os módulos C foram compilados também com `-fanalyzer`, sem
avisos.

## 3. Scanner Micro C do TP1

O arquivo adaptado `src/scanner/microc.flex` define as variáveis de posição (`linha_token`, `coluna_token` e `coluna_atual`), implementa `scanner_reset_context()`, não contém o `main()` de depuração do TP1 e aloca as mensagens léxicas com `scanner_dup()`. O adaptador `src/scanner/scanner.c` libera `symbol` e `error_msg` após copiar os valores para a estrutura `Token`.

Isso corrige no **arquivo adaptado deste pacote** a incompatibilidade de propriedade de memória identificada na revisão. O `microc.flex` original do TP1, separado deste adaptador, mantém o `main()` de teste e atribui literais a algumas mensagens; ele não deve ser compilado junto com `scanner.c` nem usado no alvo integrado sem adaptação.

A interface `scanner.c` e o driver `microc_parser_driver.c` foram compilados isoladamente em C89. Isso não equivale a executar o scanner gerado pelo Flex.

## 4. Tentativa de integração ponta a ponta

Comando executado:

```sh
make microc-parser
```

Saída relevante:

```text
flex -o build/microc_lex.c src/scanner/microc.flex
make: flex: No such file or directory
make: *** [Makefile:35: build/microc_lex.c] Error 127
```

**Resultado:** integração ponta a ponta não validada neste ambiente, pois o executável `flex` não está instalado. Portanto não afirmamos que `make microc-parser` ou a execução sobre um arquivo `.mc` passou. É necessário rodar esse alvo em uma máquina com Flex e executar os arquivos de teste de sucesso e de EOF do scanner.

## 5. Pendências em relação ao enunciado/código-base

1. **Scanner gerado a partir das regex de qualquer gramática:** não implementado. `TOKENS` e `SKIP` são lidos e armazenados, mas o projeto não converte regex em AFN, AFD minimizado e código de scanner.
2. **Opções `--tree` e `--tokens`:** não implementadas nesta versão. A opção existente `--debug` imprime uma reconstrução da gramática, não substitui automaticamente essas duas opções.
3. **Bateria oficial do código-base:** não executada. O pacote recebido no ambiente não continha a bateria original de 17 testes.
4. **`testes-sugeridos-tp2.zip`, `test_extra.mc` e os dois arquivos de teste de EOF:** não estavam disponíveis no diretório de trabalho desta revisão; não foi possível incorporá-los nem executá-los.

## 6. Divergência a confirmar no enunciado

O texto do enunciado transcrito anteriormente nesta conversa afirma que o corpo de opcionais e repetições não deve ser anulável. A última avaliação técnica recebida considera indevida a rejeição de um opcional anulável. A implementação atual segue essa avaliação: permite opcionais anuláveis quando FIRST/FOLLOW suporta uma decisão preditiva e continua rejeitando repetições anuláveis. É necessário confirmar com o professor qual interpretação será usada na avaliação formal.

## 7. Conclusão

O gerador, o cálculo de conjuntos e a geração dos parsers passaram pela bateria local em C89 estrito, inclusive as novas regressões apontadas na revisão. A validação do scanner Flex integrado permanece pendente por ausência do Flex. A ausência do gerador léxico geral, das opções `--tree`/`--tokens` e da bateria oficial significa que **esta versão ainda não deve ser descrita como integralmente aderente ao enunciado**.
