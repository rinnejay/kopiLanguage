/* Kopi parser: token -> AST, lalu resolusi simbol (slot variabel, index metode). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "lex.h"

static void *xmalloc(size_t n) {
    void *p = calloc(1, n);
    if (!p) die("out of memory");
    return p;
}

static char *xs(const char *s) {
    char *p = xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static Tok *ts;
static int ti;

static Tok *peek(void) { g_linefile = ts[ti].file; return &ts[ti]; }
static Tok *peek2(void) { return &ts[ti + 1]; }
static void advance(void) { if (ts[ti].type != T_EOF) ti++; }
static int at(int t) { return ts[ti].type == t; }
static void expect(int t, const char *what) {
    if (!at(t)) die("baris %d: expected %s", peek()->line, what);
    advance();
}

Node *newnode(int kind, int line) {
    Node *n = xmalloc(sizeof(Node));
    n->kind = kind;
    n->line = line;
    n->file = g_linefile;
    return n;
}

static Node *expr(void);
static void resolve(void);
static Node *stmt_single(void);
static Node *block(void);
static int tokat(int d, int t);
static int lambda_ahead(void);
static Method *find_method(const char *cls, const char *name);
static Method *cur_method;

/* ---------- tipe ---------- */

static int is_type_tok(int t) {
    return t == KW_INT || t == KW_BOOL || t == KW_VOID || t == KW_FN ||
           t == KW_STRING || t == KW_LONG || t == KW_DOUBLE || t == KW_FLOAT ||
           t == KW_UINT8 || t == KW_UINT16 || t == KW_UINT32 || t == KW_UINT64 ||
           t == KW_BYTE || t == KW_SHORT || t == KW_INT8 || t == KW_INT16;
}

static Ty type_opt(void) {  /* int | int[] | boolean | void | fn | String ([]) | numerik baru */
    if (at(KW_INT)) {
        advance();
        if (at('[')) { advance(); expect(']', "']' setelah '[' pada int[]"); return T_ARI; }
        return T_INT;
    }
    if (at(KW_LONG)) {
        advance();
        if (at('[')) { advance(); expect(']', "']' setelah '[' pada long[]"); return T_LNGARR; }
        return T_LONG;
    }
    if (at(KW_DOUBLE)) { advance(); return T_DOUBLE; }
    if (at(KW_FLOAT))  { advance(); return T_FLOAT; }
    if (at(KW_UINT8))  { advance(); return T_UINT8; }
    if (at(KW_UINT16)) { advance(); return T_UINT16; }
    if (at(KW_UINT32)) { advance(); return T_UINT32; }
    if (at(KW_UINT64)) { advance(); return T_UINT64; }
    if (at(KW_BYTE) || at(KW_INT8))   { advance(); return T_BYTE; }
    if (at(KW_SHORT) || at(KW_INT16)) { advance(); return T_SHORT; }
    if (at(KW_BOOL)) { advance(); return T_BOOL; }
    if (at(KW_VOID)) { advance(); return T_VOID; }
    if (at(KW_FN)) { advance(); return T_FN; }
    if (at(KW_STRING)) {
        advance();
        /* v1.16: String[] array bertipe slot 64-bit */
        if (at('[')) { advance(); expect(']', "']' pada String[]"); return T_STRARR; }
        return T_STR;
    }
    die("baris %d: tipe tidak dikenal", peek()->line);
    return T_INT;
}

static Ty type_param(void) {
    Ty t = type_opt();
    return t;
}

/* ---------- statement ---------- */

static Node *block(void) {
    expect('{', "'{'");
    Node *head = 0, *tail = 0;
    while (!at('}')) {
        Node *s = stmt_single();
        /* rantai multi-deklarasi (int a = 0, b = 1;) ditautkan dulu */
        while (tail && tail->next) tail = tail->next;
        if (tail) tail->next = s; else head = s;
        tail = s;
    }
    expect('}', "'}'");
    return head;
}

static Node *stmt_or_block(void) {
    if (at('{')) return block();
    return stmt_single();
}

static Node *parse_assignment(int line, int eat_semi) {
    Node *n = newnode(ST_ASSIGN, line);
    n->str = xs(peek()->text);
    advance();
    expect('=', "'=' pada assignment");
    n->a = expr();
    if (eat_semi) expect(';', "';'");
    return n;
}

/* x++ x-- x+=e x-=e x*=e x/=e x%=e  →  desugar ke x = x <op> x.
 * Aman karena di sini nilai kiri adalah variabel sederhana (tanpa
 * efek samping seperti a[i++] yang belum didukung). */
static Node *compound_assign(int line, int eat_semi) {
    Tok *id = peek();
    advance();
    int op = peek()->type;
    advance();
    Node *n = newnode(ST_ASSIGN, line);
    n->str = xs(id->text);
    Node *self = newnode(EX_VAR, line);
    self->str = n->str;
    Node *bin = newnode(EX_BIN, line);
    bin->a = self;
    if (op == T_PP || op == T_MM) {
        bin->str = op == T_PP ? "+" : "-";
        bin->b = newnode(EX_INT, line);
        bin->b->ival = 1;
    } else if (op == T_ANDEQ) {
        bin->str = "&";
        bin->b = expr();
    } else if (op == T_OREQ) {
        bin->str = "|";
        bin->b = expr();
    } else if (op == T_XOREQ) {
        bin->str = "^";
        bin->b = expr();
    } else if (op == T_SHLEQ) {
        bin->str = "<<";
        bin->b = expr();
    } else if (op == T_SHREQ) {
        bin->str = ">>";
        bin->b = expr();
    } else if (op == T_USHREQ) {
        bin->str = ">>>";
        bin->b = expr();
    } else {
        char c = op == T_ADDEQ ? '+' : op == T_SUBEQ ? '-' :
                 op == T_MULEQ ? '*' : op == T_DIVEQ ? '/' : '%';
        char *s = xmalloc(2); s[0] = c; s[1] = 0;
        bin->str = s;
        bin->b = expr();
    }
    n->a = bin;
    n->cp = 1;                    /* penanda: compound assignment (narrowing implisit) */
    if (eat_semi) expect(';', "';'");
    return n;
}

static int is_incuptok(int t) {
    return t == T_PP || t == T_MM || t == T_ADDEQ || t == T_SUBEQ ||
           t == T_MULEQ || t == T_DIVEQ || t == T_MODEQ ||
           t == T_ANDEQ || t == T_OREQ || t == T_XOREQ ||
           t == T_SHLEQ || t == T_SHREQ || t == T_USHREQ;
}

static Node *call_node(int line) {
    /* sudah di posisi nama; bisa "Cls.name(...)" atau "name(...)" */
    const char *first = xs(peek()->text);
    advance();
    const char *cls = prog.curcls, *name = first;
    if (at('.')) {
        advance();
        name = xs(peek()->text);
        expect(T_ID, "nama metode setelah '.'");
        cls = first;
    }
    Node *n = newnode(EX_CALL, line);
    char *full = xmalloc(strlen(cls) + strlen(name) + 2);
    sprintf(full, "%s.%s", cls, name);
    n->str = full;
    expect('(', "'(' setelah nama metode");
    /* argumen */
    Node **argv = xmalloc(sizeof(Node *) * 256);
    int argc = 0;
    if (!at(')')) {
        do {
            if (argc >= 255) die("baris %d: terlalu banyak argumen (maks 8)", peek()->line);
            argv[argc++] = expr();
        } while (at(',') && (advance(), 1));
    }
    expect(')', "')' setelah argumen");
    Node *args = 0, *tail = 0;
    for (int i = 0; i < argc; i++) {
        if (tail) tail->next = argv[i]; else args = argv[i];
        tail = argv[i];
    }
    if (tail) tail->next = 0;
    n->a = args;
    n->ival = argc;   /* sementara: jumlah argumen */
    free(argv);
    return n;
}

