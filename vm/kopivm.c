#include "vm_core.inc"

static void run(void) {
    int mid = -1;
    for (int i = 0; i < nmeths; i++)
        if (!strcmp(meths[i].name, "main") && meths[i].nargs <= 1) { mid = i; break; }
    if (mid < 0) dievm("tidak ada metode main");

    heap = malloc(sizeof(long) * (size_t)HEAPLEN);
    if (!heap) dievm("heap closure gagal dialokasi");
    heap_cap = HEAPLEN;
    heap_used = 0;
    heap_mmapped = 0;
    heap_auto = 0;

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
#include "vm_ops_base.inc"

#include "vm_ops_net.inc"

#include "vm_ops_num.inc"

#include "vm_ops_fast.inc"

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
