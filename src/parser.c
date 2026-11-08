#include "parser.h"
#include "errors.h"

static void advance_token(Parser* parser);
static bool check_token(Parser* parser, TokenType type);
static bool match_token(Parser* parser, TokenType type);
static Token consume_token(Parser* parser, TokenType type, const char* message);
static void error_at(Parser* parser, Token* token, const char* message);
static void error_at_current(Parser* parser, const char* message);
static void synchronize(Parser* parser);

static ASTNode* parse_statement(Parser* parser);
static ASTNode* parse_block_body(Parser* parser);
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_assignment_or_expr(Parser* parser);
static ASTNode* parse_logical_or(Parser* parser);
static ASTNode* parse_logical_and(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_comparison(Parser* parser);
static ASTNode* parse_range(Parser* parser);
static ASTNode* parse_term(Parser* parser);
static ASTNode* parse_factor(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* parse_call_or_index(Parser* parser);
static ASTNode* parse_primary(Parser* parser);
static ASTNode* parse_book_literal(Parser* parser);
static ASTNode* parse_method_arg(Parser* parser);

void parser_init(Parser* parser, Lexer* lexer, const char* filename) {
    parser->lexer = lexer;
    parser->filename = filename;
    parser->had_error = false;
    parser->panic_mode = false;
    memset(&parser->current, 0, sizeof(Token));
    memset(&parser->previous, 0, sizeof(Token));
    advance_token(parser);
}

static void advance_token(Parser* parser) {
    token_free(&parser->previous);
    parser->previous = parser->current;

    while (true) {
        parser->current = lexer_next_token(parser->lexer);
        if (parser->current.type != TOKEN_ERROR) break;

        error_at_current(parser, parser->current.as.str_val ? parser->current.as.str_val : "Lexer error");
    }
}

static bool check_token(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static bool match_token(Parser* parser, TokenType type) {
    if (!check_token(parser, type)) return false;
    advance_token(parser);
    return true;
}

static Token consume_token(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance_token(parser);
        return parser->previous;
    }
    error_at_current(parser, message);
    return parser->current;
}

static void error_at(Parser* parser, Token* token, const char* message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;
    yalu_report_error(token->pos, "Syntax Error", "%s", message);
}

static void error_at_current(Parser* parser, const char* message) {
    error_at(parser, &parser->current, message);
}

static void synchronize(Parser* parser) {
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;
        if (parser->previous.type == TOKEN_COLON_COLON) return;

        switch (parser->current.type) {
            case TOKEN_FN:
            case TOKEN_LP:
            case TOKEN_FR:
            case TOKEN_IF:
            case TOKEN_PR:
            case TOKEN_RT:
            case TOKEN_PLACE:
            case TOKEN_USE:
            case TOKEN_CLASS:
            case TOKEN_BK:
            case TOKEN_CN:
            case TOKEN_COLON_COLON:
                return;
            default:
                break;
        }
        advance_token(parser);
    }
}

static ASTNode* parse_string_with_interpolation(Parser* parser, Token token) {
    (void)parser;
    const char* str = token.as.str_val ? token.as.str_val : "";
    const char* interp_pos = strstr(str, "/#");

    if (!interp_pos) {
        Value val = value_make_string(str, strlen(str));
        return ast_create_literal(token.pos, val);
    }

    size_t cap = 4;
    size_t count = 0;
    ASTNode** parts = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * cap);

    const char* p = str;
    while (*p) {
        const char* next_interp = strstr(p, "/#");
        if (!next_interp) {
            size_t lit_len = strlen(p);
            if (lit_len > 0) {
                if (count >= cap) {
                    cap *= 2;
                    parts = (ASTNode**)yalu_realloc(parts, sizeof(ASTNode*) * cap);
                }
                parts[count++] = ast_create_literal(token.pos, value_make_string(p, lit_len));
            }
            break;
        }

        if (next_interp > p) {
            size_t lit_len = (size_t)(next_interp - p);
            if (count >= cap) {
                cap *= 2;
                parts = (ASTNode**)yalu_realloc(parts, sizeof(ASTNode*) * cap);
            }
            parts[count++] = ast_create_literal(token.pos, value_make_string(p, lit_len));
        }

        p = next_interp + 2; /* Skip /# */
        const char* var_start = p;
        while (isalnum((unsigned char)*p) || *p == '_') {
            p++;
        }

        if (p == var_start) {
            if (count >= cap) {
                cap *= 2;
                parts = (ASTNode**)yalu_realloc(parts, sizeof(ASTNode*) * cap);
            }
            parts[count++] = ast_create_literal(token.pos, value_make_string("/#", 2));
        } else {
            size_t var_len = (size_t)(p - var_start);
            char* var_name = yalu_strndup(var_start, var_len);
            ASTNode* expr = ast_create_var_ref(token.pos, var_name);
            yalu_free(var_name);

            while (*p == '>' && (isalnum((unsigned char)*(p + 1)) || *(p + 1) == '_')) {
                p++; /* skip > */
                const char* prop_start = p;
                while (isalnum((unsigned char)*p) || *p == '_') p++;
                char* prop_name = yalu_strndup(prop_start, (size_t)(p - prop_start));
                expr = ast_create_prop_access(token.pos, expr, prop_name);
                yalu_free(prop_name);
            }

            if (count >= cap) {
                cap *= 2;
                parts = (ASTNode**)yalu_realloc(parts, sizeof(ASTNode*) * cap);
            }
            parts[count++] = expr;
        }
    }

    return ast_create_string_interp(token.pos, parts, count);
}

