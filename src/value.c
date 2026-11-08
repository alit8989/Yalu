#include "value.h"
#include "ast.h"
#include "runtime.h"

Value value_make_null(void) {
    Value v;
    v.type = VAL_NULL;
    v.as.integer = 0;
    return v;
}

Value value_make_bool(bool b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b;
    return v;
}

Value value_make_int(int64_t i) {
    Value v;
    v.type = VAL_INT;
    v.as.integer = i;
    return v;
}

Value value_make_float(double d) {
    Value v;
    v.type = VAL_FLOAT;
    v.as.floating = d;
    return v;
}

Value value_make_string(const char* s, size_t len) {
    Value v;
    v.type = VAL_STRING;
    StringObj* obj = (StringObj*)yalu_malloc(sizeof(StringObj) + len);
    obj->refcount = 1;
    obj->length = len;
    if (s && len > 0) {
        memcpy(obj->chars, s, len);
    }
    obj->chars[len] = '\0';
    v.as.string = obj;
    return v;
}

Value value_make_string_take(char* s, size_t len) {
    Value v = value_make_string(s, len);
    yalu_free(s);
    return v;
}

Value value_make_array(size_t initial_cap) {
    Value v;
    v.type = VAL_ARRAY;
    ArrayObj* obj = (ArrayObj*)yalu_malloc(sizeof(ArrayObj));
    obj->refcount = 1;
    obj->length = 0;
    obj->capacity = initial_cap < 4 ? 4 : initial_cap;
    obj->items = (Value*)yalu_malloc(sizeof(Value) * obj->capacity);
    obj->custom_repr = NULL;
    obj->in_repr = false;
    v.as.array = obj;
    return v;
}

Value value_make_book(size_t initial_cap) {
    Value v;
    v.type = VAL_BOOK;
    BookObj* obj = (BookObj*)yalu_malloc(sizeof(BookObj));
    obj->refcount = 1;
    obj->entry_count = 0;
    obj->entry_capacity = initial_cap < 4 ? 4 : initial_cap;
    obj->entries = (BookEntry*)yalu_malloc(sizeof(BookEntry) * obj->entry_capacity);
    obj->in_repr = false;
    v.as.book = obj;
    return v;
}

Value value_make_book_vals(BookObj* book) {
    Value v;
    v.type = VAL_BOOK_VALS;
    BookCollectionObj* obj = (BookCollectionObj*)yalu_malloc(sizeof(BookCollectionObj));
    obj->refcount = 1;
    obj->book = book;
    if (book) book->refcount++;
    obj->is_vals = true;
    v.as.book_coll = obj;
    return v;
}

Value value_make_book_keys(BookObj* book) {
    Value v;
    v.type = VAL_BOOK_KEYS;
    BookCollectionObj* obj = (BookCollectionObj*)yalu_malloc(sizeof(BookCollectionObj));
    obj->refcount = 1;
    obj->book = book;
    if (book) book->refcount++;
    obj->is_vals = false;
    v.as.book_coll = obj;
    return v;
}

Value value_make_function(const char* name, ASTNode* decl, Environment* closure) {
    Value v;
    v.type = VAL_FUNCTION;
    FunctionObj* obj = (FunctionObj*)yalu_malloc(sizeof(FunctionObj));
    obj->refcount = 1;
    obj->name = yalu_strdup(name ? name : "<anonymous>");
    obj->decl = decl;
    obj->closure = closure ? env_retain(closure) : NULL;
    v.as.function = obj;
    return v;
}

Value value_make_native(NativeFn fn) {
    Value v;
    v.type = VAL_NATIVE_FN;
    v.as.native_fn = fn;
    return v;
}

Value value_make_class(const char* name, ASTNode* decl, Environment* methods) {
    Value v;
    v.type = VAL_CLASS;
    ClassObj* obj = (ClassObj*)yalu_malloc(sizeof(ClassObj));
    obj->refcount = 1;
    obj->name = yalu_strdup(name ? name : "<anonymous_class>");
    obj->decl = decl;
    obj->methods = methods ? env_retain(methods) : NULL;
    v.as.klass = obj;
    return v;
}

