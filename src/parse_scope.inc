/* parse_scope.inc: scope, nama, lambda, tipe dasar — bagian dari parse.c (unit kompilasi sama). */

typedef struct { const char *name; int slot; Ty ty; const char *cls; } SVar;

static SVar scope[1024];
static int nscope;
static int nextslot;
static int framebase;      /* batas frame metode aktif; scope di bawah ini
                              milik metode luar → ambil jadi capture */

/* v1.20: malloc{...} — batas scope & slot saat masuk region, untuk
 * menolak rujukan (String/array/objek) yang bocor keluar blok. */
static int reg_base[32], reg_slot[32], reg_sp;

static int is_ref_ty(Ty t) {
    return t == T_STR || t == T_FN || ty_is_arr(t);
}

static void scope_push(const char *name, int slot, Ty ty, const char *cls) {
    for (int i = framebase; i < nscope; i++)
        if (!strcmp(scope[i].name, name))
            die("baris ???: variabel '%s' dideklarasikan dua kali", name);
    scope[nscope].name = name;
    scope[nscope].slot = slot;
    scope[nscope].ty = ty;
    scope[nscope].cls = cls;
    nscope++;
}

static SVar *scope_find(const char *name) {
    for (int i = nscope - 1; i >= 0; i--)
        if (!strcmp(scope[i].name, name)) return &scope[i];
    die("variabel '%s' tidak ada di scope", name);
    return 0;
}

/* cari nama; bila hanya ada di FRAME LUAR (lambda aktif) → buat capture
 * snapshot: salin jadi slot lokal baru; referensi berikut pakai salinan */
static SVar *resolve_name(const char *name, int line) {
    for (int i = nscope - 1; i >= framebase; i--)
        if (!strcmp(scope[i].name, name)) return &scope[i];
    if (framebase == 0)
        die("baris %d: variabel '%s' tidak ada di scope", line, name);
    for (int i = framebase - 1; i >= 0; i--) {
        if (strcmp(scope[i].name, name)) continue;
        Method *hm = cur_method;
        if (hm->ncapt >= 32)
            die("baris %d: terlalu banyak variabel yang ditangkap lambda (maks 32)", line);
        int j = hm->ncapt++;
        hm->captslot[j] = scope[i].slot;
        hm->captty[j] = scope[i].ty;
        int copy = nextslot++;
        scope_push(name, copy, scope[i].ty, scope[i].cls);   /* frame aktif memenangkan */
        hm->capcopy[j] = copy;
        return &scope[nscope - 1];
    }
    die("baris %d: variabel '%s' tidak ada di scope", line, name);
    return 0;
}

/* cari variabel di scope tanpa membunuh proses (untuk gula array) */
static SVar *lookup_name(const char *name) {
    for (int i = nscope - 1; i >= 0; i--)
        if (!strcmp(scope[i].name, name)) return &scope[i];
    return 0;
}

/* metode instance String: s.substring(...) → Str.substring?? — dipetakan
 * ke fungsi Str.* yang sudah ada (receiver jadi argumen pertama). */
