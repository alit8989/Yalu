#include "lexer.h"
#include "errors.h"

void lexer_init(Lexer* lexer, const char* filename, const char* source) {
    lexer->filename = filename;
    lexer->source = source;
    lexer->cursor = source;
    lexer->line = 1;
    lexer->col = 1;
    lexer->prev_char = '\0';
}

static char peek(Lexer* lexer) {
    return *lexer->cursor;
}

static char peek_next(Lexer* lexer) {
    if (*lexer->cursor == '\0') return '\0';
    return *(lexer->cursor + 1);
}

static char advance(Lexer* lexer) {
    char c = *lexer->cursor;
    if (c != '\0') {
        lexer->cursor++;
        if (c == '\n') {
            lexer->line++;
            lexer->col = 1;
        } else {
            lexer->col++;
        }
        if (!isspace((unsigned char)c)) {
            lexer->prev_char = c;
        }
    }
    return c;
}

static bool match(Lexer* lexer, char expected) {
    if (*lexer->cursor == expected) {
        advance(lexer);
        return true;
    }
    return false;
}

static bool is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_ident_part(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool check_else_marker(const char* p, size_t* out_len) {
    if (p[0] != '<' || p[1] != '<' || p[2] != '"' || p[3] != '"') {
        return false;
    }
    const char* cur = p + 4;
    while (*cur == ' ' || *cur == '\t') cur++;
    if (strncmp(cur, "else", 4) != 0 && strncmp(cur, "ELSE", 4) != 0) {
        return false;
    }
    cur += 4;
    while (*cur == ' ' || *cur == '\t') cur++;
    if (cur[0] == '"' && cur[1] == '"' && cur[2] == '>' && cur[3] == '>') {
        if (out_len) *out_len = (size_t)(cur + 4 - p);
        return true;
    }
    return false;
}

static void skip_whitespace_and_comments(Lexer* lexer) {
    while (true) {
        char c = peek(lexer);

        /* Standard whitespace */
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lexer);
            continue;
        }

        /* Check for else marker <<""else"">> which should NOT be skipped as comment */
        if (check_else_marker(lexer->cursor, NULL)) {
            break;
        }

        /* Yalu comment / section marker: <<"" ... "">> */
        if (c == '<' && peek_next(lexer) == '<') {
            const char* p = lexer->cursor + 2;
            if (p[0] == '"' && p[1] == '"') {
                /* Confirmed <<"" */
                SourcePos comment_pos = make_pos(lexer->filename, lexer->line, lexer->col);
                advance(lexer); /* < */
                advance(lexer); /* < */
                advance(lexer); /* " */
                advance(lexer); /* " */

                bool closed = false;
                while (peek(lexer) != '\0') {
                    if (peek(lexer) == '"' && peek_next(lexer) == '"') {
                        const char* look = lexer->cursor + 2;
                        if (look[0] == '>' && look[1] == '>') {
                            advance(lexer); /* " */
                            advance(lexer); /* " */
                            advance(lexer); /* > */
                            advance(lexer); /* > */
                            closed = true;
                            break;
                        }
                    }
                    advance(lexer);
                }

                if (!closed) {
                    yalu_report_error(comment_pos, "Lexer Error", "Unterminated comment marker '<<\"\"'");
                }
                continue;
            }
        }

        /* Single-line comments starting with # */
        if (c == '#') {
            while (peek(lexer) != '\0' && peek(lexer) != '\n') {
                advance(lexer);
            }
            continue;
        }

        /* Single-line comments starting with // */
        if (c == '/' && peek_next(lexer) == '/') {
            while (peek(lexer) != '\0' && peek(lexer) != '\n') {
                advance(lexer);
            }
            continue;
        }

        break;
    }
}

static Token make_token(Lexer* lexer, TokenType type, SourcePos pos, const char* start, size_t len) {
    (void)lexer;
    Token token;
    memset(&token, 0, sizeof(Token));
    token.type = type;
    token.pos = pos;
    token.start = start;
    token.length = len;
    return token;
}

static Token make_error_token(Lexer* lexer, SourcePos pos, const char* message) {
    (void)lexer;
    yalu_report_error(pos, "Lexer Error", "%s", message);
    Token token;
    memset(&token, 0, sizeof(Token));
    token.type = TOKEN_ERROR;
    token.pos = pos;
    token.as.str_val = yalu_strdup(message);
    return token;
}

void token_free(Token* token) {
    if (!token) return;
    if (token->type == TOKEN_STRING || token->type == TOKEN_IDENT || token->type == TOKEN_ERROR) {
        yalu_free(token->as.str_val);
        token->as.str_val = NULL;
    }
}