Value value_make_instance(ClassObj* klass) {
    Value v;
    v.type = VAL_INSTANCE;
    InstanceObj* obj = (InstanceObj*)yalu_malloc(sizeof(InstanceObj));
    obj->refcount = 1;
    obj->klass = klass;
    if (klass) klass->refcount++;
    obj->prop_count = 0;
    obj->prop_cap = 8;
    obj->prop_order = (char**)yalu_malloc(sizeof(char*) * obj->prop_cap);
    obj->bucket_count = 16;
    obj->buckets = (PropertyEntry**)yalu_calloc(obj->bucket_count, sizeof(PropertyEntry*));
    obj->in_repr = false;
    obj->in_yalupr = false;
    v.as.instance = obj;
    return v;
}

static uint32_t hash_prop(const char* key) {
    uint32_t hash = 2166136261u;
    while (*key) {
        hash ^= (uint8_t)(*key++);
        hash *= 16777619u;
    }
    return hash;
}

void instance_set_property(InstanceObj* inst, const char* name, Value val) {
    if (!inst || !name) return;
    uint32_t h = hash_prop(name) % inst->bucket_count;
    PropertyEntry* entry = inst->buckets[h];

    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            value_release(entry->value);
            entry->value = value_copy(val);
            return;
        }
        entry = entry->next;
    }

    PropertyEntry* new_entry = (PropertyEntry*)yalu_malloc(sizeof(PropertyEntry));
    new_entry->name = yalu_strdup(name);
    new_entry->value = value_copy(val);
    new_entry->next = inst->buckets[h];
    inst->buckets[h] = new_entry;

    if (inst->prop_count >= inst->prop_cap) {
        inst->prop_cap *= 2;
        inst->prop_order = (char**)yalu_realloc(inst->prop_order, sizeof(char*) * inst->prop_cap);
    }
    inst->prop_order[inst->prop_count++] = yalu_strdup(name);
}

bool instance_get_property(InstanceObj* inst, const char* name, Value* out_val) {
    if (!inst || !name) return false;
    uint32_t h = hash_prop(name) % inst->bucket_count;
    PropertyEntry* entry = inst->buckets[h];

    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            if (out_val) {
                *out_val = value_copy(entry->value);
            }
            return true;
        }
        entry = entry->next;
    }

    return false;
}

char* instance_to_string(InstanceObj* inst, int depth) {
    if (!inst) return yalu_strdup("none");
    if (inst->in_repr) {
        return yalu_strdup("<circular reference>");
    }
    if (depth > 20) {
        return yalu_strdup("<max depth>");
    }

    inst->in_repr = true;

    const char* class_name = (inst->klass && inst->klass->name) ? inst->klass->name : "Object";

    if (inst->prop_count == 0) {
        inst->in_repr = false;
        size_t needed = strlen(class_name) + 3;
        char* buf = (char*)yalu_malloc(needed);
        snprintf(buf, needed, "%s()", class_name);
        return buf;
    }

    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)yalu_malloc(cap);

    len += snprintf(buf + len, cap - len, "%s(\n", class_name);

    for (size_t i = 0; i < inst->prop_count; i++) {
        const char* prop_name = inst->prop_order[i];
        Value val;
        if (!instance_get_property(inst, prop_name, &val)) continue;

        char* val_str = NULL;
        if (val.type == VAL_STRING) {
            size_t slen = strlen(val.as.string ? val.as.string->chars : "");
            val_str = (char*)yalu_malloc(slen + 3);
            snprintf(val_str, slen + 3, "\"%s\"", val.as.string ? val.as.string->chars : "");
        } else if (val.type == VAL_NULL) {
            val_str = yalu_strdup("none");
        } else if (val.type == VAL_INSTANCE) {
            val_str = instance_to_string(val.as.instance, depth + 1);
        } else {
            val_str = value_to_string(val);
        }
        value_release(val);

        const char* comma = (i + 1 < inst->prop_count) ? "," : "";
        size_t needed = strlen(prop_name) + strlen(val_str) + strlen(comma) + 16;
        while (len + needed >= cap) {
            cap *= 2;
            buf = (char*)yalu_realloc(buf, cap);
        }

        len += snprintf(buf + len, cap - len, "    %s: %s%s\n", prop_name, val_str, comma);
        yalu_free(val_str);
    }

    while (len + 3 >= cap) {
        cap *= 2;
        buf = (char*)yalu_realloc(buf, cap);
    }
    len += snprintf(buf + len, cap - len, ")");

    inst->in_repr = false;
    return buf;
}

