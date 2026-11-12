/* Backend ARM64 (aarch64): memancarkan assembly GAS yang bisa
   dirakit dengan clang/gcc (PIE, memanggil printf via GOT). */
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

static void emit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

static int gen_expr(Method *m, Node *e);
static void gen_stmts(Method *m, Node *lst);
static int netlbl;      /* label EX_NET unik per kemunculan */
static int arrlbl;      /* label zero-init array unik */

static int cur_ret_label;

/* push hasil ekspresi: 32-bit (w0) atau pointer closure 64-bit (x0) */
/* 32-bit int/bool vs 64-bit (pointer / numerik baru / float bits) */
static int is64ty(Ty t) {
    return t == T_FN || ty_is_arr(t) || t == T_LONG || t == T_UINT64 ||
           t == T_UINT8 || t == T_UINT16 || t == T_UINT32 ||
           t == T_BYTE || t == T_SHORT || t == T_STR ||
           t == T_FLOAT || t == T_DOUBLE;
}

static void push_x(void) { emit("\tstr\tx0, [sp, #-16]!"); }
static void pop_x(const char *r) { emit("\tldr\t%s, [sp], #16", r); }

/* muat immediate 64-bit ke x0 */
static void mov64_imm(unsigned long long v) {
    emit("\tmovz\tx0, #0x%llx", v & 0xffffULL);
    if ((v >> 16) & 0xffffULL) emit("\tmovk\tx0, #0x%llx, lsl #16", (v >> 16) & 0xffffULL);
    if ((v >> 32) & 0xffffULL) emit("\tmovk\tx0, #0x%llx, lsl #32", (v >> 32) & 0xffffULL);
    if ((v >> 48) & 0xffffULL) emit("\tmovk\tx0, #0x%llx, lsl #48", (v >> 48) & 0xffffULL);
}

/* nilai dari w0 (int/bool) → kanonik x0 */
static void widen_ty(Ty t) {
    if (t == T_INT) emit("\tsxtw\tx0, w0");
    else if (t == T_BOOL) emit("\tmov\tw0, w0");
}

/* normalisasi nilai x0 sesuai lebar/tanda tipe */
static void norm_ty(Ty t) {
    switch (t) {
    case T_INT:    emit("\tsxtw\tx0, w0"); break;
    case T_UINT32: emit("\tmov\tw0, w0"); break;
    case T_UINT16: emit("\tuxth\tw0, w0"); break;
    case T_UINT8:  emit("\tuxtb\tw0, w0"); break;
    case T_BYTE:   emit("\tsxtb\tx0, w0"); break;
    case T_SHORT:  emit("\tsxth\tx0, w0"); break;
    case T_BOOL:   emit("\tand\tw0, w0, #1"); break;
    default: break;                      /* long/uint64/float/double */
    }
}

/* konversi nilai kanonik x0 dari tipe from ke tipe to */
static void conv_ty(Ty from, Ty to) {
    if (from == to) return;
    if (ty_is_float(to)) {
        if (ty_is_float(from)) {
            if (to == T_DOUBLE) {
                emit("\tfmov\ts0, w0");
                emit("\tfcvt\td0, s0");
                emit("\tfmov\tx0, d0");
            } else {
                emit("\tfmov\td0, x0");
                emit("\tfcvt\ts0, d0");
                emit("\tfmov\tw0, s0");
            }
        } else {
            int uns = ty_is_uint(from);
            if (to == T_DOUBLE) {
                emit(uns ? "\tucvtf\td0, x0" : "\tscvtf\td0, x0");
                emit("\tfmov\tx0, d0");
            } else {
                emit(uns ? "\tucvtf\ts0, x0" : "\tscvtf\ts0, x0");
                emit("\tfmov\tw0, s0");
            }
        }
        return;
    }
    if (ty_is_float(from)) {
        int uns = ty_is_uint(to);
        if (from == T_FLOAT) emit("\tfmov\ts0, w0");
        else emit("\tfmov\td0, x0");
        const char *fl = from == T_FLOAT ? "s0" : "d0";
        if (ty_width(to) == 64)
            emit(uns ? "\tfcvtzu\tx0, %s" : "\tfcvtzs\tx0, %s", fl);
        else
            emit(uns ? "\tfcvtzu\tw0, %s" : "\tfcvtzs\tw0, %s", fl);
        norm_ty(to);
        return;
    }
    switch (to) {
    case T_INT:    emit("\tsxtw\tx0, w0"); break;
    case T_UINT32: emit("\tmov\tw0, w0"); break;
    case T_UINT16: emit("\tuxth\tw0, w0"); break;
    case T_UINT8:  emit("\tuxtb\tw0, w0"); break;
    case T_SHORT:  emit("\tsxth\tx0, w0"); break;
    case T_BYTE:   emit("\tsxtb\tx0, w0"); break;
    case T_LONG:   /* int/bool/uint* kanonik sudah benar */
    case T_UINT64: break;
    default: break;
    }
}

/* alamat lokal slot n: x27 = x29 - off (dipakai bila off > 255 —
 * batas immediat unscaled ldr/str aarch64). x27 tidak dipakai backend lain */
static void loc_addr(long off) {
    if (off <= 4095) emit("\tsub\tx27, x29, #%ld", off);
    else {
        emit("\tmov\tx27, #%ld", off);
        if (off > 65535) emit("\tmovk\tx27, #%ld, lsl #16", (off >> 16) & 0xffff);
        emit("\tsub\tx27, x29, x27");
    }
}

static void loc_store(long off, const char *reg) {
    if (off <= 255) { emit("\tstr\t%s, [x29, #-%ld]", reg, off); return; }
    loc_addr(off);
    emit("\tstr\t%s, [x27]", reg);
}

static void loc_load(long off, const char *reg) {
    if (off <= 255) { emit("\tldr\t%s, [x29, #-%ld]", reg, off); return; }
    loc_addr(off);
    emit("\tldr\t%s, [x27]", reg);
}

/* muat konstanta 64-bit ke register (movz + movk) */
static void emit_imm64(const char *reg, unsigned long v) {
    emit("\tmovz\t%s, #%lu", reg, v & 0xffffUL);
    for (int sh = 16; sh < 64; sh += 16) {
        unsigned long p = (v >> sh) & 0xffffUL;
        if (p) emit("\tmovk\t%s, #%lu, lsl #%d", reg, p, sh);
    }
}

/* v1.21: masuk arena blok — alokasi arena baru sebesar bytes, simpan handle */
static void emit_region_enter(int slot, long bytes) {
    emit_imm64("x0", (unsigned long)bytes);
    emit("\tbl\tkop_arena_enter");
    loc_store((slot + 1) * 8, "x1");
}

/* keluar arena blok — bebas arena dalam, pulihkan arena luar (x0 aman) */
static void emit_region_leave(int slot) {
    loc_load((slot + 1) * 8, "x1");
    emit("\tbl\tkop_arena_leave");
}

static void emit_loop_exit_resets(void) {
    int base = brk_sp > 0 ? loop_rg[brk_sp - 1] : 0;
    for (int i = rg_sp - 1; i >= base; i--) emit_region_leave(rg_slot[i]);
}

static void push_val(int is64) {
    if (is64) emit("\tstr\tx0, [sp, #-16]!");
    else emit("\tstr\tw0, [sp, #-16]!");
}

/* pak byte array (pointer di x10) ke kop_iobuf + base; hasil panjang di w11 */
static void pack_path_bytes(int ln, char tag, long base) {
    char l1[40], l2[40], l3[40];
    snprintf(l1, sizeof l1, ".LPK%d%ca", ln, tag);
    snprintf(l2, sizeof l2, ".LPK%d%cb", ln, tag);
    snprintf(l3, sizeof l3, ".LPK%d%cc", ln, tag);
    emit("\tadrp\tx17, kop_iobuf");
    emit("\tadd\tx17, x17, :lo12:kop_iobuf");
    if (base) emit("\tadd\tx17, x17, #%ld", base);
    emit("\tldr\tw11, [x10]");
    emit("\tcmp\tw11, #%ld", 4000 - base);
    emit("\tb.ls\t%s", l1);
    emit("\tmov\tw11, #%ld", 4000 - base);
    emit("%s:", l1);
    emit("\tmov\tw13, #0");
    emit("%s:", l2);
    emit("\tcmp\tw13, w11");
    emit("\tb.hs\t%s", l3);
    emit("\tsxtw\tx15, w13");
    emit("\tlsl\tx15, x15, #3");
    emit("\tadd\tx15, x15, #8");
    emit("\tldr\tx16, [x10, x15]");
    emit("\tstrb\tw16, [x17, x13]");
    emit("\tadd\tw13, w13, #1");
    emit("\tb\t%s", l2);
    emit("%s:", l3);
    emit("\tstrb\twzr, [x17, x11]");        /* NUL-terminate */
}

