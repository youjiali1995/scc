#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "util.h"
#include "dict.h"
#include "buffer.h"

/* buffer helper */
#define PUTC(buffer, c) \
    do { \
        buffer_push(buffer, &(c), sizeof(char)); \
    } while (0)

#define SET_STRING(buffer, s) \
    do { \
        (s) = malloc(sizeof(char) * (buffer->top + 1)); \
        memcpy(s, buffer->stack, buffer->top); \
        s[buffer->top] = '\0'; \
    } while (0)

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

static bool expect_c(int c, lexer_t *lexer)
{
    int c1 = get_c(lexer);

    if (c1 == c)
        return true;
    else {
        unget_c(c1, lexer);
        return false;
    }
}

static dict_t *kw;

/* type functions */
int is_keyword(const char *s)
{
    int ret;

    if (!kw) {
        kw = make_dict(NULL);
        dict_insert(kw, "void", (void *) KW_VOID, true);
        dict_insert(kw, "char", (void *) KW_CHAR, true);
        dict_insert(kw, "int", (void *) KW_INT, true);
        dict_insert(kw, "float", (void *) KW_FLOAT, true);
        dict_insert(kw, "double", (void *) KW_DOUBLE, true);
        dict_insert(kw, "for", (void *) KW_FOR, true);
        dict_insert(kw, "do", (void *) KW_DO, true);
        dict_insert(kw, "while", (void *) KW_WHILE, true);
        dict_insert(kw, "if", (void *) KW_IF, true);
        dict_insert(kw, "else", (void *) KW_ELSE, true);
        dict_insert(kw, "return", (void *) KW_RETURN, true);
    }
    /* triky */
    ret = (long) dict_lookup(kw, s);
    return ret ? ret : 0;
}


/* token constructors */
#define NEW_TOKEN(token, tp) \
    do { \
        (token) = malloc(sizeof(*(token))); \
        (token)->type = (tp); \
    } while (0)

static token_t *make_number(char *s)
{
    token_t *token;

    NEW_TOKEN(token, TK_NUMBER);
    token->sval = s;
    return token;
}

static token_t *make_char(int c)
{
    token_t *token;

    NEW_TOKEN(token, TK_CHAR);
    token->ival = c;
    return token;
}

static token_t *make_string(char *s)
{
    token_t *token;

    NEW_TOKEN(token, TK_STRING);
    token->sval = s;
    return token;
}

static token_t *make_keyword(int type)
{
    token_t *token;

    NEW_TOKEN(token, TK_KEYWORD);
    token->ival = type;
    return token;
}

static token_t *make_id(char *s)
{
    token_t *token;

    NEW_TOKEN(token, TK_ID);
    token->sval = s;
    return token;
}

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

static int lex_escape(int c)
{
    switch (c) {
    case '\'':
    case '\"':
    case '\?':
    case '\\':
        return c;
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    default:
        return -1;
    }
}

static token_t *lex_char(lexer_t *lexer)
{
    int c = get_c(lexer);

    if (c == '\\') {
        int temp = get_c(lexer);
        c = lex_escape(temp);
        if (c == -1)
            errorf("unknown escape sequence: \'\\%c\' in %s:%d:%d\n", temp, lexer->fname, lexer->line, lexer->column);
    }
    if (get_c(lexer) != '\'')
        errorf("missing terminating \' character in %s:%d:%d\n", lexer->fname, lexer->line, lexer->column);
    return make_char(c);
}

static token_t *lex_string(lexer_t *lexer)
{
    int c;
    char *s;
    buffer_t *string = make_buffer();

    for (c = get_c(lexer);; c = get_c(lexer))
        switch(c) {
        case '\"':
            SET_STRING(string, s);
            free_buffer(string);
            return make_string(s);

        case '\\': {
            int temp = get_c(lexer);
            c = lex_escape(temp);
            if (c == -1)
                errorf("unknown escape sequence \'\\%c\' in %s:%d:%d\n", temp, lexer->fname, lexer->line, lexer->column);
            PUTC(string, c);
            break;
        }

        default:
            if (c == EOF)
                errorf("missing terminating \" character in %s:%d:%d\n", lexer->fname, lexer->line, lexer->column);
            PUTC(string, c);
            break;
        }
}

static token_t *lex_id(lexer_t *lexer, int c)
{
    buffer_t *id = make_buffer();
    char *s;

    assert(isalpha(c) || c == '_');
    PUTC(id, c);
    for (c = get_c(lexer); isalnum(c) || c == '_'; c = get_c(lexer))
        PUTC(id, c);
    unget_c(c, lexer);

    SET_STRING(id, s);
    free_buffer(id);
    c = is_keyword(s);
    if (c) {
        free(s);
        return make_keyword(c);
    } else
        return make_id(s);
}

/* Read a number literal.
 * It only supports int, float and double. Different base numbers are not supported.
 */
static token_t *lex_number(lexer_t *lexer, char c)
{
    buffer_t *num = make_buffer();
    char *s;

    assert(isdigit(c));
    PUTC(num, c);
    for (c = get_c(lexer); isdigit(c); c = get_c(lexer))
        PUTC(num, c);
    if (c == 'f')
        errorf("invalid suffix \"f\" on integer constant int %s:%d:%d\n", lexer->fname, lexer->line, lexer->column);
    if (c == '.') {
        PUTC(num, c);
        c = get_c(lexer);
        if (!isdigit(c))
            errorf("expected digit after '.' in %s:%d:%d\n", lexer->fname, lexer->line, lexer->column);
        for (; isdigit(c); c = get_c(lexer))
            PUTC(num, c);
    }
    if (c == 'e' || c == 'E') {
        PUTC(num, c);
        c = get_c(lexer);
        if (c == '-' || c == '+') {
            PUTC(num, c);
            c = get_c(lexer);
        }
        if (!isdigit(c))
            errorf("expected digit after 'e' or 'E' in %s:%d:%d\n", lexer->fname, lexer->line, lexer->column);
        for (; isdigit(c); c = get_c(lexer))
            PUTC(num, c);
    }
    if (c == 'f' || c == 'F')
        PUTC(num, c);
    else
        unget_c(c, lexer);

    SET_STRING(num, s);
    free_buffer(num);
    return make_number(s);
}

/* lexer interface */
void lexer_init(lexer_t *lexer, const char *fname, FILE *fp)
{
    assert(lexer && fname);
    lexer->fp = fp ? fp : fopen(fname, "r");
    if (!lexer->fp)
        errorf("Can't open file %s\n", fname);
    lexer->fname = fname;
    lexer->line = 1;
    lexer->column = lexer->prev_column = 0;
    lexer->untoken = NULL;
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
        else if (c == EOF) {
            free_dict(kw, NULL, NULL);
            return NULL;
        } else {
            errorf("Unknown char %c in %s:%d:%d\n", c, lexer->fname, lexer->line, lexer->column);
            return NULL;
        }
    }
}

void unget_token(token_t *token, lexer_t *lexer)
{
    assert(lexer && !lexer->untoken);
    lexer->untoken = token;
}

token_t *peek_token(lexer_t *lexer)
{
    assert(lexer);
    if (!lexer->untoken)
        lexer->untoken = get_token(lexer);
    return lexer->untoken;
}

void free_token(token_t *token, bool free_sval)
{
    assert(token);
    if ((token->type == TK_ID || token->type == TK_NUMBER || token->type == TK_STRING)
            && free_sval)
        free(token->sval);
    free(token);
}