Value value_retain(Value v) {
    switch (v.type) {
        case VAL_STRING:
            if (v.as.string) v.as.string->refcount++;
            break;
        case VAL_ARRAY:
            if (v.as.array) v.as.array->refcount++;
            break;
        case VAL_BOOK:
            if (v.as.book) v.as.book->refcount++;
            break;
        case VAL_BOOK_VALS:
        case VAL_BOOK_KEYS:
            if (v.as.book_coll) v.as.book_coll->refcount++;
            break;
        case VAL_FUNCTION:
            if (v.as.function) v.as.function->refcount++;
            break;
        case VAL_CLASS:
            if (v.as.klass) v.as.klass->refcount++;
            break;
        case VAL_INSTANCE:
            if (v.as.instance) v.as.instance->refcount++;
            break;
        default:
            break;
    }
    return v;
}

void value_release(Value v) {
    switch (v.type) {
        case VAL_STRING:
            if (v.as.string) {
                v.as.string->refcount--;
                if (v.as.string->refcount <= 0) {
                    yalu_free(v.as.string);
                }
            }
            break;
        case VAL_ARRAY:
            if (v.as.array) {
                v.as.array->refcount--;
                if (v.as.array->refcount <= 0) {
                    for (size_t i = 0; i < v.as.array->length; i++) {
                        value_release(v.as.array->items[i]);
                    }
                    if (v.as.array->custom_repr) {
                        yalu_free(v.as.array->custom_repr);
                    }
                    yalu_free(v.as.array->items);
                    yalu_free(v.as.array);
                }
            }
            break;
        case VAL_BOOK:
            if (v.as.book) {
                v.as.book->refcount--;
                if (v.as.book->refcount <= 0) {
                    for (size_t i = 0; i < v.as.book->entry_count; i++) {
                        BookEntry* e = &v.as.book->entries[i];
                        for (size_t j = 0; j < e->pair_count; j++) {
                            yalu_free(e->keys[j]);
                            value_release(e->values[j]);
                        }
                        yalu_free(e->keys);
                        yalu_free(e->values);
                    }
                    yalu_free(v.as.book->entries);
                    yalu_free(v.as.book);
                }
            }
            break;
        case VAL_BOOK_VALS:
        case VAL_BOOK_KEYS:
            if (v.as.book_coll) {
                v.as.book_coll->refcount--;
                if (v.as.book_coll->refcount <= 0) {
                    if (v.as.book_coll->book) {
                        Value bv;
                        bv.type = VAL_BOOK;
                        bv.as.book = v.as.book_coll->book;
                        value_release(bv);
                    }
                    yalu_free(v.as.book_coll);
                }
            }
            break;
        case VAL_FUNCTION:
            if (v.as.function) {
                v.as.function->refcount--;
                if (v.as.function->refcount <= 0) {
                    if (v.as.function->closure) {
                        env_release(v.as.function->closure);
                    }
                    yalu_free(v.as.function->name);
                    yalu_free(v.as.function);
                }
            }
            break;
        case VAL_CLASS:
            if (v.as.klass) {
                v.as.klass->refcount--;
                if (v.as.klass->refcount <= 0) {
                    if (v.as.klass->methods) {
                        env_release(v.as.klass->methods);
                    }
                    yalu_free(v.as.klass->name);
                    yalu_free(v.as.klass);
                }
            }
            break;
        case VAL_INSTANCE:
            if (v.as.instance) {
                v.as.instance->refcount--;
                if (v.as.instance->refcount <= 0) {
                    for (size_t i = 0; i < v.as.instance->bucket_count; i++) {
                        PropertyEntry* entry = v.as.instance->buckets[i];
                        while (entry) {
                            PropertyEntry* next = entry->next;
                            yalu_free(entry->name);
                            value_release(entry->value);
                            yalu_free(entry);
                            entry = next;
                        }
                    }
                    yalu_free(v.as.instance->buckets);
                    for (size_t i = 0; i < v.as.instance->prop_count; i++) {
                        yalu_free(v.as.instance->prop_order[i]);
                    }
                    yalu_free(v.as.instance->prop_order);
                    if (v.as.instance->klass) {
                        v.as.instance->klass->refcount--;
                        if (v.as.instance->klass->refcount <= 0) {
                            if (v.as.instance->klass->methods) {
                                env_release(v.as.instance->klass->methods);
                            }
                            yalu_free(v.as.instance->klass->name);
                            yalu_free(v.as.instance->klass);
                        }
                    }
                    yalu_free(v.as.instance);
                }
            }
            break;
        default:
            break;
    }
}