static ASTNode* parse_print_statement(Parser* parser) {
    SourcePos pos = parser->previous.pos;

    size_t cap = 2;
    size_t count = 0;
    ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * cap);

    args[count++] = parse_expression(parser);

    while (match_token(parser, TOKEN_COMMA)) {
        if (count >= cap) {
            cap *= 2;
            args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * cap);
        }
        args[count++] = parse_expression(parser);
    }

    return ast_create_print(pos, args, count);
}

static ASTNode* parse_use_statement(Parser* parser) {
    SourcePos pos = parser->previous.pos;
    consume_token(parser, TOKEN_LBRACKET, "Expected '[' after 'use'");

    size_t cap = 64;
    size_t len = 0;
    char* path_buf = (char*)yalu_malloc(cap);
    path_buf[0] = '\0';

    while (!check_token(parser, TOKEN_RBRACKET) && !check_token(parser, TOKEN_EOF)) {
        const char* s = parser->current.start;
        size_t l = parser->current.length;
        if (parser->current.type == TOKEN_STRING && parser->current.as.str_val) {
            s = parser->current.as.str_val;
            l = strlen(s);
        }
        while (len + l + 1 >= cap) {
            cap *= 2;
            path_buf = (char*)yalu_realloc(path_buf, cap);
        }
        memcpy(path_buf + len, s, l);
        len += l;
        path_buf[len] = '\0';
        advance_token(parser);
    }

    consume_token(parser, TOKEN_RBRACKET, "Expected ']' after module path in 'use [...]'");
    ASTNode* node = ast_create_use(pos, path_buf);
    yalu_free(path_buf);
    return node;
}

static ASTNode* parse_fn_declaration(Parser* parser) {
    SourcePos pos = parser->previous.pos;
    Token name_tok = consume_token(parser, TOKEN_IDENT, "Expected function name after 'fn'");
    char* fn_name = yalu_strdup(name_tok.as.str_val ? name_tok.as.str_val : "");

    /* Parameter opening: !!< or !!<< */
    consume_token(parser, TOKEN_BANG_BANG_LT, "Expected '!!<' to open parameter list in function declaration");

    size_t param_cap = 4;
    size_t param_count = 0;
    char** param_names = (char**)yalu_malloc(sizeof(char*) * param_cap);
    ASTNode** param_defaults = (ASTNode**)yalu_calloc(param_cap, sizeof(ASTNode*));
    char** param_types = (char**)yalu_calloc(param_cap, sizeof(char*));
    bool* is_yclass = (bool*)yalu_calloc(param_cap, sizeof(bool));

    if (!check_token(parser, TOKEN_GT_BANG_BANG) && !check_token(parser, TOKEN_GT)) {
        do {
            Token p_tok = consume_token(parser, TOKEN_IDENT, "Expected parameter name");
            char* p_name = yalu_strdup(p_tok.as.str_val ? p_tok.as.str_val : "");
            ASTNode* p_default = NULL;
            char* p_type = NULL;
            bool p_yclass = (strcmp(p_name, "yclass") == 0);

            if (match_token(parser, TOKEN_COLON)) {
                /* Check if next is :(type) */
                if (match_token(parser, TOKEN_LPAREN)) {
                    Token t_tok = consume_token(parser, TOKEN_IDENT, "Expected type name in ':(type)'");
                    p_type = yalu_strdup(t_tok.as.str_val ? t_tok.as.str_val : "");
                    consume_token(parser, TOKEN_RPAREN, "Expected ')' after type name");
                } else if (check_token(parser, TOKEN_IDENT) &&
                           (strcmp(parser->current.as.str_val, "int") == 0 ||
                            strcmp(parser->current.as.str_val, "str") == 0 ||
                            strcmp(parser->current.as.str_val, "flt") == 0 ||
                            strcmp(parser->current.as.str_val, "float") == 0 ||
                            strcmp(parser->current.as.str_val, "bool") == 0 ||
                            strcmp(parser->current.as.str_val, "any") == 0)) {
                    /* Peek next char: if comma or close of params, it's type annotation only */
                    const char* cur = parser->current.start + parser->current.length;
                    while (*cur == ' ' || *cur == '\t') cur++;
                    if (*cur == ',' || *cur == '>' || *cur == '!') {
                        p_type = yalu_strdup(parser->current.as.str_val);
                        advance_token(parser);
                    } else {
                        p_default = parse_expression(parser);
                        if (match_token(parser, TOKEN_COLON)) {
                            consume_token(parser, TOKEN_LPAREN, "Expected '(' in ':(type)'");
                            Token t_tok = consume_token(parser, TOKEN_IDENT, "Expected type name in ':(type)'");
                            p_type = yalu_strdup(t_tok.as.str_val ? t_tok.as.str_val : "");
                            consume_token(parser, TOKEN_RPAREN, "Expected ')' after type name");
                        }
                    }
                } else {
                    /* Default value expression */
                    p_default = parse_expression(parser);
                    /* Optional :(type) after default: e.g. age:18:(int) */
                    if (match_token(parser, TOKEN_COLON)) {
                        consume_token(parser, TOKEN_LPAREN, "Expected '(' in ':(type)'");
                        Token t_tok = consume_token(parser, TOKEN_IDENT, "Expected type name in ':(type)'");
                        p_type = yalu_strdup(t_tok.as.str_val ? t_tok.as.str_val : "");
                        consume_token(parser, TOKEN_RPAREN, "Expected ')' after type name");
                    }
                }
            }

            if (param_count >= param_cap) {
                size_t new_cap = param_cap * 2;
                param_names = (char**)yalu_realloc(param_names, sizeof(char*) * new_cap);
                param_defaults = (ASTNode**)yalu_realloc(param_defaults, sizeof(ASTNode*) * new_cap);
                param_types = (char**)yalu_realloc(param_types, sizeof(char*) * new_cap);
                is_yclass = (bool*)yalu_realloc(is_yclass, sizeof(bool) * new_cap);
                for (size_t i = param_cap; i < new_cap; i++) {
                    param_defaults[i] = NULL;
                    param_types[i] = NULL;
                    is_yclass[i] = false;
                }
                param_cap = new_cap;
            }

            param_names[param_count] = p_name;
            param_defaults[param_count] = p_default;
            param_types[param_count] = p_type;
            is_yclass[param_count] = p_yclass;
            param_count++;

        } while (match_token(parser, TOKEN_COMMA));
    }

    if (!match_token(parser, TOKEN_GT_BANG_BANG)) {
        if (match_token(parser, TOKEN_GT)) {
            match_token(parser, TOKEN_GT);
            match_token(parser, TOKEN_BANG_BANG);
        } else {
            consume_token(parser, TOKEN_GT_BANG_BANG, "Expected '>!!', '>>!!', or '>>' to close parameter list");
        }
    }

    /* Body block starting with :: */
    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to open function body block");
    ASTNode* body = parse_block_body(parser);
    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to close function body block");

    ASTNode* node = ast_create_fn_decl_v2(pos, fn_name, param_names, param_defaults, param_types, is_yclass, param_count, body);
    yalu_free(fn_name);
    return node;
}

