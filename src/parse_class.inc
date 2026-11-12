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

static Field *class_field(Class *c, const char *name) {
    for (Field *f = c->fields; f; f = f->next)
        if (!strcmp(f->name, name)) return f;
    return 0;
}

/* tipe field: 32-bit (slot tunggal) atau referensi 64-bit (long/String/class) */
static int field_ty_ok(Ty t) {
    return t == T_INT || t == T_BOOL || t == T_BYTE ||
           t == T_SHORT || t == T_UINT8 || t == T_UINT16 ||
           t == T_LONG || t == T_STR ||   /* slot fisik 64-bit, v1.14 */
           ty_is_arr(t);                  /* v1.29: field array (pointer 64-bit) */
}

static void parse_class(void) {
    skip_mods();
    expect(KW_CLASS, "'class'");
    Tok *id = peek();
    expect(T_ID, "nama class");
    if (find_class(xs(id->text)))
        die("baris %d: class '%s' dideklarasikan dua kali", id->line, id->text);
    Class *c = xmalloc(sizeof(Class));
    c->name = xs(id->text);
    c->next = prog.classes;
    prog.classes = c;
    prog.curcls = c->name;

    expect('{', "'{' setelah nama class");
    while (!at('}')) {
        int was_static = mods_static();
        Tok *rt = peek();
        /* constructor: NamaKelas(...) — desugar ke metode instance __ctor */
        if (at(T_ID) && !strcmp(xs(rt->text), c->name) && tokat(1, '(')) {
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
        /* field bertipe class: `Titik pojok;` — slot referensi 64-bit */
        if (at(T_ID) && tokat(1, T_ID) && tokat(2, ';') && find_class(xs(rt->text))) {
            const char *fcls = xs(rt->text);
            advance();                       /* nama class */
            Tok *fn = peek();
            expect(T_ID, "nama field");
            advance();                       /* ';' */
            if (was_static)
                die("baris %d: static field belum didukung — pakai method konstanta (static int W())", fn->line);
            if (class_field(c, xs(fn->text)))
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
        Ty ret;
        const char *retcls = 0;
        if (at(T_ID) && tokat(1, T_ID) && tokat(2, '(') && find_class(xs(rt->text))) {
            retcls = xs(rt->text);       /* tipe kembalian class: P make() */
            advance();
            ret = T_ARI;
        } else {
            ret = type_opt();
            if (ret == T_FN)
                die("baris %d: metode dengan return lambda belum didukung — simpan lambda di variabel", rt->line);
        }
        Tok *mn = peek();
        expect(T_ID, "nama metode");
        /* setelah nama: '(' → metode; ';' atau '=' → deklarasi field */
        if (at(';') || at('=')) {
            if (was_static)
                die("baris %d: static field belum didukung — pakai method konstanta (static int W())", mn->line);
            if (!field_ty_ok(ret) && !retcls)
                die("baris %d: field '%s' bertipe ini belum didukung — "
                    "didukung: int/boolean/byte/short/uint8/uint16/long/String, "
                    "array, dan tipe class (double/float menyusul)",
                    mn->line, mn->text);
            if (at('=')) {
                advance();
                die("baris %d: field '%s' tidak boleh diinisialisasi — "
                    "semua field di-zero saat 'new %s()', set nilainya lewat method atau penugasan",
                    mn->line, mn->text, c->name);
            }
            advance();   /* ';' */
            if (class_field(c, xs(mn->text)))
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
                if (at(T_ID) && tokat(1, T_ID) && find_class(xs(peek()->text))) {
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
    Method *ctor = find_method(c->name, "__ctor");
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
    expect('}', "'}' tutup class");
    prog.curcls = 0;
}

