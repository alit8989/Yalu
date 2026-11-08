#include "ast.h"

static ASTNode* ast_alloc_node(ASTNodeType type, SourcePos pos) {
    ASTNode* node = (ASTNode*)yalu_malloc(sizeof(ASTNode));
    memset(node, 0, sizeof(ASTNode));
    node->type = type;
    node->pos = pos;
    return node;
}

ASTNode* ast_create_program(SourcePos pos) {
    ASTNode* node = ast_alloc_node(AST_PROGRAM, pos);
    node->as.block.stmts = NULL;
    node->as.block.count = 0;
    node->as.block.capacity = 0;
    return node;
}

void ast_program_add(ASTNode* prog, ASTNode* stmt) {
    if (!prog || !stmt) return;
    if (prog->as.block.count >= prog->as.block.capacity) {
        size_t new_cap = prog->as.block.capacity < 8 ? 8 : prog->as.block.capacity * 2;
        prog->as.block.stmts = (ASTNode**)yalu_realloc(prog->as.block.stmts, sizeof(ASTNode*) * new_cap);
        prog->as.block.capacity = new_cap;
    }
    prog->as.block.stmts[prog->as.block.count++] = stmt;
}

ASTNode* ast_create_block(SourcePos pos) {
    ASTNode* node = ast_alloc_node(AST_BLOCK, pos);
    node->as.block.stmts = NULL;
    node->as.block.count = 0;
    node->as.block.capacity = 0;
    return node;
}

void ast_block_add(ASTNode* block, ASTNode* stmt) {
    ast_program_add(block, stmt);
}

ASTNode* ast_create_var_assign(SourcePos pos, const char* name, TokenType op, ASTNode* value) {
    ASTNode* node = ast_alloc_node(AST_VAR_ASSIGN, pos);
    node->as.var_assign.name = yalu_strdup(name);
    node->as.var_assign.op = op;
    node->as.var_assign.value = value;
    return node;
}

ASTNode* ast_create_index_assign(SourcePos pos, ASTNode* target, ASTNode* index, TokenType op, ASTNode* value) {
    ASTNode* node = ast_alloc_node(AST_INDEX_ASSIGN, pos);
    node->as.index_assign.target = target;
    node->as.index_assign.index = index;
    node->as.index_assign.op = op;
    node->as.index_assign.value = value;
    return node;
}

ASTNode* ast_create_prop_assign(SourcePos pos, ASTNode* target, const char* prop_name, TokenType op, ASTNode* value) {
    ASTNode* node = ast_alloc_node(AST_PROP_ASSIGN, pos);
    node->as.prop_assign.target = target;
    node->as.prop_assign.prop_name = yalu_strdup(prop_name);
    node->as.prop_assign.op = op;
    node->as.prop_assign.value = value;
    return node;
}

ASTNode* ast_create_class_prop_assign(SourcePos pos, const char* prop_name, const char* class_name, TokenType op, ASTNode* value) {
    ASTNode* node = ast_alloc_node(AST_CLASS_PROP_ASSIGN, pos);
    node->as.class_prop_assign.prop_name = yalu_strdup(prop_name);
    node->as.class_prop_assign.class_name = yalu_strdup(class_name);
    node->as.class_prop_assign.op = op;
    node->as.class_prop_assign.value = value;
    return node;
}

ASTNode* ast_create_print(SourcePos pos, ASTNode** args, size_t count) {
    ASTNode* node = ast_alloc_node(AST_PRINT, pos);
    node->as.print.args = args;
    node->as.print.count = count;
    return node;
}

ASTNode* ast_create_fn_decl(SourcePos pos, const char* name, char** params, ASTNode** defaults, size_t count, ASTNode* body) {
    return ast_create_fn_decl_v2(pos, name, params, defaults, NULL, NULL, count, body);
}

ASTNode* ast_create_fn_decl_v2(SourcePos pos, const char* name, char** params, ASTNode** defaults, char** types, bool* is_yclass, size_t count, ASTNode* body) {
    ASTNode* node = ast_alloc_node(AST_FN_DECL, pos);
    node->as.fn_decl.name = yalu_strdup(name);
    node->as.fn_decl.param_names = params;
    node->as.fn_decl.param_defaults = defaults;
    node->as.fn_decl.param_types = types;
    node->as.fn_decl.is_yclass = is_yclass;
    node->as.fn_decl.param_count = count;
    node->as.fn_decl.body = body;
    return node;
}

