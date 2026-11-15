/* parse_class.inc: deklarasi class/metode/field — bagian dari parse.c (unit kompilasi sama). */

static void skip_mods(void) {
    while (at(KW_PUBLIC) || at(KW_PRIVATE) || at(KW_STATIC) || at(KW_FINAL)) advance();
}

static int mods_static(void) {
    /* seperti skip_mods tetapi melaporkan ada tidaknya 'static' */
    int st = 0;
    for (;;) {
        if (at(KW_PUBLIC) || at(KW_PRIVATE) || at(KW_FINAL)) { advance(); continue; }
        if (at(KW_STATIC)) { st = 1; advance(); continue; }
        break;
    }
    return st;
}

static Class *find_class(const char *name) {
    for (Class *c = prog.classes; c; c = c->next)
        if (!strcmp(c->name, name)) return c;
    return 0;
}

static TraitDef *find_trait(const char *name) {
    for (TraitDef *t = prog.traits; t; t = t->next)
        if (!strcmp(t->name, name)) return t;
    return 0;
}

/* v1.46: trait = kumpulan metode dengan implementasi default (mirip Rust trait
 * / C++ mixin). Dipakai via `class Nama : Trait1, Trait2 { ... }`. */
static void parse_trait(void) {
    Tok *tt = peek();
    advance();                               /* 'trait' */
    Tok *id = peek();
    expect(T_ID, "nama trait");
    if (find_class(xs(id->text)))
        die("baris %d: trait '%s' bentrok dengan nama class", id->line, id->text);
    if (find_trait(xs(id->text)))
        die("baris %d: trait '%s' dideklarasikan dua kali", id->line, id->text);
    TraitDef *td = xmalloc(sizeof *td);
    td->name = xs(id->text);
    td->next = prog.traits;
    prog.traits = td;
    const char *saved_cls = prog.curcls;
    prog.curcls = td->name;   /* bare call dalam body trait → diagnostik jelas */

    expect('{', "'{' setelah nama trait");
    while (!at('}')) {
        int was_static = mods_static();  /* trait method bisa static? tidak perlu, tapi biarkan */
        Ty ret = type_opt();
        if (ret == T_FN)
            die("baris %d: metode trait dengan return lambda belum didukung", peek()->line);
        Tok *mn = peek();
        expect(T_ID, "nama metode trait");
        expect('(', "'(' setelah nama metode trait");
        Param **pp = 0;
        int nparams = 0;
        int is_static = was_static;
        // We need a dummy method to parse params like regular method
        Method dummy = {0};
        dummy.params = 0;
        Param **dpp = &dummy.params;
        if (!is_static) {
            /* trait method instance: param pertama implisit `this` */
            Param *th = xmalloc(sizeof(Param));
            th->name = xs("this");
            th->ty = (int)T_ARI;
            th->cls = 0;  /* class akan di-set saat apply_traits */
            th->next = 0;
            *dpp = th;
            dpp = &th->next;
            nparams++;
        }
        if (!at(')')) {
            do {
                Ty pt;
                const char *pcls = 0;
                if (at(T_ID) && tokat(1, T_ID) && find_class(xs(peek()->text))) {
                    pcls = xs(peek()->text);
                    advance();
                    pt = T_ARI;
                } else {
                    pt = type_param();
                }
                if (pt == T_VOID) die("baris %d: tipe parameter void", peek()->line);
                Tok *pn = peek();
                expect(T_ID, "nama parameter");
                Param *p = xmalloc(sizeof(Param));
                p->name = xs(pn->text);
                p->ty = (int)pt;
                p->cls = pcls;
                p->next = 0;
                *dpp = p;
                dpp = &p->next;
                nparams++;
            } while (at(',') && (advance(), 1));
        }
        expect(')', "')' setelah parameter trait");

        Node *body = 0;
        if (at('{')) {
            body = block();  /* default implementation */
        } else {
            expect(';', "';' atau '{' setelah signature trait");
        }
        // Create TraitMethod
        TraitMethod *tm = xmalloc(sizeof *tm);
        tm->name = xs(mn->text);
        tm->ret = ret;
        tm->params = dummy.params;
        tm->nparams = nparams;
        tm->body = body;
        tm->retcls = 0;
        tm->next = td->methods;
        td->methods = tm;
    }
    expect('}', "'}' tutup trait");
    if (at(';')) advance();
    prog.curcls = saved_cls;
    (void)tt;
}

/* v1.46: enum = kumpulan konstanta int bernama (`Warna.MERAH`).
 * Tidak ada objek runtime — nilai langsung menjadi literal int saat
 * resolusi, sehingga bisa dipakai di switch/case, array, aritmetika. */
