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

static int cur_ret_label;

/* push hasil ekspresi: 32-bit (w0) atau pointer closure 64-bit (x0) */
/* 32-bit int/bool vs 64-bit (pointer lambda/array) */
static int is64ty(Ty t) { return t == T_FN || t == T_ARI; }

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

static void push_val(int is64) {
    if (is64) emit("\tstr\tx0, [sp, #-16]!");
    else emit("\tstr\tw0, [sp, #-16]!");
}

static void gen_stmt(Method *m, Node *s) {
    switch (s->kind) {
    case ST_VAR:
        gen_expr(m, s->a);
        loc_store((s->ival + 1) * 8, is64ty((Ty)s->dty) ? "x0" : "w0");
        break;
    case ST_ASSIGN:
        gen_expr(m, s->a);
        loc_store((s->ival + 1) * 8, is64ty((Ty)s->dty) ? "x0" : "w0");
        break;
    case ST_IDXASG: {
        gen_expr(m, s->c);               /* nilai */
        push_val(0);
        gen_expr(m, s->a);               /* x0 = array */
        push_val(1);
        gen_expr(m, s->b);               /* w0 = idx */
        emit("\tldr\tx1, [sp], #16");   /* array */
        emit("\tldr\tw2, [sp], #16");  /* nilai */
        emit("\tldr\tw3, [x1]");
        emit("\tcmp\tw0, w3");
        emit("\tb.hs\t.Karr");
        emit("\tsxtw\tx9, w0");
        emit("\tsxtw\tx2, w2");
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
    case ST_FOR: {
        if (s->a) gen_stmt(m, s->a);
        int ltop = nlab++, lend = nlab++, lcont = nlab++;
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
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
    case ST_BRK: emit("\tb\t.L%d", brk_lab[brk_sp - 1]); break;
    case ST_CONT: emit("\tb\t.L%d", cont_lab[brk_sp - 1]); break;
    case ST_RETURN: {
        if (s->a) gen_expr(m, s->a);
        else emit("\tmov\tw0, wzr");
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
        } else {
            gen_expr(m, s->a);
            const char *fmt = s->ival ? ".Lfmt_in" : ".Lfmt_i";
            emit("\tsxtw\tx1, w0");
            emit("\tadrp\tx0, :got:%s", fmt);
            emit("\tldr\tx0, [x0, :got_lo12:%s]", fmt);
            emit("\tbl\tprintf");
        }
        break;
    case ST_EXPR:
        if (s->a) gen_expr(m, s->a);
        break;
    case ST_BLOCK:
        gen_stmts(m, s->d);
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

/* hasil: 0 = int/bool di w0 (32-bit); 1 = POINTER closure di x0 (64-bit) */
static int gen_expr(Method *m, Node *e) {
    switch (e->kind) {
    case EX_INT: {
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
    case EX_BOOL:
        if (e->ival) emit("\tmov\tw0, #1");
        else emit("\tmov\tw0, wzr");
        return 0;
    case EX_VAR:
        if (is64ty((Ty)e->dty)) {
            loc_load((e->ival + 1) * 8, "x0");
            return 1;
        }
        loc_load((e->ival + 1) * 8, "w0");
        return 0;
    case EX_UN:
        gen_expr(m, e->a);
        if (e->str[0] == '-') emit("\tneg\tw0, w0");
        else if (e->str[0] == '~') emit("\tmvn\tw0, w0");
        else emit("\teor\tw0, w0, #1");
        return 0;
    case EX_CALL: {
        int n = 0;
        int w64[64];
        for (Node *a = e->a; a; a = a->next) {
            if (n >= 8) die("baris %d: maksimal 8 argumen untuk target native", e->line);
            w64[n] = gen_expr(m, a);
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
        return 1;
    }
    case EX_IDX: {                       /* hasil: w0 = elemen (32-bit) */
        gen_expr(m, e->a);               /* x0 = array pointer */
        push_val(1);
        gen_expr(m, e->b);               /* w0 = idx */
        emit("\tldr\tx1, [sp], #16");
        emit("\tldr\tw2, [x1]");        /* len */
        emit("\tcmp\tw0, w2");
        emit("\tb.hs\t.Karr");          /* (>0 unsigned): negatif ikut tertangkap */
        emit("\tsxtw\tx9, w0");
        emit("\tadd\tx1, x1, #8");
        emit("\tldr\tx0, [x1, x9, lsl #3]");
        emit("\tmov\tw0, w0");          /* nilai int tetap 32-bit */
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
        if (e->ival == 5) {              /* Net.bytes("literal") → arena       */
            int c = (int)e->cp;
            emit("\tadrp\tx9, :got:cpool%d", c);
            emit("\tldr\tx9, [x9, :got_lo12:cpool%d]", c);
            emit("\tmov\tw2, #0");                       /* strlen */
            emit(".LNB%da:", ln);
            emit("\tldrb\tw1, [x9, x2]");
            emit("\tadd\tw2, w2, #1");
            emit("\tcbnz\tw1, .LNB%da", ln);
            emit("\tsub\tw2, w2, #1");                   /* len = w2 */
            emit("\tsxtw\tx10, w2");
            emit("\tlsl\tx0, x10, #3");
            emit("\tadd\tx0, x0, #8");
            emit("\tbl\tkopalloc");
            emit("\tstr\tx10, [x0]");                    /* len */
            emit("\tmov\tx11, x0");                      /* simpan hasil */
            emit("\tmov\tx12, #0");                      /* i */
            emit(".LNB%db:", ln);
            emit("\tcmp\tx12, x10");
            emit("\tb.hs\t.LNB%dc", ln);
            emit("\tldrb\tw1, [x9, x12]");
            emit("\tlsl\tx13, x12, #3");
            emit("\tadd\tx13, x13, #8");
            emit("\tstr\tx1, [x11, x13]");
            emit("\tadd\tx12, x12, #1");
            emit("\tb\t.LNB%db", ln);
            emit(".LNB%dc:", ln);
            emit("\tmov\tx0, x11");
            return 1;                       /* hasil pointer array 64-bit */
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
        emit("\tldr\tw0, [x0, #%ld]", 24 + 8 * (long)e->ival);
        return 0;
    case EX_BIN: {
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
    /* init arena: blok 32 MB sekali via malloc (X25519 ladder mengalokasi
     * ~2,5 MB per panggilan; 6 vektor RFC butuh ~15 MB) */
    emit("\tmovz\tx0, #0x200, lsl #16");    /* 1<<25 bytes */
    emit("\tbl\tmalloc");
    emit("\tadrp\tx1, kop_arena_base");
    emit("\tstr\tx0, [x1, :lo12:kop_arena_base]");
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

    emit(".Koom:");
    emit("\tadrp\tx0, :got:.Lmsg_oom");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom]");
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
    emit("\tmovz\tx3, #0x200, lsl #16");
    emit("\tcmp\tx2, x3");
    emit("\tb.ge\t.Koom");
    emit("\tstr\tx2, [x1]");
    emit("\tsub\tx2, x2, x0");
    emit("\tadrp\tx3, kop_arena_base");
    emit("\tadd\tx3, x3, :lo12:kop_arena_base");
    emit("\tldr\tx3, [x3]");
    emit("\tadd\tx0, x3, x2");
    emit("\tret");

    emit("\t.section .data");
    emit("\t.align 3");
    emit("kop_arena_base: .quad 0");
    emit("kop_arena_used: .quad 0");
    emit(".comm kop_iobuf, 256, 32");      /* penyangga syscall rekursi builtin */

    emit("\t.section .rodata");
    emit(".Lfmt_i: .asciz \"%s\"", "%d");
    emit(".Lfmt_in: .asciz \"%s\"", "%d\\n");
    emit(".Lfmt_s: .asciz \"%s\"", "%s");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    emit(".Lmsg_arr: .asciz \"%s\"", "kopi: array tidak valid (indeks/ukuran salah)\\n");
    emit(".Lmsg_oom: .asciz \"%s\"", "kopi: arena memori habis\\n");
    emit(".Lfmt_sn: .asciz \"%s\"", "%s\\n");
    for (int i = 0; i < prog.ncp; i++) {
        char esc[1200];
        asm_escape(prog.cpool[i], esc, sizeof esc);
        emit("cpool%d:", i);
        emit("\t.asciz \"%s\"", esc);
    }
    fclose(out);
}