ASTNode* ast_create_while(SourcePos pos, ASTNode* condition, ASTNode* body) {
    ASTNode* node = ast_alloc_node(AST_WHILE, pos);
    node->as.while_loop.condition = condition;
    node->as.while_loop.body = body;
    return node;
}

ASTNode* ast_create_for(SourcePos pos, const char* var_name, ASTNode* start, ASTNode* end, ASTNode* step, ASTNode* body) {
    ASTNode* node = ast_alloc_node(AST_FOR, pos);
    node->as.for_loop.var_name = yalu_strdup(var_name);
    node->as.for_loop.iterable = NULL;
    node->as.for_loop.separator = NULL;
    node->as.for_loop.start = start;
    node->as.for_loop.end = end;
    node->as.for_loop.step = step;
    node->as.for_loop.body = body;
    return node;
}

ASTNode* ast_create_for_v2(SourcePos pos, const char* var_name, ASTNode* iterable, ASTNode* separator, ASTNode* body) {
    ASTNode* node = ast_alloc_node(AST_FOR, pos);
    node->as.for_loop.var_name = yalu_strdup(var_name);
    node->as.for_loop.iterable = iterable;
    node->as.for_loop.separator = separator;
    node->as.for_loop.start = NULL;
    node->as.for_loop.end = NULL;
    node->as.for_loop.step = NULL;
    node->as.for_loop.body = body;
    return node;
}

ASTNode* ast_create_if(SourcePos pos, ASTNode** conds, ASTNode** bodies, size_t branch_count, ASTNode* else_body) {
    ASTNode* node = ast_alloc_node(AST_IF, pos);
    node->as.if_stmt.conditions = conds;
    node->as.if_stmt.bodies = bodies;
    node->as.if_stmt.branch_count = branch_count;
    node->as.if_stmt.else_body = else_body;
    return node;
}

ASTNode* ast_create_return(SourcePos pos, ASTNode* value) {
    ASTNode* node = ast_alloc_node(AST_RETURN, pos);
    node->as.return_stmt.value = value;
    return node;
}

ASTNode* ast_create_break(SourcePos pos) {
    return ast_alloc_node(AST_BREAK, pos);
}

ASTNode* ast_create_continue(SourcePos pos) {
    return ast_alloc_node(AST_CONTINUE, pos);
}

ASTNode* ast_create_expr_stmt(SourcePos pos, ASTNode* expr) {
    ASTNode* node = ast_alloc_node(AST_EXPR_STMT, pos);
    node->as.expr_stmt.expr = expr;
    return node;
}

ASTNode* ast_create_use(SourcePos pos, const char* module_path) {
    ASTNode* node = ast_alloc_node(AST_USE, pos);
    node->as.use_stmt.module_path = yalu_strdup(module_path);
    return node;
}

ASTNode* ast_create_class_decl(SourcePos pos, const char* name, ASTNode** methods, size_t method_count) {
    ASTNode* node = ast_alloc_node(AST_CLASS_DECL, pos);
    node->as.class_decl.name = yalu_strdup(name);
    node->as.class_decl.methods = methods;
    node->as.class_decl.method_count = method_count;
    return node;
}

ASTNode* ast_create_literal(SourcePos pos, Value val) {
    ASTNode* node = ast_alloc_node(AST_LITERAL, pos);
    node->as.literal.val = val;
    return node;
}

ASTNode* ast_create_string_interp(SourcePos pos, ASTNode** parts, size_t count) {
    ASTNode* node = ast_alloc_node(AST_STRING_INTERP, pos);
    node->as.string_interp.parts = parts;
    node->as.string_interp.count = count;
    return node;
}

ASTNode* ast_create_var_ref(SourcePos pos, const char* name) {
    ASTNode* node = ast_alloc_node(AST_VAR_REF, pos);
    node->as.var_ref.name = yalu_strdup(name);
    return node;
}

ASTNode* ast_create_binary_op(SourcePos pos, TokenType op, ASTNode* left, ASTNode* right) {
    ASTNode* node = ast_alloc_node(AST_BINARY_OP, pos);
    node->as.binary_op.op = op;
    node->as.binary_op.left = left;
    node->as.binary_op.right = right;
    return node;
}

ASTNode* ast_create_unary_op(SourcePos pos, TokenType op, ASTNode* right) {
    ASTNode* node = ast_alloc_node(AST_UNARY_OP, pos);
    node->as.unary_op.op = op;
    node->as.unary_op.right = right;
    return node;
}

ASTNode* ast_create_call(SourcePos pos, ASTNode* callee, ASTNode** args, size_t arg_count) {
    ASTNode* node = ast_alloc_node(AST_CALL, pos);
    node->as.call.callee = callee;
    node->as.call.args = args;
    node->as.call.arg_count = arg_count;
    return node;
}

