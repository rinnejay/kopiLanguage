/* vm/jit_a64.inc — JIT minimal loop angka untuk aarch64 (v1.46).
 *
 * Ide: loop yang badannya hanya berisi operasi lokal int32
 * (OP_MULADD, OP_ADDSHR, OP_OP_STORE + OP_ILOAD/OP_IDXR_UB untuk
 * `acc = acc + buf[i]`, plus direktif OP_LINE/OP_SRCFILE yang
 * diabaikan) diterjemahkan sekali menjadi kode mesin, lalu SELURUH
 * sisa iterasi dijalankan native. Interpreter tetap menjadi fallback
 * untuk pola lain, jadi semantik tidak berubah.
 *
 * Kode yang dihasilkan untuk tiap loop (sekali per thread, di-cache):
 *
 *   prolog (simpan x19..x28, x19 = &locals[0])
 *   muat slot int ke w20..w27, slot pointer ke x20..x27
 *   kaki-loop awal: slot += delta; cmp; b.<lawan> keluar
 *   badan: muladd / add-lsr / baca byte buffer
 *   kaki-loop: slot += delta; cmp; b.<kondisi> badan
 *   keluar: mov w0,#0 (selesai) atau w0,#1 (bail), simpan slot, ret
 *
 * Bila badan membaca array di luar batas, JIT bail: slot disimpan,
 * x0=1, lalu interpreter mengulang badan pada indeks itu sehingga
 * pesan error aslinya tetap muncul. Setelah fungsi kembali normal,
 * interpreter melanjutkan tepat sesudah OP_LOOP/OP_LOOP_R dengan
 * kondisi loop sudah false. */

#if defined(__aarch64__)
#define JIT_ON 1
#else
#define JIT_ON 0
#endif

#define JIT_THRESHOLD 4
#define MAXJIT 128
#define JSTK 32

typedef int64_t (*JitFn)(int64_t *);

typedef struct {
    int meth, pc;
    int target;                 /* awal badan loop (untuk bail) */
    int state;                  /* 0 belum dicoba, 1 siap, 2 gagal */
    int hot;
    void *fn;
} JitEnt;

static _Thread_local JitEnt jitents[MAXJIT];
static _Thread_local int jit_ready;
static int jit_on = JIT_ON;

#if JIT_ON

static void jit_reset(void) {
    for (int i = 0; i < MAXJIT; i++) {
        jitents[i].pc = -1;
        jitents[i].meth = -1;
        jitents[i].target = 0;
        jitents[i].state = 0;
        jitents[i].hot = 0;
        jitents[i].fn = 0;
    }
    jit_ready = 1;
}

static JitEnt *jit_find(int meth, int pc) {
    if (!jit_ready) jit_reset();
    unsigned h = (((unsigned)meth * 2654435761u) ^ (unsigned)pc) & (MAXJIT - 1);
    for (int i = 0; i < MAXJIT; i++) {
        JitEnt *e = &jitents[(h + i) & (MAXJIT - 1)];
        if (e->pc < 0) { e->pc = pc; e->meth = meth; return e; }
        if (e->pc == pc && e->meth == meth) return e;
    }
    return 0;
}

/* ── encoder instruksi aarch64 ──────────────────────────────────── */
typedef struct { uint32_t *p; int n; } JB;

static void jb(JB *b, uint32_t ins) { b->p[b->n++] = ins; }