static Node *stmt_single(void) {
    Tok *t = peek();
    int line = t->line;

    switch (t->type) {
    case KW_INT: case KW_BOOL: case KW_FN: case KW_LONG: case KW_DOUBLE:
    case KW_FLOAT: case KW_UINT8: case KW_UINT16: case KW_UINT32: case KW_UINT64:
    case KW_BYTE: case KW_SHORT: case KW_INT8: case KW_INT16:
    case KW_STRING: {
        Ty ty = type_opt();
        /* multi-deklarasi: int a = 0, b = 1; → rantai ST_VAR via ->next */
        Node *head = 0, *tailst = 0;
        while (1) {
            Node *n = newnode(ST_VAR, line);
            n->dty = (int)ty;
            n->str = xs(peek()->text);
            expect(T_ID, "nama variabel");
            expect('=', "'=' — variabel lokal wajib diinisialisasi");
            n->a = expr();
            if (tailst) tailst->next = n; else head = n;
            tailst = n;
            if (at(',')) { advance(); continue; }
            break;
        }
        expect(';', "';'");
        return head;
    }
    case KW_IF: {
        Node *n = newnode(ST_IF, line);
        advance();
        expect('(', "'(' setelah if");
        n->a = expr();
        expect(')', "')' setelah kondisi if");
        n->b = at('{') ? block() : stmt_single();
        if (at(KW_ELSE)) { advance(); n->c = at('{') ? block() : stmt_single(); }
        return n;
    }
    case KW_WHILE: {
        Node *n = newnode(ST_WHILE, line);
        advance();
        expect('(', "'(' setelah while");
        n->a = expr();
        expect(')', "')' setelah kondisi while");
        n->b = at('{') ? block() : stmt_single();
        return n;
    }
    case KW_DO: {
        Node *n = newnode(ST_DO, line);
        advance();
        n->b = at('{') ? block() : stmt_single();
        expect(KW_WHILE, "'while' setelah badan do");
        expect('(', "'(' setelah while");
        n->a = expr();
        expect(')', "')' setelah kondisi do-while");
        expect(';', "';' setelah do-while");
        return n;
    }
    case KW_FOR: {
        Node *n = newnode(ST_FOR, line);
        advance();
        expect('(', "'(' setelah for");
        /* init */
        if (is_type_tok(peek()->type)) {
            Node *v = newnode(ST_VAR, line);
            v->dty = (int)type_opt();
            v->str = xs(peek()->text);
            expect(T_ID, "nama variabel");
            expect('=', "'='");
            v->a = expr();
            n->a = v;
        } else if (at(';')) {
            n->a = 0;
        } else {
            n->a = parse_assignment(line, 0);
        }
        expect(';', "';' setelah init for");
        n->b = at(';') ? 0 : expr();
        expect(';', "';' setelah kondisi for");
        /* langkah: assignment / ++ -- / kompon / pemanggilan — tanpa ';' */
        if (!at(')')) {
            if (at(T_ID) && peek2()->type == '=')
                n->c = parse_assignment(peek()->line, 0);
            else if (at(T_ID) && is_incuptok(peek2()->type))
                n->c = compound_assign(peek()->line, 0);
            else if (at(T_ID)) {
                Node *ex = newnode(ST_EXPR, peek()->line);
                ex->a = call_node(peek()->line);
                n->c = ex;
            } else {
                die("baris %d: langkah for tidak dikenal", peek()->line);
            }
        }
        expect(')', "')' setelah for");
        n->d = at('{') ? block() : stmt_single();
        return n;
    }
    case KW_RETURN: {
        Node *n = newnode(ST_RETURN, line);
        advance();
        if (!at(';')) n->a = expr();
        expect(';', "';' setelah return");
        return n;
    }
    case KW_BREAK: { advance(); expect(';', "';'"); Node *n = newnode(ST_BRK, line); return n; }
    case KW_CONTINUE: { advance(); expect(';', "';'"); Node *n = newnode(ST_CONT, line); return n; }
    case '{': {
        Node *n = newnode(ST_BLOCK, line);
        n->d = block();
        return n;
    }
    case ';': { advance(); Node *n = newnode(T_VOID, line); n->kind = ST_EXPR; n->a = 0; return n; }
    }

    /* System.out.println / print */
    if (at(T_ID) && !strcmp(t->text, "System")) {
        advance();
        expect('.', "'.' setelah System");
        expect(T_ID, "'out'");
        expect('.', "'.' setelah out");
        int isln = 0;
        if (at(T_ID) && !strcmp(peek()->text, "println")) { isln = 1; advance(); }
        else if (at(T_ID) && !strcmp(peek()->text, "print")) { advance(); }
        else die("baris %d: System.out.println / print", peek()->line);
        expect('(', "'('");
        Node *n = newnode(ST_PRINT, line);
        n->ival = isln;   /* flag newline */
        if (at(TK_STR) && peek2()->type == ')') {
            n->str = xs(peek()->text);
            advance();
        } else {
            n->a = expr();
        }
        expect(')', "')' setelah pernyataan print");
        expect(';', "';'");
        return n;
    }

    /* deklarasi objek class: P p = new P(); / P p = metode(); */
    if (t->type == T_ID && tokat(1, T_ID) && tokat(2, '=') && !lambda_ahead()) {
        Node *n = newnode(ST_VAR, line);
        n->dty = (int)T_ARI;
        n->cls = xs(t->text);          /* divalidasi saat resolusi */
        advance();                     /* nama class */
        n->str = xs(peek()->text);
        expect(T_ID, "nama variabel");
        expect('=', "'=' — variabel lokal wajib diinisialisasi");
        n->a = expr();
        expect(';', "';'");
        return n;
    }

    /* assignment, a[i] = v, ++/+= kompon, atau call statement */
    if (t->type == T_ID) {
        int t2 = peek2()->type;
        if (t2 == '=') return parse_assignment(line, 1);
        /* p.field = v; / p.field1.field2 = v; — penugasan field instance */
        if (t2 == '.' && tokat(2, T_ID)) {
            int isasg = 0, nfdot = 2;
            if (tokat(3, '=')) isasg = 1;
            else if (tokat(3, '.') && tokat(4, T_ID) && tokat(5, '=')) { isasg = 1; nfdot = 3; }
            else if (tokat(3, '.') && tokat(4, T_ID) && tokat(5, '('))
                die("baris %d: pemanggilan method pada field belum didukung — "
                    "simpan field ke variabel dulu: Tipe t = p.%s; lalu t.%s(...)",
                    line, ts[ti + 2].text, ts[ti + 4].text);
            if (isasg) {
                Node *s = newnode(ST_FLDASG, line);
                Node *obj = newnode(EX_VAR, line);
                obj->str = xs(t->text);
                advance();                 /* nama objek */
                advance();                 /* '.' */
                const char *f1 = xs(peek()->text);
                expect(T_ID, "nama field");
                if (nfdot == 2) {
                    /* p.field = v */
                    s->a = obj;
                    s->str = f1;
                } else {
                    /* p.field1.field2 = v — base = p.field1 */
                    expect('.', "'.' pada p.field1.field2");
                    const char *f2 = xs(peek()->text);
                    expect(T_ID, "nama field kedua");
                    Node *base = newnode(EX_FLD, line);
                    base->a = obj;
                    base->str = f1;
                    s->a = base;
                    s->str = f2;
                }
                expect('=', "'=' pada obj.field = v");
                s->c = expr();
                expect(';', "';' setelah obj.field = v");
                return s;
            }
        }
        if (t2 == '[') {
            /* a[i] = v; (indeks di sisi kiri) */
            Node *s = newnode(ST_IDXASG, line);
            Node *arr = newnode(EX_VAR, line);
            arr->str = xs(t->text);
            advance();
            expect('[', "'[' pada a[i]");
            s->a = arr;
            s->b = expr();
            expect(']', "']' pada a[i]");
            expect('=', "'=' pada a[i] = v");
            s->c = expr();
            expect(';', "';' setelah a[i] = v");
            return s;
        }
        if (is_incuptok(t2)) return compound_assign(line, 1);
        Node *ex = newnode(ST_EXPR, line);
        ex->a = call_node(line);
        expect(';', "';' setelah pemanggilan metode");
        return ex;
    }
    if (t->type == T_PP || t->type == T_MM)
        die("baris %d: bentuk '++x' belum didukung — pakai x = x + 1 atau x++", line);
    die("baris %d: statement tidak dikenal", line);
    return 0;
}

/* ---------- class / method ---------- */

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
           t == T_LONG || t == T_STR;   /* slot fisik 64-bit, v1.14 */
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
                    "didukung: int/boolean/byte/short/uint8/uint16/long/String "
                    "dan tipe class (double/float menyusul)",
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

static int tokat(int d, int t) { return ts[ti + d].type == t; }

/* deteksi lambda di posisi ekspresi:  x => ..  |  (x, y) => ..  |  () => .. */
static int lambda_ahead(void) {
    if (at(T_ID)) return tokat(1, T_ARROW);
    if (!at('(')) return 0;
    int j = 1;
    if (tokat(j, ')')) return tokat(j + 1, T_ARROW);
    if (!tokat(j, T_ID)) return 0;
    for (;;) {
        j++;
        if (tokat(j, ')')) return tokat(j + 1, T_ARROW);
        if (tokat(j, ',') && tokat(j + 1, T_ID)) { j++; continue; }
        return 0;
    }
}

/* sudut sudah di awal lambda (id atau '(') */
static Node *parse_lambda(int line) {
    Node *n = newnode(EX_ARROW, line);
    Node **pl = &n->c;
    if (at('(')) {
        advance();
        while (!at(')')) {
            Tok *p = peek();
            expect(T_ID, "parameter lambda");
            Node *pn = newnode(EX_VAR, p->line);
            pn->str = xs(p->text);
            pn->next = 0;
            *pl = pn; pl = &pn->next;
            if (at(',')) advance(); else break;
        }
        expect(')', "')' parameter lambda");
    } else {
        Tok *p = peek();
        expect(T_ID, "parameter lambda");
        Node *pn = newnode(EX_VAR, p->line);
        pn->str = xs(p->text);
        pn->next = 0;
        *pl = pn; pl = &pn->next;
    }
    expect(T_ARROW, "'=>' setelah parameter lambda");
    if (at('{')) n->d = block();   /* badan pernyataan */
    else n->a = expr();            /* badan ekspresi */
    return n;
}

/* ---------- ekspresi (precedence climbing) ---------- */

/* ── literal JSON: json { ... } / json [ ... ] (v1.6.4) ─────────────
 * Didesugar menjadi pemanggilan Json.objN/arrN/str/num/bool/nullV.
 * Batas: maksimal 8 entri per objek/array dalam satu literal. */

static int cp_push(const char *s);

static Node *mk_netbytes(const char *text, int line) {
    Node *n = newnode(EX_NET, line);
    n->ival = 5;
    n->str = xs(text);        /* cpool diisi nanti saat type_expr */
    n->dty = (int)T_ARI;
    return n;
}

static Node *mk_call(const char *name, Node **args, int nargs, int line) {
    Node *c = newnode(EX_CALL, line);
    c->str = xs(name);
    c->ival = nargs;
    Node *head = 0, *tail = 0;
    for (int i = 0; i < nargs; i++) {
        if (tail) tail->next = args[i]; else head = args[i];
        tail = args[i];
    }
    if (tail) tail->next = 0;
    c->a = head;
    return c;
}

static Node *mk_call1(const char *name, Node *a, int line) {
    Node *args[1]; args[0] = a;
    return mk_call(name, args, 1, line);
}

