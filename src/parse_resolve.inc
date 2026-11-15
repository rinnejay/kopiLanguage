/* parse_resolve.inc — resolusi pernyataan & entry (bagian dari parse.c). */

static void resolve_stmt(Node *s) {
    g_linefile = s->file;
    switch (s->kind) {
    case ST_VAR: {
        if (nscope >= 1020) die("terlalu banyak variabel lokal");
        const char *vcls = s->cls;
        if (vcls && !find_class(vcls))
            die("baris %d: tipe class '%s' tidak ada (definisikan class sebelum dipakai)",
                s->line, vcls);
        int declslot = nextslot;             /* v1.46: simpan slot sebelum
                                              * inisialisasi — inisialisasi bisa
                                              * membuat slot capture lambda baru */
        scope_push(s->str, nextslot++, (Ty)s->dty, vcls);
        Ty et = type_expr(s->a);
        /* v1.18: konversi representasi String ↔ buffer otomatis */
        if ((Ty)s->dty == T_ARI && et == T_STR) { wrap_conv(s->a, "Net.strbytes"); et = T_ARI; }
        else if ((Ty)s->dty == T_STR && et == T_ARI) { wrap_conv(s->a, "Net.mkstr"); et = T_STR; }
        else if (need_arrconv((Ty)s->dty, et)) et = wrap_arrconv(s->a, (Ty)s->dty);
        if (!ty_can_implicit((Ty)s->dty, et) && !fits_literal((Ty)s->dty, s->a))
            die("baris %d: inisialisasi '%s' tidak cocok dengan tipe deklarasinya (butuh cast)",
                s->line, s->str);
        s->ival = declslot;
        break;
    }
    case ST_ASSIGN: {
        SVar *v = resolve_name(s->str, s->line);
        Ty et = type_expr(s->a);
        int compound = (int)s->cp;
        /* v1.18: konversi representasi String ↔ buffer otomatis */
        if (v->ty == T_ARI && et == T_STR) { wrap_conv(s->a, "Net.strbytes"); et = T_ARI; }
        else if (v->ty == T_STR && et == T_ARI) { wrap_conv(s->a, "Net.mkstr"); et = T_STR; }
        else if (need_arrconv((Ty)v->ty, et)) et = wrap_arrconv(s->a, (Ty)v->ty);
        /* v1.20: rujukan tidak boleh lari ke variabel di luar malloc{} */
        if (reg_sp && is_ref_ty((Ty)v->ty) && is_ref_ty(et) &&
            (int)(v - scope) < reg_base[reg_sp - 1])
            die("baris %d: nilai rujukan tidak boleh disimpan ke '%s' yang hidup di luar malloc{} (arena blok dibebaskan di akhirnya)",
                s->line, s->str);
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
            /* v1.18: konversi representasi String ↔ buffer otomatis */
            if (cur_method->ret == T_ARI && et == T_STR) { wrap_conv(s->a, "Net.strbytes"); et = T_ARI; }
            else if (cur_method->ret == T_STR && et == T_ARI) { wrap_conv(s->a, "Net.mkstr"); et = T_STR; }
            else if (need_arrconv((Ty)cur_method->ret, et)) et = wrap_arrconv(s->a, (Ty)cur_method->ret);
            /* v1.20: nilai rujukan akan dibebaskan bila keluar lewat malloc{} */
            if (reg_sp && is_ref_ty(et))
                die("baris %d: return bertipe rujukan dari dalam malloc{} belum didukung — hitung di dalam blok lalu return setelah blok (arena blok dibebaskan)",
                    s->line);
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
        /* v1.41: Class.KONSTANTA = v → tolak dengan pesan jelas. */
        if (s->a && s->a->kind == EX_VAR && !lookup_name(s->a->str)) {
            Class *cc = find_class(s->a->str);
            if (cc && find_constfield(cc->name, s->str))
                die("baris %d: '%s.%s' konstanta static — tidak bisa diubah",
                    s->line, cc->name, s->str);
        }
        /* obj.field = v → ST_IDXASG(obj, idx, v) dengan cek tipe field */
        Field *f = field_of(s->a, s->str, s->line);
        Ty vt = type_expr(s->a);
        if (vt != T_ARI && vt != T_STR)
            die("baris %d: ruas kiri '%s.%s' bukan objek", s->line, s->a->kind == EX_VAR ? s->a->str : "?", s->str);
        Ty et = type_expr(s->c);
        Ty ft = (Ty)f->ty;
        if (need_arrconv(ft, et)) et = wrap_arrconv(s->c, ft);
        int ok;
        if (!f->cls && ty_is_arr(ft)) {
            /* v1.29: field bertipe array — tipe elemen harus sama */
            ok = (et == ft);
        }
        else if (ft == T_BOOL)
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
        /* v1.20: rujukan tidak boleh disimpan ke field objek luar malloc{} */
        if (reg_sp && is_ref_ty(et) && s->a->kind == EX_VAR &&
            s->a->ival < reg_slot[reg_sp - 1])
            die("baris %d: nilai rujukan tidak boleh disimpan ke field '%s' objek di luar malloc{} (arena blok dibebaskan di akhirnya)",
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
        if ((at == T_DBLARR || at == T_FLTARR) && !ty_is_num(et))
            die("baris %d: elemen %s harus numerik",
                s->line, at == T_DBLARR ? "double[]" : "float[]");
        if (at == T_BYTEARR || at == T_U8ARR) {
            /* v1.44: penyimpanan ke buffer packed memotong ke 8 bit;
             * nilai integer apa pun diterima (seperti assignment Java
             * dengan cast implisit di elemen buffer). */
            if (!ty_is_int(et))
                die("baris %d: elemen %s harus integer",
                    s->line, at == T_BYTEARR ? "byte[]" : "uint8[]");
        }
        else if (at != T_STRARR && at != T_DBLARR && at != T_FLTARR &&
                 !ty_is_int(et))
            die("baris %d: elemen %s harus integer (perlu cast eksplisit)",
                s->line, at == T_LNGARR ? "long[]" : "int[]");
        /* v1.20: rujukan tidak boleh disimpan ke array milik luar malloc{} */
        if (reg_sp && is_ref_ty(et) && s->a->kind == EX_VAR &&
            s->a->ival < reg_slot[reg_sp - 1])
            die("baris %d: nilai rujukan tidak boleh disimpan ke elemen array '%s' yang hidup di luar malloc{} (arena blok dibebaskan di akhirnya)",
                s->line, s->a->str);
        break;
    }
    case ST_THREAD: {
        /* v1.45: thread [h] { ... } — badan jadi lambda 0-arg, spawn,
         * hasil handle long disimpan ke variabel/elemen long[]. */
        Ty bt = type_expr(s->a);
        if (bt != T_FN)
            die("baris %d: badan thread harus blok kode", s->line);
        Node *call = newnode(EX_NET, s->line);
        call->ival = 47;                 /* Net.threadspawn */
        call->a = s->a;
        call->dty = (int)T_LONG;
        call->typed = 1;
        call->file = s->file;
        s->a = 0;
        if (!s->b) {                     /* tanpa target: detached */
            s->kind = ST_EXPR;
            s->a = call;
            s->dty = (int)T_VOID;
            break;
        }
        Node tmp;
        memset(&tmp, 0, sizeof tmp);
        tmp.line = s->line;
        tmp.file = s->file;
        if (s->b->kind == EX_VAR) {
            SVar *hv = lookup_name(s->b->str);
            if (!hv || (hv->ty != T_LONG && hv->ty != T_UINT64))
                die("baris %d: target thread '%s' harus variabel long",
                    s->line, s->b->str);
            tmp.kind = ST_ASSIGN;
            tmp.str = s->b->str;
            tmp.a = call;
        } else if (s->b->kind == EX_IDX) {
            Node *arr = s->b->a, *ix = s->b->b;
            Ty at = type_expr(arr);
            if (at != T_LNGARR)
                die("baris %d: target thread a[i] harus long[] (handle 64-bit)", s->line);
            tmp.kind = ST_IDXASG;
            tmp.a = arr;
            tmp.b = ix;
            tmp.c = call;
        } else {
            die("baris %d: target thread harus variabel long atau long[i]", s->line);
        }
        resolve_stmt(&tmp);
        Node *nx = s->next;
        *s = tmp;
        s->next = nx;
        break;
    }
    case ST_REGION: {        /* v1.20: malloc { ... } — scope sendiri + arena blok otomatis.
         * v1.21: ukuran arena (s->cp, byte) dari malloc<64mb>.
         * v1.22: tanpa <...> = OTOMATIS (runtime menyediakan arena sampai
         *        4096 MB; RAM hanya terpakai sesuai kebutuhan). */
        if (reg_sp >= 32)
            die("baris %d: terlalu banyak malloc{} bersarang (maks 32)", s->line);
        int mks = nscope, mksl = nextslot;
        if (nextslot >= 1020) die("terlalu banyak variabel lokal");
        s->ival = nextslot++;      /* slot handle arena (disembunyikan) */
        s->dty = (int)T_INT;
        reg_base[reg_sp] = mks;
        reg_slot[reg_sp] = mksl;
        reg_sp++;
        resolve_stmts(s->d);
        reg_sp--;
        nscope = mks;              /* slot tidak direset → watermark tetap */
        break;
    }
    case ST_THROW: {                       /* v1.49: throw e; */
        if (!s->a) die("baris %d: throw butuh nilai", s->line);
        Ty et = type_expr(s->a);
        if (et == T_VOID)
            die("baris %d: throw tidak menerima nilai void", s->line);
        s->dty = (int)et;                  /* codegen melebarkan sesuai tipe */
        break;
    }
    case ST_TRY: {                         /* v1.49: try/catch */
        int mks = nscope;
        resolve_stmts(s->b);
        nscope = mks;
        if (nextslot >= 1020) die("terlalu banyak variabel lokal");
        s->ival = nextslot++;              /* slot variabel penangkap */
        if (s->str) scope_push(s->str, (int)s->ival, (Ty)s->dty, 0);
        resolve_stmts(s->d);
        nscope = mks;                      /* slot tidak direset → watermark */
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
    } else if (s->kind == ST_DO) {
        /* v1.38: do..while — badan di s->b (println-literal di dalamnya
         * juga harus di-intern) */
        collect_cp_stmt(s->b);
    } else if (s->kind == ST_REGION) {
        /* v1.38: malloc { ... } — println-literal di dalam arena blok
         * tidak pernah ter-intern (cp=0 → cpool[0]) sebelum ini */
        collect_cp_stmt(s->d);
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
    while (!at(T_EOF)) {
        if (at(KW_ENUM)) parse_enum();          /* v1.46 */
        else if (at(KW_TRAIT)) parse_trait();   /* v1.46 */
        else parse_class();                     /* class + impl */
    }
    resolve();
}