static EnumDef *find_enum(const char *name) {
    for (EnumDef *e = prog.enums; e; e = e->next)
        if (!strcmp(e->name, name)) return e;
    return 0;
}

static int enum_lookup(const char *en, const char *mem, long *out) {
    EnumDef *ed = find_enum(en);
    if (!ed) return 0;
    for (EnumConst *c = ed->consts; c; c = c->next)
        if (!strcmp(c->name, mem)) {
            if (out) *out = c->val;
            return 1;
        }
    return 0;
}

static void parse_enum(void) {
    Tok *et = peek();
    advance();                               /* 'enum' */
    Tok *id = peek();
    expect(T_ID, "nama enum");
    if (find_class(xs(id->text)))
        die("baris %d: enum '%s' bentrok dengan nama class", id->line, id->text);
    if (find_enum(xs(id->text)))
        die("baris %d: enum '%s' dideklarasikan dua kali", id->line, id->text);
    EnumDef *ed = xmalloc(sizeof *ed);
    ed->name = xs(id->text);
    ed->next = prog.enums;
    prog.enums = ed;

    expect('{', "'{' setelah nama enum");
    long next = 0;
    while (!at('}')) {
        Tok *cn = peek();
        expect(T_ID, "nama konstanta enum");
        long v = next;
        if (at('=')) {
            advance();
            int neg = 0;
            if (at('-')) { advance(); neg = 1; }
            if (!at(T_NUM))
                die("baris %d: nilai enum '%s' harus literal bilangan bulat",
                    cn->line, cn->text);
            v = peek()->num;
            advance();
            if (neg) v = -v;
        }
        for (EnumConst *k = ed->consts; k; k = k->next)
            if (!strcmp(k->name, cn->text))
                die("baris %d: konstanta enum '%s' duplikat di %s",
                    cn->line, cn->text, ed->name);
        EnumConst *ec = xmalloc(sizeof *ec);
        ec->name = xs(cn->text);
        ec->val = v;
        ec->next = ed->consts;
        ed->consts = ec;
        next = v + 1;
        if (at(',')) { advance(); continue; }
        break;
    }
    expect('}', "'}' tutup enum");
    if (at(';')) advance();                  /* titik koma opsional */
    (void)et;
}

static Field *class_field(Class *c, const char *name) {
    for (Field *f = c->fields; f; f = f->next)
        if (!strcmp(f->name, name)) return f;
    return 0;
}

/* v1.47: salin AST metode trait agar tiap class punya salinan sendiri —
 * slot lokal & hasil resolusi tidak saling menimpa antar class/trait. */
static Node *clone_node(Node *n) {
    if (!n) return 0;
    Node *c = newnode(n->kind, n->line);
    c->str = n->str;
    c->ival = n->ival;
    c->fval = n->fval;
    c->cp = n->cp;
    c->dty = n->dty;
    c->typed = 0;
    c->file = n->file;
    c->cls = n->cls;
    c->meth = 0;
    c->a = clone_node(n->a);
    c->b = clone_node(n->b);
    c->c = clone_node(n->c);
    c->d = clone_node(n->d);
    c->next = clone_node(n->next);
    return c;
}

/* v1.47: salin parameter trait; param `this` implisit diberi class pemakai. */
static Param *clone_params(Param *p, const char *thiscls) {
    Param *head = 0, **tail = &head;
    for (; p; p = p->next) {
        Param *q = xmalloc(sizeof *q);
        q->name = p->name;
        q->ty = p->ty;
        q->cls = p->cls;
        q->next = 0;
        *tail = q;
        tail = &q->next;
    }
    if (head && head->ty == (int)T_ARI && !head->cls &&
        !strcmp(head->name, "this"))
        head->cls = thiscls;
    return head;
}

/* ── v1.51: generics — monomorfisasi class generik ───────────────────
 * `class Box<T> { T isi; }` tidak pernah di-resolve atau di-codegen.
 * Saat program menulis `Box<int>`, parser memanggil instantiate_generic():
 * class konkret `Box$int` dibuat dengan menyalin field & metode template
 * lalu mengganti setiap T dengan tipe argumen. Karena hasilnya class
 * biasa, ketiga backend tidak perlu tahu apa-apa soal generics. */

