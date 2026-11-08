#ifndef YALU_PARSER_H
#define YALU_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
    const char* filename;
} Parser;

void     parser_init(Parser* parser, Lexer* lexer, const char* filename);
ASTNode* parser_parse(Parser* parser);

#endif /* YALU_PARSER_H */