ASTNode* ast_create_input(SourcePos pos, TokenType input_type, ASTNode* prompt) {
    ASTNode* node = ast_alloc_node(AST_INPUT, pos);
    node->as.input.input_type = input_type;
    node->as.input.prompt = prompt;
    return node;
}

ASTNode* ast_create_range(SourcePos pos, ASTNode* start, ASTNode* end, ASTNode* step) {
    ASTNode* node = ast_alloc_node(AST_RANGE, pos);
    node->as.range.start = start;
    node->as.range.end = end;
    node->as.range.step = step;
    return node;
}

ASTNode* ast_create_array_lit(SourcePos pos, ASTNode** elements, size_t count) {
    ASTNode* node = ast_alloc_node(AST_ARRAY_LIT, pos);
    node->as.array_lit.elements = elements;
    node->as.array_lit.count = count;
    return node;
}

ASTNode* ast_create_book_lit(SourcePos pos, ASTBookEntry* entries, size_t count) {
    ASTNode* node = ast_alloc_node(AST_BOOK_LIT, pos);
    node->as.book_lit.entries = entries;
    node->as.book_lit.count = count;
    return node;
}

ASTNode* ast_create_index(SourcePos pos, ASTNode* target, ASTNode* index) {
    ASTNode* node = ast_alloc_node(AST_INDEX, pos);
    node->as.index.target = target;
    node->as.index.index = index;
    return node;
}

ASTNode* ast_create_prop_access(SourcePos pos, ASTNode* target, const char* prop_name) {
    ASTNode* node = ast_alloc_node(AST_PROP_ACCESS, pos);
    node->as.prop_access.target = target;
    node->as.prop_access.prop_name = yalu_strdup(prop_name);
    return node;
}

ASTNode* ast_create_method_call(SourcePos pos, ASTNode* target, const char* method_name, ASTNode** args, size_t arg_count) {
    ASTNode* node = ast_alloc_node(AST_METHOD_CALL, pos);
    node->as.method_call.target = target;
    node->as.method_call.method_name = yalu_strdup(method_name);
    node->as.method_call.args = args;
    node->as.method_call.arg_count = arg_count;
    return node;
}

ASTNode* ast_create_new_object(SourcePos pos, const char* class_name, ASTNode** args, size_t arg_count) {
    ASTNode* node = ast_alloc_node(AST_NEW_OBJECT, pos);
    node->as.new_object.class_name = yalu_strdup(class_name);
    node->as.new_object.args = args;
    node->as.new_object.arg_count = arg_count;
    return node;
}

ASTNode* ast_create_fullclass(SourcePos pos) {
    return ast_alloc_node(AST_FULLCLASS, pos);
}

