#ifndef LEXER_H__
#define LEXER_H__

#include <stdio.h>

/* token type */
enum {
    TK_KEYWORD,
    TK_ID,
    TK_NUMBER,
    TK_CHAR,
    TK_STRING,
    TK_PUNCT
};

/* keywords */
enum {
    KW_VOID = 1,
    KW_CHAR,
    KW_INT,
    KW_DOUBLE,
    KW_FOR,
    KW_WHILE,
    KW_IF,
    KW_ELSE,
    KW_RETURN
};

/* punctuators */
enum {
    PUNCT_INC = 256, /* ++ */
    PUNCT_IADD, /* += */
    PUNCT_AND, /* && */
    PUNCT_IAND, /* &= */
    PUNCT_OR, /* || */
    PUNCT_IOR, /* |= */

    PUNCT_DEC, /* -- */
    PUNCT_ISUB, /* -= */
    PUNCT_ARROW, /* -> */

    PUNCT_IMUL, /* *= */
    PUNCT_IDIV, /* /= */
    PUNCT_IMOD, /* %= */
    PUNCT_EQ, /* == */
    PUNCT_NE, /* != */
    PUNCT_IXOR, /* ^= */

    PUNCT_LE, /* <= */
    PUNCT_LSFT, /* << */
    PUNCT_ILSFT, /* <<= */
    PUNCT_GE, /* >= */
    PUNCT_RSFT, /* >> */
    PUNCT_IRSFT, /* >>= */

};

typedef struct token_t {
    int type;
    union {
        /* string, identifier and number */
        char *sval;
        /* char, punctuator and keyword */
        int ival;
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