Value value_copy(Value v) {
    return value_retain(v);
}

void array_append(ArrayObj* arr, Value item) {
    if (!arr) return;
    if (arr->length >= arr->capacity) {
        size_t new_cap = arr->capacity * 2;
        arr->items = (Value*)yalu_realloc(arr->items, sizeof(Value) * new_cap);
        arr->capacity = new_cap;
    }
    arr->items[arr->length++] = value_retain(item);
}

Value array_get(ArrayObj* arr, int64_t index) {
    if (!arr) return value_make_null();
    if (index < 0) {
        index = (int64_t)arr->length + index;
    }
    if (index < 0 || (size_t)index >= arr->length) {
        return value_make_null();
    }
    return value_copy(arr->items[index]);
}

void array_set(ArrayObj* arr, int64_t index, Value item) {
    if (!arr) return;
    if (index < 0) {
        index = (int64_t)arr->length + index;
    }
    if (index < 0 || (size_t)index >= arr->length) {
        return;
    }
    value_release(arr->items[index]);
    arr->items[index] = value_retain(item);
}

void array_inc(ArrayObj* arr, Value item) {
    if (!arr) return;
    if (arr->custom_repr) {
        yalu_free(arr->custom_repr);
        arr->custom_repr = NULL;
    }
    array_append(arr, item);
}

bool array_rem(ArrayObj* arr, Value item) {
    if (!arr) return false;
    for (size_t i = 0; i < arr->length; i++) {
        if (value_equals(arr->items[i], item)) {
            value_release(arr->items[i]);
            for (size_t j = i; j + 1 < arr->length; j++) {
                arr->items[j] = arr->items[j + 1];
            }
            arr->length--;
            if (arr->custom_repr) {
                yalu_free(arr->custom_repr);
                arr->custom_repr = NULL;
            }
            return true;
        }
    }
    return false;
}

size_t array_rep(ArrayObj* arr, Value old_val, Value new_val) {
    if (!arr) return 0;
    size_t count = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (value_equals(arr->items[i], old_val)) {
            value_release(arr->items[i]);
            arr->items[i] = value_retain(new_val);
            count++;
        }
    }
    if (count > 0 && arr->custom_repr) {
        yalu_free(arr->custom_repr);
        arr->custom_repr = NULL;
    }
    return count;
}

bool array_haveit(ArrayObj* arr, Value item) {
    if (!arr) return false;
    for (size_t i = 0; i < arr->length; i++) {
        if (value_equals(arr->items[i], item)) {
            return true;
        }
    }
    return false;
}

Value array_concat(ArrayObj* a, ArrayObj* b) {
    size_t total = (a ? a->length : 0) + (b ? b->length : 0);
    Value v = value_make_array(total);
    ArrayObj* res = v.as.array;
    if (a) {
        for (size_t i = 0; i < a->length; i++) {
            array_append(res, a->items[i]);
        }
    }
    if (b) {
        for (size_t i = 0; i < b->length; i++) {
            array_append(res, b->items[i]);
        }
    }
    char* s_a = a ? value_to_string((Value){ .type = VAL_ARRAY, .as.array = a }) : yalu_strdup("[]");
    char* s_b = b ? value_to_string((Value){ .type = VAL_ARRAY, .as.array = b }) : yalu_strdup("[]");
    size_t rlen = strlen(s_a) + strlen(s_b) + 3;
    res->custom_repr = (char*)yalu_malloc(rlen);
    snprintf(res->custom_repr, rlen, "%s<>%s", s_a, s_b);
    yalu_free(s_a);
    yalu_free(s_b);
    return v;
}