static Node *json_value(void);

static Node *json_string_node(void) {
    Tok *t = peek();
    expect(TK_STR, "string JSON");
    return mk_call1("Json.str", mk_netbytes(t->text, t->line), t->line);
}

static Node *json_number_node(void) {
    int line = peek()->line;
    int neg = 0;
    if (at('-')) { neg = 1; advance(); }
    if (!at(T_NUM)) die("baris %d: angka JSON tidak valid", line);
    Tok *t = peek();
    if (t->numty != T_INT && t->numty != T_DOUBLE) {
        /* long/uint/float literal: pakai teks aslinya juga */
    }
    char buf[512];
    if (neg) snprintf(buf, sizeof buf, "-%s", t->text ? t->text : "0");
    else snprintf(buf, sizeof buf, "%s", t->text ? t->text : "0");
    advance();
    return mk_call1("Json.num", mk_netbytes(buf, line), line);
}

static Node *json_value(void) {
    Tok *t = peek();
    int line = t->line;
    if (at('{')) {
        advance();
        Node *args[16];
        int n = 0;
        if (!at('}')) {
            do {
                if (n / 2 >= 8)
                    die("baris %d: literal JSON objek maksimal 8 pasangan", line);
                Tok *k = peek();
                expect(TK_STR, "kunci string JSON");
                args[n] = mk_netbytes(k->text, k->line);
                n++;
                expect(':', "':' setelah kunci JSON");
                args[n] = json_value();
                n++;
                if (at(',')) { advance(); continue; }
                break;
            } while (1);
        }
        expect('}', "'}' penutup objek JSON");
        int pairs = n / 2;
        if (pairs <= 4) {
            char name[32];
            snprintf(name, sizeof name, "Json.obj%d", pairs);
            return mk_call(name, args, n, line);
        }
        /* >4 pasangan: bangun obj4 lalu Json.put untuk sisanya */
        Node *cur = mk_call("Json.obj4", args, 8, line);
        for (int q = 4; q < pairs; q++) {
            Node *pa[3];
            pa[0] = cur;
            pa[1] = args[q * 2];
            pa[2] = args[q * 2 + 1];
            cur = mk_call("Json.put", pa, 3, line);
        }
        return cur;
    }
    if (at('[')) {
        advance();
        Node *args[8];
        int n = 0;
        if (!at(']')) {
            do {
                if (n >= 8)
                    die("baris %d: literal JSON array maksimal 8 elemen", line);
                args[n] = json_value();
                n++;
                if (at(',')) { advance(); continue; }
                break;
            } while (1);
        }
        expect(']', "']' penutup array JSON");
        char name[32];
        snprintf(name, sizeof name, "Json.arr%d", n);
        return mk_call(name, args, n, line);
    }
    if (at(TK_STR)) return json_string_node();
    if (at(T_NUM) || at('-')) return json_number_node();
    if (at(KW_TRUE))  { advance(); Node *a[1]; a[0] = newnode(EX_INT, line); a[0]->ival = 1; a[0]->dty = T_INT;
                        return mk_call1("Json.boolV", a[0], line); }
    if (at(KW_FALSE)) { advance(); Node *a[1]; a[0] = newnode(EX_INT, line); a[0]->ival = 0; a[0]->dty = T_INT;
                        return mk_call1("Json.boolV", a[0], line); }
    if (at(T_ID) && !strcmp(xs(peek()->text), "null")) {
        advance();
        return mk_call("Json.nullV", 0, 0, line);
    }
    die("baris %d: nilai JSON tidak dikenal (pakai null/true/false/angka/string/{}/[])", line);
    return 0;
}

/* ── template string `...{ekspresi}...` (v1.7.1) ────────────────── */

static int tstr_close(const char *s, int start) {
    int depth = 1;
    int i = start;
    while (s[i]) {
        char c = s[i];
        if (c == '\\') { i += 2; continue; }
        if (c == '\'' || c == '"' || c == '`') {
            char q = c;
            i++;
            while (s[i] && s[i] != q) {
                if (s[i] == '\\' && s[i + 1]) i++;
                i++;
            }
            if (s[i]) i++;
            continue;
        }
        if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) return i;
        }
        i++;
    }
    return -1;
}

static void tstr_append(Node **head, Node **tail, Node *n) {
    n->next = 0;
    if (*tail) (*tail)->next = n; else *head = n;
    *tail = n;
}

static Ty desugar_net2(Node *e, int bi, Node *a, Node *b, Ty ret) {
    Node *nn = newnode(EX_NET, e->line);
    nn->ival = bi;
    nn->dty = (int)ret;
    a->next = b;
    b->next = 0;
    nn->a = a;
    Node *savenext = e->next;
    *e = *nn;
    e->next = savenext;
    e->dty = (int)ret;
    return ret;
}

static Node *parse_tstr(const char *raw, int line) {
    int n = (int)strlen(raw);
    char *lit = xmalloc((size_t)n + 1);
    int ln = 0;
    Node *head = 0, *tail = 0;
    int i = 0;
    while (i < n) {
        char c = raw[i];
        if (c == '\\' && i + 1 < n) {
            char e = raw[i + 1];
            if (e == 'n') lit[ln] = 10;
            else if (e == 't') lit[ln] = 9;
            else if (e == 'r') lit[ln] = 13;
            else if (e == '0') die("baris %d: template: escape \\0 tidak didukung", line);
            else lit[ln] = e;
            ln++;
            i += 2;
            continue;
        }
        if (c == '{' && i + 1 < n && raw[i + 1] == '{') {
            lit[ln] = '{'; ln++; i += 2; continue;
        }
        if (c == '}' && i + 1 < n && raw[i + 1] == '}') {
            lit[ln] = '}'; ln++; i += 2; continue;
        }
        if (c == '{') {
            if (ln > 0) {
                lit[ln] = 0;
                tstr_append(&head, &tail, mk_netbytes(lit, line));
                ln = 0;
            }
            int close = tstr_close(raw, i + 1);
            if (close < 0)
                die("baris %d: template: '}' penutup ekspresi tidak ditemukan", line);
            int sublen = close - i - 1;
            char *sub = xmalloc((size_t)sublen + 1);
            memcpy(sub, raw + i + 1, (size_t)sublen);
            sub[sublen] = 0;
            int nt = 0;
            Tok *tt = lex_text(sub, "<template>", line, &nt);
            Tok *savets = ts;
            int saveti = ti;
            ts = tt;
            ti = 0;
            Node *ex = expr();
            if (!at(T_EOF))
                die("baris %d: template: ekspresi '{...}' tidak valid", line);
            ts = savets;
            ti = saveti;
            tstr_append(&head, &tail, ex);
            i = close + 1;
            continue;
        }
        lit[ln] = c;
        ln++;
        i++;
    }
    if (ln > 0) {
        lit[ln] = 0;
        tstr_append(&head, &tail, mk_netbytes(lit, line));
    }
    if (head == 0) return mk_netbytes("", line);
    if (head->next == 0 && head->kind == EX_NET) return head;
    Node *node = newnode(EX_TSTR, line);
    node->a = head;
    return node;
}

static Node *postfix(void);   /* deklarasi maju: operand cast */

static Node *primary(void) {
    Tok *t = peek();
    int line = t->line;
    if (at(T_NUM)) {
        Node *n = newnode(EX_INT, line);
        n->ival = t->num;
        n->fval = t->fnum;
        n->dty = t->numty;
        advance();
        return n;
    }
    if (at(TK_TSTR)) {
        const char *raw = xs(peek()->text);
        advance();
        return parse_tstr(raw, line);
    }
    if (at(KW_JSON)) {
        advance();
        if (!at('{') && !at('['))
            die("baris %d: setelah 'json' butuh objek '{...}' atau array '[...]'", line);
        return json_value();
    }
    /* cast eksplisit gaya Java: (tipe) unary — tipe numerik saja */
    if (at('(') && is_type_tok(ts[ti + 1].type)) {
        advance();
        Ty ct = type_opt();
        expect(')', "')' setelah tipe pada cast");
        if (!ty_is_num(ct))
            die("baris %d: cast hanya untuk tipe numerik", line);
        Node *n = newnode(EX_CAST, line);
        n->dty = (int)ct;
        n->a = postfix();
        return n;
    }
    if (at(KW_TRUE)) { Node *n = newnode(EX_BOOL, line); n->ival = 1; advance(); return n; }
    if (at(KW_FALSE)) { Node *n = newnode(EX_BOOL, line); n->ival = 0; advance(); return n; }
    if (lambda_ahead()) return parse_lambda(line);
    if (at('(')) { advance(); Node *n = expr(); expect(')', "')'"); return n; }
    if (at('-')) {
        /* angka negatif langsung dianggap unary */
        Node *n = newnode(EX_UN, line);
        n->str = "-";
        advance();
        n->a = primary();
        return n;
    }
    if (at('!')) {
        Node *n = newnode(EX_UN, line);
        n->str = "!";
        advance();
        n->a = primary();
        return n;
    }
    if (at('~')) {                    /* bitwise complement (Java) */
        Node *n = newnode(EX_UN, line);
        n->str = "~";
        advance();
        n->a = primary();
        return n;
    }
    if (at(TK_STR)) {
        Node *n = newnode(EX_STRLIT, line);
        n->str = xs(t->text);
        advance();
        return n;
    }
    if (at(T_ID) && !strcmp(xs(t->text), "new") &&
        (peek2()->type == KW_INT || peek2()->type == KW_LONG || peek2()->type == KW_STRING)) {
        Ty aty;
        if (peek2()->type == KW_INT)         aty = T_ARI;
        else if (peek2()->type == KW_LONG)   aty = T_LNGARR;
        else                                 aty = T_STRARR;
        advance();                       /* new */
        advance();                       /* int/long/String */
        expect('[', "'[' setelah 'new <tipe>'");
        Node *n = newnode(EX_NEWARR, line);
        n->dty = (int)aty;
        n->a = expr();
        expect(']', "']' pada new <tipe>[n]");
        return n;
    }
    /* new Kelas() / new Kelas(args) → EX_NEWOBJ (didesugar saat resolusi) */
    if (at(T_ID) && !strcmp(xs(t->text), "new") &&
        peek2()->type == T_ID && ts[ti + 2].type == '(') {
        advance();                       /* new */
        const char *cn = xs(peek()->text);
        advance();
        expect('(', "'(' setelah nama class");
        Node *args = 0, *tail = 0;
        int narg = 0;
        if (!at(')')) {
            do {
                Node *a = expr();
                if (tail) tail->next = a; else args = a;
                tail = a;
                narg++;
            } while (at(',') && (advance(), 1));
        }
        if (tail) tail->next = 0;
        expect(')', "')' setelah nama class");
        Node *n = newnode(EX_NEWOBJ, line);
        n->str = xs(cn);
        n->a = args;
        n->ival = narg;
        return n;
    }
    if (at(T_ID)) {
        /* panggilan metode / Class.metode(...): nama berlambang (. lalu id lalu '(') */
        if (peek2()->type == '(' ||
            (peek2()->type == '.' && ts[ti + 2].type == T_ID && ts[ti + 3].type == '('))
            return call_node(line);
        Node *n = newnode(EX_VAR, line);
        n->str = xs(t->text);
        advance();
        return n;   /* postfix (a[i], a.length) ditangani pemanggil */
    }
    die("baris %d: ekspresi tidak dikenal", line);
    return 0;
}

