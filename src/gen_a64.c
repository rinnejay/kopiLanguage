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

static int cur_ret_label;

/* push hasil ekspresi: 32-bit (w0) atau pointer closure 64-bit (x0) */
static void push_val(int is64) {
    if (is64) emit("\tstr\tx0, [sp, #-16]!");
    else emit("\tstr\tw0, [sp, #-16]!");
}

static void gen_stmt(Method *m, Node *s) {
    switch (s->kind) {
    case ST_VAR:
        gen_expr(m, s->a);
        if (s->dty == (int)T_FN) emit("\tstr\tx0, [x29, #-%ld]", (s->ival + 1) * 8);
        else emit("\tstr\tw0, [x29, #-%ld]", (s->ival + 1) * 8);
        break;
    case ST_ASSIGN:
        gen_expr(m, s->a);
        if (s->dty == (int)T_FN) emit("\tstr\tx0, [x29, #-%ld]", (s->ival + 1) * 8);
        else emit("\tstr\tw0, [x29, #-%ld]", (s->ival + 1) * 8);
        break;
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
        if (e->dty == (int)T_FN) {
            emit("\tldr\tx0, [x29, #-%ld]", (e->ival + 1) * 8);
            return 1;
        }
        emit("\tldr\tw0, [x29, #-%ld]", (e->ival + 1) * 8);
        return 0;
    case EX_UN:
        gen_expr(m, e->a);
        if (e->str[0] == '-') emit("\tneg\tw0, w0");
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
        return 0;
    }
    case EX_ARROW: {
        Method *hm = (Method *)e->cp;
        long ncap = hm->ncapt;
        emit("\tmov\tx0, #%ld", 8 * (3 + ncap));          /* ukuran blok */
        emit("\tadrp\tx9, :got:malloc");
        emit("\tldr\tx9, [x9, :got_lo12:malloc]");
        emit("\tblr\tx9");
        emit("\tadrp\tx1, %s__%s", hm->cls, hm->name);    /* kode */
        emit("\tadd\tx1, x1, :lo12:%s__%s", hm->cls, hm->name);
        emit("\tstr\tx1, [x0]");
        emit("\tmov\tw2, #%ld", ncap);
        emit("\tstr\tw2, [x0, #8]");
        emit("\tmov\tw2, #%ld", hm->nparams - 1);         /* nuser */
        emit("\tstr\tw2, [x0, #16]");
        for (int j = 0; j < ncap; j++) {
            emit("\tldr\tw2, [x29, #-%ld]", (hm->captslot[j] + 1) * 8);
            emit("\tstr\tw2, [x0, #%ld]", 24 + 8 * j);
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
    case EX_ENVLOAD:
        emit("\tldr\tx0, [x29, #-8]");   /* slot 0 = param $env */
        emit("\tldr\tw0, [x0, #%ld]", 24 + 8 * (long)e->ival);
        return 0;
    case EX_BIN: {
        char op = e->str[0];
        if (op == '&' || op == '|') {
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
                if ((Ty)p->ty == T_FN) emit("\tstr\tx%d, [x29, #-%ld]", i, (i + 1) * 8);
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
    emit("\tmov\tw0, wzr");
    emit("\tb\t%s__%s", entry->cls, entry->name);

    /* stub mati: jumlah argumen lambda tidak cocok (didapat lari b.ne .Klam) */
    emit(".Klam:");
    emit("\tadrp\tx0, :got:.Lmsg_lam");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_lam]");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    emit("\t.section .rodata");
    emit(".Lfmt_i: .asciz \"%s\"", "%d");
    emit(".Lfmt_in: .asciz \"%s\"", "%d\\n");
    emit(".Lfmt_s: .asciz \"%s\"", "%s");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    emit(".Lfmt_sn: .asciz \"%s\"", "%s\\n");
    for (int i = 0; i < prog.ncp; i++) {
        char esc[1200];
        asm_escape(prog.cpool[i], esc, sizeof esc);
        emit("cpool%d:", i);
        emit("\t.asciz \"%s\"", esc);
    }
    fclose(out);
}