static const char *gen_suffix(Ty t, const char *tcls) {
    switch (t) {
    case T_INT: return "int";
    case T_LONG: return "long";
    case T_BOOL: return "bool";
    case T_DOUBLE: return "double";
    case T_FLOAT: return "float";
    case T_BYTE: return "byte";
    case T_SHORT: return "short";
    case T_UINT8: return "u8";
    case T_UINT16: return "u16";
    case T_UINT32: return "u32";
    case T_UINT64: return "u64";
    case T_STR: return "str";
    case T_ARI: return tcls ? tcls : "obj";
    default: return "x";
    }
}

static void gen_subst_node(Node *n, const char *tmpl, const char *mangled,
                           Ty aty, const char *acls);

static void gen_subst_list(Node *n, const char *tmpl, const char *mangled,
                           Ty aty, const char *acls) {
    for (; n; n = n->next) gen_subst_node(n, tmpl, mangled, aty, acls);
}

static void gen_subst_node(Node *n, const char *tmpl, const char *mangled,
                           Ty aty, const char *acls) {
    if (!n) return;
    if ((Ty)n->dty == T_TVAR) n->dty = (int)aty;
    if (n->cls && !strcmp(n->cls, tmpl)) n->cls = mangled;
    if (n->str && (n->kind == EX_CALL || n->kind == EX_NEWOBJ ||
                   n->kind == EX_PCALL)) {
        size_t tl = strlen(tmpl);
        if (!strncmp(n->str, tmpl, tl) && n->str[tl] == '.') {
            char *q = xmalloc(tl + strlen(mangled) + strlen(n->str + tl) + 1);
            sprintf(q, "%s%s", mangled, n->str + tl);
            n->str = q;
        } else if (!strcmp(n->str, tmpl)) {
            n->str = mangled;
        }
    }
    gen_subst_node(n->a, tmpl, mangled, aty, acls);
    gen_subst_node(n->b, tmpl, mangled, aty, acls);
    gen_subst_node(n->c, tmpl, mangled, aty, acls);
    gen_subst_node(n->d, tmpl, mangled, aty, acls);
}

static const char *instantiate_generic(const char *base, Ty aty,
                                       const char *acls, int line) {
    Class *g = find_class(base);
    if (!g || !g->tparam)
        die("baris %d: '%s' bukan class generik", line, base);
    const char *suf = gen_suffix(aty, acls);
    char *name = xmalloc(strlen(base) + strlen(suf) + 2);
    sprintf(name, "%s$%s", base, suf);
    Class *ex = find_class(name);
    if (ex) return ex->name;

    Class *c = xmalloc(sizeof *c);
    c->name = name;
    c->next = prog.classes;
    prog.classes = c;

    Field **ft = &c->fields;
    for (Field *f = g->fields; f; f = f->next) {
        Field *q = xmalloc(sizeof *q);
        q->name = f->name;
        q->idx = f->idx;
        if ((Ty)f->ty == T_TVAR) { q->ty = (int)aty; q->cls = acls; }
        else {
            q->ty = f->ty;
            q->cls = (f->cls && !strcmp(f->cls, base)) ? c->name : f->cls;
        }
        *ft = q;
        ft = &q->next;
    }
    c->nfields = g->nfields;

    for (Method *m = g->gmethods; m; m = m->next) {
        Method *q = xmalloc(sizeof *q);
        *q = *m;
        q->cls = c->name;
        q->next = 0;
        if ((Ty)q->ret == T_TVAR) { q->ret = aty; q->retcls = acls; }
        else if (q->retcls && !strcmp(q->retcls, base)) q->retcls = c->name;
        Param *head = 0, **pt = &head;
        for (Param *p = m->params; p; p = p->next) {
            Param *r = xmalloc(sizeof *r);
            r->name = p->name;
            if ((Ty)p->ty == T_TVAR) { r->ty = (int)aty; r->cls = acls; }
            else {
                r->ty = p->ty;
                r->cls = (p->cls && !strcmp(p->cls, base)) ? c->name : p->cls;
            }
            *pt = r;
            pt = &r->next;
        }
        q->params = head;
        q->body = clone_node(m->body);
        gen_subst_list(q->body, base, c->name, aty, acls);
        q->hidden = 0;
        q->next = prog.methods;
        prog.methods = q;
        prog.nmethods++;
    }
    return c->name;
}

