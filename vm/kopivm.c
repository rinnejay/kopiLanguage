/* kopivm — mesin virtual untuk bytecode KOP1 yang dihasilkan kopicc -t vm. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include "ast.h"

enum {
    OP_HALT = 0, OP_ICONST, OP_ILOAD, OP_ISTORE, OP_POP,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_NOT,
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
    OP_JMP, OP_JMPF, OP_JMPT, OP_CALL, OP_RET,
    OP_PRINTILN, OP_PRINTI, OP_PRINTSLN, OP_PRINTS,
    OP_NEWCLO, OP_CALLIND, OP_ENVLOAD, OP_IDXR, OP_IDXW, OP_ALEN,
    OP_NEWARR,                      /* lambda/closure + array arena */
    OP_NET,                         /* builtin Net.* (ubit syscall host) */
    OP_BAND, OP_BOR, OP_BXOR, OP_SHL, OP_SHR, OP_USHR, OP_BNOT,   /* v1.5 bitwise */
    /* v1.6 tipe numerik baru */
    OP_ICONST64,
    OP_ADD64, OP_SUB64, OP_MUL64, OP_DIV64, OP_MOD64,
    OP_UDIV64, OP_UMOD64, OP_AND64, OP_OR64, OP_XOR64,
    OP_SHL64, OP_SHR64, OP_USHR64, OP_NEG64, OP_BNOT64,
    OP_LT64, OP_GT64, OP_LE64, OP_GE64,
    OP_LTU64, OP_GTU64, OP_LEU64, OP_GEU64,
    OP_EQ64, OP_NE64,
    OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV, OP_FMOD, OP_FNEG,
    OP_FLT, OP_FGT, OP_FLE, OP_FGE, OP_FEQ, OP_FNE,
    OP_CONV,        /* 2 byte: from,to */
    OP_NORM,        /* 1 byte: tipe */
    OP_PRINTNUM,    /* 2 byte: tipe, newline */
    OP_PRINTBUF,    /* 1 byte: newline; tulis array byte ke fd 1 */
    OP_LINE,        /* 2 byte: nomor baris sumber — stack trace */
    OP_SRCFILE,     /* 2 byte: index cpool nama file sumber */
    OP_IDXB,        /* v1.18: byte ke-i dari String packed */
    OP_PRINTSTR,    /* 1 byte: newline; tulis String packed ke fd 1 */
    OP_MARK,        /* v1.20: dorong penanda arena ke stack */
    OP_MRESET       /* v1.20: tarik penanda & bebaskan arena ke situ */
};

#define MAXM 4096
#define MAXSTACK (1 << 20)
#define MAXFRAMES 4096
#define MAXLOC 256

typedef struct {
    char name[256];
    uint16_t nargs, nlocals;
    uint32_t clen;
    const uint8_t *code;
} Meth;

static uint8_t *img;
static size_t imglen;
static Meth meths[MAXM];
static int nmeths;
static char *cpool[65536];
static int ncp;

static void dievm(const char *m);
static void dievm2(const char *m, long v);

/* heap untuk closure & array (bump arena; GC belum ada) */
/* heap untuk closure & array (bump arena; GC belum ada).
 * 1 << 22 long = 32 MB: cukup untuk beberapa X25519 penuh (ladder meng-
 * alokasi ~0,3 MB long per panggilan tanpa GC) */
#define HEAPLEN (1 << 22)
static long *heap;
static long heap_used;

static void dievm2(const char *m, long v);

static uint8_t rd8(size_t *p) { return img[(*p)++]; }
static uint16_t rd16(size_t *p) { uint16_t v = rd8(p); v |= (uint16_t)rd8(p) << 8; return v; }
static uint32_t rd32(size_t *p) {
    uint32_t v = rd16(p);
    v |= (uint32_t)rd16(p) << 16;
    return v;
}

static void load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) dievm("tidak bisa membuka file bytecode");
    fseek(f, 0, SEEK_END);
    imglen = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    img = malloc(imglen ? imglen : 1);
    if (fread(img, 1, imglen, f) != imglen) dievm("gagal membaca bytecode");
    fclose(f);

    if (imglen < 8 || memcmp(img, "KOP1", 4)) dievm("magic tidak cocok — bukan file KOP1");
    size_t p = 4;
    nmeths = rd16(&p);
    ncp = rd16(&p);
    if (nmeths > MAXM) dievm("terlalu banyak metode");
    for (int i = 0; i < nmeths; i++) {
        Meth *m = &meths[i];
        uint8_t nl = rd8(&p);
        if ((int)nl >= (int)sizeof m->name || p + nl >= imglen) dievm("berkas korup (nama)");
        memcpy(m->name, img + p, nl);
        m->name[nl] = 0;
        p += nl;
        m->nargs = rd16(&p);
        m->nlocals = rd16(&p);
        m->clen = rd32(&p);
        if (p + m->clen > imglen) dievm("berkas korup (kode)");
        m->code = img + p;
        p += m->clen;
    }
    for (int i = 0; i < ncp; i++) {
        uint32_t sl = rd32(&p);
        if (p + sl > imglen) dievm("berkas korup (string)");
        cpool[i] = malloc((size_t)sl + 1);
        memcpy(cpool[i], img + p, sl);
        cpool[i][sl] = 0;
        p += sl;
    }
}

typedef struct { int meth; long pc; long retbase; int line; const char *file; } Frame;

static int64_t stackv[MAXSTACK];
static long sp;
static Frame frames[MAXFRAMES];
static int nframes;

/* error runtime + stack trace: setiap frame menyimpan baris/file sumber
 * (diisi opcode OP_LINE / OP_SRCFILE yang dipancarkan compiler) */