static ASTNode* parse_class_declaration(Parser* parser) {
    SourcePos pos = parser->previous.pos;
    consume_token(parser, TOKEN_SLASH, "Expected '/' before class name (e.g. class /Point <...>)");
    Token name_tok = consume_token(parser, TOKEN_IDENT, "Expected class name identifier after '/'");
    char* class_name = yalu_strdup(name_tok.as.str_val ? name_tok.as.str_val : "");

    consume_token(parser, TOKEN_LT, "Expected '<' to open class body");

    size_t method_cap = 4;
    size_t method_count = 0;
    ASTNode** methods = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * method_cap);

    while (!check_token(parser, TOKEN_GT) && !check_token(parser, TOKEN_EOF)) {
        if (match_token(parser, TOKEN_FN)) {
            ASTNode* method = parse_fn_declaration(parser);
            if (method_count >= method_cap) {
                method_cap *= 2;
                methods = (ASTNode**)yalu_realloc(methods, sizeof(ASTNode*) * method_cap);
            }
            methods[method_count++] = method;
        } else if (match_token(parser, TOKEN_SEMICOLON)) {
            continue;
        } else {
            error_at_current(parser, "Expected method declaration 'fn ...' inside class body");
            advance_token(parser);
        }
    }

    consume_token(parser, TOKEN_GT, "Expected '>' to close class body");

    ASTNode* node = ast_create_class_decl(pos, class_name, methods, method_count);
    yalu_free(class_name);
    return node;
}

static ASTNode* parse_while_loop(Parser* parser) {
    SourcePos pos = parser->previous.pos;

    consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' before while loop condition");
    ASTNode* condition = parse_expression(parser);
    consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' after while loop condition");

    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to open while loop block");
    ASTNode* body = parse_block_body(parser);
    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to close while loop block");

    return ast_create_while(pos, condition, body);
}

static ASTNode* parse_for_loop(Parser* parser) {
    SourcePos pos = parser->previous.pos;

    consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' before for loop header");
    Token var_tok = consume_token(parser, TOKEN_IDENT, "Expected loop variable name in for loop");
    char* var_name = yalu_strdup(var_tok.as.str_val ? var_tok.as.str_val : "");

    consume_token(parser, TOKEN_IN, "Expected 'in' keyword in for loop");

    ASTNode* iterable = parse_expression(parser);
    ASTNode* separator = NULL;

    if (match_token(parser, TOKEN_COMMA)) {
        consume_token(parser, TOKEN_IDENT, "Expected 'j' for separator parameter");
        consume_token(parser, TOKEN_ASSIGN, "Expected '=' after 'j'");
        separator = parse_expression(parser);
    }

    consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' after for loop header");

    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to open for loop block");
    ASTNode* body = parse_block_body(parser);
    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to close for loop block");

    ASTNode* node = ast_create_for_v2(pos, var_name, iterable, separator, body);
    yalu_free(var_name);
    return node;
}

static ASTNode* parse_if_branch_body(Parser* parser) {
    SourcePos pos = parser->current.pos;
    ASTNode* block = ast_create_block(pos);

    while (!check_token(parser, TOKEN_COLON_COLON) &&
           !check_token(parser, TOKEN_COLON_EI) &&
           !check_token(parser, TOKEN_ELSE_MARKER) &&
           !check_token(parser, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_block_add(block, stmt);
        }
    }

    return block;
}

