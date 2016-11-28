#ifndef PARSER_H__
#define PARSER_H__

#include "lexer.h"
#include "vector.h"
#include "dict.h"

enum {
    CTYPE_VOID,
    CTYPE_CHAR,
    CTYPE_INT,
    CTYPE_PTR
};

typedef struct ctype_t {
    int type;
    int size;
} ctype_t;

extern ctype_t *ctype_void;
extern ctype_t *ctype_char;
extern ctype_t *ctype_int;

enum {
    NODE_UNARY,
    NODE_BINARY,
    NODE_IF,
    NODE_FOR,
    NODE_WHILE,
    NODE_FUNC_DEF,
    NODE_FUNC_CALL,
    NODE_DECL,
    NODE_INIT,
    NODE_COMPOUND_STMT
};

typedef struct node_t {
    int type;
    ctype_t *ctype;
    union {
        /* int */
        int ival;
        /* double */
        double dval;
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
        /* unary operator */
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
            /* function declaration */
            vector_t *param_types;
            vector_t *local_vars;
            struct node_t *func_body;
            /* function call */
            vector_t *params;
        };
        /* if */
        struct {
            struct node_t *if_cond;
            struct node_t *if_then;
            struct node_t *if_else;
        };
        /* for */
        struct {
            struct node_t *for_init;
            struct node_t *for_cond;
            struct node_t *for_step;
            struct node_t *for_body;
        };
        /* while */
        struct {
            struct node_t *while_cond;
            struct node_t *while_body;
        };
        /* compound statements */
        vector_t *stmts;
        /* return */
        struct node_t ret;
    }
} node_t;

typedef struct parser_t {
    lexer_t *lexer;
    /* current env */
    dict_t *env;
} parser_t;

void parser_init(parser_t *parser, lexer_t *lexer);
node_t *get_node(parser_t *parser);

#endif