static void dievm(const char *m) {
    fprintf(stderr, "kopivm: error: %s\n", m);
    if (nframes > 0) {
        fprintf(stderr, "  stack trace (terdalam dulu):\n");
        for (int i = nframes - 1; i >= 0; i--) {
            Frame *fr = &frames[i];
            fprintf(stderr, "    at %s (%s:%d)\n", meths[fr->meth].name,
                    fr->file ? fr->file : "?", fr->line);
        }
    }
    exit(1);
}

static void dievm2(const char *m, long v) {
    char buf[320];
    snprintf(buf, sizeof buf, "%s (%ld)", m, v);
    dievm(buf);
}
static int64_t localsv[MAXFRAMES][MAXLOC];

static uint64_t rd64code(Meth *m, long *pc) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)m->code[(*pc)++] << (8 * i);
    return v;
}

static int64_t norm_val(int64_t v, int t) {
    switch (t) {
    case T_INT:    return (int32_t)v;
    case T_LONG:   return v;
    case T_UINT8:  return (uint8_t)v;
    case T_UINT16: return (uint16_t)v;
    case T_BYTE:   return (int8_t)v;
    case T_SHORT:  return (int16_t)v;
    case T_UINT32: return (uint32_t)v;
    case T_UINT64: return v;
    default:       return v;
    }
}

static double bits_d(int64_t b) { double d; memcpy(&d, &b, 8); return d; }
static int64_t d_bits(double d) { int64_t b; memcpy(&b, &d, 8); return b; }
static float bits_f(int64_t b) { uint32_t u = (uint32_t)b; float f; memcpy(&f, &u, 4); return f; }
static int64_t f_bits(float f) { uint32_t u; memcpy(&u, &f, 4); return (int64_t)u; }

static double dtrunc_safe(double x) {
    if (x >= 9223372036854775807.0) return x;
    if (x <= -9223372036854775808.0) return x;
    return (double)(int64_t)x;
}

static int64_t conv_val(int64_t v, int from, int to) {
    if (from == to) return v;
    if (ty_is_float(from)) {
        double d = (from == T_FLOAT) ? (double)bits_f(v) : bits_d(v);
        if (ty_is_float(to))
            return (to == T_FLOAT) ? f_bits((float)d) : d_bits(d);
        double t = dtrunc_safe(d);
        int64_t r;
        if (t >= 9223372036854775807.0) r = INT64_MAX;
        else if (t <= -9223372036854775808.0) r = INT64_MIN;
        else r = (int64_t)t;
        return norm_val(r, to);
    }
    if (ty_is_float(to)) {
        if (ty_is_uint(from))
            return (to == T_FLOAT) ? f_bits((float)(uint64_t)v) : d_bits((double)(uint64_t)v);
        return (to == T_FLOAT) ? f_bits((float)v) : d_bits((double)v);
    }
    return norm_val(v, to);
}

static char *pack_cpath(long *a, size_t maxlen) {
    size_t len = (size_t)a[0];
    if (len > maxlen) len = maxlen;
    char *b = (char *)malloc(len + 1);
    if (!b) dievm("path: memori habis");
    for (size_t i = 0; i < len; i++) b[i] = (char)(a[1 + i] & 0xff);
    b[len] = 0;
    return b;
}

static int vargc;
static char **vargv;

static int64_t mk_str_arr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    /* v1.18: String packed — 8 byte per slot 64-bit, little-endian */
    long slots = ((long)n + 7) >> 3;
    if (heap_used + slots + 1 > HEAPLEN) dievm("arena penuh (string)");
    long *blk = heap + heap_used;
    heap_used += slots + 1;
    blk[0] = (long)n;
    for (long k = 0; k < slots; k++) blk[1 + k] = 0;
    for (size_t i = 0; i < n; i++)
        blk[1 + (i >> 3)] |= (long)(unsigned char)s[i] << ((i & 7) * 8);
    return (int64_t)blk;
}

/* baca byte ke-i dari blok packed (v1.18) */
static inline long getb(const long *blk, long i) {
    return (blk[1 + (i >> 3)] >> ((i & 7) * 8)) & 0xff;
}

/* tulis byte ke-i blok packed (slot sudah di-zero) */
static inline void setb(long *blk, long i, long v) {
    blk[1 + (i >> 3)] |= (v & 0xff) << ((i & 7) * 8);
}

