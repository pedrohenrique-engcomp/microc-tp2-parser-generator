#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/test
CC=${CC:-gcc}
CFLAGS=${CFLAGS:-'-std=c89 -pedantic -Wall -Wextra -Werror'}

expect_invalid()
{
    grammar=$1
    expected=$2
    label=$3
    output="build/test/${label}.txt"
    if ./gerador "$grammar" > /dev/null 2> "$output"; then
        echo "FALHA: $grammar foi aceita indevidamente" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" "$output"; then
        echo "FALHA: $grammar foi rejeitada pelo motivo incorreto" >&2
        echo "Esperado: $expected" >&2
        cat "$output" >&2
        exit 1
    fi
}

# 1. Geracao, compilacao e execucao do parser aritmetico.
./gerador --debug --sets --generate build/parser examples/expressoes.ebnf > build/test/expressions_sets.txt
grep -q 'expr:' build/test/expressions_sets.txt
grep -q 'FIRST={' build/test/expressions_sets.txt
$CC $CFLAGS -Ibuild build/parser.c tests/parser_runtime_test.c -o build/parser_runtime_test
./build/parser_runtime_test > build/test/expressions_runtime.txt 2> build/test/expressions_errors.txt
grep -q '6 casos de execucao passaram' build/test/expressions_runtime.txt

# 2. Segunda gramatica valida: lista de identificadores.
./gerador --sets --generate build/lista_parser examples/lista.ebnf > build/test/lista_sets.txt
$CC $CFLAGS -Ibuild build/lista_parser.c tests/lista_runtime_test.c -o build/lista_runtime_test
./build/lista_runtime_test > build/test/lista_runtime.txt 2> build/test/lista_errors.txt
grep -q '6 casos passaram' build/test/lista_runtime.txt

# 3. Terceira gramatica valida: subconjunto sintatico Micro C.
./gerador --sets --generate build/microc_parser examples/microc.ebnf > build/test/microc_sets.txt 2> build/test/microc_warnings.txt
grep -q 'programa:' build/test/microc_sets.txt
$CC $CFLAGS -Ibuild build/microc_parser.c tests/microc_runtime_test.c -o build/microc_runtime_test
./build/microc_runtime_test > build/test/microc_runtime.txt 2> build/test/microc_errors.txt
grep -q '6 casos passaram' build/test/microc_runtime.txt

# 4. A alternativa anulavel escolhe por FIRST ou FOLLOW, sem duplicar o ramo.
./gerador --generate build/nullable_branch_parser tests/nullable_branch.ebnf > /dev/null
$CC $CFLAGS -Ibuild build/nullable_branch_parser.c \
    tests/nullable_branch_runtime_test.c -o build/nullable_branch_runtime_test
./build/nullable_branch_runtime_test > build/test/nullable_branch_runtime.txt \
    2> build/test/nullable_branch_errors.txt
grep -q '4 casos da alternativa anulavel passaram' \
    build/test/nullable_branch_runtime.txt

# 4b. Opcionais com corpo anulavel sao validos; repeticoes anulaveis nao.
./gerador --sets --generate build/optional_nullable_parser \
    tests/optional_nullable.ebnf > build/test/optional_nullable_sets.txt
$CC $CFLAGS -Ibuild build/optional_nullable_parser.c \
    tests/optional_nullable_runtime_test.c -o build/optional_nullable_runtime_test
./build/optional_nullable_runtime_test > build/test/optional_nullable_runtime.txt \
    2> build/test/optional_nullable_errors.txt
grep -q '3 casos de opcional anulavel passaram' \
    build/test/optional_nullable_runtime.txt
./gerador --generate build/optional_nullable_greedy_parser \
    tests/optional_nullable_greedy.ebnf > /dev/null \
    2> build/test/optional_nullable_greedy_warnings.txt
grep -q 'sera aplicada escolha gulosa' \
    build/test/optional_nullable_greedy_warnings.txt
$CC $CFLAGS -Ibuild build/optional_nullable_greedy_parser.c \
    tests/optional_nullable_greedy_runtime_test.c \
    -o build/optional_nullable_greedy_runtime_test
./build/optional_nullable_greedy_runtime_test > \
    build/test/optional_nullable_greedy_runtime.txt \
    2> build/test/optional_nullable_greedy_errors.txt
grep -q 'opcional anulavel: escolha gulosa verificada' \
    build/test/optional_nullable_greedy_runtime.txt

# 4c. Token nomeado ID e literal "ID" sao terminais diferentes.
./gerador --sets --generate build/terminal_collision_parser \
    tests/terminal_collision.ebnf > build/test/terminal_collision_sets.txt
