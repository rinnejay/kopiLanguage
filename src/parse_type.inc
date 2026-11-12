/* parse_type.inc: resolusi tipe ekspresi — bagian dari parse.c (unit kompilasi sama). */

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
            if (pt == T_ARI) {
                /* template selalu String: buffer byte dimaterialisasi dulu */
                wrap_conv(part, "Net.mkstr");
                pt = T_STR;
            }
            if (pt != T_STR) {
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
            /* v1.18: cabang buffer dinormalisasi ke String packed */
            if (t1 == T_ARI) wrap_conv(e->b, "Net.mkstr");
            if (t2 == T_ARI) wrap_conv(e->c, "Net.mkstr");
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
            /* v1.18: buffer byte dinormalisasi ke String packed dulu */
            if (lt == T_ARI) wrap_conv(e->a, "Net.mkstr");
            if (rt == T_ARI) wrap_conv(e->b, "Net.mkstr");
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
                e->dty = (int)T_STR;    /* v1.18: Net.bytes menghasilkan String packed */
                return T_STR;
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
                /* v1.18: normalisasi representasi di batas builtin.
                 * cat/sequal/sne: semua argumen wajib String packed.
                 * buffer read-only: String → Net.strbytes (salinan).
                 * buffer yang diisi builtin: wajib int[] mentah. */
                if (bi == 22 || bi == 23 || bi == 24) {
                    if (t == T_ARI) { wrap_conv(aa, "Net.mkstr"); t = T_STR; }
                } else if (t == T_STR) {
                    int robuf = (bi == 3 || bi == 8) ? i == 1 :
                                (bi == 10) ? i == 3 :
                                (bi == 13 || bi == 14 || bi == 15 || bi == 32) ? i == 0 :
                                (bi == 16) ? i <= 1 : 0;
                    if (robuf) { wrap_conv(aa, "Net.strbytes"); t = T_ARI; }
                    else if (bi == 2 || bi == 7 || bi == 11 || bi == 33 || bi == 39)
                        die("baris %d: buffer yang diisi builtin harus int[] — pakai Net.strbytes dulu", e->line);
                }
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
        if (!strcmp(buf, "Arrays") && is_str_method(dot + 1)) {
            Ty rt = type_expr(e->a);
            if (rt == T_STR || rt == T_ARI) {
                const char *cls = (rt == T_STR) ? "Str" : "Bytes";
                const char *tgt = str_method_target(dot + 1);
                Method *sm = find_method(cls, tgt);
                int prefer = (!m) || (rt == T_STR && m && !strcmp(m->cls, "Arrays"));
                if (sm && prefer) {
                    if (!strcmp(dot + 1, "indexOf") && (int)e->ival == 2) {
                        /* s.indexOf(needle) → Str.indexOf(s, needle, 0) */
                        Node *z = newnode(EX_INT, e->line);
                        z->ival = 0;
                        z->dty = (int)T_INT;
                        Node *tail = e->a;
                        while (tail->next) tail = tail->next;
                        tail->next = z;
                    }
                    char *full = xmalloc(strlen(cls) + strlen(tgt) + 2);
                    sprintf(full, "%s.%s", cls, tgt);
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
                const char *cls = (sr->ty == T_STR) ? "Str" : "Bytes";
                const char *tgt = str_method_target(dot + 1);
                Method *sm = find_method(cls, tgt);
                if (!sm)
                    die("baris %d: metode %s '%s' butuh 'import %s;'", e->line,
                        sr->ty == T_STR ? "String" : "byte", dot + 1, cls);
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
                    char *full = xmalloc(strlen(cls) + strlen(tgt) + 2);
                    sprintf(full, "%s.%s", cls, tgt);
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
            /* v1.18: String → param int[] = salinan byte eksplisit (auto) */
            if (pt == T_ARI && at == T_STR) { wrap_conv(aa, "Net.strbytes"); at = T_ARI; }
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

