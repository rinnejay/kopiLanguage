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

static Tok *peek(void) {
    g_linefile = ts[ti].file;
    g_line = ts[ti].line;
    g_col = ts[ti].col;
    return &ts[ti];
}
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

/* ── switch (v1.19): desugar ke rantai if/else ────────────────────
 * switch (e) { case A: ... default: ... }
 *   →  { int #swN = e;
 *        if (#swN == A) { ... } else if (#swN == B) { ... } else { ... } }
 * - TANPA fall-through: badan tiap case dibungkus blok sendiri.
 * - break/continue di dalam badan tetap milik loop terluar (karena
 *   hasilnya if/else biasa) — bukan break milik switch.
 * - case hanya menerima literal bilangan bulat (boleh negatif). */
static int swtmp_n = 0;

static Node *parse_switch(int line) {
    advance();                       /* kata 'switch' */
    expect('(', "'(' setelah switch");
    Node *sel = expr();
    expect(')', "')' setelah selector switch");
    expect('{', "'{' membuka badan switch");

    struct { long val; Node *body; int line; } cases[64];
    Node *dflt = 0;
    int nc = 0;
    while (!at('}')) {
        if (at(KW_CASE)) {
            int cl = peek()->line;
            advance();
            int neg = 0;
            if (at('-')) { advance(); neg = 1; }
            if (!at(T_NUM))
                die("baris %d: case harus literal bilangan bulat", cl);
            long v = peek()->num;
            advance();
            if (neg) v = -v;
            expect(':', "':' setelah nilai case");
            for (int i = 0; i < nc; i++)
                if (cases[i].val == v)
                    die("baris %d: case %ld duplikat", cl, v);
            if (nc >= 64)
                die("baris %d: terlalu banyak case (maks 64)", cl);
            /* badan case: statement sampai case/default/'}' — tanpa fall-through */
            Node *blk = newnode(ST_BLOCK, cl);
            Node *head = 0, *tail = 0;
            while (!at(KW_CASE) && !at(KW_DEFAULT) && !at('}')) {
                Node *s = stmt_single();
                while (tail && tail->next) tail = tail->next;
                if (tail) tail->next = s; else head = s;
                tail = s;
            }
            blk->d = head;
            cases[nc].val = v;
            cases[nc].body = blk;
            cases[nc].line = cl;
            nc++;
        } else if (at(KW_DEFAULT)) {
            int dl = peek()->line;
            if (dflt)
                die("baris %d: default duplikat", dl);
            advance();
            expect(':', "':' setelah default");
            Node *blk = newnode(ST_BLOCK, dl);
            Node *head = 0, *tail = 0;
            while (!at(KW_CASE) && !at(KW_DEFAULT) && !at('}')) {
                Node *s = stmt_single();
                while (tail && tail->next) tail = tail->next;
                if (tail) tail->next = s; else head = s;
                tail = s;
            }
            blk->d = head;
            dflt = blk;
        } else {
            die("baris %d: 'case'/'default' diharapkan di dalam switch",
                peek()->line);
        }
    }
    expect('}', "'}' penutup switch");
    if (nc == 0 && !dflt)
        die("baris %d: switch tanpa satu pun case/default", line);

    /* variabel temp unik: selector dievaluasi sekali */
    char nm[32];
    snprintf(nm, sizeof nm, "#sw%d", swtmp_n++);
    Node *decl = newnode(ST_VAR, line);
    decl->dty = (int)T_INT;
    decl->str = xs(nm);
    decl->a = sel;

    /* bangun rantai if dari case terakhir */
    Node *chain = dflt;
    for (int i = nc - 1; i >= 0; i--) {
        Node *cmp = newnode(EX_BIN, cases[i].line);
        cmp->str = "==";
        cmp->a = newnode(EX_VAR, cases[i].line);
        cmp->a->str = xs(nm);
        Node *lit = newnode(EX_INT, cases[i].line);
        lit->ival = cases[i].val;
        lit->dty = (int)T_INT;
        cmp->b = lit;
        Node *iff = newnode(ST_IF, cases[i].line);
        iff->a = cmp;
        iff->b = cases[i].body;
        iff->c = chain;
        chain = iff;
    }

    /* blok pembungkus: deklarasi temp + rantai if (scope sendiri) */
    Node *wrap = newnode(ST_BLOCK, line);
    decl->next = chain;
    wrap->d = decl;
    return wrap;
}

/* v1.21: satuan ukuran arena malloc<...> (tanpa satuan = MB) */
static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        int ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

/* ── malloc[<ukuran>] { ... } (v1.20, ukuran v1.21) ─────────────────
 * malloc { ... }        → arena blok 32 MB (default)
 * malloc<64mb> { ... }  → arena blok 64 MB
 * malloc<512kb> { ... } → satuan b, kb, mb, gb (tanpa satuan = mb)
 * Compiler memasang arena baru saat masuk blok dan membebaskannya di
 * SEMUA jalur keluar: akhir blok, break/continue yang keluar dari blok
 * (lihat gen_*), dan return (lihat gen_*). Nilai > 0; maks 4096 MB. */
static long parse_malloc_size(int line) {
    if (!at('<')) return 0;          /* 0 = pakai default 32 MB */
    advance();
    if (!at(T_NUM))
        die("baris %d: malloc<...> butuh angka ukuran (contoh: malloc<64mb>)", line);
    Tok *t = peek();
    if (t->numty != T_INT && t->numty != T_LONG &&
        t->numty != T_UINT32 && t->numty != T_UINT64)
        die("baris %d: ukuran malloc harus bilangan bulat", line);
    if (t->num < 0)
        die("baris %d: ukuran malloc tidak boleh negatif", line);
    long n = t->num;
    advance();
    long mul = 1024L * 1024L;        /* default MB */
    if (at(T_ID)) {
        Tok *u = peek();
        if (ieq(u->text, "b"))       mul = 1;
        else if (ieq(u->text, "kb")) mul = 1024L;
        else if (ieq(u->text, "mb")) mul = 1024L * 1024L;
        else if (ieq(u->text, "gb")) mul = 1024L * 1024L * 1024L;
        else
            die("baris %d: satuan '%s' tidak dikenal (pakai b, kb, mb, gb)",
                line, u->text);
        advance();
    }
    expect('>', "'>' penutup ukuran malloc");
    if (n == 0)
        die("baris %d: ukuran malloc harus lebih besar dari 0", line);
    if (n > (4096L * 1024 * 1024) / mul)
        die("baris %d: ukuran malloc maksimum 4096 MB", line);
    long bytes = n * mul;
    if (bytes < 1024)
        die("baris %d: ukuran malloc minimal 1 kb", line);
    return bytes;
}

static Node *parse_malloc(int line) {
    Node *n = newnode(ST_REGION, line);
    advance();                       /* kata 'malloc' */
    n->cp = parse_malloc_size(line); /* bytes; 0 = default 32 MB */
    if (!at('{'))
        die("baris %d: 'malloc' atau 'malloc<...>' harus diikuti '{'", peek()->line);
    n->d = block();
    return n;
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

#include "parse_stmt.inc"

#include "parse_class.inc"

#include "parse_expr.inc"

#include "parse_scope.inc"

#include "parse_type.inc"

#include "parse_resolve.inc"
