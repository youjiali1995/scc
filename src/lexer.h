#ifndef LEXER_H__
#define LEXER_H__

#include <stdio.h>

enum {
    TK_KW,
    TK_ID,
    TK_NUM,
    TK_CHAR,
    TK_STRING,
    TK_PUNCT
};

typedef struct token_t {
    int type;
    union {
        /* string and identifier */
        char *s;
        double d;
        int n;
        /* char and punctuator */
        char c;
    };
} token_t;

typedef struct lexer_t {
    token_t *untoken;
    /* file */
    FILE *fp;
    const char *fname;
    unsigned int line;
    unsigned int column;
    unsigned int prev_column;
} lexer_t;

void lexer_init(lexer_t *lexer, const char *fname, FILE *fp);
token_t *get_token(lexer_t *lexer);
void unget_token(token_t *token, lexer_t *lexer);
token_t *peek_token(lexer_t *lexer);

#endif

