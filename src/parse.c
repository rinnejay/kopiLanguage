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
static Method *cur_method;

/* ---------- tipe ---------- */

static Ty type_opt(void) {  /* int | int[] | boolean | void | fn | String ([]) */
    if (at(KW_INT)) {
        advance();
        if (at('[')) { advance(); expect(']', "']' setelah '[' pada int[]"); return T_ARI; }
        return T_INT;
    }
    if (at(KW_BOOL)) { advance(); return T_BOOL; }
    if (at(KW_VOID)) { advance(); return T_VOID; }
    if (at(KW_FN)) { advance(); return T_FN; }
    if (at(KW_STRING)) {
        advance();
        /* String[] hanya untuk parameter seperti String[] args */
        if (at('[')) { advance(); expect(']', "']' pada String[]"); }
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
    } else {
        char c = op == T_ADDEQ ? '+' : op == T_SUBEQ ? '-' :
                 op == T_MULEQ ? '*' : op == T_DIVEQ ? '/' : '%';
        char *s = xmalloc(2); s[0] = c; s[1] = 0;
        bin->str = s;
        bin->b = expr();
    }
    n->a = bin;
    if (eat_semi) expect(';', "';'");
    return n;
}

static int is_incuptok(int t) {
    return t == T_PP || t == T_MM || t == T_ADDEQ || t == T_SUBEQ ||
           t == T_MULEQ || t == T_DIVEQ || t == T_MODEQ;
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
    case KW_INT: case KW_BOOL: case KW_FN: {
        Ty ty = type_opt();
        Node *n = newnode(ST_VAR, line);
        n->dty = (int)ty;
        n->str = xs(peek()->text);
        expect(T_ID, "nama variabel");
        expect('=', "'=' — variabel lokal wajib diinisialisasi");
        n->a = expr();
        expect(';', "';'");
        return n;
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
    case KW_FOR: {
        Node *n = newnode(ST_FOR, line);
        advance();
        expect('(', "'(' setelah for");
        /* init */
        if (at(KW_INT) || at(KW_BOOL) || at(KW_FN)) {
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
    case '{': return block();
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
        if (at(TK_STR)) { n->str = xs(peek()->text); advance(); }
        else n->a = expr();
        expect(')', "')' setelah pernyataan print");
        expect(';', "';'");
        return n;
    }

    /* assignment, a[i] = v, ++/+= kompon, atau call statement */
    if (t->type == T_ID) {
        int t2 = peek2()->type;
        if (t2 == '=') return parse_assignment(line, 1);
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

static void parse_class(void) {
    skip_mods();
    expect(KW_CLASS, "'class'");
    Tok *id = peek();
    expect(T_ID, "nama class");
    Class *c = xmalloc(sizeof(Class));
    c->name = xs(id->text);
    c->next = prog.classes;
    prog.classes = c;
    prog.curcls = c->name;

    expect('{', "'{' setelah nama class");
    while (!at('}')) {
        skip_mods();
        Tok *rt = peek();
        Ty ret = type_opt();
        if (ret == T_STR)
            die("baris %d: tipe kembalian String belum didukung", rt->line);
        if (ret == T_FN)
            die("baris %d: metode dengan return lambda belum didukung — simpan lambda di variabel", rt->line);
        Tok *mn = peek();
        expect(T_ID, "nama metode");
        Method *m = xmalloc(sizeof(Method));
        m->cls = c->name;
        m->name = xs(mn->text);
        m->ret = ret;
        expect('(', "'(' setelah nama metode");
        Param **pp = &m->params;
        if (!at(')')) {
            do {
                Ty pt = type_param();
                if (pt == T_VOID) die("baris %d: tipe parameter void", peek()->line);
                Tok *pn = peek();
                expect(T_ID, "nama parameter");
                Param *p = xmalloc(sizeof(Param));
                p->name = xs(pn->text);
                p->ty = (int)pt;
                *pp = p;
                pp = &p->next;
                m->nparams++;
            } while (at(',') && (advance(), 1));
        }
        expect(')', "')' setelah parameter");
        m->body = stmt_or_block();
        m->next = prog.methods;
        prog.methods = m;
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

static Node *primary(void) {
    Tok *t = peek();
    int line = t->line;
    if (at(T_NUM)) { Node *n = newnode(EX_INT, line); n->ival = t->num; advance(); return n; }
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
    if (at(T_ID) && !strcmp(xs(t->text), "new") && peek2()->type == KW_INT) {
        advance();                       /* new */
        advance();                       /* int   */
        expect('[', "'[' setelah 'new int'");
        Node *n = newnode(EX_NEWARR, line);
        n->dty = (int)T_ARI;
        n->a = expr();
        expect(']', "']' pada new int[n]");
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
        if (at('.')) {    /* hanya .length yang valid di sini */
            if (peek2()->type != T_ID || strcmp(xs(peek2()->text), "length"))
                die("baris %d: setelah '.' hanya '.length' yang didukung", l->line);
            advance(); advance();
            Node *n = newnode(EX_LEN, l->line);
            n->a = l;
            l = n;
            break;
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

static Node *rel_chain(void) {
    Node *l = add_chain();
    while (at('<') || at('>') || at(T_LE) || at(T_GE)) {
        Node *n = newnode(EX_BIN, peek()->line);
        const char *s = "?";
        if (at('<')) s = "<"; else if (at('>')) s = ">";
        else if (at(T_LE)) s = "<="; else s = ">=";
        n->str = s;
        n->a = l;
        advance();
        n->b = add_chain();
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

static Node *and_chain(void) {
    Node *l = eq_chain();
    while (at(T_AND)) {
        Node *n = newnode(EX_BIN, peek()->line);
        n->str = "&&";
        n->a = l;
        advance();
        n->b = eq_chain();
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
    return l;
}

/* ---------- resolusi simbol ---------- */

typedef struct { const char *name; int slot; Ty ty; } SVar;

static SVar scope[1024];
static int nscope;
static int nextslot;
static int framebase;      /* batas frame metode aktif; scope di bawah ini
                              milik metode luar → ambil jadi capture */

static void scope_push(const char *name, int slot, Ty ty) {
    for (int i = framebase; i < nscope; i++)
        if (!strcmp(scope[i].name, name))
            die("baris ???: variabel '%s' dideklarasikan dua kali", name);
    scope[nscope].name = name;
    scope[nscope].slot = slot;
    scope[nscope].ty = ty;
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
        scope_push(name, copy, scope[i].ty);   /* frame aktif memenangkan */
        hm->capcopy[j] = copy;
        return &scope[nscope - 1];
    }
    die("baris %d: variabel '%s' tidak ada di scope", line, name);
    return 0;
}

static Method *find_method(const char *cls, const char *name) {
    for (Method *m = prog.methods; m; m = m->next)
        if (!strcmp(m->cls, cls) && !strcmp(m->name, name)) return m;
    return 0;
}

static void resolve_stmts(Node *lst);      /* deklarasi maju */
static void collect_cp_stmt(Node *s);
static Ty type_expr(Node *e);

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
    scope_push("$env", nextslot++, T_FN);
    for (Param *p = envp->next; p; p = p->next)
        scope_push(p->name, nextslot++, T_INT);
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

static Ty type_expr(Node *e) {
    g_linefile = e->file;
    switch (e->kind) {
    case EX_INT: return T_INT;
    case EX_BOOL: return T_BOOL;
    case EX_VAR: { SVar *v = resolve_name(e->str, e->line); e->ival = v->slot; e->dty = (int)v->ty; return v->ty; }
    case EX_UN: {
        Ty t = type_expr(e->a);
        if (e->str[0] == '-' && t != T_INT)
            die("baris %d: '-' butuh operand int", e->line);
        if (e->str[0] == '!' && t != T_BOOL)
            die("baris %d: '!' butuh operand boolean", e->line);
        return e->str[0] == '-' ? T_INT : T_BOOL;
    }
    case EX_BIN: {
        Ty lt = type_expr(e->a), rt = type_expr(e->b);
        char op = e->str[0];
        if (op == '&' || op == '|') {          /* && || */
            if (lt != T_BOOL || rt != T_BOOL)
                die("baris %d: '&&' / '||' butuh operand boolean", e->line);
            return T_BOOL;
        }
        if (op == '=' || op == '!' ) {         /* == != */
            if (lt != rt) die("baris %d: tipe operand '==' / '!=' beda", e->line);
            return T_BOOL;
        }
        if (op == '<' || op == '>') {
            if (lt != T_INT || rt != T_INT)
                die("baris %d: tanda pembanding butuh int", e->line);
            return T_BOOL;
        }
        if (lt != T_INT || rt != T_INT)
            die("baris %d: operator '%s' butuh operand int", e->line, e->str);
        return T_INT;
    }
    case EX_CALL: {
        char buf[512];
        const char *dot = strchr(e->str, '.');
        memcpy(buf, e->str, (size_t)(dot - e->str)); buf[dot - e->str] = 0;
        Method *m = find_method(buf, dot + 1);
        if (!m) {
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
        int i = 0;
        for (Node *aa = e->a; aa; aa = aa->next, i++) {
            Ty at = type_expr(aa);
            Ty pt = T_INT;
            if (i < m->nparams) {   /* tipe param (params berantai) */
                Param *p = m->params;
                for (int k = 0; k < i && p; k++) p = p->next;
                if (p) pt = (Ty)p->ty;
            }
            if (pt == (Ty)T_INT && at != T_INT)
                die("baris %d: argumen %s harus int", e->line, e->str);
            if (pt == (Ty)T_BOOL && at != T_BOOL)
                die("baris %d: argumen %s harus boolean", e->line, e->str);
            if (pt == (Ty)T_FN && at != T_FN)
                die("baris %d: argumen %s butuh lambda (fn)", e->line, e->str);
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
            die("baris %d: ukuran 'new int[n]' harus int", e->line);
        if ((long)e->ival < 0) e->ival = 0;
        return T_ARI;
    case EX_IDX:
        if (type_expr(e->a) != T_ARI)
            die("baris %d: indeks [..] hanya untuk array int[]", e->line);
        if (type_expr(e->b) != T_INT)
            die("baris %d: indeks array harus int", e->line);
        return T_INT;
    case EX_LEN:
        if (type_expr(e->a) != T_ARI)
            die("baris %d: .length hanya untuk array int[]", e->line);
        return T_INT;
    }
    die("type_expr: node tidak dikenal kind=%d", e->kind);
    return T_INT;
}

static void resolve_stmts(Node *lst);

static void resolve_stmt(Node *s) {
    g_linefile = s->file;
    switch (s->kind) {
    case ST_VAR: {
        if (nscope >= 1020) die("terlalu banyak variabel lokal");
        scope_push(s->str, nextslot++, (Ty)s->dty);
        Ty et = type_expr(s->a);
        if (et != (Ty)s->dty)
            die("baris %d: inisialisasi '%s' tidak cocok dengan tipe deklarasinya", s->line, s->str);
        s->ival = nextslot - 1;
        break;
    }
    case ST_ASSIGN: {
        SVar *v = resolve_name(s->str, s->line);
        Ty et = type_expr(s->a);
        if (et != v->ty)
            die("baris %d: assignment '%s' tidak cocok tipe", s->line, s->str);
        s->ival = v->slot;
        s->dty = (int)v->ty;   /* stamp tipe → codegen 32/64-bit */
        break;
    }
    case ST_IF:
    case ST_WHILE: {
        Ty ct = type_expr(s->a);
        if (ct != T_BOOL) die("baris %d: kondisi harus boolean", s->line);
        resolve_stmts(s->b);
        if (s->c) resolve_stmts(s->c);
        break;
    }
    case ST_FOR: {
        if (s->a) resolve_stmt(s->a);
        if (s->b) {
            Ty ct = type_expr(s->b);
            if (ct != T_BOOL) die("baris %d: kondisi for harus boolean", s->line);
        }
        resolve_stmts(s->d);
        if (s->c) resolve_stmts(s->c);
        break;
    }
    case ST_BRK: case ST_CONT: break;
    case ST_RETURN: {
        if (s->a) {
            Ty et = type_expr(s->a);
            /* ret metode aktif: cari lewat prog? simpan global current */
            if (!cur_method) die("internal: return tanpa metode");
            if (et != cur_method->ret)
                die("baris %d: return tidak cocok dengan tipe metode", s->line);
        } else if (cur_method->ret != T_VOID) {
            die("baris %d: metode ini harus return nilai", s->line);
        }
        break;
    }
    case ST_PRINT: {
        if (s->str) return;
        Ty et = type_expr(s->a);
        if (et != T_INT && et != T_BOOL)
            die("baris %d: println/print hanya menerima int, boolean, atau string literal", s->line);
        break;
    }
    case ST_EXPR:
        if (s->a) {
            Ty t = type_expr(s->a);
            (void)t;
        }
        break;
    case ST_IDXASG: {
        if (type_expr(s->a) != T_ARI)
            die("baris %d: penugasan a[i] = v butuh array int[] di ruas kiri", s->line);
        if (type_expr(s->b) != T_INT)
            die("baris %d: indeks array harus int", s->line);
        if (type_expr(s->c) != T_INT)
            die("baris %d: nilai penugasan a[i] harus int", s->line);
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

/* kumpulkan string literal unik dari semua ST_PRINT ke cpool */
static void collect_cp_stmt(Node *s) {
    if (!s) return;
    g_linefile = s->file;
    if (s->kind == ST_PRINT && s->str) {
        int found = 0;
        for (int i = 0; i < prog.ncp; i++)
            if (!strcmp(prog.cpool[i], s->str)) { s->cp = i; found = 1; break; }
        if (!found) {
            if (prog.ncp >= cpcap) {
                cpcap *= 2;
                prog.cpool = realloc(prog.cpool, sizeof(char *) * (size_t)cpcap);
            }
            s->cp = prog.ncp;
            prog.cpool[prog.ncp++] = s->str;
        }
    } else if (s->kind == ST_IF || s->kind == ST_WHILE) {
        collect_cp_stmt(s->b);
        collect_cp_stmt(s->c);
    } else if (s->kind == ST_FOR) {
        collect_cp_stmt(s->a);
        collect_cp_stmt(s->d);
        collect_cp_stmt(s->c);
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
        for (Param *p = m->params; p; p = p->next)
            scope_push(p->name, nextslot++, (Ty)p->ty);
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
