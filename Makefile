# Yalu Programming Language Makefile
# Targets Windows x64 with Clang or GCC (MinGW-w64)

CC ?= clang
WINDRES ?= windres
CFLAGS = -Wall -Wextra -std=c11
CFLAGS_RELEASE = $(CFLAGS) -O2 -DNDEBUG
CFLAGS_DEBUG = $(CFLAGS) -g -O0 -DYALU_DEBUG

SRC = src/common.c \
      src/errors.c \
      src/value.c \
      src/lexer.c \
      src/ast.c \
      src/parser.c \
      src/runtime.c \
      src/interpreter.c \
      src/main.c

RES_SRC = src/yalu.rc
RES_OBJ = src/yalu_res.o
ICON = yalu.ico

TARGET = yalu.exe
TARGET_DEBUG = yalu_debug.exe

.PHONY: all release debug test check clean

all: release

$(RES_OBJ): $(RES_SRC) $(ICON)
	$(WINDRES) -I. -Isrc $(RES_SRC) -O coff -o $(RES_OBJ)

release: $(TARGET)

$(TARGET): $(SRC) $(RES_OBJ)
	$(CC) $(CFLAGS_RELEASE) $(SRC) $(RES_OBJ) -o $(TARGET)
	@echo [Build] Generated $(TARGET) with embedded icon (Release)

debug: $(TARGET_DEBUG)

$(TARGET_DEBUG): $(SRC) $(RES_OBJ)
	$(CC) $(CFLAGS_DEBUG) $(SRC) $(RES_OBJ) -o $(TARGET_DEBUG)
	@echo [Build] Generated $(TARGET_DEBUG) with embedded icon (Debug)

test: $(TARGET)
	powershell -ExecutionPolicy Bypass -File tests/run_all.ps1

clean:
	@if exist $(TARGET) del /Q $(TARGET)
	@if exist $(TARGET_DEBUG) del /Q $(TARGET_DEBUG)
	@if exist $(RES_OBJ) del /Q $(RES_OBJ)
	@echo [Clean] Finished cleanup.