static ASTNode* parse_if_statement(Parser* parser) {
    SourcePos pos = parser->previous.pos;

    size_t branch_cap = 2;
    size_t branch_count = 0;
    ASTNode** conditions = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * branch_cap);
    ASTNode** bodies = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * branch_cap);
    ASTNode* else_body = NULL;

    /* First branch: if !!cond!! :: body */
    consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' before if condition");
    ASTNode* cond = parse_expression(parser);
    consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' after if condition");

    consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to open if block");
    ASTNode* body = parse_if_branch_body(parser);

    conditions[branch_count] = cond;
    bodies[branch_count] = body;
    branch_count++;

    /* If stopped at ::, check if :ei or <<""else"">> follows */
    if (check_token(parser, TOKEN_COLON_COLON)) {
        /* Peek ahead past :: */
        Lexer look_lex = *parser->lexer;
        Token next_tok = lexer_next_token(&look_lex);
        if (next_tok.type == TOKEN_COLON_EI || next_tok.type == TOKEN_ELSE_MARKER ||
            next_tok.type == TOKEN_ELIF || next_tok.type == TOKEN_EL) {
            consume_token(parser, TOKEN_COLON_COLON, "Expected '::'");
        }
        token_free(&next_tok);
    }

    /* Else-if branches: :ei !!cond!! :: or elif !!cond!! :: */
    while (check_token(parser, TOKEN_COLON_EI) || check_token(parser, TOKEN_ELIF)) {
        if (branch_count >= branch_cap) {
            branch_cap *= 2;
            conditions = (ASTNode**)yalu_realloc(conditions, sizeof(ASTNode*) * branch_cap);
            bodies = (ASTNode**)yalu_realloc(bodies, sizeof(ASTNode*) * branch_cap);
        }

        advance_token(parser); /* Consume :ei or elif */

        consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' before else-if condition");
        cond = parse_expression(parser);
        consume_token(parser, TOKEN_BANG_BANG, "Expected '!!' after else-if condition");

        consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to open else-if block");
        body = parse_if_branch_body(parser);

        conditions[branch_count] = cond;
        bodies[branch_count] = body;
        branch_count++;

        if (check_token(parser, TOKEN_COLON_COLON)) {
            Lexer look_lex = *parser->lexer;
            Token next_tok = lexer_next_token(&look_lex);
            if (next_tok.type == TOKEN_COLON_EI || next_tok.type == TOKEN_ELSE_MARKER ||
                next_tok.type == TOKEN_ELIF || next_tok.type == TOKEN_EL) {
                consume_token(parser, TOKEN_COLON_COLON, "Expected '::'");
            }
            token_free(&next_tok);
        }
    }

    /* Else branch: <<""else"">> or el :: body :: */
    if (check_token(parser, TOKEN_ELSE_MARKER) || check_token(parser, TOKEN_EL)) {
        advance_token(parser); /* Consume <<""else"">> or el */
        if (check_token(parser, TOKEN_COLON_COLON)) {
            advance_token(parser); /* Optional :: */
        }
        else_body = parse_block_body(parser);
        consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to close conditional statement");
    } else if (check_token(parser, TOKEN_COLON_COLON)) {
        consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to close conditional statement");
    }

    return ast_create_if(pos, conditions, bodies, branch_count, else_body);
}

static ASTNode* parse_return_statement(Parser* parser) {
    SourcePos pos = parser->previous.pos;
    ASTNode* value = NULL;

    if (!check_token(parser, TOKEN_SEMICOLON) &&
        !check_token(parser, TOKEN_COLON_COLON) &&
        !check_token(parser, TOKEN_EOF)) {
        value = parse_expression(parser);
    }

    return ast_create_return(pos, value);
}

static ASTNode* parse_block_body(Parser* parser) {
    SourcePos pos = parser->current.pos;
    ASTNode* block = ast_create_block(pos);

    while (!check_token(parser, TOKEN_COLON_COLON) && !check_token(parser, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_block_add(block, stmt);
        }
    }

    return block;
}