static int is_str_method(const char *n) {
    return !strcmp(n, "charAt") || !strcmp(n, "substring") ||
           !strcmp(n, "sub") || !strcmp(n, "slice") || !strcmp(n, "len") ||
           !strcmp(n, "indexOf") || !strcmp(n, "lastIndexOf") ||
           !strcmp(n, "contains") || !strcmp(n, "startsWith") ||
           !strcmp(n, "endsWith") || !strcmp(n, "trim") ||
           !strcmp(n, "trimLeft") || !strcmp(n, "trimRight") ||
           !strcmp(n, "strip") || !strcmp(n, "upper") ||
           !strcmp(n, "lower") || !strcmp(n, "capitalize") ||
           !strcmp(n, "uncapitalize") || !strcmp(n, "swapCase") ||
           !strcmp(n, "repeat") || !strcmp(n, "equals") ||
           !strcmp(n, "equalsIgnoreCase") || !strcmp(n, "isEmpty") ||
           !strcmp(n, "isBlank") || !strcmp(n, "toInt") ||
           !strcmp(n, "toLong") || !strcmp(n, "toDouble") ||
           !strcmp(n, "toFloat") || !strcmp(n, "toShort") ||
           !strcmp(n, "toByte") || !strcmp(n, "toBool") ||
           !strcmp(n, "utf8Count") || !strcmp(n, "utf8Valid") ||
           !strcmp(n, "utf8Slice") || !strcmp(n, "utf8Reverse") ||
           !strcmp(n, "replaceSeq") || !strcmp(n, "replace") ||
           !strcmp(n, "countByte") || !strcmp(n, "count") ||
           !strcmp(n, "padLeft") || !strcmp(n, "padRight") ||
           !strcmp(n, "center") || !strcmp(n, "left") ||
           !strcmp(n, "right") || !strcmp(n, "truncate") ||
           !strcmp(n, "ellipsis") || !strcmp(n, "before") ||
           !strcmp(n, "after") || !strcmp(n, "beforeLast") ||
           !strcmp(n, "afterLast") || !strcmp(n, "between") ||
           !strcmp(n, "removePrefix") || !strcmp(n, "removeSuffix") ||
           !strcmp(n, "split") || !strcmp(n, "splitStr") ||
           !strcmp(n, "lines") || !strcmp(n, "words") ||
           !strcmp(n, "isDigits") || !strcmp(n, "isNumber") ||
           !strcmp(n, "isHex");
}

/* nama metode instance → fungsi Str.* */
static const char *str_method_target(const char *n) {
    if (!strcmp(n, "charAt")) return "at";
    if (!strcmp(n, "substring")) return "sub";   /* v1.17: sub → String */
    if (!strcmp(n, "equals")) return "eq";
    return n;
}

static int is_array_method(const char *n) {
    return !strcmp(n, "forEach") || !strcmp(n, "forEachIndexed") ||
           !strcmp(n, "map") || !strcmp(n, "filter") ||
           !strcmp(n, "reduce") || !strcmp(n, "any") ||
           !strcmp(n, "all") || !strcmp(n, "countWhere") ||
           !strcmp(n, "findIndex") || !strcmp(n, "sumBy") ||
           !strcmp(n, "copy") || !strcmp(n, "reverse");
}

static Method *find_method(const char *cls, const char *name) {
    for (Method *m = prog.methods; m; m = m->next)
        if (!strcmp(m->cls, cls) && !strcmp(m->name, name)) return m;
    return 0;
}

/* v1.41: konstanta `static T X = v;` didesugar menjadi metode X().
 * Dipakai untuk menerjemahkan akses Class.X / X → Class.X(). */
static Method *find_constfield(const char *cls, const char *name) {
    for (Method *m = prog.methods; m; m = m->next)
        if (m->is_constfield && !strcmp(m->cls, cls) &&
            !strcmp(m->name, name)) return m;
    return 0;
}

static void resolve_stmts(Node *lst);      /* deklarasi maju */
static void collect_cp_stmt(Node *s);
static Ty type_expr(Node *e);
static int cp_push(const char *s);

static int lam_counter;

static void method_append(Method *m) {
    Method **pp = &prog.methods;
    while (*pp) pp = &(*pp)->next;
    m->next = 0;
    *pp = m;
}

/* resolusi lambda: metode tersembunyi Lam_<Cls>_<n> dengan param pertama
 * $env; capture lintas-frame jadi salinan slot lokal (diisi ENVLOAD).
 * Snapshot by-value: nilai diambil saat lambda DIBUAT (evaluasi NEWCLO). */
