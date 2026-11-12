/* Backend x86-64 (AT&T syntax): assembly untuk gas, dirakit di PC dengan gcc. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static FILE *out;
static int nlab;
static int brk_lab[64], cont_lab[64], brk_sp;
/* v1.20: malloc{} — slot penanda arena aktif + rg_sp saat loop masuk */
static int rg_slot[64], rg_sp, loop_rg[64];
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
static int arrlbl;      /* label zero-init array unik */
static int cur_ret_label;

/* 32-bit int/bool vs 64-bit (pointer / numerik baru / float bits) */
static int is64ty(Ty t) {
    return t == T_FN || ty_is_arr(t) || t == T_LONG || t == T_UINT64 ||
           t == T_UINT8 || t == T_UINT16 || t == T_UINT32 ||
           t == T_BYTE || t == T_SHORT || t == T_STR ||
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

/* v1.21: masuk arena blok — alokasi arena baru, simpan handle */
static void emit_region_enter(int slot, long bytes) {
    emit("\tmovabsq\t$%ld, %%rdi", bytes);
    emit("\tcall\tkop_arena_enter");
    emit("\tmovq\t%%rax, -%ld(%%rbp)", (long)(slot + 1) * 8);
}

/* keluar arena blok — bebas arena dalam, pulihkan arena luar (rax aman) */
static void emit_region_leave(int slot) {
    emit("\tmovq\t-%ld(%%rbp), %%rdi", (long)(slot + 1) * 8);
    emit("\tcall\tkop_arena_leave");
}

static void emit_loop_exit_resets(void) {
    int base = brk_sp > 0 ? loop_rg[brk_sp - 1] : 0;
    for (int i = rg_sp - 1; i >= base; i--) emit_region_leave(rg_slot[i]);
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
        /* nilai slot: 32-bit (int dsb.) atau 64-bit (long/String/objek) */
        Ty vt = (Ty)s->c->dty;
        int v64 = ty_width(vt) == 64 || vt == T_ARI || vt == T_STR || vt == T_FN;
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
        if (!v64) emit("\tmovslq %%ecx, %%rcx");   /* nilai sign-extend 64 */
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
        loop_rg[brk_sp] = rg_sp;
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
    case ST_DO: {
        int ltop = nlab++, lcont = nlab++, lend = nlab++;
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        loop_rg[brk_sp] = rg_sp;
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = lcont; brk_sp++;
        emit(".L%d:", ltop);
        gen_stmts(m, s->b);
        emit(".L%d:", lcont);
        gen_expr(m, s->a);
        emit("\ttestl %%eax, %%eax");
        emit("\tjne .L%d", ltop);
        emit(".L%d:", lend);
        brk_sp--; brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_FOR: {
        if (s->a) gen_stmt(m, s->a);
        int ltop = nlab++, lend = nlab++, lcont = nlab++;
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        loop_rg[brk_sp] = rg_sp;
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
    case ST_BRK: emit_loop_exit_resets(); emit("\tjmp .L%d", brk_lab[brk_sp - 1]); break;
    case ST_CONT: emit_loop_exit_resets(); emit("\tjmp .L%d", cont_lab[brk_sp - 1]); break;
    case ST_RETURN: {
        if (s->a) {
            gen_expr(m, s->a);
            widen_ty((Ty)s->a->dty);
            conv_ty((Ty)s->a->dty, m->ret);
        } else {
            emit("\tmovl $0, %%eax");
        }
        for (int i = rg_sp - 1; i >= 0; i--) emit_region_leave(rg_slot[i]);
        emit("\tjmp .Lret%d", cur_ret_label);
        break;
    }
    case ST_PRINT:
        if (s->str) {
            emit("\tleaq cpool%d(%%rip), %%rsi", (int)s->cp);
            emit("\tleaq .Lfmt_%s(%%rip), %%rdi", s->ival ? "sn" : "s");
        } else if ((Ty)s->a->dty == T_ARI || (Ty)s->a->dty == T_STR) {
            int lloop = nlab++, lend = nlab++;
            gen_expr(m, s->a);
            emit("\tsubq $32, %%rsp");
            emit("\tmovq %%rax, 16(%%rsp)");
            if ((Ty)s->a->dty == T_STR) {   /* v1.18: String packed */
                emit("\tmovq $0, 8(%%rsp)");     /* counter 64-bit */
                emit(".L%d:", lloop);
                emit("\tmovq 16(%%rsp), %%r10");
                emit("\tmovq (%r10), %%rax");    /* len */
                emit("\tcmpq %%rax, 8(%%rsp)");
                emit("\tjge .L%d", lend);
                emit("\tmovq 16(%%rsp), %%r10");
                emit("\tmovq 8(%%rsp), %%r11");
                emit("\tmovzbl 8(%%r10,%%r11,1), %%edi");
                emit("\tmovq stdout@GOTPCREL(%%rip), %%rsi");
                emit("\tmovq (%%rsi), %%rsi");
                emit("\tcall fputc@PLT");
                emit("\tincq 8(%%rsp)");
                emit("\tjmp .L%d", lloop);
            } else {                        /* buffer byte-per-slot (debug) */
                emit("\tmovq $0, 8(%%rsp)");
                emit(".L%d:", lloop);
                emit("\tmovq 16(%%rsp), %%r10");
                emit("\tmovq 8(%%rsp), %%r11");
                emit("\tmovq (%r10), %%rax");
                emit("\tcmpq %%rax, %%r11");
                emit("\tjge .L%d", lend);
                emit("\tmovq %%r11, %%rax");
                emit("\tshlq $3, %%rax");
                emit("\tmovq 8(%%r10,%%rax), %%rdi");
                emit("\tmovq stdout@GOTPCREL(%%rip), %%rsi");
                emit("\tmovq (%%rsi), %%rsi");
                emit("\tcall fputc@PLT");
                emit("\tincq 8(%%rsp)");
                emit("\tjmp .L%d", lloop);
            }
            emit(".L%d:", lend);
            if (s->ival) {
                emit("\tmovl $10, %%edi");
                emit("\tmovq stdout@GOTPCREL(%%rip), %%rsi");
                emit("\tmovq (%%rsi), %%rsi");
                emit("\tcall fputc@PLT");
            }
            emit("\taddq $16, %%rsp");
            emit("\txorl %%eax, %%eax");
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

    case ST_REGION:              /* v1.20: malloc { ... } */
        emit_region_enter((int)s->ival, s->cp);   /* v1.21: ukuran custom */
        rg_slot[rg_sp++] = (int)s->ival;
        gen_stmts(m, s->d);
        rg_sp--;
        emit_region_leave((int)s->ival);
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

#include "gen_x64_expr.inc"

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

static void gen_x64_runtime(void);

void gen_x64(const char *outpath) {
    out = fopen(outpath, "w");
    if (!out) die("tidak bisa menulis %s", outpath);
    nlab = 0; par = 0;
    gen_x64_runtime();

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
        rg_sp = 0;
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
            emit("\tmovl $33554432, %%edi");    /* arena global default 32 MB */
            emit("\tcall malloc@PLT");
            emit("\tmovq %%rax, kop_arena_base(%%rip)");
            emit("\tmovq $33554432, kop_arena_cap(%%rip)");
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

    emit(".Kbnd:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tandq $-16, %%rsp");
    emit("\tleaq .Lmsg_bnd(%%rip), %%rdi");
    emit("\tcall printf@PLT");
    emit("\tmovl $1, %%edi");
    emit("\tcall exit@PLT");

    emit(".Koom:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tandq $-16, %%rsp");
    emit("\tcmpq $0, kop_arena_auto(%%rip)");
    emit("\tjne .KoomAuto");
    emit("\tmovq kop_arena_cap(%%rip), %%rsi");
    emit("\tcmpq $1048576, %%rsi");
    emit("\tjae .KoomMB");
    emit("\tshrq $10, %%rsi");               /* < 1 MB → KB */
    emit("\tleaq .Lmsg_oom_kb(%%rip), %%rdi");
    emit("\tjmp .KoomPrint");
    emit(".KoomMB:");
    emit("\tshrq $20, %%rsi");               /* ≥ 1 MB → MB */
    emit("\tleaq .Lmsg_oom(%%rip), %%rdi");
    emit("\tjmp .KoomPrint");
    emit(".KoomAuto:");
    emit("\tleaq .Lmsg_oom_auto(%%rip), %%rdi");
    emit(".KoomPrint:");
    emit("\txorl %%eax, %%eax");
    emit("\tcall printf@PLT");
    emit("\tmovl $1, %%edi");
    emit("\tcall exit@PLT");

    /* v1.22: kop_arena_enter(rdi=byte) -> rax=handle (40 byte).
       0 = OTOMATIS: mmap 4 GB MAP_NORESERVE (RAM sesuai kebutuhan).
       Handle: [0]=base, [8]=cap, [16]=used, [24]=mmapped, [32]=auto. */
    emit(".text");
    emit("\t.globl kop_arena_enter");
    emit("kop_arena_enter:");
    emit("\tpushq\t%%rbx");
    emit("\tpushq\t%%r12");
    emit("\tpushq\t%%r13");
    emit("\tmovq\t%%rdi, %%rbx");            /* ukuran byte (0 = auto) */
    emit("\tmovl\t$40, %%edi");
    emit("\tcall\tmalloc@PLT");
    emit("\ttestq\t%%rax, %%rax");
    emit("\tje\t.Koom");
    emit("\tmovq\t%%rax, %%r12");
    /* simpan konteks arena luar + flag */
    emit("\tmovq\tkop_arena_base(%%rip), %%r10");
    emit("\tmovq\t%%r10, 0(%%r12)");
    emit("\tmovq\tkop_arena_cap(%%rip), %%r10");
    emit("\tmovq\t%%r10, 8(%%r12)");
    emit("\tmovq\tkop_arena_used(%%rip), %%r10");
    emit("\tmovq\t%%r10, 16(%%r12)");
    emit("\tmovq\tkop_arena_mmapped(%%rip), %%r10");
    emit("\tmovq\t%%r10, 24(%%r12)");
    emit("\tmovq\tkop_arena_auto(%%rip), %%r10");
    emit("\tmovq\t%%r10, 32(%%r12)");
    emit("\ttestq\t%%rbx, %%rbx");
    emit("\tjne\t.KenterExplicitX");
    emit("\txorl\t%%edi, %%edi");            /* addr = NULL */
    emit("\tmovabsq\t$4294967296, %%rsi");   /* 4096 MB */
    emit("\tmovl\t$3, %%edx");               /* PROT_READ|PROT_WRITE */
    emit("\tmovl\t$0x4022, %%r10d");         /* PRIVATE|ANON|NORESERVE */
    emit("\tmovq\t$-1, %%r8");               /* fd = -1 */
    emit("\txorl\t%%r9d, %%r9d");            /* offset = 0 */
    emit("\tcall\tmmap@PLT");
    emit("\tcmpq\t$-1, %%rax");
    emit("\tje\t.KenterAutoFailX");
    emit("\tmovq\t%%rax, kop_arena_base(%%rip)");
    emit("\tmovabsq\t$4294967296, %%r10");
    emit("\tmovq\t%%r10, kop_arena_cap(%%rip)");
    emit("\tmovq\t$0, kop_arena_used(%%rip)");
    emit("\tmovq\t$1, kop_arena_mmapped(%%rip)");
    emit("\tmovq\t$1, kop_arena_auto(%%rip)");
    emit("\tjmp\t.KenterDoneX");
    emit(".KenterExplicitX:");
    emit("\tmovq\t%%rbx, %%rdi");
    emit("\tcall\tmalloc@PLT");
    emit("\ttestq\t%%rax, %%rax");
    emit("\tje\t.KenterOomX");
    emit("\tmovq\t%%rax, kop_arena_base(%%rip)");
    emit("\tmovq\t%%rbx, kop_arena_cap(%%rip)");
    emit("\tmovq\t$0, kop_arena_used(%%rip)");
    emit("\tmovq\t$0, kop_arena_mmapped(%%rip)");
    emit("\tmovq\t$0, kop_arena_auto(%%rip)");
    emit(".KenterDoneX:");
    emit("\tmovq\t%%r12, %%rax");
    emit("\tpopq\t%%r13");
    emit("\tpopq\t%%r12");
    emit("\tpopq\t%%rbx");
    emit("\tret");
    emit(".KenterOomX:");
    emit("\tmovq\t%%r12, %%rdi");
    emit("\tcall\tfree@PLT");
    emit("\tjmp\t.Koom");
    emit(".KenterAutoFailX:");
    emit("\tmovq\t%%r12, %%rdi");
    emit("\tcall\tfree@PLT");
    emit("\tleaq\t.Lmsg_oom_auto_fail(%%rip), %%rdi");
    emit("\txorl\t%%eax, %%eax");
    emit("\tcall\tprintf@PLT");
    emit("\tmovl\t$1, %%edi");
    emit("\tcall\texit@PLT");

    /* v1.22: kop_arena_leave(rdi=handle) — munmap bila arena mmap, lalu
       pulihkan arena luar + flag; rax (nilai balik) dipertahankan. */
    emit(".text");
    emit("\t.globl kop_arena_leave");
    emit("kop_arena_leave:");
    emit("\tpushq\t%%rbx");
    emit("\tpushq\t%%r12");
    emit("\tpushq\t%%r13");
    emit("\tmovq\t%%rax, %%rbx");
    emit("\tmovq\t%%rdi, %%r12");
    emit("\tcmpq\t$0, kop_arena_mmapped(%%rip)");
    emit("\tje\t.KleaveFreeX");
    emit("\tmovq\tkop_arena_base(%%rip), %%rdi");
    emit("\tmovq\tkop_arena_cap(%%rip), %%rsi");
    emit("\tcall\tmunmap@PLT");
    emit("\tjmp\t.KleaveRestoreX");
    emit(".KleaveFreeX:");
    emit("\tmovq\tkop_arena_base(%%rip), %%rdi");
    emit("\tcall\tfree@PLT");
    emit(".KleaveRestoreX:");
    emit("\tmovq\t0(%%r12), %%r10");
    emit("\tmovq\t%%r10, kop_arena_base(%%rip)");
    emit("\tmovq\t8(%%r12), %%r10");
    emit("\tmovq\t%%r10, kop_arena_cap(%%rip)");
    emit("\tmovq\t16(%%r12), %%r10");
    emit("\tmovq\t%%r10, kop_arena_used(%%rip)");
    emit("\tmovq\t24(%%r12), %%r10");
    emit("\tmovq\t%%r10, kop_arena_mmapped(%%rip)");
    emit("\tmovq\t32(%%r12), %%r10");
    emit("\tmovq\t%%r10, kop_arena_auto(%%rip)");
    emit("\tmovq\t%%r12, %%rdi");
    emit("\tcall\tfree@PLT");
    emit("\tmovq\t%%rbx, %%rax");
    emit("\tpopq\t%%r13");
    emit("\tpopq\t%%r12");
    emit("\tpopq\t%%rbx");
    emit("\tret");

    emit(".text");
    emit("\t.globl kopalloc");
    emit("kopalloc:");
    emit("\tmovq kop_arena_used(%%rip), %%rax");
    emit("\taddq %%rdi, %%rax");
    emit("\tcmpq kop_arena_cap(%%rip), %%rax");
    emit("\tjge .Koom");
    emit("\tmovq %%rax, kop_arena_used(%%rip)");
    emit("\tsubq %%rdi, %%rax");
    emit("\taddq kop_arena_base(%%rip), %%rax");
    emit("\tret");

    emit("\t.section .data");
    emit("\t.align 8");
    emit("kop_arena_base: .quad 0");
    emit("kop_arena_cap: .quad 0");
    emit("kop_arena_used: .quad 0");
    emit("kop_arena_mmapped: .quad 0");
    emit("kop_arena_auto: .quad 0");
    emit("kop_argc: .quad 0");
    emit("kop_argv: .quad 0");
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
    emit(".Lempty_str: .asciz \"%s\"", "");
    emit(".Lstty_raw: .asciz \"%s\"", "stty raw -echo 2>/dev/null");
    emit(".Lstty_sane: .asciz \"%s\"", "stty sane 2>/dev/null");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    emit(".Lmsg_arr: .asciz \"%s\"", "kopi: array tidak valid (indeks/ukuran salah) — periksa indeks 0..panjang-1\\n");
    emit(".Lmsg_bnd: .asciz \"%s\"", "kopi: byte mkstr di luar 0..255\\n");
    emit(".Lmsg_oom: .asciz \"%s\"", "kopi: arena malloc habis — batas %ld MB terlampaui (naikkan malloc<...> atau pakai malloc{} otomatis)\\n");
    emit(".Lmsg_oom_kb: .asciz \"%s\"", "kopi: arena malloc habis — batas %ld KB terlampaui (naikkan malloc<...> atau pakai malloc{} otomatis)\\n");
    emit(".Lmsg_oom_auto: .asciz \"%s\"", "kopi: arena malloc habis — batas otomatis 4096 MB terlampaui (pakai malloc<N> untuk batas lain)\\n");
    emit(".Lmsg_oom_auto_fail: .asciz \"%s\"", "kopi: gagal menyediakan arena malloc otomatis 4096 MB (coba malloc<N> lebih kecil)\\n");
    for (int i = 0; i < prog.ncp; i++) {
        char esc[1200];
        asm_escape(prog.cpool[i], esc, sizeof esc);
        emit("cpool%d: .asciz \"%s\"", i, esc);
    }
    fclose(out);
}

/* fragmen runtime x64 */
#include "gen_x64_rt.inc"
