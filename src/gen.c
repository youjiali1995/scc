#include <assert.h>
#include "gen.h"
#include "util.h"

static char suffix[9] = {0, 'b', 'w', 0, 'l', 0, 0, 0, 'q'};
static char *arg_regs[9][6] = {
    {NULL},
    {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"}, /* 1 */
    {"%di",  "%si",  "%dx", "%cx", "%r8w", "%r9w"}, /* 2 */
    {NULL},
    {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"}, /* 4 */
    {NULL}, {NULL}, {NULL},
    {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"} /* 8 */
};
static char *caller_saves[] = {"%r10", "%r11", NULL};
static char *callee_saves[] = {"%rbx", "%r12", "%r13", "%r14", "%r15", NULL};

static int offset;

#define EMIT(fmt, ...) fprintf(fp, "\t" fmt "\n", ##__VA_ARGS__)
#define EMIT_LABEL(fmt, ...) fprintf(fp, fmt ":\n", ##__VA_ARGS__)
#define PUSH(fmt, ...) \
    do { \
        fprintf(fp, "\tpushq   " fmt "\n", ##__VA_ARGS__); \
        offset += 8; \
    } while (0)
#define POP(fmt, ...) \
    do { \
        fprintf(fp, "\tpopq    " fmt "\n", ##__VA_ARGS__); \
        offset -= 8; \
    } while (0)
#define MOV(size, fmt, ...) \
    fprintf(fp, "\tmov%c    " fmt "\n", suffix[size], ##__VA_ARGS__)

static int align(int m, int n)
{
    int mod = m % n;
    return mod == 0 ? m : m - mod + n;
}

static void emit_compound_stmt(FILE *fp, node_t *node);

static void emit_constant(FILE *fp, node_t *node)
{
}

static void emit_string(FILE *fp, node_t *node)
{
    static int n = 0;

    assert(node && node->type == NODE_STRING);
    EMIT(".section\t.rodata");
    node->slabel = format(".LC%d", n++);
    EMIT_LABEL("%s", node->slabel);
    EMIT(".string \"%s\"", unescape(node->sval));
    EMIT(".text");
}

static void emit_postfix(FILE *fp, node_t *node)
{
}

static void emit_unary(FILE *fp, node_t *node)
{
}

static void emit_binary(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_BINARY);
}

static void emit_ternary(FILE *fp, node_t *node)
{
}

static void emit_if(FILE *fp, node_t *node)
{
}

static void emit_for(FILE *fp, node_t *node)
{
}

static void emit_while(FILE *fp, node_t *node)
{
}

static vector_t *get_local_var(node_t *node)
{
    size_t i;
    vector_t *vars;

    assert(node && node->type == NODE_COMPOUND_STMT);
    vars = make_vector();
    for (i = 0; i < vector_len(node->stmts); i++) {
        node_t *expr = vector_get(node->stmts, i);
        /* init-decl-list */
        if (expr->type == NODE_BINARY && expr->unary_op == ',' && expr->ctype == NULL) {
            /* iterative inorder traversal */
            vector_t *stack = make_vector();
            while (vector_len(stack) || expr) {
                if (expr->type == NODE_BINARY) {
                    vector_append(stack, expr);
                    expr = expr->left;
                } else {
                    if (expr->type == NODE_VAR_INIT)
                        vector_append(vars, expr->left);
                    else
                        vector_append(vars, expr);
                    expr = vector_len(stack) ? ((node_t *) vector_pop(stack))->right : NULL;
                }
            }
            free_vector(stack, NULL);
        } else if (expr->type == NODE_VAR_INIT) {
            vector_append(vars, expr->left);
        } else if (expr->type == NODE_VAR_DECL)
            vector_append(vars, expr);
    }
    return vars;
}

static void set_var_offset(vector_t *vars)
{
    size_t i;
    node_t *var;
    int size;

    if (!vars)
        return;
    for (i = 0; i < vector_len(vars); i++) {
        var = vector_get(vars, i);
        assert(var->type == NODE_VAR_DECL);
        size = var->ctype->size < 8 ? 4 : 8;
        offset = align(offset + var->ctype->size, size);
        var->loffset = offset;
    }
}

static void emit_func_prologue(FILE *fp, node_t *node)
{
    size_t i;

    EMIT(".text");
    EMIT(".global  %s", node->func_name);
    EMIT(".type    %s, @function", node->func_name);
    EMIT_LABEL("%s", node->func_name);
    PUSH("%%rbp");
    MOV(8, "%%rsp, %%rbp");

    offset = 0;
    set_var_offset(node->params);
    set_var_offset(get_local_var(node->func_body));
    if (offset)
        EMIT("subq    $%d, %%rsp", offset);

    /* TODO: float type %xmm
     *       > 6 args
     */
    for (i = 0; i < vector_len(node->params); i++) {
        node_t *var = vector_get(node->params, i);
        MOV(var->ctype->size, "%s, -%d(%%rbp)", arg_regs[var->ctype->size][i], var->loffset);
    }
}

static void emit_ret(FILE *fp)
{
    EMIT("leave");
    EMIT("ret");
}

static void emit_func_def(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_FUNC_DEF);
    emit_func_prologue(fp, node);
    emit_compound_stmt(fp, node->func_body);
    emit_ret(fp);
}

static void emit_func_call(FILE *fp, node_t *node)
{
    size_t i;

    assert(node && node->type == NODE_FUNC_CALL);
    for (i = 0; i < vector_len(node->params); i++) {
        node_t *arg = vector_get(node->params, i);
        emit(fp, arg);
        if (arg->type == NODE_STRING)
            MOV(8, "$%s, %s", arg->slabel, arg_regs[8][i]);
        else if (arg->type == NODE_CONSTANT)
            MOV(arg->ctype->size, "$%d, %s", arg->ival, arg_regs[arg->ctype->size][i]);
        else if (arg->type == NODE_VAR_DECL)
            MOV(arg->ctype->size, "-%d(%%rbp), %s", arg->loffset, arg_regs[arg->ctype->size][i]);
        else
            /* TODO: expr */
            MOV(8, "%%rax, %s", arg_regs[8][i]);
    }
    /* size of stack frame is times of 16 bytes */
    if (offset % 16 != 0)
        EMIT("subq    $%d, %%rsp", align(offset, 16) - offset);
    EMIT("call    %s", node->func_name);
}

static void emit_var_decl(FILE *fp, node_t *node)
{
}

static void emit_var_init(FILE *fp, node_t *node)
{
}

static void emit_compound_stmt(FILE *fp, node_t *node)
{
    size_t i;

    assert(node && node->type == NODE_COMPOUND_STMT);
    for (i = 0; i < vector_len(node->stmts); i++)
        emit(fp, vector_get(node->stmts, i));
}

static void emit_return(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_RETURN);

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
