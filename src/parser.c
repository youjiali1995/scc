#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include "parser.h"
#include "util.h"

ctype_t *ctype_void = &(ctype_t){CTYPE_VOID, 0};
ctype_t *ctype_char = &(ctype_t){CTYPE_CHAR, 1};
ctype_t *ctype_int = &(ctype_t){CTYPE_INT, 4};

/* I'm lazy */
#define _FILE_ parser->lexer->fname
#define _LINE_ parser->lexer->line

/* i/o functions */
#define NEXT() (get_token(parser->lexer))
#define PEEK() (peek_token(parser->lexer))
#define UNGET(token) (unget_token(token, parser->lexer))
#define EXPECT_PUNCT(punct) \
    do { \
        token_t *token = NEXT(); \
        if (token->type != TK_PUNCT || token->ival != (punct)) \
            errorf("expected punctator %c\n in %s:%d\n", punct, _FILE_, _LINE_); \
    } while (0)
#define TRY_PUNCT(punct) \
    (is_punct(PEEK(), punct) ? (NEXT(), true) : false)
#define EXPECT_KW(keyword) \
    do { \
        token_t *token = NEXT(); \
        if (token->type != TK_KEYWORD || token->ival != keyword) \
            errorf("expected keyword %c in %s:%d\n", keyword, _FILE_, _LINE_); \
    } while (0)
#define TRY_KW(keyword) \
    (is_keyword(PEEK(), keyword) ? (NEXT(), true) : false)

/* type check functions */
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

static bool is_assign_op(token_t *token)
{
    if (token->type != TK_PUNCT)
        return false;
    switch (token->ival) {
    case '=':
    case PUNCT_IMUL:
    case PUNCT_IDIV:
    case PUNCT_IMOD:
    case PUNCT_IADD:
    case PUNCT_ISUB:
    case PUNCT_ILSFT:
    case PUNCT_IRSFT:
    case PUNCT_IAND:
    case PUNCT_IXOR:
    case PUNCT_IOR:
        return true;

    default:
        return false;
    }
    return false;
}

static bool is_same_type(ctype_t *t, ctype_t *p)
{
    if (t == p)
        return true;
    if (t->type == CTYPE_PTR && p->type == CTYPE_PTR
        /* void * == any other type of pointer */
        && (t->ptr == ctype_void || p->ptr == ctype_void || is_same_type(t->ptr, p->ptr)))
        return true;
    return false;
}

static bool is_arith_type(ctype_t *type)
{
    if (type == ctype_int)
        return true;
    return false;
}

static bool is_lvalue(node_t *node)
{
    if (node->type == NODE_VAR_DECL)
        return true;
    return false;
}

static bool is_ptr(ctype_t *ctype)
{
    if (ctype->type == CTYPE_PTR)
        return true;
    return false;
}

static bool is_null(node_t *node)
{
    if (node->type == NODE_CONSTANT && node->ctype == ctype_int && node->ival == 0)
        return true;
    return false;
}

/* error message helper functions */
static char *type2str(ctype_t *t)
{
    static char *s[] = {
        "void",
        "character",
        "integer",
        "pointer"
    };

    assert(t && t->type >= 0 && t->type < 4);
    return s[t->type];
}

static char *punct2str(int punct)
{
    static char *s[] = {
        "++", "+=", "&&", "&=", "||", "|=", "--", "-=", "->",
        "*=", "/=", "%=", "==", "!=", "^=", "<=", "<<", "<<=",
        ">=", ">>", ">>="
    };

    assert(punct >= PUNCT_INC && punct <= PUNCT_IRSFT);
    return s[punct - PUNCT_INC];
}

/************************** type constructors *************************/
#define NEW_TYPE(ctype, tp, sz) \
    do { \
        (ctype) = calloc(1, sizeof(*ctype)); \
        (ctype)->type = (tp); \
        (ctype)->size = (sz); \
    } while (0)

static ctype_t *make_ptr(ctype_t *p)
{
    ctype_t *ctype;

    NEW_TYPE(ctype, CTYPE_PTR, 8);
    ctype->ptr = p;
    return ctype;
}

/*************************** node constructors **********************************/
#define NEW_NODE(node, tp) \
    do { \
        (node) = malloc(sizeof(*(node))); \
        (node)->type = tp; \
    } while (0)