/* postfix: primary lalu beruntun a[i] dan a.length */
static Node *postfix(void) {
    Node *l = primary();
    for (;;) {
        if (at('.')) {
            if (peek2()->type != T_ID)
                die("baris %d: setelah '.' butuh nama field/metode", l->line);
            if (!strcmp(xs(peek2()->text), "length")) {
                advance(); advance();
                Node *n = newnode(EX_LEN, l->line);
                n->a = l;
                l = n;
                break;
            }
            if (ts[ti + 2].type != '(') {
                /* akses field instance: obj.field (didesugar EX_IDX saat resolusi) */
                advance();                       /* '.' */
                const char *fname = xs(peek()->text);
                advance();                       /* nama field */
                Node *n = newnode(EX_FLD, l->line);
                n->a = l;
                n->str = fname;
                l = n;
                continue;
            }
            /* gula: a.forEach(f, ...) → Arrays.forEach(a, f, ...) */
            advance();                              /* '.' */
            const char *mname = xs(peek()->text);
            advance();                              /* nama metode */
            Node *call = newnode(EX_CALL, l->line);
            char *full = xmalloc(strlen("Arrays.") + strlen(mname) + 1);
            sprintf(full, "Arrays.%s", mname);
            call->str = full;
            expect('(', "'(' setelah nama metode array");
            Node *args = l;
            Node *tail = l;
            int argc = 1;
            if (!at(')')) {
                do {
                    Node *a = expr();
                    tail->next = a;
                    tail = a;
                    argc++;
                } while (at(',') && (advance(), 1));
            }
            expect(')', "')' setelah argumen metode array");
            tail->next = 0;
            call->a = args;
            call->ival = argc;
            l = call;
            continue;
        }
        if (at('[')) {
            advance();
            Node *n = newnode(EX_IDX, l->line);
            n->a = l;
            n->b = expr();
            expect(']', "']' pada a[i]");
            l = n;
            continue;
        }
        break;
    }
    return l;
}

static void mul_chain_cleanup_placeholder(void) { }

static Node *mul_chain(void) {
    Node *l = postfix();
    while (at('*') || at('/') || at('%')) {
        Node *n = newnode(EX_BIN, peek()->line);
        char op = (char)ts[ti].type;
        char *s = xmalloc(2); s[0] = op; s[1] = 0;
        n->str = s;
        n->a = l;
        advance();
        n->b = postfix();
        l = n;
    }
    return l;
}

static Node *add_chain(void) {
    Node *l = mul_chain();
    while (at('+') || at('-')) {
        Node *n = newnode(EX_BIN, peek()->line);
        char op = (char)ts[ti].type;
        char *s = xmalloc(2); s[0] = op; s[1] = 0;
        n->str = s;
        n->a = l;
        advance();
        n->b = mul_chain();
        l = n;
    }
    return l;
}

static Node *shift_chain(void) {
    Node *l = add_chain();
    while (at(T_SHL) || at(T_SHR) || at(T_USHR)) {
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = at(T_SHL) ? "<<" : at(T_SHR) ? ">>" : ">>>";
        n->a = l;
        advance();
        n->b = add_chain();
        l = n;
    }
    return l;
}

static Node *rel_chain(void) {
    Node *l = shift_chain();
    while (at('<') || at('>') || at(T_LE) || at(T_GE)) {
        Node *n = newnode(EX_BIN, peek()->line);
        const char *s = "?";
        if (at('<')) s = "<"; else if (at('>')) s = ">";
        else if (at(T_LE)) s = "<="; else s = ">=";
        n->str = s;
        n->a = l;
        advance();
        n->b = shift_chain();
        l = n;
    }
    return l;
}

static Node *eq_chain(void) {
    Node *l = rel_chain();
    while (at(T_EQ) || at(T_NE)) {
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = at(T_EQ) ? "==" : "!=";
        n->a = l;
        advance();
        n->b = rel_chain();
        l = n;
    }
    return l;
}

/* bitwise & : presedensi antara ^ dan == (konvensi Java) */
static Node *band_chain(void) {
    Node *l = eq_chain();
    while (at('&')) {                              /* '&' 1-char ASCII 38 */
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = "&";
        n->a = l;
        advance();
        n->b = eq_chain();
        l = n;
    }
    return l;
}

/* bitwise ^ */
static Node *bxor_chain(void) {
    Node *l = band_chain();
    while (at('^')) {
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = "^";
        n->a = l;
        advance();
        n->b = band_chain();
        l = n;
    }
    return l;
}

/* bitwise | */
static Node *bor_chain(void) {
    Node *l = bxor_chain();
    while (at('|')) {                              /* '|' 1-char ASCII 124 */
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = "|";
        n->a = l;
        advance();
        n->b = bxor_chain();
        l = n;
    }
    return l;
}

static Node *and_chain(void) {
    Node *l = bor_chain();
    while (at(T_AND)) {
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = "&&";
        n->a = l;
        advance();
        n->b = bor_chain();
        l = n;
    }
    return l;
}

static Node *expr(void) {
    Node *l = and_chain();
    while (at(T_OR)) {
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = "||";
        n->a = l;
        advance();
        n->b = and_chain();
        l = n;
    }
    if (at('?')) {
        Node *n = newnode(EX_TERN, peek()->line);
        n->a = l;
        advance();
        n->b = expr();
        expect(':', "':' pada ekspresi ternary a ? b : c");
        n->c = expr();
        return n;
    }
    return l;
}

/* ---------- resolusi simbol ---------- */

typedef struct { const char *name; int slot; Ty ty; const char *cls; } SVar;

static SVar scope[1024];
static int nscope;
static int nextslot;
static int framebase;      /* batas frame metode aktif; scope di bawah ini
                              milik metode luar → ambil jadi capture */

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
           !strcmp(n, "indexOf") || !strcmp(n, "lastIndexOf") ||
           !strcmp(n, "contains") || !strcmp(n, "startsWith") ||
           !strcmp(n, "endsWith") || !strcmp(n, "trim") ||
           !strcmp(n, "trimLeft") || !strcmp(n, "trimRight") ||
           !strcmp(n, "upper") || !strcmp(n, "lower") ||
           !strcmp(n, "repeat") || !strcmp(n, "equals") ||
           !strcmp(n, "equalsIgnoreCase") || !strcmp(n, "isEmpty") ||
           !strcmp(n, "toInt") || !strcmp(n, "utf8Count") ||
           !strcmp(n, "replaceSeq") || !strcmp(n, "countByte") ||
           !strcmp(n, "padLeft") || !strcmp(n, "padRight");
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