static Ty resolve_lambda(Node *ar) {
    Method *om = cur_method;
    int o_ns = nscope, o_nb = framebase, o_sl = nextslot;

    Method *hm = xmalloc(sizeof(Method));
    char buf[600];
    snprintf(buf, sizeof buf, "Lam_%s_%d", om->cls, lam_counter);
    lam_counter++;
    hm->cls = om->cls;
    hm->name = xs(buf);
    hm->hidden = 1;

    /* parameter $env slu 0 + parameter pengguna */
    Param *envp = xmalloc(sizeof(Param));
    envp->name = "$env";
    envp->ty = (int)T_FN;
    envp->next = 0;
    hm->params = envp;
    Param **pp = &envp->next;
    for (Node *p = ar->c; p; p = p->next) {
        Param *np = xmalloc(sizeof(Param));
        np->name = p->str;
        np->next = 0;
        *pp = np;
        pp = &np->next;
        hm->nparams++;
    }
    hm->nparams++;                      /* $env */

    cur_method = hm;
    framebase = nscope;                 /* frame baru */
    nextslot = 0;
    scope_push("$env", nextslot++, T_FN, 0);
    for (Param *p = envp->next; p; p = p->next)
        scope_push(p->name, nextslot++, T_INT, 0);
    hm->nlocals = nextslot;

    /* badan */
    Node *body, *plist_tail_pre = 0;
    if (ar->a) {                        /* badan ekspresi */
        Ty t = type_expr(ar->a);
        if (t == T_FN)
            die("baris %d: lambda mengembalikan lambda belum didukung", ar->line);
        hm->ret = t;
        Node *ret = newnode(ST_RETURN, ar->line);
        ret->a = ar->a;
        body = ret;
    } else {                            /* badan pernyataan */
        hm->ret = T_INT;
        body = ar->d;
        resolve_stmts(body);
    }

    /* preamble: salin capture dari env ke slot lokal */
    Node *pre = 0, *tail = 0;
    for (int j = 0; j < hm->ncapt; j++) {
        Node *as = newnode(ST_ASSIGN, ar->line);
        as->ival = hm->capcopy[j];
        as->dty = (int)hm->captty[j];
        Node *el = newnode(EX_ENVLOAD, ar->line);
        el->ival = j;
        el->dty = (int)hm->captty[j];
        as->a = el;
        if (tail) tail->next = as; else pre = as;
        tail = as;
    }
    if (pre) tail->next = body; else pre = body;
    hm->body = pre;
    hm->nlocals = nextslot;
    if (hm->nlocals > 256) die("baris %d: lambda terlalu banyak variabel", ar->line);

    collect_cp_stmt(hm->body);

    cur_method = om;
    nscope = o_ns;
    framebase = o_nb;
    nextslot = o_sl;

    hm->index = prog.nmethods++;
    method_append(hm);
    ar->ival = hm->index;
    ar->cp = (long)hm;                  /* langsung ke metode untuk codegen */
    return hm->ret;
}

static Ty type_expr_raw(Node *e);
static int fits_literal(Ty dst, Node *e);

/* ── constant folding (v1.47) ─────────────────────────────────────────
 * Dijalankan sekali saat resolusi tipe, setelah type_expr_raw memastikan
 * operatornya sah. Ekspresi yang seluruh operandnya literal diganti nilai
 * literal, sehingga berlaku untuk ketiga backend (a64/x64/VM) tanpa
 * menyentuh codegen. Semantik wrap mengikuti lebar tipe hasil; pembagian
 * dengan nol sengaja TIDAK dilipat agar perilaku runtime tidak berubah. */

static long fold_wrap(unsigned long long v, Ty t) {
    int w = ty_width(t);
    if (w >= 64) return (long)v;
    unsigned long long m = (1ULL << w) - 1;
    v &= m;
    if (!ty_is_uint(t) && ((v >> (w - 1)) & 1ULL)) v |= ~m;
    return (long)v;
}

static int fold_lit(Node *n, long *out) {
    if (!n) return 0;
    if ((n->kind == EX_INT || n->kind == EX_BOOL) &&
        !ty_is_float((Ty)n->dty)) {
        *out = n->ival;
        return 1;
    }
    if (n->kind == EX_UN && n->str && n->str[0] == '-' && n->a &&
        (n->a->kind == EX_INT || n->a->kind == EX_BOOL) &&
        !ty_is_float((Ty)n->a->dty)) {
        *out = -n->a->ival;
        return 1;
    }
    return 0;
}

