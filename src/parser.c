#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "parser.h"
#include "util.h"

ctype_t *ctype_void = &(ctype_t){CTYPE_VOID, 0};
ctype_t *cypte_char = &(ctype_t){CTYPE_CHAR, 1};
ctype_t *ctype_int = &(ctype_t){CTYPE_INT, 4};

#define NEXT() (get_token(parser->lexer))
#define PEEK() (peek_token(parser->lexer))
#define EXPECT_PUNCT(punct) \
    do { \
        token_t *token = NEXT(); \
        if (token->type != TK_PUNCT || token->ival != (punct)) \
            errorf("expected punctator %c\n", punct); \
    } while (0)
#define TRY_PUNCT(punct) \
    (is_punct(PEEK(), punct) ? (NEXT(), true) : false)
#define EXPECT_KW(keyword) \
    do { \
        token_t *token = NEXT(); \
        if (token->type != TK_KEYWORD || token->ival != keyword) \
            errorf("expected keyword %c\n", keyword); \
    } while (0)
#define TRY_KW(keyword) \
    (is_keyword(PEEK(), keyword) ? (NEXT(), true) : false)

static bool is_punct(token_t *token, int punct)
{
    if (token->type == TK_PUNCT && token->ival == punct)
        return true;
    return false;
}

static bool is_keyword(token_t *token, int keyword)
{
    if (token->type == TK_KEYWORD && token->ival == keyword)
        return true;
    return false;
}

static bool is_type(token_t *token)
{
    if (token->type != TK_KEYWORD)
        return false;
    switch (token->ival) {
    case KW_VOID:
    case KW_CHAR:
    case KW_INT:
    case KW_DOUBLE:
        return true;
    default:
        return false;
    }
}

/* node constructors */
#define NEW_NODE(node, tp) \
    do { \
        (node) = malloc(sizeof(*(node))); \
        (node)->type = tp; \
    } while (0)

static node_t *make_decl_var(ctype_t *ctype, char *varname)
{
    node_t *node;

    NEW_NODE(node, NODE_DECL);
    node->ctype = ctype;
    node->varname = varname;
}

static node_t *make_compound_stmt(vector_t *stmts)
{
    node_t *node;

    NEW_NODE(node, NODE_COMPOUND_STMT);
    node->stmts = stmts;
    return node;
}

static node_t *make_binary(ctype_t *ctype, int op, node_t *left, node_t *right)
{
    node_t *node;

    NEW_NODE(node, NODE_BINARY);
    node->ctype = ctype;
    node->left = left;
    node->right = right;
    return node;
}

/* parse functions */
static node_t *parse_decl(parser_t *parser);

static node_t *parse_assign_expr(parser)
{

}

static node_t *parse_expr(parser_t *parser)
{
    node_t *node;

    node = parse_assign_expr(parser);
    while (TRY_PUNCT(',')) {
        node_t *expr = parse_assign_expr(parser);
        node = make_binary(expr->ctype, ',', node, expr);
    }
    return node;
}

static node_t *parse_if_stmt(parser_t *parser)
{
}

static node_t *parse_for_stmt(parser_t *parser)
{
}

static node_t *parse_while_stmt(parser_t *parser)
{
}

static node_t *parse_compound_stmt(parser_t *parser);

static node_t *parse_stmt(parser_t *parser)
{
    token_t *token = NEXT();
    if (token->type == TK_KEYWORD || token->type == TK_PUNCT)
        switch (token->ival) {
        case '{':
            return parse_compound_stmt(parser);
        case KW_FOR:
            return parse_for_stmt(parser);
        case KW_WHILE:
            return parse_while_stmt(parser);
        case KW_IF:
            return parse_if_stmt(parser);
        default:
            errorf("unexpected keyword %d\n", token->ival);
        }
    if (TRY_PUNCT(';'))
        return NULL;
    return parse_expr(parser);
}

static node_t *parse_block_item(parser_t *parser)
{
    if (is_type(PEEK()))
        return parse_decl(parser);
    else
        return parse_stmt(parser);
}

static node_t *parse_compound_stmt(parser_t *parser)
{
    vector_t *stmts = make_vector();

    for (;;) {
        if (TRY_PUNCT('}'))
            break;
        vector_append(stmts, parse_block_item(parser));
    }
    return make_compound_stmt(stmts);
}

static ctype_t *parse_decl_spec(parser_t *parser)
{
    token_t *token = NEXT();
    if (token->type != TK_KEYWORD)
        errorf("expected type sepcifiers\n");
    switch (token->ival) {
    case KW_VOID:
        return ctype_void;
    case KW_CHAR:
        return ctype_char;
    case KW_INT:
        return ctype_int;
    default:
        errorf("expected type specifiers\n");
    }
}

static node_t *parse_declarator_func(parser_t *parser, ctype_t *ctype, char *func_name)
{
    node_t *func;

    EXPECT_PUNCT('(');
    for (;;) {
        if (TRY_PUNCT(')'))
            break;
        if (TRY_KW(KW_VOID) && TRY_PUNCT(')'))
            break;
    }
    func = calloc(1, sizeof(node_t));
    func->type = NODE_FUNC_DEF;
    func->ctype = ctype;
    func->func_name = func_name;
    return func;
}

static node_t *parse_declarator(parser_t *parser, ctype_t *ctype)
{
    token_t *token = NEXT();
    if (token->type != TK_ID)
        errorf("expected identifier\n");
    if (is_punct(PEEK(), '('))
        return parse_declarator_func(parser, ctype, token->sval);
    else
        return make_decl_var(ctype, token->sval);
}

static node_t *parse_decl(parser_t *parser)
{
    ctype_t *ctype;
    token_t *token;
    node_t *node;

    ctype = parse_decl_spec(parser);
    node = parse_declarator(parser, ctype);
    if (is_punct(PEEK(), '{')) {
        node->func_body = parse_compound_stmt(parser);
        if (!dict_insert(parser->env, node->func_name, node, true))
            errorf("redefinition of function \'%s\'", node->func_name);
    } else {
        EXPECT_PUNCT(';');
        if (!dict_insert(parser->env, node->varname, node, true))
            errorf("redefinition of variable \'%s\'", token->sval);
    }
    return node;
}

/*
static node_t *parse_func_def(parser_t *parser)
{
    ctype_t *func_type;
    node_t *func = malloc(sizeof(*func));
    dict_t *local_env = make_dict(parser->env);

    func->ctype = parse_decl_spec(parser);
    func->func_name = NEXT()->sval;
    func->param_types = NULL;
    EXPECT_PUNCT('{');
    func->func_body = parse_compound_stmt(parser);
    return func;
}
*/

node_t *get_node(parser_t *parser)
{
    return parse_decl(parser);
}

void parser_init(parser_t *parser, lexer_t *lexer)
{
    assert(parser && lexer);
    parser->lexer = lexer;
    parser->env = make_dict(NULL);
}