static Ty type_expr_raw(Node *e) {
    g_linefile = e->file;
    switch (e->kind) {
    case EX_INT:  return (Ty)e->dty;      /* tipe literal ditentukan lexer */
    case EX_BOOL: return T_BOOL;
    case EX_STRLIT:
        if (e->cp == 0 && e->str) e->cp = cp_push(e->str);
        return T_STR;
    case EX_NET: {
        if ((e->ival == 5 || e->ival == 6) && e->str) {
            e->cp = cp_push(e->str);
            e->str = 0;
        }
        if (e->ival == 5 || e->ival == 6) return (Ty)e->dty;
        if (e->ival == 22 || e->ival == 23 || e->ival == 24) {
            for (Node *aa = e->a; aa; aa = aa->next) type_expr(aa);
            return (Ty)e->dty;
        }
        for (Node *aa = e->a; aa; aa = aa->next) type_expr(aa);
        return T_INT;
    }
    case EX_TSTR: {
        Node *acc = 0;
        Node *part = e->a;
        while (part) {
            Node *next = part->next;
            part->next = 0;
            Ty pt = type_expr(part);
            Node *as = part;
            if (pt != T_ARI && pt != T_STR) {
                const char *fn = 0;
                if (pt == T_BOOL) fn = "boolText";
                else if (pt == T_INT || pt == T_BYTE || pt == T_SHORT) fn = "fromInt";
                else if (pt == T_UINT8 || pt == T_UINT16 || pt == T_UINT32) fn = "fromUInt";
                else if (pt == T_LONG) fn = "fromLong";
                else if (pt == T_UINT64) fn = "fromULong";
                else if (pt == T_FLOAT || pt == T_DOUBLE) fn = "fromDouble";
                if (!fn)
                    die("baris %d: template tidak mendukung tipe ini", e->line);
                if (!find_method("Str", fn))
                    die("baris %d: template butuh 'import Str;' untuk format angka/boolean", e->line);
                char fname[64];
                snprintf(fname, sizeof fname, "Str.%s", fn);
                as = mk_call1(fname, part, e->line);
            }
            if (acc == 0) {
                acc = as;
            } else {
                if (!find_method("Str", "cat"))
                    die("baris %d: template butuh 'import Str;' untuk penggabungan", e->line);
                Node *pair[2];
                pair[0] = acc;
                pair[1] = as;
                acc = mk_call("Str.cat", pair, 2, e->line);
            }
            part = next;
        }
        if (acc == 0) acc = mk_netbytes("", e->line);
        Node *savenext = e->next;
        int saveline = e->line;
        const char *savefile = e->file;
        Ty rt = type_expr(acc);
        *e = *acc;
        e->next = savenext;
        e->line = saveline;
        e->file = savefile;
        e->dty = (int)rt;
        return rt;
    }
    case EX_CAST: {
        Ty from = type_expr(e->a);
        Ty to = (Ty)e->dty;
        if (!ty_is_num(from) || !ty_is_num(to))
            die("baris %d: cast hanya antar tipe numerik", e->line);
        return to;
    }
    case EX_VAR: { SVar *v = resolve_name(e->str, e->line); e->ival = v->slot; e->dty = (int)v->ty; return v->ty; }
    case EX_UN: {
        Ty t = type_expr(e->a);
        if (e->str[0] == '!') {
            if (t != T_BOOL)
                die("baris %d: '!' butuh operand boolean", e->line);
            return T_BOOL;
        }
        if (e->str[0] == '~') {
            if (!ty_is_int(t))
                die("baris %d: '~' butuh operand integer", e->line);
            return t;
        }
        if (!ty_is_num(t))
            die("baris %d: '-' butuh operand numerik", e->line);
        return t;
    }
    case EX_TERN: {
        Ty ct = type_expr(e->a);
        if (ct != T_BOOL) die("baris %d: kondisi ternary harus boolean", e->line);
        Ty t1 = type_expr(e->b);
        Ty t2 = type_expr(e->c);
        Ty r = t1;
        if (t1 == t2) {
            r = t1;
        } else if ((t1 == T_STR || t1 == T_ARI) && (t2 == T_STR || t2 == T_ARI)) {
            r = T_STR;
        } else {
            die("baris %d: cabang ternary harus bertipe sama (pakai cast)", e->line);
        }
        e->dty = (int)r;
        return r;
    }
    case EX_BIN: {
        Ty lt = type_expr(e->a), rt = type_expr(e->b);
        char op = e->str[0];
        int two = e->str[1] != 0;
        if (lt == T_STR || rt == T_STR) {
            int ls = (lt == T_STR || lt == T_ARI);
            int rs = (rt == T_STR || rt == T_ARI);
            if (!ls || !rs)
                die("baris %d: operasi String hanya dengan String/teks", e->line);
            if (op == '+' && !two)
                return desugar_net2(e, 22, e->a, e->b, T_STR);
            if ((op == '=' || op == '!') && two)
                return desugar_net2(e, op == '=' ? 23 : 24, e->a, e->b, T_BOOL);
            die("baris %d: operator '%s' tidak didukung untuk String", e->line, e->str);
        }
        if ((op == '&' || op == '|') && two) {      /* "&&" / "||" saja */
            if (lt != T_BOOL || rt != T_BOOL)
                die("baris %d: '&&' / '||' butuh operand boolean", e->line);
            return T_BOOL;
        }
        int isShift = !strcmp(e->str, "<<") || !strcmp(e->str, ">>") ||
                      !strcmp(e->str, ">>>");
        if (isShift) {
            if (!ty_is_int(lt) || !ty_is_int(rt))
                die("baris %d: operator '%s' butuh operand integer", e->line, e->str);
            return lt;
        }
        if ((op == '=' || op == '!') && two) {      /* == != */
            if (lt == T_BOOL && rt == T_BOOL) return T_BOOL;
            /* identitas objek: bandingkan referensi (pointer), ala Java */
            if (ty_is_arr(lt) && lt == rt) return T_BOOL;
            if (!ty_is_num(lt) || !ty_is_num(rt))
                die("baris %d: '==' / '!=' butuh operand numerik/boolean", e->line);
            Ty p = ty_promote(lt, rt);
            if (!ty_can_implicit(p, lt) || !ty_can_implicit(p, rt))
                die("baris %d: operand '==' / '!=' perlu cast eksplisit", e->line);
            return T_BOOL;
        }
        if (op == '<' || op == '>' || !strcmp(e->str, "<=") || !strcmp(e->str, ">=")) {
            if (!ty_is_num(lt) || !ty_is_num(rt))
                die("baris %d: tanda pembanding butuh numerik", e->line);
            Ty p = ty_promote(lt, rt);
            if (!ty_can_implicit(p, lt) || !ty_can_implicit(p, rt))
                die("baris %d: operand pembanding perlu cast eksplisit", e->line);
            return T_BOOL;
        }
        if (op == '&' || op == '|' || op == '^') {
            if (!ty_is_int(lt) || !ty_is_int(rt))
                die("baris %d: operator '%s' butuh operand integer", e->line, e->str);
            Ty p = ty_promote(lt, rt);
            if (!ty_can_implicit(p, lt) || !ty_can_implicit(p, rt))
                die("baris %d: operand '%s' perlu cast eksplisit", e->line, e->str);
            return p;
        }
        if (op == '+' || op == '-' || op == '*' || op == '/' || op == '%') {
            if (!ty_is_num(lt) || !ty_is_num(rt))
                die("baris %d: operator '%s' butuh operand numerik", e->line, e->str);
            Ty p = ty_promote(lt, rt);
            if (!ty_can_implicit(p, lt) || !ty_can_implicit(p, rt))
                die("baris %d: operand '%s' perlu cast eksplisit", e->line, e->str);
            return p;
        }
        die("baris %d: operator '%s' tidak didukung", e->line, e->str);
        return T_INT;
    }
    case EX_CALL: {
        char buf[512];
        const char *dot = strchr(e->str, '.');
        memcpy(buf, e->str, (size_t)(dot - e->str)); buf[dot - e->str] = 0;
        /* namespace builtin Net.*: syscall native (socket/connect/read/write/close)
         * + bytes("literal") → array byte-as-int, dikonversi compile-time */
        if (!strcmp(buf, "Net")) {
            const char *fn = dot + 1;
            int bi;
            int want = 0;
            if      (!strcmp(fn, "socket"))  { bi = 0; want = 2; }
            else if (!strcmp(fn, "connect")) { bi = 1; want = 3; }
            else if (!strcmp(fn, "rdb"))     { bi = 2; want = 1; }
            else if (!strcmp(fn, "wrb"))     { bi = 3; want = 2; }
            else if (!strcmp(fn, "close"))   { bi = 4; want = 1; }
            else if (!strcmp(fn, "bytes"))   { bi = 5; want = 1; }
            else if (!strcmp(fn, "openr"))   { bi = 6; want = 1; }
            else if (!strcmp(fn, "recvb"))   { bi = 7; want = 2; }
            else if (!strcmp(fn, "sendb"))   { bi = 8; want = 2; }
            else if (!strcmp(fn, "bind"))    { bi = 9; want = 3; }
            else if (!strcmp(fn, "sendto"))  { bi = 10; want = 4; }
            else if (!strcmp(fn, "recvfrom")){ bi = 11; want = 3; }
            else if (!strcmp(fn, "shutdown")){ bi = 12; want = 2; }
            else if (!strcmp(fn, "openb"))   { bi = 13; want = 3; }
            else if (!strcmp(fn, "mkdirb"))  { bi = 14; want = 1; }
            else if (!strcmp(fn, "unlinkb")) { bi = 15; want = 1; }
            else if (!strcmp(fn, "renameb")) { bi = 16; want = 2; }
            else if (!strcmp(fn, "seek"))    { bi = 17; want = 3; }
            else if (!strcmp(fn, "time"))    { bi = 18; want = 0; }
            else if (!strcmp(fn, "mono"))    { bi = 19; want = 0; }
            else if (!strcmp(fn, "sleep"))   { bi = 20; want = 1; }
            else if (!strcmp(fn, "exit"))    { bi = 21; want = 1; }
            else if (!strcmp(fn, "cat"))     { bi = 22; want = 2; }
            else if (!strcmp(fn, "sequal"))  { bi = 23; want = 2; }
            else if (!strcmp(fn, "sne"))     { bi = 24; want = 2; }
            else if (!strcmp(fn, "listen"))  { bi = 25; want = 2; }
            else if (!strcmp(fn, "accept"))  { bi = 26; want = 1; }
            else if (!strcmp(fn, "setsockopt")) { bi = 27; want = 4; }
            else if (!strcmp(fn, "memmark") || !strcmp(fn, "memMark")) { bi = 28; want = 0; }
            else if (!strcmp(fn, "memreset") || !strcmp(fn, "memReset")){ bi = 29; want = 1; }
            else if (!strcmp(fn, "argc"))    { bi = 30; want = 0; }
            else if (!strcmp(fn, "argv"))    { bi = 31; want = 1; }
            else if (!strcmp(fn, "getenv"))  { bi = 32; want = 1; }
            else if (!strcmp(fn, "recv1"))   { bi = 33; want = 2; }
            else if (!strcmp(fn, "setnonblock") || !strcmp(fn, "setNonblock")) { bi = 34; want = 1; }
            else if (!strcmp(fn, "key"))     { bi = 35; want = 0; }
            else if (!strcmp(fn, "ttyRaw") || !strcmp(fn, "ttyraw")) { bi = 36; want = 1; }
            else if (!strcmp(fn, "localport")) { bi = 37; want = 1; }
            else if (!strcmp(fn, "settimeout")) { bi = 38; want = 2; }
            else if (!strcmp(fn, "pollfd"))   { bi = 39; want = 3; }
            else if (!strcmp(fn, "mkstr"))    { bi = 40; want = 1; }
            else if (!strcmp(fn, "strbytes")) { bi = 41; want = 1; }
            else die("baris %d: fungsi Net.%s tidak ada", e->line, fn);

            int narg = (int)e->ival;
            if (bi == 5) {
                if (narg != 1 || e->a == 0 || e->a->kind != EX_STRLIT)
                    die("baris %d: Net.bytes membutuhkan tepat satu string literal", e->line);
                e->kind = EX_NET;
                e->ival = bi;
                e->cp = cp_push(e->a->str);
                e->dty = (int)T_ARI;
                return T_ARI;
            }
            if (bi == 6) {
                if (narg != 1 || e->a == 0 || e->a->kind != EX_STRLIT)
                    die("baris %d: Net.openr membutuhkan tepat satu string literal", e->line);
                e->kind = EX_NET;
                e->ival = bi;
                e->cp = cp_push(e->a->str);
                e->dty = (int)T_INT;
                return T_INT;
            }
            if (narg != want)
                die("baris %d: Net.%s membutuhkan %d argumen, dapat %d", e->line, fn, want, narg);
            Node *aa = e->a;
            for (int i = 0; i < narg; i++, aa = aa->next) {
                Ty t = type_expr(aa);
                /* recvb/sendb: (fd int, buf int[]) */
                if (bi == 7 || bi == 8) {
                    if (i == 0 && t != T_INT)
                        die("baris %d: Net.%s argumen %d harus int", e->line, i, fn);
                    if (i == 1 && t != T_ARI && t != T_STR)
                        die("baris %d: Net.%s argumen %d harus int[]/String buffer", e->line, i, fn);
                } else if (bi == 9) {
                    if (t != T_INT)
                        die("baris %d: Net.bind membutuhkan (fd int, ip int, port int)", e->line);
                } else if (bi == 10) {
                    if (i < 3 && t != T_INT)
                        die("baris %d: Net.sendto argumen %d harus int", e->line, i);
                    if (i == 3 && t != T_ARI && t != T_STR)
                        die("baris %d: Net.sendto argumen buffer harus int[]/String", e->line);
                } else if (bi == 11) {
                    if (i == 0 && t != T_INT)
                        die("baris %d: Net.recvfrom argumen fd harus int", e->line);
                    if (i == 1 && t != T_ARI && t != T_STR)
                        die("baris %d: Net.recvfrom argumen buffer harus int[]/String", e->line);
                    if (i == 2 && t != T_ARI && t != T_STR)
                        die("baris %d: Net.recvfrom argumen from harus int[2]", e->line);
                } else if (bi == 12) {
                    if (t != T_INT)
                        die("baris %d: Net.shutdown membutuhkan (fd int, how int)", e->line);
                } else if (bi == 13) {
                    if (i == 0 && t != T_ARI && t != T_STR)
                        die("baris %d: Net.openb argumen path harus int[]/String", e->line);
                    if (i > 0 && t != T_INT)
                        die("baris %d: Net.openb argumen flags/mode harus int", e->line);
                } else if (bi == 14 || bi == 15) {
                    if (t != T_ARI && t != T_STR)
                        die("baris %d: Net.%s argumen path harus int[]/String", e->line, fn);
                } else if (bi == 16) {
                    if (t != T_ARI && t != T_STR)
                        die("baris %d: Net.renameb kedua argumen harus int[]/String path", e->line);
                } else if (bi == 17 || bi == 20 || bi == 21) {
                    if (t != T_INT)
                        die("baris %d: argumen Net.%s harus int", e->line, fn);
                } else if (bi == 22 || bi == 23 || bi == 24) {
                    if (t != T_STR && t != T_ARI)
                        die("baris %d: Net.%s membutuhkan String/teks", e->line, fn);
                } else if (bi == 32) {
                    if (t != T_STR && t != T_ARI)
                        die("baris %d: Net.getenv membutuhkan String/teks nama", e->line);
                } else if (bi == 33) {
                    if (i == 0 && t != T_INT)
                        die("baris %d: Net.recv1 argumen fd harus int", e->line);
                    if (i == 1 && t != T_ARI && t != T_STR)
                        die("baris %d: Net.recv1 argumen buffer harus int[]/String", e->line);
                } else if (bi == 39) {
                    if ((i == 0 || i == 1) && t != T_ARI && t != T_STR)
                        die("baris %d: Net.pollfd argumen %d harus int[]", e->line, i + 1);
                    if (i == 2 && t != T_INT)
                        die("baris %d: Net.pollfd timeout harus int", e->line);
                } else if (bi == 40) {
                    /* Net.mkstr(int[] bytes) → String (materialisasi eksplisit) */
                    if (t != T_ARI)
                        die("baris %d: Net.mkstr butuh int[] byte", e->line);
                } else if (bi == 41) {
                    /* Net.strbytes(String) → int[] salinan byte */
                    if (t != T_STR)
                        die("baris %d: Net.strbytes butuh String", e->line);
                } else if (bi == 34 || bi == 36 || bi == 37 || bi == 38) {
                    if (t != T_INT)
                        die("baris %d: argumen Net.%s harus int", e->line, fn);
                } else if (bi == 25 || bi == 26 || bi == 27 || bi == 29 || bi == 31) {
                    if (t != T_INT)
                        die("baris %d: argumen Net.%s harus int", e->line, fn);
                } else if (t != T_INT)
                    die("baris %d: argumen %d Net.%s harus int", e->line, i, fn);
            }
            e->kind = EX_NET;
            e->ival = bi;
            if (bi == 19) { e->dty = (int)T_LONG; return T_LONG; }
            if (bi == 22 || bi == 31 || bi == 32 || bi == 40) { e->dty = (int)T_STR; return T_STR; }
            if (bi == 23 || bi == 24) { e->dty = (int)T_BOOL; return T_BOOL; }
            if (bi == 41) { e->dty = (int)T_ARI; return T_ARI; }
            e->dty = (int)T_INT;
            return T_INT;
        }
        Method *m = 0;
        /* metode instance — dua jalur parser:
         *  1) postfix sugar: recv.m(...) → EX_CALL "Arrays.m", receiver = arg[0]
         *  2) call_node:    recv.m(...) → EX_CALL "recv.m", receiver belum arg */
        if (!strcmp(buf, "Arrays") && e->a) {
            /* receiver bisa variabel ber-class atau field (k.pojok) */
            const char *rcls = obj_class(e->a);
            if (rcls) {
                Method *im = find_method(rcls, dot + 1);
                if (im && im->is_inst) {
                    char *full = xmalloc(strlen(rcls) + strlen(dot) + 1);
                    sprintf(full, "%s.%s", rcls, dot + 1);
                    e->str = full;
                    m = im;
                }
            }
        }
        if (!m && strcmp(buf, "Net") && strcmp(buf, "Arrays")) {
            /* receiver adalah variabel bertipe class (atau 'this') */
            if (!strcmp(buf, "this")) {
                if (cur_method && cur_method->is_inst) {
                    Method *im = find_method(cur_method->cls, dot + 1);
                    if (im && im->is_inst) {
                        Node *recv = newnode(EX_VAR, e->line);
                        recv->str = xs("this");
                        recv->next = e->a;
                        e->a = recv;
                        e->ival = (long)((int)e->ival + 1);
                        char *full = xmalloc(strlen(cur_method->cls) + strlen(dot) + 1);
                        sprintf(full, "%s.%s", cur_method->cls, dot + 1);
                        e->str = full;
                        m = im;
                    }
                }
            } else {
                SVar *rv = lookup_name(buf);
                if (rv && rv->cls) {
                    Method *im = find_method(rv->cls, dot + 1);
                    if (im && im->is_inst) {
                        Node *recv = newnode(EX_VAR, e->line);
                        recv->str = xs(buf);
                        recv->next = e->a;
                        e->a = recv;
                        e->ival = (long)((int)e->ival + 1);
                        char *full = xmalloc(strlen(rv->cls) + strlen(dot) + 1);
                        sprintf(full, "%s.%s", rv->cls, dot + 1);
                        e->str = full;
                        m = im;
                    }
                }
            }
        }
        if (!m) m = find_method(buf, dot + 1);
        /* Parser mengubah semua x.m(...) menjadi Arrays.m(x, ...).
         * Di sini kita belokkan ke Str.* bila penerimanya String. */
        if (!m && !strcmp(buf, "Arrays") && is_str_method(dot + 1)) {
            Ty rt = type_expr(e->a);
            if (rt == T_STR || rt == T_ARI) {
                const char *tgt = str_method_target(dot + 1);
                Method *sm = find_method("Str", tgt);
                if (sm) {
                    if (!strcmp(dot + 1, "indexOf") && (int)e->ival == 2) {
                        /* s.indexOf(needle) → Str.indexOf(s, needle, 0) */
                        Node *z = newnode(EX_INT, e->line);
                        z->ival = 0;
                        z->dty = (int)T_INT;
                        Node *tail = e->a;
                        while (tail->next) tail = tail->next;
                        tail->next = z;
                    }
                    char *full = xmalloc(strlen("Str.") + strlen(tgt) + 1);
                    sprintf(full, "Str.%s", tgt);
                    e->str = full;
                    int argc2 = 0;
                    for (Node *aa = e->a; aa; aa = aa->next) argc2++;
                    e->ival = (long)argc2;
                    m = sm;
                }
            }
        }
        if (!m) {
            /* gula String: s.trim() → Str.trim(s) */
            SVar *sr = lookup_name(buf);
            if (sr && (sr->ty == T_STR || sr->ty == T_ARI) && is_str_method(dot + 1)) {
                const char *tgt = str_method_target(dot + 1);
                Method *sm = find_method("Str", tgt);
                if (!sm)
                    die("baris %d: metode String '%s' butuh 'import Str;'", e->line, dot + 1);
                if (sm) {
                    Node *recv = newnode(EX_VAR, e->line);
                    recv->str = xs(buf);
                    recv->next = e->a;
                    if (!strcmp(dot + 1, "indexOf") && (int)e->ival == 1) {
                        Node *z = newnode(EX_INT, e->line);
                        z->ival = 0;
                        z->dty = (int)T_INT;
                        Node *tail = recv;
                        while (tail->next) tail = tail->next;
                        tail->next = z;
                    }
                    char *full = xmalloc(strlen("Str.") + strlen(tgt) + 1);
                    sprintf(full, "Str.%s", tgt);
                    e->str = full;
                    int argc2 = 0;
                    for (Node *aa = recv; aa; aa = aa->next) argc2++;
                    e->ival = (long)argc2;
                    e->a = recv;
                    m = sm;
                }
            }
            if (!m) {
            /* gula array: a.forEach(f) → Arrays.forEach(a, f) */
            if (is_array_method(dot + 1)) {
                SVar *rv = lookup_name(buf);
                Method *am = find_method("Arrays", dot + 1);
                if (rv && rv->ty == T_ARI && am) {
                    Node *recv = newnode(EX_VAR, e->line);
                    recv->str = xs(buf);
                    recv->next = e->a;
                    Node *nn = newnode(EX_CALL, e->line);
                    char *full2 = xmalloc(strlen("Arrays.") + strlen(dot + 1) + 1);
                    sprintf(full2, "Arrays.%s", dot + 1);
                    nn->str = full2;
                    nn->a = recv;
                    nn->ival = (long)((int)e->ival + 1);
                    Ty gt = type_expr(nn);
                    e->str = nn->str;
                    e->a = nn->a;
                    e->ival = nn->ival;
                    e->meth = nn->meth;
                    e->kind = EX_CALL;
                    return gt;
                }
            }
            /* bukan metode: mungkin variabel lambda → pemanggilan dinamis */
            Node *vn = newnode(EX_VAR, e->line);
            vn->str = xs(dot + 1);
            Ty vt = type_expr(vn);
            if (vt != T_FN)
                die("baris %d: metode '%s' tidak ada", e->line, e->str);
            e->kind = EX_FCALL;
            e->str = 0;
            e->b = vn;
            int i = 0;
            for (Node *aa = e->a; aa; aa = aa->next) {
                if (type_expr(aa) != T_INT)
                    die("baris %d: argumen %d lambda harus int", e->line, i);
                i++;
            }
            if (i > 5)
                die("baris %d: maksimal 5 argumen pemanggilan lambda", e->line);
            return T_INT;   /* tipe kembalian lambda dinamis: int (v1) */
            }
        }
        e->meth = m;
        int i = 0;
        for (Node *aa = e->a; aa; aa = aa->next, i++) {
            Ty at = type_expr(aa);
            Ty pt = T_INT;
            if (i < m->nparams) {   /* tipe param (params berantai) */
                Param *p = m->params;
                for (int k = 0; k < i && p; k++) p = p->next;
                if (p) pt = (Ty)p->ty;
            }
            if (pt == T_FN) {
                if (at != T_FN)
                    die("baris %d: argumen %s butuh lambda (fn)", e->line, e->str);
            } else if (!ty_can_implicit(pt, at) && !fits_literal(pt, aa)) {
                die("baris %d: argumen %d %s tipe tidak cocok — butuh cast eksplisit",
                    e->line, i, e->str);
            }
        }
        int want = m->nparams, got = (int)e->ival;
        if (want != got)
            die("baris %d: metode %s butuh %d argumen, dapat %d", e->line, e->str, want, got);
        e->ival = m->index;   /* simpan index metode */
        e->dty = (long)m->ret;    /* tipe kembalian untuk backend native */
        return m->ret;
    }
    case EX_ARROW:
        resolve_lambda(e);
        return T_FN;   /* nilai lambda bertipe fn; ret internal di Method */
    case EX_ENVLOAD:
        return (Ty)e->dty;
    case EX_NEWARR:
        if (type_expr(e->a) != T_INT)
            die("baris %d: ukuran 'new <tipe>[n]' harus int", e->line);
        if ((long)e->ival < 0) e->ival = 0;
        return (Ty)e->dty;               /* T_ARI / T_LNGARR / T_STRARR */
    case EX_NEWOBJ: {
        /* new Kelas() → alokasi int[] ber-ukuran nfield (selalu ≥1),
         * semua slot di-zero (jaminan zero-init array).
         * new Kelas(args) → Kelas.__new(args) — factory sintetis yang
         * mengalokasi lalu memanggil __ctor (constructor pengguna). */
        Class *c = find_class(e->str);
        if (!c)
            die("baris %d: class '%s' tidak ada (definisikan class sebelum 'new')",
                e->line, e->str);
        Method *ctor = find_method(e->str, "__ctor");
        if (ctor) {
            int want = ctor->nparams - 1;
            if ((int)e->ival != want)
                die("baris %d: constructor %s(...) butuh %d argumen, dapat %d",
                    e->line, e->str, want, (int)e->ival);
            char *full = xmalloc(strlen(e->str) + 8);
            sprintf(full, "%s.__new", e->str);
            e->kind = EX_CALL;
            e->str = full;
            e->dty = 0;
            return type_expr(e);         /* jalur EX_CALL: cek arg & tipe */
        }
        int nf = c->nfields > 0 ? c->nfields : 1;
        e->kind = EX_NEWARR;
        e->dty = (int)T_ARI;
        e->cls = c->name;          /* identitas class untuk cek penugasan */
        Node *sz = newnode(EX_INT, e->line);
        sz->ival = nf;
        sz->dty = (int)T_INT;
        sz->typed = 1;
        e->a = sz;
        e->str = 0;
        e->ival = nf;
        return T_ARI;
    }
    case EX_FLD: {
        /* obj.field → obj[idx]; base boleh variabel ber-class atau
         * akses field berantai (v1.14). Class field tersimpan di e->cls
         * agar rantai field (k.pojok.x) dan penugasan dicek tipe-nya. */
        Field *f = field_of(e->a, e->str, e->line);
        type_expr(e->a);        /* resolusi slot receiver + cek T_ARI */
        Node *idx = newnode(EX_INT, e->line);
        idx->ival = f->idx;
        idx->dty = (int)T_INT;
        idx->typed = 1;
        e->kind = EX_IDX;
        e->b = idx;
        e->str = 0;
        e->cls = f->cls;
        return (Ty)f->ty;      /* long/String/boolean tetap tipe aslinya */
    }
    case EX_IDX: {
        Ty bt = type_expr(e->a);
        if (!ty_is_arr(bt) && bt != T_STR)
            die("baris %d: indeks [..] hanya untuk array/String", e->line);
        if (type_expr(e->b) != T_INT)
            die("baris %d: indeks array harus int", e->line);
        /* String bukan array slot: s[i] → byte int (representasi T_ARI) */
        return bt == T_STR ? T_INT : ty_elem(bt);
    }
    case EX_LEN: {
        Ty bt = type_expr(e->a);
        if (!ty_is_arr(bt) && bt != T_STR)
            die("baris %d: .length hanya untuk array/String", e->line);
        return T_INT;
    }
    }
    die("type_expr: node tidak dikenal kind=%d", e->kind);
    return T_INT;
}

