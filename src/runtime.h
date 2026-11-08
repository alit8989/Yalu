#ifndef YALU_RUNTIME_H
#define YALU_RUNTIME_H

#include "common.h"
#include "value.h"

typedef struct EnvEntry {
    char* name;
    Value value;
    struct EnvEntry* next;
} EnvEntry;

typedef struct Environment {
    int refcount;
    struct Environment* parent;
    size_t bucket_count;
    EnvEntry** buckets;
} Environment;

Environment* env_create(Environment* parent);
Environment* env_retain(Environment* env);
void         env_release(Environment* env);

void env_define(Environment* env, const char* name, Value value);
bool env_set(Environment* env, const char* name, Value value);
bool env_get(Environment* env, const char* name, Value* out_val);
bool env_contains_local(Environment* env, const char* name);
void env_export_symbols(Environment* src, Environment* dst);

/* Global runtime initialization */
Environment* runtime_init_globals(void);

#endif /* YALU_RUNTIME_H */