void book_add_entry(BookObj* book, size_t pair_count, char** keys, Value* values) {
    if (!book) return;
    if (book->entry_count >= book->entry_capacity) {
        size_t new_cap = book->entry_capacity * 2;
        book->entries = (BookEntry*)yalu_realloc(book->entries, sizeof(BookEntry) * new_cap);
        book->entry_capacity = new_cap;
    }
    BookEntry* e = &book->entries[book->entry_count++];
    e->pair_count = pair_count;
    e->keys = keys;
    e->values = values;
}

bool book_get(BookObj* book, const char* key, Value* out_val) {
    if (!book || !key) return false;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        for (size_t j = 0; j < e->pair_count; j++) {
            if (strcmp(e->keys[j], key) == 0) {
                if (out_val) *out_val = value_copy(e->values[j]);
                return true;
            }
        }
    }
    return false;
}

bool book_reverse_lookup(BookObj* book, const char* val_name, Value* out_key) {
    if (!book || !val_name) return false;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        for (size_t j = 0; j < e->pair_count; j++) {
            Value v = e->values[j];
            bool match = false;
            if (v.type == VAL_STRING && v.as.string) {
                if (strcmp(v.as.string->chars, val_name) == 0) match = true;
            } else if (v.type == VAL_INT) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%" PRId64, v.as.integer);
                if (strcmp(buf, val_name) == 0) match = true;
            } else if (v.type == VAL_FLOAT) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", v.as.floating);
                if (strcmp(buf, val_name) == 0) match = true;
            } else if (v.type == VAL_BOOL) {
                if (strcmp(val_name, v.as.boolean ? "Tr" : "Fl") == 0) match = true;
            }
            if (match) {
                if (out_key) *out_key = value_make_string(e->keys[j], strlen(e->keys[j]));
                return true;
            }
        }
    }
    return false;
}

void book_set(BookObj* book, const char* key, Value val) {
    if (!book || !key) return;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        for (size_t j = 0; j < e->pair_count; j++) {
            if (strcmp(e->keys[j], key) == 0) {
                value_release(e->values[j]);
                e->values[j] = value_retain(val);
                return;
            }
        }
    }
    char** k = (char**)yalu_malloc(sizeof(char*));
    k[0] = yalu_strdup(key);
    Value* v = (Value*)yalu_malloc(sizeof(Value));
    v[0] = value_retain(val);
    book_add_entry(book, 1, k, v);
}

bool book_edval(BookObj* book, const char* key, Value new_val) {
    if (!book || !key) return false;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        for (size_t j = 0; j < e->pair_count; j++) {
            if (strcmp(e->keys[j], key) == 0) {
                value_release(e->values[j]);
                e->values[j] = value_retain(new_val);
                return true;
            }
        }
    }
    return false;
}

bool book_edkey(BookObj* book, const char* old_key, const char* new_key) {
    if (!book || !old_key || !new_key) return false;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        for (size_t j = 0; j < e->pair_count; j++) {
            if (strcmp(e->keys[j], old_key) == 0) {
                yalu_free(e->keys[j]);
                e->keys[j] = yalu_strdup(new_key);
                return true;
            }
        }
    }
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        for (size_t j = 0; j < e->pair_count; j++) {
            Value v = e->values[j];
            if (v.type == VAL_STRING && v.as.string && strcmp(v.as.string->chars, old_key) == 0) {
                yalu_free(e->keys[j]);
                e->keys[j] = yalu_strdup(new_key);
                return true;
            }
        }
    }
    return false;
}

