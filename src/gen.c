#include <assert.h>
#include <string.h>
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

#if 0
static char *caller_saves[] = {"%r10", "%r11", NULL};
static char *callee_saves[] = {"%rbx", "%r12", "%r13", "%r14", "%r15", NULL};
#endif

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

#define PUSH_XMM(n) \
    do { \
        EMIT("subq    $8, %%rsp"); \
        EMIT("movsd   %%xmm%d, (%%rsp)", n); \
        offset += 8; \
    } while (0)
#define POP_XMM(n) \
    do { \
        EMIT("movsd   (%%rsp), %%xmm%d", n); \
        EMIT("addq    $8, %%rsp"); \
        offset -= 8; \
    } while (0)

static bool is_float(ctype_t *ctype)
{
    if (ctype == ctype_float || ctype == ctype_double)
        return true;
    return false;
}

static int align(int m, int n)
{
    int mod = m % n;
    return mod == 0 ? m : m - mod + n;
}

static char *make_jump_label(void)
{
    static int n = 0;
    return format(".L%d", n++);
}

static char *make_data_label(void)
{
    static int n = 0;
    return format(".LC%d", n++);
}

static void emit_compound_stmt(FILE *fp, node_t *node);
static void emit_assign(FILE *fp, node_t *dst, char *src);
static void emit_cmp_0(FILE *fp, node_t *node);

static void emit_constant(FILE *fp, node_t *node)
{
    int size;
    union {
        int i;
        long l;
        float f;
        double d;
    } s;

    assert(node && node->type == NODE_CONSTANT);
    size = node->ctype->size;
    if (!is_float(node->ctype)) {
        EMIT_INST("mov", size, "$%ld, %s", node->ival, rax[size]);
    } else {
        EMIT(".section\t.rodata");
        EMIT(".align %d", node->ctype->size);
        node->flabel = make_data_label();
        EMIT_LABEL(node->flabel);
        if (node->ctype == ctype_float) {
            s.f = node->fval;
            EMIT(".long   %d", s.i);
            EMIT(".text");
            EMIT("movss   %s(%%rip), %%xmm0", node->flabel);
        } else {
            s.d = node->fval;
            EMIT(".quad   %ld", s.l);
            EMIT(".text");
            EMIT("movsd   %s(%%rip), %%xmm0", node->flabel);
        }
    }
}

static void emit_string(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_STRING);
    EMIT(".section\t.rodata");
    node->slabel = make_data_label();
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
    delta = is_ptr(node->operand->ctype) ? node->operand->ctype->ptr->size : 1;
    EMIT_INST("mov", size, "%s, %s", rax[size], rcx[size]);
    EMIT_INST(inst, size, "$%d, %s", delta, rcx[size]);
    emit_assign(fp, node->operand, "%rcx");
}

static void emit_prefix_inc_dec(FILE *fp, node_t *node)
{
    char *inst;
    int size;
    int delta;

    assert(node && node->ctype == ctype_int && node->type == NODE_UNARY
            && (node->unary_op == PUNCT_INC || node->unary_op == PUNCT_DEC));
    emit(fp, node->operand);
    inst = (node->unary_op == PUNCT_INC) ? "add" : "sub";
    size = node->ctype->size;
    delta = is_ptr(node->operand->ctype) ? node->operand->ctype->ptr->size : 1;
    EMIT_INST(inst, size, "$%d, %s", delta, rax[size]);
    emit_assign(fp, node->operand, "%rax");
}

static char *get_float1_label(FILE *fp, ctype_t *ctype)
{
    static char *f1, *d1;

    assert(ctype == ctype_float || ctype == ctype_double);
    if (ctype == ctype_float) {
        if (!f1) {
            union {
                int i;
                float f;
            } s;
            s.f = 1.0f;
            f1 = make_data_label();
            EMIT(".section\t.rodata");
            EMIT_LABEL(f1);
            EMIT(".long   %d", s.i);
            EMIT(".text");
        }
        return f1;
    } else {
        if (!d1) {
            union {
                long l;
                double d;
            } s;
            s.d = 1.0;
            d1 = make_data_label();
            EMIT(".section\t.rodata");
            EMIT_LABEL(d1);
            EMIT(".quad   %ld", s.l);
            EMIT(".text");
        }
        return d1;
    }
}