static ASTNode* parse_statement(Parser* parser) {
    if (match_token(parser, TOKEN_SEMICOLON)) {
        return NULL;
    }

    if (match_token(parser, TOKEN_USE)) {
        return parse_use_statement(parser);
    }

    if (match_token(parser, TOKEN_CLASS)) {
        return parse_class_declaration(parser);
    }

    if (match_token(parser, TOKEN_PR)) {
        return parse_print_statement(parser);
    }

    if (match_token(parser, TOKEN_FN)) {
        return parse_fn_declaration(parser);
    }

    if (match_token(parser, TOKEN_LP)) {
        return parse_while_loop(parser);
    }

    if (match_token(parser, TOKEN_FR)) {
        return parse_for_loop(parser);
    }

    if (match_token(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }

    if (match_token(parser, TOKEN_RT) || match_token(parser, TOKEN_PLACE)) {
        return parse_return_statement(parser);
    }

    if (match_token(parser, TOKEN_BK)) {
        return ast_create_break(parser->previous.pos);
    }

    if (match_token(parser, TOKEN_CN)) {
        return ast_create_continue(parser->previous.pos);
    }

    if (match_token(parser, TOKEN_COLON_COLON)) {
        ASTNode* b = parse_block_body(parser);
        consume_token(parser, TOKEN_COLON_COLON, "Expected '::' to close block");
        return b;
    }

    return parse_assignment_or_expr(parser);
}

static ASTNode* parse_assignment_or_expr(Parser* parser) {
    SourcePos pos = parser->current.pos;

    if (check_token(parser, TOKEN_IDENT)) {
        char* ident_name = yalu_strdup(parser->current.as.str_val ? parser->current.as.str_val : "");
        advance_token(parser);

        /* Case 1: Standard variable assignment (ident = expr) */
        if (parser->current.type == TOKEN_ASSIGN ||
            parser->current.type == TOKEN_PLUS_ASSIGN ||
            parser->current.type == TOKEN_MINUS_ASSIGN ||
            parser->current.type == TOKEN_STAR_ASSIGN ||
            parser->current.type == TOKEN_SLASH_ASSIGN) {

            TokenType op = parser->current.type;
            advance_token(parser);
            ASTNode* val = parse_expression(parser);
            ASTNode* assign_node = ast_create_var_assign(pos, ident_name, op, val);
            yalu_free(ident_name);
            return assign_node;
        }

        /* Case 2: Class property assignment in constructor (name.Point = name) */
        if (parser->current.type == TOKEN_DOT) {
            advance_token(parser); /* Consume . */
            Token class_tok = consume_token(parser, TOKEN_IDENT, "Expected class name after '.' in property assignment");
            char* class_name = yalu_strdup(class_tok.as.str_val ? class_tok.as.str_val : "");

            if (parser->current.type == TOKEN_ASSIGN ||
                parser->current.type == TOKEN_PLUS_ASSIGN ||
                parser->current.type == TOKEN_MINUS_ASSIGN ||
                parser->current.type == TOKEN_STAR_ASSIGN ||
                parser->current.type == TOKEN_SLASH_ASSIGN) {

                TokenType op = parser->current.type;
                advance_token(parser);
                ASTNode* val = parse_expression(parser);
                ASTNode* cp_node = ast_create_class_prop_assign(pos, ident_name, class_name, op, val);
                yalu_free(ident_name);
                yalu_free(class_name);
                return cp_node;
            }
            yalu_free(class_name);
        }

        /* Case: Method invocation (ident>>method(args)) */
        if (parser->current.type == TOKEN_GT_GT) {
            advance_token(parser);
            Token mtok = consume_token(parser, TOKEN_IDENT, "Expected method name after '>>'");
            char* mname = yalu_strdup(mtok.as.str_val ? mtok.as.str_val : "");
            consume_token(parser, TOKEN_LPAREN, "Expected '(' after method name");
            size_t arg_cap = 2;
            size_t arg_count = 0;
            ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * arg_cap);
            if (!check_token(parser, TOKEN_RPAREN)) {
                do {
                    ASTNode* arg = parse_method_arg(parser);
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = arg;
                } while (match_token(parser, TOKEN_COMMA));
            }
            consume_token(parser, TOKEN_RPAREN, "Expected ')' after method arguments");
            ASTNode* target = ast_create_var_ref(pos, ident_name);
            ASTNode* mcall = ast_create_method_call(pos, target, mname, args, arg_count);
            yalu_free(ident_name);
            yalu_free(mname);
            return ast_create_expr_stmt(pos, mcall);
        }

        /* Case 3: Object property assignment (object1>age = 50) */
        if (parser->current.type == TOKEN_PROP_ACCESS) {
            advance_token(parser); /* Consume > */
            char* prop_name = NULL;
            if (parser->current.type == TOKEN_STRING) {
                prop_name = yalu_strdup(parser->current.as.str_val ? parser->current.as.str_val : "");
                advance_token(parser);
            } else {
                Token prop_tok = consume_token(parser, TOKEN_IDENT, "Expected property name after '>'");
                prop_name = yalu_strdup(prop_tok.as.str_val ? prop_tok.as.str_val : "");
            }

            if (parser->current.type == TOKEN_ASSIGN ||
                parser->current.type == TOKEN_PLUS_ASSIGN ||
                parser->current.type == TOKEN_MINUS_ASSIGN ||
                parser->current.type == TOKEN_STAR_ASSIGN ||
                parser->current.type == TOKEN_SLASH_ASSIGN) {

                TokenType op = parser->current.type;
                advance_token(parser);
                ASTNode* val = parse_expression(parser);
                ASTNode* pa_node = ast_create_prop_assign(pos, ast_create_var_ref(pos, ident_name), prop_name, op, val);
                yalu_free(ident_name);
                yalu_free(prop_name);
                return pa_node;
            }

            /* Otherwise property access in expression statement */
            ASTNode* pa = ast_create_prop_access(pos, ast_create_var_ref(pos, ident_name), prop_name);
            yalu_free(ident_name);
            yalu_free(prop_name);
            return ast_create_expr_stmt(pos, pa);
        }

        /* Case 4: Indexed assignment (arr[idx] = val) */
        if (parser->current.type == TOKEN_LBRACKET) {
            advance_token(parser);
            ASTNode* target = ast_create_var_ref(pos, ident_name);
            ASTNode* index = parse_expression(parser);
            consume_token(parser, TOKEN_RBRACKET, "Expected ']' after array index");

            while (parser->current.type == TOKEN_LBRACKET) {
                target = ast_create_index(pos, target, index);
                advance_token(parser);
                index = parse_expression(parser);
                consume_token(parser, TOKEN_RBRACKET, "Expected ']' after array index");
            }

            if (parser->current.type == TOKEN_ASSIGN ||
                parser->current.type == TOKEN_PLUS_ASSIGN ||
                parser->current.type == TOKEN_MINUS_ASSIGN ||
                parser->current.type == TOKEN_STAR_ASSIGN ||
                parser->current.type == TOKEN_SLASH_ASSIGN) {

                TokenType op = parser->current.type;
                advance_token(parser);
                ASTNode* val = parse_expression(parser);
                ASTNode* idx_assign = ast_create_index_assign(pos, target, index, op, val);
                yalu_free(ident_name);
                return idx_assign;
            }

            ASTNode* idx_node = ast_create_index(pos, target, index);
            yalu_free(ident_name);
            return ast_create_expr_stmt(pos, idx_node);
        }

        /* Case 5: Function call (ident(...) or ident !!<...>>!!) */
        if (parser->current.type == TOKEN_LPAREN || parser->current.type == TOKEN_BANG_BANG_LT) {
            ASTNode* callee = ast_create_var_ref(pos, ident_name);
            yalu_free(ident_name);
            size_t arg_cap = 4;
            size_t arg_count = 0;
            ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * arg_cap);

            bool is_bang_style = (parser->current.type == TOKEN_BANG_BANG_LT);
            advance_token(parser);

            TokenType close_tok = is_bang_style ? TOKEN_GT_BANG_BANG : TOKEN_RPAREN;

            if (!check_token(parser, close_tok)) {
                do {
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = parse_expression(parser);
                } while (match_token(parser, TOKEN_COMMA));
            }

            consume_token(parser, close_tok, is_bang_style ? "Expected '>!!' or '>>!!' after arguments" : "Expected ')' after arguments");
            ASTNode* call_node = ast_create_call(pos, callee, args, arg_count);
            return ast_create_expr_stmt(pos, call_node);
        }

        /* Standalone identifier: check if binary op follows */
        ASTNode* var_ref = ast_create_var_ref(pos, ident_name);
        yalu_free(ident_name);
        if (check_token(parser, TOKEN_PLUS) || check_token(parser, TOKEN_MINUS) ||
            check_token(parser, TOKEN_STAR) || check_token(parser, TOKEN_SLASH) ||
            check_token(parser, TOKEN_PERCENT) || check_token(parser, TOKEN_EQ) ||
            check_token(parser, TOKEN_NE) || check_token(parser, TOKEN_LT) ||
            check_token(parser, TOKEN_LE) || check_token(parser, TOKEN_GT) ||
            check_token(parser, TOKEN_GE)) {
            TokenType op = parser->current.type;
            advance_token(parser);
            ASTNode* right = parse_expression(parser);
            return ast_create_expr_stmt(pos, ast_create_binary_op(pos, op, var_ref, right));
        }

        return ast_create_expr_stmt(pos, var_ref);
    }

    ASTNode* expr = parse_expression(parser);
    return ast_create_expr_stmt(pos, expr);
}

