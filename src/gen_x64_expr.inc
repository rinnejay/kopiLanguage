/* gen_x64_expr.inc — ekspresi backend x64 (bagian dari gen_x64.c). */

static void gen_bool_chain(Method *m, Node *e) {
    /* && dan || dengan short-circuit */
    int ls = nlab++, lend = nlab++;
    gen_expr(m, e->a);
    emit("\ttestl %%eax, %%eax");
    if (e->str[0] == '&') {
        emit("\tje .L%d", ls);
        gen_expr(m, e->b);
        emit("\ttestl %%eax, %%eax");
        emit("\tje .L%d", ls);
        emit("\tmovl $1, %%eax");
    } else {
        emit("\tjne .L%d", ls);
        gen_expr(m, e->b);
        emit("\ttestl %%eax, %%eax");
        emit("\tjne .L%d", ls);
        emit("\tmovl $0, %%eax");
    }
    emit("\tjmp .L%d", lend);
    emit(".L%d:", ls);
    emit("\tmovl $%d, %%eax", e->str[0] == '&' ? 0 : 1);
    emit(".L%d:", lend);
}

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
        spush();
        gen_expr(m, e->b);
        spop("rcx");
        emit("\tcmpq %%rcx, %%rax");
        if (op == '=') emit("\tsete %%al");
        else emit("\tsetne %%al");
        emit("\tmovzbl %%al, %%eax");
        return 0;
    }

    gen_expr(m, e->a);
    widen_ty(lt);
    conv_ty(lt, P);
    spush();
    gen_expr(m, e->b);
    widen_ty(rt);
    if (!isShift) conv_ty(rt, P);
    spop("rcx");

    if (ty_is_float(P)) {
        if (P == T_FLOAT) {
            emit("\tmovd %%ecx, %%xmm0");
            emit("\tmovd %%eax, %%xmm1");
        } else {
            emit("\tmovq %%rcx, %%xmm0");
            emit("\tmovq %%rax, %%xmm1");
        }
        const char *pre = P == T_FLOAT ? "ss" : "sd";
        if (isCmp) {
            emit("\tucomi%s %%xmm1, %%xmm0", pre);
            if (op == '=') {
                emit("\tsete %%al");
                emit("\tsetnp %%dl");
                emit("\tandb %%dl, %%al");
            } else if (op == '!') {
                emit("\tsetne %%al");
                emit("\tsetp %%dl");
                emit("\torb %%dl, %%al");
            } else if (!strcmp(e->str, "<=")) {
                emit("\tsetbe %%al");
                emit("\tsetnp %%dl");
                emit("\tandb %%dl, %%al");
            } else if (!strcmp(e->str, ">=")) {
                emit("\tsetae %%al");
            } else if (op == '<') {
                emit("\tsetb %%al");
                emit("\tsetnp %%dl");
                emit("\tandb %%dl, %%al");
            } else {
                emit("\tseta %%al");
            }
            emit("\tmovzbl %%al, %%eax");
            return 0;
        }
        if (op == '+') emit("\tadd%s %%xmm1, %%xmm0", pre);
        else if (op == '-') emit("\tsub%s %%xmm1, %%xmm0", pre);
        else if (op == '*') emit("\tmul%s %%xmm1, %%xmm0", pre);
        else if (op == '/') emit("\tdiv%s %%xmm1, %%xmm0", pre);
        else if (op == '%') {
            emit("\tdiv%s %%xmm1, %%xmm0", pre);
            if (P == T_FLOAT) emit("\tcvttss2sil %%xmm0, %%eax");
            else emit("\tcvttsd2siq %%xmm0, %%rax");
            if (P == T_FLOAT) emit("\tcvtsi2ssl %%eax, %%xmm2");
            else emit("\tcvtsi2sdq %%rax, %%xmm2");
            emit("\tmul%s %%xmm1, %%xmm2", pre);
            emit("\tsub%s %%xmm2, %%xmm0", pre);
        } else die("gen_x64: operator float '%s'", e->str);
        if (P == T_FLOAT) emit("\tmovd %%xmm0, %%eax");
        else emit("\tmovq %%xmm0, %%rax");
        return is64ty(P);
    }

    if (isShift) {
        int mask = ty_width(P) - 1;
        if (mask <= 0) mask = 31;
        emit("\tmovq %%rax, %%rdx");    /* right */
        emit("\tmovq %%rcx, %%rax");    /* left */
        emit("\tmovl %%edx, %%ecx");    /* count */
        emit("\tandl $%d, %%ecx", mask);
        if (!strcmp(e->str, "<<")) emit("\tshlq %%cl, %%rax");
        else if (!strcmp(e->str, ">>")) emit("\tsarq %%cl, %%rax");
        else emit("\tshrq %%cl, %%rax");
        norm_ty(P);
        return is64ty(P);
    }
    if (isCmp) {
        int uns = ty_is_uint(P);
        emit("\tcmpq %%rax, %%rcx");
        if (op == '=') emit("\tsete %%al");
        else if (op == '!') emit("\tsetne %%al");
        else if (!strcmp(e->str, "<=")) emit(uns ? "\tsetbe %%al" : "\tsetle %%al");
        else if (!strcmp(e->str, ">=")) emit(uns ? "\tsetae %%al" : "\tsetge %%al");
        else if (op == '<') emit(uns ? "\tsetb %%al" : "\tsetl %%al");
        else emit(uns ? "\tseta %%al" : "\tsetg %%al");
        emit("\tmovzbl %%al, %%eax");
        return 0;
    }
    int uns = ty_is_uint(P);
    switch (op) {
    case '+': emit("\tleaq (%%rcx,%%rax), %%rax"); break;
    case '-': emit("\tmovq %%rcx, %%rdx"); emit("\tsubq %%rax, %%rdx"); emit("\tmovq %%rdx, %%rax"); break;
    case '*': emit("\timulq %%rcx, %%rax"); break;
    case '/':
        emit("\tmovq %%rax, %%r10");
        emit("\tmovq %%rcx, %%rax");
        if (uns) { emit("\txorl %%edx, %%edx"); emit("\tdivq %%r10"); }
        else { emit("\tcqto"); emit("\tidivq %%r10"); }
        break;
    case '%':
        emit("\tmovq %%rax, %%r10");
        emit("\tmovq %%rcx, %%rax");
        if (uns) { emit("\txorl %%edx, %%edx"); emit("\tdivq %%r10"); }
        else { emit("\tcqto"); emit("\tidivq %%r10"); }
        emit("\tmovq %%rdx, %%rax");
        break;
    case '&': emit("\tmovq %%rcx, %%rax"); emit("\tandq %%rcx, %%rax"); break;
    case '|': emit("\tmovq %%rcx, %%rax"); emit("\torq %%rcx, %%rax"); break;
    case '^': emit("\tmovq %%rcx, %%rax"); emit("\txorq %%rcx, %%rax"); break;
    default: die("gen_x64: operator '%s'", e->str);
    }
    norm_ty(P);
    return is64ty(P);
}

