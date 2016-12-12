#ifndef PARSER_H__
#define PARSER_H__

#include <stdbool.h>
#include "lexer.h"
#include "vector.h"
#include "dict.h"

enum {
    CTYPE_VOID,
    CTYPE_CHAR,
    CTYPE_INT,
    CTYPE_FLOAT,
    CTYPE_DOUBLE,
    CTYPE_PTR
};

typedef struct ctype_t {
    int type;
    int size;
    /* pointer to */
    struct ctype_t *ptr;
    /* function */
    struct ctype_t *ret;
    /* function parameter types */
    vector_t *param_types;
    /* variable argument lists */
    bool is_va;
} ctype_t;

enum {
    NODE_CONSTANT,
    NODE_STRING,
    NODE_POSTFIX, /* ++ -- */
    NODE_UNARY,
    NODE_BINARY,
    NODE_TERNARY, /* ? : */
    NODE_IF,
    NODE_FOR,
    NODE_DO_WHILE,
    NODE_WHILE,
    NODE_FUNC_DECL,
    NODE_FUNC_DEF,
    NODE_FUNC_CALL,
    NODE_VAR_DECL,
    NODE_VAR_INIT,
    NODE_VAR,
    NODE_COMPOUND_STMT,
    NODE_RETURN,
    NODE_CAST,
    NODE_ARITH_CONV
};

typedef struct node_t {
    int type;
    ctype_t *ctype;
    union {
        /* int, char */
        long ival;
        /* float/double */
        struct {
            double fval;
            char *flabel;
        };
        /* string */
        struct {
            char *sval;
            char *slabel;
        };
        /* variable */
        struct {
            char *varname;
            union {
                /* local */
                int loffset;
                /* global */
                char *glabel;
            };
        };
        /* unary or postfix++ -- */
        struct {
            int unary_op;
            struct node_t *operand;
        };
        /* binary operator */
        struct {
            int binary_op;
            struct node_t *left;
            struct node_t *right;
        };
        /* function */
        struct {
            char *func_name;
            /* save parameters for env when function declaration
             * save args when functions call */
            vector_t *params;
            struct node_t *func_body;
        };
        /* if or ternary ? : */
        struct {
            struct node_t *cond;
            struct node_t *then;
            struct node_t *els;
        };
        /* for */
        struct {
            struct node_t *for_init;
            struct node_t *for_cond;
            struct node_t *for_step;
            struct node_t *for_body;
        };
        /* while/do while */
        struct {
            struct node_t *while_cond;
            struct node_t *while_body;
        };
        /* compound statements */
        vector_t *stmts;
        /* return/cast/conv */
        struct node_t *expr;
    };
} node_t;

typedef struct parser_t {
    lexer_t *lexer;
    /* current env */
    dict_t *env;
    /* current func return type for parse_return_stmt */
    ctype_t *ret;
} parser_t;

extern ctype_t *ctype_void;
extern ctype_t *ctype_char;
extern ctype_t *ctype_int;
extern ctype_t *ctype_float;
extern ctype_t *ctype_double;

void parser_init(parser_t *parser, lexer_t *lexer);
node_t *get_node(parser_t *parser);

#endif