static void resolve_stmts(Node *lst);

/* literal int yang muat ke tipe tujuan (Java: constant narrowing) */
static int fits_literal(Ty dst, Node *e) {
    if (!ty_is_int(dst)) return 0;
    long v;
    if (e->kind == EX_INT) {
        v = e->ival;
    } else if (e->kind == EX_UN && e->str && e->str[0] == '-' &&
               e->a && e->a->kind == EX_INT) {
        v = -e->a->ival;
    } else {
        return 0;
    }
    switch (dst) {
    case T_INT:    return v >= -2147483648L && v <= 2147483647L;
    case T_LONG:   return 1;
    case T_UINT8:  return v >= 0 && v <= 255;
    case T_UINT16: return v >= 0 && v <= 65535;
    case T_UINT32: return v >= 0 && v <= 4294967295L;
    case T_UINT64: return 1;
    case T_BYTE:   return v >= -128 && v <= 127;
    case T_SHORT:  return v >= -32768 && v <= 32767;
    default: return 0;
    }
}

static void resolve_stmt(Node *s) {
    g_linefile = s->file;
    switch (s->kind) {
    case ST_VAR: {
        if (nscope >= 1020) die("terlalu banyak variabel lokal");
        const char *vcls = s->cls;
        if (vcls && !find_class(vcls))
            die("baris %d: tipe class '%s' tidak ada (definisikan class sebelum dipakai)",
                s->line, vcls);
        scope_push(s->str, nextslot++, (Ty)s->dty, vcls);
        Ty et = type_expr(s->a);
        if (!ty_can_implicit((Ty)s->dty, et) && !fits_literal((Ty)s->dty, s->a))
            die("baris %d: inisialisasi '%s' tidak cocok dengan tipe deklarasinya (butuh cast)",
                s->line, s->str);
        s->ival = nextslot - 1;
        break;
    }
    case ST_ASSIGN: {
        SVar *v = resolve_name(s->str, s->line);
        Ty et = type_expr(s->a);
        int compound = (int)s->cp;
        if (!ty_can_implicit(v->ty, et) &&
            !(compound && ty_is_int(v->ty) && ty_is_int(et)) &&
            !fits_literal(v->ty, s->a))
            die("baris %d: assignment '%s' tidak cocok tipe (butuh cast)", s->line, s->str);
        s->ival = v->slot;
        s->dty = (int)v->ty;   /* stamp tipe → codegen 32/64-bit */
        break;
    }
    case ST_BLOCK: {
        /* blok telanjang: scope sendiri */
        int mks = nscope, mki = nextslot;
        resolve_stmts(s->d);
        nscope = mks;  /* slot tidak direset → nlocals tetap watermark */
        break;
    }
    case ST_IF:
    case ST_WHILE: {
        /* scoping per-blok: deklarasi di badan blok dibereskan di akhir */
        Ty ct = type_expr(s->a);
        if (ct != T_BOOL) die("baris %d: kondisi harus boolean", s->line);
        int mks = nscope, mki = nextslot;
        resolve_stmts(s->b);
        nscope = mks;  /* slot tidak direset → nlocals tetap watermark */
        if (s->c) {   /* cabang else: scope sendiri (bisa shadow if-branch) */
            resolve_stmts(s->c);
            nscope = mks;  /* slot tidak direset → nlocals tetap watermark */
        }
        break;
    }
    case ST_DO: {
        int mks = nscope, mki = nextslot;
        resolve_stmts(s->b);
        nscope = mks;
        Ty ct = type_expr(s->a);
        if (ct != T_BOOL) die("baris %d: kondisi do-while harus boolean", s->line);
        break;
    }
    case ST_FOR: {
        /* init for (int i = ...) juga bagian dari scope blok for */
        int mks = nscope, mki = nextslot;
        if (s->a) resolve_stmt(s->a);
        if (s->b) {
            Ty ct = type_expr(s->b);
            if (ct != T_BOOL) die("baris %d: kondisi for harus boolean", s->line);
        }
        resolve_stmts(s->d);
        if (s->c) resolve_stmts(s->c);
        nscope = mks;  /* slot tidak direset → nlocals tetap watermark */
        break;
    }
    case ST_BRK: case ST_CONT: break;
    case ST_RETURN: {
        if (s->a) {
            Ty et = type_expr(s->a);
            /* ret metode aktif: cari lewat prog? simpan global current */
            if (!cur_method) die("internal: return tanpa metode");
            if (!ty_can_implicit(cur_method->ret, et) &&
                !fits_literal(cur_method->ret, s->a))
                die("baris %d: return tidak cocok dengan tipe metode (butuh cast)", s->line);
        } else if (cur_method->ret != T_VOID) {
            die("baris %d: metode ini harus return nilai", s->line);
        }
        break;
    }
    case ST_PRINT: {
        if (s->str) return;
        Ty et = type_expr(s->a);
        if (!ty_is_num(et) && et != T_BOOL && et != T_ARI && et != T_STR)
            die("baris %d: println/print hanya menerima numerik, boolean, atau teks/string literal", s->line);
        break;
    }
    case ST_EXPR:
        if (s->a) {
            Ty t = type_expr(s->a);
            (void)t;
        }
        break;
    case ST_FLDASG: {
        /* obj.field = v → ST_IDXASG(obj, idx, v) dengan cek tipe field */
        Field *f = field_of(s->a, s->str, s->line);
        Ty vt = type_expr(s->a);
        if (vt != T_ARI && vt != T_STR)
            die("baris %d: ruas kiri '%s.%s' bukan objek", s->line, s->a->kind == EX_VAR ? s->a->str : "?", s->str);
        Ty et = type_expr(s->c);
        Ty ft = (Ty)f->ty;
        int ok;
        if (ft == T_BOOL)
            ok = et == T_BOOL;
        else if (ft == T_ARI) {
            /* referensi objek; cek nama class bila bisa ditentukan */
            const char *etcls = obj_class(s->c);
            if (!etcls && s->c->kind == EX_CALL && s->c->meth && s->c->meth->retcls)
                etcls = s->c->meth->retcls;
            ok = et == T_ARI && (!etcls || !f->cls || !strcmp(etcls, f->cls));
            if (!ok && et == T_ARI && etcls && f->cls && strcmp(etcls, f->cls))
                die("baris %d: penugasan '%s' butuh %s, dapat %s",
                    s->line, s->str, f->cls, etcls);
        }
        else if (ft == T_STR)
            ok = et == T_STR;
        else
            ok = ty_can_implicit(ft, et) || fits_literal(ft, s->c);
        if (!ok)
            die("baris %d: penugasan '%s' tidak cocok tipe (butuh cast)",
                s->line, s->str);
        Node *idx = newnode(EX_INT, s->line);
        idx->ival = f->idx;
        idx->dty = (int)T_INT;
        idx->typed = 1;
        s->kind = ST_IDXASG;
        s->b = idx;
        s->str = 0;
        s->cls = 0;
        break;
    }
    case ST_IDXASG: {
        Ty at = type_expr(s->a);
        if (!ty_is_arr(at))
            die("baris %d: penugasan a[i] = v butuh array di ruas kiri", s->line);
        if (type_expr(s->b) != T_INT)
            die("baris %d: indeks array harus int", s->line);
        Ty et = type_expr(s->c);
        if (at == T_STRARR && et != T_STR)
            die("baris %d: elemen String[] harus String", s->line);
        if (at != T_STRARR && !ty_is_int(et))
            die("baris %d: elemen %s harus integer (perlu cast eksplisit)",
                s->line, at == T_LNGARR ? "long[]" : "int[]");
        break;
    }
    default:
        die("resolusi: statement kind=%d tidak dikenal", s->kind);
    }
}

