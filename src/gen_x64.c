/* Backend x86-64 (AT&T syntax): assembly untuk gas, dirakit di PC dengan gcc. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static FILE *out;
static int nlab;
static int brk_lab[64], cont_lab[64], brk_sp;
static int par;   /* parity stack: 1 saat rsp%16==8, 0 saat rk%16==0 */

static void emit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

static void spush(void) { emit("\tpushq %%rax"); par ^= 1; }
static void spop(const char *r) { emit("\tpopq %%%s", r); par ^= 1; }

static int gen_expr(Method *m, Node *e);
static void gen_stmts(Method *m, Node *lst);
static int cur_ret_label;

/* hasil ekspresi: 0 = int/bool di %eax; 1 = pointer closure di %rax */

static void gen_stmt(Method *m, Node *s) {
    switch (s->kind) {
    case ST_VAR:
        gen_expr(m, s->a);
        if (s->dty == (int)T_FN) emit("\tmovq %%rax, -%ld(%%rbp)", (s->ival + 1) * 8);
        else emit("\tmovl %%eax, -%ld(%%rbp)", (s->ival + 1) * 8);
        break;
    case ST_ASSIGN:
        gen_expr(m, s->a);
        if (s->dty == (int)T_FN) emit("\tmovq %%rax, -%ld(%%rbp)", (s->ival + 1) * 8);
        else emit("\tmovl %%eax, -%ld(%%rbp)", (s->ival + 1) * 8);
        break;
    case ST_IF: {
        int lelse = nlab++, lend = nlab++;
        gen_expr(m, s->a);
        emit("\ttestl %%eax, %%eax");
        emit("\tje .L%d", lelse);
        gen_stmts(m, s->b);
        emit("\tjmp .L%d", lend);
        emit(".L%d:", lelse);
        if (s->c) gen_stmts(m, s->c);
        emit(".L%d:", lend);
        break;
    }
    case ST_WHILE: {
        int ltop = nlab++, lend = nlab++;
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = ltop; brk_sp++;
        emit(".L%d:", ltop);
        gen_expr(m, s->a);
        emit("\ttestl %%eax, %%eax");
        emit("\tje .L%d", lend);
        gen_stmts(m, s->b);
        emit("\tjmp .L%d", ltop);
        emit(".L%d:", lend);
        brk_sp--; brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_FOR: {
        if (s->a) gen_stmt(m, s->a);
        int ltop = nlab++, lend = nlab++, lcont = nlab++;
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = lcont; brk_sp++;
        emit(".L%d:", ltop);
        if (s->b) {
            gen_expr(m, s->b);
            emit("\ttestl %%eax, %%eax");
            emit("\tje .L%d", lend);
        }
        gen_stmts(m, s->d);
        emit(".L%d:", lcont);           /* target continue: sebelum langkah */
        if (s->c) gen_stmt(m, s->c);
        emit("\tjmp .L%d", ltop);
        emit(".L%d:", lend);
        brk_sp--; brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_BRK: emit("\tjmp .L%d", brk_lab[brk_sp - 1]); break;
    case ST_CONT: emit("\tjmp .L%d", cont_lab[brk_sp - 1]); break;
    case ST_RETURN: {
        if (s->a) gen_expr(m, s->a);
        else emit("\tmovl $0, %%eax");
        emit("\tjmp .Lret%d", cur_ret_label);
        break;
    }
    case ST_PRINT:
        if (s->str) {
            emit("\tleaq cpool%d(%%rip), %%rsi", (int)s->cp);
            emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? "sn" : "s");
        } else {
            gen_expr(m, s->a);
            emit("\tmovl %%eax, %%esi");
            emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? "in" : "i");
        }
        emit("\txorl %%eax, %%eax");
        emit("\tcall printf@PLT");
        break;
    case ST_EXPR:
        if (s->a) gen_expr(m, s->a);
        break;
    default:
        die("gen_x64: statement kind=%d", s->kind);
    }
}

static void gen_stmts(Method *m, Node *lst) {
    for (Node *n = lst; n; n = n->next) gen_stmt(m, n);
}

