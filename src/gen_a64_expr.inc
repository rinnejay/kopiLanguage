/* gen_a64_expr.inc — ekspresi backend a64 (bagian dari gen_a64.c).
 * Dipisah agar gen_a64.c tidak monolitik; bukan unit kompilasi sendiri. */

static int gen_bin_typed(Method *m, Node *e) {
    Ty lt = (Ty)e->a->dty, rt = (Ty)e->b->dty;
    char op = e->str[0];
    int two = e->str[1] != 0;
    int isShift = !strcmp(e->str, "<<") || !strcmp(e->str, ">>") ||
                  !strcmp(e->str, ">>>");
    int isCmp = (op == '=' || op == '!') && two;
    if (!isCmp)
        isCmp = op == '<' || op == '>' || !strcmp(e->str, "<=") ||
                !strcmp(e->str, ">=");
    Ty P = isShift ? lt : ty_promote(lt, rt);

    /* identitas objek (v1.12): ==/!= pada dua array → bandingkan pointer 64-bit */
    if (ty_is_arr(lt) && lt == rt && isCmp) {
        gen_expr(m, e->a);
        push_x();
        gen_expr(m, e->b);
        pop_x("x9");
        emit("\tcmp\tx9, x0");
        emit("\tcset\tw0, %s", op == '=' ? "eq" : "ne");
        return 0;
    }

    gen_expr(m, e->a);
    widen_ty(lt);
    conv_ty(lt, P);
    push_x();
    gen_expr(m, e->b);
    widen_ty(rt);
    if (!isShift) conv_ty(rt, P);
    pop_x("x9");

    if (ty_is_float(P)) {
        const char *pre = P == T_FLOAT ? "s" : "d";
        if (P == T_FLOAT) { emit("\tfmov\ts0, w9"); emit("\tfmov\ts1, w0"); }
        else { emit("\tfmov\td0, x9"); emit("\tfmov\td1, x0"); }
        if (isCmp) {
            emit("\tfcmp\t%s0, %s1", pre, pre);
            if (op == '=') emit("\tcset\tw0, eq");
            else if (op == '!') emit("\tcset\tw0, ne");
            else if (!strcmp(e->str, "<=")) emit("\tcset\tw0, le");
            else if (!strcmp(e->str, ">=")) emit("\tcset\tw0, ge");
            else if (op == '<') emit("\tcset\tw0, lt");
            else emit("\tcset\tw0, gt");
            return 0;
        }
        if (op == '+') emit("\tfadd\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '-') emit("\tfsub\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '*') emit("\tfmul\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '/') emit("\tfdiv\t%s0, %s0, %s1", pre, pre, pre);
        else if (op == '%') {
            emit("\tfdiv\t%s2, %s0, %s1", pre, pre, pre);
            emit("\tfrintz\t%s2, %s2", pre, pre);
            emit("\tfmul\t%s2, %s2, %s1", pre, pre, pre);
            emit("\tfsub\t%s0, %s0, %s2", pre, pre, pre);
        } else {
            die("gen_a64: operator float '%s'", e->str);
        }
        if (P == T_FLOAT) emit("\tfmov\tw0, s0");
        else emit("\tfmov\tx0, d0");
        return is64ty(P);
    }

    if (isShift) {
        int mask = ty_width(P) - 1;
        if (mask <= 0) mask = 31;
        emit("\tmov\tw10, #%d", mask);
        emit("\tand\tw10, w0, w10");
        if (!strcmp(e->str, "<<")) emit("\tlslv\tx0, x9, x10");
        else if (!strcmp(e->str, ">>")) emit("\tasrv\tx0, x9, x10");
        else emit("\tlsrv\tx0, x9, x10");
        norm_ty(P);
        return is64ty(P);
    }
    if (isCmp) {
        int uns = ty_is_uint(P);
        emit("\tcmp\tx9, x0");
        if (op == '=') emit("\tcset\tw0, eq");
        else if (op == '!') emit("\tcset\tw0, ne");
        else if (!strcmp(e->str, "<=")) emit(uns ? "\tcset\tw0, ls" : "\tcset\tw0, le");
        else if (!strcmp(e->str, ">=")) emit(uns ? "\tcset\tw0, hs" : "\tcset\tw0, ge");
        else if (op == '<') emit(uns ? "\tcset\tw0, lo" : "\tcset\tw0, lt");
        else emit(uns ? "\tcset\tw0, hi" : "\tcset\tw0, gt");
        return 0;
    }
    int uns = ty_is_uint(P);
    switch (op) {
    case '+': emit("\tadd\tx0, x9, x0"); break;
    case '-': emit("\tsub\tx0, x9, x0"); break;
    case '*': emit("\tmul\tx0, x9, x0"); break;
    case '/': emit(uns ? "\tudiv\tx0, x9, x0" : "\tsdiv\tx0, x9, x0"); break;
    case '%':
        emit(uns ? "\tudiv\tx10, x9, x0" : "\tsdiv\tx10, x9, x0");
        emit("\tmsub\tx0, x10, x0, x9");
        break;
    case '&': emit("\tand\tx0, x9, x0"); break;
    case '|': emit("\torr\tx0, x9, x0"); break;
    case '^': emit("\teor\tx0, x9, x0"); break;
    default: die("gen_a64: operator '%s'", e->str);
    }
    norm_ty(P);
    return is64ty(P);
}

/* hasil: 0 = int/bool di w0 (32-bit); 1 = POINTER closure di x0 (64-bit) */
static int gen_expr(Method *m, Node *e) {
    switch (e->kind) {
    case EX_INT: {
        Ty t = (Ty)e->dty;
        if (t == T_INT) {
            long v = e->ival;
            if (v >= 0 && v <= 65535) {
                emit("\tmov\tw0, #%ld", v);
            } else {
                uint32_t u = (uint32_t)v;
                emit("\tmovz\tw0, #0x%lx", (long)(u & 0xffff));
                if ((u >> 16) & 0xffff)
                    emit("\tmovk\tw0, #0x%lx, lsl #16", (long)((u >> 16) & 0xffff));
            }
            return 0;
        }
        if (t == T_FLOAT) {
            union { float f; uint32_t u; } cv;
            cv.f = (float)e->fval;
            emit("\tmovz\tw0, #0x%lx", (long)(cv.u & 0xffff));
            if (cv.u >> 16) emit("\tmovk\tw0, #0x%lx, lsl #16", (long)(cv.u >> 16));
            return 1;
        }
        if (t == T_DOUBLE) {
            union { double d; unsigned long long u; } cv;
            cv.d = e->fval;
            mov64_imm(cv.u);
            return 1;
        }
        if (t == T_UINT32 || t == T_UINT8 || t == T_UINT16 || t == T_BYTE || t == T_SHORT) {
            uint32_t u = (uint32_t)e->ival;
            emit("\tmovz\tw0, #0x%lx", (long)(u & 0xffff));
            if (u >> 16) emit("\tmovk\tw0, #0x%lx, lsl #16", (long)(u >> 16));
            return 1;
        }
        mov64_imm((unsigned long long)e->ival);
        return 1;
    }
    case EX_BOOL:
        if (e->ival) emit("\tmov\tw0, #1");
        else emit("\tmov\tw0, wzr");
        return 0;
    case EX_STRLIT: {
        Node f;
        memset(&f, 0, sizeof f);
        f.kind = EX_NET;
        f.ival = 5;
        f.cp = e->cp;
        f.dty = (int)T_ARI;
        return gen_expr(m, &f);
    }
    case EX_VAR:
        if (is64ty((Ty)e->dty)) {
            loc_load((e->ival + 1) * 8, "x0");
            return 1;
        }
        loc_load((e->ival + 1) * 8, "w0");
        return 0;
    case EX_UN: {
        Ty t = (Ty)e->dty;
        int wide = is64ty(t);
        gen_expr(m, e->a);
        if (e->str[0] == '!') { emit("\teor\tw0, w0, #1"); return 0; }
        if (ty_is_float(t)) {
            if (t == T_FLOAT) { emit("\tfmov\ts0, w0"); emit("\tfneg\ts0, s0"); emit("\tfmov\tw0, s0"); }
            else { emit("\tfmov\td0, x0"); emit("\tfneg\td0, d0"); emit("\tfmov\tx0, d0"); }
            return 1;
        }
        if (!wide) {
            if (e->str[0] == '-') emit("\tneg\tw0, w0");
            else emit("\tmvn\tw0, w0");
            return 0;
        }
        widen_ty(t);
        if (e->str[0] == '-') emit("\tneg\tx0, x0");
        else emit("\tmvn\tx0, x0");
        norm_ty(t);
        return 1;
    }
    case EX_CALL: {
        int n = 0;
        int w64[64];
        Param *pp = e->meth ? e->meth->params : 0;
        for (Node *a = e->a; a; a = a->next, pp = pp ? pp->next : 0) {
            if (n >= 8) die("baris %d: maksimal 8 argumen untuk target native", e->line);
            Ty at = (Ty)a->dty;
            Ty pt = pp ? (Ty)pp->ty : T_INT;
            gen_expr(m, a);
            if (pt == T_FN) {
                w64[n] = 1;
            } else {
                widen_ty(at);
                conv_ty(at, pt);
                w64[n] = is64ty(pt);
            }
            push_val(w64[n]);
            n++;
        }
        /* arg k berada di offset (n-1-k)*16 dari sp (urut push terbalik) */
        for (int k = 0; k < n; k++) {
            if (w64[k]) emit("\tldr\tx%d, [sp, #%d]", k, (n - 1 - k) * 16);
            else emit("\tldr\tw%d, [sp, #%d]", k, (n - 1 - k) * 16);
        }
        if (n > 0) emit("\tadd\tsp, sp, #%d", n * 16);
        /* label: Class__name */
        {
            const char *dot = strchr(e->str, '.');
            char cls[256];
            size_t cl = (size_t)(dot - e->str);
            memcpy(cls, e->str, cl); cls[cl] = 0;
            emit("\tbl\t%s__%s", cls, dot + 1);
        }
        return is64ty((Ty)e->dty);       /* int[] / fn → x0; int → w0 */
    }
    case EX_ARROW: {
        Method *hm = (Method *)e->cp;
        long ncap = hm->ncapt;
        emit("\tmov\tx0, #%ld", 8 * (3 + ncap));          /* ukuran blok */
        emit("\tbl\tkopalloc");                          /* arena bump */
        emit("\tadrp\tx1, %s__%s", hm->cls, hm->name);    /* kode */
        emit("\tadd\tx1, x1, :lo12:%s__%s", hm->cls, hm->name);
        emit("\tstr\tx1, [x0]");
        emit("\tmov\tw2, #%ld", ncap);
        emit("\tstr\tw2, [x0, #8]");
        emit("\tmov\tw2, #%ld", hm->nparams - 1);         /* nuser */
        emit("\tstr\tw2, [x0, #16]");
        for (int j = 0; j < ncap; j++) {
            loc_load((hm->captslot[j] + 1) * 8, is64ty(hm->captty[j]) ? "x2" : "w2");
            emit("\tstr\tx2, [x0, #%ld]", 24 + 8 * j);
        }
        return 1;
    }
    case EX_FCALL: {
        /* argumen digunakan dulu (lebih dahulu di stack), lalu CALLEE → x19 */
        int n = 0;
        for (Node *a = e->a; a; a = a->next) {
            if (n >= 6) die("baris %d: maksimal 5 argumen pemanggilan lambda", e->line);
            gen_expr(m, a);
            push_val(0);                 /* argumen selalu int (v1) */
            n++;
        }
        gen_expr(m, e->b);               /* x0 = pointer closure */
        emit("\tmov\tx19, x0");
        for (int k = 0; k < n; k++)
            emit("\tldr\tw%d, [sp, #%d]", k + 1, (n - 1 - k) * 16);
        if (n > 0) emit("\tadd\tsp, sp, #%d", n * 16);
        /* cek arity runtime: nuser (env[2]) != n → mati rapi */
        emit("\tldr\tw9, [x19, #16]");
        emit("\tcmp\tw9, #%d", n);
        emit("\tb.ne\t.Klam");
        emit("\tmov\tx0, x19");          /* arg0 = env */
        emit("\tldr\tx20, [x19]");       /* kode */
        emit("\tblr\tx20");
        return 0;                        /* hasil kembalian: int di w0 */
    }
    case EX_NEWARR: {
        gen_expr(m, e->a);               /* w0 = n */
        emit("\tcmp\tw0, #0");
        emit("\tb.lt\t.Karr");          /* ukuran salah */
        emit("\tsxtw\tx9, w0");
        emit("\tlsl\tx0, x9, #3");      /* 8*n */
        emit("\tadd\tx0, x0, #8");      /* + header len */
        emit("\tbl\tkopalloc");
        emit("\tstr\tx9, [x0]");        /* len */
        {
            int L = arrlbl++;
            emit("\tmov\tx10, #0");
            emit(".LNZ%d:", L);
            emit("\tcmp\tx10, x9");
            emit("\tb.hs\t.LNZ%de", L);
            emit("\tadd\tx11, x0, #8");
            emit("\tstr\txzr, [x11, x10, lsl #3]");
            emit("\tadd\tx10, x10, #1");
            emit("\tb\t.LNZ%d", L);
            emit(".LNZ%de:", L);
        }
        return 1;
    }
    case EX_IDX: {                       /* hasil: w0 = elemen (32-bit) */
        gen_expr(m, e->a);               /* x0 = array/String pointer */
        push_val(1);
        gen_expr(m, e->b);               /* w0 = idx */
        emit("\tldr\tx1, [sp], #16");
        emit("\tldr\tw2, [x1]");        /* len */
        emit("\tcmp\tw0, w2");
        emit("\tb.hs\t.Karr");          /* (>0 unsigned): negatif ikut tertangkap */
        emit("\tsxtw\tx9, w0");
        emit("\tadd\tx1, x1, #8");
        if ((Ty)e->a->dty == T_STR) {   /* v1.18: String packed — byte di dalam slot */
            emit("\tldrb\tw0, [x1, x9]");
            return 0;
        }
        emit("\tldr\tx0, [x1, x9, lsl #3]");
        {   /* slot 64-bit (long/String/objek): jangan pangkas ke 32-bit */
            Ty et = (Ty)e->dty;
            if (ty_width(et) != 64 && et != T_ARI && et != T_STR && et != T_FN)
                emit("\tmov\tw0, w0");      /* nilai int tetap 32-bit */
        }
        return 0;
    }
    case EX_LEN:
        gen_expr(m, e->a);
        emit("\tldr\tw0, [x0]");
        return 0;
    case EX_NET: {                       /* syscall Linux langsung (tanpa libc) */
        int ln = netlbl++;                /* label unik per kemunculan */
        /* 0=socket(domain,type) 1=connect(fd,ipBE,port) 2=rdb(fd)
         * 3=wrb(fd,b) 4=close(fd) 5=bytes("lit") → int[]                    */
        if (e->ival == 5) {              /* Net.bytes("literal") → String packed (v1.18) */
            int c = (int)e->cp;
            emit("\tadrp\tx9, :got:cpool%d", c);
            emit("\tldr\tx9, [x9, :got_lo12:cpool%d]", c);
            emit("\tmov\tx0, x9");
            emit("\tbl\tkop_str_from_cstr");
            return 1;                       /* hasil pointer String packed */
        }
        if (e->ival == 6) {              /* Net.openr("literal") → fd          */
            int c = (int)e->cp;
            emit("\tadrp\tx9, :got:cpool%d", c);
            emit("\tldr\tx9, [x9, :got_lo12:cpool%d]", c);
            emit("\tmov\tx10, #0");
            emit(".LNO%da:", ln);
            emit("\tldrb\tw1, [x9, x10]");
            emit("\tadd\tx10, x10, #1");
            emit("\tcbnz\tw1, .LNO%da", ln);
            /* openat(AT_FDCWD=-100, path, O_RDONLY) */
            emit("\tmovn\tw0, #99");                   /* -100 */
            emit("\tmov\tx1, x9");                     /* path */
            emit("\tmov\tx2, #0");                     /* flags = O_RDONLY */
            emit("\tmov\tx8, #56");                    /* __NR_openat */
            emit("\tsvc\t#0");
            return 0;                                  /* fd | -errno */
        }
        if (e->ival == 7 || e->ival == 8) {
            /* recvb(fd, buf) / sendb(fd, buf): buf adalah pointer 64-bit */
            for (Node *aa = e->a; aa; aa = aa->next) {
                gen_expr(m, aa);
                push_val(aa == e->a ? 0 : 1);          /* fd int, buf array */
            }
        }
        if (e->ival == 7) {              /* recvb(fd, buf) → n | -1            */
            /* stack: [0..15]=buf (64-bit), [16..31]=fd; tmp via .comm */
            emit("\tldr\tx10, [sp]");                  /* buf */
            emit("\tldr\tw11, [x10]");                 /* len */
            emit("\tcmp\tw11, #0");
            emit("\tb.le\t.LRV%dz", ln);
            emit("\tmov\tw9, #0");                     /* got */
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            emit(".LRV%dl:", ln);
            emit("\tcmp\tw9, w11");
            emit("\tb.hs\t.LRV%df", ln);
            emit("\tsub\tw12, w11, w9");               /* sisa */
            emit("\tcmp\tw12, #240");
            emit("\tb.ls\t.LRV%dc", ln);
            emit("\tmov\tw12, #240");
            emit(".LRV%dc:", ln);
            /* salin ke tmp: tmp[k] = buf[1 + got + k] */
            emit("\tmov\tw13, #0");
            emit(".LRV%dcs:", ln);
            emit("\tcmp\tw13, w12");
            emit("\tb.hs\t.LRV%dcf", ln);
            emit("\tadd\tw15, w13, w9");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tldr\tx16, [x10, x15]");
            emit("\tstrb\tw16, [x17, x13]");
            emit("\tadd\tw13, w13, #1");
            emit("\tb\t.LRV%dcs", ln);
            emit(".LRV%dcf:", ln);
            emit("\tmov\tw2, w12");                    /* want */
            emit("\tmov\tx1, x17");                    /* tmp */
            emit("\tldr\tw0, [sp, #16]");              /* fd */
            emit("\tmov\tx8, #63");                    /* __NR_read */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #0");
            emit("\tb.lt\t.LRV%dry", ln);              /* galat keras */
            emit("\tb.eq\t.LRV%de", ln);               /* EOF */
            /* salin kembali: buf[1 + got + k] = tmp[k], k < x0 */
            emit("\tmov\tx13, #0");
            emit(".LRV%drs:", ln);
            emit("\tcmp\tx13, x0");
            emit("\tb.hs\t.LRV%drf", ln);
            emit("\tldrb\tw16, [x17, x13]");
            emit("\tadd\tw15, w13, w9");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tstr\tx16, [x10, x15]");
            emit("\tadd\tx13, x13, #1");
            emit("\tb\t.LRV%drs", ln);
            emit(".LRV%drf:", ln);
            emit("\tadd\tw9, w9, w0");                 /* got += nread */
            emit("\tb\t.LRV%dl", ln);
            emit(".LRV%dry:", ln);                     /* galat → -1 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tneg\tw0, wzr");
            emit("\tb\t.LRV%dh", ln);
            emit(".LRV%de:", ln);                      /* EOF → got */
            emit(".LRV%df:", ln);                      /* penuh → got */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tmov\tw0, w9");
            emit("\tb\t.LRV%dh", ln);
            emit(".LRV%dz:", ln);                      /* len<=0 → 0 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tmov\tw0, wzr");
            emit(".LRV%dh:", ln);
            return 0;
        }
        if (e->ival == 8) {              /* sendb(fd, buf) → 0 | -1            */
            emit("\tldr\tx10, [sp]");                  /* buf */
            emit("\tldr\tw11, [x10]");                 /* len */
            emit("\tcmp\tw11, #0");
            emit("\tb.le\t.LRS%df", ln);
            emit("\tmov\tw9, #0");                     /* terkirim */
            emit(".LRS%dl:", ln);
            emit("\tcmp\tw9, w11");
            emit("\tb.hs\t.LRS%df", ln);
            emit("\tsub\tw12, w11, w9");
            emit("\tcmp\tw12, #240");
            emit("\tb.ls\t.LRS%dc", ln);
            emit("\tmov\tw12, #240");
            emit(".LRS%dc:", ln);
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            /* salin ke tmp: tmp[k] = buf[1 + terkirim + k] */
            emit("\tmov\tw13, #0");
            emit(".LRS%dcs:", ln);
            emit("\tcmp\tw13, w12");
            emit("\tb.hs\t.LRS%dcf", ln);
            emit("\tadd\tw15, w13, w9");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tldr\tx16, [x10, x15]");
            emit("\tstrb\tw16, [x17, x13]");
            emit("\tadd\tw13, w13, #1");
            emit("\tb\t.LRS%dcs", ln);
            emit(".LRS%dcf:", ln);
            emit("\tmov\tw2, w12");
            emit("\tmov\tx1, x17");
            emit("\tldr\tw0, [sp, #16]");
            emit("\tmov\tx8, #64");                    /* __NR_write */
            emit("\tsvc\t#0");
            emit("\tcmp\tw0, #0");
            emit("\tb.lt\t.LRS%de", ln);
            emit("\tadd\tw9, w9, w0");                 /* penuh/parsial */
            emit("\tb\t.LRS%dl", ln);
            emit(".LRS%de:", ln);                      /* galat → -1 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tneg\tw0, wzr");
            emit("\tb\t.LRS%dh", ln);
            emit(".LRS%df:", ln);                      /* selesai/len 0 → 0 */
            emit("\tadd\tsp, sp, #%d", 2 * 16);
            emit("\tmov\tw0, wzr");
            emit(".LRS%dh:", ln);
            return 0;
        }
        if (e->ival >= 9 && e->ival <= 12) {
            /* bind/sendto/recvfrom/shutdown — argumen array di-push 64-bit */
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                int isArr = (e->ival == 10 && idx == 3) ||
                            (e->ival == 11 && (idx == 1 || idx == 2));
                gen_expr(m, aa);
                push_val(isArr);
            }
            if (e->ival == 9) {             /* bind(fd, ip, port) → 0/-errno */
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tldr\tw6, [sp, #32]");           /* ip */
                emit("\tldr\tw7, [sp, #16]");           /* port */
                emit("\tlsr\tw4, w7, #8");
                emit("\tstrb\tw4, [sp, #2]");
                emit("\tand\tw4, w7, #0xff");
                emit("\tstrb\tw4, [sp, #3]");
                emit("\tlsr\tw4, w6, #24");
                emit("\tstrb\tw4, [sp, #4]");
                emit("\tlsr\tw4, w6, #16");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #5]");
                emit("\tlsr\tw4, w6, #8");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #6]");
                emit("\tand\tw4, w6, #0xff");
                emit("\tstrb\tw4, [sp, #7]");
                emit("\tmovz\tw4, #2");
                emit("\tstrb\tw4, [sp, #0]");
                emit("\tldr\tx0, [sp, #48]");           /* fd */
                emit("\tmov\tx1, sp");
                emit("\tmov\tw2, #16");
                emit("\tmov\tx8, #200");                /* __NR_bind */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #64");
                return 0;
            }
            if (e->ival == 12) {            /* shutdown(fd, how) */
                emit("\tldr\tw0, [sp, #16]");
                emit("\tldr\tw1, [sp, #0]");
                emit("\tmov\tx8, #210");                /* __NR_shutdown */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #32");
                return 0;
            }
            if (e->ival == 10) {            /* sendto(fd, ip, port, buf) → n */
                emit("\tstp\txzr, xzr, [sp, #-16]!");  /* sa */
                emit("\tldr\tx10, [sp, #16]");          /* buf */
                emit("\tldr\tw6, [sp, #48]");           /* ip */
                emit("\tldr\tw7, [sp, #32]");           /* port */
                emit("\tlsr\tw4, w7, #8");
                emit("\tstrb\tw4, [sp, #2]");
                emit("\tand\tw4, w7, #0xff");
                emit("\tstrb\tw4, [sp, #3]");
                emit("\tlsr\tw4, w6, #24");
                emit("\tstrb\tw4, [sp, #4]");
                emit("\tlsr\tw4, w6, #16");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #5]");
                emit("\tlsr\tw4, w6, #8");
                emit("\tand\tw4, w4, #0xff");
                emit("\tstrb\tw4, [sp, #6]");
                emit("\tand\tw4, w6, #0xff");
                emit("\tstrb\tw4, [sp, #7]");
                emit("\tmovz\tw4, #2");
                emit("\tstrb\tw4, [sp, #0]");
                /* pak ke iobuf (maks 240 byte per datagram, batas v1) */
                emit("\tldr\tw11, [x10]");
                emit("\tmov\tw3, #1200");
                emit("\tcmp\tw11, w3");
                emit("\tb.ls\t.LST%dc", ln);
                emit("\tmov\tw11, #1200");
                emit(".LST%dc:", ln);
                emit("\tadrp\tx17, kop_iobuf");
                emit("\tadd\tx17, x17, :lo12:kop_iobuf");
                emit("\tmov\tw13, #0");
                emit(".LST%dk:", ln);
                emit("\tcmp\tw13, w11");
                emit("\tb.hs\t.LST%df", ln);
                emit("\tsxtw\tx15, w13");
                emit("\tlsl\tx15, x15, #3");
                emit("\tadd\tx15, x15, #8");
                emit("\tldr\tx16, [x10, x15]");
                emit("\tstrb\tw16, [x17, x13]");
                emit("\tadd\tw13, w13, #1");
                emit("\tb\t.LST%dk", ln);
                emit(".LST%df:", ln);
                emit("\tldr\tx0, [sp, #64]");           /* fd */
                emit("\tmov\tx1, x17");
                emit("\tmov\tw2, w11");
                emit("\tmov\tx3, #0");
                emit("\tmov\tx4, sp");
                emit("\tmov\tx5, #16");
                emit("\tmov\tx8, #206");                /* __NR_sendto */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #80");
                return 0;
            }
            if (e->ival == 11) {            /* recvfrom(fd, buf, from) → n */
                emit("\tstp\txzr, xzr, [sp, #-16]!");  /* sa */
                emit("\tstp\txzr, xzr, [sp, #-16]!");  /* addrlen */
                emit("\tmov\tw9, #16");
                emit("\tstr\tw9, [sp]");
                emit("\tadrp\tx17, kop_iobuf");
                emit("\tadd\tx17, x17, :lo12:kop_iobuf");
                emit("\tldr\tx0, [sp, #64]");           /* fd */
                emit("\tmov\tx1, x17");
                emit("\tmov\tw2, #1200");
                emit("\tmov\tx3, #0");
                emit("\tadd\tx4, sp, #16");
                emit("\tmov\tx5, sp");
                emit("\tmov\tx8, #207");                /* __NR_recvfrom */
                emit("\tsvc\t#0");
                emit("\tcmp\tx0, #0");
                emit("\tb.lt\t.LRF%de", ln);
                emit("\tmov\tw11, w0");                /* simpan n */
                emit("\tldr\tx10, [sp, #48]");          /* buf */
                emit("\tmov\tw13, #0");
                emit(".LRF%dk:", ln);
                emit("\tcmp\tw13, w11");
                emit("\tb.hs\t.LRF%df", ln);
                emit("\tldrb\tw16, [x17, x13]");
                emit("\tsxtw\tx15, w13");
                emit("\tlsl\tx15, x15, #3");
                emit("\tadd\tx15, x15, #8");
                emit("\tstr\tx16, [x10, x15]");
                emit("\tadd\tw13, w13, #1");
                emit("\tb\t.LRF%dk", ln);
                emit(".LRF%df:", ln);
                /* from[0] = ip, from[1] = port (host order) */
                emit("\tldr\tx12, [sp, #32]");
                emit("\tldrb\tw6, [sp, #20]");
                emit("\tlsl\tw6, w6, #24");
                emit("\tldrb\tw4, [sp, #21]");
                emit("\tlsl\tw4, w4, #16");
                emit("\torr\tw6, w6, w4");
                emit("\tldrb\tw4, [sp, #22]");
                emit("\tlsl\tw4, w4, #8");
                emit("\torr\tw6, w6, w4");
                emit("\tldrb\tw4, [sp, #23]");
                emit("\torr\tw6, w6, w4");
                emit("\tldrb\tw3, [sp, #18]");
                emit("\tlsl\tw3, w3, #8");
                emit("\tldrb\tw4, [sp, #19]");
                emit("\torr\tw3, w3, w4");
                emit("\tstr\tw6, [x12, #8]");
                emit("\tstr\tw3, [x12, #16]");
                emit("\tmov\tw0, w11");
                emit("\tb\t.LRF%dh", ln);
                emit(".LRF%de:", ln);                    /* galat → x0 apa adanya */
                emit(".LRF%dh:", ln);
                emit("\tadd\tsp, sp, #80");
                return 0;
            }
        }
        if (e->ival >= 13 && e->ival <= 21) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                int isArr = (e->ival == 13 && idx == 0) ||
                            (e->ival == 14 || e->ival == 15) ||
                            (e->ival == 16 && idx < 2);
                push_val(isArr);
            }
            if (e->ival == 13) {            /* openb(path, flags, mode) */
                emit("\tldr\tx10, [sp, #32]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tmovn\tw0, #99");               /* AT_FDCWD */
                emit("\tmov\tx1, x17");
                emit("\tldr\tw2, [sp, #16]");          /* flags */
                emit("\tldr\tw3, [sp, #0]");           /* mode */
                emit("\tmov\tx8, #56");                /* __NR_openat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #48");
                return 0;
            }
            if (e->ival == 14) {            /* mkdirb(path) */
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tmovn\tw0, #99");
                emit("\tmov\tx1, x17");
                emit("\tmov\tw2, #493");               /* 0755 */
                emit("\tmov\tx8, #34");                /* __NR_mkdirat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 15) {            /* unlinkb(path) */
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tmovn\tw0, #99");
                emit("\tmov\tx1, x17");
                emit("\tmov\tx2, #0");
                emit("\tmov\tx8, #35");                /* __NR_unlinkat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 16) {            /* renameb(old, new) */
                emit("\tldr\tx10, [sp, #16]");
                pack_path_bytes(ln, 'a', 0);
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'b', 2048);
                emit("\tmovn\tw0, #99");
                emit("\tsub\tx1, x17, #2048");         /* path lama */
                emit("\tmovn\tw2, #99");
                emit("\tmov\tx3, x17");                /* path baru */
                emit("\tmov\tx8, #38");                /* __NR_renameat */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #32");
                return 0;
            }
            if (e->ival == 17) {            /* seek(fd, off, whence) */
                emit("\tldr\tw0, [sp, #32]");
                emit("\tldr\tw1, [sp, #16]");
                emit("\tsxtw\tx1, w1");
                emit("\tldr\tw2, [sp, #0]");
                emit("\tmov\tx8, #62");                /* __NR_lseek */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #48");
                return 0;
            }
            if (e->ival == 18) {            /* time() → detik epoch */
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tmov\tw0, #0");                 /* CLOCK_REALTIME */
                emit("\tmov\tx1, sp");
                emit("\tmov\tx8, #113");               /* __NR_clock_gettime */
                emit("\tsvc\t#0");
                emit("\tldr\tx0, [sp]");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 19) {            /* mono() → milidetik monotonik */
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tmov\tw0, #1");                 /* CLOCK_MONOTONIC */
                emit("\tmov\tx1, sp");
                emit("\tmov\tx8, #113");
                emit("\tsvc\t#0");
                emit("\tldr\tx1, [sp]");
                emit("\tldr\tx2, [sp, #8]");
                emit("\tmov\tx3, #1000");
                emit("\tmul\tx0, x1, x3");
                emit("\tmovz\tx3, #0x4240");
                emit("\tmovk\tx3, #0xf, lsl #16");
                emit("\tudiv\tx2, x2, x3");
                emit("\tadd\tx0, x0, x2");
                emit("\tadd\tsp, sp, #16");
                return 1;
            }
            if (e->ival == 20) {            /* sleep(ms) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tmov\tw1, #1000");
                emit("\tudiv\tw2, w0, w1");            /* detik */
                emit("\tmsub\tw3, w2, w1, w0");        /* sisa ms */
                emit("\tmovz\tw4, #0x4240");
                emit("\tmovk\tw4, #0xf, lsl #16");
                emit("\tmul\tw3, w3, w4");
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tsxtw\tx2, w2");
                emit("\tstr\tx2, [sp]");
                emit("\tsxtw\tx3, w3");
                emit("\tstr\tx3, [sp, #8]");
                emit("\tmov\tx0, sp");
                emit("\tmov\tx1, #0");
                emit("\tmov\tx8, #115");               /* __NR_nanosleep */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                emit("\tadd\tsp, sp, #16");
                emit("\tmov\tw0, wzr");
                return 0;
            }
            if (e->ival == 21) {            /* exit(code) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tmov\tx8, #93");                /* __NR_exit */
                emit("\tsvc\t#0");
                return 0;
            }
        }
        if (e->ival >= 22 && e->ival <= 24) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                push_val(1);                    /* pointer array/String */
            }
            if (e->ival == 22) {                /* cat(a,b) → kop_cat_packed (v1.18) */
                emit("\tldr\tx0, [sp, #16]");       /* a */
                emit("\tldr\tx1, [sp]");            /* b */
                emit("\tbl\tkop_cat_packed");
                emit("\tadd\tsp, sp, #32");
                return 1;
            }
            /* sequal / sne → kop_sequal_packed (v1.18) */
            emit("\tldr\tx0, [sp, #16]");       /* a */
            emit("\tldr\tx1, [sp]");            /* b */
            emit("\tbl\tkop_sequal_packed");
            emit("\tadd\tsp, sp, #32");
            if (e->ival == 24) emit("\teor\tw0, w0, #1");
            return 0;
        }
        if (e->ival >= 25 && e->ival <= 32) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                push_val(e->ival == 32 && idx == 0);
            }
            if (e->ival == 25) {             /* listen(fd, backlog) */
                emit("\tldr\tw0, [sp, #16]");
                emit("\tldr\tw1, [sp, #0]");
                emit("\tmov\tx8, #201");
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #32");
                return 0;
            }
            if (e->ival == 26) {             /* accept(fd) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tmov\tx1, #0");
                emit("\tmov\tx2, #0");
                emit("\tmov\tx3, #0");
                emit("\tmov\tx8, #242");            /* __NR_accept4 */
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 27) {             /* setsockopt(fd,lvl,opt,val) */
                emit("\tldr\tw0, [sp, #48]");
                emit("\tldr\tw1, [sp, #32]");
                emit("\tldr\tw2, [sp, #16]");
                emit("\tldr\tw3, [sp, #0]");
                emit("\tstp\txzr, xzr, [sp, #-16]!");
                emit("\tstr\tw3, [sp]");
                emit("\tmov\tx3, sp");
                emit("\tmov\tx8, #208");
                emit("\tsvc\t#0");
                emit("\tadd\tsp, sp, #16");
                emit("\tadd\tsp, sp, #64");
                return 0;
            }
            if (e->ival == 28) {             /* memmark() */
                emit("\tadrp\tx9, kop_arena_used");
                emit("\tadd\tx9, x9, :lo12:kop_arena_used");
                emit("\tldr\tw0, [x9]");
                return 0;
            }
            if (e->ival == 29) {             /* memreset(mark) */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tadrp\tx9, kop_arena_used");
                emit("\tadd\tx9, x9, :lo12:kop_arena_used");
                emit("\tcmp\tw0, #0");
                emit("\tb.ge\t.LMR%dP", ln);
                emit("\tmov\tw0, wzr");
                emit(".LMR%dP:", ln);
                emit("\tldr\tw10, [x9]");
                emit("\tcmp\tw0, w10");
                emit("\tb.le\t.LMR%dS", ln);
                emit("\tmov\tw0, w10");
                emit(".LMR%dS:", ln);
                emit("\tstr\tw0, [x9]");
                emit("\tadd\tsp, sp, #16");
                return 0;
            }
            if (e->ival == 30) {             /* argc() */
                emit("\tadrp\tx9, kop_argc");
                emit("\tadd\tx9, x9, :lo12:kop_argc");
                emit("\tldr\tw0, [x9]");
                return 0;
            }
            if (e->ival == 31) {             /* argv(i) → String */
                emit("\tldr\tw0, [sp, #0]");
                emit("\tadrp\tx9, kop_argc");
                emit("\tadd\tx9, x9, :lo12:kop_argc");
                emit("\tldr\tw9, [x9]");
                emit("\tcmp\tw0, #0");
                emit("\tb.lt\t.LAV%dE", ln);
                emit("\tcmp\tw0, w9");
                emit("\tb.hs\t.LAV%dE", ln);
                emit("\tadrp\tx10, kop_argv");
                emit("\tadd\tx10, x10, :lo12:kop_argv");
                emit("\tldr\tx10, [x10]");
                emit("\tsxtw\tx11, w0");
                emit("\tldr\tx0, [x10, x11, lsl #3]");
                emit("\tbl\tkop_str_from_cstr");
                emit("\tb\t.LAV%dD", ln);
                emit(".LAV%dE:", ln);
                emit("\tadrp\tx0, .Lempty_str");
                emit("\tadd\tx0, x0, :lo12:.Lempty_str");
                emit("\tbl\tkop_str_from_cstr");
                emit(".LAV%dD:", ln);
                emit("\tadd\tsp, sp, #16");
                return 1;
            }
            if (e->ival == 32) {             /* getenv(name) → String */
                emit("\tldr\tx10, [sp, #0]");
                pack_path_bytes(ln, 'g', 0);
                emit("\tadrp\tx0, kop_iobuf");
                emit("\tadd\tx0, x0, :lo12:kop_iobuf");
                emit("\tbl\tgetenv");
                emit("\tcbz\tx0, .LGE%dE", ln);
                emit("\tbl\tkop_str_from_cstr");
                emit("\tb\t.LGE%dD", ln);
                emit(".LGE%dE:", ln);
                emit("\tadrp\tx0, .Lempty_str");
                emit("\tadd\tx0, x0, :lo12:.Lempty_str");
                emit("\tbl\tkop_str_from_cstr");
                emit(".LGE%dD:", ln);
                emit("\tadd\tsp, sp, #16");
                return 1;
            }
        }
        if (e->ival == 33) {                /* recv1(fd, buf) → n (satu read) */
            gen_expr(m, e->a);
            push_val(0);
            gen_expr(m, e->a->next);
            push_val(1);
            emit("\tldr\tx10, [sp]");
            emit("\tldr\tw0, [sp, #16]");
            emit("\tldr\tw11, [x10]");
            emit("\tcmp\tw11, #4000");
            emit("\tb.ls\t.LR1%dc", ln);
            emit("\tmov\tw11, #4000");
            emit(".LR1%dc:", ln);
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            emit("\tmov\tx1, x17");
            emit("\tmov\tw2, w11");
            emit("\tmov\tx8, #63");
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #0");
            emit("\tb.le\t.LR1%dd", ln);
            emit("\tmov\tw12, w0");
            emit("\tmov\tw13, #0");
            emit(".LR1%dk:", ln);
            emit("\tcmp\tw13, w12");
            emit("\tb.hs\t.LR1%df", ln);
            emit("\tldrb\tw16, [x17, x13]");
            emit("\tsxtw\tx15, w13");
            emit("\tlsl\tx15, x15, #3");
            emit("\tadd\tx15, x15, #8");
            emit("\tstr\tx16, [x10, x15]");
            emit("\tadd\tw13, w13, #1");
            emit("\tb\t.LR1%dk", ln);
            emit(".LR1%df:", ln);
            emit("\tmov\tw0, w12");
            emit(".LR1%dd:", ln);
            emit("\tadd\tsp, sp, #32");
            return 0;
        }
        if (e->ival == 39) {                /* pollfd(fds, revents, ms) */
            gen_expr(m, e->a);
            push_val(1);
            gen_expr(m, e->a->next);
            push_val(1);
            gen_expr(m, e->a->next->next);
            push_val(0);
            emit("\tsub\tsp, sp, #512");
            emit("\tldr\tx10, [sp, #544]");       /* fds */
            emit("\tldr\tw9, [x10]");             /* n */
            emit("\tcmp\tw9, #64");
            emit("\tb.le\t.LPF%dc", ln);
            emit("\tmov\tw9, #64");
            emit(".LPF%dc:", ln);
            emit("\tmov\tx13, #0");
            emit(".LPF%dl:", ln);
            emit("\tcmp\tx13, x9");
            emit("\tb.hs\t.LPF%dd", ln);
            emit("\tsxtw\tx4, w13");
            emit("\tlsl\tx4, x4, #3");
            emit("\tadd\tx6, x10, x4");
            emit("\tldr\tw14, [x6, #8]");
            emit("\tadd\tx6, sp, x4");
            emit("\tstr\tw14, [x6]");
            emit("\tmov\tw14, #1");               /* POLLIN */
            emit("\tstrh\tw14, [x6, #4]");
            emit("\tmov\tw14, #0");
            emit("\tstrh\tw14, [x6, #6]");
            emit("\tadd\tx13, x13, #1");
            emit("\tb\t.LPF%dl", ln);
            emit(".LPF%dd:", ln);
            emit("\tstr\tw9, [sp, #504]");        /* simpan n (caller-saved) */
            emit("\tmov\tx0, sp");
            emit("\tmov\tw1, w9");
            emit("\tldr\tw2, [sp, #512]");
            emit("\tbl\tpoll");
            emit("\tldr\tw9, [sp, #504]");
            emit("\tldr\tx15, [sp, #528]");       /* revents */
            emit("\tmov\tx13, #0");
            emit("\tmov\tw0, #0");                /* jumlah siap */
            emit(".LPF%dk:", ln);
            emit("\tcmp\tx13, x9");
            emit("\tb.hs\t.LPF%df", ln);
            emit("\tsxtw\tx4, w13");
            emit("\tlsl\tx4, x4, #3");
            emit("\tadd\tx6, sp, x4");
            emit("\tldrh\tw16, [x6, #6]");
            emit("\tmov\tw3, #0");
            emit("\tcbz\tw16, .LPF%dw", ln);
            emit("\ttst\tw16, #1");
            emit("\tb.eq\t.LPF%dni", ln);
            emit("\tmov\tw3, #1");
            emit(".LPF%dni:", ln);
            emit("\ttst\tw16, #56");              /* ERR|HUP|NVAL */
            emit("\tb.eq\t.LPF%dne", ln);
            emit("\torr\tw3, w3, #2");
            emit(".LPF%dne:", ln);
            emit(".LPF%dw:", ln);
            emit("\tadd\tx6, x15, x4");
            emit("\tsxtw\tx5, w3");
            emit("\tstr\tx5, [x6, #8]");
            emit("\tcbz\tw3, .LPF%dn", ln);
            emit("\tadd\tw0, w0, #1");
            emit(".LPF%dn:", ln);
            emit("\tadd\tx13, x13, #1");
            emit("\tb\t.LPF%dk", ln);
            emit(".LPF%df:", ln);
            emit("\tadd\tsp, sp, #512");
            emit("\tadd\tsp, sp, #48");
            return 0;
        }
        if (e->ival == 38) {                /* settimeout(fd, ms) */
            gen_expr(m, e->a);
            push_val(0);
            gen_expr(m, e->a->next);
            push_val(0);
            emit("\tldr\tw1, [sp, #0]");          /* ms */
            emit("\tmov\tw3, #1000");
            emit("\tudiv\tw2, w1, w3");           /* detik */
            emit("\tmsub\tw4, w2, w3, w1");       /* sisa ms */
            emit("\tmov\tw5, #1000");
            emit("\tmul\tw4, w4, w5");            /* mikrodetik */
            emit("\tstp\txzr, xzr, [sp, #-16]!");
            emit("\tsxtw\tx2, w2");
            emit("\tstr\tx2, [sp]");
            emit("\tsxtw\tx4, w4");
            emit("\tstr\tx4, [sp, #8]");
            emit("\tldr\tw0, [sp, #32]");         /* fd */
            emit("\tmov\tx1, #1");                /* SOL_SOCKET */
            emit("\tmov\tx2, #20");               /* SO_RCVTIMEO */
            emit("\tmov\tx3, sp");
            emit("\tmov\tx4, #16");
            emit("\tmov\tx8, #208");
            emit("\tsvc\t#0");
            emit("\tldr\tw0, [sp, #32]");
            emit("\tmov\tx1, #1");
            emit("\tmov\tx2, #21");               /* SO_SNDTIMEO */
            emit("\tmov\tx3, sp");
            emit("\tmov\tx4, #16");
            emit("\tmov\tx8, #208");
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #16");
            emit("\tadd\tsp, sp, #32");
            return 0;
        }
        if (e->ival == 37) {                /* localport(fd) -> port | -1 */
            gen_expr(m, e->a);
            push_val(0);
            emit("\tsub\tsp, sp, #32");
            emit("\tmov\tw9, #16");
            emit("\tstr\tw9, [sp]");
            emit("\tadd\tx1, sp, #16");
            emit("\tldr\tw0, [sp, #32]");
            emit("\tmov\tx2, sp");
            emit("\tmov\tx8, #204");          /* __NR_getsockname */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #0");
            emit("\tb.lt\t.LPORT%de", ln);
            emit("\tldrb\tw3, [sp, #18]");
            emit("\tldrb\tw4, [sp, #19]");
            emit("\tlsl\tw3, w3, #8");
            emit("\torr\tw0, w3, w4");
            emit("\tb\t.LPORT%dd", ln);
            emit(".LPORT%de:", ln);
            emit("\tmov\tw0, #-1");
            emit(".LPORT%dd:", ln);
            emit("\tadd\tsp, sp, #48");
            return 0;
        }
        if (e->ival == 34) {                /* setnonblock(fd) -> 0/-errno */
            gen_expr(m, e->a);
            push_val(0);
            emit("\tldr\tw0, [sp, #0]");
            emit("\tmov\tx1, #4");            /* F_SETFL */
            emit("\tmov\tx2, #2048");         /* O_NONBLOCK */
            emit("\tmov\tx8, #25");           /* __NR_fcntl */
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #16");
            return 0;
        }
        if (e->ival == 35) {                /* key() -> byte | -1 */
            emit("\tmov\tx0, #0");
            emit("\tmov\tx1, #4");
            emit("\tmov\tx2, #2048");
            emit("\tmov\tx8, #25");
            emit("\tsvc\t#0");
            emit("\tadrp\tx17, kop_iobuf");
            emit("\tadd\tx17, x17, :lo12:kop_iobuf");
            emit("\tmov\tx0, #0");
            emit("\tmov\tx1, x17");
            emit("\tmov\tx2, #1");
            emit("\tmov\tx8, #63");           /* __NR_read */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #1");
            emit("\tb.ne\t.LKEY%de", ln);
            emit("\tldrb\tw0, [x17]");
            emit("\tb\t.LKEY%dd", ln);
            emit(".LKEY%de:", ln);
            emit("\tmov\tw0, #-1");
            emit(".LKEY%dd:", ln);
            return 0;
        }
        if (e->ival == 36) {                /* ttyRaw(on) -> 0 */
            gen_expr(m, e->a);
            push_val(0);
            emit("\tldr\tw0, [sp, #0]");
            emit("\tcbz\tw0, .LTTY%ds", ln);
            emit("\tadrp\tx0, .Lstty_raw");
            emit("\tadd\tx0, x0, :lo12:.Lstty_raw");
            emit("\tb\t.LTTY%dc", ln);
            emit(".LTTY%ds:", ln);
            emit("\tadrp\tx0, .Lstty_sane");
            emit("\tadd\tx0, x0, :lo12:.Lstty_sane");
            emit(".LTTY%dc:", ln);
            emit("\tbl\tsystem");
            emit("\tmov\tw0, #0");
            emit("\tadd\tsp, sp, #16");
            return 0;
        }
        if (e->ival == 40 || e->ival == 41) { /* mkstr(int[])→String | strbytes(String)→int[] */
            /* v1.18: konversi representasi via helper runtime */
            gen_expr(m, e->a);
            push_val(1);
            emit("\tldr\tx0, [sp]");
            emit("\tbl\t%s", e->ival == 40 ? "kop_mkstr" : "kop_strbytes");
            emit("\tadd\tsp, sp, #16");
            return 0;
        }
        int n = 0;
        for (Node *aa = e->a; aa; aa = aa->next) {
            gen_expr(m, aa);
            push_val(0);                    /* arg selalu int */
            n++;
        }
        if (n <= 0) return 0;
        if (e->ival == 0) {                 /* socket(domain, type) */
            emit("\tldr\tw0, [sp, #%d]", (n - 1 - 0) * 16);
            emit("\tldr\tw1, [sp, #0]");
            emit("\tadd\tsp, sp, #%d", n * 16);
            emit("\tmov\tw2, #0");                     /* proto = 0 */
            emit("\tmov\tx8, #198");                   /* __NR_socket */
            emit("\tsvc\t#0");
        } else if (e->ival == 1) {          /* connect(fd, ipBE, port) */
            emit("\tstp\txzr, xzr, [sp, #-16]!");      /* sockaddr 16B nol */
            emit("\tldrb\tw4, [sp, #36]");             /* ip b0  (a) */
            emit("\tldr\tw6, [sp, #32]");              /* ipBE */
            emit("\tldr\tw7, [sp, #16]");              /* port */
            emit("\tlsr\tw4, w7, #8");
            emit("\tstrb\tw4, [sp, #2]");              /* port hi */
            emit("\tand\tw4, w7, #0xff");
            emit("\tstrb\tw4, [sp, #3]");              /* port lo */
            emit("\tlsr\tw4, w6, #24");
            emit("\tstrb\tw4, [sp, #4]");
            emit("\tlsr\tw4, w6, #16");
            emit("\tand\tw4, w4, #0xff");
            emit("\tstrb\tw4, [sp, #5]");
            emit("\tlsr\tw4, w6, #8");
            emit("\tand\tw4, w4, #0xff");
            emit("\tstrb\tw4, [sp, #6]");
            emit("\tand\tw4, w6, #0xff");
            emit("\tstrb\tw4, [sp, #7]");
            emit("\tmovz\tw4, #2");                    /* AF_INET */
            emit("\tstrb\tw4, [sp, #0]");
            emit("\tldr\tx0, [sp, #48]");              /* fd  (16+32) */
            emit("\tmov\tx1, sp");                     /* &sa */
            emit("\tmov\tw2, #16");
            emit("\tmov\tx8, #203");                   /* __NR_connect */
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #64");                /* sockaddr + 3 arg */
        } else if (e->ival == 2) {          /* rdb(fd) */
            emit("\tstp\txzr, xzr, [sp, #-16]!");      /* buffer 1B */
            emit("\tldr\tx0, [sp, #16]");              /* fd */
            emit("\tmov\tx1, sp");
            emit("\tmov\tw2, #1");
            emit("\tmov\tx8, #63");                    /* __NR_read */
            emit("\tsvc\t#0");
            emit("\tcmp\tx0, #1");
            emit("\tb.ne\t1f");
            emit("\tldrb\tw0, [sp]");                  /* byte → 0..255 */
            emit("\tb\t2f");
            emit("1:");
            emit("\tmov\tw0, #-1");                    /* EOF / error → -1 */
            emit("2:");
            emit("\tadd\tsp, sp, #32");
        } else if (e->ival == 3) {          /* wrb(fd, b) */
            /* stack: [16..31]=b (push terakhir), [32..47]=fd */
            emit("\tstp\txzr, xzr, [sp, #-16]!");
            emit("\tldr\tw2, [sp, #16]");              /* byte */
            emit("\tstrb\tw2, [sp]");
            emit("\tldr\tx0, [sp, #32]");              /* fd */
            emit("\tmov\tx1, sp");
            emit("\tmov\tw2, #1");
            emit("\tmov\tx8, #64");                    /* __NR_write */
            emit("\tsvc\t#0");
            emit("\tadd\tsp, sp, #32");
            emit("\tcmp\tx0, #1");
            emit("\tb.ne\t4f");
            emit("\tmov\tx0, xzr");
            emit("4:");
        } else if (e->ival == 4) {          /* close(fd) */
            emit("\tldr\tx0, [sp, #0]");
            emit("\tadd\tsp, sp, #%d", n * 16);
            emit("\tmov\tx8, #57");                    /* __NR_close */
            emit("\tsvc\t#0");
        }
        return 0;                           /* hasil int di w0/x0 */
    }
    case EX_ENVLOAD:
        emit("\tldr\tx0, [x29, #-8]");   /* slot 0 = param $env */
        if (is64ty((Ty)e->dty)) {
            emit("\tldr\tx0, [x0, #%ld]", 24 + 8 * (long)e->ival);
            return 1;
        }
        emit("\tldr\tw0, [x0, #%ld]", 24 + 8 * (long)e->ival);
        return 0;
    case EX_CAST: {
        Ty from = (Ty)e->a->dty, to = (Ty)e->dty;
        gen_expr(m, e->a);
        widen_ty(from);
        conv_ty(from, to);
        return is64ty(to);
    }
    case EX_TERN: {
        int lelse = nlab++, lend = nlab++;
        gen_expr(m, e->a);
        emit("\tcbz\tw0, .L%d", lelse);
        int w1 = gen_expr(m, e->b);
        emit("\tb\t.L%d", lend);
        emit(".L%d:", lelse);
        int w2 = gen_expr(m, e->c);
        emit(".L%d:", lend);
        return (w1 && w2) ? 1 : is64ty((Ty)e->dty);
    }
    case EX_BIN: {
        Ty bl = (Ty)e->a->dty, br = (Ty)e->b->dty, bres = (Ty)e->dty;
        if (!((bl == T_INT || bl == T_BOOL) && (br == T_INT || br == T_BOOL) &&
              (bres == T_INT || bres == T_BOOL)))
            return gen_bin_typed(m, e);
        char op = e->str[0];
        if ((op == '&' || op == '|') && e->str[1] != 0) {
            /* hasil 0/1 keduanya: and/or langsung karena operand boolean 0/1 */
            int lelse = nlab++, lend = nlab++;
            gen_expr(m, e->a);
            if (op == '&') {
                emit("\tcbz\tw0, .L%d", lelse);
                gen_expr(m, e->b);
                emit("\tcbz\tw0, .L%d", lelse);
                emit("\tmov\tw0, #1");
                emit("\tb\t.L%d", lend);
            } else {
                emit("\tcbnz\tw0, .L%d", lelse);
                gen_expr(m, e->b);
                emit("\tcbnz\tw0, .L%d", lelse);
                emit("\tmov\tw0, wzr");
                emit("\tb\t.L%d", lend);
            }
            emit(".L%d:", lelse);
            if (op == '&') emit("\tmov\tw0, wzr");
            else emit("\tmov\tw0, #1");
            emit(".L%d:", lend);
            return 0;
        }
        gen_expr(m, e->a);
        push_w0();
        gen_expr(m, e->b);
        pop_w("w9");
        /* operator 2-karakter dulu supaya tidak tabrak case '<' / '>' */
        if (!strcmp(e->str, "<=")) { emit("\tcmp\tw9, w0"); emit("\tcset\tw0, le"); break; }
        if (!strcmp(e->str, ">=")) { emit("\tcmp\tw9, w0"); emit("\tcset\tw0, ge"); break; }
        if (!strcmp(e->str, "<<")) { emit("\tlslv\tw0, w9, w0"); break; }   /* w9=left w0=right */
        if (!strcmp(e->str, ">>")) { emit("\tasrv\tw0, w9, w0"); break; }
        if (!strcmp(e->str, ">>>")) { emit("\tlsrv\tw0, w9, w0"); break; }
        switch (op) {
        case '+': emit("\tadd\tw0, w9, w0"); break;
        case '-': emit("\tsub\tw0, w9, w0"); break;
        case '*': emit("\tmul\tw0, w9, w0"); break;
        case '/':
            emit("\tsdiv\tw0, w9, w0");
            break;
        case '%':
            emit("\tsdiv\tw10, w9, w0");
            emit("\tmsub\tw0, w10, w0, w9");
            break;
        case '<': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, lt"); break;
        case '>': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, gt"); break;
        case '=': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, eq"); break;
        case '!': emit("\tcmp\tw9, w0"); emit("\tcset\tw0, ne"); break;
        case '&': emit("\tand\tw0, w9, w0"); break;     /* bitwise (len 1) */
        case '|': emit("\torr\tw0, w9, w0"); break;
        case '^': emit("\teor\tw0, w9, w0"); break;
        default:
            die("gen_a64: operator '%s'", e->str);
        }
        return 0;
    }
    default:
        die("gen_a64: ekspresi kind=%d", e->kind);
        return 0;
    }
    return 0;
}


