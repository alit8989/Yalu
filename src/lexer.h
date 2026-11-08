#ifndef YALU_LEXER_H
#define YALU_LEXER_H

#include "common.h"

typedef enum {
    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,

    /* Literals */
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_BOOL,
    TOKEN_NULL,
    TOKEN_IDENT,

    /* Keywords */
    TOKEN_FN,          /* fn */
    TOKEN_LP,          /* lp (while loop) */
    TOKEN_FR,          /* fr (for loop) */
    TOKEN_IN,          /* in */
    TOKEN_PR,          /* pr (print) */
    TOKEN_RT,          /* rt / return */
    TOKEN_PLACE,       /* place (return value) */
    TOKEN_IF,          /* if */
    TOKEN_ELIF,        /* elif */
    TOKEN_EL,          /* el / else */
    TOKEN_COLON_EI,    /* :ei */
    TOKEN_ELSE_MARKER, /* <<""else"">> */
    TOKEN_BK,          /* bk / break */
    TOKEN_CN,          /* cn / continue */
    TOKEN_AND,         /* and */
    TOKEN_OR,          /* or */
    TOKEN_NOT,         /* not */
    TOKEN_USE,         /* use */
    TOKEN_CLASS,       /* class */
    TOKEN_FULLCLASS,   /* fullclass */

    /* Input Keywords */
    TOKEN_INP_INT,     /* inp.int */
    TOKEN_INP_STR,     /* inp.str */
    TOKEN_INP_FLT,     /* inp.flt */

    /* Yalu Distinctive Delimiters */
    TOKEN_COLON_COLON,   /* :: (block open/close) */
    TOKEN_BANG_BANG,     /* !! (condition open/close) */
    TOKEN_BANG_BANG_LT,  /* !!< or !!<< (param list open) */
    TOKEN_GT_BANG_BANG,  /* >!! or >>!! (param list close) */
    TOKEN_AT,            /* @ (range operator) */
    TOKEN_PROP_ACCESS,   /* > (property access) */
    TOKEN_GT_GT,         /* >> (method invocation) */
    TOKEN_DOT_DOT,       /* .. (key-value pair separator) */
    TOKEN_LT_GT,         /* <> (diamond / list concatenation) */

    /* Arithmetic Operators */
    TOKEN_PLUS,          /* + */
    TOKEN_MINUS,         /* - */
    TOKEN_STAR,          /* * */
    TOKEN_SLASH,         /* / */
    TOKEN_PERCENT,       /* % */

    /* Assignment Operators */
    TOKEN_ASSIGN,        /* = */
    TOKEN_PLUS_ASSIGN,   /* += */
    TOKEN_MINUS_ASSIGN,  /* -= */
    TOKEN_STAR_ASSIGN,   /* *= */
    TOKEN_SLASH_ASSIGN,  /* /= */

    /* Comparison Operators */
    TOKEN_EQ,            /* == */
    TOKEN_NE,            /* != */
    TOKEN_LT,            /* < */
    TOKEN_LE,            /* <= */
    TOKEN_GT,            /* > */
    TOKEN_GE,            /* >= */

    /* Logical Operators */
    TOKEN_AMP_AMP,       /* && */
    TOKEN_PIPE_PIPE,     /* || */
    TOKEN_BANG,          /* ! */

    /* Punctuation */
    TOKEN_LPAREN,        /* ( */
    TOKEN_RPAREN,        /* ) */
    TOKEN_LBRACKET,      /* [ */
    TOKEN_RBRACKET,      /* ] */
    TOKEN_LBRACE,        /* { */
    TOKEN_RBRACE,        /* } */
    TOKEN_COMMA,         /* , */
    TOKEN_COLON,         /* : */
    TOKEN_DOT,           /* . */
    TOKEN_SEMICOLON      /* ; */
} TokenType;

typedef struct {
    TokenType type;
    SourcePos pos;
    const char* start;
    size_t length;
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        char* str_val;
    } as;
} Token;

typedef struct {
    const char* filename;
    const char* source;
    const char* cursor;
    int line;
    int col;
    char prev_char; /* Tracks previous non-whitespace character for context */
} Lexer;

void   lexer_init(Lexer* lexer, const char* filename, const char* source);
Token  lexer_next_token(Lexer* lexer);
void   token_free(Token* token);
const char* token_type_name(TokenType type);

#endif /* YALU_LEXER_H */
