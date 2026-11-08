#include "common.h"

void* yalu_malloc(size_t size) {
    if (size == 0) return NULL;
    void* p = malloc(size);
    if (!p) {
        fprintf(stderr, "[Yalu Fatal] Out of memory (failed to allocate %zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void* yalu_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    void* p = calloc(count, size);
    if (!p) {
        fprintf(stderr, "[Yalu Fatal] Out of memory (failed to calloc %zu x %zu bytes)\n", count, size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void* yalu_realloc(void* ptr, size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }
    void* p = realloc(ptr, new_size);
    if (!p) {
        fprintf(stderr, "[Yalu Fatal] Out of memory (failed to realloc %zu bytes)\n", new_size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void yalu_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

char* yalu_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)yalu_malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

char* yalu_strndup(const char* s, size_t n) {
    if (!s) return NULL;
    char* copy = (char*)yalu_malloc(n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

