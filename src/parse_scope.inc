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

static Ty type_expr(Node *e) {
    if (e->typed) return (Ty)e->dty;
    Ty t = type_expr_raw(e);
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