$CC $CFLAGS -Ibuild build/terminal_collision_parser.c \
    tests/terminal_collision_runtime_test.c -o build/terminal_collision_runtime_test
./build/terminal_collision_runtime_test > build/test/terminal_collision_runtime.txt
grep -q 'colisao entre ID nomeado e literal ID resolvida' \
    build/test/terminal_collision_runtime.txt
grep -q 'PARSER_T_ID' build/terminal_collision_parser.h
grep -q 'PARSER_LITERAL_1' build/terminal_collision_parser.h

# 4d. Um literal "$" nao pode colidir com o marcador de EOF.
./gerador --generate build/literal_dollar_parser tests/literal_dollar.ebnf > /dev/null
$CC $CFLAGS -Ibuild build/literal_dollar_parser.c \
    tests/literal_dollar_runtime_test.c -o build/literal_dollar_runtime_test
./build/literal_dollar_runtime_test > build/test/literal_dollar_runtime.txt
grep -q "literal '\$' distinguido do fim de arquivo" build/test/literal_dollar_runtime.txt

# 5. Regras inalcançaveis geram aviso, mas nao criam funcoes static sem uso.
./gerador --generate build/unreachable_parser tests/inalcancavel.ebnf \
    > /dev/null 2> build/test/unreachable_warnings.txt
grep -q "'sobra'" build/test/unreachable_warnings.txt
if grep -q 'parse_sobra' build/unreachable_parser.c; then
    echo 'FALHA: regra inalcançavel foi gerada' >&2
    exit 1
fi
$CC $CFLAGS -Werror -Ibuild -c build/unreachable_parser.c \
    -o build/unreachable_parser.o

# 6. Literais com aspas, barra invertida e fechamento de comentario produzem C valido.
./gerador --generate build/escape_parser tests/escape_literals.ebnf > /dev/null
$CC $CFLAGS -Werror -Ibuild -c build/escape_parser.c -o build/escape_parser.o
grep -Fq '*\/' build/escape_parser.c

# 7. Regenera o header com o vocabulario Micro C para compilar a ponte.
./gerador --generate build/parser examples/microc.ebnf > /dev/null 2> build/test/microc_codegen_warnings.txt
$CC $CFLAGS -Iinclude -Ibuild -Isrc/scanner -c src/scanner/scanner.c -o build/test/scanner.o
$CC $CFLAGS -Iinclude -Ibuild -Isrc/scanner -c src/scanner/microc_parser_driver.c -o build/test/microc_parser_driver.o

# 8. Gramaticas invalidas: cada teste confere tambem o diagnostico esperado.
expect_invalid tests/erro_nao_definido.ebnf "foi usado mas nao foi definido" undefined
expect_invalid tests/erro_recursao_esquerda.ebnf "recursao a esquerda direta ou indireta" left_existing
expect_invalid tests/erro_sintaxe.ebnf "erro de leitura EBNF" syntax
expect_invalid tests/erro_improdutivo.ebnf "e improdutivo" unproductive
expect_invalid tests/erro_conflito_ll1.ebnf "conflito LL(1) em regra 'stmt'" ll1_conflict
expect_invalid tests/erro_nullable_repetition.ebnf "corpo de repeticao e anulavel" nullable_repetition
expect_invalid tests/erro_nullable_optional_repetition.ebnf "corpo de repeticao e anulavel" nullable_optional_repetition
expect_invalid tests/erro_token_nao_declarado.ebnf "nao foi declarado em TOKENS" undeclared_token
expect_invalid tests/erro_terminal_duplicado.ebnf "declarado mais de uma vez" duplicate_token
expect_invalid tests/erro_recursao_direta.ebnf "recursao a esquerda direta ou indireta em 'lista'" left_direct
expect_invalid tests/erro_recursao_indireta.ebnf "recursao a esquerda direta ou indireta em 'a'" left_indirect
expect_invalid tests/erro_conflito_literal.ebnf 'FIRST em comum {","}' ll1_literal
if [ "$(grep -c "recursao a esquerda direta ou indireta em '" build/test/left_indirect.txt)" -ne 2 ]; then
    echo 'FALHA: ciclo indireto deveria identificar exatamente a e b' >&2
    cat build/test/left_indirect.txt >&2
    exit 1
fi

echo "Todos os testes passaram: 3 gramaticas validas principais; opcional anulavel; colisao token/literal/EOF; parsers gerados e compilados em C89; diagnosticos de gramaticas invalidas conferidos."
