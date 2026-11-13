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
    emit("\tadrp\tx9, kop_arena_auto");
    emit("\tadd\tx9, x9, :lo12:kop_arena_auto");
    emit("\tldr\tx9, [x9]");
    emit("\tcbnz\tx9, .KoomAuto");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tldr\tx1, [x9]");
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
    emit("\tadrp\tx1, kop_arena_used");
    emit("\tadd\tx1, x1, :lo12:kop_arena_used");
    emit("\tldr\tx2, [x1]");
    emit("\tadd\tx2, x2, x0");
    emit("\tadrp\tx3, kop_arena_cap");
    emit("\tadd\tx3, x3, :lo12:kop_arena_cap");
    emit("\tldr\tx3, [x3]");
    emit("\tcmp\tx2, x3");
    emit("\tb.ge\t.Koom");
    emit("\tstr\tx2, [x1]");
    emit("\tsub\tx2, x2, x0");
    emit("\tadrp\tx3, kop_arena_base");
    emit("\tadd\tx3, x3, :lo12:kop_arena_base");
    emit("\tldr\tx3, [x3]");
    emit("\tadd\tx0, x3, x2");
    emit("\tret");

    /* v1.22: kop_arena_enter(x0 = byte) -> x1 = handle (40 byte).
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
    /* simpan konteks arena luar + flag */
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #0]");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #8]");
    emit("\tadrp\tx9, kop_arena_used");
    emit("\tadd\tx9, x9, :lo12:kop_arena_used");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #16]");
    emit("\tadrp\tx9, kop_arena_mmapped");
    emit("\tadd\tx9, x9, :lo12:kop_arena_mmapped");
    emit("\tldr\tx10, [x9]");
    emit("\tstr\tx10, [x20, #24]");
    emit("\tadrp\tx9, kop_arena_auto");
    emit("\tadd\tx9, x9, :lo12:kop_arena_auto");
    emit("\tldr\tx10, [x9]");
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
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tstr\tx0, [x9]");
    emit("\tmov\tx1, #1");
    emit("\tlsl\tx1, x1, #32");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tstr\tx1, [x9]");
    emit("\tadrp\tx9, kop_arena_used");
    emit("\tadd\tx9, x9, :lo12:kop_arena_used");
    emit("\tstr\txzr, [x9]");
    emit("\tmov\tx1, #1");
    emit("\tadrp\tx9, kop_arena_mmapped");
    emit("\tadd\tx9, x9, :lo12:kop_arena_mmapped");
    emit("\tstr\tx1, [x9]");
    emit("\tadrp\tx9, kop_arena_auto");
    emit("\tadd\tx9, x9, :lo12:kop_arena_auto");
    emit("\tstr\tx1, [x9]");
    emit("\tb\t.KenterDone");
    emit(".KenterExplicit:");
    emit("\tmov\tx0, x19");
    emit("\tbl\tmalloc");
    emit("\tcbz\tx0, .KenterOom");
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tstr\tx0, [x9]");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tstr\tx19, [x9]");
    emit("\tadrp\tx9, kop_arena_used");
    emit("\tadd\tx9, x9, :lo12:kop_arena_used");
    emit("\tstr\txzr, [x9]");
    emit("\tadrp\tx9, kop_arena_mmapped");
    emit("\tadd\tx9, x9, :lo12:kop_arena_mmapped");
    emit("\tstr\txzr, [x9]");
    emit("\tadrp\tx9, kop_arena_auto");
    emit("\tadd\tx9, x9, :lo12:kop_arena_auto");
    emit("\tstr\txzr, [x9]");
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

    /* v1.22: kop_arena_leave(x1 = handle) — bebas arena blok (munmap bila
       mmap), pulihkan arena luar + flag; x0 (nilai balik) dipertahankan. */
    emit(".text");
    emit("\t.globl kop_arena_leave");
    emit("kop_arena_leave:");
    emit("\tstp\tx29, x30, [sp, #-32]!");
    emit("\tmov\tx29, sp");
    emit("\tstp\tx19, x20, [sp, #16]");
    emit("\tmov\tx19, x0");                    /* simpan nilai balik */
    emit("\tmov\tx20, x1");                    /* handle */
    emit("\tadrp\tx9, kop_arena_mmapped");     /* arena aktif hasil mmap? */
    emit("\tadd\tx9, x9, :lo12:kop_arena_mmapped");
    emit("\tldr\tx9, [x9]");
    emit("\tcbz\tx9, .KleaveFree");
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tldr\tx0, [x9]");
    emit("\tadrp\tx9, kop_arena_cap");
    emit("\tadd\tx9, x9, :lo12:kop_arena_cap");
    emit("\tldr\tx1, [x9]");
    emit("\tbl\tmunmap");
    emit("\tb\t.KleaveRestore");
    emit(".KleaveFree:");
    emit("\tadrp\tx9, kop_arena_base");
    emit("\tadd\tx9, x9, :lo12:kop_arena_base");
    emit("\tldr\tx0, [x9]");
    emit("\tbl\tfree");
    emit(".KleaveRestore:");
    emit("\tldr\tx9, [x20, #0]");
    emit("\tadrp\tx10, kop_arena_base");
    emit("\tadd\tx10, x10, :lo12:kop_arena_base");
    emit("\tstr\tx9, [x10]");
    emit("\tldr\tx9, [x20, #8]");
    emit("\tadrp\tx10, kop_arena_cap");
    emit("\tadd\tx10, x10, :lo12:kop_arena_cap");
    emit("\tstr\tx9, [x10]");
    emit("\tldr\tx9, [x20, #16]");
    emit("\tadrp\tx10, kop_arena_used");
    emit("\tadd\tx10, x10, :lo12:kop_arena_used");
    emit("\tstr\tx9, [x10]");
    emit("\tldr\tx9, [x20, #24]");
    emit("\tadrp\tx10, kop_arena_mmapped");
    emit("\tadd\tx10, x10, :lo12:kop_arena_mmapped");
    emit("\tstr\tx9, [x10]");
    emit("\tldr\tx9, [x20, #32]");
    emit("\tadrp\tx10, kop_arena_auto");
    emit("\tadd\tx10, x10, :lo12:kop_arena_auto");
    emit("\tstr\tx9, [x10]");
    emit("\tmov\tx0, x20");
    emit("\tbl\tfree");
    emit("\tmov\tx0, x19");
    emit("\tldp\tx19, x20, [sp, #16]");
    emit("\tldp\tx29, x30, [sp], #32");
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


    emit("\t.section .data");
    emit("\t.align 3");
    emit("kop_arena_base: .quad 0");
    emit("kop_arena_cap: .quad 0");
    emit("kop_arena_used: .quad 0");
    emit("kop_arena_mmapped: .quad 0");
    emit("kop_arena_auto: .quad 0");
    emit("kop_argc: .quad 0");
    emit("kop_argv: .quad 0");
    emit(".comm kop_iobuf, 4096, 32");      /* penyangga syscall rekursi builtin */

    emit("\t.section .rodata");
    emit(".Lfmt_i: .asciz \"%s\"", "%d");
    emit(".Lfmt_in: .asciz \"%s\"", "%d\\n");
    emit(".Lfmt_s: .asciz \"%s\"", "%s");
    emit(".Lmsg_lam: .asciz \"%s\"", "kopi: jumlah argumen lambda tidak cocok\\n");
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