const char* token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "Error";
        case TOKEN_INT: return "Integer";
        case TOKEN_FLOAT: return "Float";
        case TOKEN_STRING: return "String";
        case TOKEN_BOOL: return "Boolean";
        case TOKEN_NULL: return "Null";
        case TOKEN_IDENT: return "Identifier";
        case TOKEN_FN: return "fn";
        case TOKEN_LP: return "lp";
        case TOKEN_FR: return "fr";
        case TOKEN_IN: return "in";
        case TOKEN_PR: return "pr";
        case TOKEN_RT: return "rt";
        case TOKEN_PLACE: return "place";
        case TOKEN_IF: return "if";
        case TOKEN_ELIF: return "elif";
        case TOKEN_EL: return "el";
        case TOKEN_COLON_EI: return ":ei";
        case TOKEN_ELSE_MARKER: return "<<\"\"else\"\">>";
        case TOKEN_BK: return "bk";
        case TOKEN_CN: return "cn";
        case TOKEN_AND: return "and";
        case TOKEN_OR: return "or";
        case TOKEN_NOT: return "not";
        case TOKEN_USE: return "use";
        case TOKEN_CLASS: return "class";
        case TOKEN_FULLCLASS: return "fullclass";
        case TOKEN_INP_INT: return "inp.int";
        case TOKEN_INP_STR: return "inp.str";
        case TOKEN_INP_FLT: return "inp.flt";
        case TOKEN_COLON_COLON: return "::";
        case TOKEN_BANG_BANG: return "!!";
        case TOKEN_BANG_BANG_LT: return "!!<";
        case TOKEN_GT_BANG_BANG: return ">!!";
        case TOKEN_AT: return "@";
        case TOKEN_PROP_ACCESS: return ">";
        case TOKEN_GT_GT: return ">>";
        case TOKEN_DOT_DOT: return "..";
        case TOKEN_LT_GT: return "<>";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_PLUS_ASSIGN: return "+=";
        case TOKEN_MINUS_ASSIGN: return "-=";
        case TOKEN_STAR_ASSIGN: return "*=";
        case TOKEN_SLASH_ASSIGN: return "/=";
        case TOKEN_EQ: return "==";
        case TOKEN_NE: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_LE: return "<=";
        case TOKEN_GT: return ">";
        case TOKEN_GE: return ">=";
        case TOKEN_AMP_AMP: return "&&";
        case TOKEN_PIPE_PIPE: return "||";
        case TOKEN_BANG: return "!";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_DOT: return ".";
        case TOKEN_SEMICOLON: return ";";
        default: return "Unknown";
    }
}

static Token scan_string(Lexer* lexer, bool is_multiline) {
    SourcePos pos = make_pos(lexer->filename, lexer->line, lexer->col - (is_multiline ? 2 : 1));
    size_t cap = 64;
    size_t len = 0;
    char* buf = (char*)yalu_malloc(cap);

    bool closed = false;

    while (peek(lexer) != '\0') {
        if (is_multiline) {
            if (peek(lexer) == '"' && peek_next(lexer) == '"') {
                advance(lexer); /* " */
                advance(lexer); /* " */
                closed = true;
                break;
            }
        } else {
            if (peek(lexer) == '"') {
                advance(lexer); /* " */
                closed = true;
                break;
            }
            if (peek(lexer) == '\n') {
                break;
            }
        }

        char c = advance(lexer);
        if (c == '\\') {
            char next = advance(lexer);
            switch (next) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case 'r':  c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"':  c = '"'; break;
                case '/':  c = '/'; break;
                default:
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char*)yalu_realloc(buf, cap);
                    }
                    buf[len++] = '\\';
                    c = next;
                    break;
            }
        }

        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char*)yalu_realloc(buf, cap);
        }
        buf[len++] = c;
    }

    if (!closed) {
        yalu_free(buf);
        return make_error_token(lexer, pos, is_multiline ? "Unterminated multiline string '\"\"'" : "Unterminated string literal '\"'");
    }

    buf[len] = '\0';
    Token tok = make_token(lexer, TOKEN_STRING, pos, buf, len);
    tok.as.str_val = buf;
    return tok;
}

