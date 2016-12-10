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
#define EMIT_LABEL(label) fprintf(fp, "%s:\n", label)
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

static char *make_label(void)
{
    static int n = 0;
    return format(".L%d", n++);
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
    EMIT_LABEL(node->slabel);
    EMIT(".string \"%s\"", unescape(node->sval));
    EMIT(".text");
    EMIT_INST("mov", node->ctype->size, "$%s, %s", node->slabel, rax[node->ctype->size]);
}

static void emit_postfix_inc_dec(FILE *fp, node_t *node)
{
    char *inst;
    int size;
    int delta;

    assert(node && node->type == NODE_POSTFIX);
    emit(fp, node->operand);
    inst = (node->unary_op == PUNCT_INC) ? "add" : "sub";
    size = node->ctype->size;
    delta = (node->operand->ctype->type == CTYPE_PTR) ? node->operand->ctype->ptr->size : 1;
    EMIT_INST("mov", size, "%s, %s", rax[size], rcx[size]);
    EMIT_INST(inst, size, "$%d, %s", delta, rcx[size]);
    EMIT_INST("mov", size, "%s, -%d(%%rbp)", rcx[size], node->operand->loffset);
}

static void emit_prefix_inc_dec(FILE *fp, node_t *node)
{
    char *inst;
    int size;
    int delta;

    assert(node && node->type == NODE_UNARY
            && (node->unary_op == PUNCT_INC || node->unary_op == PUNCT_DEC));
    emit(fp, node->operand);
    inst = (node->unary_op == PUNCT_INC) ? "add" : "sub";
    size = node->ctype->size;
    delta = (node->operand->ctype->type == CTYPE_PTR) ? node->operand->ctype->ptr->size : 1;
    EMIT_INST(inst, size, "$%d, %s", delta, rax[size]);
    EMIT_INST("mov", size, "%s, -%d(%%rbp)", rax[size], node->operand->loffset);
}

static void emit_addr(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_UNARY && node->unary_op == '&');
    switch (node->operand->type) {
    case NODE_VAR:
        EMIT_INST("lea", 8, "-%d(%%rbp), %s", node->operand->loffset, rax[8]);
        break;

    case NODE_UNARY:
        /* Both & and * are ommited */
        assert(node->operand->unary_op == '*');
        emit(fp, node->operand->operand);
        break;
    /* TODO: array */
    default:
        errorf("invalid operand of \'&\'\n");
    }
}

static void emit_deref(FILE *fp, node_t *node)
{
    int size;

    assert(node && node->type == NODE_UNARY && node->unary_op == '*');
    emit(fp, node->operand);
    size = node->operand->ctype->size;
    EMIT_INST("mov", size, "(%s), %s", rax[8], rax[size]);
}