static node_t *make_var_decl(ctype_t *ctype, char *varname)
{
    node_t *node;

    NEW_NODE(node, NODE_VAR_DECL);
    node->ctype = ctype;
    node->varname = varname;
    return node;
}

static node_t *make_var_init(node_t *var, node_t *init)
{
    node_t *node;

    NEW_NODE(node, NODE_VAR_INIT);
    node->binary_op = '+';
    node->left = var;
    node->right = init;
    return node;
}

static node_t *make_compound_stmt(vector_t *stmts)
{
    node_t *node;

    NEW_NODE(node, NODE_COMPOUND_STMT);
    node->stmts = stmts;
    return node;
}

static node_t *make_unary(ctype_t *ctype, int op, node_t *operand)
{
    node_t *node;

    NEW_NODE(node, NODE_UNARY);
    node->ctype = ctype;
    node->unary_op = op;
    node->operand = operand;
    return node;
}

static node_t *make_binary(ctype_t *ctype, int op, node_t *left, node_t *right)
{
    node_t *node;

    NEW_NODE(node, NODE_BINARY);
    node->ctype = ctype;
    node->binary_op = op;
    node->left = left;
    node->right = right;
    return node;
}

static node_t *make_ternary(ctype_t *ctype, node_t *cond, node_t *then, node_t *els)
{
    node_t *node;

    NEW_NODE(node, NODE_TERNARY);
    node->ctype = ctype;
    node->cond = cond;
    node->then = then;
    node->els = els;
    return node;
}

static node_t *make_number(char *s)
{
    node_t *node;

    /* TODO */
    NEW_NODE(node, NODE_CONSTANT);
    node->ctype = ctype_int;
    node->ival = atoi(s);
    return node;
}

static node_t *make_string(char *s)
{
    node_t *node;

    NEW_NODE(node, NODE_STRING);
    node->ctype = ctype_char;
    node->sval = s;
    return node;
}

static node_t *make_if(node_t *cond, node_t *then, node_t *els)
{
    node_t *node;

    NEW_NODE(node, NODE_IF);
    node->cond = cond;
    node->then = then;
    node->els = els;
    return node;
}

static node_t *make_for(node_t *init, node_t *cond, node_t *step, node_t *body)
{
    node_t *node;

    NEW_NODE(node, NODE_FOR);
    node->for_init = init;
    node->for_cond = cond;
    node->for_step = step;
    node->for_body = body;
    return node;
}

static node_t *make_while(node_t *cond, node_t *body)
{
    node_t *node;

    NEW_NODE(node, NODE_WHILE);
    node->while_cond = cond;
    node->while_body = body;
    return node;
}

static node_t *make_return(ctype_t *ctype, node_t *ret)
{
    node_t *node;

    NEW_NODE(node, NODE_RETURN);
    node->ctype = ctype;
    node->ret = ret;
    return node;
}

/* parse functions */

static node_t *parse_expr(parser_t *parser);
static node_t *parse_cast_expr(parser_t *parser);
static node_t *parse_assign_expr(parser_t *parser);

/************************************* Expressions ****************************************/

/* primary-expression:
 *      identifier
 *      constant
 *      string-literal
 *      ( expression )
 */
static node_t *parse_primary_expr(parser_t *parser)
{
    token_t *token;
    node_t *primary;

    if (TRY_PUNCT('(')) {
        primary = parse_expr(parser);
        if (!primary)
            errorf("TODO\n");
        EXPECT_PUNCT(')');
        return primary;
    }

    token = NEXT();
    switch (token->type) {
    case TK_ID:
        primary = dict_lookup(parser->env, token->sval);
        if (!primary)
            errorf("TODO\n");
        return primary;

    case TK_NUMBER:
        /* TODO */
        return make_number(token->sval);

    case TK_CHAR:
        /* TODO */
        break;

    case TK_STRING:
        /* TODO */
        return make_string(token->sval);
    }
    return NULL;
}

/* postfix-expression:
 *      primary-expression
 *      postfix-expression [ expression ]
 *      postfix-expression ( argument-expression-list-opt )
 *      postfix-expression . identifier
 *      postfix-expression -> identifier
 *      postfix-expression +=
 *      postfix-expression --
 *      ( type-name ) { initializer-list }
 *      ( type-name ) { initializer-list , }
 */
