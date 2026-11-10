/* kopivm — mesin virtual untuk bytecode KOP1 yang dihasilkan kopicc -t vm. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

static void dievm(const char *m) {
    fprintf(stderr, "kopivm: %s\n", m);
    exit(1);
}

/* heap untuk closure & array (bump arena; GC belum ada) */
/* heap untuk closure & array (bump arena; GC belum ada).
 * 1 << 22 long = 32 MB: cukup untuk beberapa X25519 penuh (ladder meng-
 * alokasi ~0,3 MB long per panggilan tanpa GC) */
#define HEAPLEN (1 << 22)
static long *heap;
static long heap_used;

static void dievm2(const char *m, long v) {
    fprintf(stderr, "kopivm: %s (%ld)\n", m, v);
    exit(1);
}

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

typedef struct { int meth; long pc; long retbase; } Frame;

static int64_t stackv[MAXSTACK];
static long sp;
static Frame frames[MAXFRAMES];
static int nframes;
static int64_t localsv[MAXFRAMES][MAXLOC];

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
    memset(localsv[0], 0, sizeof localsv[0]);

    for (;;) {
        if (nframes <= 0) { if (sp > 0) exit((int)(stackv[0] & 0xff)); exit(0); }
        Frame *f = &frames[nframes - 1];
        Meth *m = &meths[f->meth];
        if (f->pc >= m->clen) dievm("pc luar batas — bytecode korup");
        uint8_t op = m->code[f->pc++];
        int64_t a, b;

        switch (op) {
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
            } else if (bi == 5) {                 /* Net.bytes("literal") */
                uint16_t cp16 = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                const char *s = cpool[cp16];
                long n = 0;
                while (s[n]) n++;                 /* strlen (byte-as-int, tanpa NUL) */
                if (heap_used + n + 1 > HEAPLEN) dievm("arena penuh (Net.bytes)");
                long *blk = heap + heap_used;
                heap_used += n + 1;
                blk[0] = n;
                for (long i = 0; i < n; i++)
                    blk[1 + i] = (long)(unsigned char)s[i];
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
            } else {
                dievm("Net builtin tidak dikenal");
            }
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
    if (argc < 2) {
        fprintf(stderr, "usage: kopivm <file.kbc>\n");
        return 2;
    }
    load(argv[1]);
    run();
    return 0;
}