static void run(void) {
    int mid = -1;
    for (int i = 0; i < nmeths; i++)
        if (!strcmp(meths[i].name, "main") && meths[i].nargs <= 1) { mid = i; break; }
    if (mid < 0) dievm("tidak ada metode main");

    heap = malloc(sizeof(long) * (size_t)HEAPLEN);
    if (!heap) dievm("heap closure gagal dialokasi");
    heap_used = 0;

    nframes = 1;
    frames[0].meth = mid;
    frames[0].pc = 0;
    frames[0].retbase = 0;
    frames[0].line = 0;
    frames[0].file = 0;
    memset(localsv[0], 0, sizeof localsv[0]);

    for (;;) {
        if (nframes <= 0) { if (sp > 0) exit((int)(stackv[0] & 0xff)); exit(0); }
        Frame *f = &frames[nframes - 1];
        Meth *m = &meths[f->meth];
        if (f->pc >= m->clen) dievm("pc luar batas — bytecode korup");
        uint8_t op = m->code[f->pc++];
        int64_t a, b;

        switch (op) {
        case OP_LINE: {
            int ln = m->code[f->pc] | (m->code[f->pc + 1] << 8);
            f->pc += 2;
            if (ln) f->line = ln;
            break;
        }
        case OP_SRCFILE: {
            int ci = m->code[f->pc] | (m->code[f->pc + 1] << 8);
            f->pc += 2;
            if (ci >= 0 && ci < ncp) f->file = cpool[ci];
            break;
        }
        case OP_ICONST: {
            int32_t v = (int32_t)m->code[f->pc] | ((int32_t)m->code[f->pc + 1] << 8) |
                        ((int32_t)m->code[f->pc + 2] << 16) | ((int32_t)m->code[f->pc + 3] << 24);
            f->pc += 4;
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = v;
            break;
        }
        case OP_ILOAD: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            if ((int)s >= MAXLOC) dievm("slot lokal luar batas");
            stackv[sp++] = localsv[nframes - 1][s];
            break;
        }
        case OP_ISTORE: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            if ((int)s >= MAXLOC) dievm("slot lokal luar batas");
            localsv[nframes - 1][s] = stackv[--sp];
            break;
        }
        case OP_POP: sp--; break;
        /* semantik int 32-bit (wrap, seperti Java/backend native) */
        case OP_ADD: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a + b); break;
        case OP_SUB: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a - b); break;
        case OP_MUL: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a * b); break;
        case OP_DIV:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("pembagian dengan nol");
            stackv[sp++] = a / b;
            break;
        case OP_MOD:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("modulus dengan nol");
            stackv[sp++] = a % b;
            break;
        case OP_NEG: a = stackv[--sp]; stackv[sp++] = -a; break;
        case OP_NOT: a = stackv[--sp]; stackv[sp++] = a ? 0 : 1; break;
        case OP_LT: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a < b; break;
        case OP_GT: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a > b; break;
        case OP_LE: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a <= b; break;
        case OP_GE: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a >= b; break;
        case OP_EQ: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a == b; break;
        case OP_NE: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a != b; break;
        case OP_BAND: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a & b); break;
        case OP_BOR:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a | b); break;
        case OP_BXOR: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a ^ b); break;
        case OP_SHL:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a << (b & 31)); break;
        case OP_SHR:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a >> (b & 31)); break;
        case OP_USHR: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)((uint32_t)a >> (b & 31)); break;
        case OP_BNOT: a = stackv[--sp]; stackv[sp++] = ~a; break;
        case OP_JMP:
            f->pc = (int32_t)((uint32_t)m->code[f->pc] | ((uint32_t)m->code[f->pc + 1] << 8) |
                              ((uint32_t)m->code[f->pc + 2] << 16) | ((uint32_t)m->code[f->pc + 3] << 24));
            break;
        case OP_JMPF:
        case OP_JMPT: {
            uint32_t t = (uint32_t)m->code[f->pc] | ((uint32_t)m->code[f->pc + 1] << 8) |
                         ((uint32_t)m->code[f->pc + 2] << 16) | ((uint32_t)m->code[f->pc + 3] << 24);
            f->pc += 4;
            a = stackv[--sp];
            if ((op == OP_JMPF && !a) || (op == OP_JMPT && a)) {
                if (t > m->clen) dievm("target lompatan luar batas");
                f->pc = (long)t;
            }
            break;
        }
        case OP_CALL: {
            uint16_t cm = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            uint16_t cn = (uint16_t)m->code[f->pc + 2] | ((uint16_t)m->code[f->pc + 3] << 8);
            f->pc += 4;
            if (cm >= nmeths) dievm("metode luar batas");
            Meth *mm = &meths[cm];
            if (cn != mm->nargs) dievm("jumlah argumen argumen tidak cocok");
            if (nframes >= MAXFRAMES) dievm("call stack meluap");
            if (mm->nlocals >= MAXLOC) dievm("terlalu banyak local");
            long base = sp - cn;
            if (base < 0) dievm("stack korup");
            memset(localsv[nframes], 0, sizeof localsv[nframes]);
            for (int i = 0; i < cn; i++) localsv[nframes][i] = stackv[base + i];
            sp = base;
            frames[nframes].meth = cm;
            frames[nframes].pc = 0;
            frames[nframes].retbase = base;
            frames[nframes].line = 0;
            frames[nframes].file = 0;
            nframes++;
            break;
        }
        case OP_RET: {
            int64_t v = stackv[--sp];
            long rb = f->retbase;
            nframes--;
            sp = rb;
            stackv[sp++] = v;
            break;
        }
        case OP_PRINTILN: { int64_t v = stackv[--sp]; printf("%" PRId64 "\n", v); break; }
        case OP_PRINTI: { int64_t v = stackv[--sp]; printf("%" PRId64, v); break; }
        case OP_PRINTSLN: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            printf("%s\n", cpool[s]);
            break;
        }
        case OP_PRINTS: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            printf("%s", cpool[s]);
            break;
        }
        case OP_NEWCLO: {
            uint16_t cm = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            uint16_t cn = (uint16_t)m->code[f->pc + 2] | ((uint16_t)m->code[f->pc + 3] << 8);
            f->pc += 4;
            if (cm >= (uint16_t)nmeths) dievm("closure: metode luar batas");
            Meth *mm = &meths[cm];
            if (mm->nargs == 0) dievm("closure: metode tanpa env");
            long nv = 3 + cn;
            if (heap_used + nv > HEAPLEN) dievm("heap penuh — GC belum ada (lambda dibangun dalam loop?)");
            if (mm->nlocals >= MAXLOC) dievm("terlalu banyak local (lambda)");
            long *blk = heap + heap_used;
            heap_used += nv;
            blk[0] = cm;                       /* identitas kode = index metode */
            blk[1] = cn;                       /* ncap */
            blk[2] = mm->nargs - 1;            /* nuser */
            for (int k = 0; k < cn; k++) {
                uint16_t s = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                if ((int)s >= MAXLOC) dievm("slot capture luar batas");
                blk[3 + k] = localsv[nframes - 1][s];
            }
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = (int64_t)blk;
            break;
        }
        case OP_CALLIND: {
            uint16_t cn = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            if (sp < 1) dievm("stack korup (CALLIND)");
            long clo = stackv[--sp];
            long *blk = (long *)clo;
            if (clo == 0 || blk < heap || blk >= heap + HEAPLEN)
                dievm("nilai lambda tidak valid (env di luar heap)");
            long cm = blk[0];
            if (cm >= nmeths) dievm("closure korup (index metode)");
            Meth *mm = &meths[cm];
            if ((long)cn != blk[2] || (long)cn != mm->nargs - 1)
                dievm("jumlah argumen lambda tidak cocok");
            if (nframes >= MAXFRAMES) dievm("call stack meluap");
            if (mm->nlocals >= MAXLOC) dievm("terlalu banyak local (lambda)");
            long base = sp - cn;
            if (base < 0) dievm("stack korup (CALLIND args)");
            memset(localsv[nframes], 0, sizeof localsv[nframes]);
            localsv[nframes][0] = clo;
            for (int i = 0; i < (int)cn; i++)
                localsv[nframes][i + 1] = stackv[base + i];
            sp = base;
            frames[nframes].meth = (int)cm;
            frames[nframes].pc = 0;
            frames[nframes].retbase = base;
            frames[nframes].line = 0;
            frames[nframes].file = 0;
            nframes++;
            break;
        }
        case OP_ENVLOAD: {
            uint16_t cj = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            long clo = localsv[nframes - 1][0];
            long *blk = (long *)clo;
            if (clo == 0 || blk < heap || blk >= heap + HEAPLEN)
                dievm("env lambda tidak valid");
            long ncap = blk[1];
            if ((long)cj >= ncap) dievm("capture korup");
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = blk[3 + cj];
            break;
        }
        case OP_NEWARR: {
            int64_t n = stackv[--sp];
            if (n < 0) dievm2("ukuran array negatif", (long)n);
            if (heap_used + n + 1 > HEAPLEN)
                dievm("arena penuh — GC belum ada; reset arena per request"); 
            long *blk = heap + heap_used;
            heap_used += n + 1;
            blk[0] = (long)n;
            if (n > 0) memset(blk + 1, 0, (size_t)n * sizeof(long));
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = (int64_t)blk;
            break;
        }
        case OP_IDXR: {
            int64_t idx = stackv[--sp];
            long *blk = (long *)stackv[--sp];
            if ((long *)0 == blk || blk < heap || blk >= heap + HEAPLEN)
                dievm("array tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks array di luar batas", (long)idx);
            stackv[sp++] = blk[1 + idx];
            break;
        }
        case OP_IDXW: {
            int64_t idx = stackv[--sp];
            long *blk = (long *)stackv[--sp];
            int64_t val = stackv[--sp];
            if ((long *)0 == blk || blk < heap || blk >= heap + HEAPLEN)
                dievm("array tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks array di luar batas", (long)idx);
            blk[1 + idx] = val;
            break;
        }
        case OP_IDXB: {                       /* v1.18: byte ke-i String packed */
            int64_t idx = stackv[--sp];
            long *blk = (long *)stackv[--sp];
            if ((long *)0 == blk || blk < heap || blk >= heap + HEAPLEN)
                dievm("string tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks string di luar batas", (long)idx);
            stackv[sp++] = getb(blk, idx);
            break;
        }
        case OP_ALEN: {
            long *blk = (long *)stackv[--sp];
            stackv[sp++] = blk[0];
            break;
        }
        case OP_NET: {
            uint16_t bi = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            /* array byte-as-int: [0]=len, [1+i]=elemen (semantik byte 0..255) */
            if (bi == 0) {                        /* socket(domain, type) */
                int64_t tipe = stackv[--sp], dom = stackv[--sp];
                int fd = (int)socket((int)dom, (int)tipe, 0);
                stackv[sp++] = fd;
            } else if (bi == 1) {                 /* connect(fd, ipBE, port) */
                int64_t port = stackv[--sp], ipbe = stackv[--sp], fdi = stackv[--sp];
                struct sockaddr_in sa;
                memset(&sa, 0, sizeof sa);
                sa.sin_family = AF_INET;
                /* builtin menerima nilai polos: ipBE = a*2^24+b*2^16+c*2^8+d,
                 * port = nomor host (mis. 9901); htonl/htons mengurusi urutan
                 * byte jaringan meski endian host apa pun */
                sa.sin_port = htons((uint16_t)((long)port & 0xffff));
                sa.sin_addr.s_addr = htonl((uint32_t)((long)ipbe & 0xffffffff));
                int r = (int)connect((int)fdi, (struct sockaddr *)&sa, sizeof sa);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 2) {                 /* rdb(fd) → byte 0..255 atau -1 */
                int64_t fdi = stackv[--sp];
                unsigned char b;
                long nrd = read((int)fdi, &b, 1);
                if (nrd < 0) dievm("Net.rdb: read gagal");
                stackv[sp++] = (nrd == 0) ? -1 : (long)b;
            } else if (bi == 3) {                 /* wrb(fd, byte) → 0 atau -errno */
                int64_t v = stackv[--sp], fdi = stackv[--sp];
                unsigned char b = (unsigned char)(v & 0xff);
                long nwr = write((int)fdi, &b, 1);
                stackv[sp++] = (nwr == 1) ? 0 : (nwr < 0 ? -errno : -1);
            } else if (bi == 4) {                 /* close(fd) */
                int64_t fdi = stackv[--sp];
                stackv[sp++] = close((int)fdi);
            } else if (bi == 5) {                 /* Net.bytes("literal") → String packed (v1.18) */
                uint16_t cp16 = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                stackv[sp++] = mk_str_arr(cpool[cp16]);
            } else if (bi == 40) {                /* Net.mkstr(int[] bytes) → String packed */
                long *src = (long *)stackv[--sp];
                if (!src || src < heap || src >= heap + HEAPLEN)
                    dievm("Net.mkstr: sumber bukan array valid");
                long n = src[0];
                long slots = (n + 7) >> 3;
                if (heap_used + slots + 1 > HEAPLEN) dievm("arena penuh (Net.mkstr)");
                long *blk = heap + heap_used;
                heap_used += slots + 1;
                blk[0] = n;
                for (long k = 0; k < slots; k++) blk[1 + k] = 0;
                for (long i = 0; i < n; i++) {
                    long b = src[1 + i];
                    if (b < 0 || b > 255) dievm2("Net.mkstr: byte di luar 0..255", b);
                    setb(blk, i, b);
                }
                stackv[sp++] = (int64_t)blk;
            } else if (bi == 41) {                /* Net.strbytes(String) → int[] */
                long *src = (long *)stackv[--sp];
                if (!src || src < heap || src >= heap + HEAPLEN)
                    dievm("Net.strbytes: sumber bukan string valid");
                long n = src[0];
                if (heap_used + n + 1 > HEAPLEN) dievm("arena penuh (Net.strbytes)");
                long *blk = heap + heap_used;
                heap_used += n + 1;
                blk[0] = n;
                for (long i = 0; i < n; i++) blk[1 + i] = getb(src, i);
                stackv[sp++] = (int64_t)blk;
            } else if (bi == 6) {                 /* Net.openr("literal") → fd */
                uint16_t cp16 = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                int fd = (int)open(cpool[cp16], O_RDONLY);
                stackv[sp++] = (fd < 0) ? (int64_t)-1 : (int64_t)fd;
            } else if (bi == 7) {                 /* recvb(fd, buf) → n | -1 */
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                long len = buf[0];
                long nrd = 0;
                if (len > 0) {
                    unsigned char *tmp = (unsigned char *)malloc((size_t)len);
                    if (!tmp) dievm("Net.recvb: memori habis");
                    nrd = read((int)fdi, tmp, (size_t)len);
                    if (nrd < 0) nrd = -1;
                    for (long i = 0; i < nrd; i++)
                        buf[1 + i] = (long)tmp[i];
                    free(tmp);
                }
                stackv[sp++] = (int64_t)nrd;
            } else if (bi == 8) {                 /* sendb(fd, buf) → 0 | -1 */
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                long len = buf[0];
                long terkirim = 0;
                while (terkirim < len) {
                    unsigned char tmp[4096];
                    long want = len - terkirim;
                    if (want > 4096) want = 4096;
                    for (long i = 0; i < want; i++)
                        tmp[i] = (unsigned char)(buf[1 + terkirim + i] & 0xff);
                    long nwr = write((int)fdi, tmp, (size_t)want);
                    if (nwr <= 0) { terkirim = -1; break; }
                    terkirim += nwr;
                }
                stackv[sp++] = (terkirim >= 0) ? (int64_t)0 : (int64_t)-1;
            } else if (bi == 9) {                 /* bind(fd, ip, port) */
                int64_t port = stackv[--sp], ipbe = stackv[--sp], fdi = stackv[--sp];
                struct sockaddr_in sa;
                memset(&sa, 0, sizeof sa);
                sa.sin_family = AF_INET;
                sa.sin_port = htons((uint16_t)((long)port & 0xffff));
                sa.sin_addr.s_addr = htonl((uint32_t)((long)ipbe & 0xffffffff));
                int r = bind((int)fdi, (struct sockaddr *)&sa, sizeof sa);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 10) {                /* sendto(fd, ip, port, buf) */
                long *buf = (long *)stackv[--sp];
                int64_t port = stackv[--sp], ipbe = stackv[--sp], fdi = stackv[--sp];
                struct sockaddr_in sa;
                memset(&sa, 0, sizeof sa);
                sa.sin_family = AF_INET;
                sa.sin_port = htons((uint16_t)((long)port & 0xffff));
                sa.sin_addr.s_addr = htonl((uint32_t)((long)ipbe & 0xffffffff));
                long len = buf[0];
                if (len > 1200) len = 1200;       /* batas penyangga v1 */
                unsigned char *tmp = (unsigned char *)malloc((size_t)(len > 0 ? len : 1));
                if (!tmp) dievm("Net.sendto: memori habis");
                for (long i = 0; i < len; i++) tmp[i] = (unsigned char)(buf[1 + i] & 0xff);
                ssize_t n = sendto((int)fdi, tmp, (size_t)len, 0,
                                   (struct sockaddr *)&sa, sizeof sa);
                free(tmp);
                stackv[sp++] = (n < 0) ? -errno : (int64_t)n;
            } else if (bi == 11) {                /* recvfrom(fd, buf, from) */
                long *fromArr = (long *)stackv[--sp];
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                unsigned char tmp[1200];
                struct sockaddr_in sa;
                socklen_t sl = sizeof sa;
                memset(&sa, 0, sizeof sa);
                ssize_t n = recvfrom((int)fdi, tmp, sizeof tmp, 0,
                                     (struct sockaddr *)&sa, &sl);
                if (n < 0) {
                    stackv[sp++] = -errno;
                } else {
                    for (long i = 0; i < n; i++) buf[1 + i] = (long)tmp[i];
                    fromArr[1] = (long)ntohl((uint32_t)sa.sin_addr.s_addr);
                    fromArr[2] = (long)ntohs(sa.sin_port);
                    stackv[sp++] = (int64_t)n;
                }
            } else if (bi == 12) {                /* shutdown(fd, how) */
                int64_t how = stackv[--sp], fdi = stackv[--sp];
                int r = shutdown((int)fdi, (int)how);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 13) {                /* openb(path, flags, mode) */
                int64_t mode = stackv[--sp], flags = stackv[--sp];
                long *pa = (long *)stackv[--sp];
                char *path = pack_cpath(pa, 4000);
                int fd = open(path, (int)flags, (int)mode);
                free(path);
                stackv[sp++] = (fd < 0) ? -errno : fd;
            } else if (bi == 14) {                /* mkdirb(path) */
                long *pa = (long *)stackv[--sp];
                char *path = pack_cpath(pa, 4000);
                int r = mkdir(path, 0755);
                free(path);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 15) {                /* unlinkb(path) */
                long *pa = (long *)stackv[--sp];
                char *path = pack_cpath(pa, 4000);
                int r = unlink(path);
                free(path);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 16) {                /* renameb(old, new) */
                long *na = (long *)stackv[--sp];
                long *oa = (long *)stackv[--sp];
                char *nw = pack_cpath(na, 4000);
                char *od = pack_cpath(oa, 4000);
                int r = rename(od, nw);
                free(od); free(nw);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 17) {                /* seek(fd, off, whence) */
                int64_t whence = stackv[--sp], off = stackv[--sp], fdi = stackv[--sp];
                off_t r = lseek((int)fdi, (off_t)off, (int)whence);
                stackv[sp++] = (r < 0) ? -errno : (int64_t)r;
            } else if (bi == 18) {                /* time() */
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                stackv[sp++] = (int64_t)ts.tv_sec;
            } else if (bi == 19) {                /* mono() ms */
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                stackv[sp++] = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            } else if (bi == 20) {                /* sleep(ms) */
                int64_t ms = stackv[--sp];
                struct timespec ts;
                ts.tv_sec = (time_t)(ms / 1000);
                ts.tv_nsec = (long)((ms % 1000) * 1000000);
                int r = nanosleep(&ts, 0);
                stackv[sp++] = (r < 0) ? -errno : 0;
            } else if (bi == 21) {                /* exit(code) */
                int64_t code = stackv[--sp];
                exit((int)code);
            } else if (bi == 22) {                /* cat(a,b) */
                long *b = (long *)stackv[--sp];
                long *a = (long *)stackv[--sp];
                long la = a[0], lb = b[0];
                long total = la + lb;
                long slots = (total + 7) >> 3;
                if (heap_used + slots + 1 > HEAPLEN) dievm("arena penuh (cat)");
                long *blk = heap + heap_used;
                heap_used += slots + 1;
                blk[0] = total;
                for (long k = 0; k < slots; k++) blk[1 + k] = 0;
                for (long i = 0; i < la; i++) setb(blk, i, getb(a, i));
                for (long j = 0; j < lb; j++) setb(blk, la + j, getb(b, j));
                stackv[sp++] = (int64_t)blk;
            } else if (bi == 23 || bi == 24) {    /* sequal / sne */
                long *b = (long *)stackv[--sp];
                long *a = (long *)stackv[--sp];
                int eq = (a[0] == b[0]);
                for (long i = 0; eq && i < a[0]; i++)
                    if (getb(a, i) != getb(b, i)) eq = 0;
                if (bi == 24) eq = eq ? 0 : 1;
                stackv[sp++] = eq;
            } else if (bi == 25) {                /* listen(fd, backlog) */
                int64_t backlog = stackv[--sp], fdi = stackv[--sp];
                int r = listen((int)fdi, (int)backlog);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 26) {                /* accept(fd) */
                int64_t fdi = stackv[--sp];
                int fd2 = accept((int)fdi, 0, 0);
                stackv[sp++] = (fd2 < 0) ? -errno : fd2;
            } else if (bi == 27) {                /* setsockopt(fd,lvl,opt,val) */
                int64_t val = stackv[--sp], opt = stackv[--sp];
                int64_t lvl = stackv[--sp], fdi = stackv[--sp];
                int v = (int)val;
                int r = setsockopt((int)fdi, (int)lvl, (int)opt, &v, sizeof v);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 28) {                /* memmark() */
                stackv[sp++] = (int64_t)heap_used;
            } else if (bi == 29) {                /* memreset(mark) */
                int64_t mark = stackv[--sp];
                if (mark < 0) mark = 0;
                if (mark > heap_used) mark = heap_used;
                heap_used = (long)mark;
                stackv[sp++] = 0;
            } else if (bi == 30) {                /* argc() */
                stackv[sp++] = vargc;
            } else if (bi == 31) {                /* argv(i) */
                int64_t i = stackv[--sp];
                if (i >= 0 && i < vargc)
                    stackv[sp++] = mk_str_arr(vargv[i]);
                else
                    stackv[sp++] = mk_str_arr(0);
            } else if (bi == 32) {                /* getenv(name) */
                long *pa = (long *)stackv[--sp];
                char *name = pack_cpath(pa, 4000);
                char *v = getenv(name);
                free(name);
                stackv[sp++] = mk_str_arr(v);
            } else if (bi == 33) {                /* recv1(fd, buf): satu read */
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                long len = buf[0];
                if (len > 4000) len = 4000;
                unsigned char tmp[4000];
                ssize_t n = read((int)fdi, tmp, (size_t)(len > 0 ? len : 1));
                if (n > 0) for (long i = 0; i < n; i++) buf[1 + i] = (long)tmp[i];
                stackv[sp++] = (n < 0) ? -errno : (int64_t)n;
            } else if (bi == 39) {                /* pollfd(fds, revents, ms) */
                int64_t ms = stackv[--sp];
                long *rev = (long *)stackv[--sp];
                long *fds = (long *)stackv[--sp];
                long n = fds[0];
                if (n > 64) n = 64;
                struct pollfd pf[64];
                for (long i = 0; i < n; i++) {
                    pf[i].fd = (int)fds[1 + i];
                    pf[i].events = POLLIN;
                    pf[i].revents = 0;
                }
                int r = poll(pf, (nfds_t)n, (int)ms);
                int cnt = 0;
                for (long i = 0; i < n; i++) {
                    int v = 0;
                    if (pf[i].revents & POLLIN) v |= 1;
                    if (pf[i].revents & (POLLERR | POLLHUP | POLLNVAL)) v |= 2;
                    rev[1 + i] = v;
                    if (pf[i].revents) cnt++;
                }
                stackv[sp++] = (r < 0) ? -errno : cnt;
            } else if (bi == 38) {                /* settimeout(fd, ms) */
                int64_t ms = stackv[--sp], fdi = stackv[--sp];
                struct timeval tv;
                tv.tv_sec = (time_t)(ms / 1000);
                tv.tv_usec = (suseconds_t)((ms % 1000) * 1000);
                int r = setsockopt((int)fdi, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
                if (r == 0)
                    r = setsockopt((int)fdi, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
                stackv[sp++] = (r < 0) ? -errno : 0;
            } else if (bi == 37) {                /* localport(fd) */
                int64_t fdi = stackv[--sp];
                struct sockaddr_in sa;
                socklen_t sl = sizeof sa;
                memset(&sa, 0, sizeof sa);
                int r = getsockname((int)fdi, (struct sockaddr *)&sa, &sl);
                stackv[sp++] = (r < 0) ? -errno : (int64_t)ntohs(sa.sin_port);
            } else if (bi == 34) {                /* setnonblock(fd) */
                int64_t fdi = stackv[--sp];
                int fl = fcntl((int)fdi, F_GETFL, 0);
                int r = (fl < 0) ? -1 : fcntl((int)fdi, F_SETFL, fl | O_NONBLOCK);
                stackv[sp++] = (r < 0) ? -errno : 0;
            } else if (bi == 35) {                /* key(): stdin nonblocking */
                fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
                unsigned char ch;
                ssize_t n = read(0, &ch, 1);
                stackv[sp++] = (n == 1) ? (int64_t)ch : -1;
            } else if (bi == 36) {                /* ttyRaw(on) */
                int64_t on = stackv[--sp];
                int r = system(on ? "stty raw -echo 2>/dev/null" : "stty sane 2>/dev/null");
                stackv[sp++] = (r == -1) ? -1 : 0;
            } else {
                dievm("Net builtin tidak dikenal");
            }
            break;
        }
        case OP_ICONST64: {
            uint64_t v = rd64code(m, &f->pc);
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = (int64_t)v;
            break;
        }
        case OP_ADD64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a + b; break;
        case OP_SUB64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a - b; break;
        case OP_MUL64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a * b; break;
        case OP_DIV64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("pembagian dengan nol");
            stackv[sp++] = a / b;
            break;
        case OP_MOD64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("modulus dengan nol");
            stackv[sp++] = a % b;
            break;
        case OP_UDIV64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("pembagian dengan nol");
            stackv[sp++] = (int64_t)((uint64_t)a / (uint64_t)b);
            break;
        case OP_UMOD64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("modulus dengan nol");
            stackv[sp++] = (int64_t)((uint64_t)a % (uint64_t)b);
            break;
        case OP_AND64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a & b; break;
        case OP_OR64:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a | b; break;
        case OP_XOR64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a ^ b; break;
        case OP_SHL64:
        case OP_SHR64:
        case OP_USHR64: {
            int mask = m->code[f->pc++];
            b = stackv[--sp]; a = stackv[--sp];
            int c = (int)(b & mask);
            if (op == OP_SHL64) stackv[sp++] = (int64_t)((uint64_t)a << c);
            else if (op == OP_SHR64) stackv[sp++] = a >> c;
            else stackv[sp++] = (int64_t)((uint64_t)a >> c);
            break;
        }
        case OP_NEG64: a = stackv[--sp]; stackv[sp++] = -a; break;
        case OP_BNOT64: a = stackv[--sp]; stackv[sp++] = ~a; break;
        case OP_LT64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a < b; break;
        case OP_GT64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a > b; break;
        case OP_LE64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a <= b; break;
        case OP_GE64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a >= b; break;
        case OP_LTU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a < (uint64_t)b; break;
        case OP_GTU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a > (uint64_t)b; break;
        case OP_LEU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a <= (uint64_t)b; break;
        case OP_GEU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a >= (uint64_t)b; break;
        case OP_EQ64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a == b; break;
        case OP_NE64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a != b; break;
        case OP_FADD:
        case OP_FSUB:
        case OP_FMUL:
        case OP_FDIV:
        case OP_FMOD: {
            int sub = m->code[f->pc++];
            b = stackv[--sp]; a = stackv[--sp];
            if (sub == 0) {
                float x = bits_f(a), y = bits_f(b);
                if (op == OP_FADD) stackv[sp++] = f_bits(x + y);
                else if (op == OP_FSUB) stackv[sp++] = f_bits(x - y);
                else if (op == OP_FMUL) stackv[sp++] = f_bits(x * y);
                else if (op == OP_FDIV) stackv[sp++] = f_bits(x / y);
                else {
                    double q = dtrunc_safe((double)(x / y));
                    stackv[sp++] = f_bits((float)((double)x - q * (double)y));
                }
            } else {
                double x = bits_d(a), y = bits_d(b);
                if (op == OP_FADD) stackv[sp++] = d_bits(x + y);
                else if (op == OP_FSUB) stackv[sp++] = d_bits(x - y);
                else if (op == OP_FMUL) stackv[sp++] = d_bits(x * y);
                else if (op == OP_FDIV) stackv[sp++] = d_bits(x / y);
                else {
                    double q = dtrunc_safe(x / y);
                    stackv[sp++] = d_bits(x - q * y);
                }
            }
            break;
        }
        case OP_FNEG: {
            int sub = m->code[f->pc++];
            a = stackv[--sp];
            stackv[sp++] = (sub == 0) ? f_bits(-bits_f(a)) : d_bits(-bits_d(a));
            break;
        }
        case OP_FLT:
        case OP_FGT:
        case OP_FLE:
        case OP_FGE:
        case OP_FEQ:
        case OP_FNE: {
            int sub = m->code[f->pc++];
            b = stackv[--sp]; a = stackv[--sp];
            if (sub == 0) {
                float x = bits_f(a), y = bits_f(b);
                if (op == OP_FLT) stackv[sp++] = x < y;
                else if (op == OP_FGT) stackv[sp++] = x > y;
                else if (op == OP_FLE) stackv[sp++] = x <= y;
                else if (op == OP_FGE) stackv[sp++] = x >= y;
                else if (op == OP_FEQ) stackv[sp++] = x == y;
                else stackv[sp++] = x != y;
            } else {
                double x = bits_d(a), y = bits_d(b);
                if (op == OP_FLT) stackv[sp++] = x < y;
                else if (op == OP_FGT) stackv[sp++] = x > y;
                else if (op == OP_FLE) stackv[sp++] = x <= y;
                else if (op == OP_FGE) stackv[sp++] = x >= y;
                else if (op == OP_FEQ) stackv[sp++] = x == y;
                else stackv[sp++] = x != y;
            }
            break;
        }
        case OP_CONV: {
            int from = m->code[f->pc++];
            int to = m->code[f->pc++];
            a = stackv[--sp];
            stackv[sp++] = conv_val(a, from, to);
            break;
        }
        case OP_NORM: {
            int t = m->code[f->pc++];
            stackv[sp - 1] = norm_val(stackv[sp - 1], t);
            break;
        }
        case OP_PRINTNUM: {
            int t = m->code[f->pc++];
            int ln = m->code[f->pc++];
            int64_t v = stackv[--sp];
            if (t == T_LONG) printf("%" PRId64 "%s", v, ln ? "\n" : "");
            else if (t == T_UINT64) printf("%" PRIu64 "%s", (uint64_t)v, ln ? "\n" : "");
            else if (t == T_UINT8 || t == T_UINT16 || t == T_UINT32)
                printf("%" PRIu32 "%s", (uint32_t)v, ln ? "\n" : "");
            else if (t == T_BYTE || t == T_SHORT)
                printf("%" PRId64 "%s", v, ln ? "\n" : "");
            else if (t == T_FLOAT) printf("%.9g%s", (double)bits_f(v), ln ? "\n" : "");
            else printf("%.15g%s", bits_d(v), ln ? "\n" : "");
            break;
        }
        case OP_PRINTBUF: {
            int ln = m->code[f->pc++];
            long *buf = (long *)stackv[--sp];
            long len = buf[0];
            unsigned char *tmp = (unsigned char *)malloc((size_t)(len > 0 ? len : 1));
            if (!tmp) dievm("print: memori habis");
            for (long i = 0; i < len; i++) tmp[i] = (unsigned char)(buf[1 + i] & 0xff);
            fwrite(tmp, 1, (size_t)len, stdout);
            free(tmp);
            if (ln) fputc(10, stdout);
            break;
        }
        case OP_PRINTSTR: {                   /* v1.18: String packed → fd 1 */
            int ln = m->code[f->pc++];
            long *blk = (long *)stackv[--sp];
            long len = blk[0];
            for (long i = 0; i < len; i++) fputc((int)getb(blk, i), stdout);
            if (ln) fputc(10, stdout);
            break;
        }
        case OP_MARK:                          /* v1.20: malloc{} */
            stackv[sp++] = (int64_t)heap_used;
            break;
        case OP_MRESET: {
            int64_t mark = stackv[--sp];
            if (mark < 0) mark = 0;
            if (mark > heap_used) mark = heap_used;
            heap_used = (long)mark;
            break;
        }
        case OP_HALT:
            if (sp > 0) exit((int)(stackv[0] & 0xff));
            exit(0);
        default:
            dievm("operasi tidak dikenal");
        }
    }
}

int main(int argc, char **argv) {
    if (argc > 2) { vargc = argc - 2; vargv = argv + 2; }
    else { vargc = 0; vargv = argv; }
    if (argc < 2) {
        fprintf(stderr, "usage: kopivm <file.kbc>\n");
        return 2;
    }
    load(argv[1]);
    run();
    return 0;
}
