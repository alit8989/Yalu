#ifndef YALU_INTERPRETER_H
#define YALU_INTERPRETER_H

#include "common.h"
#include "ast.h"
#include "runtime.h"

typedef struct LoadedModule {
    char* canonical_path;
    Environment* env;
    struct ASTNode* ast;
    char* source;
} LoadedModule;

typedef struct Interpreter {
    Environment* globals;
    Environment* current_env;
    bool is_returning;
    Value return_val;
    bool is_breaking;
    bool is_continuing;
    int call_depth;
    uint64_t step_count;
    uint64_t max_steps;
    bool had_runtime_error;

    /* v0.2 additions */
    char* current_filepath;
    LoadedModule* loaded_modules;
    size_t loaded_count;
    size_t loaded_cap;
    char** import_stack;
    size_t import_stack_depth;
    size_t import_stack_cap;
    struct InstanceObj* current_instance;

    /* Loop separator tracking (for fr !! ... , j = ... !!) */
    bool in_sep_loop;
    char* sep_string;
    size_t loop_current_idx;
    size_t loop_total_count;
} Interpreter;

void interp_init(Interpreter* interp);
void interp_free(Interpreter* interp);

void  interp_set_current_filepath(Interpreter* interp, const char* filepath);
bool  interp_interpret(Interpreter* interp, ASTNode* program);
Value interp_eval_expr(Interpreter* interp, ASTNode* expr);
void  interp_exec_stmt(Interpreter* interp, ASTNode* stmt);

#endif /* YALU_INTERPRETER_H */

