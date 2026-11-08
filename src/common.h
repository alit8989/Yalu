#ifndef YALU_COMMON_H
#define YALU_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

#define YALU_VERSION "0.3.0"
#define YALU_MAX_CALL_DEPTH 1000

/* Source position tracking */
typedef struct {
    const char* filename;
    int line;
    int col;
} SourcePos;

static inline SourcePos make_pos(const char* filename, int line, int col) {
    SourcePos p;
    p.filename = filename ? filename : "<unknown>";
    p.line = line;
    p.col = col;
    return p;
}

/* Safe memory management wrappers */
void* yalu_malloc(size_t size);
void* yalu_calloc(size_t count, size_t size);
void* yalu_realloc(void* ptr, size_t new_size);
void  yalu_free(void* ptr);
char* yalu_strdup(const char* s);
char* yalu_strndup(const char* s, size_t n);

#endif /* YALU_COMMON_H */

