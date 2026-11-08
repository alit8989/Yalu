#ifndef YALU_VALUE_H
#define YALU_VALUE_H

#include "common.h"
#include <inttypes.h>

struct ASTNode;
struct Environment;
struct Value;
struct ClassObj;
struct InstanceObj;
struct BookObj;
struct BookCollectionObj;

typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_ARRAY,
    VAL_LIST = VAL_ARRAY,
    VAL_BOOK,
    VAL_BOOK_VALS,
    VAL_BOOK_KEYS,
    VAL_FUNCTION,
    VAL_NATIVE_FN,
    VAL_CLASS,
    VAL_INSTANCE
} ValueType;

typedef struct StringObj {
    int refcount;
    size_t length;
    char chars[1]; /* Flexible null-terminated buffer */
} StringObj;

typedef struct ArrayObj {
    int refcount;
    size_t capacity;
    size_t length;
    struct Value* items;
    char* custom_repr;
    bool in_repr;
} ArrayObj;

typedef struct FunctionObj {
    int refcount;
    char* name;
    struct ASTNode* decl;
    struct Environment* closure;
} FunctionObj;

typedef struct Value (*NativeFn)(int argc, struct Value* argv, SourcePos pos);

typedef struct ClassObj {
    int refcount;
    char* name;
    struct ASTNode* decl;
    struct Environment* methods;
} ClassObj;

typedef struct BookCollectionObj {
    int refcount;
    struct BookObj* book;
    bool is_vals; /* true for book>vals, false for book>keys */
} BookCollectionObj;

typedef struct Value {
    ValueType type;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        StringObj* string;
        ArrayObj* array;
        struct BookObj* book;
        BookCollectionObj* book_coll;
        FunctionObj* function;
        NativeFn native_fn;
        ClassObj* klass;
        struct InstanceObj* instance;
    } as;
} Value;

typedef struct BookEntry {
    size_t pair_count;
    char** keys;
    Value* values;
} BookEntry;

typedef struct BookObj {
    int refcount;
    size_t entry_count;
    size_t entry_capacity;
    BookEntry* entries;
    bool in_repr;
} BookObj;

typedef struct PropertyEntry {
    char* name;
    Value value;
    struct PropertyEntry* next;
} PropertyEntry;

typedef struct InstanceObj {
    int refcount;
    ClassObj* klass;
    size_t prop_count;
    size_t prop_cap;
    char** prop_order;
    size_t bucket_count;
    PropertyEntry** buckets;
    bool in_repr;
    bool in_yalupr;
} InstanceObj;

/* Value Constructors */
Value value_make_null(void);
Value value_make_bool(bool b);
Value value_make_int(int64_t i);
Value value_make_float(double d);
Value value_make_string(const char* s, size_t len);
Value value_make_string_take(char* s, size_t len);
Value value_make_array(size_t initial_cap);
Value value_make_function(const char* name, struct ASTNode* decl, struct Environment* closure);
Value value_make_native(NativeFn fn);
Value value_make_class(const char* name, struct ASTNode* decl, struct Environment* methods);
Value value_make_instance(ClassObj* klass);
Value value_make_book(size_t initial_cap);
Value value_make_book_vals(BookObj* book);
Value value_make_book_keys(BookObj* book);

/* Memory Management */
Value value_retain(Value v);
void  value_release(Value v);
Value value_copy(Value v);

/* Array Helpers & Methods */
void array_append(ArrayObj* arr, Value item);
Value array_get(ArrayObj* arr, int64_t index);
void array_set(ArrayObj* arr, int64_t index, Value item);
void array_inc(ArrayObj* arr, Value item);
bool array_rem(ArrayObj* arr, Value item);
size_t array_rep(ArrayObj* arr, Value old_val, Value new_val);
bool array_haveit(ArrayObj* arr, Value item);
Value array_concat(ArrayObj* a, ArrayObj* b);

/* Book Helpers & Methods */
void book_add_entry(BookObj* book, size_t pair_count, char** keys, Value* values);
bool book_get(BookObj* book, const char* key, Value* out_val);
bool book_reverse_lookup(BookObj* book, const char* val_name, Value* out_key);
void book_set(BookObj* book, const char* key, Value val);
bool book_edval(BookObj* book, const char* key, Value new_val);
bool book_edkey(BookObj* book, const char* old_key, const char* new_key);
Value book_get_vals_list(BookObj* book);
Value book_get_keys_list(BookObj* book);
char* book_to_string(BookObj* book, int depth);

/* Instance Property Helpers */
void instance_set_property(InstanceObj* inst, const char* name, Value val);
bool instance_get_property(InstanceObj* inst, const char* name, Value* out_val);
char* instance_to_string(InstanceObj* inst, int depth);

/* Conversions and Inspections */
const char* value_type_name(ValueType type);
char* value_to_string(Value v);
void  value_print(Value v, bool quote_strings);
bool  value_is_truthy(Value v);
bool  value_equals(Value a, Value b);

#endif /* YALU_VALUE_H */
