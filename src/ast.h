#ifndef YALU_AST_H
#define YALU_AST_H

#include "common.h"
#include "lexer.h"
#include "value.h"

typedef enum {
    /* Root */
    AST_PROGRAM,

    /* Statements */
    AST_VAR_ASSIGN,
    AST_INDEX_ASSIGN,
    AST_PROP_ASSIGN,
    AST_CLASS_PROP_ASSIGN,
    AST_PRINT,
    AST_FN_DECL,
    AST_WHILE,
    AST_FOR,
    AST_IF,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_BLOCK,
    AST_EXPR_STMT,
    AST_USE,
    AST_CLASS_DECL,

    /* Expressions */
    AST_LITERAL,
    AST_STRING_INTERP,
    AST_VAR_REF,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_CALL,
    AST_INPUT,
    AST_RANGE,
    AST_ARRAY_LIT,
    AST_BOOK_LIT,
    AST_INDEX,
    AST_PROP_ACCESS,
    AST_METHOD_CALL,
    AST_NEW_OBJECT,
    AST_FULLCLASS
} ASTNodeType;

typedef struct ASTBookEntry {
    size_t pair_count;
    char** keys;
    struct ASTNode** values;
} ASTBookEntry;

typedef struct ASTNode {
    ASTNodeType type;
    SourcePos pos;
    union {
        /* AST_PROGRAM, AST_BLOCK */
        struct {
            struct ASTNode** stmts;
            size_t count;
            size_t capacity;
        } block;

        /* AST_VAR_ASSIGN */
        struct {
            char* name;
            TokenType op;
            struct ASTNode* value;
        } var_assign;

        /* AST_INDEX_ASSIGN */
        struct {
            struct ASTNode* target;
            struct ASTNode* index;
            TokenType op;
            struct ASTNode* value;
        } index_assign;

        /* AST_PROP_ASSIGN */
        struct {
            struct ASTNode* target;
            char* prop_name;
            TokenType op;
            struct ASTNode* value;
        } prop_assign;

        /* AST_CLASS_PROP_ASSIGN */
        struct {
            char* prop_name;
            char* class_name;
            TokenType op;
            struct ASTNode* value;
        } class_prop_assign;

        /* AST_PRINT */
        struct {
            struct ASTNode** args;
            size_t count;
        } print;

        /* AST_FN_DECL */
        struct {
            char* name;
            char** param_names;
            struct ASTNode** param_defaults;
            char** param_types;
            bool* is_yclass;
            size_t param_count;
            struct ASTNode* body;
        } fn_decl;

        /* AST_WHILE */
        struct {
            struct ASTNode* condition;
            struct ASTNode* body;
        } while_loop;

        /* AST_FOR */
        struct {
            char* var_name;
            struct ASTNode* iterable;
            struct ASTNode* separator;
            struct ASTNode* start;
            struct ASTNode* end;
            struct ASTNode* step;
            struct ASTNode* body;
        } for_loop;

        /* AST_IF */
        struct {
            struct ASTNode** conditions;
            struct ASTNode** bodies;
            size_t branch_count;
            struct ASTNode* else_body;
        } if_stmt;

        /* AST_RETURN */
        struct {
            struct ASTNode* value;
        } return_stmt;

        /* AST_EXPR_STMT */
        struct {
            struct ASTNode* expr;
        } expr_stmt;

        /* AST_USE */
        struct {
            char* module_path;
        } use_stmt;

        /* AST_CLASS_DECL */
        struct {
            char* name;
            struct ASTNode** methods;
            size_t method_count;
        } class_decl;

        /* AST_LITERAL */
        struct {
            Value val;
        } literal;

        /* AST_STRING_INTERP */
        struct {
            struct ASTNode** parts;
            size_t count;
        } string_interp;

        /* AST_VAR_REF */
        struct {
            char* name;
        } var_ref;

        /* AST_BINARY_OP */
        struct {
            TokenType op;
            struct ASTNode* left;
            struct ASTNode* right;
        } binary_op;

        /* AST_UNARY_OP */
        struct {
            TokenType op;
            struct ASTNode* right;
        } unary_op;

        /* AST_CALL */
        struct {
            struct ASTNode* callee;
            struct ASTNode** args;
            size_t arg_count;
        } call;

        /* AST_INPUT */
        struct {
            TokenType input_type;
            struct ASTNode* prompt;
        } input;

        /* AST_RANGE */
        struct {
            struct ASTNode* start;
            struct ASTNode* end;
            struct ASTNode* step;
        } range;

        /* AST_ARRAY_LIT */
        struct {
            struct ASTNode** elements;
            size_t count;
        } array_lit;

        /* AST_INDEX */
        struct {
            struct ASTNode* target;
            struct ASTNode* index;
        } index;

        /* AST_PROP_ACCESS */
        struct {
            struct ASTNode* target;
            char* prop_name;
        } prop_access;

        /* AST_BOOK_LIT */
        struct {
            ASTBookEntry* entries;
            size_t count;
        } book_lit;

        /* AST_METHOD_CALL */
        struct {
            struct ASTNode* target;
            char* method_name;
            struct ASTNode** args;
            size_t arg_count;
        } method_call;

        /* AST_NEW_OBJECT */
        struct {
            char* class_name;
            struct ASTNode** args;
            size_t arg_count;
        } new_object;
    } as;
} ASTNode;

