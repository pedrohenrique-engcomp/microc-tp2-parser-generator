CC       ?= gcc
FLEX     ?= flex
AR       ?= ar
CFLAGS   ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude -Isrc/scanner
BUILD    := build

GEN_SOURCES := \
    src/main.c \
    src/core/grammar.c \
    src/core/tokens.c \
    src/ebnf/ebnf_lexer.c \
    src/ebnf/ebnf_parser.c \
    src/analysis/verificacao.c \
    src/analysis/conjuntos.c \
    src/analysis/ll1.c \
    src/codegen/codegen.c
GEN_OBJECTS := $(GEN_SOURCES:src/%.c=$(BUILD)/%.o)

all: gerador

gerador: $(GEN_OBJECTS)
	$(CC) $(CFLAGS) $(GEN_OBJECTS) -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Biblioteca do scanner Flex adaptado do TP1. Requer flex instalado.
scanner: $(BUILD)/microc_lex.o $(BUILD)/scanner/scanner.o $(BUILD)/core/tokens.o
	$(AR) rcs $(BUILD)/libmicrocscanner.a $^

$(BUILD)/microc_lex.c: src/scanner/microc.flex
	@mkdir -p $(BUILD)
	$(FLEX) -o $@ $<

$(BUILD)/microc_lex.o: $(BUILD)/microc_lex.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function -c $< -o $@

$(BUILD)/scanner/%.o: src/scanner/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Testes funcionais usam scanners de teste deterministas; a ponte com o
# scanner Flex do TP1 tambem e compilada separadamente em C89.
test: gerador
	./tests/run_tests.sh

# Executavel integrado: requer Flex instalado.
microc-parser: gerador scanner
	./gerador --generate $(BUILD)/parser examples/microc.ebnf
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(BUILD) src/scanner/microc_parser_driver.c \
	    $(BUILD)/parser.c $(BUILD)/libmicrocscanner.a -o $(BUILD)/microc-parser

clean:
	rm -rf $(BUILD) gerador

.PHONY: all scanner test microc-parser clean