static void emit_float_postfix_inc_dec(FILE *fp, node_t *node)
{
    char *inst, *label;
    char suffix;

    assert(node && node->type == NODE_POSTFIX);
    suffix = (node->ctype == ctype_float) ? 's' : 'd';
    inst = (node->unary_op == PUNCT_INC) ? "adds" : "subs";
    emit(fp, node->operand);
    label = get_float1_label(fp, node->ctype);
    PUSH_XMM(0);
    EMIT("movs%c   %s(%%rip), %%xmm1", suffix, label);
    EMIT("%s%c   %%xmm1, %%xmm0", inst, suffix);
    emit_assign(fp, node->operand, "%xmm0");
    POP_XMM(0);

}

static void emit_float_prefix_inc_dec(FILE *fp, node_t *node)
{
    char *inst, *label;
    char suffix;

    assert(node && (node->ctype == ctype_float || node->ctype == ctype_double)
            && node->type == NODE_UNARY && (node->unary_op == PUNCT_INC || node->unary_op == PUNCT_DEC));
    suffix = (node->ctype == ctype_float) ? 's' : 'd';
    inst = (node->unary_op == PUNCT_INC) ? "adds" : "subs";
    emit(fp, node->operand);
    label = get_float1_label(fp, node->ctype);
    EMIT("movs%c   %s(%%rip), %%xmm1", suffix, label);
    EMIT("%s%c   %%xmm1, %%xmm0", inst, suffix);
    emit_assign(fp, node->operand, "%xmm0");
}

static void emit_addr(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_UNARY && node->unary_op == '&');
    switch (node->operand->type) {
    case NODE_VAR:
        EMIT_INST("lea", node->ctype->size, "-%d(%%rbp), %s", node->operand->loffset, rax[node->ctype->size]);
        break;

    case NODE_UNARY:
        /* Both & and * are ommited */
        assert(node->operand->unary_op == '*');
        emit(fp, node->operand->operand);
        break;

    default:
        errorf("invalid operand of \'&\'\n");
    }
}

static void emit_deref(FILE *fp, node_t *node)
{
    ctype_t *ctype;

    assert(node && node->type == NODE_UNARY && node->unary_op == '*');
    emit(fp, node->operand);
    ctype = node->ctype;
    if (ctype != ctype_float && ctype != ctype_double) {
        EMIT_INST("mov", ctype->size, "(%s), %s", rax[node->operand->ctype->size], rax[ctype->size]);
    } else {
        char *inst = (ctype == ctype_float) ? "movss" : "movsd";
        EMIT("%s   (%s), %%xmm0", inst, rax[node->operand->ctype->size]);
    }
}

static void emit_float_neg(FILE *fp, node_t *node)
{
    static char *f, *d;
    char *label, suffix;

    assert(node && node->type == NODE_UNARY && node->unary_op == '-');
    if (node->ctype == ctype_float) {
        if (!f) {
            EMIT(".section\t.rodata");
            EMIT(".align 16");
            f = make_data_label();
            EMIT_LABEL(f);
            EMIT(".long   2147483648");
            EMIT(".long   0");
            EMIT(".long   0");
            EMIT(".long   0");
            EMIT(".text");
        }
        label = f;
        suffix = 's';
    } else {
        if (!d) {
            EMIT(".section\t.rodata");
            EMIT(".align 16");
            d = make_data_label();
            EMIT_LABEL(d);
            EMIT(".long   0");
            EMIT(".long   -2147483648");
            EMIT(".long   0");
            EMIT(".long   0");
            EMIT(".text");
        }
        label = d;
        suffix = 'd';
    }
    emit(fp, node->operand);
    EMIT("movs%c   %s(%%rip), %%xmm1", suffix, label);
    EMIT("xorp%c   %%xmm1, %%xmm0", suffix);
}

