#include "interpreter.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

/* Forward declarations */
static Value call_instance_method(Interpreter* interp, InstanceObj* inst, FunctionObj* fn, size_t argc, ASTNode** arg_nodes);
static char* interp_instance_to_string(Interpreter* interp, InstanceObj* inst);

void interp_init(Interpreter* interp) {
    interp->globals = runtime_init_globals();
    interp->current_env = interp->globals;
    interp->is_returning = false;
    interp->return_val = value_make_null();
    interp->is_breaking = false;
    interp->is_continuing = false;
    interp->call_depth = 0;
    interp->step_count = 0;
    interp->max_steps = 0; /* 0 = unlimited */
    interp->had_runtime_error = false;

    interp->current_filepath = NULL;
    interp->loaded_modules = NULL;
    interp->loaded_count = 0;
    interp->loaded_cap = 0;
    interp->import_stack = NULL;
    interp->import_stack_depth = 0;
    interp->import_stack_cap = 0;
    interp->current_instance = NULL;

    interp->in_sep_loop = false;
    interp->sep_string = NULL;
    interp->loop_current_idx = 0;
    interp->loop_total_count = 0;
}

void interp_free(Interpreter* interp) {
    if (interp->is_returning) {
        value_release(interp->return_val);
        interp->is_returning = false;
    }
    if (interp->globals) {
        env_release(interp->globals);
        interp->globals = NULL;
        interp->current_env = NULL;
    }
    if (interp->current_filepath) {
        yalu_free(interp->current_filepath);
        interp->current_filepath = NULL;
    }
    for (size_t i = 0; i < interp->loaded_count; i++) {
        yalu_free(interp->loaded_modules[i].canonical_path);
        env_release(interp->loaded_modules[i].env);
        if (interp->loaded_modules[i].ast) {
            ast_free(interp->loaded_modules[i].ast);
        }
        if (interp->loaded_modules[i].source) {
            yalu_free(interp->loaded_modules[i].source);
        }
    }
    if (interp->loaded_modules) {
        yalu_free(interp->loaded_modules);
        interp->loaded_modules = NULL;
        interp->loaded_count = 0;
        interp->loaded_cap = 0;
    }
    for (size_t i = 0; i < interp->import_stack_depth; i++) {
        yalu_free(interp->import_stack[i]);
    }
    if (interp->import_stack) {
        yalu_free(interp->import_stack);
        interp->import_stack = NULL;
        interp->import_stack_depth = 0;
        interp->import_stack_cap = 0;
    }
    if (interp->sep_string) {
        yalu_free(interp->sep_string);
        interp->sep_string = NULL;
    }
}

void interp_set_current_filepath(Interpreter* interp, const char* filepath) {
    if (interp->current_filepath) {
        yalu_free(interp->current_filepath);
        interp->current_filepath = NULL;
    }
    if (!filepath || strcmp(filepath, "<repl>") == 0) {
        interp->current_filepath = yalu_strdup("<repl>");
        return;
    }
#ifdef _WIN32
    char* full = _fullpath(NULL, filepath, 0);
    if (full) {
        for (char* p = full; *p; p++) {
            if (*p == '\\') *p = '/';
        }
        interp->current_filepath = full;
        return;
    }
#endif
    interp->current_filepath = yalu_strdup(filepath);
}

static char* get_directory_of_file(const char* filepath) {
    if (!filepath || strcmp(filepath, "<repl>") == 0) {
        return yalu_strdup(".");
    }
    char* copy = yalu_strdup(filepath);
    char* last_slash = strrchr(copy, '/');
    char* last_backslash = strrchr(copy, '\\');
    char* slash = last_slash > last_backslash ? last_slash : last_backslash;
    if (slash) {
        *slash = '\0';
        return copy;
    }
    yalu_free(copy);
    return yalu_strdup(".");
}

static char* test_module_candidate(const char* base_dir, const char* subpath) {
    char combined[1024];
    if (base_dir && strcmp(base_dir, ".") != 0) {
        snprintf(combined, sizeof(combined), "%s/%s", base_dir, subpath);
    } else {
        snprintf(combined, sizeof(combined), "%s", subpath);
    }
    FILE* f = fopen(combined, "rb");
    if (f) {
        fclose(f);
#ifdef _WIN32
        char* full = _fullpath(NULL, combined, 0);
        if (full) {
            for (char* p = full; *p; p++) {
                if (*p == '\\') *p = '/';
            }
            return full;
        }
#endif
        return yalu_strdup(combined);
    }
    return NULL;
}

static char* resolve_module(Interpreter* interp, const char* mod_spec, SourcePos pos) {
    char* dir = get_directory_of_file(interp->current_filepath);
    char* found = NULL;

    /* 1. Try directly in file's directory */
    found = test_module_candidate(dir, mod_spec);
    if (found) { yalu_free(dir); return found; }

    /* 2. Try with .yl extension */
    char spec_yl[512];
    snprintf(spec_yl, sizeof(spec_yl), "%s.yl", mod_spec);
    found = test_module_candidate(dir, spec_yl);
    if (found) { yalu_free(dir); return found; }

    /* 3. Try <mod>/main.yl or <mod>/init.yl */
    char spec_sub[512];
    snprintf(spec_sub, sizeof(spec_sub), "%s/main.yl", mod_spec);
    found = test_module_candidate(dir, spec_sub);
    if (found) { yalu_free(dir); return found; }

    snprintf(spec_sub, sizeof(spec_sub), "%s/init.yl", mod_spec);
    found = test_module_candidate(dir, spec_sub);
    if (found) { yalu_free(dir); return found; }

    /* 4. If not explicitly relative (no leading . or /), search CWD and ./modules/ */
    if (mod_spec[0] != '.' && mod_spec[0] != '/' && mod_spec[0] != '\\') {
        found = test_module_candidate(".", mod_spec);
        if (found) { yalu_free(dir); return found; }

        found = test_module_candidate(".", spec_yl);
        if (found) { yalu_free(dir); return found; }

        char spec_mod[512];
        snprintf(spec_mod, sizeof(spec_mod), "modules/%s.yl", mod_spec);
        found = test_module_candidate(".", spec_mod);
        if (found) { yalu_free(dir); return found; }

        snprintf(spec_mod, sizeof(spec_mod), "modules/%s/main.yl", mod_spec);
        found = test_module_candidate(".", spec_mod);
        if (found) { yalu_free(dir); return found; }
    }

    yalu_report_error(pos, "Module Error", "Cannot find module '%s' (searched relative to '%s')", mod_spec, dir);
    interp->had_runtime_error = true;
    yalu_free(dir);
    return NULL;
}

