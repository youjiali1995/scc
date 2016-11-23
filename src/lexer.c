#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "lexer.h"
#include "error.h"
#include "dict.h"

/* i/o functions */
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
        lexer->column = lexer->prev_column;
    } else
        lexer->column--;
    ungetc(c, lexer->fp);
}

static int expect_c(int c, lexer_t *lexer)
{
    int c1 = get_c(lexer);
    if (c1 == c)
        return 1;
    else {
        unget_c(c1, lexer);
        return 0;
    }
}

/* token constructors */
#define NEW_TOKEN(token, tp) \
    do { \
        (token) = malloc(sizeof(*(token))); \
        (token)->type = (tp); \
    } while (0)

static token_t *make_punct(int c)
{
    token_t *token;
    NEW_TOKEN(token, TK_PUNCT);
    token->ival = c;
    return token;
}

static token_t *make_punct_2(lexer_t *lexer, int punct_type, int expect1, int punct_type1)
{
    if (expect_c(expect1, lexer))
        return make_punct(punct_type1);
    else
        return make_punct(punct_type);
}

static token_t *make_punct_3(lexer_t *lexer, int punct_type, int expect1, int punct_type1, int expect2, int punct_type2)
{
    int c = get_c(lexer);
    if (c == expect1)
        return make_punct(punct_type1);
    else if (c == expect2)
        return make_punct(punct_type2);
    else {
        unget_c(c, lexer);
        return make_punct(punct_type);
    }
}

/* lex functions*/
static void lex_whitespace(lexer_t *lexer)
{
    int c;

    while (isspace(c = get_c(lexer)))
        ;
    unget_c(c, lexer);
}

static token_t *lex_number(lexer_t *lexer, int c)
{

}

/* lexer interface */
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


token_t *get_token(lexer_t *lexer)
{
    int c;

    if (lexer->untoken) {
        token_t *temp = lexer->untoken;
        lexer->untoken = NULL;
        return temp;
    }

    lex_whitespace(lexer);
    c = get_c(lexer);
    switch (c) {
    case '[': case ']': case '(': case ')': case '{': case '}': case '.':
    case '~': case ':': case ',': case ';': case '?':
        return make_punct(c);

    case '+':
        return make_punct_3(lexer, '+', '+', PUNCT_INC, '=', PUNCT_IADD);

    case '&':
        return make_punct_3(lexer, '&', '&', PUNCT_AND, '=', PUNCT_IAND);

    case '|':
        return make_punct_3(lexer, '|', '|', PUNCT_OR, '=', PUNCT_IOR);

    case '-':
        c = get_c(lexer);
        if (c == '-')
            return make_punct(PUNCT_DEC);
        else if (c == '=')
            return make_punct(PUNCT_ISUB);
        else if (c == '>')
            return make_punct(PUNCT_ARROW);
        else {
            unget_c(c, lexer);
            return make_punct('-');
        }

    case '*':
        return make_punct_2(lexer, '*', '=', PUNCT_IMUL);

    case '/':
        return make_punct_2(lexer, '/', '=', PUNCT_IDIV);

    case '%':
        return make_punct_2(lexer, '%', '=', PUNCT_IMOD);

    case '=':
        return make_punct_2(lexer, '=', '=', PUNCT_EQ);

    case '!':
        return make_punct_2(lexer, '!', '=', PUNCT_NE);

    case '^':
        return make_punct_2(lexer, '^', '=', PUNCT_IXOR);

    case '<':
        c = get_c(lexer);
        if (c == '=')
            return make_punct(PUNCT_LE);
        else if (c == '<')
            return make_punct_2(lexer, PUNCT_LSFT, '=', PUNCT_ILSFT);
        else {
            unget_c(c, lexer);
            return make_punct('<');
        }

    case '>':
        c = get_c(lexer);
        if (c == '=')
            return make_punct(PUNCT_GE);
        else if (c == '>')
            return make_punct_2(lexer, PUNCT_RSFT, '=', PUNCT_IRSFT);
        else {
            unget_c(c, lexer);
            return make_punct('>');
        }

    case '\'':
        return lex_char(lexer);

    case '\"':
        return lex_string(lexer);

    default:
        if (isdigit(c))
            return lex_number(lexer, c);
        else if (isalpha(c) || c == '_')
            return lex_id(lexer, c);
        else
            error("Unknown char %c\n", c);
    }
}