static node_t *parse_postfix_expr(parser_t *parser)
{
    node_t *post;
    token_t *token;

    if (TRY_PUNCT('(')) {
        /* TODO: compound literal */
    }
    post = parse_primary_expr(parser);
    token = NEXT();
    if (token->type == TK_PUNCT)
        switch (token->ival) {
        case '[':
            /* TODO */
            break;

        case '(': {
            /* TODO */
            vector_t *params = make_vector();
            node_t *param = parse_assign_expr(parser);
            vector_append(params, param);
            EXPECT_PUNCT(')');
            post->type = NODE_FUNC_CALL;
            post->params = params;
            return post;
        }
        case '.':
            /* TODO */
            break;

        case PUNCT_ARROW:
            /* TODO */
            break;

        case PUNCT_INC:
        case PUNCT_DEC:
            /* TODO */
            return make_unary(ctype_int, token->ival, post);
        }
    UNGET(token);
    return post;
}

/* unary-expression:
 *      postfix-expression
 *      ++ unary-expression
 *      -- unary-expression
 *      unary-operator cast-expression
 *      sizeof unary-expression
 *      sizeof ( type-name )
 *
 * unary-operator: one of
 *      & * + - ~ !
 */
static node_t *parse_unary_expr(parser_t *parser)
{
    token_t *token = NEXT();

    if (is_punct(token, PUNCT_INC) || is_punct(token, PUNCT_DEC)) {
        node_t *unary = parse_unary_expr(parser);
        if (!unary)
            errorf("TODO\n");
        return make_unary(ctype_int, token->ival, unary);
    }

    if (is_punct(token, '&') || is_punct(token, '*') || is_punct(token, '+')
            || is_punct(token, '-') || is_punct(token, '~') || is_punct(token, '!')) {
        node_t *cast = parse_cast_expr(parser);
        if (!cast)
            errorf("TODO\n");
        return make_unary(ctype_int, token->ival, cast);
    }

    /* TODO: sizeof */
    UNGET(token);
    return parse_postfix_expr(parser);
}

/* cast-expression:
 *      unary-expression
 *      ( type-name ) cast-expression
 */
static node_t *parse_cast_expr(parser_t *parser)
{
    if (TRY_PUNCT('(')) {
        node_t *cast;
        /* TODO */
        NEXT();
        EXPECT_PUNCT(')');
        cast = parse_cast_expr(parser);
        if (!cast)
            errorf("TODO\n");
        return cast;
    }
    return parse_unary_expr(parser);
}

/* multiplicative-expression:
 *      cast-expression
 *      multiplicative * cast-expression
 *      multiplicative / cast-expression
 *      multiplicative % cast-expression
 */
static node_t *parse_multiplicative_expr(parser_t *parser)
{
    node_t *mul = parse_cast_expr(parser);
    token_t *token;

    for (token = NEXT(); is_punct(token, '*') || is_punct(token, '/') || is_punct(token, '%'); token = NEXT()) {
        node_t *cast = parse_cast_expr(parser);
        if (!cast)
            errorf("TODO\n");
        mul = make_binary(ctype_int, token->ival, mul, cast);
    }
    UNGET(token);
    return mul;
}

/* additive-expression:
 *      multiplicative-expression
 *      additive-expression + multiplicative-expression
 *      additive-expression - multipilicative-expression
 */
static node_t *parse_additive_expr(parser_t *parser)
{
    node_t *add = parse_multiplicative_expr(parser);
    token_t *token;

    for (token = NEXT(); is_punct(token, '+') || is_punct(token, '-'); token = NEXT()) {
        node_t *mul = parse_multiplicative_expr(parser);
        /* TODO: type check and more concise error message: pointer type */
        /* pointer +- integer */
        if (is_ptr(add->ctype) && mul->ctype == ctype_int)
            add = make_binary(add->ctype, token->ival, add, mul);
        /* integer + pointer */
        else if (is_punct(token, '+') && add->ctype == ctype_int && is_ptr(mul->ctype))
            add = make_binary(mul->ctype, '+', add, mul);
        /* pointer - pointer */
        else if (is_punct(token, '-') && is_ptr(add->ctype) && is_same_type(add->ctype, mul->ctype))
            add = make_binary(ctype_int, '-', add, mul);
        /* number +- number */
        else if (is_arith_type(add->ctype) && is_arith_type(mul->ctype))
            add = make_binary(add->ctype, token->ival, add, mul);
        else
            errorf("invalid operands to binary %c (have \'%s\' and \'%s\') in %s:%d\n",
                    token->ival, type2str(add->ctype), type2str(mul->ctype), _FILE_, _LINE_);
        add = make_binary(ctype_int, token->ival, add, mul);
    }
    UNGET(token);
    return add;
}