/* Panggil metode. Invariant: par masuk P, keluar P. Saat call, sp selaras 16. */
static void x64_call(Method *m, Node *e) {
    int n = 0;
    int w64[64];
    for (Node *a = e->a; a; a = a->next) {
        if (n >= 6) die("baris %d: maksimal 6 argumen untuk target x86-64", e->line);
        w64[n] = gen_expr(m, a);
        spush();
        n++;
    }
    static const char *reg32[6] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
    static const char *reg64[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    for (int k = 0; k < n; k++) {
        const char *r = w64[k] ? reg64[k] : reg32[k];
        emit("\t%s %d(%%rsp), %%%s", w64[k] ? "movq" : "movl", (n - 1 - k) * 8, r);
    }
    if (n > 0) { emit("\taddq $%d, %%rsp", n * 8); par ^= (n & 1); }
    int need = par;
    if (need) emit("\tsubq $8, %%rsp");
    const char *dot = strchr(e->str, '.');
    char cls[300];
    memcpy(cls, e->str, (size_t)(dot - e->str)); cls[dot - e->str] = 0;
    emit("\tcall %s__%s", cls, dot + 1);
    if (need) emit("\taddq $8, %%rsp");
}

static void gen_bool_chain(Method *m, Node *e) {
    /* && dan || dengan short-circuit */
    int ls = nlab++, lend = nlab++;
    gen_expr(m, e->a);
    emit("\ttestl %%eax, %%eax");
    if (e->str[0] == '&') {
        emit("\tje .L%d", ls);
        gen_expr(m, e->b);
        emit("\ttestl %%eax, %%eax");
        emit("\tje .L%d", ls);
        emit("\tmovl $1, %%eax");
    } else {
        emit("\tjne .L%d", ls);
        gen_expr(m, e->b);
        emit("\ttestl %%eax, %%eax");
        emit("\tjne .L%d", ls);
        emit("\tmovl $0, %%eax");
    }
    emit("\tjmp .L%d", lend);
    emit(".L%d:", ls);
    emit("\tmovl $%d, %%eax", e->str[0] == '&' ? 0 : 1);
    emit(".L%d:", lend);
}

static int gen_expr(Method *m, Node *e) {
    switch (e->kind) {
    case EX_INT:
        emit("\tmovl $%ld, %%eax", e->ival);
        break;
    case EX_BOOL:
        emit("\tmovl $%d, %%eax", (int)e->ival);
        break;
    case EX_VAR:
        if (e->dty == (int)T_FN) {
            emit("\tmovq -%ld(%%rbp), %%rax", (e->ival + 1) * 8);
            return 1;
        }
        emit("\tmovl -%ld(%%rbp), %%eax", (e->ival + 1) * 8);
        break;
    case EX_UN:
        gen_expr(m, e->a);
        if (e->str[0] == '-') emit("\tnegl %%eax");
        else emit("\txorl $1, %%eax");
        break;
    case EX_CALL:
        x64_call(m, e);
        break;
    case EX_ARROW: {
        /* closure = blok 64-bit: [0]=kode [1]=ncapt [2]=nuser [3+j]=capture */
        Method *hm = (Method *)e->cp;
        emit("\tmovq $%ld, %%rdi", 8 * (3 + (long)hm->ncapt));
        int need = par;
        if (need) emit("\tsubq $8, %%rsp");
        emit("\tcall malloc@PLT");
        if (need) emit("\taddq $8, %%rsp");
        emit("\tmovq %%rax, %%r10");
        emit("\tleaq %s__%s(%%rip), %%rax", hm->cls, hm->name);
        emit("\tmovq %%rax, (%%r10)");
        emit("\tmovl $%d, 8(%%r10)", hm->ncapt);
        emit("\tmovl $%d, 16(%%r10)", hm->nparams - 1);
        for (int j = 0; j < hm->ncapt; j++) {
            emit("\tmovl -%ld(%%rbp), %%eax", (hm->captslot[j] + 1) * 8);
            emit("\tmovl %%eax, %ld(%%r10)", 24 + 8 * j);
        }
        emit("\tmovq %%r10, %%rax");
        return 1;
    }
    case EX_FCALL: {
        int n = 0;
        for (Node *a = e->a; a; a = a->next) {
            if (n >= 5) die("baris %d: maksimal 5 argumen pemanggilan lambda", e->line);
            gen_expr(m, a);
            spush();
            n++;
        }
        gen_expr(m, e->b);                /* %rax = closure */
        emit("\tmovq %%rax, %%rbx");
        emit("\tmovl 16(%%rbx), %%eax");  /* nuser */
        emit("\tcmpl $%d, %%eax", n);
        emit("\tjne .Klam");
        emit("\tmovq (%%rbx), %%r11");    /* kode */
        emit("\tmovq %%rbx, %%rdi");      /* arg0 = env */
        static const char *reg32[5] = {"esi", "edx", "ecx", "r8d", "r9d"};
        for (int k = 0; k < n; k++)
            emit("\tmovl %d(%%rsp), %%%s", (n - 1 - k) * 8, reg32[k]);
        if (n > 0) { emit("\taddq $%d, %%rsp", n * 8); par ^= (n & 1); }
        int need = par;
        if (need) emit("\tsubq $8, %%rsp");
        emit("\tcall *%%r11");
        if (need) emit("\taddq $8, %%rsp");
        break;
    }
    case EX_ENVLOAD:
        emit("\tmovq -8(%%rbp), %%rax");  /* slot 0 = param $env */
        emit("\tmovl %ld(%%rax), %%eax", 24 + 8 * (long)e->ival);
        break;
    case EX_BIN: {
        char op = e->str[0];
        if (op == '&' || op == '|') { gen_bool_chain(m, e); break; }
        gen_expr(m, e->a);
        spush();
        gen_expr(m, e->b);
        spop("rcx");
        switch (op) {
        case '+': emit("\taddl %%ecx, %%eax"); break;
        case '-': emit("\tsubl %%ecx, %%eax"); break;
        case '*': emit("\timull %%ecx, %%eax"); break;
        case '/':
            emit("\txchgl %%eax, %%ecx");
            emit("\tcltd");
            emit("\tidivl %%ecx");
            break;
        case '%':
            emit("\txchgl %%eax, %%ecx");
            emit("\tcltd");
            emit("\tidivl %%ecx");
            emit("\tmovl %%edx, %%eax");
            break;
        default: {
            const char *set;
            if (!strcmp(e->str, "<=")) set = "setle";
            else if (!strcmp(e->str, ">=")) set = "setge";
            else if (op == '<') set = "setl";
            else if (op == '>') set = "setg";
            else if (op == '=') set = "sete";
            else if (op == '!') set = "setne";
            else die("gen_x64: operator '%s'", e->str);
            emit("\tcmpl %%ecx, %%eax");
            emit("\t%s %%al", set);
            emit("\tmovzbl %%al, %%eax");
        }
        }
        return 0;
    }
    default:
        die("gen_x64: ekspresi kind=%d", e->kind);
        return 0;
    }
    return 0;
}

static void asm_escape(const char *s, char *dst, size_t cap) {
    size_t j = 0;
    for (const char *p = s; *p && j + 2 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = (char)c; }
        else if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\t') { dst[j++] = '\\'; dst[j++] = 't'; }
        else dst[j++] = (char)c;
    }
    dst[j] = 0;
}