static void resolve_stmts(Node *lst) {
    for (Node *n = lst; n; n = n->next) resolve_stmt(n);
}

static int cpcap;

/* push string ke cpool (dedup); dipakai collector ST_PRINT & Net.bytes */
static int cp_push(const char *s) {
    for (int i = 0; i < prog.ncp; i++)
        if (!strcmp(prog.cpool[i], s)) return i;
    if (prog.ncp >= cpcap) {
        cpcap *= 2;
        prog.cpool = realloc(prog.cpool, sizeof(char *) * (size_t)cpcap);
    }
    prog.cpool[prog.ncp] = s;
    return prog.ncp++;
}

/* intern dari file lain (gen_vm: nama file sumber untuk stack trace) */
int cp_intern(const char *s) { return cp_push(s); }

/* kumpulkan string literal unik dari semua ST_PRINT ke cpool */
static void collect_cp_stmt(Node *s) {
    if (!s) return;
    g_linefile = s->file;
    if (s->kind == ST_PRINT && s->str) {
        s->cp = cp_push(s->str);
    } else if (s->kind == ST_IF || s->kind == ST_WHILE) {
        collect_cp_stmt(s->b);
        collect_cp_stmt(s->c);
    } else if (s->kind == ST_BLOCK) {
        collect_cp_stmt(s->d);
    } else if (s->kind == ST_FOR) {
        collect_cp_stmt(s->a);
        collect_cp_stmt(s->d);
        collect_cp_stmt(s->c);
    } else if (s->kind == ST_EXPR && s->a) {
        /* cpool untuk Net.bytes diatur saat type_expr */
    }
    collect_cp_stmt(s->next);
}

