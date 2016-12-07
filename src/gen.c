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
static char *rax[9] = {NULL, "%al", "%ax", NULL, "%eax", NULL, NULL, NULL, "%rax"};
static char *rcx[9] = {NULL, "%cl", "%cx", NULL, "%ecx", NULL, NULL, NULL, "%rcx"};
static char *caller_saves[] = {"%r10", "%r11", NULL};
static char *callee_saves[] = {"%rbx", "%r12", "%r13", "%r14", "%r15", NULL};

static int offset;

#define EMIT(fmt, ...) fprintf(fp, "\t" fmt "\n", ##__VA_ARGS__)
#define EMIT_LABEL(fmt, ...) fprintf(fp, fmt ":\n", ##__VA_ARGS__)
#define EMIT_INST(inst, size, fmt, ...) \
    fprintf(fp, "\t%s%c    " fmt "\n", inst, suffix[size], ##__VA_ARGS__)

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

static int align(int m, int n)
{
    int mod = m % n;
    return mod == 0 ? m : m - mod + n;
}

static void emit_compound_stmt(FILE *fp, node_t *node);

static void emit_constant(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_CONSTANT);
    EMIT_INST("mov", node->ctype->size, "$%d, %s", node->ival, rax[node->ctype->size]);
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
    EMIT_INST("mov", node->ctype->size, "$%s, %s", node->slabel, rax[node->ctype->size]);
}

static void emit_postfix(FILE *fp, node_t *node)
{
}

static void emit_unary(FILE *fp, node_t *node)
{
}

static void emit_bit_binary(FILE *fp, node_t *node)
{
    char *inst;
    int size;

    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '&':
        inst = "and";
        break;
    case '^':
        inst = "xor";
        break;
    case '|':
        inst = "or";
        break;
    default:
        errorf("invalid bit binary op %c\n", node->binary_op);
        break;
    }

    size = node->ctype->size;
    emit(fp, node->left);
    PUSH("%%rax");
    emit(fp, node->right);
    POP("%%rcx");
    EMIT_INST(inst, size, "%s, %s", rcx[size], rax[size]);
}

static void emit_arith_binary(FILE *fp, node_t *node)
{
    char *inst;
    int size;

    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '+':
        inst = "add";
        break;
    case '-':
        inst = "sub";
        break;
    case '*':
        inst = "imul";
        break;
    case '/':
    case '%':
        inst = "idiv";
        break;
    case PUNCT_LSFT:
        inst = "sal";
        break;
    case PUNCT_RSFT:
        inst = "sar";
        break;

    default:
        errorf("invalid arith binary op %c", node->binary_op);
    }

    size = node->ctype->size;
    if (node->binary_op == '/' || node->binary_op == '%' || node->binary_op == '-'
            || node->binary_op == PUNCT_LSFT || node->binary_op == PUNCT_RSFT) {
        emit(fp, node->left);
        PUSH("%%rax");
        emit(fp, node->right);
        EMIT_INST("mov", size, "%s, %s", rax[size], rcx[size]);
        POP("%%rax");
        if (node->binary_op == '-') {
            EMIT_INST(inst, size, "%s, %s", rcx[size], rax[size]);
        } else if (node->binary_op == '/' || node->binary_op == '%') {
            EMIT("ctld");
            EMIT_INST(inst, size, "%s", rcx[size]);
            if (node->binary_op == '%')
                EMIT_INST("mov", size, "%%edx, %s", rax[size]);
        } else {
            EMIT_INST(inst, size, "%%cl, %s", rax[size]);
        }
    } else {
        emit(fp, node->left);
        PUSH("%%rax");
        emit(fp, node->right);
        POP("%%rcx");
        EMIT_INST(inst, size, "%s, %s", rcx[size], rax[size]);
    }
}

static void emit_log_binary(FILE *fp, node_t *node)
{
}

static void emit_assign_binary(FILE *fp, node_t *node)
{
}

static void emit_cmp_binary(FILE *fp, node_t *node)
{
}