static ASTNode* parse_expression(Parser* parser) {
    return parse_logical_or(parser);
}

static ASTNode* parse_logical_or(Parser* parser) {
    ASTNode* left = parse_logical_and(parser);

    while (match_token(parser, TOKEN_PIPE_PIPE) || match_token(parser, TOKEN_OR)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_logical_and(parser);
        left = ast_create_binary_op(pos, op, left, right);
    }

    return left;
}

static ASTNode* parse_logical_and(Parser* parser) {
    ASTNode* left = parse_equality(parser);

    while (match_token(parser, TOKEN_AMP_AMP) || match_token(parser, TOKEN_AND)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_equality(parser);
        left = ast_create_binary_op(pos, op, left, right);
    }

    return left;
}

static ASTNode* parse_equality(Parser* parser) {
    ASTNode* left = parse_comparison(parser);

    while (match_token(parser, TOKEN_EQ) || match_token(parser, TOKEN_NE)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_comparison(parser);
        left = ast_create_binary_op(pos, op, left, right);
    }

    return left;
}

static ASTNode* parse_comparison(Parser* parser) {
    ASTNode* left = parse_range(parser);

    while (match_token(parser, TOKEN_LT) || match_token(parser, TOKEN_LE) ||
           match_token(parser, TOKEN_GT) || match_token(parser, TOKEN_GE)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_range(parser);
        left = ast_create_binary_op(pos, op, left, right);
    }

    return left;
}

static ASTNode* parse_range(Parser* parser) {
    ASTNode* left = parse_term(parser);

    if (match_token(parser, TOKEN_AT)) {
        SourcePos pos = parser->previous.pos;
        ASTNode* end = parse_term(parser);
        ASTNode* step = NULL;
        if (match_token(parser, TOKEN_AT)) {
            step = parse_term(parser);
        }
        return ast_create_range(pos, left, end, step);
    }

    return left;
}

static ASTNode* parse_term(Parser* parser) {
    ASTNode* left = parse_factor(parser);

    while (match_token(parser, TOKEN_PLUS) || match_token(parser, TOKEN_MINUS) || match_token(parser, TOKEN_LT_GT)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_factor(parser);
        left = ast_create_binary_op(pos, op, left, right);
    }

    return left;
}

static ASTNode* parse_factor(Parser* parser) {
    ASTNode* left = parse_unary(parser);

    while (match_token(parser, TOKEN_STAR) || match_token(parser, TOKEN_SLASH) || match_token(parser, TOKEN_PERCENT)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_unary(parser);
        left = ast_create_binary_op(pos, op, left, right);
    }

    return left;
}

static ASTNode* parse_unary(Parser* parser) {
    if (match_token(parser, TOKEN_MINUS) || match_token(parser, TOKEN_BANG) || match_token(parser, TOKEN_NOT)) {
        TokenType op = parser->previous.type;
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_unary(parser);
        return ast_create_unary_op(pos, op, right);
    }

    return parse_call_or_index(parser);
}

static ASTNode* parse_method_arg(Parser* parser) {
    ASTNode* left = parse_expression(parser);
    if (match_token(parser, TOKEN_DOT_DOT)) {
        SourcePos pos = parser->previous.pos;
        ASTNode* right = parse_expression(parser);
        return ast_create_binary_op(pos, TOKEN_DOT_DOT, left, right);
    }
    return left;
}