static int gen_expr(Method *m, Node *e) {
    switch (e->kind) {
    case EX_INT: {
        Ty t = (Ty)e->dty;
        if (t == T_INT) {
            emit("\tmovl $%ld, %%eax", e->ival);
        } else if (t == T_FLOAT) {
            union { float f; uint32_t u; } cv;
            cv.f = (float)e->fval;
            emit("\tmovl $%lu, %%eax", (unsigned long)cv.u);
        } else if (t == T_DOUBLE) {
            union { double d; uint64_t u; } cv;
            cv.d = e->fval;
            emit("\tmovabsq $%llu, %%rax", (unsigned long long)cv.u);
        } else if (t == T_UINT32 || t == T_UINT8 || t == T_UINT16 || t == T_BYTE || t == T_SHORT) {
            emit("\tmovl $%lu, %%eax", (unsigned long)e->ival);
        } else {
            emit("\tmovabsq $%ld, %%rax", e->ival);
        }
        break;
    }
    case EX_BOOL:
        emit("\tmovl $%d, %%eax", (int)e->ival);
        break;
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
            emit("\tmovq -%ld(%%rbp), %%rax", (e->ival + 1) * 8);
            return 1;
        }
        emit("\tmovl -%ld(%%rbp), %%eax", (e->ival + 1) * 8);
        break;
    case EX_CAST: {
        Ty from = (Ty)e->a->dty, to = (Ty)e->dty;
        gen_expr(m, e->a);
        widen_ty(from);
        conv_ty(from, to);
        return is64ty(to);
    }
    case EX_UN: {
        Ty t = (Ty)e->dty;
        gen_expr(m, e->a);
        if (e->str[0] == '!') { emit("\txorl $1, %%eax"); break; }
        if (ty_is_float(t)) {
            if (t == T_FLOAT) emit("\tbtcq $31, %%rax");
            else emit("\tbtcq $63, %%rax");
            return 1;
        }
        if (!is64ty(t)) {
            if (e->str[0] == '-') emit("\tnegl %%eax");
            else emit("\tnotl %%eax");
            break;
        }
        if (e->str[0] == '-') emit("\tnegq %%rax");
        else emit("\tnotq %%rax");
        norm_ty(t);
        return 1;
    }
    case EX_CALL:
        x64_call(m, e);
        return is64ty((Ty)e->dty);       /* int[] / fn → %rax; int → %eax */
    case EX_ARROW: {
        /* closure = blok 64-bit: [0]=kode [1]=ncapt [2]=nuser [3+j]=capture */
        Method *hm = (Method *)e->cp;
        emit("\tmovq $%ld, %%rdi", 8 * (3 + (long)hm->ncapt));
        emit("\tcall kopalloc");        /* arena bump: rdi = byte */
        emit("\tmovq %%rax, %%r10");
        emit("\tleaq %s__%s(%%rip), %%rax", hm->cls, hm->name);
        emit("\tmovq %%rax, (%%r10)");
        emit("\tmovl $%d, 8(%%r10)", hm->ncapt);
        emit("\tmovl $%d, 16(%%r10)", hm->nparams - 1);
        for (int j = 0; j < hm->ncapt; j++) {
            emit("\tmovl -%ld(%%rbp), %%eax", (hm->captslot[j] + 1) * 8);
            emit("\tmovl %%eax, %ld(%%r10)", 24 + 8 * j);
        }
        emit("\tmovq %%r10, %%rax");
        return 1;
    }
    case EX_FCALL: {
        int n = 0;
        for (Node *a = e->a; a; a = a->next) {
            if (n >= 5) die("baris %d: maksimal 5 argumen pemanggilan lambda", e->line);
            gen_expr(m, a);
            spush();
            n++;
        }
        gen_expr(m, e->b);                /* %rax = closure */
        emit("\tmovq %%rax, %%rbx");
        emit("\tmovl 16(%%rbx), %%eax");  /* nuser */
        emit("\tcmpl $%d, %%eax", n);
        emit("\tjne .Klam");
        emit("\tmovq (%%rbx), %%r11");    /* kode */
        emit("\tmovq %%rbx, %%rdi");      /* arg0 = env */
        static const char *reg32[5] = {"esi", "edx", "ecx", "r8d", "r9d"};
        for (int k = 0; k < n; k++)
            emit("\tmovl %d(%%rsp), %%%s", (n - 1 - k) * 8, reg32[k]);
        if (n > 0) { emit("\taddq $%d, %%rsp", n * 8); par ^= (n & 1); }
        int need = par;
        if (need) emit("\tsubq $8, %%rsp");
        emit("\tcall *%%r11");
        if (need) emit("\taddq $8, %%rsp");
        break;
    }
    case EX_NEWARR: {
        gen_expr(m, e->a);               /* eax = n */
        emit("\ttestl %%eax, %%eax");
        emit("\tjl .Karr");             /* ukuran salah */
        emit("\tmovl %%eax, %%r10d");   /* simpan n */
        emit("\tshlq $3, %%r10");       /* 8*n */
        emit("\taddq $8, %%r10");       /* + header len */
        emit("\tmovq %%r10, %%rdi");
        emit("\tcall kopalloc");
        emit("\tmovq %%r10, (%%rax)");  /* len */
        {
            int L = arrlbl++;
            emit("\tmovq %%r10, %%rcx");
            emit("\tshrq $3, %%rcx");      /* n */
            emit("\txorl %%edx, %%edx");
            emit(".LNZX%d:", L);
            emit("\tcmpq %%rcx, %%rdx");
            emit("\tjge .LNZX%de", L);
            emit("\tmovq $0, 8(%%rax,%%rdx,8)");
            emit("\tincq %%rdx");
            emit("\tjmp .LNZX%d", L);
            emit(".LNZX%de:", L);
        }
        return 1;
    }
    case EX_IDX: {
        gen_expr(m, e->a);               /* rax = array/String */
        spush();
        gen_expr(m, e->b);               /* eax = idx */
        emit("\tpopq %%r10");           /* array */
        emit("\tcmpl (%r10), %%eax");   /* len */
        emit("\tjae .Karr");            /* unsigned: negatif tertangkap */
        emit("\tmovl %%eax, %%eax");
        emit("\tcltq");
        if ((Ty)e->a->dty == T_STR) {   /* v1.18: byte ke-i String packed */
            emit("\tmovzbl 8(%%r10,%%rax), %%eax");
            return 0;
        }
        emit("\tmovq (%%r10,%%rax,8), %%rax");   /* elemen 8-byte */
        return 0;
    }
    case EX_LEN:
        gen_expr(m, e->a);
        emit("\tmovl (%rax), %%eax");
        return 0;
    case EX_NET: {                       /* syscall Linux langsung (tanpa libc) */
        int ln = netlbl++;                /* label unik per kemunculan */
        /* 0=socket 1=connect 2=rdb 3=wrb 4=close 5=bytes */
        if (e->ival == 5) {              /* Net.bytes("literal") → String packed (v1.18) */
            int c = (int)e->cp;
            emit("\tleaq cpool%d(%%rip), %%rdi", c);
            emit("\tcall kop_str_from_cstr");
            return 1;
        }
        if (e->ival == 6) {              /* Net.openr("literal") → fd          */
            int c = (int)e->cp;
            emit("\tleaq cpool%d(%%rip), %%rsi", c);
            emit("\tmovq %%rsi, %%rdx");                 /* strlen */
            emit("\txorl %%eax, %%eax");
            emit(".LNO%da:", ln);
            emit("\tcmpb $0, (%%rdx)");
            emit("\tje .LNO%db", ln);
            emit("\taddq $1, %%rdx");
            emit("\tjmp .LNO%da", ln);
            emit(".LNO%db:", ln);
            /* openat(AT_FDCWD=-100, path, O_RDONLY) */
            emit("\tmovl $-100, %%edi");
            emit("\txorl %%edx, %%edx");                 /* flags = O_RDONLY */
            emit("\tmovl $257, %%eax");                  /* __NR_openat */
            emit("\tsyscall");
            return 0;                                    /* fd | -errno */
        }
        if (e->ival == 7 || e->ival == 8) {
            for (Node *aa = e->a; aa; aa = aa->next) {
                gen_expr(m, aa);
                spush();
            }
        }
        if (e->ival == 7) {              /* recvb(fd, buf) → n | -1            */
            /* stack: [0]=buf (64-bit), [8]=fd; tmp via .comm kop_iobuf */
            emit("\tmovq (%%rsp), %%r10");               /* buf */
            emit("\tmovl (%%r10), %%r8d");               /* len */
            emit("\ttestl %%r8d, %%r8d");
            emit("\tjle .LRX%dz", ln);
            emit("\txorl %%r9d, %%r9d");                 /* got */
            emit("\tleaq kop_iobuf(%%rip), %%r11");      /* tmp base */
            emit(".LRX%dl:", ln);
            emit("\tcmpl %%r8d, %%r9d");
            emit("\tjge .LRX%df", ln);
            emit("\tmovl %%r8d, %%eax");
            emit("\tsubl %%r9d, %%eax");                 /* sisa */
            emit("\tcmpl $240, %%eax");
            emit("\tjle .LRX%dc", ln);
            emit("\tmovl $240, %%eax");
            emit(".LRX%dc:", ln);                        /* want = eax */
            emit("\tmovl %%eax, %%edx");                 /* want untuk syscall */
            /* salin ke tmp: tmp[k] = buf[1 + got + k] */
            emit("\txorl %%ecx, %%ecx");
            emit(".LRX%dcs:", ln);
            emit("\tcmpl %%eax, %%ecx");
            emit("\tjge .LRX%dcf", ln);
            emit("\tmovl %%r9d, %%eax");
            emit("\taddl %%ecx, %%eax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq 8(%%r10,%%rax), %%rax");
            emit("\tmovb %%al, (%%r11,%%rcx)");
            emit("\tincl %%ecx");
            emit("\tjmp .LRX%dcs", ln);
            emit(".LRX%dcf:", ln);
            emit("\tmovl 8(%%rsp), %%edi");              /* fd */
            emit("\tmovq %%r11, %%rsi");                 /* tmp */
            emit("\txorl %%eax, %%eax");                 /* __NR_read = 0 */
            emit("\tsyscall");
            emit("\ttestq %%rax, %%rax");
            emit("\tjs .LRX%dry", ln);                   /* galat keras */
            emit("\tje .LRX%de", ln);                    /* EOF */
            /* salin kembali: buf[1 + got + k] = tmp[k], k < rax */
            emit("\txorl %%ecx, %%ecx");
            emit(".LRX%drs:", ln);
            emit("\tcmpq %%rax, %%rcx");
            emit("\tjge .LRX%drf", ln);
            emit("\tmovzbq (%%r11,%%rcx), %%rdx");
            emit("\tmovl %%r9d, %%eax");
            emit("\taddl %%ecx, %%eax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq %%rdx, 8(%%r10,%%rax)");
            emit("\tincl %%ecx");
            emit("\tjmp .LRX%drs", ln);
            emit(".LRX%drf:", ln);
            emit("\taddl %%eax, %%r9d");                 /* got += nread */
            emit("\tjmp .LRX%dl", ln);
            emit(".LRX%dry:", ln);                       /* galat → -1 */
            emit("\taddq $16, %%rsp");
            emit("\tmovl $-1, %%eax");
            emit("\tjmp .LRX%dh", ln);
            emit(".LRX%de:", ln);                        /* EOF → got */
            emit("\tjmp .LRX%df", ln);
            emit(".LRX%df:", ln);                        /* penuh → got */
            emit("\tmovl %%r9d, %%eax");
            emit("\taddq $16, %%rsp");
            emit("\tjmp .LRX%dh", ln);
            emit(".LRX%dz:", ln);                        /* len<=0 → 0 */
            emit("\txorl %%eax, %%eax");
            emit("\taddq $16, %%rsp");
            emit(".LRX%dh:", ln);
            return 0;
        }
        if (e->ival == 8) {              /* sendb(fd, buf) → 0 | -1            */
            emit("\tmovq (%%rsp), %%r10");               /* buf */
            emit("\tmovl (%%r10), %%r8d");               /* len */
            emit("\ttestl %%r8d, %%r8d");
            emit("\tjle .LSX%df", ln);
            emit("\txorl %%r9d, %%r9d");                 /* terkirim */
            emit("\tleaq kop_iobuf(%%rip), %%r11");      /* tmp base */
            emit(".LSX%dl:", ln);
            emit("\tcmpl %%r8d, %%r9d");
            emit("\tjge .LSX%df", ln);
            emit("\tmovl %%r8d, %%eax");
            emit("\tsubl %%r9d, %%eax");
            emit("\tcmpl $240, %%eax");
            emit("\tjle .LSX%dc", ln);
            emit("\tmovl $240, %%eax");
            emit(".LSX%dc:", ln);                        /* want = eax */
            /* salin ke tmp: tmp[k] = buf[1 + terkirim + k] */
            emit("\tmovl %%eax, %%edx");                 /* want untuk syscall */
            emit("\txorl %%ecx, %%ecx");
            emit(".LSX%dcs:", ln);
            emit("\tcmpl %%edx, %%ecx");
            emit("\tjge .LSX%dcf", ln);
            emit("\tmovl %%r9d, %%eax");
            emit("\taddl %%ecx, %%eax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq 8(%%r10,%%rax), %%rax");
            emit("\tmovb %%al, (%%r11,%%rcx)");
            emit("\tincl %%ecx");
            emit("\tjmp .LSX%dcs", ln);
            emit(".LSX%dcf:", ln);
            emit("\tmovl 8(%%rsp), %%edi");              /* fd */
            emit("\tmovq %%r11, %%rsi");
            emit("\tmovl %%edx, %%edx");                 /* want */
            /* push r8/r9: sendto memakai r8,r9 utk addr/addrlen */
            emit("\tpushq %%r8");
            emit("\tpushq %%r9");
            emit("\tmovl 24(%%rsp), %%edi");             /* fd (setelah 2 push) */
            emit("\tmovq %%r11, %%rsi");
            emit("\tmovl %%edx, %%edx");
            emit("\tmovl $16384, %%r10d");               /* MSG_NOSIGNAL */
            emit("\txorl %%r8d, %%r8d");                 /* addr = NULL */
            emit("\txorl %%r9d, %%r9d");                 /* addrlen = 0 */
            emit("\tmovl $44, %%eax");                   /* __NR_sendto */
            emit("\tsyscall");
            emit("\tcmpl $-88, %%eax");                  /* ENOTSOCK → write */
            emit("\tjne .LSX%dso", ln);
            emit("\tmovl 24(%%rsp), %%edi");
            emit("\tmovq %%r11, %%rsi");
            emit("\tmovl %%edx, %%edx");
            emit("\tmovl $1, %%eax");                    /* __NR_write */
            emit("\tsyscall");
            emit(".LSX%dso:", ln);
            emit("\tleaq kop_iobuf(%%rip), %%r11");      /* syscall merusak r11 */
            emit("\tpopq %%r9");
            emit("\tpopq %%r8");
            emit("\ttestq %%rax, %%rax");
            emit("\tjs .LSX%de", ln);
            emit("\taddl %%eax, %%r9d");                 /* penuh/parsial */
            emit("\tjmp .LSX%dl", ln);
            emit(".LSX%de:", ln);                        /* galat → -1 */
            emit("\taddq $16, %%rsp");
            emit("\tmovl $-1, %%eax");
            emit("\tjmp .LSX%dh", ln);
            emit(".LSX%df:", ln);                        /* selesai → 0 */
            emit("\txorl %%eax, %%eax");
            emit("\taddq $16, %%rsp");
            emit(".LSX%dh:", ln);
            return 0;
        }
        if (e->ival >= 9 && e->ival <= 12) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                spush();
            }
            emit("\tsubq $32, %%rsp");                  /* sa + addrlen */
            emit("\tmovq $0, (%%rsp)");
            emit("\tmovq $0, 8(%%rsp)");
            emit("\tmovq $0, 16(%%rsp)");
            emit("\tmovq $0, 24(%%rsp)");
            if (e->ival == 9) {                        /* bind(fd, ip, port) */
                emit("\tmovl 40(%%rsp), %%r8d");       /* ip */
                emit("\tmovl 32(%%rsp), %%r9d");       /* port */
                emit("\tmovl %%r9d, %%eax");
                emit("\tshrl $8, %%eax");
                emit("\tmovb %%al, 2(%%rsp)");
                emit("\tmovl %%r9d, %%eax");
                emit("\tmovb %%al, 3(%%rsp)");
                emit("\tmovl $4, %%ecx");
                emit(".LSB%da:", ln);
                emit("\tmovl %%r8d, %%eax");
                emit("\tshrl $24, %%eax");
                emit("\tmovb %%al, (%%rsp,%%rcx)");
                emit("\tshll $8, %%r8d");
                emit("\tincl %%ecx");
                emit("\tcmpl $8, %%ecx");
                emit("\tjne .LSB%da", ln);
                emit("\tmovw $2, (%%rsp)");
                emit("\tmovl 48(%%rsp), %%edi");      /* fd */
                emit("\tmovq %%rsp, %%rsi");
                emit("\tmovl $16, %%edx");
                emit("\tmovl $49, %%eax");            /* __NR_bind */
                emit("\tsyscall");
                emit("\taddq $32, %%rsp");
                emit("\taddq $24, %%rsp");
                return 0;
            }
            if (e->ival == 12) {                       /* shutdown(fd, how) */
                emit("\tmovl 40(%%rsp), %%edi");
                emit("\tmovl 32(%%rsp), %%esi");
                emit("\tmovl $48, %%eax");            /* __NR_shutdown */
                emit("\tsyscall");
                emit("\taddq $32, %%rsp");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 10) {                       /* sendto(fd,ip,port,buf) */
                emit("\tmovq 32(%%rsp), %%r10");      /* buf */
                emit("\tmovl 48(%%rsp), %%r8d");      /* ip */
                emit("\tmovl 40(%%rsp), %%r9d");      /* port */
                emit("\tmovl %%r9d, %%eax");
                emit("\tshrl $8, %%eax");
                emit("\tmovb %%al, 2(%%rsp)");
                emit("\tmovl %%r9d, %%eax");
                emit("\tmovb %%al, 3(%%rsp)");
                emit("\tmovl $4, %%ecx");
                emit(".LSD%da:", ln);
                emit("\tmovl %%r8d, %%eax");
                emit("\tshrl $24, %%eax");
                emit("\tmovb %%al, (%%rsp,%%rcx)");
                emit("\tshll $8, %%r8d");
                emit("\tincl %%ecx");
                emit("\tcmpl $8, %%ecx");
                emit("\tjne .LSD%da", ln);
                emit("\tmovw $2, (%%rsp)");
                /* pak ke kop_iobuf, cap 240 */
                emit("\tmovl (%%r10), %%r11d");      /* len */
                emit("\tcmpl $1200, %%r11d");
                emit("\tjle .LSD%dc", ln);
                emit("\tmovl $1200, %%r11d");
                emit(".LSD%dc:", ln);
                emit("\tleaq kop_iobuf(%%rip), %%rbx");
                emit("\txorl %%ecx, %%ecx");
                emit(".LSD%dk:", ln);
                emit("\tcmpl %%r11d, %%ecx");
                emit("\tjge .LSD%df", ln);
                emit("\tmovl %%ecx, %%eax");
                emit("\tshlq $3, %%rax");
                emit("\tmovq 8(%%r10,%%rax), %%rax");
                emit("\tmovb %%al, (%%rbx,%%rcx)");
                emit("\tincl %%ecx");
                emit("\tjmp .LSD%dk", ln);
                emit(".LSD%df:", ln);
                emit("\tmovl 56(%%rsp), %%edi");      /* fd */
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl %%r11d, %%edx");
                emit("\txorl %%r10d, %%r10d");
                emit("\tmovq %%rsp, %%r8");
                emit("\tmovl $16, %%r9d");
                emit("\tmovl $44, %%eax");            /* __NR_sendto */
                emit("\tsyscall");
                emit("\taddq $32, %%rsp");
                emit("\taddq $32, %%rsp");
                return 0;
            }
            if (e->ival == 11) {                       /* recvfrom(fd,buf,from) */
                emit("\tmovl $16, 16(%%rsp)");        /* addrlen */
                emit("\tleaq kop_iobuf(%%rip), %%rbx");
                emit("\tmovl 48(%%rsp), %%edi");      /* fd */
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl $1200, %%edx");
                emit("\txorl %%r10d, %%r10d");
                emit("\tmovq %%rsp, %%r8");
                emit("\tleaq 16(%%rsp), %%r9");
                emit("\tmovl $45, %%eax");            /* __NR_recvfrom */
                emit("\tsyscall");
                emit("\ttestq %%rax, %%rax");
                emit("\tjs .LSF%de", ln);
                emit("\tmovq 40(%%rsp), %%r10");      /* buf */
                emit("\txorl %%ecx, %%ecx");
                emit(".LSF%dk:", ln);
                emit("\tcmpq %%rax, %%rcx");
                emit("\tjge .LSF%df", ln);
                emit("\tmovzbl (%%rbx,%%rcx), %%edx");
                emit("\tmovl %%ecx, %%r11d");
                emit("\tshlq $3, %%r11");
                emit("\tmovq %%rdx, 8(%%r10,%%r11)");
                emit("\tincl %%ecx");
                emit("\tjmp .LSF%dk", ln);
                emit(".LSF%df:", ln);
                emit("\tmovq 32(%%rsp), %%r11");      /* from[] */
                emit("\tmovzbl 4(%%rsp), %%edx");
                emit("\tshll $24, %%edx");
                emit("\tmovzbl 5(%%rsp), %%esi");
                emit("\tshll $16, %%esi");
                emit("\torl %%esi, %%edx");
                emit("\tmovzbl 6(%%rsp), %%esi");
                emit("\tshll $8, %%esi");
                emit("\torl %%esi, %%edx");
                emit("\tmovzbl 7(%%rsp), %%esi");
                emit("\torl %%esi, %%edx");
                emit("\tmovzbl 2(%%rsp), %%esi");
                emit("\tshll $8, %%esi");
                emit("\tmovzbl 3(%%rsp), %%edi");
                emit("\torl %%edi, %%esi");
                emit("\tmovq %%rdx, 8(%%r11)");
                emit("\tmovq %%rsi, 16(%%r11)");
                emit(".LSF%de:", ln);
                emit("\taddq $32, %%rsp");
                emit("\taddq $24, %%rsp");
                return 0;
            }
        }
        if (e->ival >= 13 && e->ival <= 21) {
            int na = 0;
            for (Node *aa = e->a; aa; aa = aa->next) { gen_expr(m, aa); spush(); na++; }
            if (e->ival == 13) {                       /* openb(path,flags,mode) */
                emit("\tmovq 16(%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovl $-100, %%edi");
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl 8(%%rsp), %%edx");
                emit("\tmovl (%%rsp), %%r10d");
                emit("\tmovl $257, %%eax");
                emit("\tsyscall");
                emit("\taddq $24, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 14) {                       /* mkdirb(path) */
                emit("\tmovq (%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovl $-100, %%edi");
                emit("\tmovq %%rbx, %%rsi");
                emit("\tmovl $493, %%edx");
                emit("\tmovl $258, %%eax");
                emit("\tsyscall");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 15) {                       /* unlinkb(path) */
                emit("\tmovq (%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovl $-100, %%edi");
                emit("\tmovq %%rbx, %%rsi");
                emit("\txorl %%edx, %%edx");
                emit("\tmovl $263, %%eax");
                emit("\tsyscall");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 16) {                       /* renameb(old,new) */
                emit("\tmovq 8(%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'a', 0);
                emit("\tmovq (%%rsp), %%r10");
                pack_path_bytes_x64(ln, 'b', 2048);
                emit("\tmovl $-100, %%edi");
                emit("\tleaq -2048(%%rbx), %%rsi");
                emit("\tmovl $-100, %%edx");
                emit("\tmovq %%rbx, %%r10");
                emit("\tmovl $264, %%eax");
                emit("\tsyscall");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 17) {                       /* seek(fd,off,whence) */
                emit("\tmovl 16(%%rsp), %%edi");
                emit("\tmovslq 8(%%rsp), %%rsi");
                emit("\tmovl (%%rsp), %%edx");
                emit("\tmovl $8, %%eax");
                emit("\tsyscall");
                emit("\taddq $24, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 18) {                       /* time() */
                emit("\tsubq $16, %%rsp");
                emit("\txorl %%edi, %%edi");
                emit("\tmovq %%rsp, %%rsi");
                emit("\tmovl $228, %%eax");
                emit("\tsyscall");
                emit("\tmovq (%%rsp), %%rax");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 19) {                       /* mono() ms */
                emit("\tsubq $16, %%rsp");
                emit("\tmovl $1, %%edi");
                emit("\tmovq %%rsp, %%rsi");
                emit("\tmovl $228, %%eax");
                emit("\tsyscall");
                emit("\tmovq (%%rsp), %%r8");
                emit("\timulq $1000, %%r8, %%r8");
                emit("\tmovq 8(%%rsp), %%rax");
                emit("\txorl %%edx, %%edx");
                emit("\tmovq $1000000, %%rcx");
                emit("\tdivq %%rcx");
                emit("\taddq %%r8, %%rax");
                emit("\taddq $16, %%rsp");
                return 1;
            }
            if (e->ival == 20) {                       /* sleep(ms) */
                emit("\tmovq (%%rsp), %%rax");
                emit("\tmovq $1000, %%rcx");
                emit("\txorl %%edx, %%edx");
                emit("\tdivq %%rcx");
                emit("\tsubq $16, %%rsp");
                emit("\tmovq %%rax, (%%rsp)");
                emit("\timulq $1000000, %%rdx, %%rdx");
                emit("\tmovq %%rdx, 8(%%rsp)");
                emit("\tmovq %%rsp, %%rdi");
                emit("\txorl %%esi, %%esi");
                emit("\tmovl $35, %%eax");
                emit("\tsyscall");
                emit("\taddq $16, %%rsp");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                emit("\txorl %%eax, %%eax");
                return 0;
            }
            if (e->ival == 21) {                       /* exit(code) */
                emit("\tmovl (%%rsp), %%edi");
                emit("\tmovl $60, %%eax");
                emit("\tsyscall");
                return 0;
            }
        }
        if (e->ival >= 22 && e->ival <= 24) {
            int idx = 0;
            for (Node *aa = e->a; aa; aa = aa->next, idx++) {
                gen_expr(m, aa);
                spush();
            }
            if (e->ival == 22) {                     /* cat(a,b) → kop_cat_packed (v1.18) */
                emit("\tmovq 16(%%rsp), %%rdi");     /* a */
                emit("\tmovq (%%rsp), %%rsi");       /* b */
                emit("\tcall kop_cat_packed");
                emit("\taddq $16, %%rsp");
                return 1;
            }
            /* sequal / sne → kop_sequal_packed (v1.18) */
            emit("\tmovq 16(%%rsp), %%rdi");
            emit("\tmovq (%%rsp), %%rsi");
            emit("\tcall kop_sequal_packed");
            emit("\taddq $16, %%rsp");
            if (e->ival == 24) emit("\txorl $1, %%eax");
            return 0;
        }
        if (e->ival >= 25 && e->ival <= 32) {
            int na = 0;
            for (Node *aa = e->a; aa; aa = aa->next) { gen_expr(m, aa); spush(); na++; }
            if (e->ival == 25) {                       /* listen(fd, backlog) */
                emit("\tmovl 8(%%rsp), %%edi");
                emit("\tmovl (%%rsp), %%esi");
                emit("\tmovl $50, %%eax");
                emit("\tsyscall");
                emit("\taddq $16, %%rsp");
                return 0;
            }
            if (e->ival == 26) {                       /* accept(fd) */
                emit("\tmovl (%%rsp), %%edi");
                emit("\txorl %%esi, %%esi");
                emit("\txorl %%edx, %%edx");
                emit("\txorl %%r10d, %%r10d");
                emit("\tmovl $288, %%eax");          /* accept4 */
                emit("\tsyscall");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 27) {                       /* setsockopt */
                emit("\tmovl 24(%%rsp), %%edi");
                emit("\tmovl 16(%%rsp), %%esi");
                emit("\tmovl 8(%%rsp), %%edx");
                emit("\tmovl (%%rsp), %%r10d");
                emit("\tsubq $16, %%rsp");
                emit("\tmovl %%r10d, (%%rsp)");
                emit("\tmovq %%rsp, %%r10");
                emit("\tmovl $54, %%eax");
                emit("\tsyscall");
                emit("\taddq $16, %%rsp");
                emit("\taddq $32, %%rsp");
                return 0;
            }
            if (e->ival == 28) {                       /* memmark */
                emit("\tmovl kop_arena_used(%%rip), %%eax");
                return 0;
            }
            if (e->ival == 29) {                       /* memreset */
                emit("\tmovl (%%rsp), %%eax");
                emit("\tleaq kop_arena_used(%%rip), %%rcx");
                emit("\tcmpl $0, %%eax");
                emit("\tjge .LMRX%dP", ln);
                emit("\txorl %%eax, %%eax");
                emit(".LMRX%dP:", ln);
                emit("\tmovq (%%rcx), %%rdx");
                emit("\tcmpq %%rdx, %%rax");
                emit("\tjle .LMRX%dS", ln);
                emit("\tmovq %%rdx, %%rax");
                emit(".LMRX%dS:", ln);
                emit("\tmovq %%rax, (%%rcx)");
                emit("\taddq $8, %%rsp");
                par ^= 1;
                return 0;
            }
            if (e->ival == 30) {                       /* argc */
                emit("\tmovl kop_argc(%%rip), %%eax");
                return 0;
            }
            int need = par;
            if (need) { emit("\tsubq $8, %%rsp"); par ^= 1; }
            int arg_off = need ? 8 : 0;
            if (e->ival == 31) {                       /* argv(i) */
                emit("\tmovl %d(%%rsp), %%eax", arg_off);
                emit("\tleaq kop_argc(%%rip), %%rcx");
                emit("\tcmpl (%%rcx), %%eax");
                emit("\tjae .LAVX%dE", ln);
                emit("\tleaq kop_argv(%%rip), %%rcx");
                emit("\tmovq (%%rcx), %%rcx");
                emit("\tmovslq %%eax, %%rax");
                emit("\tmovq (%%rcx,%%rax,8), %%rdi");
                emit("\tcall kop_str_from_cstr");
                emit("\tjmp .LAVX%dD", ln);
                emit(".LAVX%dE:", ln);
                emit("\tleaq .Lempty_str(%%rip), %%rdi");
                emit("\tcall kop_str_from_cstr");
                emit(".LAVX%dD:", ln);
                if (need) emit("\taddq $8, %%rsp");
                emit("\taddq $8, %%rsp");
                par ^= need ? 0 : 1;
                return 1;
            }
            if (e->ival == 32) {                       /* getenv */
                emit("\tmovq %d(%%rsp), %%r10", arg_off);
                pack_path_bytes_x64(ln, 'g', 0);
                emit("\tleaq kop_iobuf(%%rip), %%rdi");
                emit("\tcall getenv@PLT");
                emit("\ttestq %%rax, %%rax");
                emit("\tje .LGEX%dE", ln);
                emit("\tmovq %%rax, %%rdi");
                emit("\tcall kop_str_from_cstr");
                emit("\tjmp .LGEX%dD", ln);
                emit(".LGEX%dE:", ln);
                emit("\tleaq .Lempty_str(%%rip), %%rdi");
                emit("\tcall kop_str_from_cstr");
                emit(".LGEX%dD:", ln);
                if (need) emit("\taddq $8, %%rsp");
                emit("\taddq $8, %%rsp");
                par ^= need ? 0 : 1;
                return 1;
            }
        }
        if (e->ival == 33) {                       /* recv1(fd, buf) */
            gen_expr(m, e->a); spush();
            gen_expr(m, e->a->next); spush();
            emit("\tmovq (%%rsp), %%r10");          /* buf */
            emit("\tmovl 8(%%rsp), %%edi");         /* fd */
            emit("\tmovl (%%r10), %%r11d");         /* len */
            emit("\tcmpl $4000, %%r11d");
            emit("\tjle .LR1X%dc", ln);
            emit("\tmovl $4000, %%r11d");
            emit(".LR1X%dc:", ln);
            emit("\tleaq kop_iobuf(%%rip), %%rsi");
            emit("\tmovl %%r11d, %%edx");
            emit("\txorl %%eax, %%eax");
            emit("\tsyscall");
            emit("\ttestq %%rax, %%rax");
            emit("\tjle .LR1X%dd", ln);
            emit("\tmovl %%eax, %%r12d");
            emit("\txorl %%ecx, %%ecx");
            emit(".LR1X%dk:", ln);
            emit("\tcmpl %%r12d, %%ecx");
            emit("\tjge .LR1X%df", ln);
            emit("\tmovzbl (%%rsi,%%rcx), %%edx");
            emit("\tmovslq %%ecx, %%rax");
            emit("\tshlq $3, %%rax");
            emit("\tmovq %%rdx, 8(%%r10,%%rax)");
            emit("\tincl %%ecx");
            emit("\tjmp .LR1X%dk", ln);
            emit(".LR1X%df:", ln);
            emit("\tmovl %%r12d, %%eax");
            emit(".LR1X%dd:", ln);
            emit("\taddq $16, %%rsp");
            return 0;
        }
        if (e->ival == 39) {                       /* pollfd(fds, revents, ms) */
            gen_expr(m, e->a); spush();
            gen_expr(m, e->a->next); spush();
            gen_expr(m, e->a->next->next); spush();
            int pad = par;
            if (pad) emit("\tsubq $8, %%rsp");
            emit("\tsubq $512, %%rsp");
            int A = 512 + pad;
            emit("\tmovq %d(%%rsp), %%r10", A + 16);   /* fds */
            emit("\tmovl (%%r10), %%r9d");
            emit("\tcmpl $64, %%r9d");
            emit("\tjle .LPFX%dc", ln);
            emit("\tmovl $64, %%r9d");
            emit(".LPFX%dc:", ln);
            emit("\tmovl %%r9d, %d(%%rsp)", A - 8);    /* simpan n */
            emit("\txorl %%ecx, %%ecx");
            emit(".LPFX%dl:", ln);
            emit("\tcmpl %%r9d, %%ecx");
            emit("\tjge .LPFX%dd", ln);
            emit("\tmovslq %%ecx, %%rax");
            emit("\tmovl 8(%%r10,%%rax,8), %%edx");
            emit("\tmovq %%rax, %%r11");
            emit("\tshlq $3, %%r11");
            emit("\tmovl %%edx, (%%rsp,%%r11)");
            emit("\tmovw $1, 4(%%rsp,%%r11)");
            emit("\tmovw $0, 6(%%rsp,%%r11)");
            emit("\tincl %%ecx");
            emit("\tjmp .LPFX%dl", ln);
            emit(".LPFX%dd:", ln);
            emit("\tmovq %%rsp, %%rdi");
            emit("\tmovl %d(%%rsp), %%esi", A - 8);
            emit("\tmovl %d(%%rsp), %%edx", A);
            emit("\tcall poll@PLT");
            emit("\tmovl %d(%%rsp), %%r9d", A - 8);
            emit("\tmovq %d(%%rsp), %%r10", A + 8);    /* revents */
            emit("\txorl %%ecx, %%ecx");
            emit("\txorl %%eax, %%eax");
            emit(".LPFX%dk:", ln);
            emit("\tcmpl %%r9d, %%ecx");
            emit("\tjge .LPFX%df", ln);
            emit("\tmovslq %%ecx, %%r11");
            emit("\tshlq $3, %%r11");
            emit("\tmovzwl 6(%%rsp,%%r11), %%esi");
            emit("\txorl %%edi, %%edi");
            emit("\ttestw %%si, %%si");
            emit("\tje .LPFX%dn2", ln);
            emit("\ttestw $1, %%si");
            emit("\tje .LPFX%dn1", ln);
            emit("\tmovl $1, %%edi");
            emit(".LPFX%dn1:", ln);
            emit("\ttestw $56, %%si");
            emit("\tje .LPFX%dn2", ln);
            emit("\torl $2, %%edi");
            emit(".LPFX%dn2:", ln);
            emit("\tmovq %%rdi, 8(%%r10,%%r11)");
            emit("\ttestl %%edi, %%edi");
            emit("\tje .LPFX%dn", ln);
            emit("\tincl %%eax");
            emit(".LPFX%dn:", ln);
            emit("\tincl %%ecx");
            emit("\tjmp .LPFX%dk", ln);
            emit(".LPFX%df:", ln);
            emit("\taddq $512, %%rsp");
            if (pad) emit("\taddq $8, %%rsp");
            emit("\taddq $24, %%rsp");
            par ^= 1;
            return 0;
        }
        if (e->ival == 38) {                       /* settimeout(fd, ms) */
            gen_expr(m, e->a); spush();
            gen_expr(m, e->a->next); spush();
            emit("\tmovq (%%rsp), %%rax");        /* ms */
            emit("\txorl %%edx, %%edx");
            emit("\tmovq $1000, %%rcx");
            emit("\tdivq %%rcx");                 /* rax=detik, rdx=sisa ms */
            emit("\timulq $1000, %%rdx, %%rdx");
            emit("\tsubq $16, %%rsp");
            emit("\tmovq %%rax, (%%rsp)");
            emit("\tmovq %%rdx, 8(%%rsp)");
            emit("\tmovl 32(%%rsp), %%edi");      /* fd */
            emit("\tmovl $1, %%esi");
            emit("\tmovl $20, %%edx");            /* SO_RCVTIMEO */
            emit("\tmovq %%rsp, %%r10");
            emit("\tmovl $16, %%r8d");
            emit("\tmovl $54, %%eax");
            emit("\tsyscall");
            emit("\tmovl 32(%%rsp), %%edi");
            emit("\tmovl $1, %%esi");
            emit("\tmovl $21, %%edx");            /* SO_SNDTIMEO */
            emit("\tmovq %%rsp, %%r10");
            emit("\tmovl $16, %%r8d");
            emit("\tmovl $54, %%eax");
            emit("\tsyscall");
            emit("\taddq $16, %%rsp");
            emit("\taddq $16, %%rsp");
            par ^= 0;
            return 0;
        }
        if (e->ival == 37) {                       /* localport(fd) */
            gen_expr(m, e->a); spush();
            emit("\tsubq $32, %%rsp");
            emit("\tmovl $16, (%%rsp)");
            emit("\tleaq 16(%%rsp), %%rsi");
            emit("\tmovl 32(%%rsp), %%edi");
            emit("\tmovq %%rsp, %%rdx");
            emit("\tmovl $51, %%eax");            /* getsockname */
            emit("\tsyscall");
            emit("\ttestq %%rax, %%rax");
            emit("\tjs .LPORTX%de", ln);
            emit("\tmovzbl 18(%%rsp), %%eax");
            emit("\tshll $8, %%eax");
            emit("\tmovzbl 19(%%rsp), %%ecx");
            emit("\torl %%ecx, %%eax");
            emit("\tjmp .LPORTX%dd", ln);
            emit(".LPORTX%de:", ln);
            emit("\tmovl $-1, %%eax");
            emit(".LPORTX%dd:", ln);
            emit("\taddq $48, %%rsp");
            par ^= 1;
            return 0;
        }
        if (e->ival == 34) {                       /* setnonblock(fd) */
            gen_expr(m, e->a); spush();
            emit("\tmovl (%%rsp), %%edi");
            emit("\tmovl $4, %%esi");               /* F_SETFL */
            emit("\tmovl $2048, %%edx");            /* O_NONBLOCK */
            emit("\tmovl $72, %%eax");              /* fcntl */
            emit("\tsyscall");
            emit("\taddq $8, %%rsp");
            par ^= 1;
            return 0;
        }
        if (e->ival == 35) {                       /* key() */
            emit("\txorl %%edi, %%edi");
            emit("\tmovl $4, %%esi");
            emit("\tmovl $2048, %%edx");
            emit("\tmovl $72, %%eax");
            emit("\tsyscall");
            emit("\tleaq kop_iobuf(%%rip), %%rsi");
            emit("\txorl %%edi, %%edi");
            emit("\tmovl $1, %%edx");
            emit("\txorl %%eax, %%eax");            /* read */
            emit("\tsyscall");
            emit("\tcmpq $1, %%rax");
            emit("\tjne .LKEYX%de", ln);
            emit("\tleaq kop_iobuf(%%rip), %%rcx");
            emit("\tmovzbl (%%rcx), %%eax");
            emit("\tjmp .LKEYX%dd", ln);
            emit(".LKEYX%de:", ln);
            emit("\tmovl $-1, %%eax");
            emit(".LKEYX%dd:", ln);
            return 0;
        }
        if (e->ival == 36) {                       /* ttyRaw(on) */
            gen_expr(m, e->a); spush();
            emit("\tmovl (%%rsp), %%edi");
            emit("\taddq $8, %%rsp");
            par ^= 1;
            emit("\ttestl %%edi, %%edi");
            emit("\tje .LTTYX%ds", ln);
            emit("\tleaq .Lstty_raw(%%rip), %%rdi");
            emit("\tjmp .LTTYX%dc", ln);
            emit(".LTTYX%ds:", ln);
            emit("\tleaq .Lstty_sane(%%rip), %%rdi");
            emit(".LTTYX%dc:", ln);
            int pad = par;
            if (pad) emit("\tsubq $8, %%rsp");
            emit("\tcall system@PLT");
            if (pad) emit("\taddq $8, %%rsp");
            emit("\tmovl $0, %%eax");
            return 0;
        }
        if (e->ival == 40 || e->ival == 41) {      /* v1.18: konversi representasi via helper */
            gen_expr(m, e->a); spush();
            emit("\tmovq (%%rsp), %%rdi");
            emit("\tcall %s", e->ival == 40 ? "kop_mkstr" : "kop_strbytes");
            emit("\taddq $16, %%rsp");
            return 0;
        }
        int n = 0;
        for (Node *aa = e->a; aa; aa = aa->next) {
            gen_expr(m, aa);
            spush();
            n++;
        }
        if (e->ival == 0) {                              /* socket(domain, type) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1 - 0) * 8);
            emit("\tmovl %d(%%rsp), %%esi", (n - 1 - 1) * 8);
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\txorl %%edx, %%edx");                /* proto */
            emit("\tmovl $41, %%eax");                  /* __NR_socket */
            emit("\tsyscall");
        } else if (e->ival == 1) {                       /* connect(fd, ipBE, port) */
            /* sockaddr_in 16 byte di stack, lalu syscall */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1 - 0) * 8);   /* fd */
            emit("\tmovl %d(%%rsp), %%esi", (n - 1 - 1) * 8);   /* ipBE */
            emit("\tmovl %d(%%rsp), %%edx", (n - 1 - 2) * 8);   /* port */
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tpushq $0");
            emit("\tpushq $0");                         /* 16 byte nol */
            emit("\tandq $-16, %%rsp");
            emit("\tmovb $2, (%%rsp)");                 /* AF_INET */
            emit("\tmovl %%edx, %%eax");
            emit("\tshrl $8, %%eax");
            emit("\tmovb %%al, 2(%%rsp)");              /* port hi */
            emit("\tmovb %%dl, 3(%%rsp)");              /* port lo */
            emit("\tmovl %%esi, %%eax");
            emit("\tshrl $24, %%eax");
            emit("\tmovb %%al, 4(%%rsp)");
            emit("\tmovl %%esi, %%eax");
            emit("\tshrl $16, %%eax");
            emit("\tmovb %%al, 5(%%rsp)");
            emit("\tmovl %%esi, %%eax");
            emit("\tshrl $8, %%eax");
            emit("\tmovb %%al, 6(%%rsp)");
            emit("\tmovb %%sil, 7(%%rsp)");
            emit("\tmovq %%rsp, %%rsi");
            emit("\tmovl $16, %%edx");
            emit("\tmovl $42, %%eax");                  /* __NR_connect */
            emit("\tsyscall");
            emit("\taddq $16, %%rsp");
        } else if (e->ival == 2) {                       /* rdb(fd) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1) * 8);
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tpushq $0");                         /* buffer 1B */
            emit("\tpushq $0");
            emit("\tmovq %%rsp, %%rsi");
            emit("\tmovl $1, %%edx");
            emit("\txorl %%eax, %%eax");                /* __NR_read */
            emit("\tsyscall");
            emit("\tcmpq $1, %%rax");
            emit("\tjne 1f");
            emit("\tmovzbq (%%rsp), %%rax");            /* byte 0..255 */
            emit("\tjmp 2f");
            emit("1:");
            emit("\tmovl $-1, %%eax");                  /* EOF / error */
            emit("2:");
            emit("\taddq $16, %%rsp");
        } else if (e->ival == 3) {                       /* wrb(fd, b) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1 - 0) * 8);   /* fd */
            emit("\tmovl %d(%%rsp), %%esi", (n - 1 - 1) * 8);   /* byte */
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tpushq $0");
            emit("\tpushq $0");
            emit("\tmovb %%sil, (%%rsp)");
            emit("\tmovq %%rsp, %%rsi");
            emit("\tmovl $1, %%edx");
            emit("\tmovl $1, %%eax");                   /* __NR_write */
            emit("\tsyscall");
            emit("\taddq $16, %%rsp");
            emit("\tdecl %%eax");
            emit("\tjne 1f");
            emit("\txorl %%eax, %%eax");
            emit("\tjmp 2f");
            emit("1:");
            emit("2:");
        } else if (e->ival == 4) {                       /* close(fd) */
            emit("\tmovl %d(%%rsp), %%edi", (n - 1) * 8);
            emit("\taddq $%d, %%rsp", n * 8);
            emit("\tmovl $3, %%eax");                   /* __NR_close */
            emit("\tsyscall");
        }
        return 0;
    }
    case EX_ENVLOAD:
        emit("\tmovq -8(%%rbp), %%rax");  /* slot 0 = param $env */
        if (is64ty((Ty)e->dty)) {
            emit("\tmovq %ld(%%rax), %%rax", 24 + 8 * (long)e->ival);
            return 1;
        }
        emit("\tmovl %ld(%%rax), %%eax", 24 + 8 * (long)e->ival);
        break;
    case EX_TERN: {
        int lelse = nlab++, lend = nlab++;
        gen_expr(m, e->a);
        emit("\ttestl %%eax, %%eax");
        emit("\tje .L%d", lelse);
        int w1 = gen_expr(m, e->b);
        emit("\tjmp .L%d", lend);
        emit(".L%d:", lelse);
        int w2 = gen_expr(m, e->c);
        emit(".L%d:", lend);
        return (w1 && w2) ? 1 : is64ty((Ty)e->dty);
    }
    case EX_BIN: {
        Ty bl = (Ty)e->a->dty, br = (Ty)e->b->dty, bres = (Ty)e->dty;
        if (!((bl == T_INT || bl == T_BOOL) && (br == T_INT || br == T_BOOL) &&
              (bres == T_INT || bres == T_BOOL))) {
            return gen_bin_typed(m, e);
        }
        char op = e->str[0];
        if ((op == '&' || op == '|') && e->str[1] != 0) { gen_bool_chain(m, e); break; }
        gen_expr(m, e->a);
        spush();
        gen_expr(m, e->b);
        spop("rcx");                       /* konvensi: ecx=left, eax=right */
        /* operator 2-karakter dulu supaya tidak tabrak case '<' / '>' */
        if (!strcmp(e->str, "<<") || !strcmp(e->str, ">>") ||
            !strcmp(e->str, ">>>") || op == '-' || op == '/' || op == '%') {
            emit("\txchgl %%eax, %%ecx");  /* xchg: eax=left, ecx(count) untuk shift/idiv */
        }
        if (!strcmp(e->str, "<<")) emit("\tshll %%cl, %%eax");
        else if (!strcmp(e->str, ">>")) emit("\tsarl %%cl, %%eax");
        else if (!strcmp(e->str, ">>>")) emit("\tshrl %%cl, %%eax");
        else switch (op) {
        case '+': emit("\taddl %%ecx, %%eax"); break;
        case '-': emit("\tsubl %%ecx, %%eax"); break;   /* eax = left - right */
        case '*': emit("\timull %%ecx, %%eax"); break;
        case '/':
            emit("\tcltd");
            emit("\tidivl %%ecx");
            break;
        case '%':
            emit("\tcltd");
            emit("\tidivl %%ecx");
            emit("\tmovl %%edx, %%eax");
            break;
        default: {
            const char *set;
            if (!strcmp(e->str, "<=")) set = "setle";
            else if (!strcmp(e->str, ">=")) set = "setge";
            else if (op == '<') set = "setl";
            else if (op == '>') set = "setg";
            else if (op == '=') set = "sete";
            else if (op == '!') set = "setne";
            else if (op == '&') { emit("\tandl %%ecx, %%eax"); break; }  /* bitwise (len 1) */
            else if (op == '|') { emit("\torl %%ecx, %%eax"); break; }
            else if (op == '^') { emit("\txorl %%ecx, %%eax"); break; }
            else die("gen_x64: operator '%s'", e->str);
            emit("\tcmpl %%eax, %%ecx");   /* bandingkan eksplisit left vs right */
            emit("\t%s %%al", set);
            emit("\tmovzbl %%al, %%eax");
        }
        }
        return 0;
    }
    default:
        die("gen_x64: ekspresi kind=%d", e->kind);
        return 0;
    }
    return 0;
}

