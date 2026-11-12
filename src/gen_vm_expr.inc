/* gen_vm_expr.inc — ekspresi bytecode KOP1 (bagian dari gen_vm.c). */

static void gen_bin_typed(Node *e) {
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

    /* identitas objek (v1.12): ==/!= pada dua array → bandingkan referensi 64-bit */
    if (ty_is_arr(lt) && lt == rt && isCmp) {
        gen_expr(e->a);
        gen_expr(e->b);
        em8(op == '=' ? OP_EQ64 : OP_NE64);
        return;
    }

    gen_expr(e->a);
    emit_conv(lt, P);
    gen_expr(e->b);
    if (!isShift) emit_conv(rt, P);

    if (ty_is_float(P)) {
        int sub = (P == T_FLOAT) ? 0 : 1;
        if (isCmp) {
            if (op == '=') em8(OP_FEQ);
            else if (op == '!') em8(OP_FNE);
            else if (!strcmp(e->str, "<=")) em8(OP_FLE);
            else if (!strcmp(e->str, ">=")) em8(OP_FGE);
            else if (op == '<') em8(OP_FLT);
            else em8(OP_FGT);
            em8(sub);
        } else {
            if (op == '+') em8(OP_FADD);
            else if (op == '-') em8(OP_FSUB);
            else if (op == '*') em8(OP_FMUL);
            else if (op == '/') em8(OP_FDIV);
            else if (op == '%') em8(OP_FMOD);
            else die("gen_vm: operator float '%s'", e->str);
            em8(sub);
        }
        return;
    }
    int uns = ty_is_uint(P);
    if (isShift) {
        int mask = ty_width(P) - 1;
        if (mask <= 0) mask = 31;
        if (!strcmp(e->str, "<<")) em8(OP_SHL64);
        else if (!strcmp(e->str, ">>")) em8(OP_SHR64);
        else em8(OP_USHR64);
        em8(mask);
        emit_norm(P);
        return;
    }
    if (isCmp) {
        if (op == '=') em8(OP_EQ64);
        else if (op == '!') em8(OP_NE64);
        else if (!strcmp(e->str, "<=")) em8(uns ? OP_LEU64 : OP_LE64);
        else if (!strcmp(e->str, ">=")) em8(uns ? OP_GEU64 : OP_GE64);
        else if (op == '<') em8(uns ? OP_LTU64 : OP_LT64);
        else em8(uns ? OP_GTU64 : OP_GT64);
        return;
    }
    switch (op) {
    case '+': em8(OP_ADD64); break;
    case '-': em8(OP_SUB64); break;
    case '*': em8(OP_MUL64); break;
    case '/': em8(uns ? OP_UDIV64 : OP_DIV64); break;
    case '%': em8(uns ? OP_UMOD64 : OP_MOD64); break;
    case '&': em8(OP_AND64); break;
    case '|': em8(OP_OR64); break;
    case '^': em8(OP_XOR64); break;
    default: die("gen_vm: operator '%s'", e->str);
    }
    emit_norm(P);
}

