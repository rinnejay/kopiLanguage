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
        int v64 = ty_width(vt) == 64 || vt == T_ARI || vt == T_STR || vt == T_FN ||
                  vt == T_LNGARR || vt == T_STRARR;   /* v1.29: array = pointer 64-bit */
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
#include "gen_a64_expr.inc"

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

static void gen_a64_runtime(void);

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

    gen_a64_runtime();
}

/* fragmen runtime a64 (helper assembly + data) */
#include "gen_a64_rt.inc"
