/* Backend VM: memancarkan bytecode "KOP1" untuk dieksekusi kopivm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

#define MAXBUF (1 << 22)

enum {
    OP_HALT = 0, OP_ICONST, OP_ILOAD, OP_ISTORE, OP_POP,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_NOT,
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
    OP_JMP, OP_JMPF, OP_JMPT, OP_CALL, OP_RET,
    OP_PRINTILN, OP_PRINTI, OP_PRINTSLN, OP_PRINTS,
    OP_NEWCLO, OP_CALLIND, OP_ENVLOAD, OP_IDXR, OP_IDXW, OP_ALEN,
    OP_NEWARR,                      /* lambda/closure + array arena */
    OP_NET,                         /* builtin Net.* (syscall host) */
    OP_BAND, OP_BOR, OP_BXOR, OP_SHL, OP_SHR, OP_USHR, OP_BNOT,   /* v1.5 bitwise */
    OP_ICONST64,
    OP_ADD64, OP_SUB64, OP_MUL64, OP_DIV64, OP_MOD64,
    OP_UDIV64, OP_UMOD64, OP_AND64, OP_OR64, OP_XOR64,
    OP_SHL64, OP_SHR64, OP_USHR64, OP_NEG64, OP_BNOT64,
    OP_LT64, OP_GT64, OP_LE64, OP_GE64,
    OP_LTU64, OP_GTU64, OP_LEU64, OP_GEU64,
    OP_EQ64, OP_NE64,
    OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV, OP_FMOD, OP_FNEG,
    OP_FLT, OP_FGT, OP_FLE, OP_FGE, OP_FEQ, OP_FNE,
    OP_CONV, OP_NORM, OP_PRINTNUM, OP_PRINTBUF,
    OP_LINE, OP_SRCFILE,            /* v1.13: baris sumber utk stack trace */
    OP_IDXB, OP_PRINTSTR,           /* v1.18: String packed */
    OP_MARK, OP_MRESET,             /* v1.20: memMark/memReset manual */
    OP_RENTER, OP_RLEAVE,           /* v1.21: arena blok malloc<...> */
    /* v1.22: superinstruksi loop panas (backend VM saja) */
    OP_CMP_JMPT, OP_CMP_JMPF,       /* [type][cmp][slot:2][imm:4][target:4] */
    OP_IINC,                        /* [type][slot:2][delta:4] */
    OP_BIN_L,                       /* [type][op][slotA:2][slotB:2] */
    OP_OP_STORE,                    /* [type][op][slot:2] — slot = slot op pop() */
    OP_IDXR_L,                      /* [slotArr:2][slotIdx:2] */
    OP_IDXW_L,                      /* [slotArr:2][slotIdx:2] — pop nilai */
    OP_LOOP                         /* [type][cmp][slot:2][delta:4][imm:4][target:4] */
};

typedef struct {
    uint8_t buf[MAXBUF];
    int len;
    struct { int pos, lab; } fixes[65536];
    int nfixes;
    int labpos[65536];
    int nlab;
} Ctx;

static Ctx cx;
static Method *cur_meth;
static int brk_lab[64], cont_lab[64], brk_sp, last_line;
/* v1.20: malloc{} — slot penanda arena aktif + rg_sp saat loop masuk */
static int rg_slot[64], rg_sp, loop_rg[64];

static const char *last_file;

int cp_intern(const char *s);    /* parse.c — intern nama file ke cpool */

static void em8(int v) { cx.buf[cx.len++] = (uint8_t)v; }
static void em16(int v) { em8(v & 0xff); em8((v >> 8) & 0xff); }
static void em32(long v) {
    em8((int)(v & 0xff)); em8((int)((v >> 8) & 0xff));
    em8((int)((v >> 16) & 0xff)); em8((int)((v >> 24) & 0xff));
}

static void emit_conv(Ty from, Ty to) {
    if (from == to) return;
    em8(OP_CONV); em8((int)from); em8((int)to);
}
static void emit_norm(Ty t) { em8(OP_NORM); em8((int)t); }