void ast_free(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                ast_free(node->as.block.stmts[i]);
            }
            yalu_free(node->as.block.stmts);
            break;

        case AST_VAR_ASSIGN:
            yalu_free(node->as.var_assign.name);
            ast_free(node->as.var_assign.value);
            break;

        case AST_INDEX_ASSIGN:
            ast_free(node->as.index_assign.target);
            ast_free(node->as.index_assign.index);
            ast_free(node->as.index_assign.value);
            break;

        case AST_PROP_ASSIGN:
            ast_free(node->as.prop_assign.target);
            yalu_free(node->as.prop_assign.prop_name);
            ast_free(node->as.prop_assign.value);
            break;

        case AST_CLASS_PROP_ASSIGN:
            yalu_free(node->as.class_prop_assign.prop_name);
            yalu_free(node->as.class_prop_assign.class_name);
            ast_free(node->as.class_prop_assign.value);
            break;

        case AST_PRINT:
            for (size_t i = 0; i < node->as.print.count; i++) {
                ast_free(node->as.print.args[i]);
            }
            yalu_free(node->as.print.args);
            break;

        case AST_FN_DECL:
            yalu_free(node->as.fn_decl.name);
            for (size_t i = 0; i < node->as.fn_decl.param_count; i++) {
                yalu_free(node->as.fn_decl.param_names[i]);
                if (node->as.fn_decl.param_defaults && node->as.fn_decl.param_defaults[i]) {
                    ast_free(node->as.fn_decl.param_defaults[i]);
                }
                if (node->as.fn_decl.param_types && node->as.fn_decl.param_types[i]) {
                    yalu_free(node->as.fn_decl.param_types[i]);
                }
            }
            yalu_free(node->as.fn_decl.param_names);
            yalu_free(node->as.fn_decl.param_defaults);
            if (node->as.fn_decl.param_types) yalu_free(node->as.fn_decl.param_types);
            if (node->as.fn_decl.is_yclass) yalu_free(node->as.fn_decl.is_yclass);
            ast_free(node->as.fn_decl.body);
            break;

        case AST_WHILE:
            ast_free(node->as.while_loop.condition);
            ast_free(node->as.while_loop.body);
            break;

        case AST_FOR:
            yalu_free(node->as.for_loop.var_name);
            ast_free(node->as.for_loop.iterable);
            ast_free(node->as.for_loop.separator);
            ast_free(node->as.for_loop.start);
            ast_free(node->as.for_loop.end);
            ast_free(node->as.for_loop.step);
            ast_free(node->as.for_loop.body);
            break;

        case AST_IF:
            for (size_t i = 0; i < node->as.if_stmt.branch_count; i++) {
                ast_free(node->as.if_stmt.conditions[i]);
                ast_free(node->as.if_stmt.bodies[i]);
            }
            yalu_free(node->as.if_stmt.conditions);
            yalu_free(node->as.if_stmt.bodies);
            ast_free(node->as.if_stmt.else_body);
            break;

        case AST_RETURN:
            ast_free(node->as.return_stmt.value);
            break;

        case AST_BREAK:
        case AST_CONTINUE:
        case AST_FULLCLASS:
            break;

        case AST_EXPR_STMT:
            ast_free(node->as.expr_stmt.expr);
            break;

        case AST_USE:
            yalu_free(node->as.use_stmt.module_path);
            break;

        case AST_CLASS_DECL:
            yalu_free(node->as.class_decl.name);
            for (size_t i = 0; i < node->as.class_decl.method_count; i++) {
                ast_free(node->as.class_decl.methods[i]);
            }
            yalu_free(node->as.class_decl.methods);
            break;

        case AST_LITERAL:
            value_release(node->as.literal.val);
            break;

        case AST_STRING_INTERP:
            for (size_t i = 0; i < node->as.string_interp.count; i++) {
                ast_free(node->as.string_interp.parts[i]);
            }
            yalu_free(node->as.string_interp.parts);
            break;

        case AST_VAR_REF:
            yalu_free(node->as.var_ref.name);
            break;

        case AST_BINARY_OP:
            ast_free(node->as.binary_op.left);
            ast_free(node->as.binary_op.right);
            break;

        case AST_UNARY_OP:
            ast_free(node->as.unary_op.right);
            break;

        case AST_CALL:
            ast_free(node->as.call.callee);
            for (size_t i = 0; i < node->as.call.arg_count; i++) {
                ast_free(node->as.call.args[i]);
            }
            yalu_free(node->as.call.args);
            break;

        case AST_INPUT:
            ast_free(node->as.input.prompt);
            break;

        case AST_RANGE:
            ast_free(node->as.range.start);
            ast_free(node->as.range.end);
            ast_free(node->as.range.step);
            break;

        case AST_ARRAY_LIT:
            for (size_t i = 0; i < node->as.array_lit.count; i++) {
                ast_free(node->as.array_lit.elements[i]);
            }
            yalu_free(node->as.array_lit.elements);
            break;

        case AST_BOOK_LIT:
            for (size_t i = 0; i < node->as.book_lit.count; i++) {
                ASTBookEntry* e = &node->as.book_lit.entries[i];
                for (size_t j = 0; j < e->pair_count; j++) {
                    yalu_free(e->keys[j]);
                    ast_free(e->values[j]);
                }
                yalu_free(e->keys);
                yalu_free(e->values);
            }
            yalu_free(node->as.book_lit.entries);
            break;

        case AST_INDEX:
            ast_free(node->as.index.target);
            ast_free(node->as.index.index);
            break;

        case AST_PROP_ACCESS:
            ast_free(node->as.prop_access.target);
            yalu_free(node->as.prop_access.prop_name);
            break;

        case AST_METHOD_CALL:
            ast_free(node->as.method_call.target);
            yalu_free(node->as.method_call.method_name);
            for (size_t i = 0; i < node->as.method_call.arg_count; i++) {
                ast_free(node->as.method_call.args[i]);
            }
            yalu_free(node->as.method_call.args);
            break;

        case AST_NEW_OBJECT:
            yalu_free(node->as.new_object.class_name);
            for (size_t i = 0; i < node->as.new_object.arg_count; i++) {
                ast_free(node->as.new_object.args[i]);
            }
            yalu_free(node->as.new_object.args);
            break;
    }

    yalu_free(node);
}