Value book_get_vals_list(BookObj* book) {
    if (!book) return value_make_array(0);
    Value v = value_make_array(book->entry_count);
    ArrayObj* arr = v.as.array;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        if (e->pair_count == 1) {
            array_append(arr, e->values[0]);
        } else {
            Value sub = value_make_array(e->pair_count);
            for (size_t j = 0; j < e->pair_count; j++) {
                array_append(sub.as.array, e->values[j]);
            }
            array_append(arr, sub);
            value_release(sub);
        }
    }
    return v;
}

Value book_get_keys_list(BookObj* book) {
    if (!book) return value_make_array(0);
    Value v = value_make_array(book->entry_count);
    ArrayObj* arr = v.as.array;
    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        if (e->pair_count == 1) {
            Value ks = value_make_string(e->keys[0], strlen(e->keys[0]));
            array_append(arr, ks);
            value_release(ks);
        } else {
            Value sub = value_make_array(e->pair_count);
            for (size_t j = 0; j < e->pair_count; j++) {
                Value ks = value_make_string(e->keys[j], strlen(e->keys[j]));
                array_append(sub.as.array, ks);
                value_release(ks);
            }
            array_append(arr, sub);
            value_release(sub);
        }
    }
    return v;
}

char* book_to_string(BookObj* book, int depth) {
    if (!book) return yalu_strdup("{}");
    if (book->in_repr) return yalu_strdup("<circular reference>");
    if (depth > 20) return yalu_strdup("<max depth>");
    if (book->entry_count == 0) return yalu_strdup("{}");

    book->in_repr = true;
    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)yalu_malloc(cap);
    len += snprintf(buf + len, cap - len, "{\n");

    for (size_t i = 0; i < book->entry_count; i++) {
        BookEntry* e = &book->entries[i];
        size_t klen = 0;
        for (size_t j = 0; j < e->pair_count; j++) klen += strlen(e->keys[j]) + 4;
        char* kbuf = (char*)yalu_malloc(klen + 1);
        kbuf[0] = '\0';
        for (size_t j = 0; j < e->pair_count; j++) {
            if (j > 0) strcat(kbuf, "<>");
            strcat(kbuf, e->keys[j]);
        }

        char* vbuf = NULL;
        if (e->pair_count == 1) {
            if (e->values[0].type == VAL_BOOK) {
                vbuf = book_to_string(e->values[0].as.book, depth + 1);
            } else {
                vbuf = value_to_string(e->values[0]);
            }
        } else {
            size_t vcap = 64;
            vbuf = (char*)yalu_malloc(vcap);
            vbuf[0] = '\0';
            for (size_t j = 0; j < e->pair_count; j++) {
                char* vs = value_to_string(e->values[j]);
                size_t needed = strlen(vbuf) + strlen(vs) + 4;
                while (needed >= vcap) {
                    vcap *= 2;
                    vbuf = (char*)yalu_realloc(vbuf, vcap);
                }
                if (j > 0) strcat(vbuf, "<>");
                strcat(vbuf, vs);
                yalu_free(vs);
            }
        }

        const char* comma = (i + 1 < book->entry_count) ? "," : "";
        size_t needed = strlen(kbuf) + strlen(vbuf) + strlen(comma) + 16;
        while (len + needed >= cap) {
            cap *= 2;
            buf = (char*)yalu_realloc(buf, cap);
        }
        len += snprintf(buf + len, cap - len, "    %s .. %s%s\n", kbuf, vbuf, comma);
        yalu_free(kbuf);
        yalu_free(vbuf);
    }

    while (len + 3 >= cap) {
        cap *= 2;
        buf = (char*)yalu_realloc(buf, cap);
    }
    len += snprintf(buf + len, cap - len, "}");
    book->in_repr = false;
    return buf;
}

const char* value_type_name(ValueType type) {
    switch (type) {
        case VAL_NULL: return "Null";
        case VAL_BOOL: return "Bool";
        case VAL_INT: return "Int";
        case VAL_FLOAT: return "Float";
        case VAL_STRING: return "String";
        case VAL_ARRAY: return "List";
        case VAL_BOOK: return "Book";
        case VAL_BOOK_VALS: return "BookValues";
        case VAL_BOOK_KEYS: return "BookKeys";
        case VAL_FUNCTION: return "Function";
        case VAL_NATIVE_FN: return "NativeFunction";
        case VAL_CLASS: return "Class";
        case VAL_INSTANCE: return "Instance";
        default: return "Unknown";
    }
}

