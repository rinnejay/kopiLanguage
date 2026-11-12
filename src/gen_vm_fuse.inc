/* gen_vm_fuse.inc — pola & superinstruksi loop panas (bagian gen_vm.c, v1.22). */

/* ── v1.22: superinstruksi loop panas ───────────────────────────────
 * Pola bytecode yang muncul jutaan kali di loop angka digabung menjadi
 * satu opcode. Hasilnya identik dengan urutan opcode aslinya, hanya
 * jumlah dispatch yang turun (loop panas: 15 → 3 per iterasi). */

/* kode perbandingan: 0 <, 1 <=, 2 >, 3 >=, 4 ==, 5 != */
static int cmp_code(const char *s) {
    if (!strcmp(s, "<"))  return 0;
    if (!strcmp(s, "<=")) return 1;
    if (!strcmp(s, ">"))  return 2;
    if (!strcmp(s, ">=")) return 3;
    if (!strcmp(s, "==")) return 4;
    if (!strcmp(s, "!=")) return 5;
    return -1;
}
static int cmp_swap(int c) {
    static const int sw[6] = { 2, 3, 0, 1, 4, 5 };
    return sw[c];
}
/* operator int: 0 +, 1 -, 2 *, 3 &, 4 |, 5 ^ */
static int bin_code(const char *s) {
    if (!strcmp(s, "+")) return 0;
    if (!strcmp(s, "-")) return 1;
    if (!strcmp(s, "*")) return 2;
    if (!strcmp(s, "&")) return 3;
    if (!strcmp(s, "|")) return 4;
    if (!strcmp(s, "^")) return 5;
    return -1;
}
static void fix_at(int lab) {
    cx.fixes[cx.nfixes].pos = cx.len;
    cx.fixes[cx.nfixes].lab = lab;
    cx.nfixes++;
    em32(0);
}
/* lipat ekspresi konstan int32 (aritmetika dibungkus seperti OP_ADD/SUB/MUL) */
static int const_int(Node *e, long *out) {
    if (!e || (Ty)e->dty != T_INT) return 0;
    switch (e->kind) {
    case EX_INT: *out = (int32_t)e->ival; return 1;
    case EX_UN: {
        long v;
        if (!const_int(e->a, &v)) return 0;
        if (!strcmp(e->str, "-")) { *out = (int32_t)(-(int64_t)v); return 1; }
        if (!strcmp(e->str, "~")) { *out = (int32_t)(~(int64_t)v); return 1; }
        return 0;
    }
    case EX_BIN: {
        long x, y;
        if (!const_int(e->a, &x) || !const_int(e->b, &y)) return 0;
        if (!strcmp(e->str, "+")) *out = (int32_t)((int64_t)x + (int64_t)y);
        else if (!strcmp(e->str, "-")) *out = (int32_t)((int64_t)x - (int64_t)y);
        else if (!strcmp(e->str, "*")) *out = (int32_t)((int64_t)x * (int64_t)y);
        else if (!strcmp(e->str, "&")) *out = (int32_t)(x & y);
        else if (!strcmp(e->str, "|")) *out = (int32_t)(x | y);
        else if (!strcmp(e->str, "^")) *out = (int32_t)(x ^ y);
        else return 0;
        return 1;
    }
    default: return 0;
    }
}

/* cocokkan kondisi `var-int <cmp> konstanta` (tanpa efek samping) */
static int cond_pat(Node *e, int *slot, int *cmp, int *imm) {
    if (!e || e->kind != EX_BIN) return 0;
    int c = cmp_code(e->str);
    if (c < 0) return 0;
    if ((Ty)e->a->dty != T_INT || (Ty)e->b->dty != T_INT) return 0;
    Node *v = 0;
    long iv;
    if (e->a->kind == EX_VAR && const_int(e->b, &iv)) v = e->a;
    else if (e->b->kind == EX_VAR && const_int(e->a, &iv)) {
        v = e->b; c = cmp_swap(c);
    } else return 0;
    *slot = (int)v->ival; *cmp = c; *imm = (int)iv;
    return 1;
}
/* cabang kondisi: satu opcode bila polanya cocok, kalau tidak cara lama */
static void emit_cond_jmp(Node *cond, int lab, int jtrue) {
    int slot, cmp, imm;
    if (!cond_pat(cond, &slot, &cmp, &imm)) {
        gen_expr(cond);
        jmp(jtrue ? OP_JMPT : OP_JMPF, lab);
        return;
    }
    em8(jtrue ? OP_CMP_JMPT : OP_CMP_JMPF);
    em8(0);                     /* type: 0 = int32 */
    em8(cmp); em16(slot); em32(imm);
    fix_at(lab);
}
/* cocokkan langkah `i = i +/- k` (k konstan); delta = ±k */
static int step_pat(Node *st, int slot, int *delta) {
    if (!st || st->kind != ST_ASSIGN) return 0;
    if ((int)st->ival != slot || (Ty)st->dty != T_INT) return 0;
    Node *b = st->a;
    if (!b || b->kind != EX_BIN || (Ty)b->dty != T_INT) return 0;
    if (strcmp(b->str, "+") && strcmp(b->str, "-")) return 0;
    if (b->a->kind != EX_VAR || (int)b->a->ival != slot) return 0;
    long k;
    if (!const_int(b->b, &k)) return 0;
    if (k < 0 || k > 2147483647L) return 0;
    *delta = (b->str[0] == '+') ? (int)k : -(int)k;
    return 1;
}

