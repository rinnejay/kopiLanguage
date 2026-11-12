/* parse_expr.inc: lambda, JSON, template, ekspresi — bagian dari parse.c (unit kompilasi sama). */

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
    n->dty = (int)T_STR;      /* v1.18: Net.bytes menghasilkan String packed */
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

/* v1.18: bungkus ekspresi jadi Net.strbytes(expr) / Net.mkstr(expr) —
 * konversi material antar representasi String packed ↔ buffer byte.
 * Dipakai compiler untuk koersi implisit di batas pemanggilan. */
static Ty type_expr(Node *e);

static void wrap_conv(Node *aa, const char *fn) {
    Node *inner = newnode(aa->kind, aa->line);
    *inner = *aa;
    inner->next = 0;                     /* jangan ikutkan sisa argumen luar */
    aa->kind = EX_CALL;
    aa->str = xs(fn);
    aa->a = inner;
    aa->ival = 1;
    aa->b = 0;
    aa->cp = 0;
    aa->cls = 0;
    aa->meth = 0;
    aa->typed = 0;
    type_expr(aa);
}

static Node *parse_tstr(const char *raw, int line) {
    /* ekspresi di dalam `{...}` memakai nama berkas asli, bukan "<template>",
       agar pesan error bisa ditunjuk ke berkas/baris yang benar. */
    const char *self = g_linefile ? g_linefile : "<template>";
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
            Tok *tt = lex_text(sub, self, line, &nt);
            Tok *savets = ts;
            int saveti = ti;
            ts = tt;
            ti = 0;
            Node *ex = expr();
            if (!at(T_EOF))
                die("baris %d: template: ekspresi '{...}' tidak valid", line);
            ts = savets;
            ti = saveti;
            g_linefile = self;
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