char* value_to_string(Value v) {
    char buf[128];
    switch (v.type) {
        case VAL_NULL:
            return yalu_strdup("Null");
        case VAL_BOOL:
            return yalu_strdup(v.as.boolean ? "Tr" : "Fl");
        case VAL_INT:
            snprintf(buf, sizeof(buf), "%" PRId64, v.as.integer);
            return yalu_strdup(buf);
        case VAL_FLOAT: {
            snprintf(buf, sizeof(buf), "%.14g", v.as.floating);
            if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')) {
                size_t l = strlen(buf);
                if (l + 2 < sizeof(buf)) {
                    buf[l] = '.';
                    buf[l + 1] = '0';
                    buf[l + 2] = '\0';
                }
            }
            return yalu_strdup(buf);
        }
        case VAL_STRING:
            return yalu_strdup(v.as.string ? v.as.string->chars : "");
        case VAL_ARRAY: {
            if (v.as.array && v.as.array->custom_repr) {
                return yalu_strdup(v.as.array->custom_repr);
            }
            size_t total = 3;
            for (size_t i = 0; i < v.as.array->length; i++) {
                char* s = value_to_string(v.as.array->items[i]);
                total += strlen(s) + 2;
                yalu_free(s);
            }
            char* out = (char*)yalu_malloc(total);
            out[0] = '[';
            out[1] = '\0';
            for (size_t i = 0; i < v.as.array->length; i++) {
                if (i > 0) strcat(out, ", ");
                char* s = value_to_string(v.as.array->items[i]);
                strcat(out, s);
                yalu_free(s);
            }
            strcat(out, "]");
            return out;
        }
        case VAL_BOOK:
            return book_to_string(v.as.book, 0);
        case VAL_BOOK_VALS: {
            BookCollectionObj* bc = v.as.book_coll;
            if (!bc || !bc->book) return yalu_strdup("book>vals:\n");
            size_t cap = 256;
            size_t len = 0;
            char* buf = (char*)yalu_malloc(cap);
            len += snprintf(buf + len, cap - len, "book>vals:\n");
            for (size_t i = 0; i < bc->book->entry_count; i++) {
                BookEntry* e = &bc->book->entries[i];
                for (size_t j = 0; j < e->pair_count; j++) {
                    char* s = value_to_string(e->values[j]);
                    size_t slen = strlen(s);
                    while (len + slen + 2 >= cap) {
                        cap *= 2;
                        buf = (char*)yalu_realloc(buf, cap);
                    }
                    len += snprintf(buf + len, cap - len, "%s\n", s);
                    yalu_free(s);
                }
            }
            if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
            return buf;
        }
        case VAL_BOOK_KEYS: {
            BookCollectionObj* bc = v.as.book_coll;
            if (!bc || !bc->book) return yalu_strdup("book>keys:\n");
            size_t cap = 256;
            size_t len = 0;
            char* buf = (char*)yalu_malloc(cap);
            len += snprintf(buf + len, cap - len, "book>keys:\n");
            for (size_t i = 0; i < bc->book->entry_count; i++) {
                BookEntry* e = &bc->book->entries[i];
                for (size_t j = 0; j < e->pair_count; j++) {
                    size_t slen = strlen(e->keys[j]);
                    while (len + slen + 2 >= cap) {
                        cap *= 2;
                        buf = (char*)yalu_realloc(buf, cap);
                    }
                    len += snprintf(buf + len, cap - len, "%s\n", e->keys[j]);
                }
            }
            if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
            return buf;
        }
        case VAL_FUNCTION: {
            snprintf(buf, sizeof(buf), "<fn %s>", v.as.function->name ? v.as.function->name : "anonymous");
            return yalu_strdup(buf);
        }
        case VAL_NATIVE_FN:
            return yalu_strdup("<native fn>");
        case VAL_CLASS: {
            snprintf(buf, sizeof(buf), "<class %s>", v.as.klass->name ? v.as.klass->name : "anonymous");
            return yalu_strdup(buf);
        }
        case VAL_INSTANCE:
            return instance_to_string(v.as.instance, 0);
    }
    return yalu_strdup("<invalid>");
}