static const char *parse_generic_ref(const char *base, int line) {
    expect('<', "'<' pada argumen tipe generik");
    Ty aty;
    const char *acls = 0;
    int selfref = 0;
    if (at(T_ID) && find_class(xs(peek()->text))) {
        acls = xs(peek()->text);
        Class *ac = find_class(acls);
        if (ac->tparam)
            die("baris %d: argumen tipe generik bersarang belum didukung", line);
        advance();
        aty = T_ARI;
    } else if (at(T_ID) && g_tvar && !strcmp(xs(peek()->text), g_tvar)) {
        /* `Box<T>` di dalam `class Box<T>`: rujukan ke instansiasi yang
         * sedang dibuat. Kembalikan nama template; gen_subst_node akan
         * menggantinya dengan nama konkret saat instansiasi. */
        if (!prog.curcls || strcmp(base, prog.curcls))
            die("baris %d: argumen tipe generik dari class generik lain "
                "belum didukung", line);
        advance();
        aty = T_TVAR;
        selfref = 1;
    } else {
        aty = type_opt();
    }
    if (ty_is_arr(aty))
        die("baris %d: array sebagai argumen tipe generik belum didukung", line);
    expect('>', "'>' penutup argumen tipe");
    if (selfref) return base;
    return instantiate_generic(base, aty, acls, line);
}

/* v1.46: apply_traits — salin metode trait ke class (default implementation).
 * Jika class sudah punya metode dengan nama sama → skip (override).
 * v1.47: metode trait yang punya body juga disalin dengan nama mangled
 * `__tr_<Trait>_<method>` agar bisa dipanggil via `Trait::method()`. */
static void apply_traits(Class *c, TraitDef *td) {
    for (TraitMethod *tm = td->methods; tm; tm = tm->next) {
        int overridden = find_method(c->name, tm->name) != 0;
        /* implementasi asli trait untuk Trait::method() */
        if (tm->body) {
            char *mname = xmalloc(strlen(td->name) + strlen(tm->name) + 8);
            sprintf(mname, "__tr_%s_%s", td->name, tm->name);
            if (!find_method(c->name, mname)) {
                Method *pm = xmalloc(sizeof *pm);
                pm->cls = c->name;
                pm->name = mname;
                pm->ret = tm->ret;
                pm->params = clone_params(tm->params, c->name);
                pm->nparams = tm->nparams;
                pm->body = clone_node(tm->body);
                pm->retcls = tm->retcls;
                pm->is_inst = pm->params && !strcmp(pm->params->name, "this");
                pm->hidden = 0;
                pm->is_constfield = 0;
                pm->next = prog.methods;
                prog.methods = pm;
                prog.nmethods++;
            }
        }
        if (overridden)
            continue;  /* override: metode class menang */
        Method *m = xmalloc(sizeof *m);
        m->cls = c->name;
        m->name = xs(tm->name);
        m->ret = tm->ret;
        m->params = clone_params(tm->params, c->name);
        m->nparams = tm->nparams;
        m->body = clone_node(tm->body);
        m->retcls = tm->retcls;
        m->is_inst = m->params && !strcmp(m->params->name, "this");
        m->hidden = 0;
        m->is_constfield = 0;
        m->next = prog.methods;
        prog.methods = m;
        prog.nmethods++;
    }
}

/* tipe field: 32-bit (slot tunggal) atau referensi 64-bit (long/String/class) */
static int field_ty_ok(Ty t) {
    if (t == T_TVAR) return 1;            /* v1.51: field template generik */
    return t == T_INT || t == T_BOOL || t == T_BYTE ||
           t == T_SHORT || t == T_UINT8 || t == T_UINT16 ||
           t == T_LONG || t == T_STR ||   /* slot fisik 64-bit, v1.14 */
           ty_is_arr(t);                  /* v1.29: field array (pointer 64-bit) */
}

/* v1.51: pada posisi `Nama<T>`, tentukan apakah deklarasi ini metode
 * (`Nama<T> f(`) atau field (`Nama<T> f;`). Argumen tipe tidak bersarang,
 * jadi cukup cari '>' pertama lalu lihat ID dan token sesudahnya. */
static int generic_decl_is_method(void) {
    int k = ti + 1;
    while (ts[k].type && ts[k].type != '>' && ts[k].type != T_EOF) k++;
    return ts[k].type == '>' && ts[k + 1].type == T_ID && ts[k + 2].type == '(';
}

/* v1.50: struct = class tanpa metode/constructor/trait; hanya kumpulan
 * field. Semua field publik dan di-zero oleh `new Nama()` (memakai jalur
 * class: EX_NEWOBJ → EX_NEWARR, akses field → EX_IDX). */