/* AST Node Constructors */
ASTNode* ast_create_program(SourcePos pos);
void     ast_program_add(ASTNode* prog, ASTNode* stmt);

ASTNode* ast_create_block(SourcePos pos);
void     ast_block_add(ASTNode* block, ASTNode* stmt);

ASTNode* ast_create_var_assign(SourcePos pos, const char* name, TokenType op, ASTNode* value);
ASTNode* ast_create_index_assign(SourcePos pos, ASTNode* target, ASTNode* index, TokenType op, ASTNode* value);
ASTNode* ast_create_prop_assign(SourcePos pos, ASTNode* target, const char* prop_name, TokenType op, ASTNode* value);
ASTNode* ast_create_class_prop_assign(SourcePos pos, const char* prop_name, const char* class_name, TokenType op, ASTNode* value);
ASTNode* ast_create_print(SourcePos pos, ASTNode** args, size_t count);
ASTNode* ast_create_fn_decl(SourcePos pos, const char* name, char** params, ASTNode** defaults, size_t count, ASTNode* body);
ASTNode* ast_create_fn_decl_v2(SourcePos pos, const char* name, char** params, ASTNode** defaults, char** types, bool* is_yclass, size_t count, ASTNode* body);
ASTNode* ast_create_while(SourcePos pos, ASTNode* condition, ASTNode* body);
ASTNode* ast_create_for(SourcePos pos, const char* var_name, ASTNode* start, ASTNode* end, ASTNode* step, ASTNode* body);
ASTNode* ast_create_for_v2(SourcePos pos, const char* var_name, ASTNode* iterable, ASTNode* separator, ASTNode* body);
ASTNode* ast_create_if(SourcePos pos, ASTNode** conds, ASTNode** bodies, size_t branch_count, ASTNode* else_body);
ASTNode* ast_create_return(SourcePos pos, ASTNode* value);
ASTNode* ast_create_break(SourcePos pos);
ASTNode* ast_create_continue(SourcePos pos);
ASTNode* ast_create_expr_stmt(SourcePos pos, ASTNode* expr);
ASTNode* ast_create_use(SourcePos pos, const char* module_path);
ASTNode* ast_create_class_decl(SourcePos pos, const char* name, ASTNode** methods, size_t method_count);

ASTNode* ast_create_literal(SourcePos pos, Value val);
ASTNode* ast_create_string_interp(SourcePos pos, ASTNode** parts, size_t count);
ASTNode* ast_create_var_ref(SourcePos pos, const char* name);
ASTNode* ast_create_binary_op(SourcePos pos, TokenType op, ASTNode* left, ASTNode* right);
ASTNode* ast_create_unary_op(SourcePos pos, TokenType op, ASTNode* right);
ASTNode* ast_create_call(SourcePos pos, ASTNode* callee, ASTNode** args, size_t arg_count);
ASTNode* ast_create_input(SourcePos pos, TokenType input_type, ASTNode* prompt);
ASTNode* ast_create_range(SourcePos pos, ASTNode* start, ASTNode* end, ASTNode* step);
ASTNode* ast_create_array_lit(SourcePos pos, ASTNode** elements, size_t count);
ASTNode* ast_create_book_lit(SourcePos pos, ASTBookEntry* entries, size_t count);
ASTNode* ast_create_index(SourcePos pos, ASTNode* target, ASTNode* index);
ASTNode* ast_create_prop_access(SourcePos pos, ASTNode* target, const char* prop_name);
ASTNode* ast_create_method_call(SourcePos pos, ASTNode* target, const char* method_name, ASTNode** args, size_t arg_count);
ASTNode* ast_create_new_object(SourcePos pos, const char* class_name, ASTNode** args, size_t arg_count);
ASTNode* ast_create_fullclass(SourcePos pos);

/* AST cleanup */
void ast_free(ASTNode* node);

#endif /* YALU_AST_H */
