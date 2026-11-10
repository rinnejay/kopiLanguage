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

/* 32-bit int/bool vs 64-bit (pointer lambda/array) */
static int is64ty(Ty t) { return t == T_FN || t == T_ARI; }

/* hasil ekspresi: 0 = int/bool di %eax; 1 = pointer closure di %rax */

static void gen_stmt(Method *m, Node *s) {
    switch (s->kind) {
    case ST_VAR:
        gen_expr(m, s->a);
        if (is64ty((Ty)s->dty)) emit("\tmovq %%rax, -%ld(%%rbp)", (s->ival + 1) * 8);
        else emit("\tmovl %%eax, -%ld(%%rbp)", (s->ival + 1) * 8);
        break;
    case ST_ASSIGN:
        gen_expr(m, s->a);
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
        if (is64ty((Ty)e->dty)) {
            emit("\tmovq -%ld(%%rbp), %%rax", (e->ival + 1) * 8);
            return 1;
        }
        emit("\tmovl -%ld(%%rbp), %%eax", (e->ival + 1) * 8);
        break;
    case EX_UN:
        gen_expr(m, e->a);
        if (e->str[0] == '-') emit("\tnegl %%eax");
        else if (e->str[0] == '~') emit("\tnotl %%eax");
        else emit("\txorl $1, %%eax");
        break;
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
        emit("\tmovl %ld(%%rax), %%eax", 24 + 8 * (long)e->ival);
        break;
    case EX_BIN: {
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
                if (i >= 6) break;
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