static void fold_to_int(Node *e, Ty t, long v) {
    e->kind = EX_INT;
    e->str = 0;
    e->dty = (int)t;
    e->ival = v;
    e->fval = (double)v;
    e->typed = 1;
}

static void fold_to_bool(Node *e, int v) {
    e->kind = EX_BOOL;
    e->str = 0;
    e->dty = (int)T_BOOL;
    e->ival = v;
    e->typed = 1;
}

static Ty fold_un(Node *e, Ty t) {
    if (!e->a) return t;
    Node *a = e->a;
    if ((a->kind == EX_INT || a->kind == EX_BOOL) &&
        ty_is_float((Ty)a->dty)) {
        if (e->str[0] == '-') {
            e->kind = EX_INT;
            e->str = 0;
            e->dty = (int)t;
            e->ival = 0;
            e->fval = -a->fval;
            if (t == T_FLOAT) e->fval = (double)(float)e->fval;
            e->typed = 1;
        }
        return t;
    }
    long v;
    if (!fold_lit(a, &v)) return t;
    if (e->str[0] == '!') {
        fold_to_bool(e, v == 0);
        return T_BOOL;
    }
    if (e->str[0] == '~') {
        fold_to_int(e, t, fold_wrap(~(unsigned long long)v, t));
        return t;
    }
    fold_to_int(e, t, fold_wrap((unsigned long long)(-v), t));
    return t;
}

static Ty fold_bin(Node *e, Ty t) {
    if (e->kind != EX_BIN || !e->a || !e->b || !e->str) return t;
    Node *A = e->a, *B = e->b;
    char op = e->str[0];
    int two = e->str[1] != 0;
    Ty lt = (Ty)A->dty, rt = (Ty)B->dty;

    /* float: hanya aritmetika; perbandingan dibiarkan ke runtime agar
     * aturan NaN/-0.0 persis sama dengan mesin target. */
    if ((A->kind == EX_INT && ty_is_float(lt)) &&
        (B->kind == EX_INT && ty_is_float(rt)) &&
        (op == '+' || op == '-' || op == '*' || op == '/') && !two) {
        Ty P = ty_promote(lt, rt);
        double r;
        if (P == T_FLOAT) {
            float x = (float)A->fval, y = (float)B->fval, z;
            if (op == '+') z = x + y;
            else if (op == '-') z = x - y;
            else if (op == '*') z = x * y;
            else z = x / y;
            r = (double)z;
        } else {
            double x = A->fval, y = B->fval;
            if (op == '+') r = x + y;
            else if (op == '-') r = x - y;
            else if (op == '*') r = x * y;
            else r = x / y;
        }
        e->kind = EX_INT;
        e->str = 0;
        e->dty = (int)P;
        e->ival = 0;
        e->fval = r;
        e->typed = 1;
        return P;
    }

    long av, bv;
    if (!fold_lit(A, &av) || !fold_lit(B, &bv)) return t;
    if (!ty_is_int(lt) || !ty_is_int(rt)) return t;

    int isShift = !strcmp(e->str, "<<") || !strcmp(e->str, ">>") ||
                  !strcmp(e->str, ">>>");
    int isCmp = (op == '=' || op == '!') && two;
    if (!isCmp && !isShift)
        isCmp = op == '<' || op == '>' || !strcmp(e->str, "<=") ||
                !strcmp(e->str, ">=");
    int isLogic = (op == '&' || op == '|') && two;

    Ty P = isShift ? lt : ty_promote(lt, rt);
    if (ty_is_float(P) || !ty_is_int(P)) return t;
    int w = ty_width(P);
    if (w <= 0 || w > 64) return t;

    unsigned long long mask = w >= 64 ? ~0ULL : ((1ULL << w) - 1);
    unsigned long long ua = (unsigned long long)av & mask;
    unsigned long long ub = (unsigned long long)bv & mask;
    long sa = (long)(ua << (64 - w)) >> (64 - w);
    long sb = (long)(ub << (64 - w)) >> (64 - w);

    if (isLogic) {
        fold_to_bool(e, (op == '&') ? (sa && sb) : (sa || sb));
        return T_BOOL;
    }
    if (isCmp) {
        int r;
        if (op == '=') r = ua == ub;
        else if (op == '!') r = ua != ub;
        else if (ty_is_uint(P)) {
            if (!strcmp(e->str, "<=")) r = ua <= ub;
            else if (!strcmp(e->str, ">=")) r = ua >= ub;
            else if (op == '<') r = ua < ub;
            else r = ua > ub;
        } else {
            if (!strcmp(e->str, "<=")) r = sa <= sb;
            else if (!strcmp(e->str, ">=")) r = sa >= sb;
            else if (op == '<') r = sa < sb;
            else r = sa > sb;
        }
        fold_to_bool(e, r);
        return T_BOOL;
    }

    unsigned long long ur;
    if (isShift) {
        /* hanya 32/64-bit: shift byte/short punya aturan lebar yang
         * berbeda antar jalur codegen, jadi dibiarkan */
        if (w != 32 && w != 64) return t;
        unsigned cnt = (unsigned)(ub & (unsigned long long)(w - 1));
        if (op == '<') {                     /* "<<" */
            ur = ua << cnt;
        } else if (!strcmp(e->str, ">>>")) {
            ur = ua >> cnt;
        } else {                             /* ">>" aritmetika */
            if (cnt == 0) ur = ua;
            else if (sa < 0)
                ur = (ua >> cnt) | (((1ULL << cnt) - 1) << (w - cnt));
            else ur = ua >> cnt;
        }
    } else if (op == '+' || op == '-' || op == '*') {
        if (op == '+') ur = ua + ub;
        else if (op == '-') ur = ua - ub;
        else ur = ua * ub;
    } else if (op == '&' || op == '|' || op == '^') {
        if (op == '&') ur = ua & ub;
        else if (op == '|') ur = ua | ub;
        else ur = ua ^ ub;
    } else if (op == '/' || op == '%') {
        if (ub == 0) return t;               /* jangan ubah perilaku runtime */
        if (ty_is_uint(P)) {
            ur = (op == '/') ? (ua / ub) : (ua % ub);
        } else if (sb == -1 && sa == (w == 64 ? (long)(1ULL << 63)
                                              : (long)(-2147483647L - 1))) {
            ur = (op == '/') ? (unsigned long long)sa : 0;  /* INT_MIN / -1 */
        } else {
            ur = (unsigned long long)((op == '/') ? (sa / sb) : (sa % sb));
        }
    } else {
        return t;
    }
    fold_to_int(e, P, fold_wrap(ur, P));
    return P;
}

