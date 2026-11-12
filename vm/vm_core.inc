/* vm_core.inc — header, state global, loader, helper runtime kopivm.
 * Bagian dari vm/kopivm.c (unit kompilasi sama). */

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
#include <sys/mman.h>
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
    OP_MRESET,      /* v1.20: tarik penanda & bebaskan arena ke situ */
    OP_RENTER,      /* v1.21: 4 byte ukuran; masuk arena blok malloc<...> */
    OP_RLEAVE,      /* v1.21: keluar arena blok & bebaskan */
    /* v1.22: superinstruksi loop panas */
    OP_CMP_JMPT, OP_CMP_JMPF,
    OP_IINC,
    OP_BIN_L,
    OP_OP_STORE,
    OP_IDXR_L,
    OP_IDXW_L,
    OP_LOOP
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

/* heap untuk closure & array (bump arena; GC belum ada).
 * Arena global 32 MB; setiap blok malloc<...> mendapat arena sendiri.
 * v1.22: blok `malloc { ... }` (tanpa ukuran) = OTOMATIS — mmap 4096 MB
 * dengan MAP_NORESERVE, sehingga RAM baru terpakai saat disentuh. */
#define HEAPLEN (1 << 22)          /* slot long (32 MB) */
#define AUTOCAP (4096L * 1024 * 1024)   /* batas arena otomatis */
static long *heap;
static long heap_cap;              /* kapasitas arena aktif, satuan slot */
static long heap_used;
static int heap_mmapped;           /* arena aktif hasil mmap? */
static int heap_auto;              /* batas otomatis (pesan error) */

/* tumpukan konteks arena untuk malloc{} bersarang */
typedef struct {
    long *base; long cap; long used;
    int mmapped, auto_mode;
} ArenaCtx;
#define MAXARENA 64
static ArenaCtx astk[MAXARENA];
static int asp;

/* pointer berasal dari arena aktif atau salah satu arena luar (nested) */
static int in_heap(const long *p) {
    if (p >= heap && p < heap + heap_cap) return 1;
    for (int i = asp - 1; i >= 0; i--)
        if (p >= astk[i].base && p < astk[i].base + astk[i].cap) return 1;
    return 0;
}

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
/* rekomendasi singkat untuk error runtime yang umum */
static const char *rt_hint(const char *m) {
    if (strstr(m, "indeks array di luar batas"))
        return "indeks valid 0..panjang-1; periksa batas loop dan nilai indeks.";
    if (strstr(m, "array tidak valid"))
        return "pastikan array dibuat dengan `new` dan tidak dipakai setelah blok malloc{} berakhir.";
    if (strstr(m, "pembagian dengan nol"))
        return "pastikan pembagi != 0 sebelum operasi pembagian/modulo.";
    if (strstr(m, "arena malloc habis"))
        return "naikkan batas dengan `malloc<64mb>`, atau gunakan `malloc { }` (otomatis).";
    if (strstr(m, "ukuran array negatif"))
        return "ukuran `new int[n]` harus >= 0.";
    if (strstr(m, "magic tidak cocok"))
        return "file .kbc rusak/versi lain; kompilasi ulang dengan kopicc yang sama.";
    if (strstr(m, "bukan file KOP1"))
        return "jalankan `kopicc file.kop -t vm -o file.kbc` lalu `kopivm file.kbc`.";
    return 0;
}

static void dievm(const char *m) {
    fprintf(stderr, "kopivm: error: %s\n", m);
    const char *h = rt_hint(m);
    if (h) fprintf(stderr, "  saran: %s\n", h);
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

/* error saat batas arena aktif (default 32 MB / malloc<...>) terlampaui */
static void oom_arena(void) {
    char buf[224];
    long bytes = heap_cap * 8;
    const char *auto_ = heap_auto ? "otomatis " : "";
    const char *hint = heap_auto ? "pakai malloc<N> untuk batas lain" :
                                   "naikkan malloc<...>";
    if (bytes >= (1L << 20))
        snprintf(buf, sizeof buf,
            "arena malloc habis — batas %s%ld MB terlampaui (%s)",
            auto_, bytes >> 20, hint);
    else
        snprintf(buf, sizeof buf,
            "arena malloc habis — batas %s%ld KB terlampaui (%s)",
            auto_, bytes >> 10, hint);
    dievm(buf);
}

static int64_t mk_str_arr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    /* v1.18: String packed — 8 byte per slot 64-bit, little-endian */
    long slots = ((long)n + 7) >> 3;
    if (heap_used + slots + 1 > heap_cap) oom_arena();
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

/* ── v1.22: helper superinstruksi loop panas ─────────────────────── */
static inline int cmp_i32(int32_t x, int32_t y, int c) {
    switch (c) {
    case 0: return x < y;
    case 1: return x <= y;
    case 2: return x > y;
    case 3: return x >= y;
    case 4: return x == y;
    default: return x != y;
    }
}
static inline int cmp_i64(int64_t x, int64_t y, int c) {
    switch (c) {
    case 0: return x < y;
    case 1: return x <= y;
    case 2: return x > y;
    case 3: return x >= y;
    case 4: return x == y;
    default: return x != y;
    }
}
/* aritmetika int32 dengan wrap, sama seperti OP_ADD/SUB/MUL/… */
static inline int64_t arith_i32(int64_t x, int64_t y, int op) {
    int32_t a = (int32_t)x, b = (int32_t)y;
    switch (op) {
    case 0: return (int32_t)((int64_t)a + (int64_t)b);
    case 1: return (int32_t)((int64_t)a - (int64_t)b);
    case 2: return (int32_t)((int64_t)a * (int64_t)b);
    case 3: return a & b;
    case 4: return a | b;
    default: return a ^ b;
    }
}
static inline uint32_t rd32c(const uint8_t *c) {
    return (uint32_t)c[0] | ((uint32_t)c[1] << 8) |
           ((uint32_t)c[2] << 16) | ((uint32_t)c[3] << 24);
}