static uint32_t e_stp_pre(int rt, int rt2, int rn, int imm7) {
    return 0xA9800000u | ((uint32_t)(imm7 & 0x7F) << 15) |
           ((uint32_t)rt2 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}
static uint32_t e_ldp_post(int rt, int rt2, int rn, int imm7) {
    return 0xA8C00000u | ((uint32_t)(imm7 & 0x7F) << 15) |
           ((uint32_t)rt2 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}
static uint32_t e_ldr_w(int rt, int rn, int off) {
    return 0xB9400000u | ((uint32_t)(off / 4) << 10) |
           ((uint32_t)rn << 5) | (uint32_t)rt;
}
static uint32_t e_ldr_x(int rt, int rn, int off) {
    return 0xF9400000u | ((uint32_t)(off / 8) << 10) |
           ((uint32_t)rn << 5) | (uint32_t)rt;
}
static uint32_t e_str_x(int rt, int rn, int off) {
    return 0xF9000000u | ((uint32_t)(off / 8) << 10) |
           ((uint32_t)rn << 5) | (uint32_t)rt;
}
static uint32_t e_sxtw(int rd, int rn) {
    return 0x93407C00u | ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_movz(int rd, uint32_t imm16) {
    return 0x52800000u | ((imm16 & 0xFFFF) << 5) | (uint32_t)rd;
}
static uint32_t e_movk16(int rd, uint32_t imm16) {
    return 0x72A00000u | ((imm16 & 0xFFFF) << 5) | (uint32_t)rd;  /* LSL #16 */
}
static uint32_t e_madd(int rd, int rn, int rm, int ra) {
    return 0x1B000000u | ((uint32_t)rm << 16) | ((uint32_t)ra << 10) |
           ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_mul(int rd, int rn, int rm) {
    return e_madd(rd, rn, rm, 31);          /* mul w, w, w */
}
static uint32_t e_add(int rd, int rn, int rm) {
    return 0x0B000000u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_sub(int rd, int rn, int rm) {
    return 0x4B000000u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_and(int rd, int rn, int rm) {
    return 0x0A000000u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_orr(int rd, int rn, int rm) {
    return 0x2A000000u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_eor(int rd, int rn, int rm) {
    return 0x4A000000u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_mov_w(int rd, int rm) {
    return e_orr(rd, 31, rm);               /* mov w, w  == orr w, wzr, w */
}
static uint32_t e_add_lsr(int rd, int rn, int rm, int sh) {
    return 0x0B000000u | ((uint32_t)rm << 16) | (1u << 22) |
           ((uint32_t)sh << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}
static uint32_t e_cmp(int rn, int rm) {
    return 0x6B00001Fu | ((uint32_t)rm << 16) | ((uint32_t)rn << 5);
}
static uint32_t e_cmp_imm(int rn, int imm12) {
    return 0x7100001Fu | ((uint32_t)(imm12 & 0xFFF) << 10) | ((uint32_t)rn << 5);
}
static uint32_t e_bcond(int cond) { return 0x54000000u | (uint32_t)(cond & 0xF); }
static uint32_t e_b(void) { return 0x14000000u; }
static uint32_t e_cbz_x(int rt) { return 0xB4000000u | (uint32_t)rt; }

/* LDRB/LDRSB (register offset, UXTW #0): [Xn, Wm, uxtw] */
static uint32_t e_ldrb_uxtw(int rt, int rn, int rm) {
    return 0x38604800u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rt;
}
static uint32_t e_ldrsb_uxtw(int rt, int rn, int rm) {
    return 0x38A04800u | ((uint32_t)rm << 16) |
           ((uint32_t)rn << 5) | (uint32_t)rt;
}
/* add Xd, Xn, #8 */
static uint32_t e_add_imm8(int rd, int rn) {
    return 0x91002000u | ((uint32_t)rn << 5) | (uint32_t)rd;
}

/* kode kondisi ARM64: EQ=0 NE=1 GE=10 LT=11 GT=12 LE=13 */
static int arm_cond(int cm) {
    static const int cont[6] = { 11, 13, 12, 10, 0, 1 };  /* <,<=,>,>=,==,!= */
    static const int inv[6]  = { 10, 12, 13, 11, 1, 0 };
    return cm < 0 || cm > 5 ? 0 : (cont[cm] | (inv[cm] << 4));
}

/* materialisasi konstanta 32-bit ke w<reg> */
static void emit_const(JB *b, int reg, uint32_t v) {
    jb(b, e_movz(reg, v & 0xFFFF));
    if (v >> 16) jb(b, e_movk16(reg, v >> 16));
}

/* kaki loop: slot += delta; cmp slot, batas; b.cond → (dipatch pemanggil) */
static int emit_footer(JB *b, int sreg, int breg, int32_t imm,
                       int32_t delta, int cm, int use_inv) {
    if (delta != 0) {
        emit_const(b, 9, (uint32_t)delta);
        jb(b, e_add(sreg, sreg, 9));
    }
    if (breg >= 0) {
        jb(b, e_cmp(sreg, breg));
    } else if (imm >= 0 && imm <= 4095) {
        jb(b, e_cmp_imm(sreg, (int)imm));
    } else {
        emit_const(b, 10, (uint32_t)imm);
        jb(b, e_cmp(sreg, 10));
    }
    int cond = arm_cond(cm);
    int at = b->n;
    jb(b, e_bcond(use_inv ? (cond >> 4) : (cond & 0xF)));
    return at;
}

static void patch_bcond(uint32_t *buf, int at, int to) {
    int off = to - at;
    buf[at] |= ((uint32_t)off & 0x7FFFF) << 5;
}
/* cbz dan b.cond sama-sama menyimpan imm19 di bit 5..23 */
static void patch_cbz(uint32_t *buf, int at, int to) {
    int off = to - at;
    buf[at] |= ((uint32_t)off & 0x7FFFF) << 5;
}
static void patch_b(uint32_t *buf, int at, int to) {
    int off = to - at;
    buf[at] |= ((uint32_t)off & 0x3FFFFFF);
}

/* terjemahkan loop di sekitar fpc (opcode OP_LOOP/OP_LOOP_R).
 * target badan disimpan di *out_target. */
static void *jit_compile(Meth *m, int fpc, int *out_target) {
    const uint8_t *code = m->code;
    int is_r = code[fpc] == OP_LOOP_R;
    if (code[fpc] != OP_LOOP && !is_r) return 0;

    const uint8_t *c = code + fpc + 1;
    int type = c[0], cm = c[1];
    if (type != 0 || cm < 0 || cm > 5) return 0;
    int slot = c[2] | (c[3] << 8);
    int32_t delta = (int32_t)rd32c(c + 4);
    int32_t imm = 0;
    int bound = -1;
    uint32_t target;
    if (is_r) {
        bound = c[8] | (c[9] << 8);
        target = rd32c(c + 10);
    } else {
        imm = (int32_t)rd32c(c + 8);
        target = rd32c(c + 12);
    }
    if ((long)target >= fpc || (long)target < 0) return 0;
    *out_target = (int)target;

    /* ── pass 1: klasifikasi slot (int vs pointer) + validasi pola ── */
    int ptr[MAXLOC], written[MAXLOC], used[MAXLOC];
    memset(ptr, 0, sizeof ptr);
    memset(written, 0, sizeof written);
    memset(used, 0, sizeof used);
    int st[JSTK], sn = 0;
    int bases[256], idxs[256], nidx = 0;

    int p = (int)target;
    while (p < fpc) {
        uint8_t op = code[p];
        if (op == OP_LINE || op == OP_SRCFILE) {
            p += 3;
        } else if (op == OP_MULADD) {
            int sl = code[p + 1] | (code[p + 2] << 8);
            if (sl >= MAXLOC) return 0;
            used[sl] = 1; written[sl] = 1;
            p += 11;
        } else if (op == OP_ADDSHR) {
            int sx = code[p + 1] | (code[p + 2] << 8);
            int sy = code[p + 3] | (code[p + 4] << 8);
            if (sx >= MAXLOC || sy >= MAXLOC) return 0;
            used[sx] = 1; used[sy] = 1; written[sx] = 1;
            p += 6;
        } else if (op == OP_ILOAD) {
            int sl = code[p + 1] | (code[p + 2] << 8);
            if (sl >= MAXLOC || sn >= JSTK) return 0;
            used[sl] = 1;
            st[sn++] = sl;
            p += 3;
        } else if (op == OP_IDXR_UB || op == OP_IDXR_B) {
            if (sn < 2) return 0;
            int ix = st[--sn];
            int bs = st[--sn];
            if (bs < 0 || ix < 0) return 0;
            if (nidx >= 256) return 0;
            bases[nidx] = bs; idxs[nidx] = ix; nidx++;
            ptr[bs] = 1;
            st[sn++] = -1;              /* hasil = temporer */
            p += 1;
        } else if (op == OP_OP_STORE) {
            int ot = code[p + 1], bo = code[p + 2];
            int sl = code[p + 3] | (code[p + 4] << 8);
            if (ot != 0 || bo > 5 || sl >= MAXLOC || sn < 1) return 0;
            sn--;
            used[sl] = 1; written[sl] = 1;
            p += 5;
        } else {
            return 0;                   /* opcode lain → jangan JIT */
        }
        if (p > fpc) return 0;
    }
    if (p != fpc || sn != 0) return 0;

    /* validasi silang: basis harus pointer, indeks/target tulis bukan pointer */
    for (int i = 0; i < nidx; i++) {
        if (!ptr[bases[i]] || ptr[idxs[i]]) return 0;
    }
    for (int i = 0; i < MAXLOC; i++)
        if (ptr[i] && written[i]) return 0;

    int sreg, breg = -1;
    if (slot >= MAXLOC) return 0;
    if (is_r && bound >= MAXLOC) return 0;
    if (is_r && ptr[bound]) return 0;
    if (is_r && written[bound] && bound != slot) return 0;  /* batas diubah badan */
    used[slot] = 1;
    if (is_r) used[bound] = 1;

    /* ── peta slot → w20..w27 (maks 8 slot) ───────────────────── */
    int smap[MAXLOC], regs[8], ns = 0;
    for (int i = 0; i < MAXLOC; i++) smap[i] = -1;
    for (int i = 0; i < MAXLOC; i++) {
        if (!used[i]) continue;
        if (ns >= 8) return 0;
        smap[i] = 20 + ns; regs[ns++] = i;
    }
    sreg = smap[slot];
    if (is_r) breg = smap[bound];

    /* ── hasilkan kode ────────────────────────────────────────── */
    long pg = sysconf(_SC_PAGESIZE);
    if (pg <= 0) pg = 4096;
    uint32_t *buf = mmap(0, (size_t)pg, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) return 0;

    JB b = { buf, 0 };
    jb(&b, e_stp_pre(29, 30, 31, -2));      /* stp x29,x30,[sp,#-16]! */
    jb(&b, 0x910003FD);                     /* mov x29, sp */
    jb(&b, e_stp_pre(19, 20, 31, -2));
    jb(&b, e_stp_pre(21, 22, 31, -2));
    jb(&b, e_stp_pre(23, 24, 31, -2));
    jb(&b, e_stp_pre(25, 26, 31, -2));
    jb(&b, e_stp_pre(27, 28, 31, -2));
    jb(&b, 0xAA0003F3);                     /* mov x19, x0 */
    for (int i = 0; i < ns; i++) {
        int sl = regs[i];
        if (ptr[sl]) jb(&b, e_ldr_x(20 + i, 19, sl * 8));
        else         jb(&b, e_ldr_w(20 + i, 19, sl * 8));
    }

    int pre = emit_footer(&b, sreg, breg, imm, delta, cm, 1);
    int body_start = b.n;
    int bail[256], nbail = 0;

    sn = 0;                             /* tumpukan simulasi untuk codegen */
    p = (int)target;
    while (p < fpc) {
        uint8_t op = code[p];
        if (op == OP_LINE || op == OP_SRCFILE) { p += 3; continue; }
        if (op == OP_MULADD) {
            int sl = code[p + 1] | (code[p + 2] << 8);
            uint32_t mul = rd32c(code + p + 3);
            uint32_t add = rd32c(code + p + 7);
            emit_const(&b, 9, mul);
            emit_const(&b, 10, add);
            jb(&b, e_madd(smap[sl], smap[sl], 9, 10));
            p += 11;
            continue;
        }
        if (op == OP_ADDSHR) {
            int sx = code[p + 1] | (code[p + 2] << 8);
            int sy = code[p + 3] | (code[p + 4] << 8);
            int sh = code[p + 5];
            jb(&b, e_add_lsr(smap[sx], smap[sx], smap[sy], sh));
            p += 6;
            continue;
        }
        if (op == OP_ILOAD) {
            int sl = code[p + 1] | (code[p + 2] << 8);
            st[sn++] = sl;              /* smap dipakai saat dikonsumsi */
            p += 3;
            continue;
        }
        if (op == OP_IDXR_UB || op == OP_IDXR_B) {
            int ix = st[--sn];
            int bs = st[--sn];
            int rb = smap[bs], ri = smap[ix];
            if (nbail >= 256) { munmap(buf, (size_t)pg); return 0; }
            bail[nbail++] = b.n;
            jb(&b, e_cbz_x(rb));            /* basis NULL → bail */
            jb(&b, e_ldr_w(11, rb, 0));     /* w11 = len */
            jb(&b, e_cmp(ri, 11));
            bail[nbail++] = b.n;
            jb(&b, e_bcond(2));             /* b.hs bail (idx >= len / negatif) */
            jb(&b, e_add_imm8(12, rb));     /* x12 = data */
            jb(&b, op == OP_IDXR_B ? e_ldrsb_uxtw(11, 12, ri)
                                   : e_ldrb_uxtw(11, 12, ri));
            st[sn++] = -1;                  /* hasil di w11 */
            p += 1;
            continue;
        }
        /* OP_OP_STORE */
        int bo = code[p + 2];
        int sl = code[p + 3] | (code[p + 4] << 8);
        int v = st[--sn];
        int rv = (v < 0) ? 11 : smap[v];
        int rd = smap[sl];
        switch (bo) {
        case 0: jb(&b, e_add(rd, rd, rv)); break;
        case 1: jb(&b, e_sub(rd, rd, rv)); break;
        case 2: jb(&b, e_mul(rd, rd, rv)); break;
        case 3: jb(&b, e_and(rd, rd, rv)); break;
        case 4: jb(&b, e_orr(rd, rd, rv)); break;
        default: jb(&b, e_eor(rd, rd, rv)); break;
        }
        p += 5;
    }

    int back = emit_footer(&b, sreg, breg, imm, delta, cm, 0);
    int exit_l = b.n;

    jb(&b, e_movz(0, 0));                   /* selesai normal */
    int to_epi = b.n;
    jb(&b, e_b());
    int bail_l = b.n;
    jb(&b, e_movz(0, 1));                   /* bail → interpreter ulangi */
    int epi = b.n;

    for (int i = 0; i < ns; i++) {
        int sl = regs[i];
        if (ptr[sl]) {
            jb(&b, e_str_x(20 + i, 19, sl * 8));
        } else {
            jb(&b, e_sxtw(20 + i, 20 + i));
            jb(&b, e_str_x(20 + i, 19, sl * 8));
        }
    }
    jb(&b, e_ldp_post(27, 28, 31, 2));
    jb(&b, e_ldp_post(25, 26, 31, 2));
    jb(&b, e_ldp_post(23, 24, 31, 2));
    jb(&b, e_ldp_post(21, 22, 31, 2));
    jb(&b, e_ldp_post(19, 20, 31, 2));
    jb(&b, e_ldp_post(29, 30, 31, 2));
    jb(&b, 0xD65F03C0);                     /* ret */

    if ((size_t)b.n * 4 > (size_t)pg) { munmap(buf, (size_t)pg); return 0; }

    patch_bcond(buf, pre, exit_l);
    patch_bcond(buf, back, body_start);
    for (int i = 0; i < nbail; i++) patch_cbz(buf, bail[i], bail_l);
    patch_b(buf, to_epi, epi);

    __builtin___clear_cache((char *)buf, (char *)buf + b.n * 4);
    if (mprotect(buf, (size_t)pg, PROT_READ | PROT_EXEC) != 0) {
        munmap(buf, (size_t)pg);
        return 0;
    }
    return buf;
}

/* dipanggil interpreter tepat sesudah opcode OP_LOOP/OP_LOOP_R dibaca;
 * mengembalikan 1 bila loop sudah dijalankan sampai selesai oleh JIT. */
static int jit_try(Frame *f, Meth *m) {
    if (!jit_on) return 0;
    int fpc = (int)f->pc - 1;
    if (fpc < 0) return 0;
    uint8_t op = m->code[fpc];
    if (op != OP_LOOP && op != OP_LOOP_R) return 0;
    JitEnt *e = jit_find(f->meth, fpc);
    if (!e) return 0;
    if (e->state == 1) {
        int64_t r = ((JitFn)e->fn)(&localsv[nframes - 1][0]);
        if (r) {
            e->state = 2;               /* jangan coba lagi */
            f->pc = (long)e->target;    /* interpreter ulangi badan */
            return 1;                   /* lewati handler OP_LOOP_* */
        }
        f->pc += (op == OP_LOOP_R) ? 14 : 16;
        return 1;
    }
    if (e->state == 2) return 0;
    if (++e->hot >= JIT_THRESHOLD) {
        int tgt = 0;
        void *fn = jit_compile(m, fpc, &tgt);
        if (fn) { e->fn = fn; e->state = 1; e->target = tgt; }
        else e->state = 2;
    }
    return 0;
}

#else  /* !JIT_ON */

static int jit_try(Frame *f, Meth *m) { (void)f; (void)m; return 0; }

#endif
