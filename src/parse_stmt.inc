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

