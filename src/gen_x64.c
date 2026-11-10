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
static int netlbl;      /* label EX_NET unik per kemunculan */
static int cur_ret_label;

/* 32-bit int/bool vs 64-bit (pointer / numerik baru / float bits) */
static int is64ty(Ty t) {
    return t == T_FN || t == T_ARI || t == T_LONG || t == T_UINT64 ||
           t == T_UINT8 || t == T_UINT16 || t == T_UINT32 ||
           t == T_BYTE || t == T_SHORT ||
           t == T_FLOAT || t == T_DOUBLE;
}

/* rax kanonik: int sign-extended; bool zero; uint* zero; float bits low32 */
static void widen_ty(Ty t) {
    if (t == T_INT) emit("\tmovslq %%eax, %%rax");
    else if (t == T_BOOL) emit("\tmovl %%eax, %%eax");
}

static void norm_ty(Ty t) {
    switch (t) {
    case T_INT:    emit("\tmovslq %%eax, %%rax"); break;
    case T_UINT32: emit("\tmovl %%eax, %%eax"); break;
    case T_UINT16: emit("\tmovzwl %%ax, %%eax"); break;
    case T_UINT8:  emit("\tmovzbl %%al, %%eax"); break;
    case T_BYTE:   emit("\tmovsbq %%al, %%rax"); break;
    case T_SHORT:  emit("\tmovswq %%ax, %%rax"); break;
    case T_BOOL:   emit("\tandl $1, %%eax"); break;
    default: break;
    }
}

/* konversi kanonik rax dari from ke to */
static void conv_ty(Ty from, Ty to) {
    if (from == to) return;
    if (ty_is_float(to)) {
        if (ty_is_float(from)) {
            if (from == T_FLOAT && to == T_DOUBLE) {
                emit("\tmovd %%eax, %%xmm0");
                emit("\tcvtss2sd %%xmm0, %%xmm0");
                emit("\tmovq %%xmm0, %%rax");
            } else if (from == T_DOUBLE && to == T_FLOAT) {
                emit("\tmovq %%rax, %%xmm0");
                emit("\tcvtsd2ss %%xmm0, %%xmm0");
                emit("\tmovd %%xmm0, %%eax");
            }
            return;
        }
        int uns = ty_is_uint(from);
        const char *ops = (to == T_FLOAT) ? "ss" : "sd";
        if (!uns) {
            if (ty_width(from) == 64) emit("\t%s %%rax, %%xmm0", to == T_FLOAT ? "cvtsi2ssq" : "cvtsi2sdq");
            else emit("\t%s %%eax, %%xmm0", to == T_FLOAT ? "cvtsi2ssl" : "cvtsi2sdl");
            if (to == T_FLOAT) emit("\tmovd %%xmm0, %%eax");
            else emit("\tmovq %%xmm0, %%rax");
            return;
        }
        /* unsigned: uint8/16/32 selalu < 2^31 → jalur signed aman;
         * uint64 >= 2^63 ditangani dengan trik (v>>1)|(v&1) lalu ×2 */
        if (from == T_UINT64) {
            int l1 = nlab++, l2 = nlab++;
            emit("\ttestq %%rax, %%rax");
            emit("\tjs .L%d", l1);
            emit("\t%s %%rax, %%xmm0", to == T_FLOAT ? "cvtsi2ssq" : "cvtsi2sdq");
            emit("\tjmp .L%d", l2);
            emit(".L%d:", l1);
            emit("\tmovq %%rax, %%rcx");
            emit("\tshrq $1, %%rcx");
            emit("\tandl $1, %%eax");
            emit("\torq %%rax, %%rcx");
            emit("\t%s %%rcx, %%xmm0", to == T_FLOAT ? "cvtsi2ssq" : "cvtsi2sdq");
            if (to == T_FLOAT) emit("\taddss %%xmm0, %%xmm0");
            else emit("\taddsd %%xmm0, %%xmm0");
            emit(".L%d:", l2);
        } else {
            /* uint8/16/32 kanonik zero-extended → konversi 64-bit signed aman */
            emit("\t%s %%rax, %%xmm0", to == T_FLOAT ? "cvtsi2ssq" : "cvtsi2sdq");
        }
        if (to == T_FLOAT) emit("\tmovd %%xmm0, %%eax");
        else emit("\tmovq %%xmm0, %%rax");
        return;
    }
    if (ty_is_float(from)) {
        int uns = ty_is_uint(to);
        if (from == T_FLOAT) emit("\tmovd %%eax, %%xmm0");
        else emit("\tmovq %%rax, %%xmm0");
        (void)uns;
        if (ty_width(to) == 64) {
            emit("\t%s %%xmm0, %%rax", from == T_FLOAT ? "cvttss2siq" : "cvttsd2siq");
        } else {
            emit("\t%s %%xmm0, %%eax", from == T_FLOAT ? "cvttss2sil" : "cvttsd2sil");
        }
        norm_ty(to);
        return;
    }
    switch (to) {
    case T_INT:    emit("\tmovslq %%eax, %%rax"); break;
    case T_UINT32: emit("\tmovl %%eax, %%eax"); break;
    case T_UINT16: emit("\tmovzwl %%ax, %%eax"); break;
    case T_UINT8:  emit("\tmovzbl %%al, %%eax"); break;
    case T_BYTE:   emit("\tmovsbq %%al, %%rax"); break;
    case T_SHORT:  emit("\tmovswq %%ax, %%rax"); break;
    case T_LONG: case T_UINT64: break;
    default: break;
    }
}

/* hasil ekspresi: 0 = int/bool di %eax; 1 = pointer closure di %rax */

/* pak byte array (pointer di r10) ke kop_iobuf + base; panjang di r11 */
static void pack_path_bytes_x64(int ln, char tag, long base) {
    emit("\tleaq kop_iobuf(%%rip), %%rbx");
    if (base) emit("\taddq $%ld, %%rbx", base);
    emit("\tmovl (%%r10), %%r11d");
    emit("\tcmpl $%ld, %%r11d", 4000 - base);
    emit("\tjle .LPKX%d%ca", ln, tag);
    emit("\tmovl $%ld, %%r11d", 4000 - base);
    emit(".LPKX%d%ca:", ln, tag);
    emit("\txorl %%ecx, %%ecx");
    emit(".LPKX%d%cb:", ln, tag);
    emit("\tcmpl %%r11d, %%ecx");
    emit("\tjge .LPKX%d%cc", ln, tag);
    emit("\tmovl %%ecx, %%eax");
    emit("\tshlq $3, %%rax");
    emit("\tmovq 8(%%r10,%%rax), %%rdx");
    emit("\tmovb %%dl, (%%rbx,%%rcx)");
    emit("\tincl %%ecx");
    emit("\tjmp .LPKX%d%cb", ln, tag);
    emit(".LPKX%d%cc:", ln, tag);
}