/* shift-expression:
 *      additive-expression
 *      shift-expression << additive-expression
 *      shift-expression >> additive-expression
 */
static node_t *parse_shift_expr(parser_t *parser)
{
    node_t *shift = parse_additive_expr(parser);
    token_t *token;

    for (token = NEXT(); is_punct(token, PUNCT_LSFT) || is_punct(token, PUNCT_RSFT); token = NEXT()) {
        node_t *add = parse_additive_expr(parser);
        if (shift->ctype != ctype_int || add->ctype != ctype_int)
            errorf("invalid operands to binary %s (have \'%s\' and \'%s\') in %s:%d\n",
                    punct2str(token->ival), type2str(shift->ctype), type2str(add->ctype));
        shift = make_binary(ctype_int, token->ival, shift, add);
    }
    UNGET(token);
    return shift;
}

/* relational_expression:
 *      shift-expression
 *      relational-expression <  shift-expression
 *      relational-expression >  shift-expression
 *      relational-expression <= shift-expression
 *      relational-expression >= shift-expression
 */
static node_t *parse_relational_expr(parser_t *parser)
{
    node_t *rel = parse_shift_expr(parser);
    token_t *token;

    for (token = NEXT();
        is_punct(token, '<') || is_punct(token, '>') || is_punct(token, PUNCT_LE) || is_punct(token, PUNCT_GE);
        token = NEXT()) {
        node_t *shift = parse_shift_expr(parser);
        /* TODO: refactory */
        if (is_ptr(rel->ctype) && is_ptr(shift->ctype)) {
            if (!is_same_type(rel->ctype, shift->ctype))
                errorf("comparison of distinct pointer types lacks a cast in %s:%d\n", _FILE_, _LINE_);
        } else if (!((is_arith_type(rel->ctype) && is_arith_type(shift->ctype))
                    || (is_ptr(rel->ctype) && is_null(shift))
                    || (is_ptr(shift->ctype) && is_null(rel))))
            errorf("comparison between %s and %s in %s:%d\n",
                    type2str(rel->ctype), type2str(shift->ctype), _FILE_, _LINE_);
        make_binary(ctype_int, token->ival, rel, shift);
    }
    UNGET(token);
    return rel;
}

/* equality-expression:
 *      relational-expression
 *      equality-expression == relational-expression
 *      equality-expression != relational-expression
 */
static node_t *parse_equality_expr(parser_t *parser)
{
    node_t *eq = parse_relational_expr(parser);
    token_t *token;

    for (token = NEXT(); is_punct(token, PUNCT_EQ) || is_punct(token, PUNCT_NE); token = NEXT()) {
        node_t *rel = parse_relational_expr(parser);
        /* both operands are pointers to qualified or unqualified versions of compatible types */
        if (is_ptr(eq->ctype) && is_ptr(rel->ctype)) {
            if (!is_same_type(eq->ctype, rel->ctype))
                errorf("comparison of distinct pointer types lacks a cast in %s:%d\n", _FILE_, _LINE_);
                    /* both operands have arithmetic type */
        } else if (!((is_arith_type(eq->ctype) && is_arith_type(rel->ctype))
                    /* one operand is a pointer and the other is a null pointer constant */
                    || (is_ptr(eq->ctype) && is_null(rel))
                    || (is_ptr(rel->ctype) && is_null(eq))))
            errorf("comparison between %s and %s in %s:%d\n",
                    type2str(eq->ctype), type2str(rel->ctype), _FILE_, _LINE_);
        eq = make_binary(ctype_int, token->ival, eq, rel);
    }
    UNGET(token);
    return eq;
}