static ASTNode* parse_call_or_index(Parser* parser) {
    ASTNode* expr = parse_primary(parser);

    while (true) {
        if (match_token(parser, TOKEN_PROP_ACCESS)) {
            SourcePos pos = parser->previous.pos;
            char* prop_name = NULL;
            if (match_token(parser, TOKEN_STRING)) {
                prop_name = yalu_strdup(parser->previous.as.str_val ? parser->previous.as.str_val : "");
            } else {
                Token prop_tok = consume_token(parser, TOKEN_IDENT, "Expected property name after '>'");
                prop_name = yalu_strdup(prop_tok.as.str_val ? prop_tok.as.str_val : "");
            }
            expr = ast_create_prop_access(pos, expr, prop_name);
            yalu_free(prop_name);
        } else if (match_token(parser, TOKEN_GT_GT)) {
            SourcePos pos = parser->previous.pos;
            Token mtok = consume_token(parser, TOKEN_IDENT, "Expected method name after '>>'");
            char* mname = yalu_strdup(mtok.as.str_val ? mtok.as.str_val : "");
            consume_token(parser, TOKEN_LPAREN, "Expected '(' after method name");
            size_t arg_cap = 2;
            size_t arg_count = 0;
            ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * arg_cap);
            if (!check_token(parser, TOKEN_RPAREN)) {
                do {
                    ASTNode* arg = parse_method_arg(parser);
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = arg;
                } while (match_token(parser, TOKEN_COMMA));
            }
            consume_token(parser, TOKEN_RPAREN, "Expected ')' after method arguments");
            expr = ast_create_method_call(pos, expr, mname, args, arg_count);
            yalu_free(mname);
        } else if (match_token(parser, TOKEN_LPAREN)) {
            SourcePos pos = parser->previous.pos;
            size_t arg_cap = 4;
            size_t arg_count = 0;
            ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * arg_cap);

            if (!check_token(parser, TOKEN_RPAREN)) {
                do {
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = parse_expression(parser);
                } while (match_token(parser, TOKEN_COMMA));
            }

            consume_token(parser, TOKEN_RPAREN, "Expected ')' after function arguments");
            expr = ast_create_call(pos, expr, args, arg_count);
        } else if (match_token(parser, TOKEN_BANG_BANG_LT)) {
            SourcePos pos = parser->previous.pos;
            size_t arg_cap = 4;
            size_t arg_count = 0;
            ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * arg_cap);

            if (!check_token(parser, TOKEN_GT_BANG_BANG)) {
                do {
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = parse_expression(parser);
                } while (match_token(parser, TOKEN_COMMA));
            }

            consume_token(parser, TOKEN_GT_BANG_BANG, "Expected '>!!' or '>>!!' after arguments");
            expr = ast_create_call(pos, expr, args, arg_count);
        } else if (match_token(parser, TOKEN_LBRACKET)) {
            SourcePos pos = parser->previous.pos;
            ASTNode* index = parse_expression(parser);
            consume_token(parser, TOKEN_RBRACKET, "Expected ']' after array index");
            expr = ast_create_index(pos, expr, index);
        } else {
            break;
        }
    }

    return expr;
}

static void unpack_lt_gt_values(ASTNode* node, ASTNode*** out_vals, size_t* count, size_t* cap) {
    if (node && node->type == AST_BINARY_OP && node->as.binary_op.op == TOKEN_LT_GT) {
        unpack_lt_gt_values(node->as.binary_op.left, out_vals, count, cap);
        unpack_lt_gt_values(node->as.binary_op.right, out_vals, count, cap);
        yalu_free(node);
    } else if (node) {
        if (*count >= *cap) {
            *cap *= 2;
            *out_vals = (ASTNode**)yalu_realloc(*out_vals, sizeof(ASTNode*) * (*cap));
        }
        (*out_vals)[(*count)++] = node;
    }
}

static ASTNode* parse_book_literal(Parser* parser) {
    SourcePos pos = parser->previous.pos;
    size_t cap = 4;
    size_t count = 0;
    ASTBookEntry* entries = (ASTBookEntry*)yalu_malloc(sizeof(ASTBookEntry) * cap);

    while (!check_token(parser, TOKEN_RBRACE) && !check_token(parser, TOKEN_EOF)) {
        size_t key_cap = 2;
        size_t key_count = 0;
        char** keys = (char**)yalu_malloc(sizeof(char*) * key_cap);

        do {
            char* key_name = NULL;
            if (match_token(parser, TOKEN_STRING)) {
                key_name = yalu_strdup(parser->previous.as.str_val ? parser->previous.as.str_val : "");
            } else if (match_token(parser, TOKEN_IDENT)) {
                key_name = yalu_strdup(parser->previous.as.str_val ? parser->previous.as.str_val : "");
            } else {
                error_at_current(parser, "Expected string or identifier for Book key");
                break;
            }
            if (key_count >= key_cap) {
                key_cap *= 2;
                keys = (char**)yalu_realloc(keys, sizeof(char*) * key_cap);
            }
            keys[key_count++] = key_name;
        } while (match_token(parser, TOKEN_LT_GT));

        consume_token(parser, TOKEN_DOT_DOT, "Expected '..' after key in Book entry");

        size_t val_cap = 2;
        size_t val_count = 0;
        ASTNode** values = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * val_cap);

        do {
            ASTNode* val = parse_expression(parser);
            if (val_count >= val_cap) {
                val_cap *= 2;
                values = (ASTNode**)yalu_realloc(values, sizeof(ASTNode*) * val_cap);
            }
            values[val_count++] = val;
        } while (match_token(parser, TOKEN_LT_GT));

        if (key_count > 1 && val_count == 1 && values[0]->type == AST_BINARY_OP && values[0]->as.binary_op.op == TOKEN_LT_GT) {
            size_t new_cap = key_count;
            size_t new_count = 0;
            ASTNode** new_vals = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * new_cap);
            unpack_lt_gt_values(values[0], &new_vals, &new_count, &new_cap);
            yalu_free(values);
            values = new_vals;
            val_count = new_count;
        }

        if (count >= cap) {
            cap *= 2;
            entries = (ASTBookEntry*)yalu_realloc(entries, sizeof(ASTBookEntry) * cap);
        }
        entries[count].pair_count = (key_count > val_count) ? key_count : val_count;
        entries[count].keys = keys;
        entries[count].values = values;
        count++;

        match_token(parser, TOKEN_COMMA);
    }

    consume_token(parser, TOKEN_RBRACE, "Expected '}' after Book entries");
    return ast_create_book_lit(pos, entries, count);
}