/* keluar arena blok: bebaskan arena dalam, pulihkan arena luar */
static void emit_region_leave(int slot) {
    (void)slot;                       /* VM menyimpan konteks di stack internal */
    em8(OP_RLEAVE);
}
/* reset semua region yang dilompati break/continue: yang dibuka SETELAH
 * loop terdekat (kalau ada) */
static void emit_loop_exit_resets(void) {
    int base = brk_sp > 0 ? loop_rg[brk_sp - 1] : 0;
    for (int i = rg_sp - 1; i >= base; i--) emit_region_leave(rg_slot[i]);
}

static int newlab(void) { return cx.nlab++; }
static void mark(int lab) { cx.labpos[lab] = cx.len; }
static void jmp(int op, int lab) {
    em8(op);
    cx.fixes[cx.nfixes].pos = cx.len;
    cx.fixes[cx.nfixes].lab = lab;
    cx.nfixes++;
    em32(0); /* placeholder */
}

static void gen_expr(Node *e);
static void gen_stmts(Node *lst);

#include "gen_vm_fuse.inc"

static void gen_stmt(Node *s) {
    /* direktif baris sumber (v1.13): kopivm menyimpannya per frame untuk
     * stack trace saat error runtime. Nama file di-intern ke cpool hanya
     * ketika berganti — umumnya sekali di awal metode. */
    if (s->line > 0) {
        if (s->line != last_line) { em8(OP_LINE); em16(s->line & 0xffff); last_line = s->line; }
    }
    if (s->file && (!last_file || strcmp(s->file, last_file))) {
        em8(OP_SRCFILE); em16(cp_intern(s->file));
        last_file = s->file;
    }
    switch (s->kind) {
    case ST_VAR:
    case ST_ASSIGN:
        /* v1.22: x = x <op> y → OP_OP_STORE (baca/tulis lokal tanpa stack) */
        if (s->kind == ST_ASSIGN && s->a && s->a->kind == EX_BIN &&
            (Ty)s->dty == T_INT && (Ty)s->a->dty == T_INT &&
            s->a->a->kind == EX_VAR && (int)s->a->a->ival == (int)s->ival &&
            (Ty)s->a->b->dty == T_INT && bin_code(s->a->str) >= 0) {
            gen_expr(s->a->b);
            em8(OP_OP_STORE); em8(0); em8(bin_code(s->a->str));
            em16((int)s->ival);
            break;
        }
        gen_expr(s->a);
        emit_conv((Ty)s->a->dty, (Ty)s->dty);
        em8(OP_ISTORE); em16((int)s->ival);
        break;
    case ST_IF: {
        int lelse = newlab(), lend = newlab();
        gen_expr(s->a);
        jmp(OP_JMPF, lelse);
        gen_stmts(s->b);
        jmp(OP_JMP, lend);
        mark(lelse);
        if (s->c) gen_stmts(s->c);
        mark(lend);
        break;
    }
    case ST_WHILE: {
        int ltop = newlab(), lend = newlab();
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        int slot, cmp, imm;
        int fast = s->a && cond_pat(s->a, &slot, &cmp, &imm);
        loop_rg[brk_sp] = rg_sp;
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = ltop; brk_sp++;
        if (fast) {
            /* v1.22: cek awal sekali, lalu tes di kaki loop (hemat JMP) */
            emit_cond_jmp(s->a, lend, 0);
            mark(ltop);
            gen_stmts(s->b);
            emit_cond_jmp(s->a, ltop, 1);
        } else {
            mark(ltop);
            gen_expr(s->a);
            jmp(OP_JMPF, lend);
            gen_stmts(s->b);
            jmp(OP_JMP, ltop);
        }
        mark(lend);
        brk_sp--;
        brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_DO: {
        int ltop = newlab(), lcont = newlab(), lend = newlab();
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        loop_rg[brk_sp] = rg_sp;
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = lcont; brk_sp++;
        mark(ltop);
        gen_stmts(s->b);
        mark(lcont);
        emit_cond_jmp(s->a, ltop, 1);   /* v1.22: fusi bila pola cocok */
        mark(lend);
        brk_sp--;
        brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_FOR: {
        if (s->a) gen_stmt(s->a);       /* init (ST_VAR / ST_ASSIGN) */
        int ltop = newlab(), lend = newlab(), lcont = newlab();
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        int slot, cmp, imm, delta;
        int fast = s->b && cond_pat(s->b, &slot, &cmp, &imm);
        int inc = fast && step_pat(s->c, slot, &delta);
        loop_rg[brk_sp] = rg_sp;
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = lcont; brk_sp++;
        if (fast) {
            /* v1.22: cek awal sekali; kaki loop = satu opcode OP_LOOP
             * (tambah langkah + tes + lompat balik) bila langkahnya i±k */
            emit_cond_jmp(s->b, lend, 0);
            mark(ltop);
            gen_stmts(s->d);
            mark(lcont);                /* target continue: sebelum langkah */
            if (inc) {
                em8(OP_LOOP); em8(0); em8(cmp); em16(slot);
                em32(delta); em32(imm);
                fix_at(ltop);
            } else {
                if (s->c) gen_stmt(s->c);
                emit_cond_jmp(s->b, ltop, 1);
            }
        } else {
            mark(ltop);
            if (s->b) { gen_expr(s->b); jmp(OP_JMPF, lend); }
            gen_stmts(s->d);
            mark(lcont);
            if (s->c) gen_stmt(s->c);
            jmp(OP_JMP, ltop);
        }
        mark(lend);
        brk_sp--;
        brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_BRK: emit_loop_exit_resets(); jmp(OP_JMP, brk_lab[brk_sp - 1]); break;
    case ST_CONT: emit_loop_exit_resets(); jmp(OP_JMP, cont_lab[brk_sp - 1]); break;
    case ST_RETURN:
        if (s->a) {
            gen_expr(s->a);
            emit_conv((Ty)s->a->dty, cur_meth->ret);
        } else {
            em8(OP_ICONST); em32(0);
        }
        for (int i = rg_sp - 1; i >= 0; i--) emit_region_leave(rg_slot[i]);
        em8(OP_RET);
        break;
    case ST_PRINT:
        if (s->str) {
            em8(s->ival ? OP_PRINTSLN : OP_PRINTS); em16((int)s->cp);
        } else {
            gen_expr(s->a);
            Ty pt = (Ty)s->a->dty;
            if (pt == T_INT || pt == T_BOOL) {
                em8(s->ival ? OP_PRINTILN : OP_PRINTI);
            } else if (pt == T_STR) {
                em8(OP_PRINTSTR); em8(s->ival ? 1 : 0);   /* v1.18: packed */
            } else if (pt == T_ARI) {
                em8(OP_PRINTBUF); em8(s->ival ? 1 : 0);
            } else {
                em8(OP_PRINTNUM); em8((int)pt); em8(s->ival ? 1 : 0);
            }
        }
        break;
    case ST_EXPR:
        if (s->a) { gen_expr(s->a); em8(OP_POP); }
        break;
    case ST_BLOCK:
        gen_stmts(s->d);
        break;

    case ST_REGION:              /* v1.20: malloc { ... } */
        em8(OP_RENTER);          /* v1.21: arena baru sebesar s->cp byte */
        em32(s->cp);
        rg_slot[rg_sp++] = (int)s->ival;
        gen_stmts(s->d);
        rg_sp--;
        emit_region_leave((int)s->ival);
        break;

    case ST_IDXASG:
        gen_expr(s->c);          /* nilai - dipakai paling atas */
        /* v1.22: a[i] = v dengan a dan i variabel lokal → satu opcode */
        if (s->a->kind == EX_VAR && s->b->kind == EX_VAR) {
            em8(OP_IDXW_L);
            em16((int)s->a->ival); em16((int)s->b->ival);
            break;
        }
        gen_expr(s->a);          /* array pointer */
        gen_expr(s->b);          /* idx */
        em8(OP_IDXW);
        break;
    default:
        die("gen_vm: statement kind=%d", s->kind);
    }
}

static void gen_stmts(Node *lst) {
    for (Node *n = lst; n; n = n->next) gen_stmt(n);
}

#include "gen_vm_expr.inc"

static void write_method(FILE *f, Method *m) {
    cur_meth = m;
    cx.len = 0; cx.nfixes = 0; cx.nlab = 0;
    last_line = 0; last_file = 0;
    memset(cx.labpos, 0xff, sizeof(cx.labpos));

    gen_stmts(m->body);
    em8(OP_ICONST); em32(0);   /* kembalian implisit */
    em8(OP_RET);

    /* patch lompatan */
    for (int i = 0; i < cx.nfixes; i++) {
        int pos = cx.fixes[i].pos, lab = cx.fixes[i].lab;
        long target = cx.labpos[lab];
        if (target < 0) die("label %d tidak didefinisikan", lab);
        cx.buf[pos] = (uint8_t)(target & 0xff);
        cx.buf[pos + 1] = (uint8_t)((target >> 8) & 0xff);
        cx.buf[pos + 2] = (uint8_t)((target >> 16) & 0xff);
        cx.buf[pos + 3] = (uint8_t)((target >> 24) & 0xff);
    }

    size_t nl = strlen(m->name);
    if (nl > 255) die("nama metode terlalu panjang");
    fputc((int)nl, f);
    fwrite(m->name, 1, nl, f);
    uint8_t hdr[8];
    hdr[0] = (uint8_t)(m->nparams & 0xff); hdr[1] = (uint8_t)((m->nparams >> 8) & 0xff);
    hdr[2] = (uint8_t)(m->nlocals & 0xff); hdr[3] = (uint8_t)((m->nlocals >> 8) & 0xff);
    hdr[4] = (uint8_t)(cx.len & 0xff); hdr[5] = (uint8_t)((cx.len >> 8) & 0xff);
    hdr[6] = (uint8_t)((cx.len >> 16) & 0xff); hdr[7] = (uint8_t)((cx.len >> 24) & 0xff);
    fwrite(hdr, 1, 8, f);
    fwrite(cx.buf, 1, (size_t)cx.len, f);
}

void gen_vm(const char *outpath) {
    /* pre-pass: intern nama file sumber tiap metode SEBELUM header ditulis,
     * agar jumlah cpool di header sudah final (SRCFILE untuk stack trace) */
    for (Method *m = prog.methods; m; m = m->next)
        if (m->body && m->body->file)
            cp_intern(m->body->file);
    if (prog.ncp > 0xffff) die("terlalu banyak konstanta string");
    if (prog.nmethods > 0xffff) die("terlalu banyak metode");
    FILE *f = fopen(outpath, "wb");
    if (!f) die("tidak bisa menulis %s", outpath);
    fwrite("KOP1", 1, 4, f);
    uint8_t cnt[4];
    cnt[0] = (uint8_t)(prog.nmethods & 0xff); cnt[1] = (uint8_t)((prog.nmethods >> 8) & 0xff);
    cnt[2] = (uint8_t)(prog.ncp & 0xff); cnt[3] = (uint8_t)((prog.ncp >> 8) & 0xff);
    fwrite(cnt, 1, 4, f);
    for (Method *m = prog.methods; m; m = m->next) write_method(f, m);
    for (int i = 0; i < prog.ncp; i++) {
        size_t sl = strlen(prog.cpool[i]);
        long lens[4] = { (long)(sl & 0xff), (long)((sl >> 8) & 0xff), (long)((sl >> 16) & 0xff), (long)((sl >> 24) & 0xff) };
        for (int k = 0; k < 4; k++) fputc((int)lens[k], f);
        fwrite(prog.cpool[i], 1, sl, f);
    }
    fclose(f);
}
