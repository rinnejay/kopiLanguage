/* vm_ops_base.inc — potongan switch run() di kopivm.c. */

        case OP_LINE: {
            int ln = m->code[f->pc] | (m->code[f->pc + 1] << 8);
            f->pc += 2;
            if (ln) f->line = ln;
            break;
        }
        case OP_SRCFILE: {
            int ci = m->code[f->pc] | (m->code[f->pc + 1] << 8);
            f->pc += 2;
            if (ci >= 0 && ci < ncp) f->file = cpool[ci];
            break;
        }
        case OP_ICONST: {
            int32_t v = (int32_t)m->code[f->pc] | ((int32_t)m->code[f->pc + 1] << 8) |
                        ((int32_t)m->code[f->pc + 2] << 16) | ((int32_t)m->code[f->pc + 3] << 24);
            f->pc += 4;
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = v;
            break;
        }
        case OP_ILOAD: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            if ((int)s >= MAXLOC) dievm("slot lokal luar batas");
            stackv[sp++] = localsv[nframes - 1][s];
            break;
        }
        case OP_ISTORE: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            if ((int)s >= MAXLOC) dievm("slot lokal luar batas");
            localsv[nframes - 1][s] = stackv[--sp];
            break;
        }
        case OP_POP: sp--; break;
        /* semantik int 32-bit (wrap, seperti Java/backend native) */
        case OP_ADD: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a + b); break;
        case OP_SUB: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a - b); break;
        case OP_MUL: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a * b); break;
        case OP_DIV:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("pembagian dengan nol");
            stackv[sp++] = a / b;
            break;
        case OP_MOD:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("modulus dengan nol");
            stackv[sp++] = a % b;
            break;
        case OP_NEG: a = stackv[--sp]; stackv[sp++] = -a; break;
        case OP_NOT: a = stackv[--sp]; stackv[sp++] = a ? 0 : 1; break;
        case OP_LT: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a < b; break;
        case OP_GT: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a > b; break;
        case OP_LE: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a <= b; break;
        case OP_GE: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a >= b; break;
        case OP_EQ: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a == b; break;
        case OP_NE: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a != b; break;
        case OP_BAND: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a & b); break;
        case OP_BOR:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a | b); break;
        case OP_BXOR: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a ^ b); break;
        case OP_SHL:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a << (b & 31)); break;
        case OP_SHR:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)(a >> (b & 31)); break;
        case OP_USHR: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (int32_t)((uint32_t)a >> (b & 31)); break;
        case OP_BNOT: a = stackv[--sp]; stackv[sp++] = ~a; break;
        case OP_JMP:
            f->pc = (int32_t)((uint32_t)m->code[f->pc] | ((uint32_t)m->code[f->pc + 1] << 8) |
                              ((uint32_t)m->code[f->pc + 2] << 16) | ((uint32_t)m->code[f->pc + 3] << 24));
            break;
        case OP_JMPF:
        case OP_JMPT: {
            uint32_t t = (uint32_t)m->code[f->pc] | ((uint32_t)m->code[f->pc + 1] << 8) |
                         ((uint32_t)m->code[f->pc + 2] << 16) | ((uint32_t)m->code[f->pc + 3] << 24);
            f->pc += 4;
            a = stackv[--sp];
            if ((op == OP_JMPF && !a) || (op == OP_JMPT && a)) {
                if (t > m->clen) dievm("target lompatan luar batas");
                f->pc = (long)t;
            }
            break;
        }
        case OP_CALL: {
            uint16_t cm = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            uint16_t cn = (uint16_t)m->code[f->pc + 2] | ((uint16_t)m->code[f->pc + 3] << 8);
            f->pc += 4;
            if (cm >= nmeths) dievm("metode luar batas");
            Meth *mm = &meths[cm];
            if (cn != mm->nargs) dievm("jumlah argumen argumen tidak cocok");
            if (nframes >= MAXFRAMES) dievm("call stack meluap");
            if (mm->nlocals >= MAXLOC) dievm("terlalu banyak local");
            long base = sp - cn;
            if (base < 0) dievm("stack korup");
            memset(localsv[nframes], 0, sizeof localsv[nframes]);
            for (int i = 0; i < cn; i++) localsv[nframes][i] = stackv[base + i];
            sp = base;
            frames[nframes].meth = cm;
            frames[nframes].pc = 0;
            frames[nframes].retbase = base;
            frames[nframes].line = 0;
            frames[nframes].file = 0;
            nframes++;
            break;
        }
        case OP_RET: {
            int64_t v = stackv[--sp];
            long rb = f->retbase;
            nframes--;
            sp = rb;
            stackv[sp++] = v;
            break;
        }
        case OP_PRINTILN: { int64_t v = stackv[--sp]; printf("%" PRId64 "\n", v); break; }
        case OP_PRINTI: { int64_t v = stackv[--sp]; printf("%" PRId64, v); break; }
        case OP_PRINTSLN: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            printf("%s\n", cpool[s]);
            break;
        }
        case OP_PRINTS: {
            uint16_t s = m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            printf("%s", cpool[s]);
            break;
        }
        case OP_NEWCLO: {
            uint16_t cm = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            uint16_t cn = (uint16_t)m->code[f->pc + 2] | ((uint16_t)m->code[f->pc + 3] << 8);
            f->pc += 4;
            if (cm >= (uint16_t)nmeths) dievm("closure: metode luar batas");
            Meth *mm = &meths[cm];
            if (mm->nargs == 0) dievm("closure: metode tanpa env");
            long nv = 3 + cn;
            if (heap_used + nv > heap_cap) oom_arena();
            if (mm->nlocals >= MAXLOC) dievm("terlalu banyak local (lambda)");
            long *blk = heap + heap_used;
            heap_used += nv;
            blk[0] = cm;                       /* identitas kode = index metode */
            blk[1] = cn;                       /* ncap */
            blk[2] = mm->nargs - 1;            /* nuser */
            for (int k = 0; k < cn; k++) {
                uint16_t s = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                if ((int)s >= MAXLOC) dievm("slot capture luar batas");
                blk[3 + k] = localsv[nframes - 1][s];
            }
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = (int64_t)blk;
            break;
        }
        case OP_CALLIND: {
            uint16_t cn = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            if (sp < 1) dievm("stack korup (CALLIND)");
            long clo = stackv[--sp];
            long *blk = (long *)clo;
            if (clo == 0 || !in_heap(blk))
                dievm("nilai lambda tidak valid (env di luar heap)");
            long cm = blk[0];
            if (cm >= nmeths) dievm("closure korup (index metode)");
            Meth *mm = &meths[cm];
            if ((long)cn != blk[2] || (long)cn != mm->nargs - 1)
                dievm("jumlah argumen lambda tidak cocok");
            if (nframes >= MAXFRAMES) dievm("call stack meluap");
            if (mm->nlocals >= MAXLOC) dievm("terlalu banyak local (lambda)");
            long base = sp - cn;
            if (base < 0) dievm("stack korup (CALLIND args)");
            memset(localsv[nframes], 0, sizeof localsv[nframes]);
            localsv[nframes][0] = clo;
            for (int i = 0; i < (int)cn; i++)
                localsv[nframes][i + 1] = stackv[base + i];
            sp = base;
            frames[nframes].meth = (int)cm;
            frames[nframes].pc = 0;
            frames[nframes].retbase = base;
            frames[nframes].line = 0;
            frames[nframes].file = 0;
            nframes++;
            break;
        }
        case OP_ENVLOAD: {
            uint16_t cj = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            long clo = localsv[nframes - 1][0];
            long *blk = (long *)clo;
            if (clo == 0 || !in_heap(blk))
                dievm("env lambda tidak valid");
            long ncap = blk[1];
            if ((long)cj >= ncap) dievm("capture korup");
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = blk[3 + cj];
            break;
        }
        case OP_NEWARR: {
            int64_t n = stackv[--sp];
            if (n < 0) dievm2("ukuran array negatif", (long)n);
            if (heap_used + n + 1 > heap_cap) oom_arena();
            long *blk = heap + heap_used;
            heap_used += n + 1;
            blk[0] = (long)n;
            if (n > 0) memset(blk + 1, 0, (size_t)n * sizeof(long));
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = (int64_t)blk;
            break;
        }
        case OP_IDXR: {
            int64_t idx = stackv[--sp];
            long *blk = (long *)stackv[--sp];
            if (!blk || !in_heap(blk))
                dievm("array tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks array di luar batas", (long)idx);
            stackv[sp++] = blk[1 + idx];
            break;
        }
        case OP_IDXW: {
            int64_t idx = stackv[--sp];
            long *blk = (long *)stackv[--sp];
            int64_t val = stackv[--sp];
            if (!blk || !in_heap(blk))
                dievm("array tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks array di luar batas", (long)idx);
            blk[1 + idx] = val;
            break;
        }
        case OP_IDXB: {                       /* v1.18: byte ke-i String packed */
            int64_t idx = stackv[--sp];
            long *blk = (long *)stackv[--sp];
            if (!blk || !in_heap(blk))
                dievm("string tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks string di luar batas", (long)idx);
            stackv[sp++] = getb(blk, idx);
            break;
        }
        case OP_ALEN: {
            long *blk = (long *)stackv[--sp];
            stackv[sp++] = blk[0];
            break;
        }