static ASTNode* parse_primary(Parser* parser) {
    SourcePos pos = parser->current.pos;

    if (match_token(parser, TOKEN_INT)) {
        return ast_create_literal(pos, value_make_int(parser->previous.as.int_val));
    }

    if (match_token(parser, TOKEN_FLOAT)) {
        return ast_create_literal(pos, value_make_float(parser->previous.as.float_val));
    }

    if (match_token(parser, TOKEN_BOOL)) {
        return ast_create_literal(pos, value_make_bool(parser->previous.as.bool_val));
    }

    if (match_token(parser, TOKEN_NULL)) {
        return ast_create_literal(pos, value_make_null());
    }

    if (match_token(parser, TOKEN_FULLCLASS)) {
        return ast_create_fullclass(pos);
    }

    if (match_token(parser, TOKEN_STRING)) {
        return parse_string_with_interpolation(parser, parser->previous);
    }

    /* Object creation: /Point(...) */
    if (match_token(parser, TOKEN_SLASH)) {
        Token class_tok = consume_token(parser, TOKEN_IDENT, "Expected class name after '/' for object creation");
        char* class_name = yalu_strdup(class_tok.as.str_val ? class_tok.as.str_val : "");

        size_t arg_cap = 4;
        size_t arg_count = 0;
        ASTNode** args = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * arg_cap);

        if (match_token(parser, TOKEN_LPAREN)) {
            if (!check_token(parser, TOKEN_RPAREN)) {
                do {
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = parse_expression(parser);
                } while (match_token(parser, TOKEN_COMMA));
            }
            consume_token(parser, TOKEN_RPAREN, "Expected ')' after constructor arguments");
        } else if (match_token(parser, TOKEN_BANG_BANG_LT)) {
            if (!check_token(parser, TOKEN_GT_BANG_BANG)) {
                do {
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = (ASTNode**)yalu_realloc(args, sizeof(ASTNode*) * arg_cap);
                    }
                    args[arg_count++] = parse_expression(parser);
                } while (match_token(parser, TOKEN_COMMA));
            }
            consume_token(parser, TOKEN_GT_BANG_BANG, "Expected '>!!' or '>>!!' after constructor arguments");
        }

        ASTNode* obj_node = ast_create_new_object(pos, class_name, args, arg_count);
        yalu_free(class_name);
        return obj_node;
    }

    if (match_token(parser, TOKEN_IDENT)) {
        return ast_create_var_ref(pos, parser->previous.as.str_val);
    }

    /* Input expressions: inp.int, inp.str, inp.flt */
    if (match_token(parser, TOKEN_INP_INT) || match_token(parser, TOKEN_INP_STR) || match_token(parser, TOKEN_INP_FLT)) {
        TokenType input_type = parser->previous.type;
        ASTNode* prompt = NULL;

        if (check_token(parser, TOKEN_STRING) || check_token(parser, TOKEN_LPAREN)) {
            prompt = parse_unary(parser);
        }
        return ast_create_input(pos, input_type, prompt);
    }

    /* Parentheses: ( expr ) */
    if (match_token(parser, TOKEN_LPAREN)) {
        ASTNode* expr = parse_expression(parser);
        consume_token(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    /* Array literal: [ elem1, elem2, ... ] */
    if (match_token(parser, TOKEN_LBRACKET)) {
        size_t cap = 4;
        size_t count = 0;
        ASTNode** elements = (ASTNode**)yalu_malloc(sizeof(ASTNode*) * cap);

        if (!check_token(parser, TOKEN_RBRACKET)) {
            do {
                if (count >= cap) {
                    cap *= 2;
                    elements = (ASTNode**)yalu_realloc(elements, sizeof(ASTNode*) * cap);
                }
                elements[count++] = parse_expression(parser);
            } while (match_token(parser, TOKEN_COMMA));
        }

        consume_token(parser, TOKEN_RBRACKET, "Expected ']' after array literal elements");
        return ast_create_array_lit(pos, elements, count);
    }

    /* Book literal: { key .. val, ... } */
    if (match_token(parser, TOKEN_LBRACE)) {
        return parse_book_literal(parser);
    }

    error_at_current(parser, "Expected expression");
    advance_token(parser);
    return ast_create_literal(pos, value_make_null());
}

ASTNode* parser_parse(Parser* parser) {
    SourcePos pos = make_pos(parser->filename, 1, 1);
    ASTNode* program = ast_create_program(pos);

    while (!check_token(parser, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            ast_program_add(program, stmt);
        }
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }

    if (parser->had_error) {
        ast_free(program);
        return NULL;
    }

    return program;
}
