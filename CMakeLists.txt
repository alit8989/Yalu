cmake_minimum_required(VERSION 3.15)
project(Yalu VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /wd4996)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(YALU_SOURCES
    src/common.c
    src/common.h
    src/errors.c
    src/errors.h
    src/value.c
    src/value.h
    src/lexer.c
    src/lexer.h
    src/ast.c
    src/ast.h
    src/parser.c
    src/parser.h
    src/runtime.c
    src/runtime.h
    src/interpreter.c
    src/interpreter.h
    src/main.c
)

if(WIN32)
    list(APPEND YALU_SOURCES src/yalu.rc)
endif()

add_executable(yalu ${YALU_SOURCES})
target_include_directories(yalu PRIVATE src)

set_target_properties(yalu PROPERTIES
    OUTPUT_NAME "yalu"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Optional Debug Executable target
add_executable(yalu_debug ${YALU_SOURCES})
target_include_directories(yalu_debug PRIVATE src)
target_compile_definitions(yalu_debug PRIVATE YALU_DEBUG)
if(NOT MSVC)
    target_compile_options(yalu_debug PRIVATE -g -O0)
endif()
set_target_properties(yalu_debug PROPERTIES
    OUTPUT_NAME "yalu_debug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