static void parse_class(int is_struct) {
    const char *kw = is_struct ? "struct" : "class";
    if (is_struct) advance();                /* 'struct' */
    else {
        skip_mods();
    }
    int is_impl = 0;
    if (!is_struct && at(KW_IMPL)) { advance(); is_impl = 1; }
    else if (!is_struct) expect(KW_CLASS, "'class'");
    Tok *id = peek();
    expect(T_ID, is_impl ? "nama class setelah impl" : "nama class");
    Class *c;
    if (is_impl) {
        c = find_class(xs(id->text));
        if (!c)
            die("baris %d: impl '%s': class belum dideklarasikan — "
                "taruh impl setelah class-nya (gaya definisi luar C++)",
                id->line, id->text);
    } else {
        if (find_class(xs(id->text)))
            die("baris %d: %s '%s' dideklarasikan dua kali", id->line, kw, id->text);
        if (find_enum(xs(id->text)))
            die("baris %d: %s '%s' bentrok dengan nama enum", id->line, kw, id->text);
        c = xmalloc(sizeof(Class));
        c->name = xs(id->text);
        c->next = prog.classes;
        prog.classes = c;
    }
    if (!is_struct && at('<')) {          /* v1.51: class Box<T> */
        advance();
        Tok *tp = peek();
        expect(T_ID, "nama parameter tipe generik");
        c->tparam = xs(tp->text);
        expect('>', "'>' penutup parameter tipe");
    }
    prog.curcls = c->name;

    /* v1.46: class Main : Trait1, Trait2 { ... } */
    TraitDef *trait_list[16];
    int nt = 0;
    if (at(':')) {
        if (is_struct)
            die("baris %d: struct tidak mendukung trait — pakai class",
                peek()->line);
        if (c->tparam)
            die("baris %d: class generik belum mendukung trait", peek()->line);
        advance();
        do {
            Tok *tn = peek();
            expect(T_ID, "nama trait");
            TraitDef *td = find_trait(xs(tn->text));
            if (!td)
                die("baris %d: trait '%s' tidak ada (definisikan trait sebelum dipakai)",
                    tn->line, tn->text);
            if (nt >= 16) die("baris %d: terlalu banyak trait (maks 16)", tn->line);
            trait_list[nt++] = td;
        } while (at(',') && (advance(), 1));
    }

    g_tvar = c->tparam;                   /* v1.51: T dikenali di badan class */
    expect('{', is_struct ? "'{' setelah nama struct" : "'{' setelah nama class");
    while (!at('}')) {
        int was_static = 0;
        if (is_struct) {
            if (at(KW_PUBLIC) || at(KW_PRIVATE) || at(KW_FINAL))
                die("baris %d: struct tidak memakai modifier akses — semua field publik",
                    peek()->line);
            if (at(KW_STATIC))
                die("baris %d: struct tidak mendukung field/metode static", peek()->line);
        } else {
            was_static = mods_static();
        }
        Tok *rt = peek();
        /* constructor: NamaKelas(...) — desugar ke metode instance __ctor */
        if (at(T_ID) && !strcmp(xs(rt->text), c->name) && tokat(1, '(')) {
            if (is_struct)
                die("baris %d: struct tidak punya constructor — pakai "
                    "`new %s()` lalu set field-nya", rt->line, c->name);
            if (is_impl)
                die("baris %d: impl tidak boleh berisi constructor — "
                    "definisikan constructor di dalam class", rt->line);
            advance();                       /* nama class */
            if (was_static)
                die("baris %d: constructor tidak boleh static", rt->line);
            if (find_method(c->name, "__ctor"))
                die("baris %d: class %s sudah punya constructor (overload belum didukung)",
                    rt->line, c->name);
            Method *m = xmalloc(sizeof(Method));
            m->cls = c->name;
            m->name = xs("__ctor");
            m->ret = (int)T_VOID;
            m->is_inst = 1;
            expect('(', "'(' setelah nama constructor");
            Param **pp = &m->params;
            Param *th = xmalloc(sizeof(Param));
            th->name = xs("this");
            th->ty = (int)T_ARI;
            th->cls = c->name;
            *pp = th;
            pp = &th->next;
            m->nparams++;
            if (!at(')')) {
                do {
                    Ty pt;
                    const char *pcls = 0;
                    if (at(T_ID) && tokat(1, '<') && find_class(xs(peek()->text)) &&
                        find_class(xs(peek()->text))->tparam) {
                        const char *base = xs(peek()->text);
                        advance();
                        pcls = parse_generic_ref(base, peek()->line);
                        pt = T_ARI;
                    } else if (at(T_ID) && tokat(1, T_ID) && find_class(xs(peek()->text))) {
                        pcls = xs(peek()->text);
                        advance();
                        pt = T_ARI;
                    } else {
                        pt = type_param();
                    }
                    if (pt == T_VOID) die("baris %d: tipe parameter void", peek()->line);
                    Tok *pn = peek();
                    expect(T_ID, "nama parameter");
                    Param *p = xmalloc(sizeof(Param));
                    p->name = xs(pn->text);
                    p->ty = (int)pt;
                    p->cls = pcls;
                    *pp = p;
                    pp = &p->next;
                    m->nparams++;
                } while (at(',') && (advance(), 1));
            }
            expect(')', "')' setelah parameter constructor");
            m->body = stmt_or_block();
            m->next = prog.methods;
            prog.methods = m;
            prog.nmethods++;
            continue;
        }
        /* v1.51: field bertipe class generik: `Box<int> isi;` */
        if (at(T_ID) && tokat(1, '<') && find_class(xs(rt->text)) &&
            find_class(xs(rt->text))->tparam && !generic_decl_is_method()) {
            const char *base = xs(rt->text);
            advance();
            const char *fcls = parse_generic_ref(base, rt->line);
            Tok *fn = peek();
            expect(T_ID, "nama field");
            advance();                       /* ';' */
            if (is_impl)
                die("baris %d: impl hanya berisi metode; field '%s' harus "
                    "dideklarasikan di dalam class", fn->line, fn->text);
            if (was_static)
                die("baris %d: static field bertipe class belum didukung", fn->line);
            if (class_field(c, xs(fn->text)) ||
                find_constfield(c->name, xs(fn->text)))
                die("baris %d: field '%s' dideklarasikan dua kali", fn->line, fn->text);
            Field *f = xmalloc(sizeof(Field));
            f->name = xs(fn->text);
            f->idx = c->nfields++;
            f->ty = (int)T_ARI;
            f->cls = fcls;
            f->next = c->fields;
            c->fields = f;
            continue;
        }
        /* field bertipe class: `Titik pojok;` — slot referensi 64-bit */
        if (at(T_ID) && tokat(1, T_ID) && tokat(2, ';') && find_class(xs(rt->text))) {
            const char *fcls = xs(rt->text);
            if (find_class(fcls)->tparam)
                die("baris %d: field '%s' memakai class generik '%s' tanpa "
                    "argumen tipe (mis. %s<int>)",
                    rt->line, ts[ti + 1].text, fcls, fcls);
            advance();                       /* nama class */
            Tok *fn = peek();
            expect(T_ID, "nama field");
            advance();                       /* ';' */
            if (is_impl)
                die("baris %d: impl hanya berisi metode; field '%s' harus "
                    "dideklarasikan di dalam class", fn->line, fn->text);
            if (was_static)
                die("baris %d: static field bertipe class belum didukung — "
                    "pakai static method yang mengembalikan objek", fn->line);
            if (class_field(c, xs(fn->text)) ||
                find_constfield(c->name, xs(fn->text)))
                die("baris %d: field '%s' dideklarasikan dua kali", fn->line, fn->text);
            Field *f = xmalloc(sizeof(Field));
            f->name = xs(fn->text);
            f->idx = c->nfields++;
            f->ty = (int)T_ARI;
            f->cls = fcls;
            f->next = c->fields;
            c->fields = f;
            continue;
        }
        if (was_static && at(T_ID) && tokat(1, T_ID) &&
            (tokat(2, '=') || tokat(2, ';')))
            die("baris %d: static field bertipe class belum didukung — "
                "pakai static method yang mengembalikan objek", rt->line);
        Ty ret;
        const char *retcls = 0;
        if (at(T_ID) && tokat(1, '<') && find_class(xs(rt->text)) &&
            find_class(xs(rt->text))->tparam) {
            const char *base = xs(rt->text);
            advance();
            retcls = parse_generic_ref(base, rt->line);
            ret = T_ARI;
        } else if (at(T_ID) && tokat(1, T_ID) && tokat(2, '(') && find_class(xs(rt->text))) {
            retcls = xs(rt->text);       /* tipe kembalian class: P make() */
            if (find_class(retcls)->tparam)
                die("baris %d: tipe kembalian memakai class generik '%s' tanpa "
                    "argumen tipe (mis. %s<int>)", rt->line, retcls, retcls);
            advance();
            ret = T_ARI;
        } else {
            ret = type_opt();
            /* v1.46: metode boleh mengembalikan fn (dipakai Chan.recvFn). */
        }
        Tok *mn = peek();
        expect(T_ID, "nama metode");
        /* setelah nama: '(' → metode; ';' atau '=' → deklarasi field */
        if (at(';') || at('=')) {
            if (is_impl && !was_static)
                die("baris %d: impl hanya berisi metode; field '%s' harus "
                    "dideklarasikan di dalam class", mn->line, mn->text);
            if (was_static) {
                /* v1.41: `static T X = v;` didesugar menjadi metode static
                 * tanpa parameter X(). Semua akses `Class.X` / `X` di
                 * dalam class diubah menjadi panggilan X(). Tidak ada
                 * storage global; nilai dihitung ulang tiap akses. */
                if (at(';'))
                    die("baris %d: static field '%s' butuh nilai awal — "
                        "tulis `static <tipe> %s = ...;`",
                        mn->line, mn->text, mn->text);
                if (!field_ty_ok(ret) || ty_is_arr(ret))
                    die("baris %d: static field '%s' hanya mendukung "
                        "int/boolean/byte/short/uint8/uint16/long/String",
                        mn->line, mn->text);
                if (class_field(c, xs(mn->text)) ||
                    find_method(c->name, xs(mn->text)))
                    die("baris %d: '%s' dideklarasikan dua kali", mn->line, mn->text);
                advance();                       /* '=' */
                Node *val = expr();
                expect(';', "';' setelah nilai static field");
                Method *sm = xmalloc(sizeof(Method));
                sm->cls = c->name;
                sm->name = xs(mn->text);
                sm->ret = ret;
                sm->is_inst = 0;
                sm->is_constfield = 1;
                Node *rv = newnode(ST_RETURN, mn->line);
                rv->a = val;
                sm->body = rv;
                sm->next = prog.methods;
                prog.methods = sm;
                prog.nmethods++;
                continue;
            }
            if (!field_ty_ok(ret) && !retcls && !ty_is_float(ret))
                die("baris %d: field '%s' bertipe ini belum didukung — "
                    "didukung: int/boolean/byte/short/uint8/uint16/long/String, "
                    "array, dan tipe class%s",
                    mn->line, mn->text, is_struct ? ", double/float" : " (double/float menyusul)");
            if (at('=')) {
                advance();
                die("baris %d: field '%s' tidak boleh diinisialisasi — "
                    "semua field di-zero saat 'new %s()', set nilainya lewat method atau penugasan",
                    mn->line, mn->text, c->name);
            }
            advance();   /* ';' */
            if (class_field(c, xs(mn->text)) ||
                find_constfield(c->name, xs(mn->text)))
                die("baris %d: field '%s' dideklarasikan dua kali", mn->line, mn->text);
            Field *f = xmalloc(sizeof(Field));
            f->name = xs(mn->text);
            f->idx = c->nfields++;
            f->ty = (int)(retcls ? T_INT : ret);
            f->cls = retcls;
            f->next = c->fields;
            c->fields = f;
            continue;
        }
        if (is_struct)
            die("baris %d: struct '%s' hanya berisi field — '%s' terlihat "
                "seperti metode; pakai class bila butuh metode",
                mn->line, c->name, mn->text);
        int is_static = was_static;
        Method *m = xmalloc(sizeof(Method));
        m->cls = c->name;
        m->name = xs(mn->text);
        m->ret = ret;
        m->retcls = retcls;
        m->is_inst = !is_static;
        expect('(', "'(' setelah nama metode");
        Param **pp = &m->params;
        if (m->is_inst) {
            /* metode instance: param pertama implisit `this` (int[] tape) */
            Param *th = xmalloc(sizeof(Param));
            th->name = xs("this");
            th->ty = (int)T_ARI;
            th->cls = c->name;
            *pp = th;
            pp = &th->next;
            m->nparams++;
        }
        if (!at(')')) {
            do {
                Ty pt;
                const char *pcls = 0;
                if (at(T_ID) && tokat(1, '<') && find_class(xs(peek()->text)) &&
                    find_class(xs(peek()->text))->tparam) {
                    const char *base = xs(peek()->text);
                    advance();
                    pcls = parse_generic_ref(base, peek()->line);
                    pt = T_ARI;
                } else if (at(T_ID) && tokat(1, T_ID) && find_class(xs(peek()->text))) {
                    pcls = xs(peek()->text);      /* parameter bertipe class */
                    advance();
                    pt = T_ARI;
                } else {
                    pt = type_param();
                }
                if (pt == T_VOID) die("baris %d: tipe parameter void", peek()->line);
                Tok *pn = peek();
                expect(T_ID, "nama parameter");
                Param *p = xmalloc(sizeof(Param));
                p->name = xs(pn->text);
                p->ty = (int)pt;
                p->cls = pcls;
                *pp = p;
                pp = &p->next;
                m->nparams++;
            } while (at(',') && (advance(), 1));
        }
        expect(')', "')' setelah parameter");
        if (!strcmp(m->name, "main") && m->is_inst)
            die("baris %d: main harus static — metode instance tidak bisa jadi titik masuk", mn->line);
        m->body = stmt_or_block();
        m->next = prog.methods;
        prog.methods = m;
        prog.nmethods++;
    }
    /* factory sintetis untuk constructor: static Kelas __new(args...) {
     *     Kelas t = new Kelas();   (alokasi mentah, zero-init)
     *     t.__ctor(args...);
     *     return t; } — 'new Kelas(args)' → Kelas.__new(args) */
    Method *ctor = is_impl ? 0 : find_method(c->name, "__ctor");
    if (ctor) {
        if (find_method(c->name, "__new"))
            die("class %s: nama '__new' dipakai compiler — ganti nama method", c->name);
        int cln = ctor->body ? ctor->body->line : 0;
        g_linefile = ctor->body ? ctor->body->file : g_linefile;
        Method *nm = xmalloc(sizeof(Method));
        nm->cls = c->name;
        nm->name = xs("__new");
        nm->ret = (int)T_ARI;
        nm->retcls = c->name;
        nm->is_inst = 0;
        Param **pp = &nm->params;
        for (Param *p = ctor->params; p; p = p->next) {
            if (!strcmp(p->name, "this")) continue;
            Param *q = xmalloc(sizeof(Param));
            q->name = p->name;
            q->ty = p->ty;
            q->cls = p->cls;
            *pp = q;
            pp = &q->next;
            nm->nparams++;
        }
        Node *decl = newnode(ST_VAR, cln);
        decl->dty = (int)T_ARI;
        decl->cls = c->name;
        decl->str = xs("t");
        Node *newarr = newnode(EX_NEWARR, cln);   /* alokasi mentah */
        newarr->dty = (int)T_ARI;
        Node *sz = newnode(EX_INT, cln);
        sz->ival = c->nfields > 0 ? c->nfields : 1;
        sz->dty = (int)T_INT;
        sz->typed = 1;
        newarr->a = sz;
        decl->a = newarr;
        Node *call = newnode(EX_CALL, cln);
        char *full = xmalloc(strlen(c->name) + 8);
        sprintf(full, "%s.__ctor", c->name);
        call->str = full;
        Node *tv = newnode(EX_VAR, cln);
        tv->str = xs("t");
        Node *ahead = tv, *atail = tv;
        for (Param *p = nm->params; p; p = p->next) {
            Node *pv = newnode(EX_VAR, cln);
            pv->str = xs(p->name);
            atail->next = pv;
            atail = pv;
        }
        atail->next = 0;
        call->a = ahead;
        call->ival = nm->nparams + 1;
        Node *stc = newnode(ST_EXPR, cln);
        stc->a = call;
        Node *ret = newnode(ST_RETURN, cln);
        ret->a = newnode(EX_VAR, cln);
        ret->a->str = xs("t");
        decl->next = stc;
        stc->next = ret;
        nm->body = decl;
        nm->next = prog.methods;
        prog.methods = nm;
        prog.nmethods++;
    }
    /* v1.46: apply_traits — terapkan default metode trait ke class.
     * Override: jika class sudah punya metode dengan nama yang sama,
     * metode class menang. */
    if (is_impl) {
        /* impl: tambahkan metode instance baru ke class yang sudah ada */
        for (Method **pp = &prog.methods; *pp; pp = &(*pp)->next) {
            if ((*pp)->cls && !strcmp((*pp)->cls, c->name) && !(*pp)->hidden &&
                !(*pp)->is_constfield) {
                /* ini metode dari impl; biarkan saja, sudah terdaftar */
            }
        }
    }

    /* v1.46: terapkan trait — metode trait yang belum ada di class
     * ditambahkan sebagai metode class biasa (override = tulis ulang di
     * badan class). */
    for (int i = 0; i < nt; i++)
        apply_traits(c, trait_list[i]);

    /* v1.51: pisahkan metode template generik dari prog.methods supaya
     * tidak di-resolve / di-codegen (T masih abstrak). Salinannya dibuat
     * per instansiasi oleh instantiate_generic(). */
    if (c->tparam) {
        Method **pp = &prog.methods;
        while (*pp) {
            Method *m = *pp;
            if (m->cls && !strcmp(m->cls, c->name)) {
                *pp = m->next;
                m->next = c->gmethods;
                c->gmethods = m;
                prog.nmethods--;
            } else {
                pp = &m->next;
            }
        }
    }
    g_tvar = 0;
    expect('}', is_struct ? "'}' tutup struct" : "'}' tutup class");
    prog.curcls = 0;
}

