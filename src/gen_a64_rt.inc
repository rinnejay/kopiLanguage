/* gen_a64_rt.inc — runtime assembly a64 + data (bagian dari gen_a64.c). */

static void gen_a64_runtime(void) {
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

    emit(".Kbnd:");
    emit("\tadrp\tx0, :got:.Lmsg_bnd");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_bnd]");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    emit(".Koom:");
    tls_addr("x9");
    emit("\tldr\tx9, [x9, #32]");           /* auto? */
    emit("\tcbnz\tx9, .KoomAuto");
    tls_addr("x9");
    emit("\tldr\tx1, [x9, #8]");            /* cap */
    emit("\tmovz\tx10, #0x10, lsl #16");     /* 1 MB */
    emit("\tcmp\tx1, x10");
    emit("\tb.hs\t.KoomMB");
    emit("\tlsr\tx1, x1, #10");              /* < 1 MB → KB */
    emit("\tadrp\tx0, :got:.Lmsg_oom_kb");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom_kb]");
    emit("\tb\t.KoomPrint");
    emit(".KoomMB:");
    emit("\tlsr\tx1, x1, #20");              /* ≥ 1 MB → MB */
    emit("\tadrp\tx0, :got:.Lmsg_oom");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom]");
    emit("\tb\t.KoomPrint");
    emit(".KoomAuto:");
    emit("\tadrp\tx0, :got:.Lmsg_oom_auto");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom_auto]");
    emit(".KoomPrint:");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    /* kopalloc: x0 = byte yang diminta -> x0 = pointer dari arena bump */
    emit(".text");
    emit("\t.globl kopalloc");
    emit("kopalloc:");
    tls_addr("x1");
    emit("\tldr\tx2, [x1, #16]");           /* used */
    emit("\tadd\tx2, x2, x0");
    emit("\tldr\tx3, [x1, #8]");            /* cap */
    emit("\tcmp\tx2, x3");
    emit("\tb.ge\t.Koom");
    emit("\tstr\tx2, [x1, #16]");
    emit("\tsub\tx2, x2, x0");
    emit("\tldr\tx3, [x1, #0]");            /* base */
    emit("\tadd\tx0, x3, x2");
    emit("\tret");

    /* v1.49: kop_throw(x0 = nilai exception) — lompat ke blok catch
       terdekat dengan memulihkan sp/x29/x30 dari catatan handler.
       Catatan ada di frame metode pemilik try (lihat gen_stmt ST_TRY):
       [0]=prev [8]=sp [16]=x29 [24]=x30 [32]=landing pad [40]=nilai
       [48..127]=x19..x28. */
    emit(".text");
    emit("\t.globl kop_throw");
    emit("kop_throw:");
    tls_addr("x9");
    emit("\tldr\tx10, [x9, #56]");
    emit("\tcbz\tx10, .Kuncaught");
    emit("\tstr\tx0, [x10, #40]");         /* nilai exception */
    emit("\tldr\tx11, [x10, #0]");         /* prev handler */
    emit("\tstr\tx11, [x9, #56]");
    emit("\tldr\tx12, [x10, #32]");        /* landing pad */
    emit("\tldr\tx11, [x10, #8]");         /* sp saat try masuk */
    emit("\tldr\tx29, [x10, #16]");
    emit("\tldr\tx30, [x10, #24]");
    emit("\tldp\tx19, x20, [x10, #48]");
    emit("\tldp\tx21, x22, [x10, #64]");
    emit("\tldp\tx23, x24, [x10, #80]");
    emit("\tldp\tx25, x26, [x10, #96]");
    emit("\tldp\tx27, x28, [x10, #112]");
    emit("\tmov\tsp, x11");
    emit("\tldr\tx0, [x10, #40]");
    emit("\tbr\tx12");
    emit(".Kuncaught:");
    emit("\tadrp\tx0, :got:.Lmsg_exc");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_exc]");
    emit("\tadrp\tx1, :got:stderr");
    emit("\tldr\tx1, [x1, :got_lo12:stderr]");
    emit("\tldr\tx1, [x1]");
    emit("\tbl\tfputs");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    /* v1.22/v1.45: kop_arena_enter(x0 = byte) -> x1 = handle (40 byte).
       State arena aktif disimpan thread-lokal (TLS kop_tls).
       0 = OTOMATIS: mmap 4 GB MAP_NORESERVE (RAM hanya sesuai kebutuhan).
       Handle: [0]=base, [8]=cap, [16]=used, [24]=mmapped, [32]=auto. */
    emit(".text");
    emit("\t.globl kop_arena_enter");
    emit("kop_arena_enter:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");                    /* ukuran byte (0 = auto) */
    emit("\tmov\tx0, #40");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .Koom");                  /* gagal alokasi handle */
    emit("\tmov\tx20, x0");
    /* simpan konteks arena luar + flag di handle */
    tls_addr("x9");
    emit("\tldr\tx10, [x9, #0]");
    emit("\tstr\tx10, [x20, #0]");
    emit("\tldr\tx10, [x9, #8]");
    emit("\tstr\tx10, [x20, #8]");
    emit("\tldr\tx10, [x9, #16]");
    emit("\tstr\tx10, [x20, #16]");
    emit("\tldr\tx10, [x9, #24]");
    emit("\tstr\tx10, [x20, #24]");
    emit("\tldr\tx10, [x9, #32]");
    emit("\tstr\tx10, [x20, #32]");
    /* mode otomatis? */
    emit("\tcmp\tx19, #0");
    emit("\tb.ne\t.KenterExplicit");
    emit("\tmov\tx0, #0");                    /* addr = NULL */
    emit("\tmov\tx1, #1");
    emit("\tlsl\tx1, x1, #32");               /* 4096 MB */
    emit("\tmov\tx2, #3");                    /* PROT_READ|PROT_WRITE */
    emit("\tmovz\tx3, #0x4022");              /* PRIVATE|ANON|NORESERVE */
    emit("\tmovn\tx4, #0");                   /* fd = -1 */
    emit("\tmov\tx5, #0");                    /* offset = 0 */
    emit("\tbl\tmmap");
    emit("\tmov\tx9, #-1");
    emit("\tcmp\tx0, x9");
    emit("\tb.eq\t.KenterAutoFail");
    tls_addr("x9");
    emit("\tstr\tx0, [x9, #0]");              /* base */
    emit("\tmov\tx1, #1");
    emit("\tlsl\tx1, x1, #32");
    emit("\tstr\tx1, [x9, #8]");              /* cap */
    emit("\tstr\txzr, [x9, #16]");            /* used */
    emit("\tmov\tx1, #1");
    emit("\tstr\tx1, [x9, #24]");             /* mmapped */
    emit("\tstr\tx1, [x9, #32]");             /* auto */
    emit("\tb\t.KenterDone");
    emit(".KenterExplicit:");
    emit("\tmov\tx0, x19");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .KenterOom");
    tls_addr("x9");
    emit("\tstr\tx0, [x9, #0]");
    emit("\tstr\tx19, [x9, #8]");
    emit("\tstr\txzr, [x9, #16]");
    emit("\tstr\txzr, [x9, #24]");
    emit("\tstr\txzr, [x9, #32]");
    emit(".KenterDone:");
    emit("\tmov\tx1, x20");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");
    emit(".KenterOom:");
    emit("\tmov\tx0, x20");
    emit("\tbl\tfree");
    emit("\tb\t.Koom");
    emit(".KenterAutoFail:");
    emit("\tmov\tx0, x20");
    emit("\tbl\tfree");
    emit("\tadrp\tx0, :got:.Lmsg_oom_auto_fail");
    emit("\tldr\tx0, [x0, :got_lo12:.Lmsg_oom_auto_fail]");
    emit("\tbl\tprintf");
    emit("\tmov\tw0, #1");
    emit("\tadrp\tx9, :got:exit");
    emit("\tldr\tx9, [x9, :got_lo12:exit]");
    emit("\tblr\tx9");

    /* v1.22/v1.45: kop_arena_leave(x1 = handle) — bebas arena blok (munmap
       bila mmap), pulihkan arena luar + flag; x0 (nilai balik) dipertahankan. */
    emit(".text");
    emit("\t.globl kop_arena_leave");
    emit("kop_arena_leave:");
    emit("\tstp\tx29, x30, [sp, #-48]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tstp\tx21, x22, [sp, #32]");
    emit("\tmov\tx19, x0");                    /* simpan nilai balik */
    emit("\tmov\tx20, x1");                    /* handle (berisi arena luar) */
    tls_addr("x21");
    emit("\tldr\tx9, [x21, #24]");             /* mmapped? */
    emit("\tcbz\tx9, .KleaveFree");
    emit("\tldr\tx0, [x21, #0]");              /* base arena aktif */
    emit("\tldr\tx1, [x21, #8]");              /* cap arena aktif */
    emit("\tbl\tmunmap");
    emit("\tb\t.KleaveRestore");
    emit(".KleaveFree:");
    emit("\tldr\tx0, [x21, #0]");
    emit("\tbl\tfree");
    emit(".KleaveRestore:");
    tls_addr("x9");
    emit("\tldr\tx10, [x20, #0]");
    emit("\tstr\tx10, [x9, #0]");
    emit("\tldr\tx10, [x20, #8]");
    emit("\tstr\tx10, [x9, #8]");
    emit("\tldr\tx10, [x20, #16]");
    emit("\tstr\tx10, [x9, #16]");
    emit("\tldr\tx10, [x20, #24]");
    emit("\tstr\tx10, [x9, #24]");
    emit("\tldr\tx10, [x20, #32]");
    emit("\tstr\tx10, [x9, #32]");
    emit("\tmov\tx0, x20");
    emit("\tbl\tfree");
    emit("\tmov\tx0, x19");
    emit("\tldp\tx21, x22, [sp, #32]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #48");
    emit("\tret");

    /* v1.45: arena awal thread utama — 32 MB, id = 1 */
    emit(".text");
    emit("\t.globl kop_arena_init_main");
    emit("kop_arena_init_main:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit_imm64("x0", 32UL * 1024 * 1024);
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .Koom");
    emit("\tmov\tx19, x0");
    tls_addr("x9");
    emit("\tstr\tx19, [x9, #0]");
    emit_imm64("x10", 32UL * 1024 * 1024);
    emit("\tstr\tx10, [x9, #8]");
    emit("\tstr\txzr, [x9, #16]");
    emit("\tstr\txzr, [x9, #24]");
    emit("\tstr\txzr, [x9, #32]");
    emit("\tmov\tw10, #1");
    emit("\tstr\tx10, [x9, #40]");            /* id thread utama */
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    /* v1.45: arena thread baru — 32 MB sendiri, id dari counter global */
    emit(".text");
    emit("\t.globl kop_arena_thread_init");
    emit("kop_arena_thread_init:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit_imm64("x0", 32UL * 1024 * 1024);
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .Koom");
    emit("\tmov\tx19, x0");
    tls_addr("x9");
    emit("\tstr\tx19, [x9, #0]");
    emit_imm64("x10", 32UL * 1024 * 1024);
    emit("\tstr\tx10, [x9, #8]");
    emit("\tstr\txzr, [x9, #16]");
    emit("\tstr\txzr, [x9, #24]");
    emit("\tstr\txzr, [x9, #32]");
    emit("\tadrp\tx10, kop_thread_next");
    emit("\tadd\tx10, x10, :lo12:kop_thread_next");
    emit(".Ltidinit:");
    emit("\tldaxr\tw11, [x10]");
    emit("\tadd\tw12, w11, #1");
    emit("\tstlxr\tw13, w12, [x10]");
    emit("\tcbnz\tw13, .Ltidinit");
    emit("\tstr\tw11, [x9, #40]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    /* v1.45: memMark/memReset memakai state TLS */
    emit(".text");
    emit("\t.globl kop_arena_mark");
    emit("kop_arena_mark:");
    tls_addr("x9");
    emit("\tldr\tw0, [x9, #16]");
    emit("\tret");
    emit("\t.globl kop_arena_reset");
    emit("kop_arena_reset:");
    tls_addr("x9");
    emit("\tcmp\tw0, #0");
    emit("\tb.ge\t.LARS0");
    emit("\tmov\tw0, wzr");
    emit(".LARS0:");
    emit("\tldr\tw10, [x9, #16]");
    emit("\tcmp\tw0, w10");
    emit("\tb.le\t.LARS1");
    emit("\tmov\tw0, w10");
    emit(".LARS1:");
    emit("\tstr\tw0, [x9, #16]");
    emit("\tret");

    /* ── v1.45: thread OS di atas pthread (libc) ─────────────────── */

    /* x0 = env closure -> x0 = tcb | 0.
       tcb: [0]=pthread_t, [8]=done, [16]=ret, [24]=id (32 byte) */
    emit(".text");
    emit("\t.globl kop_thread_spawn");
    emit("kop_thread_spawn:");
    emit("\tstp\tx29, x30, [sp, #-48]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tstp\tx21, x22, [sp, #32]");
    emit("\tmov\tx19, x0");                    /* env */
    emit("\tmov\tx0, #32");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .KTSfail");
    emit("\tmov\tx20, x0");                    /* tcb */
    emit("\tstp\txzr, xzr, [x20]");
    emit("\tstp\txzr, xzr, [x20, #16]");
    emit("\tmov\tx0, #16");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .KTSfail");
    emit("\tmov\tx21, x0");                    /* ctx */
    emit("\tstr\tx19, [x21, #0]");
    emit("\tstr\tx20, [x21, #8]");
    /* id unik */
    emit("\tadrp\tx10, kop_thread_next");
    emit("\tadd\tx10, x10, :lo12:kop_thread_next");
    emit(".Ltidspawn:");
    emit("\tldaxr\tw11, [x10]");
    emit("\tadd\tw12, w11, #1");
    emit("\tstlxr\tw13, w12, [x10]");
    emit("\tcbnz\tw13, .Ltidspawn");
    emit("\tstr\tw11, [x20, #24]");
    /* pthread_create(&tcb->tid, NULL, trampoline, ctx) */
    emit("\tmov\tx0, x20");
    emit("\tmov\tx1, #0");
    emit("\tadrp\tx2, kop_thread_trampoline");
    emit("\tadd\tx2, x2, :lo12:kop_thread_trampoline");
    emit("\tmov\tx3, x21");
    emit("\tbl\tpthread_create");
    emit("\tcbnz\tw0, .KTSfail");
    emit("\tmov\tx0, x20");
    emit("\tldp\tx21, x22, [sp, #32]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #48");
    emit("\tret");
    emit(".KTSfail:");
    emit("\tmov\tx0, #0");
    emit("\tldp\tx21, x22, [sp, #32]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #48");
    emit("\tret");

    /* x0 = ctx {env, tcb} — badan thread: init arena, panggil closure */
    emit(".text");
    emit("kop_thread_trampoline:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");
    emit("\tldr\tx20, [x19, #8]");             /* tcb */
    emit("\tbl\tkop_arena_thread_init");
    emit("\tldr\tx9, [x19, #0]");              /* env closure */
    emit("\tldr\tw10, [x9, #16]");             /* nuser */
    emit("\tcbnz\tw10, .Klam");                /* badan harus tanpa parameter */
    emit("\tldr\tx11, [x9]");                  /* kode lambda */
    emit("\tmov\tx0, x9");                     /* arg0 = env */
    emit("\tblr\tx11");
    emit("\tstr\tw0, [x20, #16]");             /* ret */
    emit("\tmov\tw9, #1");
    emit("\tadd\tx10, x20, #8");
    emit("\tstlr\tw9, [x10]");                 /* done = 1 */
    emit("\tmov\tx0, #0");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_thread_join");
    emit("kop_thread_join:");
    emit("\tcbz\tx0, .KTJz");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #-16]!");
    emit("\tmov\tx19, x0");
    emit("\tldr\tx0, [x19, #0]");
    emit("\tmov\tx1, #0");
    emit("\tbl\tpthread_join");
    emit("\tldr\tw0, [x19, #16]");
    emit("\tldp\tx19, x20, [sp], #16");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");
    emit(".KTJz:");
    emit("\tmov\tw0, wzr");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_thread_alive");
    emit("kop_thread_alive:");
    emit("\tcbz\tx0, .KTAz");
    emit("\tadd\tx9, x0, #8");
    emit("\tldar\tw0, [x9]");
    emit("\teor\tw0, w0, #1");
    emit("\tret");
    emit(".KTAz:");
    emit("\tmov\tw0, wzr");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_thread_id");
    emit("kop_thread_id:");
    emit("\tcbz\tx0, .KTIz");
    emit("\tldr\tw0, [x0, #24]");
    emit("\tret");
    emit(".KTIz:");
    emit("\tmov\tw0, wzr");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_thread_self");
    emit("kop_thread_self:");
    tls_addr("x9");
    emit("\tldr\tw0, [x9, #40]");
    emit("\tret");

    /* ── v1.46: channel CSP + mutex (pthread) ─────────────────────
     * Chan (256 byte):
     *   +0   pthread_mutex_t (ruang 64)
     *   +64  pthread_cond_t  not_empty (ruang 64)
     *   +128 pthread_cond_t  not_full  (ruang 64)
     *   +192 buf ptr, +200 cap, +208 head, +216 tail,
     *   +224 count,  +232 closed
     * Mutex (64 byte): +0 pthread_mutex_t. */
    emit(".text");
    emit("\t.globl kop_chan_new");
    emit("kop_chan_new:");
    emit("\tstp\tx29, x30, [sp, #-48]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tstp\tx21, x22, [sp, #32]");
    emit("\tmov\tx19, x0");
    emit("\tcmp\tx19, #1");
    emit("\tb.ge\t.LCNcap");
    emit("\tmov\tx19, #1");
    emit(".LCNcap:");
    emit("\tmov\tx0, #256");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .LCNfail");
    emit("\tmov\tx20, x0");
    emit("\tmov\tx0, x20");
    emit("\tmov\tx1, #0");
    emit("\tbl\tpthread_mutex_init");
    emit("\tadd\tx0, x20, #64");
    emit("\tmov\tx1, #0");
    emit("\tbl\tpthread_cond_init");
    emit("\tadd\tx0, x20, #128");
    emit("\tmov\tx1, #0");
    emit("\tbl\tpthread_cond_init");
    emit("\tmov\tx0, x19");
    emit("\tlsl\tx0, x0, #3");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .LCNfail");
    emit("\tstr\tx0, [x20, #192]");
    emit("\tstr\tx19, [x20, #200]");
    emit("\tstr\txzr, [x20, #208]");
    emit("\tstr\txzr, [x20, #216]");
    emit("\tstr\txzr, [x20, #224]");
    emit("\tstr\txzr, [x20, #232]");
    emit("\tmov\tx0, x20");
    emit("\tb\t.LCNdone");
    emit(".LCNfail:");
    emit("\tmov\tx0, #0");
    emit(".LCNdone:");
    emit("\tldp\tx21, x22, [sp, #32]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #48");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_chan_send");
    emit("kop_chan_send:");
    emit("\tstp\tx29, x30, [sp, #-48]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tstp\tx21, x22, [sp, #32]");
    emit("\tmov\tx19, x0");
    emit("\tmov\tx20, x1");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_lock");
    emit(".LCSloop:");
    emit("\tldr\tx9, [x19, #232]");
    emit("\tcbnz\tx9, .LCSclosed");
    emit("\tldr\tx9, [x19, #224]");
    emit("\tldr\tx10, [x19, #200]");
    emit("\tcmp\tx9, x10");
    emit("\tb.lo\t.LCSstore");
    emit("\tadd\tx0, x19, #128");
    emit("\tmov\tx1, x19");
    emit("\tbl\tpthread_cond_wait");
    emit("\tb\t.LCSloop");
    emit(".LCSstore:");
    emit("\tldr\tx9, [x19, #216]");
    emit("\tldr\tx10, [x19, #192]");
    emit("\tstr\tx20, [x10, x9, lsl #3]");
    emit("\tadd\tx9, x9, #1");
    emit("\tldr\tx10, [x19, #200]");
    emit("\tcmp\tx9, x10");
    emit("\tb.lo\t.LCSnowrap");
    emit("\tmov\tx9, #0");
    emit(".LCSnowrap:");
    emit("\tstr\tx9, [x19, #216]");
    emit("\tldr\tx9, [x19, #224]");
    emit("\tadd\tx9, x9, #1");
    emit("\tstr\tx9, [x19, #224]");
    emit("\tadd\tx0, x19, #64");
    emit("\tbl\tpthread_cond_signal");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tw0, #0");
    emit("\tb\t.LCSdone");
    emit(".LCSclosed:");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tw0, #-1");
    emit(".LCSdone:");
    emit("\tldp\tx21, x22, [sp, #32]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #48");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_chan_recv");
    emit("kop_chan_recv:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_lock");
    emit(".LCRloop:");
    emit("\tldr\tx9, [x19, #224]");
    emit("\tcbnz\tx9, .LCRpop");
    emit("\tldr\tx9, [x19, #232]");
    emit("\tcbnz\tx9, .LCRzero");
    emit("\tadd\tx0, x19, #64");
    emit("\tmov\tx1, x19");
    emit("\tbl\tpthread_cond_wait");
    emit("\tb\t.LCRloop");
    emit(".LCRpop:");
    emit("\tldr\tx9, [x19, #208]");
    emit("\tldr\tx10, [x19, #192]");
    emit("\tldr\tx20, [x10, x9, lsl #3]");
    emit("\tadd\tx9, x9, #1");
    emit("\tldr\tx10, [x19, #200]");
    emit("\tcmp\tx9, x10");
    emit("\tb.lo\t.LCRnowrap");
    emit("\tmov\tx9, #0");
    emit(".LCRnowrap:");
    emit("\tstr\tx9, [x19, #208]");
    emit("\tldr\tx9, [x19, #224]");
    emit("\tsub\tx9, x9, #1");
    emit("\tstr\tx9, [x19, #224]");
    emit("\tadd\tx0, x19, #128");
    emit("\tbl\tpthread_cond_signal");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tx0, x20");
    emit("\tb\t.LCRdone");
    emit(".LCRzero:");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tx0, #0");
    emit(".LCRdone:");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_chan_recv_ok");
    emit("kop_chan_recv_ok:");
    emit("\tstp\tx29, x30, [sp, #-64]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tstp\tx21, x22, [sp, #32]");
    emit("\tmov\tx19, x0");                   /* handle */
    emit("\tmov\tx20, x1");                   /* int[] ok */
    emit("\tmov\tx21, #0");                   /* value */
    emit("\tmov\tx22, #0");                   /* got */
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_lock");
    emit(".LCROloop:");
    emit("\tldr\tx9, [x19, #224]");
    emit("\tcbnz\tx9, .LCROpop");
    emit("\tldr\tx9, [x19, #232]");
    emit("\tcbnz\tx9, .LCROzero");
    emit("\tadd\tx0, x19, #64");
    emit("\tmov\tx1, x19");
    emit("\tbl\tpthread_cond_wait");
    emit("\tb\t.LCROloop");
    emit(".LCROpop:");
    emit("\tldr\tx9, [x19, #208]");
    emit("\tldr\tx10, [x19, #192]");
    emit("\tldr\tx21, [x10, x9, lsl #3]");
    emit("\tmov\tx22, #1");
    emit("\tadd\tx9, x9, #1");
    emit("\tldr\tx10, [x19, #200]");
    emit("\tcmp\tx9, x10");
    emit("\tb.lo\t.LCROnowrap");
    emit("\tmov\tx9, #0");
    emit(".LCROnowrap:");
    emit("\tstr\tx9, [x19, #208]");
    emit("\tldr\tx9, [x19, #224]");
    emit("\tsub\tx9, x9, #1");
    emit("\tstr\tx9, [x19, #224]");
    emit("\tadd\tx0, x19, #128");
    emit("\tbl\tpthread_cond_signal");
    emit("\tb\t.LCROunlock");
    emit(".LCROzero:");
    emit("\tmov\tx21, #0");
    emit("\tmov\tx22, #0");
    emit(".LCROunlock:");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tcbz\tx20, .LCROdone");
    emit("\tstr\tx22, [x20, #8]");            /* ok[0] */
    emit(".LCROdone:");
    emit("\tmov\tx0, x21");
    emit("\tldp\tx21, x22, [sp, #32]");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #64");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_chan_close");
    emit("kop_chan_close:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_lock");
    emit("\tmov\tx9, #1");
    emit("\tstr\tx9, [x19, #232]");
    emit("\tadd\tx0, x19, #64");
    emit("\tbl\tpthread_cond_broadcast");
    emit("\tadd\tx0, x19, #128");
    emit("\tbl\tpthread_cond_broadcast");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tw0, #0");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_chan_len");
    emit("kop_chan_len:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_lock");
    emit("\tldr\tx20, [x19, #224]");
    emit("\tmov\tx0, x19");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tw0, w20");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_chan_closed");
    emit("kop_chan_closed:");
    emit("\tldr\tw0, [x0, #232]");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_mutex_new");
    emit("kop_mutex_new:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx0, #64");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .LMNfail");
    emit("\tmov\tx19, x0");
    emit("\tmov\tx0, x19");
    emit("\tmov\tx1, #0");
    emit("\tbl\tpthread_mutex_init");
    emit("\tmov\tx0, x19");
    emit("\tb\t.LMNdone");
    emit(".LMNfail:");
    emit("\tmov\tx0, #0");
    emit(".LMNdone:");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_mutex_lock");
    emit("kop_mutex_lock:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tbl\tpthread_mutex_lock");
    emit("\tmov\tw0, #0");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_mutex_unlock");
    emit("kop_mutex_unlock:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tbl\tpthread_mutex_unlock");
    emit("\tmov\tw0, #0");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    emit(".text");
    emit("\t.globl kop_mutex_trylock");
    emit("kop_mutex_trylock:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tbl\tpthread_mutex_trylock");
    emit("\tcmp\tw0, #0");
    emit("\tcset\tw0, eq");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0 = char* C → x0 = String packed (v1.18: 8 byte per slot) */
    emit(".text");
    emit("\t.globl kop_str_from_cstr");
    emit("kop_str_from_cstr:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tmov\tw2, #0");
    emit(".LSCa:");
    emit("\tldrb\tw1, [x9, x2]");
    emit("\tcbz\tw1, .LSCb");
    emit("\tadd\tw2, w2, #1");
    emit("\tb\t.LSCa");
    emit(".LSCb:");
    emit("\tstp\tx9, x2, [sp, #-16]!");
    emit("\tsxtw\tx10, w2");                       /* n */
    emit("\tadd\tx11, x10, #7");
    emit("\tlsr\tx11, x11, #3");                   /* slots */
    emit("\tlsl\tx0, x11, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tldp\tx9, x2, [sp], #16");
    emit("\tsxtw\tx10, w2");                       /* n */
    emit("\tadd\tx11, x10, #7");
    emit("\tlsr\tx11, x11, #3");                   /* slots (recompute) */
    emit("\tmov\tx12, #0");
    emit("\tadd\tx13, x0, #8");                    /* basis slot data */
    emit(".LSCz:");
    emit("\tcmp\tx12, x11");
    emit("\tb.hs\t.LSCw");
    emit("\tstr\txzr, [x13, x12, lsl #3]");
    emit("\tadd\tx12, x12, #1");
    emit("\tb\t.LSCz");
    emit(".LSCw:");
    emit("\tstr\tx10, [x0]");                      /* len = n */
    emit("\tmov\tx12, #0");
    emit(".LSCc:");
    emit("\tcmp\tx12, x10");
    emit("\tb.hs\t.LSCd");
    emit("\tldrb\tw1, [x9, x12]");
    emit("\tand\tx13, x12, #7");
    emit("\tlsl\tx13, x13, #3");                   /* shift */
    emit("\tlsr\tx14, x12, #3");
    emit("\tlsl\tx14, x14, #3");
    emit("\tadd\tx14, x14, #8");                   /* slot offset */
    emit("\tldr\tx15, [x0, x14]");
    emit("\tlsl\tx16, x1, x13");
    emit("\torr\tx15, x15, x16");
    emit("\tstr\tx15, [x0, x14]");
    emit("\tadd\tx12, x12, #1");
    emit("\tb\t.LSCc");
    emit(".LSCd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* ── v1.18: runtime String packed ───────────────────────────── */

    /* x0 = int[] byte-per-slot → x0 = String packed (byte 0..255) */
    emit(".text");
    emit("\t.globl kop_mkstr");
    emit("kop_mkstr:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tldr\tx10, [x9]");                      /* n */
    emit("\tadd\tx11, x10, #7");
    emit("\tlsr\tx11, x11, #3");                   /* slots */
    emit("\tlsl\tx0, x11, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tmov\tx12, #0");
    emit("\tadd\tx14, x0, #8");                    /* basis slot data */
    emit(".LMKz:");
    emit("\tcmp\tx12, x11");
    emit("\tb.hs\t.LMKw");
    emit("\tstr\txzr, [x14, x12, lsl #3]");
    emit("\tadd\tx12, x12, #1");
    emit("\tb\t.LMKz");
    emit(".LMKw:");
    emit("\tstr\tx10, [x0]");
    emit("\tmov\tx13, #0");
    emit(".LMKl:");
    emit("\tcmp\tx13, x10");
    emit("\tb.hs\t.LMKd");
    emit("\tadd\tx14, x9, #8");
    emit("\tldr\tx14, [x14, x13, lsl #3]");        /* byte-per-slot src */
    emit("\tcmp\tx14, #255");
    emit("\tb.hi\t.Kbnd");
    emit("\tand\tx15, x13, #7");
    emit("\tlsl\tx15, x15, #3");                   /* shift */
    emit("\tlsl\tx14, x14, x15");
    emit("\tlsr\tx16, x13, #3");
    emit("\tlsl\tx16, x16, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tldr\tx17, [x0, x16]");
    emit("\torr\tx17, x17, x14");
    emit("\tstr\tx17, [x0, x16]");
    emit("\tadd\tx13, x13, #1");
    emit("\tb\t.LMKl");
    emit(".LMKd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0 = String packed → x0 = int[] byte-per-slot */
    emit(".text");
    emit("\t.globl kop_strbytes");
    emit("kop_strbytes:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tldr\tx10, [x9]");                      /* n */
    emit("\tlsl\tx0, x10, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tstr\tx10, [x0]");
    emit("\tmov\tx11, #0");
    emit(".LSB1:");
    emit("\tcmp\tx11, x10");
    emit("\tb.hs\t.LSB2");
    emit("\tlsr\tx13, x11, #3");
    emit("\tlsl\tx13, x13, #3");
    emit("\tadd\tx13, x13, #8");
    emit("\tldr\tx14, [x9, x13]");                 /* slot packed */
    emit("\tand\tx15, x11, #7");
    emit("\tlsl\tx15, x15, #3");                   /* shift */
    emit("\tlsr\tx14, x14, x15");
    emit("\tand\tx14, x14, #0xff");
    emit("\tlsl\tx16, x11, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tstr\tx14, [x0, x16]");
    emit("\tadd\tx11, x11, #1");
    emit("\tb\t.LSB1");
    emit(".LSB2:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0, x1 = String packed → x0 = concat packed */
    emit(".text");
    emit("\t.globl kop_cat_packed");
    emit("kop_cat_packed:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tmov\tx9, x0");
    emit("\tmov\tx10, x1");
    emit("\tldr\tx11, [x9]");                      /* la */
    emit("\tldr\tx12, [x10]");                     /* lb */
    emit("\tadd\tx13, x11, x12");                  /* total */
    emit("\tadd\tx14, x13, #7");
    emit("\tlsr\tx14, x14, #3");                   /* slots */
    emit("\tstp\tx9, x10, [sp, #-16]!");
    emit("\tstp\tx11, x12, [sp, #-16]!");
    emit("\tlsl\tx0, x14, #3");
    emit("\tadd\tx0, x0, #8");
    emit("\tbl\tkopalloc");
    emit("\tldp\tx11, x12, [sp], #16");
    emit("\tldp\tx9, x10, [sp], #16");
    emit("\tmov\tx15, #0");
    emit("\tadd\tx1, x0, #8");                     /* basis slot data */
    emit(".LCz:");
    emit("\tcmp\tx15, x14");
    emit("\tb.hs\t.LCw");
    emit("\tstr\txzr, [x1, x15, lsl #3]");
    emit("\tadd\tx15, x15, #1");
    emit("\tb\t.LCz");
    emit(".LCw:");
    emit("\tstr\tx13, [x0]");                      /* len = total */
    emit("\tmov\tx15, #0");                        /* i untuk a */
    emit(".LCa:");
    emit("\tcmp\tx15, x11");
    emit("\tb.hs\t.LCb");
    emit("\tlsr\tx16, x15, #3");
    emit("\tlsl\tx16, x16, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tldr\tx17, [x9, x16]");
    emit("\tand\tx1, x15, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsr\tx17, x17, x1");
    emit("\tand\tx17, x17, #0xff");                /* getb(a, i) */
    emit("\tlsr\tx2, x15, #3");
    emit("\tlsl\tx2, x2, #3");
    emit("\tadd\tx2, x2, #8");
    emit("\tldr\tx3, [x0, x2]");
    emit("\tand\tx1, x15, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsl\tx17, x17, x1");
    emit("\torr\tx3, x3, x17");
    emit("\tstr\tx3, [x0, x2]");
    emit("\tadd\tx15, x15, #1");
    emit("\tb\t.LCa");
    emit(".LCb:");
    emit("\tmov\tx15, #0");                        /* j untuk b */
    emit(".LCc:");
    emit("\tcmp\tx15, x12");
    emit("\tb.hs\t.LCd");
    emit("\tlsr\tx16, x15, #3");
    emit("\tlsl\tx16, x16, #3");
    emit("\tadd\tx16, x16, #8");
    emit("\tldr\tx17, [x10, x16]");
    emit("\tand\tx1, x15, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsr\tx17, x17, x1");
    emit("\tand\tx17, x17, #0xff");                /* getb(b, j) */
    emit("\tadd\tx5, x15, x11");                   /* pos = la + j */
    emit("\tlsr\tx2, x5, #3");
    emit("\tlsl\tx2, x2, #3");
    emit("\tadd\tx2, x2, #8");
    emit("\tldr\tx3, [x0, x2]");
    emit("\tand\tx1, x5, #7");
    emit("\tlsl\tx1, x1, #3");
    emit("\tlsl\tx17, x17, x1");
    emit("\torr\tx3, x3, x17");
    emit("\tstr\tx3, [x0, x2]");
    emit("\tadd\tx15, x15, #1");
    emit("\tb\t.LCc");
    emit(".LCd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");

    /* x0, x1 = String packed → w0 = isi sama (1/0) */
    emit(".text");
    emit("\t.globl kop_sequal_packed");
    emit("kop_sequal_packed:");
    emit("\tstp\tx29, x30, [sp, #-16]!");
    emit("\tmov\tx29, sp");
    emit("\tldr\tx9, [x0]");                       /* la */
    emit("\tldr\tx10, [x1]");                      /* lb */
    emit("\tcmp\tx9, x10");
    emit("\tb.ne\t.LSQn");
    emit("\tmov\tx11, #0");
    emit(".LSQ1:");
    emit("\tcmp\tx11, x9");
    emit("\tb.hs\t.LSQy");
    emit("\tlsr\tx12, x11, #3");
    emit("\tlsl\tx12, x12, #3");
    emit("\tadd\tx12, x12, #8");
    emit("\tldr\tx13, [x0, x12]");
    emit("\tand\tx14, x11, #7");
    emit("\tlsl\tx14, x14, #3");
    emit("\tlsr\tx13, x13, x14");
    emit("\tand\tx13, x13, #0xff");
    emit("\tldr\tx15, [x1, x12]");
    emit("\tlsr\tx15, x15, x14");
    emit("\tand\tx15, x15, #0xff");
    emit("\tcmp\tx13, x15");
    emit("\tb.ne\t.LSQn");
    emit("\tadd\tx11, x11, #1");
    emit("\tb\t.LSQ1");
    emit(".LSQy:");
    emit("\tmov\tw0, #1");
    emit("\tb\t.LSQd");
    emit(".LSQn:");
    emit("\tmov\tw0, #0");
    emit(".LSQd:");
    emit("\tldp\tx29, x30, [sp], #16");
    emit("\tret");


    emit("\t.section .tbss,\"awT\",@nobits");
    emit("\t.balign 64");
    emit("kop_tls: .zero 64");              /* base,cap,used,mmapped,auto,id,?,[56]=handler */
    emit("\t.section .data");
    emit("\t.align 3");
    emit("kop_thread_next: .quad 2");       /* id thread berikut (1 = utama) */
    emit("kop_argc: .quad 0");
    emit("kop_argv: .quad 0");
    emit(".comm kop_iobuf, 4096, 32");      /* penyangga syscall rekursi builtin */

    emit("\t.section .rodata");
    emit(".Lfmt_i: .asciz \"%s\"", "%d");
    emit(".Lfmt_in: .asciz \"%s\"", "%d\\n");
    emit(".Lfmt_s: .asciz \"%s\"", "%s");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
    emit(".Lmsg_exc: .asciz \"%s\"", "kopi: exception tidak tertangkap (throw tanpa try/catch)\\n");
    emit(".Lmsg_arr: .asciz \"%s\"", "kopi: array tidak valid (indeks/ukuran salah) — periksa indeks 0..panjang-1\\n");
    emit(".Lmsg_bnd: .asciz \"%s\"", "kopi: byte mkstr di luar 0..255\\n");
    emit(".Lmsg_oom: .asciz \"%s\"", "kopi: arena malloc habis — batas %ld MB terlampaui (naikkan malloc<...> atau pakai malloc{} otomatis)\\n");
    emit(".Lmsg_oom_kb: .asciz \"%s\"", "kopi: arena malloc habis — batas %ld KB terlampaui (naikkan malloc<...> atau pakai malloc{} otomatis)\\n");
    emit(".Lmsg_oom_auto: .asciz \"%s\"", "kopi: arena malloc habis — batas otomatis 4096 MB terlampaui (pakai malloc<N> untuk batas lain)\\n");
    emit(".Lmsg_oom_auto_fail: .asciz \"%s\"", "kopi: gagal menyediakan arena malloc otomatis 4096 MB (coba malloc<N> lebih kecil)\\n");
    emit(".Lempty_str: .asciz \"%s\"", "");
    emit(".Lstty_raw: .asciz \"%s\"", "stty raw -echo 2>/dev/null");
    emit(".Lstty_sane: .asciz \"%s\"", "stty sane 2>/dev/null");
    emit(".Lfmt_sn: .asciz \"%s\"", "%s\\n");
    emit(".Lfmt_ld: .asciz \"%s\"", "%ld");
    emit(".Lfmt_ldn: .asciz \"%s\"", "%ld\\n");
    emit(".Lfmt_lu: .asciz \"%s\"", "%lu");
    emit(".Lfmt_lun: .asciz \"%s\"", "%lu\\n");
    emit(".Lfmt_u: .asciz \"%s\"", "%u");
    emit(".Lfmt_un: .asciz \"%s\"", "%u\\n");
    emit(".Lfmt_g: .asciz \"%s\"", "%.15g");
    emit(".Lfmt_gn: .asciz \"%s\"", "%.15g\\n");
    emit(".Lfmt_gf: .asciz \"%s\"", "%.9g");
    emit(".Lfmt_gfn: .asciz \"%s\"", "%.9g\\n");
    for (int i = 0; i < prog.ncp; i++) {
        size_t eln = strlen(prog.cpool[i]) * 2 + 8;
        char *esc = malloc(eln);
        if (!esc) die("memori host habis");
        asm_escape(prog.cpool[i], esc, eln);
        emit("cpool%d:", i);
        emit("\t.asciz \"%s\"", esc);
        free(esc);
    }
    fclose(out);
}