static void emit_unary(FILE *fp, node_t *node)
{
    int size;

    assert(node && node->type == NODE_UNARY);
    switch(node->unary_op) {
    case PUNCT_INC:
    case PUNCT_DEC:
        emit_prefix_inc_dec(fp, node);
        break;

    case '+':
        break;

    case '-':
    case '~':
        size = node->operand->ctype->size;
        emit(fp, node->operand);
        EMIT_INST(node->unary_op == '-' ? "neg" : "not", size, "%s", rax[size]);
        break;

    case '!':
        size = node->operand->ctype->size;
        emit(fp, node->operand);
        EMIT_INST("cmp", size, "$0, %s", rax[size]);
        EMIT("sete    %%al");
        EMIT("movzbl  %%al, %%eax");
        break;

    case '&':
        emit_addr(fp, node);
        break;

    case '*':
        emit_deref(fp, node);
        break;

    default:
        errorf("invalid unary op %c\n", node->unary_op);
    }
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
        errorf("invalid arith binary op %c\n", node->binary_op);
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
            EMIT("cltd");
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

static void emit_log_and_binary(FILE *fp, node_t *node)
{
    int size;
    char *label;

    assert(node && node->type == NODE_BINARY && node->binary_op == PUNCT_AND);
    emit(fp, node->left);
    size = node->left->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    label = make_label();
    EMIT("je      %s", label);

    emit(fp, node->right);
    size = node->right->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    EMIT("je      %s", label);

    size = node->ctype->size;
    EMIT_INST("mov", size, "$1, %s", rax[size]);
    EMIT_LABEL(label);
}

static void emit_log_or_binary(FILE *fp, node_t *node)
{
    int size;
    char *label1, *label2;

    assert(node && node->type == NODE_BINARY && node->binary_op == PUNCT_OR);
    emit(fp, node->left);
    size = node->left->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    label1 = make_label();
    EMIT("jne     %s", label1);

    emit(fp, node->right);
    size = node->left->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    label2 = make_label();
    EMIT("je      %s", label2);

    EMIT_LABEL(label1);
    size = node->ctype->size;
    EMIT_INST("mov", size, "$1, %s", rax[size]);
    EMIT_LABEL(label2);
}

static void emit_assign_binary(FILE *fp, node_t *node)
{
    int size;
    node_t *lvalue;

    assert(node && node->type == NODE_BINARY && node->binary_op == '=');
    emit(fp, node->right);
    size = node->ctype->size;
    lvalue = node->left;
    if (lvalue->type == NODE_VAR)
        EMIT_INST("mov", size, "%s, -%d(%%rbp)", rax[size], lvalue->loffset);
    else {
        assert(lvalue->type == NODE_UNARY && lvalue->unary_op == '*');
        PUSH("%%rax");
        emit(fp, lvalue->operand);
        POP("%%rcx");
        EMIT_INST("mov", size, "%s, (%%rax)", rcx[size]);
        EMIT_INST("mov", size, "%s, %s", rcx[size], rax[size]);
        /*
        EMIT_INST("mov", 8, "%%rax, %%rcx");
        POP("%%rax");
        EMIT_INST("mov", size, "%s, (%%rcx)", rax[size]);
        */
    }
}

static void emit_cmp_binary(FILE *fp, node_t *node)
{
    char *op;
    int size;

    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '<':
        op = "setl";
        break;
    case '>':
        op = "setg";
        break;
    case PUNCT_LE:
        op = "setle";
        break;
    case PUNCT_GE:
        op = "setge";
        break;
    case PUNCT_EQ:
        op = "sete";
        break;
    case PUNCT_NE:
        op = "setne";
        break;

    default:
        errorf("invalid cmp binary op %c\n", node->binary_op);
        break;
    }

    emit(fp, node->left);
    PUSH("%%rax");
    emit(fp, node->right);
    POP("%%rcx");
    size = node->left->ctype->size;
    EMIT_INST("cmp", size, "%s, %s", rax[size], rcx[size]);
    EMIT("%s    %%al", op);
    EMIT("movzbl  %%al, %%eax");
}

static void emit_comma_binary(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_BINARY && node->binary_op == ',');
    emit(fp, node->left);
    emit(fp, node->right);
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

    case PUNCT_AND:
        emit_log_and_binary(fp, node);
        break;
    case PUNCT_OR:
        emit_log_or_binary(fp, node);
        break;

    case '=':
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
    int size;
    char *els;
    char *done;

    assert(node && node->type == NODE_TERNARY);
    emit(fp, node->cond);
    size = node->cond->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    els = make_label();
    EMIT("je      %s", els);
    emit(fp, node->then);
    done = make_label();
    EMIT("jmp     %s", done);
    EMIT_LABEL(els);
    emit(fp, node->els);
    EMIT_LABEL(done);
}

static void emit_if(FILE *fp, node_t *node)
{
    int size;
    char *label;

    assert(node && node->type == NODE_IF);
    /*      if (!cond)
     *          goto false;
     *      then;
     *      goto done;
     * false:
     *      else;
     * done:
     */
    emit(fp, node->cond);
    size = node->cond->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    label = make_label();
    EMIT("je      %s", label);
    emit(fp, node->then);
    if (node->els) {
        char *done = make_label();
        EMIT("jmp     %s", done);
        EMIT_LABEL(label);
        emit(fp, node->els);
        EMIT_LABEL(done);
    } else
        EMIT_LABEL(label);
}