static void emit_unary(FILE *fp, node_t *node)
{
    int size;

    assert(node && node->type == NODE_UNARY);
    switch(node->unary_op) {
    case PUNCT_INC:
    case PUNCT_DEC:
        if (is_float(node->ctype))
            emit_float_prefix_inc_dec(fp, node);
        else
            emit_prefix_inc_dec(fp, node);
        break;

    case '+':
        break;

    case '-':
        if (is_float(node->ctype)) {
            emit_float_neg(fp, node);
            break;
        }
        /* fall through */
    case '~':
        size = node->operand->ctype->size;
        emit(fp, node->operand);
        EMIT_INST(node->unary_op == '-' ? "neg" : "not", size, "%s", rax[size]);
        break;

    case '!':
        emit_cmp_0(fp, node->operand);
        EMIT("%s    %%al", is_float(node->operand->ctype) ? "setnp" : "sete");
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

static int bit(int n)
{
    int i;

    for (i = 0; n != 1; i++, n >>= 1)
        ;
    return i;
}

static void emit_ptr_arith_binary(FILE *fp, node_t *node)
{
    int size;
    int shift_bits;

    assert(is_ptr(node->left->ctype));
    size = node->left->ctype->size;
    shift_bits = bit(node->left->ctype->ptr->size);
    emit(fp, node->left);
    PUSH("%%rax");
    emit(fp, node->right);
    POP("%%rcx");
    /* ptr - ptr */
    if (is_ptr(node->right->ctype)) {
        assert(node->binary_op == '-');
        EMIT_INST("sub", size, "%s, %s", rax[size], rcx[size]);
        EMIT_INST("mov", size, "%s, %s", rcx[size], rax[size]);
        if (shift_bits)
            EMIT_INST("sar", size, "$%d, %s", shift_bits, rax[size]);
    /* ptr +- int */
    } else {
        /* sign extend %eax to %rax */
        EMIT("cltq");
        EMIT_INST("sal", size, "$%d, %s", shift_bits, rax[size]);
        EMIT_INST(node->binary_op == '-' ? "sub" : "add", size, "%s, %s", rax[size], rcx[size]);
        EMIT_INST("mov", size, "%s, %s", rcx[size], rax[size]);
    }
}

static void emit_arith_binary(FILE *fp, node_t *node)
{
    char *inst;
    int size;

    assert(node && node->type == NODE_BINARY);
    if (is_ptr(node->left->ctype)) {
        emit_ptr_arith_binary(fp, node);
        return;
    }

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

static void emit_float_arith_binary(FILE *fp, node_t *node)
{
    char suffix, *inst;

    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '+':
        inst = "adds";
        break;
    case '-':
        inst = "subs";
        break;
    case '*':
        inst = "muls";
        break;
    case '/':
        inst = "divs";
        break;

    default:
        errorf("invalid arith binary op %c\n", node->binary_op);
    }

    suffix = (node->ctype == ctype_float) ? 's' : 'd';
    if (node->binary_op == '+' || node->binary_op == '*') {
        emit(fp, node->left);
        PUSH_XMM(0);
        emit(fp, node->right);
        POP_XMM(1);
        EMIT("%s%c   %%xmm1, %%xmm0", inst, suffix);
    } else {
        emit(fp, node->left);
        PUSH_XMM(0);
        emit(fp, node->right);
        EMIT("movs%c   %%xmm0, %%xmm1", suffix);
        POP_XMM(0);
        EMIT("%s%c   %%xmm1, %%xmm0", inst, suffix);
    }
}

static void emit_cmp_0(FILE *fp, node_t *node)
{
    emit(fp, node);
    if (is_float(node->ctype)) {
        char suffix = (node->ctype == ctype_float) ? 's' : 'd';
        EMIT("xorp%c   %%xmm1, %%xmm1", suffix);
        EMIT("ucomis%c %%xmm0, %%xmm1", suffix);
    } else {
        int size = node->ctype->size;
        EMIT_INST("test", size, "%s, %s", rax[size], rax[size]);
    }
}

static void emit_log_and_binary(FILE *fp, node_t *node)
{
    int size;
    char *inst;
    char *f, *done;

    /* A && B:
     *      if (!A)
     *          goto false;
     *      if (!B)
     *          goto false;
     *      eax = 1;
     *      goto done;
     * false:
     *      eax = 0;
     * done:
     */
    assert(node && node->type == NODE_BINARY && node->binary_op == PUNCT_AND);
    emit_cmp_0(fp, node->left);
    inst = is_float(node->left->ctype) ? "jnp" : "je";
    f = make_jump_label();
    EMIT("%s      %s", inst, f);
    emit_cmp_0(fp, node->right);
    inst = is_float(node->right->ctype) ? "jnp" : "je";
    EMIT("%s      %s", inst, f);

    size = node->ctype->size;
    EMIT_INST("mov", size, "$1, %s", rax[size]);
    done = make_jump_label();
    EMIT("jmp     %s", done);
    EMIT_LABEL(f);
    EMIT_INST("mov", size, "$0, %s", rax[size]);
    EMIT_LABEL(done);
}

static void emit_log_or_binary(FILE *fp, node_t *node)
{
    int size;
    char *inst;
    char *t, *done;

    /* A || B:
     *      if (A)
     *          goto true;
     *      if (B)
     *          goto true;
     *      eax = 0;
     *      goto done;
     * true:
     *      eax = 1;
     * done:
     */
    assert(node && node->type == NODE_BINARY && node->binary_op == PUNCT_OR);
    emit_cmp_0(fp, node->left);
    inst = is_float(node->left->ctype) ? "jp" : "jne";
    t = make_jump_label();
    EMIT("%s      %s", inst, t);
    emit_cmp_0(fp, node->right);
    inst = is_float(node->right->ctype) ? "jp" : "jne";
    EMIT("%s      %s", inst, t);

    size = node->ctype->size;
    EMIT_INST("mov", size, "$0, %s", rax[size]);
    done = make_jump_label();
    EMIT("jmp     %s", done);
    EMIT_LABEL(t);
    EMIT_INST("mov", size, "$1, %s", rax[size]);
    EMIT_LABEL(done);
}

static void emit_assign(FILE *fp, node_t *dst, char *src)
{
    assert(dst && src);
    if (is_float(dst->ctype)) {
        char suffix = (dst->ctype == ctype_float) ? 's' : 'd';
        if (dst->type == NODE_VAR) {
            EMIT("movs%c   %s, -%d(%%rbp)", suffix, src, dst->loffset);
        } else {
            emit(fp, dst->operand);
            EMIT("movs%c   %s, (%%rax)", suffix, src);
        }
    } else {
        int size = dst->ctype->size;
        if (dst->type == NODE_VAR) {
            EMIT_INST("mov", size, "%s, -%d(%%rbp)",
                    !strcmp(src, "%rax") ? rax[size] : rcx[size], dst->loffset);
        } else {
            if (!strcmp(src, "%rax")) {
                PUSH("%%rax");
                emit(fp, dst->operand);
                EMIT_INST("mov", 8, "%s, %s", rax[8], rcx[8]);
                POP("%%rax");
                EMIT_INST("mov", size, "%s, (%%rcx)", rax[size]);
            } else {
                assert(!strcmp(src, "%rcx"));
                emit(fp, dst->operand);
                EMIT_INST("mov", size, "%s, (%%rax)", rcx[size]);
            }
        }
    }
}

static void emit_assign_binary(FILE *fp, node_t *node)
{
    char *src;

    assert(node && node->type == NODE_BINARY && node->binary_op == '=');
    emit(fp, node->right);
    src = is_float(node->ctype) ? "%xmm0" : "%rax";
    emit_assign(fp, node->left, src);
}

static void emit_cmp_binary(FILE *fp, node_t *node)
{
    char *inst;
    int size;

    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '<':
        inst = "setl";
        break;
    case '>':
        inst = "setg";
        break;
    case PUNCT_LE:
        inst = "setle";
        break;
    case PUNCT_GE:
        inst = "setge";
        break;
    case PUNCT_EQ:
        inst = "sete";
        break;
    case PUNCT_NE:
        inst = "setne";
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
    EMIT("%s    %%al", inst);
    EMIT("movzbl  %%al, %%eax");
}

static void emit_float_cmp_binary(FILE *fp, node_t *node)
{
    char *inst;
    char suffix;

    assert(node && node->type == NODE_BINARY);
    switch (node->binary_op) {
    case '<':
        inst = "setna";
        break;
    case '>':
        inst = "seta";
        break;
    case PUNCT_LE:
        inst = "setnae";
        break;
    case PUNCT_GE:
        inst = "setae";
        break;
    case PUNCT_EQ:
        inst = "setnp";
        break;
    case PUNCT_NE:
        inst = "setp";
        break;

    default:
        errorf("invalid cmp binary op %c\n", node->binary_op);
        break;
    }

    suffix = (node->left->ctype == ctype_float) ? 's' : 'd';
    emit(fp, node->left);
    PUSH_XMM(0);
    emit(fp, node->right);
    POP_XMM(1);
    EMIT("ucomis%c %%xmm0, %%xmm1", suffix);
    EMIT("%s    %%al", inst);
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

    case '+': case '-': case '*': case '/':
        if (is_float(node->ctype)) {
            emit_float_arith_binary(fp, node);
            break;
        }
        /* fall through */
    case '%': case PUNCT_LSFT: case PUNCT_RSFT:
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
        if (is_float(node->left->ctype)) {
            emit_float_cmp_binary(fp, node);
            break;
        }
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
    char *f, *done;

    /* A ? B : C
     *      if (!A)
     *          goto false;
     *      B
     *      goto done;
     * false:
     *      C
     * done:
     */
    assert(node && node->type == NODE_TERNARY);
    emit_cmp_0(fp, node->cond);
    f = make_jump_label();
    EMIT("%s      %s", is_float(node->ctype) ? "jnp" : "je", f);
    emit(fp, node->then);
    done = make_jump_label();
    EMIT("jmp     %s", done);
    EMIT_LABEL(f);
    emit(fp, node->els);
    EMIT_LABEL(done);
}

static void emit_if(FILE *fp, node_t *node)
{
    char *f;

    assert(node && node->type == NODE_IF);
    /*      if (!cond)
     *          goto false;
     *      then;
     *      goto done;
     * false:
     *      else;
     * done:
     */
    emit_cmp_0(fp, node->cond);
    f = make_jump_label();
    EMIT("%s      %s", is_float(node->cond->ctype) ? "jnp" : "je", f);
    emit(fp, node->then);
    if (node->els) {
        char *done = make_jump_label();
        EMIT("jmp     %s", done);
        EMIT_LABEL(f);
        emit(fp, node->els);
        EMIT_LABEL(done);
    } else
        EMIT_LABEL(f);
}

static void emit_for(FILE *fp, node_t *node)
{
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
    test = make_jump_label();
    EMIT("jmp     %s", test);
    loop = make_jump_label();
    EMIT_LABEL(loop);
    emit(fp, node->for_body);
    emit(fp, node->for_step);
    EMIT_LABEL(test);
    if (node->for_cond) {
        emit_cmp_0(fp, node->for_cond);
        EMIT("%s      %s", is_float(node->for_cond->ctype) ? "jp" : "jne", loop);
    } else
        EMIT("jmp     %s", loop);
}

static void emit_do_while(FILE *fp, node_t *node)
{
    char *loop;

    assert(node && node->type == NODE_DO_WHILE);
    /* loop:
     *      body;
     *      if (cond)
     *          goto loop;
     */
    loop = make_jump_label();
    EMIT_LABEL(loop);
    emit(fp, node->while_body);
    emit_cmp_0(fp, node->while_cond);
    EMIT("%s      %s", is_float(node->while_cond->ctype) ? "jp" : "jne", loop);
}

static void emit_while(FILE *fp, node_t *node)
{
    char *loop, *test;

    assert(node && node->type == NODE_WHILE);
    /*      goto test;
     * loop:
     *      body;
     * test:
     *      if (cond)
     *          goto loop;
     */
    test = make_jump_label();
    EMIT("jmp     %s", test);
    loop = make_jump_label();
    EMIT_LABEL(loop);
    emit(fp, node->while_body);
    EMIT_LABEL(test);
    emit_cmp_0(fp, node->while_cond);
    EMIT("%s      %s", is_float(node->while_cond->ctype) ? "jp" : "jne", loop);
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
        size = var->ctype->size;
        if (is_array(var->ctype))
            offset = align(offset + var->ctype->ptr->size * var->ctype->len, size);
        else
            offset = align(offset + var->ctype->size, size);
        var->loffset = offset;
    }
    offset = align(offset, 8);
}

static void emit_func_prologue(FILE *fp, node_t *node)
{
    size_t i;
    int float_idx, int_idx;

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

    /* TODO:
     *       > 6 args
     */
    for (i = float_idx = int_idx = 0; i < vector_len(node->params); i++) {
        node_t *var = vector_get(node->params, i);
        if (is_float(var->ctype)) {
            char suffix = (var->ctype == ctype_float) ? 's' : 'd';
            EMIT("movs%c   %%xmm%d, -%d(%%rbp)", suffix, float_idx++, var->loffset);
        } else {
            int size = var->ctype->size;
            EMIT_INST("mov", size, "%s, -%d(%%rbp)", arg_regs[size][int_idx++], var->loffset);
        }
        var->type = NODE_VAR;
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

/* TODO: used to profile */
#if 0
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
#endif

static void emit_func_call(FILE *fp, node_t *node)
{
    int i;
    int float_idx, int_idx;
    node_t *arg;

    assert(node && node->type == NODE_FUNC_CALL);
    for (i = vector_len(node->params) - 1; i >= 0; i--)  {
        arg = vector_get(node->params, i);
        emit(fp, arg);
        if (is_float(arg->ctype))
            PUSH_XMM(0);
        else
            PUSH("%%rax");
    }
    for (i = float_idx = int_idx = 0; i < vector_len(node->params); i++) {
        arg = vector_get(node->params, i);
        if (is_float(arg->ctype))
            POP_XMM(float_idx++);
        else
            POP("%s", arg_regs[8][int_idx++]);
    }

    if (node->is_va)
        EMIT_INST("mov", 4, "$%d, %s", float_idx, rax[4]);
    /* size of stack frame is times of 16 bytes */
    if (offset % 16 != 0) {
        int temp;
        temp = align(offset, 16);
        EMIT("subq    $%d, %%rsp", temp - offset);
        EMIT("call    %s", node->func_name);
        EMIT("addq    $%d, %%rsp", temp - offset);
    } else
        EMIT("call    %s", node->func_name);
}

static void emit_var_decl(FILE *fp, node_t *node)
{
    assert(node && (node->type == NODE_VAR_DECL || node->type == NODE_VAR));
    /* Avoid emit var to rax when decl */
    if (node->type == NODE_VAR_DECL)
        node->type = NODE_VAR;
    else {
        if (is_float(node->ctype))
            EMIT("movs%c   -%d(%%rbp), %%xmm0", (node->ctype == ctype_float) ? 's' : 'd', node->loffset);
        else if (is_array(node->ctype))
            EMIT_INST("lea", node->ctype->size, "-%d(%%rbp), %s", node->loffset, rax[node->ctype->size]);
        else
            EMIT_INST("mov", node->ctype->size, "-%d(%%rbp), %s", node->loffset, rax[node->ctype->size]);
    }
}

static void emit_var_init(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_VAR_INIT);
    node->left->type = NODE_VAR;
    emit(fp, node->right);
    if (is_float(node->left->ctype)) {
        EMIT("movs%c   %%xmm0, -%d(%%rbp)", (node->left->ctype == ctype_float) ? 's' : 'd', node->left->loffset);
    } else {
        int size = node->left->ctype->size;
        EMIT_INST("mov", size, "%s, -%d(%%rbp)", rax[size], node->left->loffset);
    }
}

static void emit_array_init(FILE *fp, node_t *node)
{
    int loffset;
    int size;
    size_t i;

    assert(node && node->type == NODE_ARRAY_INIT);
    node->array->type = NODE_VAR;
    loffset = node->array->loffset;
    size = node->array->ctype->ptr->size;
    for (i = 0; i < vector_len(node->array_init); i++, loffset -= size) {
        node_t *init = vector_get(node->array_init, i);
        emit(fp, init);
        if (is_float(init->ctype))
            EMIT("movs%c   %%xmm0, -%d(%%rbp)", (init->ctype == ctype_float) ? 's' :'d', loffset);
        else
            EMIT_INST("mov", size, "%s, -%d(%%rbp)", rax[size], loffset);
    }
    for (; i < node->array->ctype->len; i++, loffset -= size) {
        EMIT_INST("mov", size, "$0, -%d(%%rbp)", loffset);
    }
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
            while (expr) {
                if (expr->type == NODE_BINARY) {
                    vector_append(stack, expr);
                    expr = expr->left;
                } else {
                    if (expr->type == NODE_VAR_INIT)
                        vector_append(vars, expr->left);
                    else if (expr->type == NODE_ARRAY_INIT)
                        vector_append(vars, expr->array);
                    else
                        vector_append(vars, expr);
                    expr = vector_len(stack) ? ((node_t *) vector_pop(stack))->right : NULL;
                }
            }
            free_vector(stack, NULL);
        } else if (expr->type == NODE_VAR_INIT)
            vector_append(vars, expr->left);
        else if (expr->type == NODE_VAR_DECL)
            vector_append(vars, expr);
        else if (expr->type == NODE_ARRAY_INIT)
            vector_append(vars, expr->array);
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
        EMIT("subq    $%d, %%rsp", offset - prev_offset);
    for (i = 0; i < vector_len(node->stmts); i++)
        emit(fp, vector_get(node->stmts, i));
    if (offset != prev_offset) {
        EMIT("addq    $%d, %%rsp", offset - prev_offset);
        offset = prev_offset;
    }
}

static void emit_return(FILE *fp, node_t *node)
{
    assert(node && node->type == NODE_RETURN);
    emit(fp, node->expr);
    emit_ret(fp);
}

static void emit_cast(FILE *fp, node_t *node)
{
}

static void emit_arith_conv(FILE *fp, node_t *node)
{
    ctype_t *from, *to;
    char *inst;

    assert(node && node->type == NODE_ARITH_CONV);
    emit(fp, node->expr);
    from = node->expr->ctype;
    to = node->ctype;
    if (from == ctype_int) {
        /* int to float/double */
        inst = (to == ctype_float) ? "cvtsi2ss" : "cvtsi2sd";
        EMIT("%s %s, %%xmm0", inst, rax[from->size]);
    } else if (to == ctype_int) {
        /* float/double to int */
        inst = (from == ctype_float) ? "cvtss2si" : "cvttsd2si";
        EMIT("%s %%xmm0, %s", inst, rax[to->size]);
    } else {
        /* float to double/double to float */
        inst = (from == ctype_float) ? "cvtps2pd" : "cvtpd2ps";
        EMIT("%s %%xmm0, %%xmm0", inst);
    }
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
        if (is_float(node->ctype))
            emit_float_postfix_inc_dec(fp, node);
        else
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
    case NODE_ARRAY_INIT:
        emit_array_init(fp, node);
        break;
    case NODE_COMPOUND_STMT:
        emit_compound_stmt(fp, node);
        break;
    case NODE_RETURN:
        emit_return(fp, node);
        break;
    case NODE_CAST:
        emit_cast(fp, node);
        break;
    case NODE_ARITH_CONV:
        emit_arith_conv(fp, node);
        break;

    default:
        errorf("invalid node type\n");
        break;
    }
}
