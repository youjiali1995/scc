#include <assert.h>
#include "gen.h"

static char *arg_regs[] = {"di", "si", "dx", "cx", "r8", "r9", NULL};
static char *caller_saves[] = {"%%r10", "%%r11", NULL};
static char *callee_saves[] = {"%%rbx", "%%r12", "%%r13", "%%r14", "%%r15", NULL};

#define EMIT(fmt, ...) \
    do { \
        fprintf(fp, "\t", fmt, ##__VA_ARGS__); \
        fprintf(fp, "\n"); \
    } while (0)

#define EMIT_LABEL(fmt, ...) \
    do { \
        fprintf(fp, fmt, ##__VA_ARGS__); \
        fprintf(fp, ":\n"); \
    } while (0)

void emit_constant(FILE *fp, node_t *node)
{
}

void emit_string(FILE *fp, node_t *node)
{
}

void emit_postfix(FILE *fp, node_t *node)
{
}

void emit_unary(FILE *fp, node_t *node)
{
}

void emit_binary(FILE *fp, node_t *node)
{
}

void emit_ternary(FILE *fp, node_t *node)
{
}

void emit_if(FILE *fp, node_t *node)
{
}

void emit_for(FILE *fp, node_t *node)
{
}

void emit_while(FILE *fp, node_t *node)
{
}

void emit_func_def(FILE *fp, node_t *node)
{
}

void emit_func_call(FILE *fp, node_t *node)
{
}

void emit_var_decl(FILE *fp, node_t *node)
{
}

void emit_var_init(FILE *fp, node_t *node)
{
}

void emit_compound_stmt(FILE *fp, node_t *node)
{
}

void emit_return(FILE *fp, node_t *node)
{
}

void emit(FILE *fp, node_t *node)
{
    assert(fp);
    if (!node)
        return;

    switch (node->type) {
    case NODE_CONSTANT:
        emit_constant(fp, node);
        break;
    case NODE_STRING:
        emit_string(fp, node);
        break;
    case NODE_POSTFIX:
        emit_postfix(fp, node);
        break;
    case NODE_UNARY:
        emit_unary(fp, node);
        break;
    case NODE_BINARY:
        emit_unary(fp, node);
        break;
    case NODE_TERNARY:
        emit_ternary(fp, node);
        break;
    case NODE_IF:
        emit_if(fp, node);
        break;
    case NODE_FOR:
        emit_for(fp, node);
        break;
    case NODE_WHILE:
        emit_while(fp, node);
        break;
    case NODE_FUNC_DEF:
        emit_func_def(fp, node);
        break;
    case NODE_FUNC_CALL:
        emit_func_call(fp, node);
        break;
    case NODE_VAR_DECL:
        emit_var_decl(fp, node);
        break;
    case NODE_VAR_INIT:
        emit_var_init(fp, node);
        break;
    case NODE_COMPOUND_STMT:
        emit_compound_stmt(fp, node);
        break;
    case NODE_RETURN:
        emit_return(fp, node);
        break;

    default:
        break;
    }
}