/* AND-expression:
 *      equality-expression
 *      AND-expression & equality-expression
 */
static node_t *parse_bit_and_expr(parser_t *parser)
{
    node_t *bitand = parse_equality_expr(parser);
    while (TRY_PUNCT('&')) {
        node_t *eq = parse_equality_expr(parser);
        if (bitand->ctype != ctype_int || eq->ctype != ctype_int)
            errorf("invalid operands to binary '&' (have \'%s\' and \'%s\') in %s:%d\n",
                    type2str(bitand->ctype), type2str(eq->ctype), _FILE_, _LINE_);
        bitand = make_binary(ctype_int, '&', bitand, eq);
    }
    return bitand;
}

/* exclusive-OR-expression:
 *      AND-expression
 *      exclusive-OR-expression ^ AND-expression
 */
static node_t *parse_bit_xor_expr(parser_t *parser)
{
    node_t *bitxor = parse_bit_and_expr(parser);
    while (TRY_PUNCT('^')) {
        node_t *bitand = parse_bit_and_expr(parser);
        if (bitxor->ctype != ctype_int || bitand->ctype != ctype_int)
            errorf("invalid operands to binary '^' (have \'%s\' and \'%s\') in %s:%d\n",
                    type2str(bitxor->ctype), type2str(bitand->ctype), _FILE_, _LINE_);
        bitxor = make_binary(ctype_int, '^', bitxor, bitand);
    }
    return bitxor;
}

/* inclusive-OR-expression:
 *      exclusive-OR-expression
 *      inclusive-OR-expression | exclusive-OR-expression
 */
static node_t *parse_bit_or_expr(parser_t *parser)
{
    node_t *bitor = parse_bit_xor_expr(parser);
    while (TRY_PUNCT('|')) {
        node_t *bitxor = parse_bit_xor_expr(parser);
        if (bitor->ctype != ctype_int || bitxor->ctype != ctype_int)
            errorf("invalid operands to binary '|' (have \'%s\' and \'%s\') in %s:%d\n",
                    type2str(bitor->ctype), type2str(bitxor->ctype), _FILE_, _LINE_);
        bitor = make_binary(ctype_int, '|', bitor, bitxor);
    }
    return bitor;
}

/* logical-AND-expression:
 *      inclusive-OR-expression
 *      logical-AND-expression && inclusive-OR-expression
 */
static node_t *parse_log_and_expr(parser_t *parser)
{
    node_t *logand = parse_bit_or_expr(parser);
    while (TRY_PUNCT(PUNCT_AND)) {
        node_t *bitor = parse_bit_or_expr(parser);
        /* TODO: type check scalar check */
        logand = make_binary(ctype_int, PUNCT_AND, logand, bitor);
    }
    return logand;
}

/* logical-OR-expression:
 *      logical-AND-expression
 *      logical-OR-expression || logical-AND-expression
 */
static node_t *parse_log_or_expr(parser_t *parser)
{
    node_t *logor = parse_log_and_expr(parser);
    while (TRY_PUNCT(PUNCT_OR)) {
        node_t *logand = parse_log_and_expr(parser);
        /* TODO: type check scalar type */
        logor = make_binary(ctype_int, PUNCT_OR, logor, logand);
    }
    return logor;
}

/* conditional-expression:
 *      logical-OR-expression
 *      logical-OR-expression ? expression : conditional-expression
 */
static node_t *parse_cond_expr(parser_t *parser)
{
    node_t *cond = parse_log_or_expr(parser);
    if (TRY_PUNCT('?')) {
        node_t *then = parse_expr(parser);
        EXPECT_PUNCT(':');
        node_t *els = parse_cond_expr(parser);
        /* TODO: type */
        if (!is_same_type(then->ctype, els->ctype))
            errorf("type mismatch in conditional expression in %s:%d\n", _FILE_, _LINE_);
        return make_ternary(then->ctype, cond, then, els);
    }
    return cond;
}
/* assignment-expression:
 *      conditional-expression
 *      unary-expression assignment-operator assignment-expression
 */
