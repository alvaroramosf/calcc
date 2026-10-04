# ==============================================================================
# Makefile - Modular Micro C to Retro-Calculator Transpiler (mk56c)
# ==============================================================================

CC ?= gcc
ENV_BIN ?= $(CURDIR)/env/bin
export PATH := $(ENV_BIN):$(PATH)

FLEX  ?= $(shell which flex 2>/dev/null || echo $(ENV_BIN)/flex)
BISON ?= $(shell which bison 2>/dev/null || echo $(ENV_BIN)/bison)

CFLAGS ?= -Wall -Wextra -Wpedantic -O2 -Iinclude -I$(BUILD_DIR)/frontend -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS ?= -lm

# Build directories
BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_NAME  := mk56c
TARGET    := $(BUILD_DIR)/$(BIN_NAME)

# Parser & Lexer generation targets
PARSER_Y   := src/frontend/parser.y
LEXER_L    := src/frontend/lexer.l
PARSER_SRC := $(BUILD_DIR)/frontend/parser.tab.c
PARSER_HDR := $(BUILD_DIR)/frontend/parser.tab.h
LEXER_SRC  := $(BUILD_DIR)/frontend/lex.yy.c

PARSER_OBJ := $(OBJ_DIR)/frontend/parser.tab.o
LEXER_OBJ  := $(OBJ_DIR)/frontend/lex.yy.o

# Source files auto-discovery
SRCS := $(wildcard src/*.c) \
        $(wildcard src/frontend/*.c) \
        $(wildcard src/core/*.c) \
        $(wildcard src/backend/*.c) \
        $(wildcard src/backend/targets/*.c)

# Object files corresponding to hand-written sources
OBJS := $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Total objects to link
ALL_OBJS := $(OBJS) $(PARSER_OBJ) $(LEXER_OBJ)

# Header dependencies
DEPS := $(OBJS:.o=.d)

# Colors for terminal output
COLOR_RESET  := \033[0m
COLOR_GREEN  := \033[32m
COLOR_BLUE   := \033[34m
COLOR_YELLOW := \033[33m
COLOR_MAGENTA:= \033[35m

.PHONY: all clean test help dirs

all: dirs $(TARGET)

help:
	@echo "Available targets:"
	@echo "  make              - Build $(BIN_NAME) in $(BUILD_DIR)/"
	@echo "  make test         - Run test suite against tests/"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Display this help message"

dirs:
	@mkdir -p $(BUILD_DIR)/frontend
	@mkdir -p $(OBJ_DIR)/frontend
	@mkdir -p $(OBJ_DIR)/core
	@mkdir -p $(OBJ_DIR)/backend/targets

# Bison generation
$(PARSER_SRC) $(PARSER_HDR): $(PARSER_Y) | dirs
	@echo "$(COLOR_MAGENTA)[BISON]$(COLOR_RESET) $<"
	$(BISON) -d -Wno-yacc $< -o $(PARSER_SRC)

# Flex generation
$(LEXER_SRC): $(LEXER_L) $(PARSER_HDR) | dirs
	@echo "$(COLOR_MAGENTA)[FLEX]$(COLOR_RESET)  $<"
	$(FLEX) -o $(LEXER_SRC) $<

# Compile generated parser (relaxed warnings for generated code)
$(PARSER_OBJ): $(PARSER_SRC)
	@echo "$(COLOR_YELLOW)[CC]$(COLOR_RESET)    $<"
	$(CC) -Iinclude -I$(BUILD_DIR)/frontend -O2 -c $< -o $@

# Compile generated lexer (relaxed warnings for generated code)
$(LEXER_OBJ): $(LEXER_SRC)
	@echo "$(COLOR_YELLOW)[CC]$(COLOR_RESET)    $<"
	$(CC) -Iinclude -I$(BUILD_DIR)/frontend -O2 -c $< -o $@

$(TARGET): $(PARSER_HDR) $(ALL_OBJS)
	@echo "$(COLOR_BLUE)[LD]$(COLOR_RESET)    $@"
	$(CC) $(ALL_OBJS) -o $@ $(LDFLAGS)
	@echo "$(COLOR_GREEN)[OK]$(COLOR_RESET)    Build complete: $@"

# Compile hand-written C sources with automatic dependency generation
$(OBJ_DIR)/%.o: src/%.c $(PARSER_HDR)
	@mkdir -p $(dir $@)
	@echo "$(COLOR_YELLOW)[CC]$(COLOR_RESET)    $<"
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

test: all
	@./tests/run_tests.sh

clean:
	@echo "$(COLOR_YELLOW)[CLEAN]$(COLOR_RESET) Removing $(BUILD_DIR)..."
	rm -rf $(BUILD_DIR)
	@echo "$(COLOR_GREEN)[OK]$(COLOR_RESET)    Clean completed."