static void emit_comma_binary(FILE *fp, node_t *node)
{
}

static void emit_binary(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '&': case '|': case '^':
        emit_bit_binary(fp, node);
        break;

    case '+': case '-': case '*': case '/': case '%':
    case PUNCT_LSFT: case PUNCT_RSFT:
        emit_arith_binary(fp, node);
        break;

    case PUNCT_AND: case PUNCT_OR:
        emit_log_binary(fp, node);
        break;

    case '=': case PUNCT_IADD: case PUNCT_ISUB: case PUNCT_IMUL: case PUNCT_IDIV: case PUNCT_IMOD:
    case PUNCT_ILSFT: case PUNCT_IRSFT: case PUNCT_IAND: case PUNCT_IOR: case PUNCT_IXOR:
        emit_assign_binary(fp, node);
        break;

    case '<': case '>': case PUNCT_LE: case PUNCT_GE: case PUNCT_EQ: case PUNCT_NE:
        emit_cmp_binary(fp, node);
        break;

    case ',':
        emit_comma_binary(fp, node);
        break;

    default:
        errorf("inknown binary op %c\n", node->binary_op);
        break;
    }

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
    EMIT(".globl  %s", node->func_name);
    EMIT(".type   %s, @function", node->func_name);
    EMIT_LABEL("%s", node->func_name);
    PUSH("%%rbp");
    EMIT_INST("mov", 8, "%%rsp, %%rbp");

    offset = 0;
    set_var_offset(node->params);
    if (offset)
        EMIT_INST("sub", 8, "$%d, %%rsp", offset);

    /* TODO: float type %xmm
     *       > 6 args
     */
    for (i = 0; i < vector_len(node->params); i++) {
        node_t *var = vector_get(node->params, i);
        EMIT_INST("mov", var->ctype->size, "%s, -%d(%%rbp)", arg_regs[var->ctype->size][i], var->loffset);
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

/*
static void mov_var(FILE *fp, node_t *node, char *reg)
{
    int size;

    assert(node && reg);
    size = node->ctype->size;
    switch (node->type) {
    case NODE_STRING:
        EMIT_INST("mov", size, "$%s, %s", node->slabel, reg);
        break;
    case NODE_CONSTANT:
        EMIT_INST("mov", size, "$%d, %s", node->ival, reg);
        break;
    case NODE_VAR_DECL:
        EMIT_INST("mov", size, "-%d(%%rbp), %s", node->loffset, reg);
        break;

    default:
        EMIT_INST("mov", size, "%s, %s", rax[size], reg);
    }
}
*/

static void emit_func_call(FILE *fp, node_t *node)
{
    size_t i;

    assert(node && node->type == NODE_FUNC_CALL);
    for (i = 0; i < vector_len(node->params); i++) {
        node_t *arg = vector_get(node->params, i);
        int size = arg->ctype->size;
        emit(fp, arg);
        EMIT_INST("mov", size, "%s, %s", rax[size], arg_regs[size][i]);
    }
    /* size of stack frame is times of 16 bytes */
    if (offset % 16 != 0)
        EMIT("subq    $%d, %%rsp", align(offset, 16) - offset);
    EMIT("call    %s", node->func_name);
}

static void emit_var_decl(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_VAR_DECL);
    EMIT_INST("mov", node->ctype->size, "-%d(%%rbp), %s", node->loffset, rax[node->ctype->size]);
}

static void emit_var_init(FILE *fp, node_t *node)
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

static void emit_compound_stmt(FILE *fp, node_t *node)
{
    size_t i;
    int prev_offset;

    assert(node && node->type == NODE_COMPOUND_STMT);
    prev_offset = offset;
    set_var_offset(get_local_var(node));
    if (offset != prev_offset)
        EMIT_INST("sub", 8, "$%d, %%rsp", offset - prev_offset);
    for (i = 0; i < vector_len(node->stmts); i++)
        emit(fp, vector_get(node->stmts, i));
}

static void emit_return(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_RETURN);
    emit(fp, node->ret);
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
        emit_binary(fp, node);
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
