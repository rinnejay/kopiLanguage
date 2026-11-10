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
static int brk_lab[64], cont_lab[64], brk_sp;

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

static void gen_stmt(Node *s) {
    switch (s->kind) {
    case ST_VAR:
    case ST_ASSIGN:
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
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = ltop; brk_sp++;
        mark(ltop);
        gen_expr(s->a);
        jmp(OP_JMPF, lend);
        gen_stmts(s->b);
        jmp(OP_JMP, ltop);
        mark(lend);
        brk_sp--;
        brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_DO: {
        int ltop = newlab(), lcont = newlab(), lend = newlab();
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = lcont; brk_sp++;
        mark(ltop);
        gen_stmts(s->b);
        mark(lcont);
        gen_expr(s->a);
        jmp(OP_JMPT, ltop);
        mark(lend);
        brk_sp--;
        brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_FOR: {
        if (s->a) gen_stmt(s->a);       /* init (ST_VAR / ST_ASSIGN) */
        int ltop = newlab(), lend = newlab(), lcont = newlab();
        int pdb = brk_lab[brk_sp], pdc = cont_lab[brk_sp];
        brk_lab[brk_sp] = lend; cont_lab[brk_sp] = lcont; brk_sp++;
        mark(ltop);
        if (s->b) { gen_expr(s->b); jmp(OP_JMPF, lend); }
        gen_stmts(s->d);
        mark(lcont);                    /* target continue: sebelum langkah */
        if (s->c) gen_stmt(s->c);       /* langkah update */
        jmp(OP_JMP, ltop);
        mark(lend);
        brk_sp--;
        brk_lab[brk_sp] = pdb; cont_lab[brk_sp] = pdc;
        break;
    }
    case ST_BRK: jmp(OP_JMP, brk_lab[brk_sp - 1]); break;
    case ST_CONT: jmp(OP_JMP, cont_lab[brk_sp - 1]); break;
    case ST_RETURN:
        if (s->a) {
            gen_expr(s->a);
            emit_conv((Ty)s->a->dty, cur_meth->ret);
        } else {
            em8(OP_ICONST); em32(0);
        }
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
            } else if (pt == T_ARI || pt == T_STR) {
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

    case ST_IDXASG:
        gen_expr(s->c);          /* nilai - dipakai paling atas */
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

static void gen_bin_typed(Node *e) {
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

    gen_expr(e->a);
    emit_conv(lt, P);
    gen_expr(e->b);
    if (!isShift) emit_conv(rt, P);

    if (ty_is_float(P)) {
        int sub = (P == T_FLOAT) ? 0 : 1;
        if (isCmp) {
            if (op == '=') em8(OP_FEQ);
            else if (op == '!') em8(OP_FNE);
            else if (!strcmp(e->str, "<=")) em8(OP_FLE);
            else if (!strcmp(e->str, ">=")) em8(OP_FGE);
            else if (op == '<') em8(OP_FLT);
            else em8(OP_FGT);
            em8(sub);
        } else {
            if (op == '+') em8(OP_FADD);
            else if (op == '-') em8(OP_FSUB);
            else if (op == '*') em8(OP_FMUL);
            else if (op == '/') em8(OP_FDIV);
            else if (op == '%') em8(OP_FMOD);
            else die("gen_vm: operator float '%s'", e->str);
            em8(sub);
        }
        return;
    }
    int uns = ty_is_uint(P);
    if (isShift) {
        int mask = ty_width(P) - 1;
        if (mask <= 0) mask = 31;
        if (!strcmp(e->str, "<<")) em8(OP_SHL64);
        else if (!strcmp(e->str, ">>")) em8(OP_SHR64);
        else em8(OP_USHR64);
        em8(mask);
        emit_norm(P);
        return;
    }
    if (isCmp) {
        if (op == '=') em8(OP_EQ64);
        else if (op == '!') em8(OP_NE64);
        else if (!strcmp(e->str, "<=")) em8(uns ? OP_LEU64 : OP_LE64);
        else if (!strcmp(e->str, ">=")) em8(uns ? OP_GEU64 : OP_GE64);
        else if (op == '<') em8(uns ? OP_LTU64 : OP_LT64);
        else em8(uns ? OP_GTU64 : OP_GT64);
        return;
    }
    switch (op) {
    case '+': em8(OP_ADD64); break;
    case '-': em8(OP_SUB64); break;
    case '*': em8(OP_MUL64); break;
    case '/': em8(uns ? OP_UDIV64 : OP_DIV64); break;
    case '%': em8(uns ? OP_UMOD64 : OP_MOD64); break;
    case '&': em8(OP_AND64); break;
    case '|': em8(OP_OR64); break;
    case '^': em8(OP_XOR64); break;
    default: die("gen_vm: operator '%s'", e->str);
    }
    emit_norm(P);
}

static void gen_expr(Node *e) {
    switch (e->kind) {
    case EX_INT: {
        Ty t = (Ty)e->dty;
        if (t == T_INT) {
            em8(OP_ICONST); em32(e->ival);
        } else {
            unsigned long long bits;
            if (t == T_FLOAT) {
                union { float f; uint32_t u; } cv;
                cv.f = (float)e->fval;
                bits = cv.u;
            } else if (t == T_DOUBLE) {
                union { double d; uint64_t u; } cv;
                cv.d = e->fval;
                bits = cv.u;
            } else {
                bits = (unsigned long long)e->ival;
            }
            em8(OP_ICONST64);
            for (int k = 0; k < 8; k++) em8((int)((bits >> (8 * k)) & 0xff));
        }
        break;
    }
    case EX_BOOL: em8(OP_ICONST); em32(e->ival); break;
    case EX_STRLIT:
        em8(OP_NET); em16(5); em16((int)e->cp);
        break;
    case EX_CAST:
        gen_expr(e->a);
        emit_conv((Ty)e->a->dty, (Ty)e->dty);
        break;
    case EX_VAR: em8(OP_ILOAD); em16((int)e->ival); break;
    case EX_UN: {
        Ty t = (Ty)e->dty;
        gen_expr(e->a);
        if (e->str[0] == '!') { em8(OP_NOT); break; }
        if (ty_is_float(t)) {
            em8(OP_FNEG);
            em8(t == T_FLOAT ? 0 : 1);
            break;
        }
        if (t == T_INT) {
            em8(e->str[0] == '-' ? OP_NEG : OP_BNOT);
            break;
        }
        em8(e->str[0] == '-' ? OP_NEG64 : OP_BNOT64);
        emit_norm(t);
        break;
    }
    case EX_CALL: {
        int i = 0;
        Param *pp = e->meth ? e->meth->params : 0;
        for (Node *a = e->a; a; a = a->next, pp = pp ? pp->next : 0) {
            gen_expr(a);
            if (pp) emit_conv((Ty)a->dty, (Ty)pp->ty);
            i++;
        }
        em8(OP_CALL); em16((int)e->ival); em16(i);
        break;
    }
    case EX_ARROW: {
        Method *hm = (Method *)e->cp;
        em8(OP_NEWCLO);
        em16(hm->index);
        em16(hm->ncapt);
        for (int j = 0; j < hm->ncapt; j++)
            em16((int)hm->captslot[j]);
        break;
    }
    case EX_FCALL: {
        int i = 0;
        for (Node *a = e->a; a; a = a->next) { gen_expr(a); i++; }
        gen_expr(e->b);
        em8(OP_CALLIND); em16(i);
        break;
    }
    case EX_NET:
        if (e->ival != 5 && e->ival != 6)     /* bytes/openr: literal dicerna compile-time */
            for (Node *aa = e->a; aa; aa = aa->next) gen_expr(aa);
        em8(OP_NET);
        em16((int)e->ival);
        if (e->ival == 5 || e->ival == 6) em16((int)e->cp);   /* bytes/openr: cpool */
        break;
    case EX_ENVLOAD:
        em8(OP_ENVLOAD);
        em16((int)e->ival);
        break;
    case EX_NEWARR:
        gen_expr(e->a);
        em8(OP_NEWARR);
        break;
    case EX_IDX:
        gen_expr(e->a);          /* array pointer */
        gen_expr(e->b);          /* indeks */
        em8(OP_IDXR);
        break;
    case EX_LEN:
        gen_expr(e->a);
        em8(OP_ALEN);
        break;
    case EX_TERN: {
        int lelse = newlab(), lend = newlab();
        gen_expr(e->a);
        jmp(OP_JMPF, lelse);
        gen_expr(e->b);
        jmp(OP_JMP, lend);
        mark(lelse);
        gen_expr(e->c);
        mark(lend);
        break;
    }
    case EX_BIN: {
        Ty bl = (Ty)e->a->dty, br = (Ty)e->b->dty, bres = (Ty)e->dty;
        if (!((bl == T_INT || bl == T_BOOL) && (br == T_INT || br == T_BOOL) &&
              (bres == T_INT || bres == T_BOOL))) {
            gen_bin_typed(e);
            break;
        }
        char op = e->str[0];
        if ((op == '&' || op == '|') && e->str[1] != 0) {
            /* short-circuit: hasil selalu 0/1 */
            int lres = newlab(), lend = newlab();
            int vfall = op == '&' ? 1 : 0;   /* nilai jika keduanya benar/salah */
            int vres  = op == '&' ? 0 : 1;   /* nilai saat lompatan dipakai */
            gen_expr(e->a);
            jmp(op == '&' ? OP_JMPF : OP_JMPT, lres);
            gen_expr(e->b);
            jmp(op == '&' ? OP_JMPF : OP_JMPT, lres);
            em8(OP_ICONST); em32(vfall);
            jmp(OP_JMP, lend);
            mark(lres);
            em8(OP_ICONST); em32(vres);
            mark(lend);
            break;
        }
        gen_expr(e->a);
        gen_expr(e->b);
        /* operator 2-karakter dulu, supaya tidak tabrak case '<' / '>' */
        if (!strcmp(e->str, "<=")) em8(OP_LE);
        else if (!strcmp(e->str, ">=")) em8(OP_GE);
        else if (!strcmp(e->str, "<<")) em8(OP_SHL);
        else if (!strcmp(e->str, ">>")) em8(OP_SHR);
        else if (!strcmp(e->str, ">>>")) em8(OP_USHR);
        else switch (op) {
        case '+': em8(OP_ADD); break;
        case '-': em8(OP_SUB); break;
        case '*': em8(OP_MUL); break;
        case '/': em8(OP_DIV); break;
        case '%': em8(OP_MOD); break;
        case '<': em8(OP_LT); break;
        case '>': em8(OP_GT); break;
        case '=': em8(OP_EQ); break;
        case '!': em8(OP_NE); break;
        case '&': em8(OP_BAND); break;      /* bitwise (len 1) */
        case '|': em8(OP_BOR); break;
        case '^': em8(OP_BXOR); break;
        default: die("gen_vm: operator '%s'", e->str);
        }
        break;
    }
    default:
        die("gen_vm: ekspresi kind=%d", e->kind);
    }
}

/* tulis satu metode: isi buffer bytecode lalu flush sebagai entri file */
static void write_method(FILE *f, Method *m) {
    cur_meth = m;
    cx.len = 0; cx.nfixes = 0; cx.nlab = 0;
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