void value_print(Value v, bool quote_strings) {
    if (v.type == VAL_STRING) {
        if (quote_strings) {
            printf("\"%s\"", v.as.string ? v.as.string->chars : "");
        } else {
            printf("%s", v.as.string ? v.as.string->chars : "");
        }
    } else {
        char* s = value_to_string(v);
        printf("%s", s);
        yalu_free(s);
    }
}

bool value_is_truthy(Value v) {
    switch (v.type) {
        case VAL_NULL: return false;
        case VAL_BOOL: return v.as.boolean;
        case VAL_INT: return v.as.integer != 0;
        case VAL_FLOAT: return v.as.floating != 0.0;
        case VAL_STRING: return v.as.string && v.as.string->length > 0;
        case VAL_ARRAY: return v.as.array && v.as.array->length > 0;
        case VAL_BOOK: return v.as.book && v.as.book->entry_count > 0;
        case VAL_BOOK_VALS:
        case VAL_BOOK_KEYS: return v.as.book_coll && v.as.book_coll->book && v.as.book_coll->book->entry_count > 0;
        case VAL_FUNCTION: return true;
        case VAL_NATIVE_FN: return true;
        case VAL_CLASS: return true;
        case VAL_INSTANCE: return true;
        default: return false;
    }
}

bool value_equals(Value a, Value b) {
    if (a.type == b.type) {
        switch (a.type) {
            case VAL_NULL: return true;
            case VAL_BOOL: return a.as.boolean == b.as.boolean;
            case VAL_INT: return a.as.integer == b.as.integer;
            case VAL_FLOAT: return a.as.floating == b.as.floating;
            case VAL_STRING:
                if (a.as.string == b.as.string) return true;
                if (!a.as.string || !b.as.string) return false;
                if (a.as.string->length != b.as.string->length) return false;
                return strcmp(a.as.string->chars, b.as.string->chars) == 0;
            case VAL_ARRAY:
                if (a.as.array == b.as.array) return true;
                if (!a.as.array || !b.as.array) return false;
                if (a.as.array->length != b.as.array->length) return false;
                for (size_t i = 0; i < a.as.array->length; i++) {
                    if (!value_equals(a.as.array->items[i], b.as.array->items[i])) return false;
                }
                return true;
            case VAL_BOOK: {
                if (!a.as.book || !b.as.book) return a.as.book == b.as.book;
                if (a.as.book->entry_count != b.as.book->entry_count) return false;
                for (size_t i = 0; i < a.as.book->entry_count; i++) {
                    BookEntry* ea = &a.as.book->entries[i];
                    BookEntry* eb = &b.as.book->entries[i];
                    if (ea->pair_count != eb->pair_count) return false;
                    for (size_t j = 0; j < ea->pair_count; j++) {
                        if (strcmp(ea->keys[j], eb->keys[j]) != 0) return false;
                        if (!value_equals(ea->values[j], eb->values[j])) return false;
                    }
                }
                return true;
            }
            case VAL_BOOK_VALS:
            case VAL_BOOK_KEYS:
                return a.as.book_coll == b.as.book_coll;
            case VAL_FUNCTION: return a.as.function == b.as.function;
            case VAL_NATIVE_FN: return a.as.native_fn == b.as.native_fn;
            case VAL_CLASS: return a.as.klass == b.as.klass;
            case VAL_INSTANCE: return a.as.instance == b.as.instance;
        }
    }
    if (a.type == VAL_INT && b.type == VAL_FLOAT) {
        return (double)a.as.integer == b.as.floating;
    }
    if (a.type == VAL_FLOAT && b.type == VAL_INT) {
        return a.as.floating == (double)b.as.integer;
    }
    return false;
}