static int cpcap;

static void resolve(void) {
    int idx = 0;
    for (Method *m = prog.methods; m; m = m->next) {
        m->index = idx++;
        if (!strcmp(m->name, "main") && (m->nparams == 0 || m->nparams == 1) && prog.mainidx < 0)
            prog.mainidx = m->index;
    }
    if (prog.mainidx < 0)
        die("tidak ada metode main() — butuh 'static void main()' di class mana pun");

    /* cegah dua metode dengan nama sama (import duplikat / file dobel) */
    for (Method *m = prog.methods; m; m = m->next) {
        if (m->hidden) continue;
        for (Method *m2 = m->next; m2; m2 = m2->next)
            if (!strcmp(m->cls, m2->cls) && !strcmp(m->name, m2->name))
                die("metode '%s.%s' dideklarasikan dua kali — cek duplikat saat import",
                    m2->cls, m2->name);
    }

    prog.ncp = 0;
    cpcap = 16;
    prog.cpool = xmalloc(sizeof(char *) * (size_t)cpcap);

    for (Method *m = prog.methods; m; m = m->next) {
        if (m->hidden) continue;      /* badan lambda sudah di-resolve + preamble */
        cur_method = m;
        nscope = 0;
        framebase = 0;
        nextslot = 0;
        for (Param *p = m->params; p; p = p->next) {
            if (p->cls && !find_class(p->cls))
                die("baris ???: tipe class '%s' tidak ada (param metode %s.%s)",
                    p->cls, m->cls, m->name);
            scope_push(p->name, nextslot++, (Ty)p->ty, p->cls);
        }
        m->nlocals = nextslot;
        resolve_stmts(m->body);
        collect_cp_stmt(m->body);
        m->nlocals = nextslot;
        if (m->nlocals > 256) die("metode %s.%s punya terlalu banyak local", m->cls, m->name);
    }
    cur_method = 0;
}

/* ---------- entry point parser ---------- */

void parse_all(Tok *toks) {
    ts = toks;
    ti = 0;
    prog.mainidx = -1;
    while (!at(T_EOF)) parse_class();
    resolve();
}