static Ty type_expr(Node *e) {
    if (e->typed) return (Ty)e->dty;
    Ty t = type_expr_raw(e);
    if (e->kind == EX_UN) t = fold_un(e, t);
    else if (e->kind == EX_BIN) t = fold_bin(e, t);
    e->dty = (int)t;          /* semua ekspresi menyimpan tipe hasilnya */
    e->typed = 1;
    return t;
}

/* class dari ekspresi objek sederhana: variabel ber-class, akses field
 * (berantai), atau `new Kelas()`. 0 bila bukan objek bertipe class. */
static const char *obj_class(Node *n) {
    if (n->kind == EX_VAR) {
        SVar *rv = lookup_name(n->str);
        return rv && rv->cls ? rv->cls : 0;
    }
    if (n->kind == EX_FLD) {
        type_expr(n);            /* resolusi berantai: base dulu */
        return n->cls;
    }
    if (n->kind == EX_NEWARR) return n->cls;
    return 0;
}

static Field *field_of(Node *obj, const char *fname, int line) {
    const char *bc = obj_class(obj);
    if (!bc)
        die("baris %d: akses field '%s' butuh variabel objek sederhana "
            "(simpan dulu: P p = ...; lalu p.%s)", line, fname, fname);
    Class *c = find_class(bc);
    Field *f = c ? class_field(c, fname) : 0;
    if (!f)
        die("baris %d: class %s tidak punya field '%s'", line, bc, fname);
    return f;
}