static void emit_for(FILE *fp, node_t *node)
{
    int size;
    char *test, *loop;

    assert(node && node->type == NODE_FOR);
    /*      init;
     *      goto test;
     * loop:
     *      body;
     *      step;
     * test:
     *      if (cond)
     *          goto loop;
     */
    emit(fp, node->for_init);
    test = make_label();
    EMIT("jmp     %s", test);
    loop = make_label();
    EMIT_LABEL(loop);
    emit(fp, node->for_body);
    emit(fp, node->for_step);
    EMIT_LABEL(test);
    if (node->for_cond) {
        emit(fp, node->for_cond);
        size = node->for_cond->ctype->size;
        EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
        EMIT("jne     %s", loop);
    } else
        EMIT("jmp     %s", loop);
}
static void emit_do_while(FILE *fp, node_t *node)
{
    int size;
    char *loop;

    assert(node && node->type == NODE_DO_WHILE);
    /* loop:
     *      body;
     *      if (cond)
     *          goto loop;
     */
    loop = make_label();
    EMIT_LABEL(loop);
    emit(fp, node->while_body);
    emit(fp, node->while_cond);
    size = node->while_cond->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    EMIT("jne     %s", loop);
}

static void emit_while(FILE *fp, node_t *node)
{
    int size;
    char *loop, *done;

    assert(node && node->type == NODE_WHILE);
    /* loop:
     *      if (!cond)
     *          goto end;
     *      body;
     *      goto loop;
     * done:
     *
    loop = make_label();
    EMIT_LABEL(loop);
    emit(fp, node->while_cond);
    size = node->while_cond->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    done = make_label();
    EMIT("je      %s", done);
    emit(fp, node->while_body);
    EMIT("jmp     %s", loop);
    EMIT_LABEL(done);
    */

    /*      goto test;
     * loop:
     *      body;
     * test:
     *      if (cond)
     *          goto loop;
     */
    done = make_label();
    EMIT("jmp     %s", done);
    loop = make_label();
    EMIT_LABEL(loop);
    emit(fp, node->while_body);
    EMIT_LABEL(done);
    emit(fp, node->while_cond);
    size = node->while_cond->ctype->size;
    EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    EMIT("jne     %s", loop);
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
    EMIT_LABEL(node->func_name);
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

/* TODO: used to profile
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
    /* TODO: %eax must be set to the number of floating point arguments
     * Now set 0
     */
    if (node->ctype->is_va)
        EMIT_INST("mov", 4, "$0, %s", rax[4]);
    /* size of stack frame is times of 16 bytes */
    if (offset % 16 != 0)
        EMIT("subq    $%d, %%rsp", align(offset, 16) - offset);
    EMIT("call    %s", node->func_name);
}

static void emit_var_decl(FILE *fp, node_t *node)
{
    assert(node && (node->type == NODE_VAR_DECL || node->type == NODE_VAR));
    /* Avoid emit var to rax when decl */
    if (node->type == NODE_VAR_DECL)
        node->type = NODE_VAR;
    else
        EMIT_INST("mov", node->ctype->size, "-%d(%%rbp), %s", node->loffset, rax[node->ctype->size]);
}

static void emit_var_init(FILE *fp, node_t *node)
{
    int size;

    assert(node && node->type == NODE_VAR_INIT);
    node->left->type = NODE_VAR;
    emit(fp, node->right);
    size = node->left->ctype->size;
    EMIT_INST("mov", size, "%s, -%d(%%rbp)", rax[size], node->left->loffset);
}

static vector_t *get_local_var(node_t *node)
{
    size_t i;
    vector_t *vars;

    assert(node && node->type == NODE_COMPOUND_STMT);
    if (!node->stmts)
        return NULL;

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
        emit_postfix_inc_dec(fp, node);
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
    case NODE_DO_WHILE:
        emit_do_while(fp, node);
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
    case NODE_VAR:
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
        errorf("invalid node type\n");
        break;
    }
}
