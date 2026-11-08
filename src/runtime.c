#include "runtime.h"
#include "errors.h"

#define ENV_DEFAULT_BUCKETS 32

static uint32_t hash_string(const char* key) {
    uint32_t hash = 2166136261u;
    while (*key) {
        hash ^= (uint8_t)(*key++);
        hash *= 16777619u;
    }
    return hash;
}

Environment* env_create(Environment* parent) {
    Environment* env = (Environment*)yalu_malloc(sizeof(Environment));
    env->refcount = 1;
    env->parent = parent ? env_retain(parent) : NULL;
    env->bucket_count = ENV_DEFAULT_BUCKETS;
    env->buckets = (EnvEntry**)yalu_calloc(env->bucket_count, sizeof(EnvEntry*));
    return env;
}

Environment* env_retain(Environment* env) {
    if (env) {
        env->refcount++;
    }
    return env;
}

void env_release(Environment* env) {
    if (!env) return;
    env->refcount--;
    if (env->refcount <= 0) {
        for (size_t i = 0; i < env->bucket_count; i++) {
            EnvEntry* entry = env->buckets[i];
            while (entry) {
                EnvEntry* next = entry->next;
                yalu_free(entry->name);
                value_release(entry->value);
                yalu_free(entry);
                entry = next;
            }
        }
        yalu_free(env->buckets);
        if (env->parent) {
            env_release(env->parent);
        }
        yalu_free(env);
    }
}

bool env_contains_local(Environment* env, const char* name) {
    if (!env || !name) return false;
    uint32_t h = hash_string(name) % env->bucket_count;
    EnvEntry* entry = env->buckets[h];
    while (entry) {
        if (strcmp(entry->name, name) == 0) return true;
        entry = entry->next;
    }
    return false;
}

void env_export_symbols(Environment* src, Environment* dst) {
    if (!src || !dst) return;
    for (size_t i = 0; i < src->bucket_count; i++) {
        EnvEntry* entry = src->buckets[i];
        while (entry) {
            env_define(dst, entry->name, entry->value);
            entry = entry->next;
        }
    }
}

void env_define(Environment* env, const char* name, Value value) {
    if (!env || !name) return;
    uint32_t h = hash_string(name) % env->bucket_count;
    EnvEntry* entry = env->buckets[h];

    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            value_release(entry->value);
            entry->value = value_copy(value);
            return;
        }
        entry = entry->next;
    }

    EnvEntry* new_entry = (EnvEntry*)yalu_malloc(sizeof(EnvEntry));
    new_entry->name = yalu_strdup(name);
    new_entry->value = value_copy(value);
    new_entry->next = env->buckets[h];
    env->buckets[h] = new_entry;
}

bool env_set(Environment* env, const char* name, Value value) {
    if (!name) return false;
    Environment* curr = env;

    while (curr) {
        uint32_t h = hash_string(name) % curr->bucket_count;
        EnvEntry* entry = curr->buckets[h];
        while (entry) {
            if (strcmp(entry->name, name) == 0) {
                value_release(entry->value);
                entry->value = value_copy(value);
                return true;
            }
            entry = entry->next;
        }
        curr = curr->parent;
    }

    /* If not found in any scope, define in current scope */
    env_define(env, name, value);
    return true;
}

bool env_get(Environment* env, const char* name, Value* out_val) {
    if (!name) return false;
    Environment* curr = env;

    while (curr) {
        uint32_t h = hash_string(name) % curr->bucket_count;
        EnvEntry* entry = curr->buckets[h];
        while (entry) {
            if (strcmp(entry->name, name) == 0) {
                if (out_val) {
                    *out_val = value_copy(entry->value);
                }
                return true;
            }
            entry = entry->next;
        }
        curr = curr->parent;
    }

    return false;
}

/* Built-in standard library functions */
static Value native_len(int argc, Value* argv, SourcePos pos) {
    if (argc < 1) {
        yalu_report_error(pos, "Runtime Error", "len() expects 1 argument");
        return value_make_null();
    }
    Value v = argv[0];
    if (v.type == VAL_STRING) {
        return value_make_int((int64_t)v.as.string->length);
    }
    if (v.type == VAL_ARRAY) {
        return value_make_int((int64_t)v.as.array->length);
    }
    if (v.type == VAL_BOOK) {
        return value_make_int((int64_t)v.as.book->entry_count);
    }
    if (v.type == VAL_BOOK_VALS || v.type == VAL_BOOK_KEYS) {
        return value_make_int((int64_t)(v.as.book_coll && v.as.book_coll->book ? v.as.book_coll->book->entry_count : 0));
    }
    yalu_report_error(pos, "Type Error", "len() argument must be String or Array, got %s", value_type_name(v.type));
    return value_make_null();
}