static void gen_stmt(Method *m, Node *s) {
    switch (s->kind) {
    case ST_VAR:
    case ST_ASSIGN: {
        gen_expr(m, s->a);
        widen_ty((Ty)s->a->dty);
        conv_ty((Ty)s->a->dty, (Ty)s->dty);
        loc_store((s->ival + 1) * 8, is64ty((Ty)s->dty) ? "x0" : "w0");
        break;
    }
    case ST_IDXASG: {
        /* nilai slot: 32-bit (int dsb.) atau 64-bit (long/String/objek) */
        Ty vt = (Ty)s->c->dty;
        int v64 = ty_width(vt) == 64 || vt == T_ARI || vt == T_STR || vt == T_FN;
        gen_expr(m, s->c);               /* nilai */
        push_val(v64);
        gen_expr(m, s->a);               /* x0 = array */
        push_val(1);
        gen_expr(m, s->b);               /* w0 = idx */
        emit("\tldr\tx1, [sp], #16");   /* array */
        if (v64) {
            emit("\tldr\tx2, [sp], #16");  /* nilai 64-bit utuh */
        } else {
            emit("\tldr\tw2, [sp], #16");  /* nilai */
        }
        emit("\tldr\tw3, [x1]");
        emit("\tcmp\tw0, w3");
        emit("\tb.hs\t.Karr");
        emit("\tsxtw\tx9, w0");
        if (!v64) emit("\tsxtw\tx2, w2");
        emit("\tadd\tx1, x1, #8");
        emit("\tstr\tx2, [x1, x9, lsl #3]");
        break;
    }
    case ST_IF: {
        int lelse = nlab++, lend = nlab++;
        gen_expr(m, s->a);
        emit("\tcbz\tw0, .L%d", lelse);
        gen_stmts(m, s->b);
        emit("\tb\t.L%d", lend);
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
        emit("\tcbz\tw0, .L%d", lend);
        gen_stmts(m, s->b);
        emit("\tb\t.L%d", ltop);
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
        emit("\tcbnz\tw0, .L%d", ltop);
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
        if (s->b) { gen_expr(m, s->b); emit("\tcbz\tw0, .L%d", lend); }
        gen_stmts(m, s->d);
        emit(".L%d:", lcont);           /* target continue: sebelum langkah */
        if (s->c) gen_stmt(m, s->c);
        emit("\tb\t.L%d", ltop);
        emit(".L%d:", lend);
        brk_sp--; brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_BRK: emit_loop_exit_resets(); emit("\tb\t.L%d", brk_lab[brk_sp - 1]); break;
    case ST_CONT: emit_loop_exit_resets(); emit("\tb\t.L%d", cont_lab[brk_sp - 1]); break;
    case ST_RETURN: {
        if (s->a) {
            gen_expr(m, s->a);
            widen_ty((Ty)s->a->dty);
            conv_ty((Ty)s->a->dty, m->ret);
        } else {
            emit("\tmov\tw0, wzr");
        }
        for (int i = rg_sp - 1; i >= 0; i--) emit_region_leave(rg_slot[i]);
        emit("\tb\t.Lret%d", cur_ret_label);
        break;
    }
    case ST_PRINT:
        if (s->str) {
            /* printf("<fmt %s>", "<string literal>") */
            const char *fmt = s->ival ? ".Lfmt_sn" : ".Lfmt_s";
            emit("\tadrp\tx0, :got:%s", fmt);
            emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            emit("\tadrp\tx1, :got:cpool%d", (int)s->cp);
            emit("\tldr\tx1, [x1, :got_lo12:cpool%d]", (int)s->cp);
            emit("\tbl\tprintf");
        } else if ((Ty)s->a->dty == T_ARI || (Ty)s->a->dty == T_STR) {
            int lloop = nlab++, lend = nlab++;
            gen_expr(m, s->a);
            emit("\tsub\tsp, sp, #32");
            emit("\tstr\tx0, [sp]");
            if ((Ty)s->a->dty == T_STR) {   /* v1.18: String packed */
                emit("\tstr\txzr, [sp, #8]");   /* counter 64-bit di stack */
                emit(".L%d:", lloop);
                emit("\tldr\tx14, [sp]");
                emit("\tldr\tx15, [x14]");      /* len (reload — fputc merusak) */
                emit("\tldr\tx16, [sp, #8]");   /* i */
                emit("\tcmp\tx16, x15");
                emit("\tb.hs\t.L%d", lend);
                emit("\tadd\tx14, x14, #8");    /* data = blk+8 */
                emit("\tldrb\tw0, [x14, x16]");
                emit("\tadrp\tx1, :got:stdout");
                emit("\tldr\tx1, [x1, :got_lo12:stdout]");
                emit("\tldr\tx1, [x1]");
                emit("\tbl\tfputc");
                emit("\tldr\tx16, [sp, #8]");
                emit("\tadd\tx16, x16, #1");
                emit("\tstr\tx16, [sp, #8]");
                emit("\tb\t.L%d", lloop);
            } else {                        /* buffer byte-per-slot (debug) */
                emit("\tstr\twzr, [sp, #8]");
                emit(".L%d:", lloop);
                emit("\tldr\tx14, [sp]");
                emit("\tldr\tw16, [sp, #8]");
                emit("\tldr\tw15, [x14]");
                emit("\tcmp\tw16, w15");
                emit("\tb.hs\t.L%d", lend);
                emit("\tsxtw\tx9, w16");
                emit("\tlsl\tx9, x9, #3");
                emit("\tadd\tx9, x9, #8");
                emit("\tldr\tw0, [x14, x9]");
                emit("\tadrp\tx1, :got:stdout");
                emit("\tldr\tx1, [x1, :got_lo12:stdout]");
                emit("\tldr\tx1, [x1]");
                emit("\tbl\tfputc");
                emit("\tldr\tw16, [sp, #8]");
                emit("\tadd\tw16, w16, #1");
                emit("\tstr\tw16, [sp, #8]");
                emit("\tb\t.L%d", lloop);
            }
            emit(".L%d:", lend);
            if (s->ival) {
                emit("\tmov\tw0, #10");
                emit("\tadrp\tx1, :got:stdout");
                emit("\tldr\tx1, [x1, :got_lo12:stdout]");
                emit("\tldr\tx1, [x1]");
                emit("\tbl\tfputc");
            }
            emit("\tadd\tsp, sp, #32");
            emit("\tmov\tw0, wzr");
        } else {
            gen_expr(m, s->a);
            Ty pt = (Ty)s->a->dty;
            if (pt == T_INT || pt == T_BOOL || pt == T_BYTE || pt == T_SHORT) {
                const char *fmt = s->ival ? ".Lfmt_in" : ".Lfmt_i";
                emit("\tsxtw\tx1, w0");
                emit("\tadrp\tx0, :got:%s", fmt);
                emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            } else if (pt == T_LONG) {
                const char *fmt = s->ival ? ".Lfmt_ldn" : ".Lfmt_ld";
                emit("\tmov\tx1, x0");
                emit("\tadrp\tx0, :got:%s", fmt);
                emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            } else if (pt == T_UINT64) {
                const char *fmt = s->ival ? ".Lfmt_lun" : ".Lfmt_lu";
                emit("\tmov\tx1, x0");
                emit("\tadrp\tx0, :got:%s", fmt);
                emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            } else if (pt == T_UINT8 || pt == T_UINT16 || pt == T_UINT32) {
                const char *fmt = s->ival ? ".Lfmt_un" : ".Lfmt_u";
                emit("\tmov\tw1, w0");
                emit("\tadrp\tx0, :got:%s", fmt);
                emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            } else if (pt == T_FLOAT) {
                const char *fmt = s->ival ? ".Lfmt_gfn" : ".Lfmt_gf";
                emit("\tfmov\ts0, w0");
                emit("\tfcvt\td0, s0");
                emit("\tadrp\tx0, :got:%s", fmt);
                emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            } else {
                const char *fmt = s->ival ? ".Lfmt_gn" : ".Lfmt_g";
                emit("\tfmov\td0, x0");
                emit("\tadrp\tx0, :got:%s", fmt);
                emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            }
            emit("\tbl\tprintf");
        }
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
        die("gen_a64: statement kind=%d", s->kind);
    }
}

static void gen_stmts(Method *m, Node *lst) {
    for (Node *n = lst; n; n = n->next) gen_stmt(m, n);
}

static void push_w0(void) { emit("\tstr\tw0, [sp, #-16]!"); }
static void pop_w(const char *r) { emit("\tldr\t%s, [sp], #16", r); }

/* operasi biner untuk tipe numerik baru (long, uint*, float, double) */
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

    /* identitas objek (v1.12): ==/!= pada dua array → bandingkan pointer 64-bit */
    if (ty_is_arr(lt) && lt == rt && isCmp) {
        gen_expr(m, e->a);
        push_x();
        gen_expr(m, e->b);
        pop_x("x9");
        emit("\tcmp\tx9, x0");
        emit("\tcset\tw0, %s", op == '=' ? "eq" : "ne");
        return 0;
    }

    gen_expr(m, e->a);
    widen_ty(lt);
    conv_ty(lt, P);
    push_x();
    gen_expr(m, e->b);
    widen_ty(rt);
    if (!isShift) conv_ty(rt, P);
    pop_x("x9");

    if (ty_is_float(P)) {
        const char *pre = P == T_FLOAT ? "s" : "d";
        if (P == T_FLOAT) { emit("\tfmov\ts0, w9"); emit("\tfmov\ts1, w0"); }
        else { emit("\tfmov\td0, x9"); emit("\tfmov\td1, x0"); }
        if (isCmp) {
            emit("\tfcmp\t%s0, %s1", pre, pre);
            if (op == '=') emit("\tcset\tw0, eq");
            else if (op == '!') emit("\tcset\tw0, ne");
            else if (!strcmp(e->str, "<=")) emit("\tcset\tw0, le");
            else if (!strcmp(e->str, ">=")) emit("\tcset\tw0, ge");
            else if (op == '<') emit("\tcset\tw0, lt");
            else emit("\tcset\tw0, gt");
            return 0;
        }
        if (op == '+') emit("\tfadd\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '-') emit("\tfsub\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '*') emit("\tfmul\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '/') emit("\tfdiv\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '%') {
            emit("\tfdiv\t%s2, %s0, %s1", pre, pre, pre);
            emit("\tfrintz\t%s2, %s2", pre, pre);
            emit("\tfmul\t%s2, %s2, %s1", pre, pre, pre);
            emit("\tfsub\t%s0, %s0, %s2", pre, pre, pre);
        } else {
            die("gen_a64: operator float '%s'", e->str);
        }
        if (P == T_FLOAT) emit("\tfmov\tw0, s0");
        else emit("\tfmov\tx0, d0");
        return is64ty(P);
    }

    if (isShift) {
        int mask = ty_width(P) - 1;
        if (mask <= 0) mask = 31;
        emit("\tmov\tw10, #%d", mask);
        emit("\tand\tw10, w0, w10");
        if (!strcmp(e->str, "<<")) emit("\tlslv\tx0, x9, x10");
        else if (!strcmp(e->str, ">>")) emit("\tasrv\tx0, x9, x10");
        else emit("\tlsrv\tx0, x9, x10");
        norm_ty(P);
        return is64ty(P);
    }
    if (isCmp) {
        int uns = ty_is_uint(P);
        emit("\tcmp\tx9, x0");
        if (op == '=') emit("\tcset\tw0, eq");
        else if (op == '!') emit("\tcset\tw0, ne");
        else if (!strcmp(e->str, "<=")) emit(uns ? "\tcset\tw0, ls" : "\tcset\tw0, le");
        else if (!strcmp(e->str, ">=")) emit(uns ? "\tcset\tw0, hs" : "\tcset\tw0, ge");
        else if (op == '<') emit(uns ? "\tcset\tw0, lo" : "\tcset\tw0, lt");
        else emit(uns ? "\tcset\tw0, hi" : "\tcset\tw0, gt");
        return 0;
    }
    int uns = ty_is_uint(P);
    switch (op) {
    case '+': emit("\tadd\tx0, x9, x0"); break;
    case '-': emit("\tsub\tx0, x9, x0"); break;
    case '*': emit("\tmul\tx0, x9, x0"); break;
    case '/': emit(uns ? "\tudiv\tx0, x9, x0" : "\tsdiv\tx0, x9, x0"); break;
    case '%':
        emit(uns ? "\tudiv\tx10, x9, x0" : "\tsdiv\tx10, x9, x0");
        emit("\tmsub\tx0, x10, x0, x9");
        break;
    case '&': emit("\tand\tx0, x9, x0"); break;
    case '|': emit("\torr\tx0, x9, x0"); break;
    case '^': emit("\teor\tx0, x9, x0"); break;
    default: die("gen_a64: operator '%s'", e->str);
    }
    norm_ty(P);
    return is64ty(P);
}