static Token scan_number(Lexer* lexer, SourcePos pos, const char* start) {
    bool is_float = false;

    while (isdigit((unsigned char)peek(lexer))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && isdigit((unsigned char)peek_next(lexer))) {
        is_float = true;
        advance(lexer);
        while (isdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
    }

    if ((peek(lexer) == 'e' || peek(lexer) == 'E') &&
        (isdigit((unsigned char)peek_next(lexer)) || peek_next(lexer) == '+' || peek_next(lexer) == '-')) {
        is_float = true;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        while (isdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
    }

    size_t len = (size_t)(lexer->cursor - start);
    char* num_str = yalu_strndup(start, len);

    Token tok = make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INT, pos, start, len);
    if (is_float) {
        tok.as.float_val = strtod(num_str, NULL);
    } else {
        tok.as.int_val = (int64_t)strtoll(num_str, NULL, 10);
    }
    yalu_free(num_str);
    return tok;
}

static Token scan_identifier_or_keyword(Lexer* lexer, SourcePos pos, const char* start) {
    while (is_ident_part(peek(lexer))) {
        advance(lexer);
    }

    size_t len = (size_t)(lexer->cursor - start);

    /* Check for inp.int, inp.str, inp.flt */
    if (len == 3 && strncmp(start, "inp", 3) == 0 && peek(lexer) == '.') {
        const char* dot = lexer->cursor;
        if (strncmp(dot, ".int", 4) == 0 && !is_ident_part(dot[4])) {
            advance(lexer); advance(lexer); advance(lexer); advance(lexer);
            return make_token(lexer, TOKEN_INP_INT, pos, start, (size_t)(lexer->cursor - start));
        }
        if (strncmp(dot, ".str", 4) == 0 && !is_ident_part(dot[4])) {
            advance(lexer); advance(lexer); advance(lexer); advance(lexer);
            return make_token(lexer, TOKEN_INP_STR, pos, start, (size_t)(lexer->cursor - start));
        }
        if ((strncmp(dot, ".flt", 4) == 0 || strncmp(dot, ".flo", 4) == 0) && !is_ident_part(dot[4])) {
            advance(lexer); advance(lexer); advance(lexer); advance(lexer);
            return make_token(lexer, TOKEN_INP_FLT, pos, start, (size_t)(lexer->cursor - start));
        }
    }

    TokenType type = TOKEN_IDENT;

    switch (len) {
        case 2:
            if (strncmp(start, "fn", 2) == 0) type = TOKEN_FN;
            else if (strncmp(start, "lp", 2) == 0) type = TOKEN_LP;
            else if (strncmp(start, "fr", 2) == 0) type = TOKEN_FR;
            else if (strncmp(start, "in", 2) == 0) type = TOKEN_IN;
            else if (strncmp(start, "pr", 2) == 0) type = TOKEN_PR;
            else if (strncmp(start, "rt", 2) == 0) type = TOKEN_RT;
            else if (strncmp(start, "if", 2) == 0) type = TOKEN_IF;
            else if (strncmp(start, "el", 2) == 0) type = TOKEN_EL;
            else if (strncmp(start, "bk", 2) == 0) type = TOKEN_BK;
            else if (strncmp(start, "cn", 2) == 0) type = TOKEN_CN;
            else if (strncmp(start, "Tr", 2) == 0) {
                Token tok = make_token(lexer, TOKEN_BOOL, pos, start, len);
                tok.as.bool_val = true;
                return tok;
            }
            else if (strncmp(start, "Fl", 2) == 0) {
                Token tok = make_token(lexer, TOKEN_BOOL, pos, start, len);
                tok.as.bool_val = false;
                return tok;
            }
            else if (strncmp(start, "or", 2) == 0) type = TOKEN_OR;
            break;
        case 3:
            if (strncmp(start, "and", 3) == 0) type = TOKEN_AND;
            else if (strncmp(start, "not", 3) == 0) type = TOKEN_NOT;
            else if (strncmp(start, "nil", 3) == 0) type = TOKEN_NULL;
            else if (strncmp(start, "use", 3) == 0) type = TOKEN_USE;
            break;
        case 4:
            if (strncmp(start, "elif", 4) == 0) type = TOKEN_ELIF;
            else if (strncmp(start, "else", 4) == 0) type = TOKEN_EL;
            else if (strncmp(start, "Null", 4) == 0) type = TOKEN_NULL;
            else if (strncmp(start, "null", 4) == 0) type = TOKEN_NULL;
            else if (strncmp(start, "true", 4) == 0) {
                Token tok = make_token(lexer, TOKEN_BOOL, pos, start, len);
                tok.as.bool_val = true;
                return tok;
            }
            break;
        case 5:
            if (strncmp(start, "false", 5) == 0) {
                Token tok = make_token(lexer, TOKEN_BOOL, pos, start, len);
                tok.as.bool_val = false;
                return tok;
            }
            else if (strncmp(start, "break", 5) == 0) type = TOKEN_BK;
            else if (strncmp(start, "place", 5) == 0) type = TOKEN_PLACE;
            else if (strncmp(start, "class", 5) == 0) type = TOKEN_CLASS;
            break;
        case 6:
            if (strncmp(start, "return", 6) == 0) type = TOKEN_RT;
            break;
        case 8:
            if (strncmp(start, "continue", 8) == 0) type = TOKEN_CN;
            break;
        case 9:
            if (strncmp(start, "fullclass", 9) == 0) type = TOKEN_FULLCLASS;
            break;
        default:
            break;
    }

    Token tok = make_token(lexer, type, pos, start, len);
    if (type == TOKEN_IDENT) {
        tok.as.str_val = yalu_strndup(start, len);
    }
    return tok;
}

static bool check_multiline_string_ahead(const char* cursor) {
    const char* p = cursor;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\r' && *p != '\n') return false;
    if (*p == '\r') p++;
    if (*p == '\n') p++;

    while (*p != '\0') {
        const char* line_start = p;
        while (*p != '\r' && *p != '\n' && *p != '\0') {
            if (*p == '"' && *(p + 1) == '"') {
                return true;
            }
            p++;
        }
        if (strncmp(line_start, "pr ", 3) == 0 ||
            strncmp(line_start, "fn ", 3) == 0 ||
            strncmp(line_start, "lp ", 3) == 0 ||
            strncmp(line_start, "fr ", 3) == 0 ||
            strncmp(line_start, "if ", 3) == 0 ||
            strncmp(line_start, "rt ", 3) == 0 ||
            strncmp(line_start, "place ", 6) == 0 ||
            strncmp(line_start, "use ", 4) == 0 ||
            strncmp(line_start, "class ", 6) == 0) {
            return false;
        }
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
    return false;
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace_and_comments(lexer);

    SourcePos pos = make_pos(lexer->filename, lexer->line, lexer->col);
    const char* start = lexer->cursor;

    /* Check for <<""else"">> */
    size_t else_len = 0;
    if (check_else_marker(start, &else_len)) {
        for (size_t i = 0; i < else_len; i++) {
            advance(lexer);
        }
        return make_token(lexer, TOKEN_ELSE_MARKER, pos, start, else_len);
    }

    char c = advance(lexer);

    if (c == '\0') {
        return make_token(lexer, TOKEN_EOF, pos, start, 0);
    }

    /* Check for N! (not-equal comparison operator) */
    if (c == 'N' && peek(lexer) == '!') {
        advance(lexer); /* consume '!' */
        return make_token(lexer, TOKEN_NE, pos, start, 2);
    }

    /* Check identifier / keyword */
    if (is_ident_start(c)) {
        return scan_identifier_or_keyword(lexer, pos, start);
    }

    /* Check number */
    if (isdigit((unsigned char)c)) {
        return scan_number(lexer, pos, start);
    }

    /* Check strings */
    if (c == '"') {
        if (peek(lexer) == '"') {
            if (check_multiline_string_ahead(lexer->cursor + 1)) {
                advance(lexer); /* second " */
                while (peek(lexer) == ' ' || peek(lexer) == '\t') {
                    advance(lexer);
                }
                if (peek(lexer) == '\r') advance(lexer);
                if (peek(lexer) == '\n') advance(lexer);
                return scan_string(lexer, true);
            } else {
                advance(lexer); /* second " */
                Token tok = make_token(lexer, TOKEN_STRING, pos, start, 2);
                tok.as.str_val = yalu_strdup("");
                return tok;
            }
        } else {
            return scan_string(lexer, false);
        }
    }

    /* Delimiters and operators */
    switch (c) {
        case ':':
            if (match(lexer, ':')) {
                return make_token(lexer, TOKEN_COLON_COLON, pos, start, 2);
            }
            if (peek(lexer) == 'e' && peek_next(lexer) == 'i') {
                const char* after = lexer->cursor + 2;
                if (!is_ident_part(*after)) {
                    advance(lexer); /* e */
                    advance(lexer); /* i */
                    return make_token(lexer, TOKEN_COLON_EI, pos, start, 3);
                }
            }
            return make_token(lexer, TOKEN_COLON, pos, start, 1);

        case '!':
            if (peek(lexer) == '!') {
                advance(lexer); /* ! */
                if (peek(lexer) == '<') {
                    advance(lexer); /* < */
                    if (peek(lexer) == '<') advance(lexer); /* optional << */
                    return make_token(lexer, TOKEN_BANG_BANG_LT, pos, start, (size_t)(lexer->cursor - start));
                }
                return make_token(lexer, TOKEN_BANG_BANG, pos, start, 2);
            }
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_NE, pos, start, 2);
            }
            return make_token(lexer, TOKEN_BANG, pos, start, 1);

        case '>': {
            if (peek(lexer) == '>') {
                const char* look = lexer->cursor + 1;
                if (look[0] == '!' && look[1] == '!') {
                    advance(lexer); /* > */
                    advance(lexer); /* ! */
                    advance(lexer); /* ! */
                    return make_token(lexer, TOKEN_GT_BANG_BANG, pos, start, 4);
                }
                while (*look == ' ' || *look == '\t') look++;
                if (look[0] == ':' && look[1] == ':') {
                    advance(lexer); /* second > */
                    return make_token(lexer, TOKEN_GT_BANG_BANG, pos, start, 2);
                }
                /* Method invocation operator >> (e.g. list>>inc, book>>add) */
                advance(lexer); /* second > */
                return make_token(lexer, TOKEN_GT_GT, pos, start, 2);
            } else if (peek(lexer) == '!' && peek_next(lexer) == '!') {
                advance(lexer); /* ! */
                advance(lexer); /* ! */
                return make_token(lexer, TOKEN_GT_BANG_BANG, pos, start, 3);
            }
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_GE, pos, start, 2);
            }
            if (match(lexer, '-')) {
                return make_token(lexer, TOKEN_GT, pos, start, 2); /* >- greater-than operator */
            }

            /* Property access: e.g. object1>name, book>"name" */
            char next_c = peek(lexer);
            if (start > lexer->source && !isspace((unsigned char)*(start - 1)) &&
                (is_ident_part(*(start - 1)) || *(start - 1) == ')' || *(start - 1) == ']' || *(start - 1) == '"') &&
                (is_ident_start(next_c) || next_c == '"')) {
                return make_token(lexer, TOKEN_PROP_ACCESS, pos, start, 1);
            }

            return make_token(lexer, TOKEN_GT, pos, start, 1);
        }

        case '<':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_LE, pos, start, 2);
            }
            if (match(lexer, '-')) {
                return make_token(lexer, TOKEN_LT, pos, start, 2); /* <- less-than operator */
            }
            if (match(lexer, '>')) {
                return make_token(lexer, TOKEN_LT_GT, pos, start, 2); /* <> list combination operator */
            }
            return make_token(lexer, TOKEN_LT, pos, start, 1);

        case '=':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_EQ, pos, start, 2);
            }
            return make_token(lexer, TOKEN_ASSIGN, pos, start, 1);

        case '+':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_PLUS_ASSIGN, pos, start, 2);
            }
            return make_token(lexer, TOKEN_PLUS, pos, start, 1);

        case '-':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_MINUS_ASSIGN, pos, start, 2);
            }
            return make_token(lexer, TOKEN_MINUS, pos, start, 1);

        case '*':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_STAR_ASSIGN, pos, start, 2);
            }
            return make_token(lexer, TOKEN_STAR, pos, start, 1);

        case '/':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_SLASH_ASSIGN, pos, start, 2);
            }
            return make_token(lexer, TOKEN_SLASH, pos, start, 1);

        case '%':
            return make_token(lexer, TOKEN_PERCENT, pos, start, 1);

        case '@':
            return make_token(lexer, TOKEN_AT, pos, start, 1);

        case '&':
            if (match(lexer, '&')) {
                return make_token(lexer, TOKEN_AMP_AMP, pos, start, 2);
            }
            break;

        case '|':
            if (match(lexer, '|')) {
                return make_token(lexer, TOKEN_PIPE_PIPE, pos, start, 2);
            }
            break;

        case '{': return make_token(lexer, TOKEN_LBRACE, pos, start, 1);
        case '}': return make_token(lexer, TOKEN_RBRACE, pos, start, 1);
        case '(': return make_token(lexer, TOKEN_LPAREN, pos, start, 1);
        case ')': return make_token(lexer, TOKEN_RPAREN, pos, start, 1);
        case '[': return make_token(lexer, TOKEN_LBRACKET, pos, start, 1);
        case ']': return make_token(lexer, TOKEN_RBRACKET, pos, start, 1);
        case ',': return make_token(lexer, TOKEN_COMMA, pos, start, 1);
        case '.':
            if (match(lexer, '.')) {
                return make_token(lexer, TOKEN_DOT_DOT, pos, start, 2);
            }
            return make_token(lexer, TOKEN_DOT, pos, start, 1);
        case ';': return make_token(lexer, TOKEN_SEMICOLON, pos, start, 1);

        default:
            break;
    }

    char err_buf[64];
    snprintf(err_buf, sizeof(err_buf), "Unexpected character '%c' (0x%02X)", c, (unsigned char)c);
    return make_error_token(lexer, pos, err_buf);
}