static node_t *parse_assign_expr(parser_t *parser)
{
    node_t *node;
    node_t *assign;
    token_t *token;

    node = parse_cond_expr(parser);
    if (node->type == NODE_BINARY || node->type == NODE_TERNARY)
        return node;
    token = NEXT();
    if (!is_assign_op(token)) {
        UNGET(token);
        return node;
    }
    if (!is_lvalue(node))
        errorf("lvalue required as left operand of assignment in %s:%d\n", _FILE_, _LINE_);
    assign = parse_assign_expr(parser);
    if (token->ival == '=') {
        if (!is_same_type(node->ctype, assign->ctype))
            errorf("assignment make %s from %s without a cast in %s:%d\n",
                    type2str(node->ctype), type2str(assign->ctype), _FILE_, _LINE_);
    } else if (node->ctype != ctype_int || assign->ctype != ctype_int)
        errorf("invalid operands to binary %s (have \'%s\' and \'%s\') in %s:%d\n",
                punct2str(token->ival), type2str(node->ctype), type2str(assign->ctype), _FILE_, _LINE_);
    return make_binary(node->ctype, token->ival, node, assign);
}

/* expression:
 *      assignment-expression
 *      expression , assignment-expression
 */
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

/* Delcarations */
static node_t *parse_declarator(parser_t *parser, ctype_t *ctype);

/* declaration-specifiers:
 *      storage-class-specifier declaration-sepcifiers-opt
 *      type-spcifier declaration-specifiers-opt
 *      type-qualifier declaration-sepcifiers-opt
 *      function-specifier declaration-specifiers-opt
 */
static ctype_t *parse_decl_spec(parser_t *parser)
{
    /* TODO */
    token_t *token = NEXT();
    if (token->type != TK_KEYWORD)
        errorf("expected type sepcifiers in %s:%d\n", _FILE_, _LINE_);
    switch (token->ival) {
    case KW_VOID:
        return ctype_void;
    case KW_CHAR:
        return ctype_char;
    case KW_INT:
        return ctype_int;
    default:
        errorf("expected type specifiers in %s:%d\n", _FILE_, _LINE_);
    }
    return NULL;
}

/* parameter-list:
 *      parameter-declaration
 *      parameter-list , parameter-declaration
 */
static vector_t *parse_param_list(parser_t *parser)
{
    vector_t *params;
    token_t *token;

    token = NEXT();
    if (is_keyword(token, KW_VOID) && is_punct(PEEK(), ')'))
        return NULL;
    UNGET(token);
    params = make_vector();
    do {
        ctype_t *ctype = parse_decl_spec(parser);
        node_t *node = parse_declarator(parser, ctype);
        vector_append(params, node);
    } while (TRY_PUNCT(','));
    return params;
}

/* direct-declarator:
 *      identifier
 *      ( declarator )
 *      direct-declarator [ type-qualifier-list-opt assignment-expression-opt ]
 *      direct-declarator [ static type-qualifier-list-opt assign-expression ]
 *      direct-declarator [ type-qualifier-list staic assignment-expression ]
 *      direct-declarator [ type-qualifier-list-opt * ]
 *      direct-declarator ( parameter-type-list )
 *      direct-declarator ( identifier-list-opt)
 */
/* TODO */
static node_t *parse_direct_decl(parser_t *parser, ctype_t *ctype)
{
    node_t *decl;
    token_t *token;

    if (TRY_PUNCT('(')) {
        decl = parse_declarator(parser, ctype);
        EXPECT_PUNCT(')');
    } else if ((token = NEXT())->type != TK_ID)
        errorf("expected identifier in %s:%d\n", _FILE_, _LINE_);
    if (TRY_PUNCT('(')) {
        size_t i;
        decl = calloc(1, sizeof(*decl));
        decl->type = NODE_FUNC_DECL;
        decl->func_name = token->sval;
        decl->params = parse_param_list(parser);
        decl->ctype = make_ptr(NULL);
        decl->ctype->ret = ctype;
        decl->ctype->params = make_vector();
        for (i = 0; i < vector_len(decl->params); i++)
            vector_append(decl->ctype->params, ((node_t *) vector_get(decl->params, i))->ctype);
        EXPECT_PUNCT(')');
    } else {
        decl = make_var_decl(ctype, token->sval);
    }
    return decl;
}

/* declarator:
 *      pointer-opt direct-declarator
 */