/* hasil: 0 = int/bool di w0 (32-bit); 1 = POINTER closure di x0 (64-bit) */
static int gen_expr(Method *m, Node *e) {
    switch (e->kind) {
    case EX_INT: {
        Ty t = (Ty)e->dty;
        if (t == T_INT) {
            long v = e->ival;
            if (v >= 0 && v <= 65535) {
                emit("\tmov\tw0, #%ld", v);
            } else {
                uint32_t u = (uint32_t)v;
                emit("\tmovz\tw0, #0x%lx", (long)(u & 0xffff));
                if ((u >> 16) & 0xffff)
                    emit("\tmovk\tw0, #0x%lx, lsl #16", (long)((u >> 16) & 0xffff));
            }
            return 0;
        }
        if (t == T_FLOAT) {
            union { float f; uint32_t u; } cv;
            cv.f = (float)e->fval;
            emit("\tmovz\tw0, #0x%lx", (long)(cv.u & 0xffff));
            if (cv.u >> 16) emit("\tmovk\tw0, #0x%lx, lsl #16", (long)(cv.u >> 16));
            return 1;
        }
        if (t == T_DOUBLE) {
            union { double d; unsigned long long u; } cv;
            cv.d = e->fval;
            mov64_imm(cv.u);
            return 1;
        }
        if (t == T_UINT32 || t == T_UINT8 || t == T_UINT16 || t == T_BYTE || t == T_SHORT) {
            uint32_t u = (uint32_t)e->ival;
            emit("\tmovz\tw0, #0x%lx", (long)(u & 0xffff));
            if (u >> 16) emit("\tmovk\tw0, #0x%lx, lsl #16", (long)(u >> 16));
            return 1;
        }
        mov64_imm((unsigned long long)e->ival);
        return 1;
    }
    case EX_BOOL:
        if (e->ival) emit("\tmov\tw0, #1");
        else emit("\tmov\tw0, wzr");
        return 0;
    case EX_STRLIT: {
        Node f;
        memset(&f, 0, sizeof f);
        f.kind = EX_NET;
        f.ival = 5;
        f.cp = e->cp;
        f.dty = (int)T_ARI;
        return gen_expr(m, &f);
    }
    case EX_VAR:
        if (is64ty((Ty)e->dty)) {
            loc_load((e->ival + 1) * 8, "x0");
            return 1;
        }
        loc_load((e->ival + 1) * 8, "w0");
        return 0;
    case EX_UN: {
        Ty t = (Ty)e->dty;
        int wide = is64ty(t);
        gen_expr(m, e->a);
        if (e->str[0] == '!') { emit("\teor\tw0, w0, #1"); return 0; }
        if (ty_is_float(t)) {
            if (t == T_FLOAT) { emit("\tfmov\ts0, w0"); emit("\tfneg\ts0, s0"); emit("\tfmov\tw0, s0"); }
            else { emit("\tfmov\td0, x0"); emit("\tfneg\td0, d0"); emit("\tfmov\tx0, d0"); }
            return 1;
        }
        if (!wide) {
            if (e->str[0] == '-') emit("\tneg\tw0, w0");
            else emit("\tmvn\tw0, w0");
            return 0;
        }
        widen_ty(t);
        if (e->str[0] == '-') emit("\tneg\tx0, x0");
        else emit("\tmvn\tx0, x0");
        norm_ty(t);
        return 1;
    }
    case EX_CALL: {
        int n = 0;
        int w64[64];
        Param *pp = e->meth ? e->meth->params : 0;
        for (Node *a = e->a; a; a = a->next, pp = pp ? pp->next : 0) {
            if (n >= 8) die("baris %d: maksimal 8 argumen untuk target native", e->line);
            Ty at = (Ty)a->dty;
            Ty pt = pp ? (Ty)pp->ty : T_INT;
            gen_expr(m, a);
            if (pt == T_FN) {
                w64[n] = 1;
            } else {
                widen_ty(at);
                conv_ty(at, pt);
                w64[n] = is64ty(pt);
            }
            push_val(w64[n]);
            n++;
        }
        /* arg k berada di offset (n-1-k)*16 dari sp (urut push terbalik) */
        for (int k = 0; k < n; k++) {
            if (w64[k]) emit("\tldr\tx%d, [sp, #%d]", k, (n - 1 - k) * 16);
            else emit("\tldr\tw%d, [sp, #%d]", k, (n - 1 - k) * 16);
        }
        if (n > 0) emit("\tadd\tsp, sp, #%d", n * 16);
        /* label: Class__name */
        {
            const char *dot = strchr(e->str, '.');
            char cls[256];
            size_t cl = (size_t)(dot - e->str);
            memcpy(cls, e->str, cl); cls[cl] = 0;
            emit("\tbl\t%s__%s", cls, dot + 1);
        }
        return is64ty((Ty)e->dty);       /* int[] / fn → x0; int → w0 */
    }
    case EX_ARROW: {
        Method *hm = (Method *)e->cp;
        long ncap = hm->ncapt;
        emit("\tmov\tx0, #%ld", 8 * (3 + ncap));          /* ukuran blok */
        emit("\tbl\tkopalloc");                          /* arena bump */
        emit("\tadrp\tx1, %s__%s", hm->cls, hm->name);    /* kode */
        emit("\tadd\tx1, x1, :lo12:%s__%s", hm->cls, hm->name);
        emit("\tstr\tx1, [x0]");
        emit("\tmov\tw2, #%ld", ncap);
        emit("\tstr\tw2, [x0, #8]");
        emit("\tmov\tw2, #%ld", hm->nparams - 1);         /* nuser */
        emit("\tstr\tw2, [x0, #16]");
        for (int j = 0; j < ncap; j++) {
            loc_load((hm->captslot[j] + 1) * 8, is64ty(hm->captty[j]) ? "x2" : "w2");
            emit("\tstr\tx2, [x0, #%ld]", 24 + 8 * j);
        }
        return 1;
    }
    case EX_FCALL: {
        /* argumen digunakan dulu (lebih dahulu di stack), lalu CALLEE → x19 */
        int n = 0;
        for (Node *a = e->a; a; a = a->next) {
            if (n >= 6) die("baris %d: maksimal 5 argumen pemanggilan lambda", e->line);
            gen_expr(m, a);
            push_val(0);                 /* argumen selalu int (v1) */
            n++;
        }
        gen_expr(m, e->b);               /* x0 = pointer closure */
        emit("\tmov\tx19, x0");
        for (int k = 0; k < n; k++)
            emit("\tldr\tw%d, [sp, #%d]", k + 1, (n - 1 - k) * 16);
        if (n > 0) emit("\tadd\tsp, sp, #%d", n * 16);
        /* cek arity runtime: nuser (env[2]) != n → mati rapi */
        emit("\tldr\tw9, [x19, #16]");
        emit("\tcmp\tw9, #%d", n);
        emit("\tb.ne\t.Klam");
        emit("\tmov\tx0, x19");          /* arg0 = env */
        emit("\tldr\tx20, [x19]");       /* kode */
        emit("\tblr\tx20");
        return 0;                        /* hasil kembalian: int di w0 */
    }
    case EX_NEWARR: {
        gen_expr(m, e->a);               /* w0 = n */
        emit("\tcmp\tw0, #0");
        emit("\tb.lt\t.Karr");          /* ukuran salah */
        emit("\tsxtw\tx9, w0");
        emit("\tlsl\tx0, x9, #3");      /* 8*n */
        emit("\tadd\tx0, x0, #8");      /* + header len */
        emit("\tbl\tkopalloc");
        emit("\tstr\tx9, [x0]");        /* len */
        {
            int L = arrlbl++;
            emit("\tmov\tx10, #0");
            emit(".LNZ%d:", L);
            emit("\tcmp\tx10, x9");
            emit("\tb.hs\t.LNZ%de", L);
            emit("\tadd\tx11, x0, #8");
            emit("\tstr\txzr, [x11, x10, lsl #3]");
            emit("\tadd\tx10, x10, #1");
            emit("\tb\t.LNZ%d", L);
            emit(".LNZ%de:", L);
        }
        return 1;
    }
    case EX_IDX: {                       /* hasil: w0 = elemen (32-bit) */
        gen_expr(m, e->a);               /* x0 = array/String pointer */
        push_val(1);
        gen_expr(m, e->b);               /* w0 = idx */
        emit("\tldr\tx1, [sp], #16");
        emit("\tldr\tw2, [x1]");        /* len */
        emit("\tcmp\tw0, w2");
        emit("\tb.hs\t.Karr");          /* (>0 unsigned): negatif ikut tertangkap */
        emit("\tsxtw\tx9, w0");
        emit("\tadd\tx1, x1, #8");
        if ((Ty)e->a->dty == T_STR) {   /* v1.18: String packed — byte di dalam slot */
            emit("\tldrb\tw0, [x1, x9]");
            return 0;
        }
        emit("\tldr\tx0, [x1, x9, lsl #3]");
        {   /* slot 64-bit (long/String/objek): jangan pangkas ke 32-bit */
            Ty et = (Ty)e->dty;
            if (ty_width(et) != 64 && et != T_ARI && et != T_STR && et != T_FN)
                emit("\tmov\tw0, w0");      /* nilai int tetap 32-bit */
        }
        return 0;
    }
    case EX_LEN:
        gen_expr(m, e->a);
        emit("\tldr\tw0, [x0]");
        return 0;
    case EX_NET: {                       /* syscall Linux langsung (tanpa libc) */
        int ln = netlbl++;                /* label unik per kemunculan */
        /* 0=socket(domain,type) 1=connect(fd,ipBE,port) 2=rdb(fd)
         * 3=wrb(fd,b) 4=close(fd) 5=bytes("lit") → int[]                    */
        if (e->ival == 5) {              /* Net.bytes("literal") → String packed (v1.18) */
            int c = (int)e->cp;
            emit("\tadrp\tx9, :got:cpool%d", c);
            emit("\tldr\tx9, [x9, :got_lo12:cpool%d]", c);
            emit("\tmov\tx0, x9");
            emit("\tbl\tkop_str_from_cstr");
            return 1;                       /* hasil pointer String packed */
        }
        if (e->ival == 6) {              /* Net.openr("literal") → fd          */
            int c = (int)e->cp;
            emit("\tadrp\tx9, :got:cpool%d", c);
            emit("\tldr\tx9, [x9, :got_lo12:cpool%d]", c);
            emit("\tmov\tx10, #0");
            emit(".LNO%da:", ln);
            emit("\tldrb\tw1, [x9, x10]");
            emit("\tadd\tx10, x10, #1");
            emit("\tcbnz\tw1, .LNO%da", ln);
            /* openat(AT_FDCWD=-100, path, O_RDONLY) */
            emit("\tmovn\tw0, #99");                   /* -100 */
            emit("\tmov\tx1, x9");                     /* path */
            emit("\tmov\tx2, #0");                     /* flags = O_RDONLY */
            emit("\tmov\tx8, #56");                    /* __NR_openat */
            emit("\tsvc\t#0");
            return 0;                                  /* fd | -errno */
        }
        if (e->ival == 7 || e->ival == 8) {
            /* recvb(fd, buf) / sendb(fd, buf): buf adalah pointer 64-bit */
            for (Node *aa = e->a; aa; aa = aa->next) {
                gen_expr(m, aa);
                push_val(aa == e->a ? 0 : 1);          /* fd int, buf array */
            }
        }
        if (e->ival == 7) {              /* recvb(fd, buf) → n | -1            */
            /* stack: [0..15]=buf (64-bit), [16..31]=fd; tmp via .comm */
            emit("\tldr\tx10, [sp]");                  /* buf */
            emit("\tldr\tw11, [x10]");                 /* len */
            emit("\tcmp\tw11, #0");
            emit("\tb.le\t.LRV%dz", ln);
            emit("\tmov\tw9, #0");                     /* got */
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            emit(".LRV%dl:", ln);
            emit("\tcmp\tw9, w11");
            emit("\tb.hs\t.LRV%df", ln);
            emit("\tsub\tw12, w11, w9");               /* sisa */
            emit("\tcmp\tw12, #240");
            emit("\tb.ls\t.LRV%dc", ln);
            emit("\tmov\tw12, #240");
            emit(".LRV%dc:", ln);
            /* salin ke tmp: tmp[k] = buf[1 + got + k] */
            emit("\tmov\tw13, #0");
            emit(".LRV%dcs:", ln);
            emit("\tcmp\tw13, w12");
            emit("\tb.hs\t.LRV%dcf", ln);
            emit("\tadd\tw15, w13, w9");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tldr\tx16, [x10, x15]");
            emit("\tstrb\tw16, [x17, x13]");
            emit("\tadd\tw13, w13, #1");
            emit("\tb\t.LRV%dcs", ln);
            emit(".LRV%dcf:", ln);
            emit("\tmov\tw2, w12");                    /* want */
            emit("\tmov\tx1, x17");                    /* tmp */
            emit("\tldr\tw0, [sp, #16]");              /* fd */
            emit("\tmov\tx8, #63");                    /* __NR_read */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #0");
            emit("\tb.lt\t.LRV%dry", ln);              /* galat keras */
            emit("\tb.eq\t.LRV%de", ln);               /* EOF */
            /* salin kembali: buf[1 + got + k] = tmp[k], k < x0 */
            emit("\tmov\tx13, #0");
            emit(".LRV%drs:", ln);
            emit("\tcmp\tx13, x0");
            emit("\tb.hs\t.LRV%drf", ln);
            emit("\tldrb\tw16, [x17, x13]");
            emit("\tadd\tw15, w13, w9");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tstr\tx16, [x10, x15]");
            emit("\tadd\tx13, x13, #1");
            emit("\tb\t.LRV%drs", ln);
            emit(".LRV%drf:", ln);
            emit("\tadd\tw9, w9, w0");                 /* got += nread */
            emit("\tb\t.LRV%dl", ln);
            emit(".LRV%dry:", ln);                     /* galat → -1 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tneg\tw0, wzr");
            emit("\tb\t.LRV%dh", ln);
            emit(".LRV%de:", ln);                      /* EOF → got */
            emit(".LRV%df:", ln);                      /* penuh → got */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tmov\tw0, w9");
            emit("\tb\t.LRV%dh", ln);
            emit(".LRV%dz:", ln);                      /* len<=0 → 0 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tmov\tw0, wzr");
            emit(".LRV%dh:", ln);
            return 0;
        }
        if (e->ival == 8) {              /* sendb(fd, buf) → 0 | -1            */
            emit("\tldr\tx10, [sp]");                  /* buf */
            emit("\tldr\tw11, [x10]");                 /* len */
            emit("\tcmp\tw11, #0");
            emit("\tb.le\t.LRS%df", ln);
            emit("\tmov\tw9, #0");                     /* terkirim */
            emit(".LRS%dl:", ln);
            emit("\tcmp\tw9, w11");
            emit("\tb.hs\t.LRS%df", ln);
            emit("\tsub\tw12, w11, w9");
            emit("\tcmp\tw12, #240");
            emit("\tb.ls\t.LRS%dc", ln);
            emit("\tmov\tw12, #240");
            emit(".LRS%dc:", ln);
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            /* salin ke tmp: tmp[k] = buf[1 + terkirim + k] */
            emit("\tmov\tw13, #0");
            emit(".LRS%dcs:", ln);
            emit("\tcmp\tw13, w12");
            emit("\tb.hs\t.LRS%dcf", ln);
            emit("\tadd\tw15, w13, w9");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tldr\tx16, [x10, x15]");
            emit("\tstrb\tw16, [x17, x13]");
            emit("\tadd\tw13, w13, #1");
            emit("\tb\t.LRS%dcs", ln);
            emit(".LRS%dcf:", ln);
            emit("\tmov\tw2, w12");
            emit("\tmov\tx1, x17");
            emit("\tldr\tw0, [sp, #16]");
            emit("\tmov\tx8, #64");                    /* __NR_write */
            emit("\tsvc\t#0");
            emit("\tcmp\tw0, #0");
            emit("\tb.lt\t.LRS%de", ln);
            emit("\tadd\tw9, w9, w0");                 /* penuh/parsial */
            emit("\tb\t.LRS%dl", ln);
            emit(".LRS%de:", ln);                      /* galat → -1 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tneg\tw0, wzr");
            emit("\tb\t.LRS%dh", ln);
            emit(".LRS%df:", ln);                      /* selesai/len 0 → 0 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tmov\tw0, wzr");
            emit(".LRS%dh:", ln);
            return 0;
        }
        if (e->ival >= 9 && e->ival <= 12) {
            /* bind/sendto/recvfrom/shutdown — argumen array di-push 64-bit */
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                int isArr = (e->ival == 10 && idx == 3) ||
                            (e->ival == 11 && (idx == 1 || idx == 2));
                gen_expr(m, aa);
                push_val(isArr);
            }
            if (e->ival == 9) {             /* bind(fd, ip, port) → 0/-errno */
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tldr\tw6, [sp, #32]");           /* ip */
                emit("\tldr\tw7, [sp, #16]");           /* port */
                emit("\tlsr\tw4, w7, #8");
                emit("\tstrb\tw4, [sp, #2]");
                emit("\tand\tw4, w7, #0xff");
                emit("\tstrb\tw4, [sp, #3]");
                emit("\tlsr\tw4, w6, #24");
                emit("\tstrb\tw4, [sp, #4]");
                emit("\tlsr\tw4, w6, #16");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #5]");
                emit("\tlsr\tw4, w6, #8");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #6]");
                emit("\tand\tw4, w6, #0xff");
                emit("\tstrb\tw4, [sp, #7]");
                emit("\tmovz\tw4, #2");
                emit("\tstrb\tw4, [sp, #0]");
                emit("\tldr\tx0, [sp, #48]");           /* fd */
                emit("\tmov\tx1, sp");
                emit("\tmov\tw2, #16");
                emit("\tmov\tx8, #200");                /* __NR_bind */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #64");
                return 0;
            }
            if (e->ival == 12) {            /* shutdown(fd, how) */
                emit("\tldr\tw0, [sp, #16]");
                emit("\tldr\tw1, [sp, #0]");
                emit("\tmov\tx8, #210");                /* __NR_shutdown */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #32");
                return 0;
            }
            if (e->ival == 10) {            /* sendto(fd, ip, port, buf) → n */
                emit("\tstp\txzr, xzr, [sp, #-16]!");  /* sa */
                emit("\tldr\tx10, [sp, #16]");          /* buf */
                emit("\tldr\tw6, [sp, #48]");           /* ip */
                emit("\tldr\tw7, [sp, #32]");           /* port */
                emit("\tlsr\tw4, w7, #8");
                emit("\tstrb\tw4, [sp, #2]");
                emit("\tand\tw4, w7, #0xff");
                emit("\tstrb\tw4, [sp, #3]");
                emit("\tlsr\tw4, w6, #24");
                emit("\tstrb\tw4, [sp, #4]");
                emit("\tlsr\tw4, w6, #16");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #5]");
                emit("\tlsr\tw4, w6, #8");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #6]");
                emit("\tand\tw4, w6, #0xff");
                emit("\tstrb\tw4, [sp, #7]");
                emit("\tmovz\tw4, #2");
                emit("\tstrb\tw4, [sp, #0]");
                /* pak ke iobuf (maks 240 byte per datagram, batas v1) */
                emit("\tldr\tw11, [x10]");
                emit("\tmov\tw3, #1200");
                emit("\tcmp\tw11, w3");
                emit("\tb.ls\t.LST%dc", ln);
                emit("\tmov\tw11, #1200");
                emit(".LST%dc:", ln);
                emit("\tadrp\tx17, kop_iobuf");
                emit("\tadd\tx17, x17, :lo12:kop_iobuf");
                emit("\tmov\tw13, #0");
                emit(".LST%dk:", ln);
                emit("\tcmp\tw13, w11");
                emit("\tb.hs\t.LST%df", ln);
                emit("\tsxtw\tx15, w13");
                emit("\tlsl\tx15, x15, #3");
                emit("\tadd\tx15, x15, #8");
                emit("\tldr\tx16, [x10, x15]");
                emit("\tstrb\tw16, [x17, x13]");
                emit("\tadd\tw13, w13, #1");
                emit("\tb\t.LST%dk", ln);
                emit(".LST%df:", ln);
                emit("\tldr\tx0, [sp, #64]");           /* fd */
                emit("\tmov\tx1, x17");
                emit("\tmov\tw2, w11");
                emit("\tmov\tx3, #0");
                emit("\tmov\tx4, sp");
                emit("\tmov\tx5, #16");
                emit("\tmov\tx8, #206");                /* __NR_sendto */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #80");
                return 0;
            }
            if (e->ival == 11) {            /* recvfrom(fd, buf, from) → n */
                emit("\tstp\txzr, xzr, [sp, #-16]!");  /* sa */
                emit("\tstp\txzr, xzr, [sp, #-16]!");  /* addrlen */
                emit("\tmov\tw9, #16");
                emit("\tstr\tw9, [sp]");
                emit("\tadrp\tx17, kop_iobuf");
                emit("\tadd\tx17, x17, :lo12:kop_iobuf");
                emit("\tldr\tx0, [sp, #64]");           /* fd */
                emit("\tmov\tx1, x17");
                emit("\tmov\tw2, #1200");
                emit("\tmov\tx3, #0");
                emit("\tadd\tx4, sp, #16");
                emit("\tmov\tx5, sp");
                emit("\tmov\tx8, #207");                /* __NR_recvfrom */
                emit("\tsvc\t#0");
                emit("\tcmp\tx0, #0");
                emit("\tb.lt\t.LRF%de", ln);
                emit("\tmov\tw11, w0");                /* simpan n */
                emit("\tldr\tx10, [sp, #48]");          /* buf */
                emit("\tmov\tw13, #0");
                emit(".LRF%dk:", ln);
                emit("\tcmp\tw13, w11");
                emit("\tb.hs\t.LRF%df", ln);
                emit("\tldrb\tw16, [x17, x13]");
                emit("\tsxtw\tx15, w13");
                emit("\tlsl\tx15, x15, #3");
                emit("\tadd\tx15, x15, #8");
                emit("\tstr\tx16, [x10, x15]");
                emit("\tadd\tw13, w13, #1");
                emit("\tb\t.LRF%dk", ln);
                emit(".LRF%df:", ln);
                /* from[0] = ip, from[1] = port (host order) */
                emit("\tldr\tx12, [sp, #32]");
                emit("\tldrb\tw6, [sp, #20]");
                emit("\tlsl\tw6, w6, #24");
                emit("\tldrb\tw4, [sp, #21]");
                emit("\tlsl\tw4, w4, #16");
                emit("\torr\tw6, w6, w4");
                emit("\tldrb\tw4, [sp, #22]");
                emit("\tlsl\tw4, w4, #8");
                emit("\torr\tw6, w6, w4");
                emit("\tldrb\tw4, [sp, #23]");
                emit("\torr\tw6, w6, w4");
                emit("\tldrb\tw3, [sp, #18]");
                emit("\tlsl\tw3, w3, #8");
                emit("\tldrb\tw4, [sp, #19]");
                emit("\torr\tw3, w3, w4");
                emit("\tstr\tw6, [x12, #8]");
                emit("\tstr\tw3, [x12, #16]");
                emit("\tmov\tw0, w11");
                emit("\tb\t.LRF%dh", ln);
                emit(".LRF%de:", ln);                    /* galat → x0 apa adanya */
                emit(".LRF%dh:", ln);
                emit("\tadd\tsp, sp, #80");
                return 0;
            }
        }
        if (e->ival >= 13 && e->ival <= 21) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                int isArr = (e->ival == 13 && idx == 0) ||
                            (e->ival == 14 || e->ival == 15) ||
                            (e->ival == 16 && idx < 2);
                push_val(isArr);
            }
            if (e->ival == 13) {            /* openb(path, flags, mode) */
                emit("\tldr\tx10, [sp, #32]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tmovn\tw0, #99");               /* AT_FDCWD */
                emit("\tmov\tx1, x17");
                emit("\tldr\tw2, [sp, #16]");          /* flags */
                emit("\tldr\tw3, [sp, #0]");           /* mode */
                emit("\tmov\tx8, #56");                /* __NR_openat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #48");
                return 0;
            }
            if (e->ival == 14) {            /* mkdirb(path) */
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tmovn\tw0, #99");
                emit("\tmov\tx1, x17");
                emit("\tmov\tw2, #493");               /* 0755 */
                emit("\tmov\tx8, #34");                /* __NR_mkdirat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 15) {            /* unlinkb(path) */
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tmovn\tw0, #99");
                emit("\tmov\tx1, x17");
                emit("\tmov\tx2, #0");
                emit("\tmov\tx8, #35");                /* __NR_unlinkat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 16) {            /* renameb(old, new) */
                emit("\tldr\tx10, [sp, #16]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'b', 2048);
                emit("\tmovn\tw0, #99");
                emit("\tsub\tx1, x17, #2048");         /* path lama */
                emit("\tmovn\tw2, #99");
                emit("\tmov\tx3, x17");                /* path baru */
                emit("\tmov\tx8, #38");                /* __NR_renameat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #32");
                return 0;
            }
            if (e->ival == 17) {            /* seek(fd, off, whence) */
                emit("\tldr\tw0, [sp, #32]");
                emit("\tldr\tw1, [sp, #16]");
                emit("\tsxtw\tx1, w1");
                emit("\tldr\tw2, [sp, #0]");
                emit("\tmov\tx8, #62");                /* __NR_lseek */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #48");
                return 0;
            }
            if (e->ival == 18) {            /* time() → detik epoch */
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tmov\tw0, #0");                 /* CLOCK_REALTIME */
                emit("\tmov\tx1, sp");
                emit("\tmov\tx8, #113");               /* __NR_clock_gettime */
                emit("\tsvc\t#0");
                emit("\tldr\tx0, [sp]");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 19) {            /* mono() → milidetik monotonik */
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tmov\tw0, #1");                 /* CLOCK_MONOTONIC */
                emit("\tmov\tx1, sp");
                emit("\tmov\tx8, #113");
                emit("\tsvc\t#0");
                emit("\tldr\tx1, [sp]");
                emit("\tldr\tx2, [sp, #8]");
                emit("\tmov\tx3, #1000");
                emit("\tmul\tx0, x1, x3");
                emit("\tmovz\tx3, #0x4240");
                emit("\tmovk\tx3, #0xf, lsl #16");
                emit("\tudiv\tx2, x2, x3");
                emit("\tadd\tx0, x0, x2");
                emit("\tadd\tsp, sp, #16");
                return 1;
            }
            if (e->ival == 20) {            /* sleep(ms) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tmov\tw1, #1000");
                emit("\tudiv\tw2, w0, w1");            /* detik */
                emit("\tmsub\tw3, w2, w1, w0");        /* sisa ms */
                emit("\tmovz\tw4, #0x4240");
                emit("\tmovk\tw4, #0xf, lsl #16");
                emit("\tmul\tw3, w3, w4");
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tsxtw\tx2, w2");
                emit("\tstr\tx2, [sp]");
                emit("\tsxtw\tx3, w3");
                emit("\tstr\tx3, [sp, #8]");
                emit("\tmov\tx0, sp");
                emit("\tmov\tx1, #0");
                emit("\tmov\tx8, #115");               /* __NR_nanosleep */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                emit("\tadd\tsp, sp, #16");
                emit("\tmov\tw0, wzr");
                return 0;
            }
            if (e->ival == 21) {            /* exit(code) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tmov\tx8, #93");                /* __NR_exit */
                emit("\tsvc\t#0");
                return 0;
            }
        }
        if (e->ival >= 22 && e->ival <= 24) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                push_val(1);                    /* pointer array/String */
            }
            if (e->ival == 22) {                /* cat(a,b) → kop_cat_packed (v1.18) */
                emit("\tldr\tx0, [sp, #16]");       /* a */
                emit("\tldr\tx1, [sp]");            /* b */
                emit("\tbl\tkop_cat_packed");
                emit("\tadd\tsp, sp, #32");
                return 1;
            }
            /* sequal / sne → kop_sequal_packed (v1.18) */
            emit("\tldr\tx0, [sp, #16]");       /* a */
            emit("\tldr\tx1, [sp]");            /* b */
            emit("\tbl\tkop_sequal_packed");
            emit("\tadd\tsp, sp, #32");
            if (e->ival == 24) emit("\teor\tw0, w0, #1");
            return 0;
        }
        if (e->ival >= 25 && e->ival <= 32) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                push_val(e->ival == 32 && idx == 0);
            }
            if (e->ival == 25) {             /* listen(fd, backlog) */
                emit("\tldr\tw0, [sp, #16]");
                emit("\tldr\tw1, [sp, #0]");
                emit("\tmov\tx8, #201");
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #32");
                return 0;
            }
            if (e->ival == 26) {             /* accept(fd) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tmov\tx1, #0");
                emit("\tmov\tx2, #0");
                emit("\tmov\tx3, #0");
                emit("\tmov\tx8, #242");            /* __NR_accept4 */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 27) {             /* setsockopt(fd,lvl,opt,val) */
                emit("\tldr\tw0, [sp, #48]");
                emit("\tldr\tw1, [sp, #32]");
                emit("\tldr\tw2, [sp, #16]");
                emit("\tldr\tw3, [sp, #0]");
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tstr\tw3, [sp]");
                emit("\tmov\tx3, sp");
                emit("\tmov\tx8, #208");
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                emit("\tadd\tsp, sp, #64");
                return 0;
            }
            if (e->ival == 28) {             /* memmark() */
                emit("\tadrp\tx9, kop_arena_used");
                emit("\tadd\tx9, x9, :lo12:kop_arena_used");
                emit("\tldr\tw0, [x9]");
                return 0;
            }
            if (e->ival == 29) {             /* memreset(mark) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tadrp\tx9, kop_arena_used");
                emit("\tadd\tx9, x9, :lo12:kop_arena_used");
                emit("\tcmp\tw0, #0");
                emit("\tb.ge\t.LMR%dP", ln);
                emit("\tmov\tw0, wzr");
                emit(".LMR%dP:", ln);
                emit("\tldr\tw10, [x9]");
                emit("\tcmp\tw0, w10");
                emit("\tb.le\t.LMR%dS", ln);
                emit("\tmov\tw0, w10");
                emit(".LMR%dS:", ln);
                emit("\tstr\tw0, [x9]");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 30) {             /* argc() */
                emit("\tadrp\tx9, kop_argc");
                emit("\tadd\tx9, x9, :lo12:kop_argc");
                emit("\tldr\tw0, [x9]");
                return 0;
            }
            if (e->ival == 31) {             /* argv(i) → String */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tadrp\tx9, kop_argc");
                emit("\tadd\tx9, x9, :lo12:kop_argc");
                emit("\tldr\tw9, [x9]");
                emit("\tcmp\tw0, #0");
                emit("\tb.lt\t.LAV%dE", ln);
                emit("\tcmp\tw0, w9");
                emit("\tb.hs\t.LAV%dE", ln);
                emit("\tadrp\tx10, kop_argv");
                emit("\tadd\tx10, x10, :lo12:kop_argv");
                emit("\tldr\tx10, [x10]");
                emit("\tsxtw\tx11, w0");
                emit("\tldr\tx0, [x10, x11, lsl #3]");
                emit("\tbl\tkop_str_from_cstr");
                emit("\tb\t.LAV%dD", ln);
                emit(".LAV%dE:", ln);
                emit("\tadrp\tx0, .Lempty_str");
                emit("\tadd\tx0, x0, :lo12:.Lempty_str");
                emit("\tbl\tkop_str_from_cstr");
                emit(".LAV%dD:", ln);
                emit("\tadd\tsp, sp, #16");
                return 1;
            }
            if (e->ival == 32) {             /* getenv(name) → String */
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'g', 0);
                emit("\tadrp\tx0, kop_iobuf");
                emit("\tadd\tx0, x0, :lo12:kop_iobuf");
                emit("\tbl\tgetenv");
                emit("\tcbz\tx0, .LGE%dE", ln);
                emit("\tbl\tkop_str_from_cstr");
                emit("\tb\t.LGE%dD", ln);
                emit(".LGE%dE:", ln);
                emit("\tadrp\tx0, .Lempty_str");
                emit("\tadd\tx0, x0, :lo12:.Lempty_str");
                emit("\tbl\tkop_str_from_cstr");
                emit(".LGE%dD:", ln);
                emit("\tadd\tsp, sp, #16");
                return 1;
            }
        }
        if (e->ival == 33) {                /* recv1(fd, buf) → n (satu read) */
            gen_expr(m, e->a);
            push_val(0);
            gen_expr(m, e->a->next);
            push_val(1);
            emit("\tldr\tx10, [sp]");
            emit("\tldr\tw0, [sp, #16]");
            emit("\tldr\tw11, [x10]");
            emit("\tcmp\tw11, #4000");
            emit("\tb.ls\t.LR1%dc", ln);
            emit("\tmov\tw11, #4000");
            emit(".LR1%dc:", ln);
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            emit("\tmov\tx1, x17");
            emit("\tmov\tw2, w11");
            emit("\tmov\tx8, #63");
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #0");
            emit("\tb.le\t.LR1%dd", ln);
            emit("\tmov\tw12, w0");
            emit("\tmov\tw13, #0");
            emit(".LR1%dk:", ln);
            emit("\tcmp\tw13, w12");
            emit("\tb.hs\t.LR1%df", ln);
            emit("\tldrb\tw16, [x17, x13]");
            emit("\tsxtw\tx15, w13");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tstr\tx16, [x10, x15]");
            emit("\tadd\tw13, w13, #1");
            emit("\tb\t.LR1%dk", ln);
            emit(".LR1%df:", ln);
            emit("\tmov\tw0, w12");
            emit(".LR1%dd:", ln);
            emit("\tadd\tsp, sp, #32");
            return 0;
        }
        if (e->ival == 39) {                /* pollfd(fds, revents, ms) */
            gen_expr(m, e->a);
            push_val(1);
            gen_expr(m, e->a->next);
            push_val(1);
            gen_expr(m, e->a->next->next);
            push_val(0);
            emit("\tsub\tsp, sp, #512");
            emit("\tldr\tx10, [sp, #544]");       /* fds */
            emit("\tldr\tw9, [x10]");             /* n */
            emit("\tcmp\tw9, #64");
            emit("\tb.le\t.LPF%dc", ln);
            emit("\tmov\tw9, #64");
            emit(".LPF%dc:", ln);
            emit("\tmov\tx13, #0");
            emit(".LPF%dl:", ln);
            emit("\tcmp\tx13, x9");
            emit("\tb.hs\t.LPF%dd", ln);
            emit("\tsxtw\tx4, w13");
            emit("\tlsl\tx4, x4, #3");
            emit("\tadd\tx6, x10, x4");
            emit("\tldr\tw14, [x6, #8]");
            emit("\tadd\tx6, sp, x4");
            emit("\tstr\tw14, [x6]");
            emit("\tmov\tw14, #1");               /* POLLIN */
            emit("\tstrh\tw14, [x6, #4]");
            emit("\tmov\tw14, #0");
            emit("\tstrh\tw14, [x6, #6]");
            emit("\tadd\tx13, x13, #1");
            emit("\tb\t.LPF%dl", ln);
            emit(".LPF%dd:", ln);
            emit("\tstr\tw9, [sp, #504]");        /* simpan n (caller-saved) */
            emit("\tmov\tx0, sp");
            emit("\tmov\tw1, w9");
            emit("\tldr\tw2, [sp, #512]");
            emit("\tbl\tpoll");
            emit("\tldr\tw9, [sp, #504]");
            emit("\tldr\tx15, [sp, #528]");       /* revents */
            emit("\tmov\tx13, #0");
            emit("\tmov\tw0, #0");                /* jumlah siap */
            emit(".LPF%dk:", ln);
            emit("\tcmp\tx13, x9");
            emit("\tb.hs\t.LPF%df", ln);
            emit("\tsxtw\tx4, w13");
            emit("\tlsl\tx4, x4, #3");
            emit("\tadd\tx6, sp, x4");
            emit("\tldrh\tw16, [x6, #6]");
            emit("\tmov\tw3, #0");
            emit("\tcbz\tw16, .LPF%dw", ln);
            emit("\ttst\tw16, #1");
            emit("\tb.eq\t.LPF%dni", ln);
            emit("\tmov\tw3, #1");
            emit(".LPF%dni:", ln);
            emit("\ttst\tw16, #56");              /* ERR|HUP|NVAL */
            emit("\tb.eq\t.LPF%dne", ln);
            emit("\torr\tw3, w3, #2");
            emit(".LPF%dne:", ln);
            emit(".LPF%dw:", ln);
            emit("\tadd\tx6, x15, x4");
            emit("\tsxtw\tx5, w3");
            emit("\tstr\tx5, [x6, #8]");
            emit("\tcbz\tw3, .LPF%dn", ln);
            emit("\tadd\tw0, w0, #1");
            emit(".LPF%dn:", ln);
            emit("\tadd\tx13, x13, #1");
            emit("\tb\t.LPF%dk", ln);
            emit(".LPF%df:", ln);
            emit("\tadd\tsp, sp, #512");
            emit("\tadd\tsp, sp, #48");
            return 0;
        }
        if (e->ival == 38) {                /* settimeout(fd, ms) */
            gen_expr(m, e->a);
            push_val(0);
            gen_expr(m, e->a->next);
            push_val(0);
            emit("\tldr\tw1, [sp, #0]");          /* ms */
            emit("\tmov\tw3, #1000");
            emit("\tudiv\tw2, w1, w3");           /* detik */
            emit("\tmsub\tw4, w2, w3, w1");       /* sisa ms */
            emit("\tmov\tw5, #1000");
            emit("\tmul\tw4, w4, w5");            /* mikrodetik */
            emit("\tstp\txzr, xzr, [sp, #-16]!");
            emit("\tsxtw\tx2, w2");
            emit("\tstr\tx2, [sp]");
            emit("\tsxtw\tx4, w4");
            emit("\tstr\tx4, [sp, #8]");
            emit("\tldr\tw0, [sp, #32]");         /* fd */
            emit("\tmov\tx1, #1");                /* SOL_SOCKET */
            emit("\tmov\tx2, #20");               /* SO_RCVTIMEO */
            emit("\tmov\tx3, sp");
            emit("\tmov\tx4, #16");
            emit("\tmov\tx8, #208");
            emit("\tsvc\t#0");
            emit("\tldr\tw0, [sp, #32]");
            emit("\tmov\tx1, #1");
            emit("\tmov\tx2, #21");               /* SO_SNDTIMEO */
            emit("\tmov\tx3, sp");
            emit("\tmov\tx4, #16");
            emit("\tmov\tx8, #208");
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #16");
            emit("\tadd\tsp, sp, #32");
            return 0;
        }
        if (e->ival == 37) {                /* localport(fd) -> port | -1 */
            gen_expr(m, e->a);
            push_val(0);
            emit("\tsub\tsp, sp, #32");
            emit("\tmov\tw9, #16");
            emit("\tstr\tw9, [sp]");
            emit("\tadd\tx1, sp, #16");
            emit("\tldr\tw0, [sp, #32]");
            emit("\tmov\tx2, sp");
            emit("\tmov\tx8, #204");          /* __NR_getsockname */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #0");
            emit("\tb.lt\t.LPORT%de", ln);
            emit("\tldrb\tw3, [sp, #18]");
            emit("\tldrb\tw4, [sp, #19]");
            emit("\tlsl\tw3, w3, #8");
            emit("\torr\tw0, w3, w4");
            emit("\tb\t.LPORT%dd", ln);
            emit(".LPORT%de:", ln);
            emit("\tmov\tw0, #-1");
            emit(".LPORT%dd:", ln);
            emit("\tadd\tsp, sp, #48");
            return 0;
        }
        if (e->ival == 34) {                /* setnonblock(fd) -> 0/-errno */
            gen_expr(m, e->a);
            push_val(0);
            emit("\tldr\tw0, [sp, #0]");
            emit("\tmov\tx1, #4");            /* F_SETFL */
            emit("\tmov\tx2, #2048");         /* O_NONBLOCK */
            emit("\tmov\tx8, #25");           /* __NR_fcntl */
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #16");
            return 0;
        }
        if (e->ival == 35) {                /* key() -> byte | -1 */
            emit("\tmov\tx0, #0");
            emit("\tmov\tx1, #4");
            emit("\tmov\tx2, #2048");
            emit("\tmov\tx8, #25");
            emit("\tsvc\t#0");
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            emit("\tmov\tx0, #0");
            emit("\tmov\tx1, x17");
            emit("\tmov\tx2, #1");
            emit("\tmov\tx8, #63");           /* __NR_read */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #1");
            emit("\tb.ne\t.LKEY%de", ln);
            emit("\tldrb\tw0, [x17]");
            emit("\tb\t.LKEY%dd", ln);
            emit(".LKEY%de:", ln);
            emit("\tmov\tw0, #-1");
            emit(".LKEY%dd:", ln);
            return 0;
        }
        if (e->ival == 36) {                /* ttyRaw(on) -> 0 */
            gen_expr(m, e->a);
            push_val(0);
            emit("\tldr\tw0, [sp, #0]");
            emit("\tcbz\tw0, .LTTY%ds", ln);
            emit("\tadrp\tx0, .Lstty_raw");
            emit("\tadd\tx0, x0, :lo12:.Lstty_raw");
            emit("\tb\t.LTTY%dc", ln);
            emit(".LTTY%ds:", ln);
            emit("\tadrp\tx0, .Lstty_sane");
            emit("\tadd\tx0, x0, :lo12:.Lstty_sane");
            emit(".LTTY%dc:", ln);
            emit("\tbl\tsystem");
            emit("\tmov\tw0, #0");
            emit("\tadd\tsp, sp, #16");
            return 0;
        }
        if (e->ival == 40 || e->ival == 41) { /* mkstr(int[])→String | strbytes(String)→int[] */
            /* v1.18: konversi representasi via helper runtime */
            gen_expr(m, e->a);
            push_val(1);
            emit("\tldr\tx0, [sp]");
            emit("\tbl\t%s", e->ival == 40 ? "kop_mkstr" : "kop_strbytes");
            emit("\tadd\tsp, sp, #16");
            return 0;
        }
        int n = 0;
        for (Node *aa = e->a; aa; aa = aa->next) {
            gen_expr(m, aa);
            push_val(0);                    /* arg selalu int */
            n++;
        }
        if (n <= 0) return 0;
        if (e->ival == 0) {                 /* socket(domain, type) */
            emit("\tldr\tw0, [sp, #%d]", (n - 1 - 0) * 16);
            emit("\tldr\tw1, [sp, #0]");
            emit("\tadd\tsp, sp, #%d", n * 16);
            emit("\tmov\tw2, #0");                     /* proto = 0 */
            emit("\tmov\tx8, #198");                   /* __NR_socket */
            emit("\tsvc\t#0");
        } else if (e->ival == 1) {          /* connect(fd, ipBE, port) */
            emit("\tstp\txzr, xzr, [sp, #-16]!");      /* sockaddr 16B nol */
            emit("\tldrb\tw4, [sp, #36]");             /* ip b0  (a) */
            emit("\tldr\tw6, [sp, #32]");              /* ipBE */
            emit("\tldr\tw7, [sp, #16]");              /* port */
            emit("\tlsr\tw4, w7, #8");
            emit("\tstrb\tw4, [sp, #2]");              /* port hi */
            emit("\tand\tw4, w7, #0xff");
            emit("\tstrb\tw4, [sp, #3]");              /* port lo */
            emit("\tlsr\tw4, w6, #24");
            emit("\tstrb\tw4, [sp, #4]");
            emit("\tlsr\tw4, w6, #16");
            emit("\tand\tw4, w4, #0xff");
            emit("\tstrb\tw4, [sp, #5]");
            emit("\tlsr\tw4, w6, #8");
            emit("\tand\tw4, w4, #0xff");
            emit("\tstrb\tw4, [sp, #6]");
            emit("\tand\tw4, w6, #0xff");
            emit("\tstrb\tw4, [sp, #7]");
            emit("\tmovz\tw4, #2");                    /* AF_INET */
            emit("\tstrb\tw4, [sp, #0]");
            emit("\tldr\tx0, [sp, #48]");              /* fd  (16+32) */
            emit("\tmov\tx1, sp");                     /* &sa */
            emit("\tmov\tw2, #16");
            emit("\tmov\tx8, #203");                   /* __NR_connect */
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #64");                /* sockaddr + 3 arg */
        } else if (e->ival == 2) {          /* rdb(fd) */
            emit("\tstp\txzr, xzr, [sp, #-16]!");      /* buffer 1B */
            emit("\tldr\tx0, [sp, #16]");              /* fd */
            emit("\tmov\tx1, sp");
            emit("\tmov\tw2, #1");
            emit("\tmov\tx8, #63");                    /* __NR_read */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #1");
            emit("\tb.ne\t1f");
            emit("\tldrb\tw0, [sp]");                  /* byte → 0..255 */
            emit("\tb\t2f");
            emit("1:");
            emit("\tmov\tw0, #-1");                    /* EOF / error → -1 */
            emit("2:");
            emit("\tadd\tsp, sp, #32");
        } else if (e->ival == 3) {          /* wrb(fd, b) */
            /* stack: [16..31]=b (push terakhir), [32..47]=fd */
            emit("\tstp\txzr, xzr, [sp, #-16]!");
            emit("\tldr\tw2, [sp, #16]");              /* byte */
            emit("\tstrb\tw2, [sp]");
            emit("\tldr\tx0, [sp, #32]");              /* fd */
            emit("\tmov\tx1, sp");
            emit("\tmov\tw2, #1");
            emit("\tmov\tx8, #64");                    /* __NR_write */
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #32");
            emit("\tcmp\tx0, #1");
            emit("\tb.ne\t4f");
            emit("\tmov\tx0, xzr");
            emit("4:");
        } else if (e->ival == 4) {          /* close(fd) */
            emit("\tldr\tx0, [sp, #0]");
            emit("\tadd\tsp, sp, #%d", n * 16);
            emit("\tmov\tx8, #57");                    /* __NR_close */
            emit("\tsvc\t#0");
        }
        return 0;                           /* hasil int di w0/x0 */
    }
    case EX_ENVLOAD:
        emit("\tldr\tx0, [x29, #-8]");   /* slot 0 = param $env */
        if (is64ty((Ty)e->dty)) {
            emit("\tldr\tx0, [x0, #%ld]", 24 + 8 * (long)e->ival);
            return 1;
        }
        emit("\tldr\tw0, [x0, #%ld]", 24 + 8 * (long)e->ival);
        return 0;
    case EX_CAST: {
        Ty from = (Ty)e->a->dty, to = (Ty)e->dty;
        gen_expr(m, e->a);
        widen_ty(from);
        conv_ty(from, to);
        return is64ty(to);
    }
    case EX_TERN: {
        int lelse = nlab++, lend = nlab++;
        gen_expr(m, e->a);
        emit("\tcbz\tw0, .L%d", lelse);
        int w1 = gen_expr(m, e->b);
        emit("\tb\t.L%d", lend);
        emit(".L%d:", lelse);
        int w2 = gen_expr(m, e->c);
        emit(".L%d:", lend);
        return (w1 && w2) ? 1 : is64ty((Ty)e->dty);
    }
    case EX_BIN: {
        Ty bl = (Ty)e->a->dty, br = (Ty)e->b->dty, bres = (Ty)e->dty;
        if (!((bl == T_INT || bl == T_BOOL) && (br == T_INT || br == T_BOOL) &&
              (bres == T_INT || bres == T_BOOL)))
            return gen_bin_typed(m, e);
        char op = e->str[0];
        if ((op == '&' || op == '|') && e->str[1] != 0) {
            /* hasil 0/1 keduanya: and/or langsung karena operand boolean 0/1 */
            int lelse = nlab++, lend = nlab++;
            gen_expr(m, e->a);
            if (op == '&') {
                emit("\tcbz\tw0, .L%d", lelse);
                gen_expr(m, e->b);
                emit("\tcbz\tw0, .L%d", lelse);
                emit("\tmov\tw0, #1");
                emit("\tb\t.L%d", lend);
            } else {
                emit("\tcbnz\tw0, .L%d", lelse);
                gen_expr(m, e->b);
                emit("\tcbnz\tw0, .L%d", lelse);
                emit("\tmov\tw0, wzr");
                emit("\tb\t.L%d", lend);
            }
            emit(".L%d:", lelse);
            if (op == '&') emit("\tmov\tw0, wzr");
            else emit("\tmov\tw0, #1");
            emit(".L%d:", lend);
            return 0;
        }
        gen_expr(m, e->a);
        push_w0();
        gen_expr(m, e->b);
        pop_w("w9");
        /* operator 2-karakter dulu supaya tidak tabrak case '<' / '>' */
        if (!strcmp(e->str, "<=")) { emit("\tcmp\tw9, w0"); emit("\tcset\tw0, le"); break; }
        if (!strcmp(e->str, ">=")) { emit("\tcmp\tw9, w0"); emit("\tcset\tw0, ge"); break; }
        if (!strcmp(e->str, "<<")) { emit("\tlslv\tw0, w9, w0"); break; }   /* w9=left w0=right */
        if (!strcmp(e->str, ">>")) { emit("\tasrv\tw0, w9, w0"); break; }
        if (!strcmp(e->str, ">>>")) { emit("\tlsrv\tw0, w9, w0"); break; }
        switch (op) {
        case '+': emit("\tadd\tw0, w9, w0"); break;
        case '-': emit("\tsub\tw0, w9, w0"); break;
        case '*': emit("\tmul\tw0, w9, w0"); break;
        case '/':
            emit("\tsdiv\tw0, w9, w0");
            break;
        case '%':
            emit("\tsdiv\tw10, w9, w0");
            emit("\tmsub\tw0, w10, w0, w9");
            break;
        case '<': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, lt"); break;
        case '>': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, gt"); break;
        case '=': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, eq"); break;
        case '!': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, ne"); break;
        case '&': emit("\tand\tw0, w9, w0"); break;     /* bitwise (len 1) */
        case '|': emit("\torr\tw0, w9, w0"); break;
        case '^': emit("\teor\tw0, w9, w0"); break;
        default:
            die("gen_a64: operator '%s'", e->str);
        }
        return 0;
    }
    default:
        die("gen_a64: ekspresi kind=%d", e->kind);
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