static void gen_expr(Node *e) {
    switch (e->kind) {
    case EX_INT: {
        Ty t = (Ty)e->dty;
        if (t == T_INT) {
            em8(OP_ICONST); em32(e->ival);
        } else {
            unsigned long long bits;
            if (t == T_FLOAT) {
                union { float f; uint32_t u; } cv;
                cv.f = (float)e->fval;
                bits = cv.u;
            } else if (t == T_DOUBLE) {
                union { double d; uint64_t u; } cv;
                cv.d = e->fval;
                bits = cv.u;
            } else {
                bits = (unsigned long long)e->ival;
            }
            em8(OP_ICONST64);
            for (int k = 0; k < 8; k++) em8((int)((bits >> (8 * k)) & 0xff));
        }
        break;
    }
    case EX_BOOL: em8(OP_ICONST); em32(e->ival); break;
    case EX_STRLIT:
        em8(OP_NET); em16(5); em16((int)e->cp);
        break;
    case EX_CAST:
        gen_expr(e->a);
        emit_conv((Ty)e->a->dty, (Ty)e->dty);
        break;
    case EX_VAR: em8(OP_ILOAD); em16((int)e->ival); break;
    case EX_UN: {
        Ty t = (Ty)e->dty;
        gen_expr(e->a);
        if (e->str[0] == '!') { em8(OP_NOT); break; }
        if (ty_is_float(t)) {
            em8(OP_FNEG);
            em8(t == T_FLOAT ? 0 : 1);
            break;
        }
        if (t == T_INT) {
            em8(e->str[0] == '-' ? OP_NEG : OP_BNOT);
            break;
        }
        em8(e->str[0] == '-' ? OP_NEG64 : OP_BNOT64);
        emit_norm(t);
        break;
    }
    case EX_CALL: {
        int i = 0;
        Param *pp = e->meth ? e->meth->params : 0;
        for (Node *a = e->a; a; a = a->next, pp = pp ? pp->next : 0) {
            gen_expr(a);
            if (pp) emit_conv((Ty)a->dty, (Ty)pp->ty);
            i++;
        }
        em8(OP_CALL); em16((int)e->ival); em16(i);
        break;
    }
    case EX_ARROW: {
        Method *hm = (Method *)e->cp;
        em8(OP_NEWCLO);
        em16(hm->index);
        em16(hm->ncapt);
        for (int j = 0; j < hm->ncapt; j++)
            em16((int)hm->captslot[j]);
        break;
    }
    case EX_FCALL: {
        int i = 0;
        for (Node *a = e->a; a; a = a->next) { gen_expr(a); i++; }
        gen_expr(e->b);
        em8(OP_CALLIND); em16(i);
        break;
    }
    case EX_NET:
        if (e->ival != 5 && e->ival != 6)     /* bytes/openr: literal dicerna compile-time */
            for (Node *aa = e->a; aa; aa = aa->next) gen_expr(aa);
        em8(OP_NET);
        em16((int)e->ival);
        if (e->ival == 5 || e->ival == 6) em16((int)e->cp);   /* bytes/openr: cpool */
        break;
    case EX_ENVLOAD:
        em8(OP_ENVLOAD);
        em16((int)e->ival);
        break;
    case EX_NEWARR:
        gen_expr(e->a);
        em8(OP_NEWARR);
        break;
    case EX_IDX:
        /* v1.22: a[i] dengan a dan i lokal → satu opcode */
        if (e->a->kind == EX_VAR && e->b->kind == EX_VAR &&
            (Ty)e->a->dty != T_STR) {
            em8(OP_IDXR_L);
            em16((int)e->a->ival); em16((int)e->b->ival);
            break;
        }
        gen_expr(e->a);          /* array pointer */
        gen_expr(e->b);          /* indeks */
        if ((Ty)e->a->dty == T_STR) em8(OP_IDXB);   /* v1.18: String packed */
        else em8(OP_IDXR);
        break;
    case EX_LEN:
        gen_expr(e->a);
        em8(OP_ALEN);
        break;
    case EX_TERN: {
        int lelse = newlab(), lend = newlab();
        gen_expr(e->a);
        jmp(OP_JMPF, lelse);
        gen_expr(e->b);
        jmp(OP_JMP, lend);
        mark(lelse);
        gen_expr(e->c);
        mark(lend);
        break;
    }
    case EX_BIN: {
        Ty bl = (Ty)e->a->dty, br = (Ty)e->b->dty, bres = (Ty)e->dty;
        if (!((bl == T_INT || bl == T_BOOL) && (br == T_INT || br == T_BOOL) &&
              (bres == T_INT || bres == T_BOOL))) {
            gen_bin_typed(e);
            break;
        }
        char op = e->str[0];
        if ((op == '&' || op == '|') && e->str[1] != 0) {
            /* short-circuit: hasil selalu 0/1 */
            int lres = newlab(), lend = newlab();
            int vfall = op == '&' ? 1 : 0;   /* nilai jika keduanya benar/salah */
            int vres  = op == '&' ? 0 : 1;   /* nilai saat lompatan dipakai */
            gen_expr(e->a);
            jmp(op == '&' ? OP_JMPF : OP_JMPT, lres);
            gen_expr(e->b);
            jmp(op == '&' ? OP_JMPF : OP_JMPT, lres);
            em8(OP_ICONST); em32(vfall);
            jmp(OP_JMP, lend);
            mark(lres);
            em8(OP_ICONST); em32(vres);
            mark(lend);
            break;
        }
        /* v1.22: dua operand lokal + operator aritmetika → satu opcode */
        if (e->a->kind == EX_VAR && e->b->kind == EX_VAR &&
            (Ty)e->dty == T_INT && bin_code(e->str) >= 0) {
            em8(OP_BIN_L); em8(0); em8(bin_code(e->str));
            em16((int)e->a->ival); em16((int)e->b->ival);
            break;
        }
        gen_expr(e->a);
        gen_expr(e->b);
        /* operator 2-karakter dulu, supaya tidak tabrak case '<' / '>' */
        if (!strcmp(e->str, "<=")) em8(OP_LE);
        else if (!strcmp(e->str, ">=")) em8(OP_GE);
        else if (!strcmp(e->str, "<<")) em8(OP_SHL);
        else if (!strcmp(e->str, ">>")) em8(OP_SHR);
        else if (!strcmp(e->str, ">>>")) em8(OP_USHR);
        else switch (op) {
        case '+': em8(OP_ADD); break;
        case '-': em8(OP_SUB); break;
        case '*': em8(OP_MUL); break;
        case '/': em8(OP_DIV); break;
        case '%': em8(OP_MOD); break;
        case '<': em8(OP_LT); break;
        case '>': em8(OP_GT); break;
        case '=': em8(OP_EQ); break;
        case '!': em8(OP_NE); break;
        case '&': em8(OP_BAND); break;      /* bitwise (len 1) */
        case '|': em8(OP_BOR); break;
        case '^': em8(OP_BXOR); break;
        default: die("gen_vm: operator '%s'", e->str);
        }
        break;
    }
    default:
        die("gen_vm: ekspresi kind=%d", e->kind);
    }
}

/* tulis satu metode: isi buffer bytecode lalu flush sebagai entri file */