static node_t *parse_declarator(parser_t *parser, ctype_t *ctype)
{
    int n;
    node_t *node;

    for (n = 0; TRY_PUNCT('*'); n++)
        ;
    node = parse_direct_decl(parser, ctype);
    ctype = node->type == NODE_VAR_DECL ? node->ctype : node->ctype->ret;
    while (n-- > 0)
        ctype = make_ptr(ctype);
    if (node->type == NODE_VAR_DECL) {
        if (ctype == ctype_void)
            errorf("variable \'%s\' declared void in %s:%d\n", node->varname, _FILE_, _LINE_);
        node->ctype = ctype;
    } else
        node->ctype->ret = ctype;
    return node;
}

/* initializer:
 *      assignment-expression
 *      { initializer-list }
 *      { initializer-list , }
 */
static node_t *parse_initializer(parser_t *parser)
{
    /* TODO: array, structure */
    return parse_assign_expr(parser);
}

/* init-declarator:
 *      declarator
 *      declarator = initializer
 */
static node_t *parse_init_decl(parser_t *parser, ctype_t *ctype)
{
    node_t *decl = parse_declarator(parser, ctype);
    dict_insert(parser->env, decl->varname, decl, true);
    if (TRY_PUNCT('=')) {
        node_t *init = parse_initializer(parser);
        if (!is_same_type(init->ctype, decl->ctype))
            errorf("initialization makes %s from %s without a cast in %s:%d\n",
                    type2str(decl->ctype), type2str(init->ctype), _FILE_, _LINE_);
        return make_var_init(decl, init);
    }
    return decl;
}

/* init-declarator-list:
 *      init-declarator
 *      init-declarator-list , init-declarator
 */
static node_t *parse_init_decl_list(parser_t *parser, ctype_t *ctype)
{
    node_t *list = parse_init_decl(parser, ctype);
    while (TRY_PUNCT(',')) {
        node_t *declarator = parse_init_decl(parser, ctype);
        list = make_binary(NULL, ',', list, declarator);
    }
    return list;
}

/* declaration:
 *      declaration-specifiers init-declarator-list-opt ;
 */
static node_t *parse_decl(parser_t *parser)
{
    ctype_t *ctype;
    node_t *node;

    ctype = parse_decl_spec(parser);
    node = parse_init_decl_list(parser, ctype);
    EXPECT_PUNCT(';');
    return node;
}

/********************************* Statements ****************************************/
static node_t *parse_stmt(parser_t *parser);

/* selection-statement:
 *      if ( expression ) statement
 *      if ( expression ) statement else statement
 */
static node_t *parse_if_stmt(parser_t *parser)
{
    node_t *cond;
    node_t *then;
    node_t *els = NULL;

    EXPECT_PUNCT('(');
    if (TRY_PUNCT(')'))
        errorf("expected expression before \')\' token in %s:%d\n", _FILE_, _LINE_);
    cond = parse_expr(parser);
    EXPECT_PUNCT(')');
    then = parse_stmt(parser);
    if (TRY_KW(KW_ELSE))
        els = parse_stmt(parser);
    return make_if(cond, then, els);
}

/* iteration-statment:
 *      for ( expression-opt ; expression-opt ; expression-opt ) statement
 */
static node_t *parse_for_stmt(parser_t *parser)
{
    node_t *init, *cond, *step, *body;

    EXPECT_PUNCT('(');
    if (TRY_PUNCT(';'))
        init = NULL;
    else {
        init = parse_expr(parser);
        EXPECT_PUNCT(';');
    }
    if (TRY_PUNCT(';'))
        cond = NULL;
    else {
        cond = parse_expr(parser);
        EXPECT_PUNCT(';');
    }
    if (TRY_PUNCT(')'))
        step = NULL;
    else {
        step = parse_expr(parser);
        EXPECT_PUNCT(')');
    }
    body = parse_stmt(parser);
    return make_for(init, cond, step, body);
}

/* iteration-statement:
 *      while ( expression ) statement
 */
static node_t *parse_while_stmt(parser_t *parser)
{
    node_t *cond;
    node_t *body;

    EXPECT_PUNCT('(');
    cond = parse_expr(parser);
    EXPECT_PUNCT(')');
    body = parse_stmt(parser);
    return make_while(cond, body);
}

