#include <stdlib.h>
#include <assert.h>
#include "lexer.h"
#include "error.h"

static int get_c(lexer_t *lexer)
{
    int c = getc(lexer->fp);
    if (c == '\n') {
        lexer->line++;
        lexer->prev_column = lexer->column;
        lexer->column = 0;
    } else
        lexer->column++;
    return c;
}

static void unget_c(int c, lexer_t *lexer)
{
    if (c == '\n') {
        lexer->line--;
        lexer->column = lerxer->prev_column;
    } else
        lexer->column--;
    ungetc(c, lexer->fp);
}

void lexer_init(lexer_t *lexer, const char *fname, FILE *fp)
{
    assert(lexer && (fname || fp));
    lexer->fp = fp ? fp : fopen(fname, "r");
    if (!lexer->fp)
        error("Can't open file %s\n", fname);
    lexer->fname = fname;
    lexer->line = 1;
    lexer->column = lexer->prev_column = 0;
    lexer->untoken = NULL;
}

static token_t *make_token(int type)
{
    token_t *token = malloc(sizeof(*token));
    token->type = type;
    return token;
}


void lex_whitespace(lexer_t *lexer)
{
    int c;

    while (isspace(c = get_c(lexer)))
        ;
    unget_c(c, lexer);
}

token_t *get_token(lexer_t *lexer)
{
    int c;

    assert(lexer);
    if (lexer->untoken) {
        token_t *temp = lexer->untoken;
        lexer->untoken = NULL;
        return temp;
    }

    lex_whitespace(lexer);
    c = get_c(lexer);
    switch (c) {
    case '[': case ']': case '(': case ')': case '{': case '}': case '.':
    case ':': case ',': case ';': case '?':
        return make_punct(c);

    }
}

