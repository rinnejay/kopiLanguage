#include "vm_core.inc"

/* v1.45: thread OS di VM. Handle yang dilihat program adalah pointer
 * ke VMThread (disimpan sebagai long), sama seperti handle di backend a64.
 * Setiap thread punya VState sendiri: stack operand, frame, locals, dan
 * arena heap. Arena tidak dibebaskan saat thread selesai agar objek yang
 * dibagikan (array/closure) tetap sah selama thread lain memakainya;
 * join memberi sinkronisasi memori seperti pthread_join. */
typedef struct {
    pthread_t tid;
    _Atomic int done;
    long ret;
    int id;
    int joined;
    int64_t clo;
    VState *vs;
} VMThread;

#define MAXVTHREADS 8192
static VMThread *vthreads[MAXVTHREADS];
static _Atomic int nvthreads;
static _Atomic int thread_id_next = 2;      /* 1 = thread utama */

static long vm_loop(void);

static VMThread *find_vthread(long h) {
    if (!h) return 0;
    int n = atomic_load_explicit(&nvthreads, memory_order_acquire);
    if (n > MAXVTHREADS) n = MAXVTHREADS;
    for (int i = 0; i < n; i++)
        if ((long)vthreads[i] == h) return vthreads[i];
    return 0;
}

static int vm_thread_valid(int64_t clo, Meth **out) {    long *blk = (long *)clo;
    if (clo == 0 || !in_heap(blk)) return 0;
    long cm = blk[0];
    if (cm < 0 || cm >= nmeths) return 0;
    Meth *mm = &meths[cm];
    /* badan thread: lambda tanpa parameter → env + 0 argumen pengguna */
    if (mm->nargs != 1 || blk[2] != 0) return 0;
    if (out) *out = mm;
    return 1;
}

static void *vm_thread_entry(void *arg) {
    VMThread *t = (VMThread *)arg;
    vt = t->vs;
    Meth *mm;
    if (!vm_thread_valid(t->clo, &mm)) {
        fprintf(stderr, "kopivm: error: badan thread bukan lambda tanpa parameter\n");
        exit(1);
    }
    frames[0].meth = mm - meths;
    frames[0].pc = 0;
    frames[0].retbase = 0;
    frames[0].line = 0;
    frames[0].file = 0;
    localsv[0][0] = t->clo;
    nframes = 1;
    t->ret = vm_loop();
    atomic_store_explicit(&t->done, 1, memory_order_release);
    return 0;
}

static long vm_thread_spawn(int64_t clo) {
    if (!vm_thread_valid(clo, 0))
        dievm("thread: badan bukan lambda tanpa parameter");
    VMThread *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->id = atomic_fetch_add_explicit(&thread_id_next, 1, memory_order_relaxed);
    t->clo = clo;
    t->vs = vstate_new(t->id);
    int idx = atomic_fetch_add_explicit(&nvthreads, 1, memory_order_relaxed);
    if (idx >= MAXVTHREADS) return 0;
    vthreads[idx] = t;
    atomic_thread_fence(memory_order_release);
    if (pthread_create(&t->tid, 0, vm_thread_entry, t) != 0) {
        vthreads[idx] = 0;
        return 0;
    }
    return (long)t;
}

static long vm_thread_join(long h) {
    VMThread *t = find_vthread(h);
    if (!t) return 0;
    if (!t->joined) {
        pthread_join(t->tid, 0);
        t->joined = 1;
    }
    return t->ret;
}

static long vm_thread_alive(long h) {
    VMThread *t = find_vthread(h);
    if (!t) return 0;
    return atomic_load_explicit(&t->done, memory_order_acquire) ? 0 : 1;
}

static long vm_thread_id(long h) {
    VMThread *t = find_vthread(h);
    return t ? t->id : 0;
}

static long vm_thread_self(void) {
    return vt ? vt->id : 0;
}

/* jalankan interpreter untuk state thread aktif sampai frame habis;
 * mengembalikan kode keluar (nilai balik main / badan thread). */
static long vm_loop(void) {
    for (;;) {
        /* nilai balik penuh: badan thread bisa `return` > 255; kode keluar
         * proses tetap dipotong OS ke 8 bit saat main() kembali. */
        if (nframes <= 0) return sp > 0 ? stackv[0] : 0;
        Frame *f = &frames[nframes - 1];
        Meth *m = &meths[f->meth];
        if (f->pc >= m->clen) dievm("pc luar batas — bytecode korup");
        uint8_t op = m->code[f->pc++];
        int64_t a, b;

        switch (op) {
#include "vm_ops_base.inc"

#include "vm_ops_net.inc"

#include "vm_ops_num.inc"

#include "vm_ops_fast.inc"

        case OP_HALT:
            return sp > 0 ? stackv[0] : 0;
        default:
            dievm("operasi tidak dikenal");
        }
    }
}

static long run(void) {
    int mid = -1;
    for (int i = 0; i < nmeths; i++)
        if (!strcmp(meths[i].name, "main") && meths[i].nargs <= 1) { mid = i; break; }
    if (mid < 0) dievm("tidak ada metode main");

    vt = vstate_new(1);

    nframes = 1;
    frames[0].meth = mid;
    frames[0].pc = 0;
    frames[0].retbase = 0;
    frames[0].line = 0;
    frames[0].file = 0;
    memset(localsv[0], 0, (size_t)MAXLOC * sizeof(int64_t));

    return vm_loop();
}

int main(int argc, char **argv) {
    if (argc > 2) { vargc = argc - 2; vargv = argv + 2; }
    else { vargc = 0; vargv = argv; }
    if (argc < 2) {
        fprintf(stderr, "usage: kopivm <file.kbc>\n");
        return 2;
    }
    load(argv[1]);
    return (int)run();
}