/* jump-statement:
 *      return expression-opt ;
 */
static node_t *parse_return_stmt(parser_t *parser)
{
    node_t *expr;

    if (TRY_PUNCT(';')) {
        if (parser->ret != ctype_void)
            errorf("\'return\' with no value, in function returning non-void in %s:%d\n", _FILE_, _LINE_);
        return make_return(ctype_void, NULL);
    }
    expr = parse_expr(parser);
    if (!is_same_type(expr->ctype, parser->ret))
        errorf("return makes %s from %s without a cast in %s:%d\n",
                type2str(parser->ret), type2str(expr->ctype), _FILE_, _LINE_);
    EXPECT_PUNCT(';');
    return make_return(expr->ctype, expr);
}

/* block-item:
 *      declaration
 *      statement
 */
static node_t *parse_block_item(parser_t *parser)
{
    if (is_type(PEEK()))
        return parse_decl(parser);
    else
        return parse_stmt(parser);
}

/* compound-statement:
 *      { block-item-list-opt }
 */
static node_t *parse_compound_stmt(parser_t *parser)
{
    vector_t *stmts = make_vector();

    for (;;) {
        if (TRY_PUNCT('}'))
            break;
        vector_append(stmts, parse_block_item(parser));
    }
    if (!vector_len(stmts)) {
        free_vector(stmts, NULL);
        stmts = NULL;
    }
    return make_compound_stmt(stmts);
}

/* statement:
 *      labeled-statement
 *      compound-statement
 *      expression-statement
 *      selection-statement
 *      iteration-statement
 *      jump-statement
 */
static node_t *parse_stmt(parser_t *parser)
{
    node_t *stmt;
    token_t *token = NEXT();

    if (token->type == TK_KEYWORD || token->type == TK_PUNCT)
        switch (token->ival) {
        case '{': {
            dict_t *env = parser->env;
            parser->env = make_dict(env);
            stmt = parse_compound_stmt(parser);
            parser->env = env;
            return stmt;
        }
        case KW_FOR:
            return parse_for_stmt(parser);
        case KW_WHILE:
            return parse_while_stmt(parser);
        case KW_IF:
            return parse_if_stmt(parser);
        case KW_RETURN:
            return parse_return_stmt(parser);
        case ';':
            return NULL;

        default:
            errorf("unexpected keyword %d\n", token->ival);
        }
    UNGET(token);
    stmt = parse_expr(parser);
    EXPECT_PUNCT(';');
    return stmt;
}

/********************* External definitions ***********************/

/* function-definition:
 *      declaration-specifiers declarator declaration-list-opt compound-statement
 */
static node_t *parse_func_def(parser_t *parser)
{
    dict_t *env;
    node_t *func;
    ctype_t *ctype;
    size_t i;

    ctype = parse_decl_spec(parser);
    func = parse_declarator(parser, ctype);
    func->type = NODE_FUNC_DEF;
    env = parser->env;
    parser->env = make_dict(env);
    parser->ret = func->ctype->ret;
    for (i = 0; i < vector_len(func->params); i++) {
        node_t *param = vector_get(func->params, i);
        /* TODO: pointer to func as param */
        dict_insert(parser->env, param->varname, param, true);
    }
    EXPECT_PUNCT('{');
    func->func_body = parse_compound_stmt(parser);
    parser->env = env;
    parser->ret = NULL;
    dict_insert(parser->env, func->func_name, func, true);
    return func;
}

node_t *get_node(parser_t *parser)
{
    if (!PEEK())
        return NULL;
    return parse_func_def(parser);
}

static void builtin_init(dict_t *env)
{
    node_t *func_puts;

    NEW_NODE(func_puts, NODE_FUNC_DEF);
    func_puts->ctype = make_ptr(NULL);
    func_puts->ctype->ret = ctype_int;
    func_puts->func_name = "puts";
    func_puts->params = make_vector();
    vector_append(func_puts->params, ctype_char);
    func_puts->func_body = NULL;

    dict_insert(env, "puts", func_puts, true);
}

void parser_init(parser_t *parser, lexer_t *lexer)
{
    assert(parser && lexer);
    parser->lexer = lexer;
    parser->env = make_dict(NULL);
    parser->ret = NULL;

    builtin_init(parser->env);
}