void gen_a64(const char *outpath) {
    out = fopen(outpath, "w");
    if (!out) die("tidak bisa menulis %s", outpath);
    nlab = 0;
    emit(".arch armv8-a");
    emit(".text");

    /* prototipe printf supaya perakitan aman (tipe tak dipakai) */
    for (Method *m = prog.methods; m; m = m->next) {
        emit("%s__%s:", m->cls, m->name);
        long nslots = m->nlocals;
        long framew = ((nslots * 8 + 15) / 16) * 16;
        if (nslots > 510) die("metode %s.%s: terlalu banyak local untuk backend a64", m->cls, m->name);
        emit("\tstp\tx29, x30, [sp, #-16]!");
        emit("\tmov\tx29, sp");
        if (framew > 0) emit("\tsub\tsp, sp, #%ld", framew);
        {   /* simpan parameter (register w0..w7) ke slot lokal;
                param T_FN (lambda/env) disalin 64-bit (x0) */
            int i = 0;
            for (Param *p = m->params; p; p = p->next, i++) {
                if (is64ty((Ty)p->ty)) emit("\tstr\tx%d, [x29, #-%ld]", i, (i + 1) * 8);
                else emit("\tstr\tw%d, [x29, #-%ld]", i, (i + 1) * 8);
            }
        }
        brk_sp = 0;
        rg_sp = 0;
        cur_ret_label = nlab++;
        gen_stmts(m, m->body);
        emit("\tmov\tw0, wzr");          /* kembalian default jalur jatuh */
        emit(".Lret%d:", cur_ret_label);
        emit("\tmov\tsp, x29");
        emit("\tldp\tx29, x30, [sp], #16");
        emit("\tret");
    }

    /* titik masuk ELF/PIE: main() -> Main.main, status exit 0 */
    Method *entry = prog.methods;
    {   /* cari metode main */
        for (Method *m = prog.methods; m; m = m->next)
            if (m->index == prog.mainidx) entry = m;
    }
    emit("\t.globl main");
    emit("main:");
    /* frame main sendiri: x30 harus selamat dari bl malloc di bawah ini,
       kalau tidak epilogue class akan ret kembali ke tengah main (loop) */
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tsub\tw10, w0, #1");            /* buang nama program */
    emit("\tcmp\tw10, #0");
    emit("\tb.ge\t.LARGok");
    emit("\tmov\tw10, wzr");
    emit(".LARGok:");
    emit("\tadrp\tx9, kop_argc");
    emit("\tadd\tx9, x9, :lo12:kop_argc");
    emit("\tstr\tw10, [x9]");
    emit("\tadrp\tx9, kop_argv");
    emit("\tadd\tx9, x9, :lo12:kop_argv");
    emit("\tadd\tx11, x1, #8");
    emit("\tstr\tx11, [x9]");
    /* init arena: blok 32 MB sekali via malloc (X25519 ladder mengalokasi
     * ~2,5 MB per panggilan; 6 vektor RFC butuh ~15 MB) */
    emit_imm64("x0", 32UL * 1024 * 1024);   /* arena global default 32 MB */
    emit("\tbl\tmalloc");
    emit("\tadrp\tx1, kop_arena_base");
    emit("\tstr\tx0, [x1, :lo12:kop_arena_base]");
    emit_imm64("x9", 32UL * 1024 * 1024);
    emit("\tadrp\tx1, kop_arena_cap");
    emit("\tstr\tx9, [x1, :lo12:kop_arena_cap]");
    emit("\tmov\tw0, wzr");                 /* arg main (abaikan argc/argv) */
    emit("\tbl\t%s__%s", entry->cls, entry->name);   /* class main: ret balik */
    emit("\tmov\tw0, wzr");                 /* exit status 0 */
    emit("\tmov\tsp, x29");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* stub mati: jumlah argumen lambda tidak cocok (didapat lari b.ne .Klam) */
    emit(".Klam:");
    emit("\tadrp\tx0, :got:.Lmsg_lam");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_lam]");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    emit(".Karr:");
    emit("\tadrp\tx0, :got:.Lmsg_arr");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_arr]");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    emit(".Kbnd:");
    emit("\tadrp\tx0, :got:.Lmsg_bnd");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_bnd]");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    emit(".Koom:");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tldr\tx1, [x9]");
    emit("\tmovz\tx10, #0x10, lsl #16");     /* 1 MB */
    emit("\tcmp\tx1, x10");
    emit("\tb.hs\t.KoomMB");
    emit("\tlsr\tx1, x1, #10");              /* < 1 MB → KB */
    emit("\tadrp\tx0, :got:.Lmsg_oom_kb");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom_kb]");
    emit("\tb\t.KoomPrint");
    emit(".KoomMB:");
    emit("\tlsr\tx1, x1, #20");              /* ≥ 1 MB → MB */
    emit("\tadrp\tx0, :got:.Lmsg_oom");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom]");
    emit(".KoomPrint:");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    /* kopalloc: x0 = byte yang diminta -> x0 = pointer dari arena bump */
    emit(".text");
    emit("\t.globl kopalloc");
    emit("kopalloc:");
    emit("\tadrp\tx1, kop_arena_used");
    emit("\tadd\tx1, x1, :lo12:kop_arena_used");
    emit("\tldr\tx2, [x1]");
    emit("\tadd\tx2, x2, x0");
    emit("\tadrp\tx3, kop_arena_cap");
    emit("\tadd\tx3, x3, :lo12:kop_arena_cap");
    emit("\tldr\tx3, [x3]");
    emit("\tcmp\tx2, x3");
    emit("\tb.ge\t.Koom");
    emit("\tstr\tx2, [x1]");
    emit("\tsub\tx2, x2, x0");
    emit("\tadrp\tx3, kop_arena_base");
    emit("\tadd\tx3, x3, :lo12:kop_arena_base");
    emit("\tldr\tx3, [x3]");
    emit("\tadd\tx0, x3, x2");
    emit("\tret");

    /* v1.21: kop_arena_enter(x0 = byte) -> x1 = handle arena baru.
       Handle menyimpan (base, cap, used) arena luar; arena baru di-malloc
       dan dibebaskan oleh kop_arena_leave. */
    emit(".text");
    emit("\t.globl kop_arena_enter");
    emit("kop_arena_enter:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");                    /* ukuran byte */
    emit("\tmov\tx0, #32");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .Koom");                  /* gagal alokasi handle */
    emit("\tmov\tx20, x0");
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #0]");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #8]");
    emit("\tadrp\tx9, kop_arena_used");
    emit("\tadd\tx9, x9, :lo12:kop_arena_used");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #16]");
    emit("\tmov\tx0, x19");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .KenterOom");
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tstr\tx0, [x9]");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tstr\tx19, [x9]");
    emit("\tadrp\tx9, kop_arena_used");
    emit("\tadd\tx9, x9, :lo12:kop_arena_used");
    emit("\tstr\txzr, [x9]");
    emit("\tmov\tx1, x20");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");
    emit(".KenterOom:");
    emit("\tmov\tx0, x20");
    emit("\tbl\tfree");
    emit("\tb\t.Koom");

    /* v1.21: kop_arena_leave(x1 = handle) — bebas arena blok, pulihkan
       arena luar; x0 (nilai balik) dipertahankan. */
    emit(".text");
    emit("\t.globl kop_arena_leave");
    emit("kop_arena_leave:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");                    /* simpan nilai balik */
    emit("\tmov\tx20, x1");                    /* handle */
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tldr\tx0, [x9]");
    emit("\tbl\tfree");
    emit("\tldr\tx9, [x20, #0]");
    emit("\tadrp\tx10, kop_arena_base");
    emit("\tadd\tx10, x10, :lo12:kop_arena_base");
    emit("\tstr\tx9, [x10]");
    emit("\tldr\tx9, [x20, #8]");
    emit("\tadrp\tx10, kop_arena_cap");
    emit("\tadd\tx10, x10, :lo12:kop_arena_cap");
    emit("\tstr\tx9, [x10]");
    emit("\tldr\tx9, [x20, #16]");
    emit("\tadrp\tx10, kop_arena_used");
    emit("\tadd\tx10, x10, :lo12:kop_arena_used");
    emit("\tstr\tx9, [x10]");
    emit("\tmov\tx0, x20");
    emit("\tbl\tfree");
    emit("\tmov\tx0, x19");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    /* x0 = char* C → x0 = String packed (v1.18: 8 byte per slot) */
    emit(".text");
    emit("\t.globl kop_str_from_cstr");
    emit("kop_str_from_cstr:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tmov\tw2, #0");
    emit(".LSCa:");
    emit("\tldrb\tw1, [x9, x2]");
    emit("\tcbz\tw1, .LSCb");
    emit("\tadd\tw2, w2, #1");
    emit("\tb\t.LSCa");
    emit(".LSCb:");
    emit("\tstp\tx9, x2, [sp, #-16]!");
    emit("\tsxtw\tx10, w2");                       /* n */
    emit("\tadd\tx11, x10, #7");
    emit("\tlsr\tx11, x11, #3");                   /* slots */
    emit("\tlsl\tx0, x11, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tldp\tx9, x2, [sp], #16");
    emit("\tsxtw\tx10, w2");                       /* n */
    emit("\tadd\tx11, x10, #7");
    emit("\tlsr\tx11, x11, #3");                   /* slots (recompute) */
    emit("\tmov\tx12, #0");
    emit("\tadd\tx13, x0, #8");                    /* basis slot data */
    emit(".LSCz:");
    emit("\tcmp\tx12, x11");
    emit("\tb.hs\t.LSCw");
    emit("\tstr\txzr, [x13, x12, lsl #3]");
    emit("\tadd\tx12, x12, #1");
    emit("\tb\t.LSCz");
    emit(".LSCw:");
    emit("\tstr\tx10, [x0]");                      /* len = n */
    emit("\tmov\tx12, #0");
    emit(".LSCc:");
    emit("\tcmp\tx12, x10");
    emit("\tb.hs\t.LSCd");
    emit("\tldrb\tw1, [x9, x12]");
    emit("\tand\tx13, x12, #7");
    emit("\tlsl\tx13, x13, #3");                   /* shift */
    emit("\tlsr\tx14, x12, #3");
    emit("\tlsl\tx14, x14, #3");
    emit("\tadd\tx14, x14, #8");                   /* slot offset */
    emit("\tldr\tx15, [x0, x14]");
    emit("\tlsl\tx16, x1, x13");
    emit("\torr\tx15, x15, x16");
    emit("\tstr\tx15, [x0, x14]");
    emit("\tadd\tx12, x12, #1");
    emit("\tb\t.LSCc");
    emit(".LSCd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* ── v1.18: runtime String packed ───────────────────────────── */

    /* x0 = int[] byte-per-slot → x0 = String packed (byte 0..255) */
    emit(".text");
    emit("\t.globl kop_mkstr");
    emit("kop_mkstr:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tldr\tx10, [x9]");                      /* n */
    emit("\tadd\tx11, x10, #7");
    emit("\tlsr\tx11, x11, #3");                   /* slots */
    emit("\tlsl\tx0, x11, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tmov\tx12, #0");
    emit("\tadd\tx14, x0, #8");                    /* basis slot data */
    emit(".LMKz:");
    emit("\tcmp\tx12, x11");
    emit("\tb.hs\t.LMKw");
    emit("\tstr\txzr, [x14, x12, lsl #3]");
    emit("\tadd\tx12, x12, #1");
    emit("\tb\t.LMKz");
    emit(".LMKw:");
    emit("\tstr\tx10, [x0]");
    emit("\tmov\tx13, #0");
    emit(".LMKl:");
    emit("\tcmp\tx13, x10");
    emit("\tb.hs\t.LMKd");
    emit("\tadd\tx14, x9, #8");
    emit("\tldr\tx14, [x14, x13, lsl #3]");        /* byte-per-slot src */
    emit("\tcmp\tx14, #255");
    emit("\tb.hi\t.Kbnd");
    emit("\tand\tx15, x13, #7");
    emit("\tlsl\tx15, x15, #3");                   /* shift */
    emit("\tlsl\tx14, x14, x15");
    emit("\tlsr\tx16, x13, #3");
    emit("\tlsl\tx16, x16, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tldr\tx17, [x0, x16]");
    emit("\torr\tx17, x17, x14");
    emit("\tstr\tx17, [x0, x16]");
    emit("\tadd\tx13, x13, #1");
    emit("\tb\t.LMKl");
    emit(".LMKd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0 = String packed → x0 = int[] byte-per-slot */
    emit(".text");
    emit("\t.globl kop_strbytes");
    emit("kop_strbytes:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tldr\tx10, [x9]");                      /* n */
    emit("\tlsl\tx0, x10, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tstr\tx10, [x0]");
    emit("\tmov\tx11, #0");
    emit(".LSB1:");
    emit("\tcmp\tx11, x10");
    emit("\tb.hs\t.LSB2");
    emit("\tlsr\tx13, x11, #3");
    emit("\tlsl\tx13, x13, #3");
    emit("\tadd\tx13, x13, #8");
    emit("\tldr\tx14, [x9, x13]");                 /* slot packed */
    emit("\tand\tx15, x11, #7");
    emit("\tlsl\tx15, x15, #3");                   /* shift */
    emit("\tlsr\tx14, x14, x15");
    emit("\tand\tx14, x14, #0xff");
    emit("\tlsl\tx16, x11, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tstr\tx14, [x0, x16]");
    emit("\tadd\tx11, x11, #1");
    emit("\tb\t.LSB1");
    emit(".LSB2:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0, x1 = String packed → x0 = concat packed */
    emit(".text");
    emit("\t.globl kop_cat_packed");
    emit("kop_cat_packed:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tmov\tx10, x1");
    emit("\tldr\tx11, [x9]");                      /* la */
    emit("\tldr\tx12, [x10]");                     /* lb */
    emit("\tadd\tx13, x11, x12");                  /* total */
    emit("\tadd\tx14, x13, #7");
    emit("\tlsr\tx14, x14, #3");                   /* slots */
    emit("\tstp\tx9, x10, [sp, #-16]!");
    emit("\tstp\tx11, x12, [sp, #-16]!");
    emit("\tlsl\tx0, x14, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tldp\tx11, x12, [sp], #16");
    emit("\tldp\tx9, x10, [sp], #16");
    emit("\tmov\tx15, #0");
    emit("\tadd\tx1, x0, #8");                     /* basis slot data */
    emit(".LCz:");
    emit("\tcmp\tx15, x14");
    emit("\tb.hs\t.LCw");
    emit("\tstr\txzr, [x1, x15, lsl #3]");
    emit("\tadd\tx15, x15, #1");
    emit("\tb\t.LCz");
    emit(".LCw:");
    emit("\tstr\tx13, [x0]");                      /* len = total */
    emit("\tmov\tx15, #0");                        /* i untuk a */
    emit(".LCa:");
    emit("\tcmp\tx15, x11");
    emit("\tb.hs\t.LCb");
    emit("\tlsr\tx16, x15, #3");
    emit("\tlsl\tx16, x16, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tldr\tx17, [x9, x16]");
    emit("\tand\tx1, x15, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsr\tx17, x17, x1");
    emit("\tand\tx17, x17, #0xff");                /* getb(a, i) */
    emit("\tlsr\tx2, x15, #3");
    emit("\tlsl\tx2, x2, #3");
    emit("\tadd\tx2, x2, #8");
    emit("\tldr\tx3, [x0, x2]");
    emit("\tand\tx1, x15, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsl\tx17, x17, x1");
    emit("\torr\tx3, x3, x17");
    emit("\tstr\tx3, [x0, x2]");
    emit("\tadd\tx15, x15, #1");
    emit("\tb\t.LCa");
    emit(".LCb:");
    emit("\tmov\tx15, #0");                        /* j untuk b */
    emit(".LCc:");
    emit("\tcmp\tx15, x12");
    emit("\tb.hs\t.LCd");
    emit("\tlsr\tx16, x15, #3");
    emit("\tlsl\tx16, x16, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tldr\tx17, [x10, x16]");
    emit("\tand\tx1, x15, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsr\tx17, x17, x1");
    emit("\tand\tx17, x17, #0xff");                /* getb(b, j) */
    emit("\tadd\tx5, x15, x11");                   /* pos = la + j */
    emit("\tlsr\tx2, x5, #3");
    emit("\tlsl\tx2, x2, #3");
    emit("\tadd\tx2, x2, #8");
    emit("\tldr\tx3, [x0, x2]");
    emit("\tand\tx1, x5, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsl\tx17, x17, x1");
    emit("\torr\tx3, x3, x17");
    emit("\tstr\tx3, [x0, x2]");
    emit("\tadd\tx15, x15, #1");
    emit("\tb\t.LCc");
    emit(".LCd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0, x1 = String packed → w0 = isi sama (1/0) */
    emit(".text");
    emit("\t.globl kop_sequal_packed");
    emit("kop_sequal_packed:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tldr\tx9, [x0]");                       /* la */
    emit("\tldr\tx10, [x1]");                      /* lb */
    emit("\tcmp\tx9, x10");
    emit("\tb.ne\t.LSQn");
    emit("\tmov\tx11, #0");
    emit(".LSQ1:");
    emit("\tcmp\tx11, x9");
    emit("\tb.hs\t.LSQy");
    emit("\tlsr\tx12, x11, #3");
    emit("\tlsl\tx12, x12, #3");
    emit("\tadd\tx12, x12, #8");
    emit("\tldr\tx13, [x0, x12]");
    emit("\tand\tx14, x11, #7");
    emit("\tlsl\tx14, x14, #3");
    emit("\tlsr\tx13, x13, x14");
    emit("\tand\tx13, x13, #0xff");
    emit("\tldr\tx15, [x1, x12]");
    emit("\tlsr\tx15, x15, x14");
    emit("\tand\tx15, x15, #0xff");
    emit("\tcmp\tx13, x15");
    emit("\tb.ne\t.LSQn");
    emit("\tadd\tx11, x11, #1");
    emit("\tb\t.LSQ1");
    emit(".LSQy:");
    emit("\tmov\tw0, #1");
    emit("\tb\t.LSQd");
    emit(".LSQn:");
    emit("\tmov\tw0, #0");
    emit(".LSQd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");


    emit("\t.section .data");
    emit("\t.align 3");
    emit("kop_arena_base: .quad 0");
    emit("kop_arena_cap: .quad 0");
    emit("kop_arena_used: .quad 0");
    emit("kop_argc: .quad 0");
    emit("kop_argv: .quad 0");
    emit(".comm kop_iobuf, 4096, 32");      /* penyangga syscall rekursi builtin */

    emit("\t.section .rodata");
    emit(".Lfmt_i: .asciz \"%s\"", "%d");
    emit(".Lfmt_in: .asciz \"%s\"", "%d\\n");
    emit(".Lfmt_s: .asciz \"%s\"", "%s");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    emit(".Lmsg_arr: .asciz \"%s\"", "kopi: array tidak valid (indeks/ukuran salah)\\n");
    emit(".Lmsg_bnd: .asciz \"%s\"", "kopi: byte mkstr di luar 0..255\\n");
    emit(".Lmsg_oom: .asciz \"%s\"", "kopi: arena malloc habis — batas %ld MB terlampaui\\n");
    emit(".Lmsg_oom_kb: .asciz \"%s\"", "kopi: arena malloc habis — batas %ld KB terlampaui\\n");
    emit(".Lempty_str: .asciz \"%s\"", "");
    emit(".Lstty_raw: .asciz \"%s\"", "stty raw -echo 2>/dev/null");
    emit(".Lstty_sane: .asciz \"%s\"", "stty sane 2>/dev/null");
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
    for (int i = 0; i < prog.ncp; i++) {
        char esc[1200];
        asm_escape(prog.cpool[i], esc, sizeof esc);
        emit("cpool%d:", i);
        emit("\t.asciz \"%s\"", esc);
    }
    fclose(out);
}