void gen_x64(const char *outpath) {
    out = fopen(outpath, "w");
    if (!out) die("tidak bisa menulis %s", outpath);
    nlab = 0; par = 0;
    emit(".text");

    for (Method *m = prog.methods; m; m = m->next) {
        emit("%s__%s:", m->cls, m->name);
        long framew = ((m->nlocals * 8 + 15) / 16) * 16;
        emit("\tpushq %%rbp");
        emit("\tmovq %%rsp, %%rbp");
        if (framew > 0) emit("\tsubq $%ld, %%rsp", framew);
        {
            static const char *regs[6] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
            static const char *regs64[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            int i = 0;
            for (Param *p = m->params; p; p = p->next, i++) {
                if (i >= 6) break;
                if ((Ty)p->ty == T_FN)   /* param lambda disalin 64-bit */
                    emit("\tmovq %%%s, -%ld(%%rbp)", regs64[i], (i + 1) * 8);
                else
                    emit("\tmovl %%%s, -%ld(%%rbp)", regs[i], (i + 1) * 8);
            }
        }
        brk_sp = 0;
        cur_ret_label = nlab++;
        gen_stmts(m, m->body);
        emit("\tmovl $0, %%eax");        /* kembalian default jalur jatuh */
        emit(".Lret%d:", cur_ret_label);
        emit("\tmovq %%rbp, %%rsp");
        emit("\tpopq %%rbp");
        emit("\tret");
    }

    for (Method *m = prog.methods; m; m = m->next)
        if (m->index == prog.mainidx) {
            emit("\t.globl main");
            emit("main:");
            emit("\tjmp %s__%s", m->cls, m->name);
        }

    /* stub mati: jumlah argumen lambda tidak cocok (dilompati dari jne .Klam).
       push+andq menjamin rsp selaras 16 apa pun parity saat masuk. */
    emit(".Klam:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tandq $-16, %%rsp");
    emit("\tleaq .Lmsg_lam(%%rip), %%rdi");
    emit("\tcall printf@PLT");
    emit("\tmovl $1, %%edi");
    emit("\tcall exit@PLT");

    emit("\t.section .rodata");
    emit(".Lfmt_i: .asciz \"%s\"", "%d");
    emit(".Lfmt_in: .asciz \"%s\"", "%d\\n");
    emit(".Lfmt_s: .asciz \"%s\"", "%s");
    emit(".Lfmt_sn: .asciz \"%s\"", "%s\\n");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    for (int i = 0; i < prog.ncp; i++) {
        char esc[1200];
        asm_escape(prog.cpool[i], esc, sizeof esc);
        emit("cpool%d: .asciz \"%s\"", i, esc);
    }
    fclose(out);
}