static void exec_use(Interpreter* interp, ASTNode* stmt) {
    const char* mod_spec = stmt->as.use_stmt.module_path;
    char* canonical = resolve_module(interp, mod_spec, stmt->pos);
    if (!canonical) return;

    /* Check circular import */
    for (size_t i = 0; i < interp->import_stack_depth; i++) {
        if (strcmp(interp->import_stack[i], canonical) == 0) {
            yalu_report_error(stmt->pos, "Module Error", "Circular import detected: '%s' is already being imported", canonical);
            interp->had_runtime_error = true;
            yalu_free(canonical);
            return;
        }
    }

    /* Check cache */
    for (size_t i = 0; i < interp->loaded_count; i++) {
        if (strcmp(interp->loaded_modules[i].canonical_path, canonical) == 0) {
            env_export_symbols(interp->loaded_modules[i].env, interp->current_env);
            yalu_free(canonical);
            return;
        }
    }

    /* Read module file */
    FILE* f = fopen(canonical, "rb");
    if (!f) {
        yalu_report_error(stmt->pos, "Module Error", "Cannot open module file '%s'", canonical);
        interp->had_runtime_error = true;
        yalu_free(canonical);
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = (char*)yalu_malloc((size_t)sz + 1);
    size_t rd = fread(src, 1, (size_t)sz, f);
    src[rd] = '\0';
    fclose(f);

    /* Strip BOM */
    if ((unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) {
        memmove(src, src + 3, rd - 3 + 1);
    }

    /* Push import stack */
    if (interp->import_stack_depth >= interp->import_stack_cap) {
        interp->import_stack_cap = interp->import_stack_cap == 0 ? 8 : interp->import_stack_cap * 2;
        interp->import_stack = (char**)yalu_realloc(interp->import_stack, sizeof(char*) * interp->import_stack_cap);
    }
    interp->import_stack[interp->import_stack_depth++] = yalu_strdup(canonical);

    /* Lex & Parse */
    Lexer mod_lexer;
    lexer_init(&mod_lexer, canonical, src);
    Parser mod_parser;
    parser_init(&mod_parser, &mod_lexer, canonical);
    ASTNode* mod_prog = parser_parse(&mod_parser);

    if (!mod_prog || mod_parser.had_error) {
        yalu_report_error(stmt->pos, "Module Error", "Failed to parse module '%s'", canonical);
        interp->had_runtime_error = true;
        yalu_free(interp->import_stack[--interp->import_stack_depth]);
        if (mod_prog) ast_free(mod_prog);
        yalu_free(src);
        yalu_free(canonical);
        return;
    }

    /* Create fresh module environment inheriting from globals */
    Environment* mod_env = env_create(interp->globals);
    char* prev_file = interp->current_filepath;
    Environment* prev_env = interp->current_env;

    interp->current_filepath = yalu_strdup(canonical);
    interp->current_env = mod_env;

    interp_exec_stmt(interp, mod_prog);

    /* Restore execution context */
    yalu_free(interp->current_filepath);
    interp->current_filepath = prev_file;
    interp->current_env = prev_env;

    /* Pop import stack */
    yalu_free(interp->import_stack[--interp->import_stack_depth]);

    /* Cache module (transfer ownership of ast and source to cache) */
    if (interp->loaded_count >= interp->loaded_cap) {
        interp->loaded_cap = interp->loaded_cap == 0 ? 8 : interp->loaded_cap * 2;
        interp->loaded_modules = (LoadedModule*)yalu_realloc(interp->loaded_modules, sizeof(LoadedModule) * interp->loaded_cap);
    }
    interp->loaded_modules[interp->loaded_count].canonical_path = canonical;
    interp->loaded_modules[interp->loaded_count].env = mod_env;
    interp->loaded_modules[interp->loaded_count].ast = mod_prog;
    interp->loaded_modules[interp->loaded_count].source = src;
    env_retain(mod_env);
    interp->loaded_count++;

    /* Export symbols into current environment */
    env_export_symbols(mod_env, interp->current_env);
}

static char* read_line_stdin(void) {
    size_t cap = 128;
    size_t len = 0;
    char* buf = (char*)yalu_malloc(cap);
    int c;

    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (c == '\r') continue;
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char*)yalu_realloc(buf, cap);
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return buf;
}

static Value eval_string_interp(Interpreter* interp, ASTNode* node) {
    size_t total_cap = 64;
    size_t total_len = 0;
    char* result = (char*)yalu_malloc(total_cap);
    result[0] = '\0';

    for (size_t i = 0; i < node->as.string_interp.count; i++) {
        Value part_val = interp_eval_expr(interp, node->as.string_interp.parts[i]);
        char* str_repr = NULL;
        if (part_val.type == VAL_INSTANCE) {
            str_repr = interp_instance_to_string(interp, part_val.as.instance);
        } else {
            str_repr = value_to_string(part_val);
        }
        size_t repr_len = strlen(str_repr);

        if (total_len + repr_len + 1 >= total_cap) {
            while (total_len + repr_len + 1 >= total_cap) {
                total_cap *= 2;
            }
            result = (char*)yalu_realloc(result, total_cap);
        }

        memcpy(result + total_len, str_repr, repr_len);
        total_len += repr_len;
        result[total_len] = '\0';

        yalu_free(str_repr);
        value_release(part_val);
    }

    Value v = value_make_string(result, total_len);
    yalu_free(result);
    return v;
}

static Value eval_input(Interpreter* interp, ASTNode* node) {
    if (node->as.input.prompt) {
        Value prompt_val = interp_eval_expr(interp, node->as.input.prompt);
        char* prompt_str = value_to_string(prompt_val);
        printf("%s", prompt_str);
        fflush(stdout);
        yalu_free(prompt_str);
        value_release(prompt_val);
    }

    char* line = read_line_stdin();
    if (!line) line = yalu_strdup("");

    if (node->as.input.input_type == TOKEN_INP_STR) {
        Value v = value_make_string(line, strlen(line));
        yalu_free(line);
        return v;
    }

    if (node->as.input.input_type == TOKEN_INP_INT) {
        char* start = line;
        while (isspace((unsigned char)*start)) start++;
        char* end = start + strlen(start) - 1;
        while (end >= start && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }

        bool valid = true;
        char* p = start;
        if (*p == '+' || *p == '-') p++;
        if (*p == '\0') valid = false;
        while (*p) {
            if (!isdigit((unsigned char)*p)) {
                valid = false;
                break;
            }
            p++;
        }

        if (!valid) {
            yalu_report_error(node->pos, "Input Error", "Invalid integer input: '%s'", line);
            interp->had_runtime_error = true;
            yalu_free(line);
            return value_make_int(0);
        }

        int64_t val = (int64_t)strtoll(start, NULL, 10);
        yalu_free(line);
        return value_make_int(val);
    }

    if (node->as.input.input_type == TOKEN_INP_FLT) {
        char* endptr = NULL;
        double val = strtod(line, &endptr);
        while (endptr && isspace((unsigned char)*endptr)) endptr++;
        if (!endptr || *endptr != '\0') {
            yalu_report_error(node->pos, "Input Error", "Invalid float input: '%s'", line);
            interp->had_runtime_error = true;
            yalu_free(line);
            return value_make_float(0.0);
        }
        yalu_free(line);
        return value_make_float(val);
    }

    yalu_free(line);
    return value_make_null();
}

static bool validate_type(Value val, const char* expected_type) {
    if (!expected_type) return true;
    if (strcmp(expected_type, "str") == 0 || strcmp(expected_type, "string") == 0) {
        return val.type == VAL_STRING;
    }
    if (strcmp(expected_type, "int") == 0 || strcmp(expected_type, "integer") == 0) {
        return val.type == VAL_INT;
    }
    if (strcmp(expected_type, "flt") == 0 || strcmp(expected_type, "float") == 0) {
        return val.type == VAL_FLOAT || val.type == VAL_INT;
    }
    if (strcmp(expected_type, "bool") == 0) {
        return val.type == VAL_BOOL;
    }
    return true;
}

static Value call_instance_method(Interpreter* interp, InstanceObj* inst, FunctionObj* fn, size_t argc, ASTNode** arg_nodes) {
    if (interp->call_depth >= YALU_MAX_CALL_DEPTH) {
        yalu_report_error(fn->decl->pos, "Runtime Error", "Maximum recursion depth exceeded (%d)", YALU_MAX_CALL_DEPTH);
        interp->had_runtime_error = true;
        return value_make_null();
    }

    ASTNode* decl = fn->decl;
    Environment* call_env = env_create(fn->closure);
    Environment* prev_env = interp->current_env;
    InstanceObj* prev_inst = interp->current_instance;

    /* Bind yclass automatically */
    Value inst_val = value_make_instance(inst->klass);
    /* Release dummy instance and point directly to inst */
    value_release(inst_val);
    inst_val.type = VAL_INSTANCE;
    inst_val.as.instance = inst;
    env_define(call_env, "yclass", inst_val);

    size_t user_arg_idx = 0;
    for (size_t i = 0; i < decl->as.fn_decl.param_count; i++) {
        const char* param_name = decl->as.fn_decl.param_names[i];
        bool is_yc = decl->as.fn_decl.is_yclass ? decl->as.fn_decl.is_yclass[i] : false;
        if (strcmp(param_name, "yclass") == 0) is_yc = true;

        Value arg_val;
        if (is_yc) {
            arg_val = value_copy(inst_val);
        } else {
            if (user_arg_idx < argc) {
                arg_val = interp_eval_expr(interp, arg_nodes[user_arg_idx++]);
            } else if (decl->as.fn_decl.param_defaults && decl->as.fn_decl.param_defaults[i]) {
                interp->current_env = call_env;
                arg_val = interp_eval_expr(interp, decl->as.fn_decl.param_defaults[i]);
                interp->current_env = prev_env;
            } else {
                yalu_report_error(decl->pos, "Runtime Error", "Missing required argument for parameter '%s' in method '%s'",
                                  param_name, fn->name ? fn->name : "anonymous");
                interp->had_runtime_error = true;
                arg_val = value_make_null();
            }

            if (decl->as.fn_decl.param_types && decl->as.fn_decl.param_types[i]) {
                const char* exp_type = decl->as.fn_decl.param_types[i];
                if (!validate_type(arg_val, exp_type)) {
                    yalu_report_error(decl->pos, "Type Error", "Parameter '%s' expects type '%s', got '%s'",
                                      param_name, exp_type, value_type_name(arg_val.type));
                    interp->had_runtime_error = true;
                    env_define(call_env, param_name, arg_val);
                    value_release(arg_val);
                    break;
                }
            }
        }

        env_define(call_env, param_name, arg_val);
        value_release(arg_val);
    }

    interp->call_depth++;
    interp->current_env = call_env;
    interp->current_instance = inst;

    interp_exec_stmt(interp, decl->as.fn_decl.body);

    Value res = value_make_null();
    if (interp->is_returning) {
        res = interp->return_val;
        interp->is_returning = false;
        interp->return_val = value_make_null();
    }

    interp->call_depth--;
    interp->current_env = prev_env;
    interp->current_instance = prev_inst;
    env_release(call_env);
    return res;
}

static char* interp_instance_to_string(Interpreter* interp, InstanceObj* inst) {
    if (!inst) return yalu_strdup("none");

    if (inst->in_yalupr) {
        /* Recursion protection if yalupr triggers yalupr */
        return instance_to_string(inst, 0);
    }

    if (inst->klass && inst->klass->methods) {
        Value yalupr_fn;
        if (env_get(inst->klass->methods, "yalupr", &yalupr_fn)) {
            if (yalupr_fn.type == VAL_FUNCTION) {
                inst->in_yalupr = true;
                Value res = call_instance_method(interp, inst, yalupr_fn.as.function, 0, NULL);
                inst->in_yalupr = false;
                value_release(yalupr_fn);
                char* s = value_to_string(res);
                value_release(res);
                return s;
            }
            value_release(yalupr_fn);
        }
    }

    return instance_to_string(inst, 0);
}

static Value eval_call(Interpreter* interp, ASTNode* node) {
    /* Check if callee is a method call on an instance: target>method(...) */
    if (node->as.call.callee->type == AST_PROP_ACCESS) {
        ASTNode* pa = node->as.call.callee;
        Value target = interp_eval_expr(interp, pa->as.prop_access.target);

        if (target.type == VAL_INSTANCE) {
            const char* m_name = pa->as.prop_access.prop_name;
            Value m_val;
            if (target.as.instance->klass && target.as.instance->klass->methods &&
                env_get(target.as.instance->klass->methods, m_name, &m_val)) {

                if (m_val.type == VAL_FUNCTION) {
                    Value res = call_instance_method(interp, target.as.instance, m_val.as.function,
                                                     node->as.call.arg_count, node->as.call.args);
                    value_release(m_val);
                    value_release(target);
                    return res;
                }
                value_release(m_val);
            }
        }
        value_release(target);
    }

    Value callee = interp_eval_expr(interp, node->as.call.callee);

    if (callee.type == VAL_NATIVE_FN) {
        size_t argc = node->as.call.arg_count;
        Value* argv = (Value*)yalu_malloc(sizeof(Value) * (argc > 0 ? argc : 1));
        for (size_t i = 0; i < argc; i++) {
            argv[i] = interp_eval_expr(interp, node->as.call.args[i]);
        }

        Value res = callee.as.native_fn((int)argc, argv, node->pos);

        for (size_t i = 0; i < argc; i++) {
            value_release(argv[i]);
        }
        yalu_free(argv);
        value_release(callee);
        return res;
    }

    if (callee.type == VAL_FUNCTION) {
        if (interp->call_depth >= YALU_MAX_CALL_DEPTH) {
            yalu_report_error(node->pos, "Runtime Error", "Maximum recursion depth exceeded (%d)", YALU_MAX_CALL_DEPTH);
            interp->had_runtime_error = true;
            value_release(callee);
            return value_make_null();
        }

        FunctionObj* fn = callee.as.function;
        ASTNode* decl = fn->decl;

        Environment* call_env = env_create(fn->closure);
        Environment* prev_env = interp->current_env;

        for (size_t i = 0; i < decl->as.fn_decl.param_count; i++) {
            const char* param_name = decl->as.fn_decl.param_names[i];
            Value arg_val;

            if (i < node->as.call.arg_count) {
                arg_val = interp_eval_expr(interp, node->as.call.args[i]);
            } else if (decl->as.fn_decl.param_defaults && decl->as.fn_decl.param_defaults[i]) {
                interp->current_env = call_env;
                arg_val = interp_eval_expr(interp, decl->as.fn_decl.param_defaults[i]);
                interp->current_env = prev_env;
            } else {
                arg_val = value_make_null();
            }

            if (decl->as.fn_decl.param_types && decl->as.fn_decl.param_types[i]) {
                const char* exp_type = decl->as.fn_decl.param_types[i];
                if (!validate_type(arg_val, exp_type)) {
                    yalu_report_error(node->pos, "Type Error", "Parameter '%s' expects type '%s', got '%s'",
                                      param_name, exp_type, value_type_name(arg_val.type));
                    interp->had_runtime_error = true;
                    env_define(call_env, param_name, arg_val);
                    value_release(arg_val);
                    break;
                }
            }

            env_define(call_env, param_name, arg_val);
            value_release(arg_val);
        }

        interp->call_depth++;
        interp->current_env = call_env;

        interp_exec_stmt(interp, decl->as.fn_decl.body);

        Value res = value_make_null();
        if (interp->is_returning) {
            res = interp->return_val;
            interp->is_returning = false;
            interp->return_val = value_make_null();
        }

        interp->call_depth--;
        interp->current_env = prev_env;
        env_release(call_env);
        value_release(callee);
        return res;
    }

    /* Class instantiation via call syntax Point(...) */
    if (callee.type == VAL_CLASS) {
        ClassObj* klass = callee.as.klass;
        Value inst_val = value_make_instance(klass);

        Value inti_val;
        if (klass->methods && env_get(klass->methods, "inti", &inti_val)) {
            if (inti_val.type == VAL_FUNCTION) {
                Value res = call_instance_method(interp, inst_val.as.instance, inti_val.as.function,
                                                 node->as.call.arg_count, node->as.call.args);
                value_release(res);
            }
            value_release(inti_val);
        }
        value_release(callee);
        return inst_val;
    }

    yalu_report_error(node->pos, "Type Error", "Cannot call non-function value of type '%s'", value_type_name(callee.type));
    interp->had_runtime_error = true;
    value_release(callee);
    return value_make_null();
}

static Value eval_new_object(Interpreter* interp, ASTNode* node) {
    const char* class_name = node->as.new_object.class_name;
    Value class_val;

    if (!env_get(interp->current_env, class_name, &class_val)) {
        yalu_report_error(node->pos, "Runtime Error", "Undefined class '%s'", class_name);
        interp->had_runtime_error = true;
        return value_make_null();
    }

    if (class_val.type != VAL_CLASS) {
        yalu_report_error(node->pos, "Type Error", "'%s' is not a class", class_name);
        interp->had_runtime_error = true;
        value_release(class_val);
        return value_make_null();
    }

    ClassObj* klass = class_val.as.klass;
    Value inst_val = value_make_instance(klass);

    Value inti_val;
    if (klass->methods && env_get(klass->methods, "inti", &inti_val)) {
        if (inti_val.type == VAL_FUNCTION) {
            Value res = call_instance_method(interp, inst_val.as.instance, inti_val.as.function,
                                             node->as.new_object.arg_count, node->as.new_object.args);
            value_release(res);
        }
        value_release(inti_val);
    } else if (node->as.new_object.arg_count > 0) {
        yalu_report_error(node->pos, "Runtime Error", "Class '%s' has no constructor 'inti' but arguments were provided", class_name);
        interp->had_runtime_error = true;
    }

    value_release(class_val);
    return inst_val;
}

static Value eval_prop_access(Interpreter* interp, ASTNode* node) {
    Value target = interp_eval_expr(interp, node->as.prop_access.target);
    const char* prop_name = node->as.prop_access.prop_name;

    if (target.type == VAL_BOOK) {
        if (strcmp(prop_name, "vals") == 0 || strcmp(prop_name, "val") == 0) {
            Value res = value_make_book_vals(target.as.book);
            value_release(target);
            return res;
        }
        if (strcmp(prop_name, "keys") == 0 || strcmp(prop_name, "key") == 0) {
            Value res = value_make_book_keys(target.as.book);
            value_release(target);
            return res;
        }
        Value val;
        if (book_get(target.as.book, prop_name, &val)) {
            value_release(target);
            return val;
        }
        if (book_reverse_lookup(target.as.book, prop_name, &val)) {
            value_release(target);
            return val;
        }
        yalu_report_error(node->pos, "Key Error", "Key or value '%s' not found in Book", prop_name);
        interp->had_runtime_error = true;
        value_release(target);
        return value_make_null();
    }

    if (target.type != VAL_INSTANCE) {
        yalu_report_error(node->pos, "Type Error", "Cannot access property '%s' on non-instance of type '%s'",
                          prop_name, value_type_name(target.type));
        interp->had_runtime_error = true;
        value_release(target);
        return value_make_null();
    }

    InstanceObj* inst = target.as.instance;
    Value prop_val;
    if (instance_get_property(inst, prop_name, &prop_val)) {
        value_release(target);
        return prop_val;
    }

    if (inst->klass && inst->klass->methods && env_get(inst->klass->methods, prop_name, &prop_val)) {
        value_release(target);
        return prop_val;
    }

    yalu_report_error(node->pos, "Runtime Error", "Object of class '%s' has no property or method '%s'",
                      inst->klass ? inst->klass->name : "Object", prop_name);
    interp->had_runtime_error = true;
    value_release(target);
    return value_make_null();
}

static Value eval_method_call(Interpreter* interp, ASTNode* node) {
    Value target = interp_eval_expr(interp, node->as.method_call.target);
    const char* method = node->as.method_call.method_name;
    size_t argc = node->as.method_call.arg_count;
    ASTNode** arg_nodes = node->as.method_call.args;

    if (target.type == VAL_ARRAY) {
        ArrayObj* arr = target.as.array;
        if (strcmp(method, "inc") == 0) {
            if (argc < 1) {
                yalu_report_error(node->pos, "Argument Error", "inc() expects at least 1 argument");
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }
            for (size_t i = 0; i < argc; i++) {
                Value val = interp_eval_expr(interp, arg_nodes[i]);
                array_inc(arr, val);
                value_release(val);
            }
            return target;
        }
        if (strcmp(method, "rem") == 0) {
            if (argc < 1) {
                yalu_report_error(node->pos, "Argument Error", "rem() expects 1 argument");
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }
            Value val = interp_eval_expr(interp, arg_nodes[0]);
            array_rem(arr, val);
            value_release(val);
            return target;
        }
        if (strcmp(method, "rep") == 0) {
            if (argc < 2) {
                yalu_report_error(node->pos, "Argument Error", "rep() expects 2 arguments: rep(old, new)");
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }
            Value old_val = interp_eval_expr(interp, arg_nodes[0]);
            Value new_val = interp_eval_expr(interp, arg_nodes[1]);
            array_rep(arr, old_val, new_val);
            value_release(old_val);
            value_release(new_val);
            return target;
        }
        if (strcmp(method, "haveit") == 0) {
            if (argc < 1) {
                yalu_report_error(node->pos, "Argument Error", "haveit() expects 1 argument");
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }
            Value val = interp_eval_expr(interp, arg_nodes[0]);
            bool found = array_haveit(arr, val);
            value_release(val);
            value_release(target);
            return value_make_bool(found);
        }
        yalu_report_error(node->pos, "Runtime Error", "Unknown List method '%s'", method);
        interp->had_runtime_error = true;
        value_release(target);
        return value_make_null();
    }

    if (target.type == VAL_BOOK) {
        BookObj* book = target.as.book;
        if (strcmp(method, "add") == 0) {
            if (argc == 1) {
                Value arg_val = interp_eval_expr(interp, arg_nodes[0]);
                if (arg_val.type == VAL_BOOK) {
                    BookObj* src = arg_val.as.book;
                    for (size_t i = 0; i < src->entry_count; i++) {
                        BookEntry* e = &src->entries[i];
                        for (size_t j = 0; j < e->pair_count; j++) {
                            book_set(book, e->keys[j], e->values[j]);
                        }
                    }
                } else {
                    yalu_report_error(node->pos, "Argument Error", "add() with 1 argument expects key .. value pair");
                    interp->had_runtime_error = true;
                }
                value_release(arg_val);
            } else if (argc >= 2) {
                Value k_val = interp_eval_expr(interp, arg_nodes[0]);
                Value v_val = interp_eval_expr(interp, arg_nodes[1]);
                char* key_str = (k_val.type == VAL_STRING) ? yalu_strdup(k_val.as.string->chars) : value_to_string(k_val);
                book_set(book, key_str, v_val);
                yalu_free(key_str);
                value_release(k_val);
                value_release(v_val);
            } else {
                yalu_report_error(node->pos, "Argument Error", "add() expects at least 1 argument");
                interp->had_runtime_error = true;
            }
            return target;
        }
        if (strcmp(method, "edval") == 0) {
            if (argc < 2) {
                yalu_report_error(node->pos, "Argument Error", "edval() expects 2 arguments: edval(key, new_val)");
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }
            Value k_val = interp_eval_expr(interp, arg_nodes[0]);
            Value v_val = interp_eval_expr(interp, arg_nodes[1]);
            char* key_str = (k_val.type == VAL_STRING) ? yalu_strdup(k_val.as.string->chars) : value_to_string(k_val);
            book_edval(book, key_str, v_val);
            yalu_free(key_str);
            value_release(k_val);
            value_release(v_val);
            return target;
        }
        if (strcmp(method, "edkey") == 0) {
            if (argc < 2) {
                yalu_report_error(node->pos, "Argument Error", "edkey() expects 2 arguments: edkey(old_key_or_val, new_key)");
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }
            Value old_k_val = interp_eval_expr(interp, arg_nodes[0]);
            Value new_k_val = interp_eval_expr(interp, arg_nodes[1]);
            char* old_k_str = (old_k_val.type == VAL_STRING) ? yalu_strdup(old_k_val.as.string->chars) : value_to_string(old_k_val);
            char* new_k_str = (new_k_val.type == VAL_STRING) ? yalu_strdup(new_k_val.as.string->chars) : value_to_string(new_k_val);
            book_edkey(book, old_k_str, new_k_str);
            yalu_free(old_k_str);
            yalu_free(new_k_str);
            value_release(old_k_val);
            value_release(new_k_val);
            return target;
        }
        yalu_report_error(node->pos, "Runtime Error", "Unknown Book method '%s'", method);
        interp->had_runtime_error = true;
        value_release(target);
        return value_make_null();
    }

    if (target.type == VAL_INSTANCE) {
        InstanceObj* inst = target.as.instance;
        Value method_val;
        if (inst->klass && inst->klass->methods && env_get(inst->klass->methods, method, &method_val)) {
            if (method_val.type == VAL_FUNCTION) {
                Value res = call_instance_method(interp, inst, method_val.as.function, argc, arg_nodes);
                value_release(method_val);
                value_release(target);
                return res;
            }
            value_release(method_val);
        }
        yalu_report_error(node->pos, "Runtime Error", "Class '%s' has no method '%s'",
                          inst->klass ? inst->klass->name : "Object", method);
        interp->had_runtime_error = true;
        value_release(target);
        return value_make_null();
    }

    yalu_report_error(node->pos, "Type Error", "Cannot call method '%s' on value of type '%s'",
                      method, value_type_name(target.type));
    interp->had_runtime_error = true;
    value_release(target);
    return value_make_null();
}

static void exec_class_decl(Interpreter* interp, ASTNode* stmt) {
    const char* name = stmt->as.class_decl.name;
    Environment* methods = env_create(interp->current_env);

    for (size_t i = 0; i < stmt->as.class_decl.method_count; i++) {
        ASTNode* m = stmt->as.class_decl.methods[i];
        Value fn_val = value_make_function(m->as.fn_decl.name, m, methods);
        env_set(methods, m->as.fn_decl.name, fn_val);
        value_release(fn_val);
    }

    Value class_val = value_make_class(name, stmt, methods);
    env_set(interp->current_env, name, class_val);

    /* Also bind /name in environment if needed */
    char slash_name[256];
    snprintf(slash_name, sizeof(slash_name), "/%s", name);
    env_set(interp->current_env, slash_name, class_val);

    value_release(class_val);
}

Value interp_eval_expr(Interpreter* interp, ASTNode* expr) {
    if (!expr || interp->had_runtime_error) {
        return value_make_null();
    }

    switch (expr->type) {
        case AST_LITERAL:
            return value_copy(expr->as.literal.val);

        case AST_STRING_INTERP:
            return eval_string_interp(interp, expr);

        case AST_VAR_REF: {
            Value v;
            if (!env_get(interp->current_env, expr->as.var_ref.name, &v)) {
                yalu_report_error(expr->pos, "Runtime Error", "Undefined variable '%s'", expr->as.var_ref.name);
                interp->had_runtime_error = true;
                return value_make_null();
            }
            return v;
        }

        case AST_PROP_ACCESS:
            return eval_prop_access(interp, expr);

        case AST_METHOD_CALL:
            return eval_method_call(interp, expr);

        case AST_BOOK_LIT: {
            Value book_val = value_make_book(expr->as.book_lit.count);
            BookObj* book = book_val.as.book;
            for (size_t i = 0; i < expr->as.book_lit.count; i++) {
                ASTBookEntry* e = &expr->as.book_lit.entries[i];
                char** keys = (char**)yalu_malloc(sizeof(char*) * e->pair_count);
                Value* vals = (Value*)yalu_malloc(sizeof(Value) * e->pair_count);
                for (size_t j = 0; j < e->pair_count; j++) {
                    keys[j] = yalu_strdup(e->keys[j]);
                    Value val;
                    if (e->values[j]->type == AST_VAR_REF) {
                        if (!env_get(interp->current_env, e->values[j]->as.var_ref.name, &val)) {
                            val = value_make_string(e->values[j]->as.var_ref.name, strlen(e->values[j]->as.var_ref.name));
                        }
                    } else {
                        val = interp_eval_expr(interp, e->values[j]);
                    }
                    vals[j] = val;
                }
                book_add_entry(book, e->pair_count, keys, vals);
            }
            return book_val;
        }

        case AST_NEW_OBJECT:
            return eval_new_object(interp, expr);

        case AST_FULLCLASS: {
            InstanceObj* target_inst = interp->current_instance;
            if (!target_inst) {
                Value yclass_val;
                if (env_get(interp->current_env, "yclass", &yclass_val)) {
                    if (yclass_val.type == VAL_INSTANCE) {
                        target_inst = yclass_val.as.instance;
                    }
                    value_release(yclass_val);
                }
            }
            if (!target_inst) {
                yalu_report_error(expr->pos, "Runtime Error", "'fullclass' used outside of class instance context");
                interp->had_runtime_error = true;
                return value_make_null();
            }
            char* str = instance_to_string(target_inst, 0);
            Value res = value_make_string_take(str, strlen(str));
            return res;
        }

        case AST_BINARY_OP: {
            TokenType op = expr->as.binary_op.op;

            /* Short-circuit logical operators */
            if (op == TOKEN_AMP_AMP || op == TOKEN_AND) {
                Value left = interp_eval_expr(interp, expr->as.binary_op.left);
                if (!value_is_truthy(left)) {
                    return left;
                }
                value_release(left);
                return interp_eval_expr(interp, expr->as.binary_op.right);
            }

            if (op == TOKEN_PIPE_PIPE || op == TOKEN_OR) {
                Value left = interp_eval_expr(interp, expr->as.binary_op.left);
                if (value_is_truthy(left)) {
                    return left;
                }
                value_release(left);
                return interp_eval_expr(interp, expr->as.binary_op.right);
            }

            Value left = interp_eval_expr(interp, expr->as.binary_op.left);
            Value right = interp_eval_expr(interp, expr->as.binary_op.right);

            /* String concatenation */
            if (op == TOKEN_PLUS && (left.type == VAL_STRING || right.type == VAL_STRING)) {
                char* s1 = (left.type == VAL_INSTANCE) ? interp_instance_to_string(interp, left.as.instance) : value_to_string(left);
                char* s2 = (right.type == VAL_INSTANCE) ? interp_instance_to_string(interp, right.as.instance) : value_to_string(right);
                size_t l1 = strlen(s1);
                size_t l2 = strlen(s2);
                char* combined = (char*)yalu_malloc(l1 + l2 + 1);
                memcpy(combined, s1, l1);
                memcpy(combined + l1, s2, l2);
                combined[l1 + l2] = '\0';
                Value res = value_make_string_take(combined, l1 + l2);
                yalu_free(s1);
                yalu_free(s2);
                value_release(left);
                value_release(right);
                return res;
            }

            /* Equality */
            if (op == TOKEN_EQ) {
                bool eq = value_equals(left, right);
                value_release(left);
                value_release(right);
                return value_make_bool(eq);
            }
            if (op == TOKEN_NE) {
                bool eq = value_equals(left, right);
                value_release(left);
                value_release(right);
                return value_make_bool(!eq);
            }

            if (op == TOKEN_LT_GT) {
                if (left.type == VAL_ARRAY && right.type == VAL_ARRAY) {
                    char* s1 = value_to_string(left);
                    char* s2 = value_to_string(right);
                    Value res = array_concat(left.as.array, right.as.array);
                    size_t l1 = strlen(s1), l2 = strlen(s2);
                    char* combined = (char*)yalu_malloc(l1 + l2 + 3);
                    snprintf(combined, l1 + l2 + 3, "%s<>%s", s1, s2);
                    res.as.array->custom_repr = combined;
                    yalu_free(s1);
                    yalu_free(s2);
                    value_release(left);
                    value_release(right);
                    return res;
                } else {
                    Value res = value_make_array(2);
                    array_inc(res.as.array, left);
                    array_inc(res.as.array, right);
                    value_release(left);
                    value_release(right);
                    return res;
                }
            }

            if (op == TOKEN_DOT_DOT) {
                char* k_str = (left.type == VAL_STRING) ? yalu_strdup(left.as.string->chars) : value_to_string(left);
                char** keys = (char**)yalu_malloc(sizeof(char*));
                keys[0] = k_str;
                Value* vals = (Value*)yalu_malloc(sizeof(Value));
                vals[0] = value_retain(right);
                Value b = value_make_book(1);
                book_add_entry(b.as.book, 1, keys, vals);
                value_release(left);
                value_release(right);
                return b;
            }

            /* Numeric comparisons and arithmetic */
            if (left.type == VAL_INT && right.type == VAL_INT) {
                int64_t a = left.as.integer;
                int64_t b = right.as.integer;
                value_release(left);
                value_release(right);

                switch (op) {
                    case TOKEN_PLUS:    return value_make_int(a + b);
                    case TOKEN_MINUS:   return value_make_int(a - b);
                    case TOKEN_STAR:    return value_make_int(a * b);
                    case TOKEN_SLASH:
                        if (b == 0) {
                            yalu_report_error(expr->pos, "Runtime Error", "Division by zero");
                            interp->had_runtime_error = true;
                            return value_make_null();
                        }
                        return value_make_int(a / b);
                    case TOKEN_PERCENT:
                        if (b == 0) {
                            yalu_report_error(expr->pos, "Runtime Error", "Modulo by zero");
                            interp->had_runtime_error = true;
                            return value_make_null();
                        }
                        return value_make_int(a % b);
                    case TOKEN_LT:      return value_make_bool(a < b);
                    case TOKEN_LE:      return value_make_bool(a <= b);
                    case TOKEN_GT:      return value_make_bool(a > b);
                    case TOKEN_GE:      return value_make_bool(a >= b);
                    default: break;
                }
            } else if ((left.type == VAL_INT || left.type == VAL_FLOAT) &&
                       (right.type == VAL_INT || right.type == VAL_FLOAT)) {
                double a = (left.type == VAL_FLOAT) ? left.as.floating : (double)left.as.integer;
                double b = (right.type == VAL_FLOAT) ? right.as.floating : (double)right.as.integer;
                value_release(left);
                value_release(right);

                switch (op) {
                    case TOKEN_PLUS:    return value_make_float(a + b);
                    case TOKEN_MINUS:   return value_make_float(a - b);
                    case TOKEN_STAR:    return value_make_float(a * b);
                    case TOKEN_SLASH:
                        if (b == 0.0) {
                            yalu_report_error(expr->pos, "Runtime Error", "Division by zero");
                            interp->had_runtime_error = true;
                            return value_make_null();
                        }
                        return value_make_float(a / b);
                    case TOKEN_LT:      return value_make_bool(a < b);
                    case TOKEN_LE:      return value_make_bool(a <= b);
                    case TOKEN_GT:      return value_make_bool(a > b);
                    case TOKEN_GE:      return value_make_bool(a >= b);
                    default: break;
                }
            } else if (left.type == VAL_STRING && right.type == VAL_STRING) {
                int cmp = strcmp(left.as.string->chars, right.as.string->chars);
                value_release(left);
                value_release(right);
                switch (op) {
                    case TOKEN_LT: return value_make_bool(cmp < 0);
                    case TOKEN_LE: return value_make_bool(cmp <= 0);
                    case TOKEN_GT: return value_make_bool(cmp > 0);
                    case TOKEN_GE: return value_make_bool(cmp >= 0);
                    default: break;
                }
            }

            yalu_report_error(expr->pos, "Type Error", "Invalid operands for binary operator '%s': '%s' and '%s'",
                              token_type_name(op), value_type_name(left.type), value_type_name(right.type));
            interp->had_runtime_error = true;
            value_release(left);
            value_release(right);
            return value_make_null();
        }

        case AST_UNARY_OP: {
            Value r = interp_eval_expr(interp, expr->as.unary_op.right);
            TokenType op = expr->as.unary_op.op;

            if (op == TOKEN_MINUS) {
                if (r.type == VAL_INT) {
                    int64_t i = -r.as.integer;
                    value_release(r);
                    return value_make_int(i);
                }
                if (r.type == VAL_FLOAT) {
                    double d = -r.as.floating;
                    value_release(r);
                    return value_make_float(d);
                }
                yalu_report_error(expr->pos, "Type Error", "Unary '-' requires numeric operand, got '%s'", value_type_name(r.type));
                interp->had_runtime_error = true;
                value_release(r);
                return value_make_null();
            }

            if (op == TOKEN_BANG || op == TOKEN_NOT) {
                bool b = !value_is_truthy(r);
                value_release(r);
                return value_make_bool(b);
            }

            value_release(r);
            return value_make_null();
        }

        case AST_CALL:
            return eval_call(interp, expr);

        case AST_INPUT:
            return eval_input(interp, expr);

        case AST_RANGE: {
            Value v_start = interp_eval_expr(interp, expr->as.range.start);
            Value v_end = interp_eval_expr(interp, expr->as.range.end);

            if (v_start.type != VAL_INT || v_end.type != VAL_INT) {
                yalu_report_error(expr->pos, "Type Error", "Range bounds must be integers");
                interp->had_runtime_error = true;
                value_release(v_start);
                value_release(v_end);
                return value_make_null();
            }

            int64_t start = v_start.as.integer;
            int64_t end = v_end.as.integer;
            int64_t step = (start <= end) ? 1 : -1;

            if (expr->as.range.step) {
                Value v_step = interp_eval_expr(interp, expr->as.range.step);
                if (v_step.type == VAL_INT && v_step.as.integer != 0) {
                    step = v_step.as.integer;
                }
                value_release(v_step);
            }

            value_release(v_start);
            value_release(v_end);

            Value arr = value_make_array(16);
            int64_t curr = start;
            while ((step > 0 && curr <= end) || (step < 0 && curr >= end)) {
                array_append(arr.as.array, value_make_int(curr));
                curr += step;
            }

            return arr;
        }

        case AST_ARRAY_LIT: {
            size_t count = expr->as.array_lit.count;
            Value arr = value_make_array(count);
            for (size_t i = 0; i < count; i++) {
                Value elem = interp_eval_expr(interp, expr->as.array_lit.elements[i]);
                array_append(arr.as.array, elem);
                value_release(elem);
            }
            return arr;
        }

        case AST_INDEX: {
            Value target = interp_eval_expr(interp, expr->as.index.target);
            Value index = interp_eval_expr(interp, expr->as.index.index);

            if (target.type == VAL_ARRAY) {
                if (index.type != VAL_INT) {
                    yalu_report_error(expr->pos, "Type Error", "Array index must be integer, got '%s'", value_type_name(index.type));
                    interp->had_runtime_error = true;
                    value_release(target);
                    value_release(index);
                    return value_make_null();
                }
                int64_t idx = index.as.integer;
                value_release(index);
                int64_t norm_idx = idx;
                if (norm_idx < 0) norm_idx = (int64_t)target.as.array->length + norm_idx;
                if (norm_idx < 0 || (size_t)norm_idx >= target.as.array->length) {
                    yalu_report_error(expr->pos, "Index Error", "List index out of range: %lld (length: %zu)",
                                      (long long)idx, target.as.array->length);
                    interp->had_runtime_error = true;
                    value_release(target);
                    return value_make_null();
                }
                Value res = array_get(target.as.array, norm_idx);
                value_release(target);
                return res;
            }

            if (target.type == VAL_STRING) {
                if (index.type != VAL_INT) {
                    yalu_report_error(expr->pos, "Type Error", "String index must be integer, got '%s'", value_type_name(index.type));
                    interp->had_runtime_error = true;
                    value_release(target);
                    value_release(index);
                    return value_make_null();
                }
                int64_t idx = index.as.integer;
                value_release(index);
                StringObj* s = target.as.string;
                if (idx < 0) idx = (int64_t)s->length + idx;
                if (idx < 0 || (size_t)idx >= s->length) {
                    yalu_report_error(expr->pos, "Index Error", "String index out of range: %lld (length: %zu)",
                                      (long long)idx, s->length);
                    interp->had_runtime_error = true;
                    value_release(target);
                    return value_make_null();
                }
                char ch[2] = { s->chars[idx], '\0' };
                Value res = value_make_string(ch, 1);
                value_release(target);
                return res;
            }

            if (target.type == VAL_BOOK_VALS) {
                if (index.type != VAL_INT) {
                    yalu_report_error(expr->pos, "Type Error", "Book values index must be integer, got '%s'", value_type_name(index.type));
                    interp->had_runtime_error = true;
                    value_release(target);
                    value_release(index);
                    return value_make_null();
                }
                int64_t idx = index.as.integer;
                value_release(index);
                Value vals_list = book_get_vals_list(target.as.book_coll->book);
                int64_t norm_idx = idx;
                if (norm_idx < 0) norm_idx = (int64_t)vals_list.as.array->length + norm_idx;
                if (norm_idx < 0 || (size_t)norm_idx >= vals_list.as.array->length) {
                    yalu_report_error(expr->pos, "Index Error", "Book values index out of range: %lld (length: %zu)",
                                      (long long)idx, vals_list.as.array->length);
                    interp->had_runtime_error = true;
                    value_release(vals_list);
                    value_release(target);
                    return value_make_null();
                }
                Value res = array_get(vals_list.as.array, norm_idx);
                value_release(vals_list);
                value_release(target);
                return res;
            }

            if (target.type == VAL_BOOK_KEYS) {
                if (index.type != VAL_INT) {
                    yalu_report_error(expr->pos, "Type Error", "Book keys index must be integer, got '%s'", value_type_name(index.type));
                    interp->had_runtime_error = true;
                    value_release(target);
                    value_release(index);
                    return value_make_null();
                }
                int64_t idx = index.as.integer;
                value_release(index);
                Value keys_list = book_get_keys_list(target.as.book_coll->book);
                int64_t norm_idx = idx;
                if (norm_idx < 0) norm_idx = (int64_t)keys_list.as.array->length + norm_idx;
                if (norm_idx < 0 || (size_t)norm_idx >= keys_list.as.array->length) {
                    yalu_report_error(expr->pos, "Index Error", "Book keys index out of range: %lld (length: %zu)",
                                      (long long)idx, keys_list.as.array->length);
                    interp->had_runtime_error = true;
                    value_release(keys_list);
                    value_release(target);
                    return value_make_null();
                }
                Value res = array_get(keys_list.as.array, norm_idx);
                value_release(keys_list);
                value_release(target);
                return res;
            }

            if (target.type == VAL_BOOK) {
                char* key_str = (index.type == VAL_STRING) ? yalu_strdup(index.as.string->chars) : value_to_string(index);
                value_release(index);
                Value res;
                if (book_get(target.as.book, key_str, &res)) {
                    yalu_free(key_str);
                    value_release(target);
                    return res;
                }
                if (book_reverse_lookup(target.as.book, key_str, &res)) {
                    yalu_free(key_str);
                    value_release(target);
                    return res;
                }
                yalu_report_error(expr->pos, "Key Error", "Key '%s' not found in Book", key_str);
                yalu_free(key_str);
                interp->had_runtime_error = true;
                value_release(target);
                return value_make_null();
            }

            yalu_report_error(expr->pos, "Type Error", "Cannot index non-indexable value of type '%s'", value_type_name(target.type));
            interp->had_runtime_error = true;
            value_release(target);
            value_release(index);
            return value_make_null();
        }

        default:
            return value_make_null();
    }
}

void interp_exec_stmt(Interpreter* interp, ASTNode* stmt) {
    if (!stmt || interp->had_runtime_error || interp->is_returning || interp->is_breaking || interp->is_continuing) {
        return;
    }

    /* Safety step counter */
    interp->step_count++;
    if (interp->max_steps > 0 && interp->step_count > interp->max_steps) {
        yalu_report_error(stmt->pos, "Runtime Error", "Execution step limit reached (%llu steps)", (unsigned long long)interp->max_steps);
        interp->had_runtime_error = true;
        return;
    }

    switch (stmt->type) {
        case AST_PROGRAM:
        case AST_BLOCK: {
            bool is_nested_block = (stmt->type == AST_BLOCK);
            Environment* prev_env = interp->current_env;

            if (is_nested_block) {
                interp->current_env = env_create(prev_env);
            }

            for (size_t i = 0; i < stmt->as.block.count; i++) {
                interp_exec_stmt(interp, stmt->as.block.stmts[i]);
                if (interp->is_returning || interp->is_breaking || interp->is_continuing || interp->had_runtime_error) {
                    break;
                }
            }

            if (is_nested_block) {
                Environment* local = interp->current_env;
                interp->current_env = prev_env;
                env_release(local);
            }
            break;
        }

        case AST_USE:
            exec_use(interp, stmt);
            break;

        case AST_CLASS_DECL:
            exec_class_decl(interp, stmt);
            break;

        case AST_VAR_ASSIGN: {
            const char* name = stmt->as.var_assign.name;
            TokenType op = stmt->as.var_assign.op;

            if (op == TOKEN_ASSIGN) {
                Value val = interp_eval_expr(interp, stmt->as.var_assign.value);
                env_set(interp->current_env, name, val);
                value_release(val);
            } else {
                Value curr;
                if (!env_get(interp->current_env, name, &curr)) {
                    yalu_report_error(stmt->pos, "Runtime Error", "Undefined variable '%s' in compound assignment", name);
                    interp->had_runtime_error = true;
                    return;
                }
                Value rhs = interp_eval_expr(interp, stmt->as.var_assign.value);
                Value new_val = value_make_null();

                if (op == TOKEN_PLUS_ASSIGN && (curr.type == VAL_STRING || rhs.type == VAL_STRING)) {
                    char* s1 = value_to_string(curr);
                    char* s2 = value_to_string(rhs);
                    size_t l1 = strlen(s1);
                    size_t l2 = strlen(s2);
                    char* combined = (char*)yalu_malloc(l1 + l2 + 1);
                    memcpy(combined, s1, l1);
                    memcpy(combined + l1, s2, l2);
                    combined[l1 + l2] = '\0';
                    new_val = value_make_string_take(combined, l1 + l2);
                    yalu_free(s1);
                    yalu_free(s2);
                } else if (curr.type == VAL_INT && rhs.type == VAL_INT) {
                    int64_t a = curr.as.integer;
                    int64_t b = rhs.as.integer;
                    switch (op) {
                        case TOKEN_PLUS_ASSIGN:  new_val = value_make_int(a + b); break;
                        case TOKEN_MINUS_ASSIGN: new_val = value_make_int(a - b); break;
                        case TOKEN_STAR_ASSIGN:  new_val = value_make_int(a * b); break;
                        case TOKEN_SLASH_ASSIGN:
                            if (b == 0) {
                                yalu_report_error(stmt->pos, "Runtime Error", "Division by zero");
                                interp->had_runtime_error = true;
                            } else {
                                new_val = value_make_int(a / b);
                            }
                            break;
                        default: break;
                    }
                } else if ((curr.type == VAL_INT || curr.type == VAL_FLOAT) &&
                           (rhs.type == VAL_INT || rhs.type == VAL_FLOAT)) {
                    double a = (curr.type == VAL_FLOAT) ? curr.as.floating : (double)curr.as.integer;
                    double b = (rhs.type == VAL_FLOAT) ? rhs.as.floating : (double)rhs.as.integer;
                    switch (op) {
                        case TOKEN_PLUS_ASSIGN:  new_val = value_make_float(a + b); break;
                        case TOKEN_MINUS_ASSIGN: new_val = value_make_float(a - b); break;
                        case TOKEN_STAR_ASSIGN:  new_val = value_make_float(a * b); break;
                        case TOKEN_SLASH_ASSIGN:
                            if (b == 0.0) {
                                yalu_report_error(stmt->pos, "Runtime Error", "Division by zero");
                                interp->had_runtime_error = true;
                            } else {
                                new_val = value_make_float(a / b);
                            }
                            break;
                        default: break;
                    }
                } else {
                    yalu_report_error(stmt->pos, "Type Error", "Invalid types for compound assignment '%s'", token_type_name(op));
                    interp->had_runtime_error = true;
                }

                value_release(curr);
                value_release(rhs);

                if (!interp->had_runtime_error) {
                    env_set(interp->current_env, name, new_val);
                }
                value_release(new_val);
            }
            break;
        }

        case AST_PROP_ASSIGN: {
            Value target = interp_eval_expr(interp, stmt->as.prop_assign.target);
            const char* prop_name = stmt->as.prop_assign.prop_name;
            TokenType op = stmt->as.prop_assign.op;

            if (target.type == VAL_BOOK) {
                BookObj* book = target.as.book;
                Value rhs = interp_eval_expr(interp, stmt->as.prop_assign.value);
                book_set(book, prop_name, rhs);
                value_release(rhs);
                value_release(target);
                break;
            }

            if (target.type != VAL_INSTANCE) {
                yalu_report_error(stmt->pos, "Type Error", "Cannot assign property '%s' on non-instance", prop_name);
                interp->had_runtime_error = true;
                value_release(target);
                return;
            }

            InstanceObj* inst = target.as.instance;
            if (op == TOKEN_ASSIGN) {
                Value val = interp_eval_expr(interp, stmt->as.prop_assign.value);
                instance_set_property(inst, prop_name, val);
                value_release(val);
            } else {
                Value curr;
                if (!instance_get_property(inst, prop_name, &curr)) {
                    curr = value_make_null();
                }
                Value rhs = interp_eval_expr(interp, stmt->as.prop_assign.value);
                Value new_val = value_make_null();

                if (op == TOKEN_PLUS_ASSIGN && (curr.type == VAL_INT && rhs.type == VAL_INT)) {
                    new_val = value_make_int(curr.as.integer + rhs.as.integer);
                } else if (op == TOKEN_MINUS_ASSIGN && (curr.type == VAL_INT && rhs.type == VAL_INT)) {
                    new_val = value_make_int(curr.as.integer - rhs.as.integer);
                } else if (op == TOKEN_STAR_ASSIGN && (curr.type == VAL_INT && rhs.type == VAL_INT)) {
                    new_val = value_make_int(curr.as.integer * rhs.as.integer);
                } else {
                    new_val = value_copy(rhs);
                }

                instance_set_property(inst, prop_name, new_val);
                value_release(curr);
                value_release(rhs);
                value_release(new_val);
            }

            value_release(target);
            break;
        }

        case AST_CLASS_PROP_ASSIGN: {
            const char* prop_name = stmt->as.class_prop_assign.prop_name;
            InstanceObj* inst = interp->current_instance;

            if (!inst) {
                Value yclass_val;
                if (env_get(interp->current_env, "yclass", &yclass_val)) {
                    if (yclass_val.type == VAL_INSTANCE) {
                        inst = yclass_val.as.instance;
                    }
                    value_release(yclass_val);
                }
            }

            if (!inst) {
                yalu_report_error(stmt->pos, "Runtime Error", "Cannot assign '%s.%s' outside of class constructor",
                                  prop_name, stmt->as.class_prop_assign.class_name);
                interp->had_runtime_error = true;
                return;
            }

            Value val = interp_eval_expr(interp, stmt->as.class_prop_assign.value);
            instance_set_property(inst, prop_name, val);
            value_release(val);
            break;
        }

        case AST_INDEX_ASSIGN: {
            Value target = interp_eval_expr(interp, stmt->as.index_assign.target);
            Value index = interp_eval_expr(interp, stmt->as.index_assign.index);

            if (target.type == VAL_BOOK) {
                char* key_str = (index.type == VAL_STRING) ? yalu_strdup(index.as.string->chars) : value_to_string(index);
                Value val = interp_eval_expr(interp, stmt->as.index_assign.value);
                book_set(target.as.book, key_str, val);
                yalu_free(key_str);
                value_release(target);
                value_release(index);
                value_release(val);
                break;
            }

            if (target.type != VAL_ARRAY || index.type != VAL_INT) {
                yalu_report_error(stmt->pos, "Type Error", "Indexed assignment requires array target and integer index");
                interp->had_runtime_error = true;
                value_release(target);
                value_release(index);
                return;
            }

            int64_t idx = index.as.integer;
            int64_t norm_idx = idx;
            if (norm_idx < 0) norm_idx = (int64_t)target.as.array->length + norm_idx;
            if (norm_idx < 0 || (size_t)norm_idx >= target.as.array->length) {
                yalu_report_error(stmt->pos, "Index Error", "List index out of range: %lld (length: %zu)",
                                  (long long)idx, target.as.array->length);
                interp->had_runtime_error = true;
                value_release(target);
                value_release(index);
                return;
            }

            Value val = interp_eval_expr(interp, stmt->as.index_assign.value);
            array_set(target.as.array, norm_idx, val);

            value_release(target);
            value_release(index);
            value_release(val);
            break;
        }

        case AST_PRINT: {
            for (size_t i = 0; i < stmt->as.print.count; i++) {
                Value v = interp_eval_expr(interp, stmt->as.print.args[i]);
                if (v.type == VAL_INSTANCE) {
                    char* s = interp_instance_to_string(interp, v.as.instance);
                    printf("%s", s);
                    yalu_free(s);
                } else {
                    value_print(v, false);
                }
                value_release(v);
                if (i + 1 < stmt->as.print.count) {
                    printf(" ");
                }
            }
            if (interp->in_sep_loop && interp->sep_string) {
                if (strcmp(interp->sep_string, ".NL") == 0) {
                    printf("\n");
                } else if (strcmp(interp->sep_string, ",.NL") == 0) {
                    printf(",\n");
                } else if (strcmp(interp->sep_string, ",") == 0) {
                    if (interp->loop_current_idx + 1 < interp->loop_total_count) {
                        printf(",");
                    }
                } else {
                    printf("%s", interp->sep_string);
                }
            } else {
                printf("\n");
            }
            fflush(stdout);
            break;
        }

        case AST_FN_DECL: {
            Value fn_val = value_make_function(stmt->as.fn_decl.name, stmt, interp->current_env);
            env_set(interp->current_env, stmt->as.fn_decl.name, fn_val);
            value_release(fn_val);
            break;
        }

        case AST_WHILE: {
            while (true) {
                Value cond = interp_eval_expr(interp, stmt->as.while_loop.condition);
                bool truthy = value_is_truthy(cond);
                value_release(cond);

                if (!truthy || interp->had_runtime_error) {
                    break;
                }

                interp_exec_stmt(interp, stmt->as.while_loop.body);

                if (interp->is_breaking) {
                    interp->is_breaking = false;
                    break;
                }
                if (interp->is_continuing) {
                    interp->is_continuing = false;
                }
                if (interp->is_returning || interp->had_runtime_error) {
                    break;
                }
            }
            break;
        }

        case AST_FOR: {
            const char* var_name = stmt->as.for_loop.var_name;
            ASTNode* iterable_node = stmt->as.for_loop.iterable;
            ASTNode* sep_node = stmt->as.for_loop.separator;

            /* Check if this is a range loop */
            bool is_range = false;
            ASTNode* range_node = NULL;
            if (iterable_node && iterable_node->type == AST_RANGE) {
                is_range = true;
                range_node = iterable_node;
            } else if (stmt->as.for_loop.start && stmt->as.for_loop.end) {
                is_range = true;
            }

            /* Save previous separator loop state */
            bool prev_in_sep_loop = interp->in_sep_loop;
            char* prev_sep_string = interp->sep_string;
            size_t prev_idx = interp->loop_current_idx;
            size_t prev_total = interp->loop_total_count;

            char* cur_sep = NULL;
            if (sep_node) {
                Value sep_val = interp_eval_expr(interp, sep_node);
                if (sep_val.type == VAL_STRING) {
                    cur_sep = yalu_strdup(sep_val.as.string->chars);
                } else {
                    cur_sep = value_to_string(sep_val);
                }
                value_release(sep_val);
                interp->in_sep_loop = true;
                interp->sep_string = cur_sep;
            }

            if (is_range) {
                ASTNode* start_node = range_node ? range_node->as.range.start : stmt->as.for_loop.start;
                ASTNode* end_node = range_node ? range_node->as.range.end : stmt->as.for_loop.end;
                ASTNode* step_node = range_node ? range_node->as.range.step : stmt->as.for_loop.step;

                Value v_start = interp_eval_expr(interp, start_node);
                Value v_end = interp_eval_expr(interp, end_node);

                if (v_start.type == VAL_INT && v_end.type == VAL_INT) {
                    int64_t start = v_start.as.integer;
                    int64_t end = v_end.as.integer;
                    int64_t step = (start <= end) ? 1 : -1;

                    if (step_node) {
                        Value v_step = interp_eval_expr(interp, step_node);
                        if (v_step.type == VAL_INT && v_step.as.integer != 0) {
                            step = v_step.as.integer;
                        }
                        value_release(v_step);
                    }

                    value_release(v_start);
                    value_release(v_end);

                    size_t total_count = 0;
                    if ((step > 0 && start <= end) || (step < 0 && start >= end)) {
                        int64_t diff = (step > 0) ? (end - start) : (start - end);
                        int64_t abs_step = (step > 0) ? step : -step;
                        total_count = (size_t)(diff / abs_step) + 1;
                    }
                    interp->loop_total_count = total_count;

                    Environment* loop_env = env_create(interp->current_env);
                    Environment* prev_env = interp->current_env;
                    interp->current_env = loop_env;

                    int64_t curr = start;
                    size_t k = 0;
                    while ((step > 0 && curr <= end) || (step < 0 && curr >= end)) {
                        interp->loop_current_idx = k;
                        env_define(loop_env, var_name, value_make_int(curr));

                        interp_exec_stmt(interp, stmt->as.for_loop.body);

                        if (interp->is_breaking) {
                            interp->is_breaking = false;
                            break;
                        }
                        if (interp->is_continuing) {
                            interp->is_continuing = false;
                        }
                        if (interp->is_returning || interp->had_runtime_error) {
                            break;
                        }

                        curr += step;
                        k++;
                    }

                    interp->current_env = prev_env;
                    env_release(loop_env);
                } else {
                    yalu_report_error(stmt->pos, "Type Error", "For-loop range bounds must be integers");
                    interp->had_runtime_error = true;
                    value_release(v_start);
                    value_release(v_end);
                }
            } else if (iterable_node) {
                Value iter_val = interp_eval_expr(interp, iterable_node);

                Value* items = NULL;
                size_t total_count = 0;
                bool cleanup_items = false;

                if (iter_val.type == VAL_ARRAY) {
                    items = iter_val.as.array->items;
                    total_count = iter_val.as.array->length;
                } else if (iter_val.type == VAL_STRING) {
                    StringObj* s = iter_val.as.string;
                    total_count = s->length;
                    items = (Value*)yalu_malloc(sizeof(Value) * (total_count > 0 ? total_count : 1));
                    for (size_t i = 0; i < total_count; i++) {
                        char ch[2] = { s->chars[i], '\0' };
                        items[i] = value_make_string(ch, 1);
                    }
                    cleanup_items = true;
                } else if (iter_val.type == VAL_BOOK_VALS) {
                    Value vals_list = book_get_vals_list(iter_val.as.book_coll->book);
                    total_count = vals_list.as.array->length;
                    items = (Value*)yalu_malloc(sizeof(Value) * (total_count > 0 ? total_count : 1));
                    for (size_t i = 0; i < total_count; i++) {
                        items[i] = value_retain(vals_list.as.array->items[i]);
                    }
                    value_release(vals_list);
                    cleanup_items = true;
                } else if (iter_val.type == VAL_BOOK_KEYS) {
                    Value keys_list = book_get_keys_list(iter_val.as.book_coll->book);
                    total_count = keys_list.as.array->length;
                    items = (Value*)yalu_malloc(sizeof(Value) * (total_count > 0 ? total_count : 1));
                    for (size_t i = 0; i < total_count; i++) {
                        items[i] = value_retain(keys_list.as.array->items[i]);
                    }
                    value_release(keys_list);
                    cleanup_items = true;
                } else if (iter_val.type == VAL_BOOK) {
                    Value keys_list = book_get_keys_list(iter_val.as.book);
                    total_count = keys_list.as.array->length;
                    items = (Value*)yalu_malloc(sizeof(Value) * (total_count > 0 ? total_count : 1));
                    for (size_t i = 0; i < total_count; i++) {
                        items[i] = value_retain(keys_list.as.array->items[i]);
                    }
                    value_release(keys_list);
                    cleanup_items = true;
                } else {
                    yalu_report_error(stmt->pos, "Type Error", "Cannot iterate over value of type '%s'",
                                      value_type_name(iter_val.type));
                    interp->had_runtime_error = true;
                }

                if (!interp->had_runtime_error) {
                    interp->loop_total_count = total_count;

                    Environment* loop_env = env_create(interp->current_env);
                    Environment* prev_env = interp->current_env;
                    interp->current_env = loop_env;

                    for (size_t i = 0; i < total_count; i++) {
                        interp->loop_current_idx = i;
                        env_define(loop_env, var_name, value_copy(items[i]));

                        interp_exec_stmt(interp, stmt->as.for_loop.body);

                        if (interp->is_breaking) {
                            interp->is_breaking = false;
                            break;
                        }
                        if (interp->is_continuing) {
                            interp->is_continuing = false;
                        }
                        if (interp->is_returning || interp->had_runtime_error) {
                            break;
                        }
                    }

                    interp->current_env = prev_env;
                    env_release(loop_env);
                }

                if (cleanup_items && items) {
                    for (size_t i = 0; i < total_count; i++) {
                        value_release(items[i]);
                    }
                    yalu_free(items);
                }
                value_release(iter_val);
            }

            if (cur_sep) {
                if (strcmp(cur_sep, ",") == 0) {
                    printf("\n");
                    fflush(stdout);
                }
                yalu_free(cur_sep);
            }

            interp->in_sep_loop = prev_in_sep_loop;
            interp->sep_string = prev_sep_string;
            interp->loop_current_idx = prev_idx;
            interp->loop_total_count = prev_total;
            break;
        }

        case AST_IF: {
            bool executed = false;
            for (size_t i = 0; i < stmt->as.if_stmt.branch_count; i++) {
                Value cond = interp_eval_expr(interp, stmt->as.if_stmt.conditions[i]);
                bool truthy = value_is_truthy(cond);
                value_release(cond);

                if (truthy) {
                    interp_exec_stmt(interp, stmt->as.if_stmt.bodies[i]);
                    executed = true;
                    break;
                }
            }

            if (!executed && stmt->as.if_stmt.else_body) {
                interp_exec_stmt(interp, stmt->as.if_stmt.else_body);
            }
            break;
        }

        case AST_RETURN: {
            if (interp->call_depth <= 0) {
                yalu_report_error(stmt->pos, "Runtime Error", "'place' used outside of function or method");
                interp->had_runtime_error = true;
                return;
            }
            Value val = value_make_null();
            if (stmt->as.return_stmt.value) {
                val = interp_eval_expr(interp, stmt->as.return_stmt.value);
            }
            interp->is_returning = true;
            interp->return_val = val;
            break;
        }

        case AST_BREAK:
            interp->is_breaking = true;
            break;

        case AST_CONTINUE:
            interp->is_continuing = true;
            break;

        case AST_EXPR_STMT: {
            Value v = interp_eval_expr(interp, stmt->as.expr_stmt.expr);
            value_release(v);
            break;
        }

        default:
            break;
    }
}

bool interp_interpret(Interpreter* interp, ASTNode* program) {
    if (!program) return false;
    interp->had_runtime_error = false;
    interp_exec_stmt(interp, program);
    return !interp->had_runtime_error;
}
