/* parse_stmt.inc: pernyataan & blok — bagian dari parse.c (unit kompilasi sama). */

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
    case KW_SWITCH: return parse_switch(line);   /* v1.19 */
    case KW_MALLOC: return parse_malloc(line);   /* v1.20 */
    case KW_THROW: {                             /* v1.49: throw e; */
        Node *n = newnode(ST_THROW, line);
        advance();
        n->a = expr();
        expect(';', "';' setelah throw");
        return n;
    }
    case KW_TRY: {                               /* v1.49: try/catch */
        Node *n = newnode(ST_TRY, line);
        advance();
        if (!at('{'))
            die("baris %d: setelah 'try' butuh '{'", line);
        n->b = block();
        expect(KW_CATCH, "'catch' setelah blok try");
        expect('(', "'(' setelah catch");
        n->dty = (int)type_opt();                /* tipe variabel penangkap */
        if (at(T_ID)) { n->str = xs(peek()->text); advance(); }
        expect(')', "')' setelah parameter catch");
        if (!at('{'))
            die("baris %d: setelah catch(...) butuh '{'", line);
        n->d = block();
        return n;
    }    case KW_THREAD: {                            /* v1.45: thread [h] { ... } */
        advance();
        Node *tgt = 0;
        if (!at('{')) {
            Tok *vt = peek();
            expect(T_ID, "nama variabel handle thread (long)");
            Node *v = newnode(EX_VAR, vt->line);
            v->str = xs(vt->text);
            if (at('[')) {
                advance();
                Node *ix = expr();
                expect(']', "']' setelah indeks target thread");
                Node *idxn = newnode(EX_IDX, vt->line);
                idxn->a = v;
                idxn->b = ix;
                tgt = idxn;
            } else {
                tgt = v;
            }
        }
        if (!at('{'))
            die("baris %d: setelah target thread butuh '{' badan thread", line);
        Node *ar = newnode(EX_ARROW, line);
        ar->c = 0;                               /* lambda tanpa parameter */
        ar->d = block();
        Node *n = newnode(ST_THREAD, line);
        n->a = ar;
        n->b = tgt;
        return n;
    }
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

    /* v1.45: `wait h;` sebagai pernyataan (join tanpa memakai hasil) */
    if (at(KW_WAIT)) {
        Node *n = newnode(ST_EXPR, line);
        n->a = expr();
        expect(';', "';' setelah wait");
        return n;
    }

    /* v1.51: deklarasi objek generik: Box<int> b = new Box<int>(); */
    if (t->type == T_ID && tokat(1, '<') && !lambda_ahead()) {
        Class *gc = find_class(xs(t->text));
        if (gc && gc->tparam) {
            Node *n = newnode(ST_VAR, line);
            const char *base = xs(t->text);
            advance();                 /* nama class generik */
            n->dty = (int)T_ARI;
            n->cls = parse_generic_ref(base, line);
            n->str = xs(peek()->text);
            expect(T_ID, "nama variabel");
            expect('=', "'=' — variabel lokal wajib diinisialisasi");
            n->a = expr();
            expect(';', "';'");
            return n;
        }
    }

    /* v1.51: class generik tanpa argumen tipe → pesan jelas */
    if (t->type == T_ID && tokat(1, T_ID)) {
        Class *gc0 = find_class(xs(t->text));
        if (gc0 && gc0->tparam)
            die("baris %d: class generik '%s' butuh argumen tipe (mis. %s<int>)",
                line, gc0->name, gc0->name);
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
            /* v1.41: Class.KONSTANTA++ / += ... → tolak jelas. */
            if (find_class(xs(t->text)) &&
                find_constfield(xs(t->text), xs(ts[ti + 2].text)) &&
                is_incuptok(ts[ti + 3].type))
                die("baris %d: '%s.%s' konstanta static — tidak bisa diubah",
                    line, t->text, ts[ti + 2].text);
            /* v1.29: p.field[i] = v — indeks pada field bertipe array */
            if (tokat(3, '[')) {
                Node *s = newnode(ST_IDXASG, line);
                Node *obj = newnode(EX_VAR, line);
                obj->str = xs(t->text);
                advance();                 /* nama objek */
                advance();                 /* '.' */
                const char *f1 = xs(peek()->text);
                expect(T_ID, "nama field");
                Node *base = newnode(EX_FLD, line);
                base->a = obj;
                base->str = f1;
                s->a = base;
                expect('[', "'[' pada p.field[i]");
                s->b = expr();
                expect(']', "']' pada p.field[i]");
                expect('=', "'=' pada p.field[i] = v");
                s->c = expr();
                expect(';', "';' setelah p.field[i] = v");
                return s;
            }
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