static const char* get_type_name_uppercase(Value v) {
    switch (v.type) {
        case VAL_STRING: return "STRING";
        case VAL_INT: return "INTEGER";
        case VAL_FLOAT: return "FLOAT";
        case VAL_ARRAY: return "LIST";
        case VAL_BOOK: return "BOOK";
        case VAL_BOOK_VALS: return "BOOK";
        case VAL_BOOK_KEYS: return "BOOK";
        case VAL_BOOL: return "BOOLEAN";
        case VAL_NULL: return "NULL";
        case VAL_CLASS: return "CLASS";
        case VAL_INSTANCE: return "OBJECT";
        case VAL_FUNCTION: return "FUNCTION";
        case VAL_NATIVE_FN: return "FUNCTION";
        default: return "UNKNOWN";
    }
}

static Value native_type(int argc, Value* argv, SourcePos pos) {
    (void)pos;
    if (argc < 1) return value_make_string("NULL", 4);
    if (argc == 1) {
        const char* t = get_type_name_uppercase(argv[0]);
        return value_make_string(t, strlen(t));
    }

    Value list_val = value_make_array(argc);
    size_t repr_cap = 64;
    char* repr = (char*)yalu_malloc(repr_cap);
    repr[0] = '\0';

    for (int i = 0; i < argc; i++) {
        const char* t = get_type_name_uppercase(argv[i]);
        Value sv = value_make_string(t, strlen(t));
        array_append(list_val.as.array, sv);
        value_release(sv);

        size_t needed = strlen(repr) + strlen(t) + 4;
        while (needed >= repr_cap) {
            repr_cap *= 2;
            repr = (char*)yalu_realloc(repr, repr_cap);
        }
        if (i > 0) strcat(repr, " ");
        strcat(repr, t);
    }
    list_val.as.array->custom_repr = repr;
    return list_val;
}

static Value native_str(int argc, Value* argv, SourcePos pos) {
    (void)pos;
    if (argc < 1) return value_make_string("", 0);
    char* s = value_to_string(argv[0]);
    Value result = value_make_string(s, strlen(s));
    yalu_free(s);
    return result;
}

static Value native_int(int argc, Value* argv, SourcePos pos) {
    (void)pos;
    if (argc < 1) return value_make_int(0);
    Value v = argv[0];
    if (v.type == VAL_INT) return v;
    if (v.type == VAL_FLOAT) return value_make_int((int64_t)v.as.floating);
    if (v.type == VAL_BOOL) return value_make_int(v.as.boolean ? 1 : 0);
    if (v.type == VAL_STRING) {
        return value_make_int((int64_t)strtoll(v.as.string->chars, NULL, 10));
    }
    return value_make_int(0);
}

static Value native_float(int argc, Value* argv, SourcePos pos) {
    (void)pos;
    if (argc < 1) return value_make_float(0.0);
    Value v = argv[0];
    if (v.type == VAL_FLOAT) return v;
    if (v.type == VAL_INT) return value_make_float((double)v.as.integer);
    if (v.type == VAL_BOOL) return value_make_float(v.as.boolean ? 1.0 : 0.0);
    if (v.type == VAL_STRING) {
        return value_make_float(strtod(v.as.string->chars, NULL));
    }
    return value_make_float(0.0);
}

static Value native_abs(int argc, Value* argv, SourcePos pos) {
    (void)pos;
    if (argc < 1) return value_make_int(0);
    Value v = argv[0];
    if (v.type == VAL_INT) {
        int64_t i = v.as.integer;
        return value_make_int(i < 0 ? -i : i);
    }
    if (v.type == VAL_FLOAT) {
        return value_make_float(fabs(v.as.floating));
    }
    return v;
}

static Value native_push(int argc, Value* argv, SourcePos pos) {
    if (argc < 2 || argv[0].type != VAL_ARRAY) {
        yalu_report_error(pos, "Runtime Error", "push() expects (array, item)");
        return value_make_null();
    }
    array_append(argv[0].as.array, argv[1]);
    return value_make_null();
}

static Value native_pop(int argc, Value* argv, SourcePos pos) {
    if (argc < 1 || argv[0].type != VAL_ARRAY) {
        yalu_report_error(pos, "Runtime Error", "pop() expects array argument");
        return value_make_null();
    }
    ArrayObj* arr = argv[0].as.array;
    if (arr->length == 0) {
        return value_make_null();
    }
    Value last = arr->items[--arr->length];
    return last;
}

Environment* runtime_init_globals(void) {
    Environment* env = env_create(NULL);

    /* Builtin constants */
    env_define(env, "Tr", value_make_bool(true));
    env_define(env, "Fl", value_make_bool(false));
    env_define(env, "Null", value_make_null());

    /* Builtin functions */
    env_define(env, "len", value_make_native(native_len));
    env_define(env, "type", value_make_native(native_type));
    env_define(env, "str", value_make_native(native_str));
    env_define(env, "int", value_make_native(native_int));
    env_define(env, "float", value_make_native(native_float));
    env_define(env, "abs", value_make_native(native_abs));
    env_define(env, "push", value_make_native(native_push));
    env_define(env, "pop", value_make_native(native_pop));

    return env;
}