static void gen_stmt(Method *m, Node *s) {
    switch (s->kind) {
    case ST_VAR:
    case ST_ASSIGN:
        gen_expr(m, s->a);
        widen_ty((Ty)s->a->dty);
        conv_ty((Ty)s->a->dty, (Ty)s->dty);
        if (is64ty((Ty)s->dty)) emit("\tmovq %%rax, -%ld(%%rbp)", (s->ival + 1) * 8);
        else emit("\tmovl %%eax, -%ld(%%rbp)", (s->ival + 1) * 8);
        break;
    case ST_IDXASG: {
        gen_expr(m, s->c);               /* nilai */
        spush();
        gen_expr(m, s->a);               /* rax = array */
        spush();
        gen_expr(m, s->b);               /* eax = idx */
        emit("\tpopq %%r10");            /* array */
        emit("\tpopq %%rcx");            /* nilai */
        emit("\ttestl %%eax, %%eax");
        emit("\tjl .Karr");
        emit("\tcmpl (%r10), %%eax");
        emit("\tjae .Karr");
        emit("\tmovl %%eax, %%eax");     /* zero-extend idx */
        emit("\tmovslq %%ecx, %%rcx");   /* nilai sign-extend 64 */
        emit("\tmovq %%rcx, 8(%%r10,%%rax,8)");
        break;
    }
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
        if (s->a) {
            gen_expr(m, s->a);
            widen_ty((Ty)s->a->dty);
            conv_ty((Ty)s->a->dty, m->ret);
        } else {
            emit("\tmovl $0, %%eax");
        }
        emit("\tjmp .Lret%d", cur_ret_label);
        break;
    }
    case ST_PRINT:
        if (s->str) {
            emit("\tleaq cpool%d(%%rip), %%rsi", (int)s->cp);
            emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? "sn" : "s");
        } else {
            gen_expr(m, s->a);
            Ty pt = (Ty)s->a->dty;
            if (pt == T_INT || pt == T_BOOL || pt == T_BYTE || pt == T_SHORT) {
                emit("\tmovl %%eax, %%esi");
                emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? "in" : "i");
                emit("\txorl %%eax, %%eax");
            } else if (pt == T_LONG || pt == T_UINT64) {
                emit("\tmovq %%rax, %%rsi");
                emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? (pt == T_LONG ? "ldn" : "lun") : (pt == T_LONG ? "ld" : "lu"));
                emit("\txorl %%eax, %%eax");
            } else if (pt == T_UINT8 || pt == T_UINT16 || pt == T_UINT32) {
                emit("\tmovl %%eax, %%esi");
                emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? "un" : "u");
                emit("\txorl %%eax, %%eax");
            } else {
                if (pt == T_FLOAT) {
                    emit("\tmovd %%eax, %%xmm0");
                    emit("\tcvtss2sd %%xmm0, %%xmm0");
                } else {
                    emit("\tmovq %%rax, %%xmm0");
                }
                emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? (pt == T_FLOAT ? "gfn" : "gn") : (pt == T_FLOAT ? "gf" : "g"));
                emit("\tmovl $1, %%eax");
            }
        }
        emit("\tcall printf@PLT");
        break;
    case ST_EXPR:
        if (s->a) gen_expr(m, s->a);
        break;
    case ST_BLOCK:
        gen_stmts(m, s->d);
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
    Param *pp = e->meth ? e->meth->params : 0;
    for (Node *a = e->a; a; a = a->next, pp = pp ? pp->next : 0) {
        if (n >= 8) die("baris %d: maksimal 8 argumen untuk target x86-64", e->line);
        Ty at = (Ty)a->dty;
        Ty pt = pp ? (Ty)pp->ty : T_INT;
        gen_expr(m, a);
        if (pt == T_FN) w64[n] = 1;
        else {
            widen_ty(at);
            conv_ty(at, pt);
            w64[n] = is64ty(pt);
        }
        spush();
        n++;
    }
    static const char *reg32[6] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
    static const char *reg64[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    int extra = n > 6 ? n - 6 : 0;
    for (int k = 0; k < n && k < 6; k++) {
        const char *r = w64[k] ? reg64[k] : reg32[k];
        emit("\t%s %d(%%rsp), %%%s", w64[k] ? "movq" : "movl", (n - 1 - k) * 8, r);
    }
    /* argumen ke-7/8 lewat stack: simpan dulu ke r10/r11 */
    if (extra > 0) emit("\tmovq %d(%%rsp), %%r10", (n - 1 - 6) * 8);
    if (extra > 1) emit("\tmovq %d(%%rsp), %%r11", (n - 1 - 7) * 8);
    if (n > 0) { emit("\taddq $%d, %%rsp", n * 8); par ^= (n & 1); }
    if (extra > 1) { emit("\tpushq %%r11"); par ^= 1; }
    if (extra > 0) { emit("\tpushq %%r10"); par ^= 1; }
    int need = par;
    if (need) emit("\tsubq $8, %%rsp");
    const char *dot = strchr(e->str, '.');
    char cls[300];
    memcpy(cls, e->str, (size_t)(dot - e->str)); cls[dot - e->str] = 0;
    emit("\tcall %s__%s", cls, dot + 1);
    if (need) emit("\taddq $8, %%rsp");
    if (extra > 0) { emit("\taddq $%d, %%rsp", extra * 8); par ^= (extra & 1); }
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

static int gen_bin_typed(Method *m, Node *e) {
    Ty lt = (Ty)e->a->dty, rt = (Ty)e->b->dty;
    char op = e->str[0];
    int two = e->str[1] != 0;
    int isShift = !strcmp(e->str, "<<") || !strcmp(e->str, ">>") ||
                  !strcmp(e->str, ">>>");
    int isCmp = (op == '=' || op == '!') && two;
    if (!isCmp)
        isCmp = op == '<' || op == '>' || !strcmp(e->str, "<=") ||
                !strcmp(e->str, ">=");
    Ty P = isShift ? lt : ty_promote(lt, rt);

    gen_expr(m, e->a);
    widen_ty(lt);
    conv_ty(lt, P);
    spush();
    gen_expr(m, e->b);
    widen_ty(rt);
    if (!isShift) conv_ty(rt, P);
    spop("rcx");

    if (ty_is_float(P)) {
        if (P == T_FLOAT) {
            emit("\tmovd %%ecx, %%xmm0");
            emit("\tmovd %%eax, %%xmm1");
        } else {
            emit("\tmovq %%rcx, %%xmm0");
            emit("\tmovq %%rax, %%xmm1");
        }
        const char *pre = P == T_FLOAT ? "ss" : "sd";
        if (isCmp) {
            emit("\tucomi%s %%xmm1, %%xmm0", pre);
            if (op == '=') {
                emit("\tsete %%al");
                emit("\tsetnp %%dl");
                emit("\tandb %%dl, %%al");
            } else if (op == '!') {
                emit("\tsetne %%al");
                emit("\tsetp %%dl");
                emit("\torb %%dl, %%al");
            } else if (!strcmp(e->str, "<=")) {
                emit("\tsetbe %%al");
                emit("\tsetnp %%dl");
                emit("\tandb %%dl, %%al");
            } else if (!strcmp(e->str, ">=")) {
                emit("\tsetae %%al");
            } else if (op == '<') {
                emit("\tsetb %%al");
                emit("\tsetnp %%dl");
                emit("\tandb %%dl, %%al");
            } else {
                emit("\tseta %%al");
            }
            emit("\tmovzbl %%al, %%eax");
            return 0;
        }
        if (op == '+') emit("\tadd%s %%xmm1, %%xmm0", pre);
        else if (op == '-') emit("\tsub%s %%xmm1, %%xmm0", pre);
        else if (op == '*') emit("\tmul%s %%xmm1, %%xmm0", pre);
        else if (op == '/') emit("\tdiv%s %%xmm1, %%xmm0", pre);
        else if (op == '%') {
            emit("\tdiv%s %%xmm1, %%xmm0", pre);
            if (P == T_FLOAT) emit("\tcvttss2sil %%xmm0, %%eax");
            else emit("\tcvttsd2siq %%xmm0, %%rax");
            if (P == T_FLOAT) emit("\tcvtsi2ssl %%eax, %%xmm2");
            else emit("\tcvtsi2sdq %%rax, %%xmm2");
            emit("\tmul%s %%xmm1, %%xmm2", pre);
            emit("\tsub%s %%xmm2, %%xmm0", pre);
        } else die("gen_x64: operator float '%s'", e->str);
        if (P == T_FLOAT) emit("\tmovd %%xmm0, %%eax");
        else emit("\tmovq %%xmm0, %%rax");
        return is64ty(P);
    }

    if (isShift) {
        int mask = ty_width(P) - 1;
        if (mask <= 0) mask = 31;
        emit("\tmovq %%rax, %%rdx");    /* right */
        emit("\tmovq %%rcx, %%rax");    /* left */
        emit("\tmovl %%edx, %%ecx");    /* count */
        emit("\tandl $%d, %%ecx", mask);
        if (!strcmp(e->str, "<<")) emit("\tshlq %%cl, %%rax");
        else if (!strcmp(e->str, ">>")) emit("\tsarq %%cl, %%rax");
        else emit("\tshrq %%cl, %%rax");
        norm_ty(P);
        return is64ty(P);
    }
    if (isCmp) {
        int uns = ty_is_uint(P);
        emit("\tcmpq %%rax, %%rcx");
        if (op == '=') emit("\tsete %%al");
        else if (op == '!') emit("\tsetne %%al");
        else if (!strcmp(e->str, "<=")) emit(uns ? "\tsetbe %%al" : "\tsetle %%al");
        else if (!strcmp(e->str, ">=")) emit(uns ? "\tsetae %%al" : "\tsetge %%al");
        else if (op == '<') emit(uns ? "\tsetb %%al" : "\tsetl %%al");
        else emit(uns ? "\tseta %%al" : "\tsetg %%al");
        emit("\tmovzbl %%al, %%eax");
        return 0;
    }
    int uns = ty_is_uint(P);
    switch (op) {
    case '+': emit("\tleaq (%%rcx,%%rax), %%rax"); break;
    case '-': emit("\tmovq %%rcx, %%rdx"); emit("\tsubq %%rax, %%rdx"); emit("\tmovq %%rdx, %%rax"); break;
    case '*': emit("\timulq %%rcx, %%rax"); break;
    case '/':
        emit("\tmovq %%rax, %%r10");
        emit("\tmovq %%rcx, %%rax");
        if (uns) { emit("\txorl %%edx, %%edx"); emit("\tdivq %%r10"); }
        else { emit("\tcqto"); emit("\tidivq %%r10"); }
        break;
    case '%':
        emit("\tmovq %%rax, %%r10");
        emit("\tmovq %%rcx, %%rax");
        if (uns) { emit("\txorl %%edx, %%edx"); emit("\tdivq %%r10"); }
        else { emit("\tcqto"); emit("\tidivq %%r10"); }
        emit("\tmovq %%rdx, %%rax");
        break;
    case '&': emit("\tmovq %%rcx, %%rax"); emit("\tandq %%rcx, %%rax"); break;
    case '|': emit("\tmovq %%rcx, %%rax"); emit("\torq %%rcx, %%rax"); break;
    case '^': emit("\tmovq %%rcx, %%rax"); emit("\txorq %%rcx, %%rax"); break;
    default: die("gen_x64: operator '%s'", e->str);
    }
    norm_ty(P);
    return is64ty(P);
}

static int gen_expr(Method *m, Node *e) {
    switch (e->kind) {
    case EX_INT: {
        Ty t = (Ty)e->dty;
        if (t == T_INT) {
            emit("\tmovl $%ld, %%eax", e->ival);
        } else if (t == T_FLOAT) {
            union { float f; uint32_t u; } cv;
            cv.f = (float)e->fval;
            emit("\tmovl $%lu, %%eax", (unsigned long)cv.u);
        } else if (t == T_DOUBLE) {
            union { double d; uint64_t u; } cv;
            cv.d = e->fval;
            emit("\tmovabsq $%llu, %%rax", (unsigned long long)cv.u);
        } else if (t == T_UINT32 || t == T_UINT8 || t == T_UINT16 || t == T_BYTE || t == T_SHORT) {
            emit("\tmovl $%lu, %%eax", (unsigned long)e->ival);
        } else {
            emit("\tmovabsq $%ld, %%rax", e->ival);
        }
        break;
    }
    case EX_BOOL:
        emit("\tmovl $%d, %%eax", (int)e->ival);
        break;
    case EX_VAR:
        if (is64ty((Ty)e->dty)) {
            emit("\tmovq -%ld(%%rbp), %%rax", (e->ival + 1) * 8);
            return 1;
        }
        emit("\tmovl -%ld(%%rbp), %%eax", (e->ival + 1) * 8);
        break;
    case EX_CAST: {
        Ty from = (Ty)e->a->dty, to = (Ty)e->dty;
        gen_expr(m, e->a);
        widen_ty(from);
        conv_ty(from, to);
        return is64ty(to);
    }
    case EX_UN: {
        Ty t = (Ty)e->dty;
        gen_expr(m, e->a);
        if (e->str[0] == '!') { emit("\txorl $1, %%eax"); break; }
        if (ty_is_float(t)) {
            if (t == T_FLOAT) emit("\tbtcq $31, %%rax");
            else emit("\tbtcq $63, %%rax");
            return 1;
        }
        if (!is64ty(t)) {
            if (e->str[0] == '-') emit("\tnegl %%eax");
            else emit("\tnotl %%eax");
            break;
        }
        if (e->str[0] == '-') emit("\tnegq %%rax");
        else emit("\tnotq %%rax");
        norm_ty(t);
        return 1;
    }
    case EX_CALL:
        x64_call(m, e);
        return is64ty((Ty)e->dty);       /* int[] / fn → %rax; int → %eax */
    case EX_ARROW: {
        /* closure = blok 64-bit: [0]=kode [1]=ncapt [2]=nuser [3+j]=capture */
        Method *hm = (Method *)e->cp;
        emit("\tmovq $%ld, %%rdi", 8 * (3 + (long)hm->ncapt));
        emit("\tcall kopalloc");        /* arena bump: rdi = byte */
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
    case EX_NEWARR: {
        gen_expr(m, e->a);               /* eax = n */
        emit("\ttestl %%eax, %%eax");
        emit("\tjl .Karr");             /* ukuran salah */
        emit("\tmovl %%eax, %%r10d");   /* simpan n */
        emit("\tshlq $3, %%r10");       /* 8*n */
        emit("\taddq $8, %%r10");       /* + header len */
        emit("\tmovq %%r10, %%rdi");
        emit("\tcall kopalloc");
        emit("\tmovq %%r10, (%rax)");   /* len */
        return 1;
    }
    case EX_IDX: {
        gen_expr(m, e->a);               /* rax = array */
        spush();
        gen_expr(m, e->b);               /* eax = idx */
        emit("\tpopq %%r10");           /* array */
        emit("\tcmpl (%r10), %%eax");   /* len */
        emit("\tjae .Karr");            /* unsigned: negatif tertangkap */
        emit("\tmovl %%eax, %%eax");
        emit("\tcltq");
        emit("\tmovq (%%r10,%%rax,8), %%rax");   /* elemen 8-byte */
        return 0;
    }
    case EX_LEN:
        gen_expr(m, e->a);
        emit("\tmovl (%rax), %%eax");
        return 0;
    case EX_NET: {                       /* syscall Linux langsung (tanpa libc) */
        int ln = netlbl++;                /* label unik per kemunculan */
        /* 0=socket 1=connect 2=rdb 3=wrb 4=close 5=bytes */
        if (e->ival == 5) {              /* Net.bytes("literal") → int[]       */
            int c = (int)e->cp;
            /* 1) strlen(s) → rcx */
            emit("\tleaq cpool%d(%%rip), %%rsi", c);
            emit("\txorl %%ecx, %%ecx");
            emit(".LNX%da:", ln);
            emit("\tcmpb $0, (%%rsi,%%rcx)");
            emit("\tje .LNX%db", ln);
            emit("\taddq $1, %%rcx");
            emit("\tjmp .LNX%da", ln);
            emit(".LNX%db:", ln);
            /* 2) alokasi arena 8*len + 8 */
            emit("\tmovq %%rcx, %%rdi");
            emit("\tshlq $3, %%rdi");
            emit("\taddq $8, %%rdi");
            emit("\tcall kopalloc");
            emit("\tmovq %%rcx, (%%rax)");               /* len */
            emit("\tmovq %%rax, %%rbx");                 /* hasil (rbx callee-saved) */
            /* 3) salin byte-as-int: rbx+8+8i = s[i] */
            emit("\tleaq cpool%d(%%rip), %%rsi", c);
            emit("\txorl %%ecx, %%ecx");
            emit(".LNX%dc:", ln);
            emit("\tcmpq %%rcx, (%%rax)");
            emit("\tjbe .LNX%dd", ln);                    /* i >= len → selesai */
            emit("\tmovzbl (%%rsi,%%rcx), %%edx");
            emit("\tshlq $3, %%rcx");
            emit("\tmovq %%rdx, 8(%%rbx,%%rcx)");
            emit("\tshrq $3, %%rcx");
            emit("\taddq $1, %%rcx");
            emit("\tjmp .LNX%dc", ln);
            emit(".LNX%dd:", ln);
            emit("\tmovq %%rbx, %%rax");
            return 1;
        }
        if (e->ival == 6) {              /* Net.openr("literal") → fd          */
            int c = (int)e->cp;
            emit("\tleaq cpool%d(%%rip), %%rsi", c);
            emit("\tmovq %%rsi, %%rdx");                 /* strlen */
            emit("\txorl %%eax, %%eax");
            emit(".LNO%da:", ln);
            emit("\tcmpb $0, (%%rdx)");
            emit("\tje .LNO%db", ln);
            emit("\taddq $1, %%rdx");
            emit("\tjmp .LNO%da", ln);
            emit(".LNO%db:", ln);
            /* openat(AT_FDCWD=-100, path, O_RDONLY) */
            emit("\tmovl $-100, %%edi");
            emit("\txorl %%edx, %%edx");                 /* flags = O_RDONLY */
            emit("\tmovl $257, %%eax");                  /* __NR_openat */
            emit("\tsyscall");
            return 0;                                    /* fd | -errno */
        }
        if (e->ival == 7 || e->ival == 8) {
            for (Node *aa = e->a; aa; aa = aa->next) {
                gen_expr(m, aa);
                spush();
            }
        }
        if (e->ival == 7) {              /* recvb(fd, buf) → n | -1            */
            /* stack: [0]=buf (64-bit), [8]=fd; tmp via .comm kop_iobuf */
            emit("\tmovq (%%rsp), %%r10");               /* buf */
            emit("\tmovl (%%r10), %%r8d");               /* len */
            emit("\ttestl %%r8d, %%r8d");
            emit("\tjle .LRX%dz", ln);
            emit("\txorl %%r9d, %%r9d");                 /* got */
            emit("\tleaq kop_iobuf(%%rip), %%r11");      /* tmp base */
            emit(".LRX%dl:", ln);
            emit("\tcmpl %%r8d, %%r9d");
            emit("\tjge .LRX%df", ln);
            emit("\tmovl %%r8d, %%eax");
            emit("\tsubl %%r9d, %%eax");                 /* sisa */
            emit("\tcmpl $240, %%eax");
            emit("\tjle .LRX%dc", ln);
            emit("\tmovl $240, %%eax");
            emit(".LRX%dc:", ln);                        /* want = eax */
            emit("\tmovl %%eax, %%edx");                 /* want untuk syscall */
            /* salin ke tmp: tmp[k] = buf[1 + got + k] */
            emit("\txorl %%ecx, %%ecx");
            emit(".LRX%dcs:", ln);
            emit("\tcmpl %%eax, %%ecx");
            emit("\tjge .LRX%dcf", ln);
            emit("\tmovl %%r9d, %%eax");
            emit("\taddl %%ecx, %%eax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq 8(%%r10,%%rax), %%rax");
            emit("\tmovb %%al, (%%r11,%%rcx)");
            emit("\tincl %%ecx");
            emit("\tjmp .LRX%dcs", ln);
            emit(".LRX%dcf:", ln);
            emit("\tmovl 8(%%rsp), %%edi");              /* fd */
            emit("\tmovq %%r11, %%rsi");                 /* tmp */
            emit("\txorl %%eax, %%eax");                 /* __NR_read = 0 */
            emit("\tsyscall");
            emit("\ttestq %%rax, %%rax");
            emit("\tjs .LRX%dry", ln);                   /* galat keras */
            emit("\tje .LRX%de", ln);                    /* EOF */
            /* salin kembali: buf[1 + got + k] = tmp[k], k < rax */
            emit("\txorl %%ecx, %%ecx");
            emit(".LRX%drs:", ln);
            emit("\tcmpq %%rax, %%rcx");
            emit("\tjge .LRX%drf", ln);
            emit("\tmovzbq (%%r11,%%rcx), %%rdx");
            emit("\tmovl %%r9d, %%eax");
            emit("\taddl %%ecx, %%eax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq %%rdx, 8(%%r10,%%rax)");
            emit("\tincl %%ecx");
            emit("\tjmp .LRX%drs", ln);
            emit(".LRX%drf:", ln);
            emit("\taddl %%eax, %%r9d");                 /* got += nread */
            emit("\tjmp .LRX%dl", ln);
            emit(".LRX%dry:", ln);                       /* galat → -1 */
            emit("\taddq $16, %%rsp");
            emit("\tmovl $-1, %%eax");
            emit("\tjmp .LRX%dh", ln);
            emit(".LRX%de:", ln);                        /* EOF → got */
            emit("\tjmp .LRX%df", ln);
            emit(".LRX%df:", ln);                        /* penuh → got */
            emit("\tmovl %%r9d, %%eax");
            emit("\taddq $16, %%rsp");
            emit("\tjmp .LRX%dh", ln);
            emit(".LRX%dz:", ln);                        /* len<=0 → 0 */
            emit("\txorl %%eax, %%eax");
            emit("\taddq $16, %%rsp");
            emit(".LRX%dh:", ln);
            return 0;
        }
        if (e->ival == 8) {              /* sendb(fd, buf) → 0 | -1            */
            emit("\tmovq (%%rsp), %%r10");               /* buf */
            emit("\tmovl (%%r10), %%r8d");               /* len */
            emit("\ttestl %%r8d, %%r8d");
            emit("\tjle .LSX%df", ln);
            emit("\txorl %%r9d, %%r9d");                 /* terkirim */
            emit("\tleaq kop_iobuf(%%rip), %%r11");      /* tmp base */
            emit(".LSX%dl:", ln);
            emit("\tcmpl %%r8d, %%r9d");
            emit("\tjge .LSX%df", ln);
            emit("\tmovl %%r8d, %%eax");
            emit("\tsubl %%r9d, %%eax");
            emit("\tcmpl $240, %%eax");
            emit("\tjle .LSX%dc", ln);
            emit("\tmovl $240, %%eax");
            emit(".LSX%dc:", ln);                        /* want = eax */
            /* salin ke tmp: tmp[k] = buf[1 + terkirim + k] */
            emit("\tmovl %%eax, %%edx");                 /* want untuk syscall */
            emit("\txorl %%ecx, %%ecx");
            emit(".LSX%dcs:", ln);
            emit("\tcmpl %%edx, %%ecx");
            emit("\tjge .LSX%dcf", ln);
            emit("\tmovl %%r9d, %%eax");
            emit("\taddl %%ecx, %%eax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq 8(%%r10,%%rax), %%rax");
            emit("\tmovb %%al, (%%r11,%%rcx)");
            emit("\tincl %%ecx");
            emit("\tjmp .LSX%dcs", ln);
            emit(".LSX%dcf:", ln);
            emit("\tmovl 8(%%rsp), %%edi");              /* fd */
            emit("\tmovq %%r11, %%rsi");
            emit("\tmovl %%edx, %%edx");                 /* want */
            emit("\tmovl $1, %%eax");                    /* __NR_write */
            emit("\tsyscall");
            emit("\ttestq %%rax, %%rax");
            emit("\tjs .LSX%de", ln);
            emit("\taddl %%eax, %%r9d");                 /* penuh/parsial */
            emit("\tjmp .LSX%dl", ln);
            emit(".LSX%de:", ln);                        /* galat → -1 */
            emit("\taddq $16, %%rsp");
            emit("\tmovl $-1, %%eax");
            emit("\tjmp .LSX%dh", ln);
            emit(".LSX%df:", ln);                        /* selesai → 0 */
            emit("\txorl %%eax, %%eax");
            emit("\taddq $16, %%rsp");
            emit(".LSX%dh:", ln);
            return 0;
        }
        if (e->ival >= 9 && e->ival <= 12) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                spush();
            }
            emit("\tsubq $32, %%rsp");                  /* sa + addrlen */
            emit("\tmovq $0, (%%rsp)");
            emit("\tmovq $0, 8(%%rsp)");
            emit("\tmovq $0, 16(%%rsp)");
            emit("\tmovq $0, 24(%%rsp)");
            if (e->ival == 9) {                        /* bind(fd, ip, port) */
                emit("\tmovl 40(%%rsp), %%r8d");       /* ip */
                emit("\tmovl 32(%%rsp), %%r9d");       /* port */
                emit("\tmovl %%r9d, %%eax");
                emit("\tshrl $8, %%eax");
                emit("\tmovb %%al, 2(%%rsp)");
                emit("\tmovl %%r9d, %%eax");
                emit("\tmovb %%al, 3(%%rsp)");
                emit("\tmovl $4, %%ecx");
                emit(".LSB%da:", ln);
                emit("\tmovl %%r8d, %%eax");
                emit("\tshrl $24, %%eax");
                emit("\tmovb %%al, (%%rsp,%%rcx)");
                emit("\tshll $8, %%r8d");
                emit("\tincl %%ecx");
                emit("\tcmpl $8, %%ecx");
                emit("\tjne .LSB%da", ln);
                emit("\tmovw $2, (%%rsp)");
                emit("\tmovl 48(%%rsp), %%edi");      /* fd */
                emit("\tmovq %%rsp, %%rsi");
                emit("\tmovl $16, %%edx");
                emit("\tmovl $49, %%eax");            /* __NR_bind */
                emit("\tsyscall");
                emit("\taddq $32, %%rsp");
                emit("\taddq $24, %%rsp");
                return 0;
            }
            if (e->ival == 12) {                       /* shutdown(fd, how) */
                emit("\tmovl 40(%%rsp), %%edi");
                emit("\tmovl 32(%%rsp), %%esi");
                emit("\tmovl $48, %%eax");            /* __NR_shutdown */
                emit("\tsyscall");
                emit("\taddq $32, %%rsp");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 10) {                       /* sendto(fd,ip,port,buf) */
                emit("\tmovq 32(%%rsp), %%r10");      /* buf */
                emit("\tmovl 48(%%rsp), %%r8d");      /* ip */
                emit("\tmovl 40(%%rsp), %%r9d");      /* port */
                emit("\tmovl %%r9d, %%eax");
                emit("\tshrl $8, %%eax");
                emit("\tmovb %%al, 2(%%rsp)");
                emit("\tmovl %%r9d, %%eax");
                emit("\tmovb %%al, 3(%%rsp)");
                emit("\tmovl $4, %%ecx");
                emit(".LSD%da:", ln);
                emit("\tmovl %%r8d, %%eax");
                emit("\tshrl $24, %%eax");
                emit("\tmovb %%al, (%%rsp,%%rcx)");
                emit("\tshll $8, %%r8d");
                emit("\tincl %%ecx");
                emit("\tcmpl $8, %%ecx");
                emit("\tjne .LSD%da", ln);
                emit("\tmovw $2, (%%rsp)");
                /* pak ke kop_iobuf, cap 240 */
                emit("\tmovl (%%r10), %%r11d");      /* len */
                emit("\tcmpl $240, %%r11d");
                emit("\tjle .LSD%dc", ln);
                emit("\tmovl $240, %%r11d");
                emit(".LSD%dc:", ln);
                emit("\tleaq kop_iobuf(%%rip), %%rbx");
                emit("\txorl %%ecx, %%ecx");
                emit(".LSD%dk:", ln);
                emit("\tcmpl %%r11d, %%ecx");
                emit("\tjge .LSD%df", ln);
                emit("\tmovl %%ecx, %%eax");
                emit("\tshlq $3, %%rax");
                emit("\tmovq 8(%%r10,%%rax), %%rax");
                emit("\tmovb %%al, (%%rbx,%%rcx)");
                emit("\tincl %%ecx");
                emit("\tjmp .LSD%dk", ln);
                emit(".LSD%df:", ln);
                emit("\tmovl 56(%%rsp), %%edi");      /* fd */
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl %%r11d, %%edx");
                emit("\txorl %%r10d, %%r10d");
                emit("\tmovq %%rsp, %%r8");
                emit("\tmovl $16, %%r9d");
                emit("\tmovl $44, %%eax");            /* __NR_sendto */
                emit("\tsyscall");
                emit("\taddq $32, %%rsp");
                emit("\taddq $32, %%rsp");
                return 0;
            }
            if (e->ival == 11) {                       /* recvfrom(fd,buf,from) */
                emit("\tmovl $16, 16(%%rsp)");        /* addrlen */
                emit("\tleaq kop_iobuf(%%rip), %%rbx");
                emit("\tmovl 48(%%rsp), %%edi");      /* fd */
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl $240, %%edx");
                emit("\txorl %%r10d, %%r10d");
                emit("\tmovq %%rsp, %%r8");
                emit("\tleaq 16(%%rsp), %%r9");
                emit("\tmovl $45, %%eax");            /* __NR_recvfrom */
                emit("\tsyscall");
                emit("\ttestq %%rax, %%rax");
                emit("\tjs .LSF%de", ln);
                emit("\tmovq 40(%%rsp), %%r10");      /* buf */
                emit("\txorl %%ecx, %%ecx");
                emit(".LSF%dk:", ln);
                emit("\tcmpq %%rax, %%rcx");
                emit("\tjge .LSF%df", ln);
                emit("\tmovzbl (%%rbx,%%rcx), %%edx");
                emit("\tmovl %%ecx, %%r11d");
                emit("\tshlq $3, %%r11");
                emit("\tmovq %%rdx, 8(%%r10,%%r11)");
                emit("\tincl %%ecx");
                emit("\tjmp .LSF%dk", ln);
                emit(".LSF%df:", ln);
                emit("\tmovq 32(%%rsp), %%r11");      /* from[] */
                emit("\tmovzbl 4(%%rsp), %%edx");
                emit("\tshll $24, %%edx");
                emit("\tmovzbl 5(%%rsp), %%esi");
                emit("\tshll $16, %%esi");
                emit("\torl %%esi, %%edx");
                emit("\tmovzbl 6(%%rsp), %%esi");
                emit("\tshll $8, %%esi");
                emit("\torl %%esi, %%edx");
                emit("\tmovzbl 7(%%rsp), %%esi");
                emit("\torl %%esi, %%edx");
                emit("\tmovzbl 2(%%rsp), %%esi");
                emit("\tshll $8, %%esi");
                emit("\tmovzbl 3(%%rsp), %%edi");
                emit("\torl %%edi, %%esi");
                emit("\tmovq %%rdx, 8(%%r11)");
                emit("\tmovq %%rsi, 16(%%r11)");
                emit(".LSF%de:", ln);
                emit("\taddq $32, %%rsp");
                emit("\taddq $24, %%rsp");
                return 0;
            }
        }
        if (e->ival >= 13 && e->ival <= 21) {
            int na = 0;
            for (Node *aa = e->a; aa; aa = aa->next) { gen_expr(m, aa); spush(); na++; }
            if (e->ival == 13) {                       /* openb(path,flags,mode) */
                emit("\tmovq 16(%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovl $-100, %%edi");
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl 8(%%rsp), %%edx");
                emit("\tmovl (%%rsp), %%r10d");
                emit("\tmovl $257, %%eax");
                emit("\tsyscall");
                emit("\taddq $24, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 14) {                       /* mkdirb(path) */
                emit("\tmovq (%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovl $-100, %%edi");
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl $493, %%edx");
                emit("\tmovl $258, %%eax");
                emit("\tsyscall");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 15) {                       /* unlinkb(path) */
                emit("\tmovq (%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovl $-100, %%edi");
                emit("\tmovq %%rbx, %%rsi");
                emit("\txorl %%edx, %%edx");
                emit("\tmovl $263, %%eax");
                emit("\tsyscall");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 16) {                       /* renameb(old,new) */
                emit("\tmovq 8(%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovq (%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'b', 2048);
                emit("\tmovl $-100, %%edi");
                emit("\tleaq -2048(%%rbx), %%rsi");
                emit("\tmovl $-100, %%edx");
                emit("\tmovq %%rbx, %%r10");
                emit("\tmovl $264, %%eax");
                emit("\tsyscall");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 17) {                       /* seek(fd,off,whence) */
                emit("\tmovl 16(%%rsp), %%edi");
                emit("\tmovslq 8(%%rsp), %%rsi");
                emit("\tmovl (%%rsp), %%edx");
                emit("\tmovl $8, %%eax");
                emit("\tsyscall");
                emit("\taddq $24, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 18) {                       /* time() */
                emit("\tsubq $16, %%rsp");
                emit("\txorl %%edi, %%edi");
                emit("\tmovq %%rsp, %%rsi");
                emit("\tmovl $228, %%eax");
                emit("\tsyscall");
                emit("\tmovq (%%rsp), %%rax");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 19) {                       /* mono() ms */
                emit("\tsubq $16, %%rsp");
                emit("\tmovl $1, %%edi");
                emit("\tmovq %%rsp, %%rsi");
                emit("\tmovl $228, %%eax");
                emit("\tsyscall");
                emit("\tmovq (%%rsp), %%r8");
                emit("\timulq $1000, %%r8, %%r8");
                emit("\tmovq 8(%%rsp), %%rax");
                emit("\txorl %%edx, %%edx");
                emit("\tmovq $1000000, %%rcx");
                emit("\tdivq %%rcx");
                emit("\taddq %%r8, %%rax");
                emit("\taddq $16, %%rsp");
                return 1;
            }
            if (e->ival == 20) {                       /* sleep(ms) */
                emit("\tmovq (%%rsp), %%rax");
                emit("\tmovq $1000, %%rcx");
                emit("\txorl %%edx, %%edx");
                emit("\tdivq %%rcx");
                emit("\tsubq $16, %%rsp");
                emit("\tmovq %%rax, (%%rsp)");
                emit("\timulq $1000000, %%rdx, %%rdx");
                emit("\tmovq %%rdx, 8(%%rsp)");
                emit("\tmovq %%rsp, %%rdi");
                emit("\txorl %%esi, %%esi");
                emit("\tmovl $35, %%eax");
                emit("\tsyscall");
                emit("\taddq $16, %%rsp");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                emit("\txorl %%eax, %%eax");
                return 0;
            }
            if (e->ival == 21) {                       /* exit(code) */
                emit("\tmovl (%%rsp), %%edi");
                emit("\tmovl $60, %%eax");
                emit("\tsyscall");
                return 0;
            }
        }
        int n = 0;
        for (Node *aa = e->a; aa; aa = aa->next) {
            gen_expr(m, aa);
            spush();
            n++;
        }
        if (e->ival == 0) {                              /* socket(domain, type) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1 - 0) * 8);
            emit("\tmovl %d(%%rsp), %%esi", (n - 1 - 1) * 8);
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\txorl %%edx, %%edx");                /* proto */
            emit("\tmovl $41, %%eax");                  /* __NR_socket */
            emit("\tsyscall");
        } else if (e->ival == 1) {                       /* connect(fd, ipBE, port) */
            /* sockaddr_in 16 byte di stack, lalu syscall */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1 - 0) * 8);   /* fd */
            emit("\tmovl %d(%%rsp), %%esi", (n - 1 - 1) * 8);   /* ipBE */
            emit("\tmovl %d(%%rsp), %%edx", (n - 1 - 2) * 8);   /* port */
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tpushq $0");
            emit("\tpushq $0");                         /* 16 byte nol */
            emit("\tandq $-16, %%rsp");
            emit("\tmovb $2, (%%rsp)");                 /* AF_INET */
            emit("\tmovl %%edx, %%eax");
            emit("\tshrl $8, %%eax");
            emit("\tmovb %%al, 2(%%rsp)");              /* port hi */
            emit("\tmovb %%dl, 3(%%rsp)");              /* port lo */
            emit("\tmovl %%esi, %%eax");
            emit("\tshrl $24, %%eax");
            emit("\tmovb %%al, 4(%%rsp)");
            emit("\tmovl %%esi, %%eax");
            emit("\tshrl $16, %%eax");
            emit("\tmovb %%al, 5(%%rsp)");
            emit("\tmovl %%esi, %%eax");
            emit("\tshrl $8, %%eax");
            emit("\tmovb %%al, 6(%%rsp)");
            emit("\tmovb %%sil, 7(%%rsp)");
            emit("\tmovq %%rsp, %%rsi");
            emit("\tmovl $16, %%edx");
            emit("\tmovl $42, %%eax");                  /* __NR_connect */
            emit("\tsyscall");
            emit("\taddq $16, %%rsp");
        } else if (e->ival == 2) {                       /* rdb(fd) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1) * 8);
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tpushq $0");                         /* buffer 1B */
            emit("\tpushq $0");
            emit("\tmovq %%rsp, %%rsi");
            emit("\tmovl $1, %%edx");
            emit("\txorl %%eax, %%eax");                /* __NR_read */
            emit("\tsyscall");
            emit("\tcmpq $1, %%rax");
            emit("\tjne 1f");
            emit("\tmovzbq (%%rsp), %%rax");            /* byte 0..255 */
            emit("\tjmp 2f");
            emit("1:");
            emit("\tmovl $-1, %%eax");                  /* EOF / error */
            emit("2:");
            emit("\taddq $16, %%rsp");
        } else if (e->ival == 3) {                       /* wrb(fd, b) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1 - 0) * 8);   /* fd */
            emit("\tmovl %d(%%rsp), %%esi", (n - 1 - 1) * 8);   /* byte */
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tpushq $0");
            emit("\tpushq $0");
            emit("\tmovb %%sil, (%%rsp)");
            emit("\tmovq %%rsp, %%rsi");
            emit("\tmovl $1, %%edx");
            emit("\tmovl $1, %%eax");                   /* __NR_write */
            emit("\tsyscall");
            emit("\taddq $16, %%rsp");
            emit("\tdecl %%eax");
            emit("\tjne 1f");
            emit("\txorl %%eax, %%eax");
            emit("\tjmp 2f");
            emit("1:");
            emit("2:");
        } else if (e->ival == 4) {                       /* close(fd) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1) * 8);
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tmovl $3, %%eax");                   /* __NR_close */
            emit("\tsyscall");
        }
        return 0;
    }
    case EX_ENVLOAD:
        emit("\tmovq -8(%%rbp), %%rax");  /* slot 0 = param $env */
        if (is64ty((Ty)e->dty)) {
            emit("\tmovq %ld(%%rax), %%rax", 24 + 8 * (long)e->ival);
            return 1;
        }
        emit("\tmovl %ld(%%rax), %%eax", 24 + 8 * (long)e->ival);
        break;
    case EX_BIN: {
        Ty bl = (Ty)e->a->dty, br = (Ty)e->b->dty, bres = (Ty)e->dty;
        if (!((bl == T_INT || bl == T_BOOL) && (br == T_INT || br == T_BOOL) &&
              (bres == T_INT || bres == T_BOOL))) {
            return gen_bin_typed(m, e);
        }
        char op = e->str[0];
        if ((op == '&' || op == '|') && e->str[1] != 0) { gen_bool_chain(m, e); break; }
        gen_expr(m, e->a);
        spush();
        gen_expr(m, e->b);
        spop("rcx");                       /* konvensi: ecx=left, eax=right */
        /* operator 2-karakter dulu supaya tidak tabrak case '<' / '>' */
        if (!strcmp(e->str, "<<") || !strcmp(e->str, ">>") ||
            !strcmp(e->str, ">>>") || op == '-' || op == '/' || op == '%') {
            emit("\txchgl %%eax, %%ecx");  /* xchg: eax=left, ecx(count) untuk shift/idiv */
        }
        if (!strcmp(e->str, "<<")) emit("\tshll %%cl, %%eax");
        else if (!strcmp(e->str, ">>")) emit("\tsarl %%cl, %%eax");
        else if (!strcmp(e->str, ">>>")) emit("\tshrl %%cl, %%eax");
        else switch (op) {
        case '+': emit("\taddl %%ecx, %%eax"); break;
        case '-': emit("\tsubl %%ecx, %%eax"); break;   /* eax = left - right */
        case '*': emit("\timull %%ecx, %%eax"); break;
        case '/':
            emit("\tcltd");
            emit("\tidivl %%ecx");
            break;
        case '%':
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
            else if (op == '&') { emit("\tandl %%ecx, %%eax"); break; }  /* bitwise (len 1) */
            else if (op == '|') { emit("\torl %%ecx, %%eax"); break; }
            else if (op == '^') { emit("\txorl %%ecx, %%eax"); break; }
            else die("gen_x64: operator '%s'", e->str);
            emit("\tcmpl %%eax, %%ecx");   /* bandingkan eksplisit left vs right */
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
        else if (c == '\r') { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c == 0)    { dst[j++] = '\\'; dst[j++] = '0'; }
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
                if (i >= 6) {
                    emit("\tmovq %d(%%rbp), %%rax", 16 + 8 * (i - 6));
                    emit("\tmovq %%rax, -%ld(%%rbp)", (i + 1) * 8);
                    continue;
                }
                if (is64ty((Ty)p->ty))   /* param lambda/array disalin 64-bit */
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
            /* frame main sendiri: call kopalloc-era malloc mengubah rsp/return,
               dan sp harus selaras 16 LEWAT call (ABI SysV) */
            emit("\tpushq %%rbp");
            emit("\tmovq %%rsp, %%rbp");
            emit("\tandq $-16, %%rsp");
            emit("\tmovl $8388608, %%edi");     /* 1<<23 bytes arena */
            emit("\tcall malloc@PLT");
            emit("\tmovq %%rax, kop_arena_base(%%rip)");
            emit("\tmovl $0, %%eax");           /* arg main diabaikan */
            emit("\tcall %s__%s", m->cls, m->name);
            emit("\tmovl $0, %%eax");           /* exit status 0 */
            emit("\tmovq %%rbp, %%rsp");
            emit("\tpopq %%rbp");
            emit("\tret");
            break;
        }

    /* stub mati: jumlah argumen lambda tidak cocok (dilompati dari jne .Klam).
       push+andq menjamin rsp selaras 16 apa pun parity saat masuk. */
    emit(".Karr:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tandq $-16, %%rsp");
    emit("\tleaq .Lmsg_arr(%%rip), %%rdi");
    emit("\tcall printf@PLT");
    emit("\tmovl $1, %%edi");
    emit("\tcall exit@PLT");

    emit(".Koom:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tandq $-16, %%rsp");
    emit("\tleaq .Lmsg_oom(%%rip), %%rdi");
    emit("\tcall printf@PLT");
    emit("\tmovl $1, %%edi");
    emit("\tcall exit@PLT");

    emit(".text");
    emit("\t.globl kopalloc");
    emit("kopalloc:");
    emit("\tmovq kop_arena_used(%%rip), %%rax");
    emit("\taddq %%rdi, %%rax");
    emit("\tcmpq $8388608, %%rax");
    emit("\tjge .Koom");
    emit("\tmovq %%rax, kop_arena_used(%%rip)");
    emit("\tsubq %%rdi, %%rax");
    emit("\taddq kop_arena_base(%%rip), %%rax");
    emit("\tret");

    emit("\t.section .data");
    emit("\t.align 8");
    emit("kop_arena_base: .quad 0");
    emit("kop_arena_used: .quad 0");
    emit("\t.comm kop_iobuf, 256, 32");    /* penyangga syscall rekursi builtin */
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
    emit(".Lfmt_ld: .asciz \"%s\"", "%ld");
    emit(".Lfmt_ldn: .asciz \"%s\"", "%ld\\n");
    emit(".Lfmt_lu: .asciz \"%s\"", "%lu");
    emit(".Lfmt_lun: .asciz \"%s\"", "%lu\\n");
    emit(".Lfmt_u: .asciz \"%s\"", "%u");
    emit(".Lfmt_un: .asciz \"%s\"", "%u\\n");
    emit(".Lfmt_g: .asciz \"%s\"", "%.15g");
    emit(".Lfmt_gn: .asciz \"%s\"", "%.15g\\n");
    emit(".Lfmt_gf: .asciz \"%s\"", "%.9g");
    emit(".Lfmt_gfn: .asciz \"%s\"", "%.9g\\n");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    emit(".Lmsg_arr: .asciz \"%s\"", "kopi: array tidak valid (indeks/ukuran salah)\\n");
    emit(".Lmsg_oom: .asciz \"%s\"", "kopi: arena memori habis\\n");
    for (int i = 0; i < prog.ncp; i++) {
        char esc[1200];
        asm_escape(prog.cpool[i], esc, sizeof esc);
        emit("cpool%d: .asciz \"%s\"", i, esc);
    }
    fclose(out);
}
