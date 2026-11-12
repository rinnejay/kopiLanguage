/* vm_ops_num.inc — potongan switch run() di kopivm.c. */

        case OP_ICONST64: {
            uint64_t v = rd64code(m, &f->pc);
            if (sp >= MAXSTACK) dievm("operand stack penuh");
            stackv[sp++] = (int64_t)v;
            break;
        }
        case OP_ADD64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a + b; break;
        case OP_SUB64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a - b; break;
        case OP_MUL64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a * b; break;
        case OP_DIV64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("pembagian dengan nol");
            stackv[sp++] = a / b;
            break;
        case OP_MOD64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("modulus dengan nol");
            stackv[sp++] = a % b;
            break;
        case OP_UDIV64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("pembagian dengan nol");
            stackv[sp++] = (int64_t)((uint64_t)a / (uint64_t)b);
            break;
        case OP_UMOD64:
            b = stackv[--sp]; a = stackv[--sp];
            if (b == 0) dievm("modulus dengan nol");
            stackv[sp++] = (int64_t)((uint64_t)a % (uint64_t)b);
            break;
        case OP_AND64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a & b; break;
        case OP_OR64:  b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a | b; break;
        case OP_XOR64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a ^ b; break;
        case OP_SHL64:
        case OP_SHR64:
        case OP_USHR64: {
            int mask = m->code[f->pc++];
            b = stackv[--sp]; a = stackv[--sp];
            int c = (int)(b & mask);
            if (op == OP_SHL64) stackv[sp++] = (int64_t)((uint64_t)a << c);
            else if (op == OP_SHR64) stackv[sp++] = a >> c;
            else stackv[sp++] = (int64_t)((uint64_t)a >> c);
            break;
        }
        case OP_NEG64: a = stackv[--sp]; stackv[sp++] = -a; break;
        case OP_BNOT64: a = stackv[--sp]; stackv[sp++] = ~a; break;
        case OP_LT64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a < b; break;
        case OP_GT64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a > b; break;
        case OP_LE64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a <= b; break;
        case OP_GE64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a >= b; break;
        case OP_LTU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a < (uint64_t)b; break;
        case OP_GTU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a > (uint64_t)b; break;
        case OP_LEU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a <= (uint64_t)b; break;
        case OP_GEU64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = (uint64_t)a >= (uint64_t)b; break;
        case OP_EQ64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a == b; break;
        case OP_NE64: b = stackv[--sp]; a = stackv[--sp]; stackv[sp++] = a != b; break;
        case OP_FADD:
        case OP_FSUB:
        case OP_FMUL:
        case OP_FDIV:
        case OP_FMOD: {
            int sub = m->code[f->pc++];
            b = stackv[--sp]; a = stackv[--sp];
            if (sub == 0) {
                float x = bits_f(a), y = bits_f(b);
                if (op == OP_FADD) stackv[sp++] = f_bits(x + y);
                else if (op == OP_FSUB) stackv[sp++] = f_bits(x - y);
                else if (op == OP_FMUL) stackv[sp++] = f_bits(x * y);
                else if (op == OP_FDIV) stackv[sp++] = f_bits(x / y);
                else {
                    double q = dtrunc_safe((double)(x / y));
                    stackv[sp++] = f_bits((float)((double)x - q * (double)y));
                }
            } else {
                double x = bits_d(a), y = bits_d(b);
                if (op == OP_FADD) stackv[sp++] = d_bits(x + y);
                else if (op == OP_FSUB) stackv[sp++] = d_bits(x - y);
                else if (op == OP_FMUL) stackv[sp++] = d_bits(x * y);
                else if (op == OP_FDIV) stackv[sp++] = d_bits(x / y);
                else {
                    double q = dtrunc_safe(x / y);
                    stackv[sp++] = d_bits(x - q * y);
                }
            }
            break;
        }
        case OP_FNEG: {
            int sub = m->code[f->pc++];
            a = stackv[--sp];
            stackv[sp++] = (sub == 0) ? f_bits(-bits_f(a)) : d_bits(-bits_d(a));
            break;
        }
        case OP_FLT:
        case OP_FGT:
        case OP_FLE:
        case OP_FGE:
        case OP_FEQ:
        case OP_FNE: {
            int sub = m->code[f->pc++];
            b = stackv[--sp]; a = stackv[--sp];
            if (sub == 0) {
                float x = bits_f(a), y = bits_f(b);
                if (op == OP_FLT) stackv[sp++] = x < y;
                else if (op == OP_FGT) stackv[sp++] = x > y;
                else if (op == OP_FLE) stackv[sp++] = x <= y;
                else if (op == OP_FGE) stackv[sp++] = x >= y;
                else if (op == OP_FEQ) stackv[sp++] = x == y;
                else stackv[sp++] = x != y;
            } else {
                double x = bits_d(a), y = bits_d(b);
                if (op == OP_FLT) stackv[sp++] = x < y;
                else if (op == OP_FGT) stackv[sp++] = x > y;
                else if (op == OP_FLE) stackv[sp++] = x <= y;
                else if (op == OP_FGE) stackv[sp++] = x >= y;
                else if (op == OP_FEQ) stackv[sp++] = x == y;
                else stackv[sp++] = x != y;
            }
            break;
        }
        case OP_CONV: {
            int from = m->code[f->pc++];
            int to = m->code[f->pc++];
            a = stackv[--sp];
            stackv[sp++] = conv_val(a, from, to);
            break;
        }
        case OP_NORM: {
            int t = m->code[f->pc++];
            stackv[sp - 1] = norm_val(stackv[sp - 1], t);
            break;
        }
        case OP_PRINTNUM: {
            int t = m->code[f->pc++];
            int ln = m->code[f->pc++];
            int64_t v = stackv[--sp];
            if (t == T_LONG) printf("%" PRId64 "%s", v, ln ? "\n" : "");
            else if (t == T_UINT64) printf("%" PRIu64 "%s", (uint64_t)v, ln ? "\n" : "");
            else if (t == T_UINT8 || t == T_UINT16 || t == T_UINT32)
                printf("%" PRIu32 "%s", (uint32_t)v, ln ? "\n" : "");
            else if (t == T_BYTE || t == T_SHORT)
                printf("%" PRId64 "%s", v, ln ? "\n" : "");
            else if (t == T_FLOAT) printf("%.9g%s", (double)bits_f(v), ln ? "\n" : "");
            else printf("%.15g%s", bits_d(v), ln ? "\n" : "");
            break;
        }
        case OP_PRINTBUF: {
            int ln = m->code[f->pc++];
            long *buf = (long *)stackv[--sp];
            long len = buf[0];
            unsigned char *tmp = (unsigned char *)malloc((size_t)(len > 0 ? len : 1));
            if (!tmp) dievm("print: memori habis");
            for (long i = 0; i < len; i++) tmp[i] = (unsigned char)(buf[1 + i] & 0xff);
            fwrite(tmp, 1, (size_t)len, stdout);
            free(tmp);
            if (ln) fputc(10, stdout);
            break;
        }
        case OP_PRINTSTR: {                   /* v1.18: String packed → fd 1 */
            int ln = m->code[f->pc++];
            long *blk = (long *)stackv[--sp];
            long len = blk[0];
            for (long i = 0; i < len; i++) fputc((int)getb(blk, i), stdout);
            if (ln) fputc(10, stdout);
            break;
        }
        case OP_RENTER: {                      /* v1.21: arena blok malloc<...> */
            uint32_t bytes = (uint32_t)m->code[f->pc] |
                             ((uint32_t)m->code[f->pc + 1] << 8) |
                             ((uint32_t)m->code[f->pc + 2] << 16) |
                             ((uint32_t)m->code[f->pc + 3] << 24);
            f->pc += 4;
            if (asp >= MAXARENA) dievm("terlalu banyak malloc{} bersarang");
            /* v1.22: bytes==0 → OTOMATIS (mmap 4096 MB, RAM sesuai kebutuhan) */
            int auto_mode = (bytes == 0);
            long maplen = auto_mode ? AUTOCAP : ((long)bytes / 8) * 8;
            long slots = maplen / 8;
            long *nb;
            if (auto_mode) {
                void *mp = mmap(NULL, (size_t)maplen, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                                -1, 0);
                if (mp == MAP_FAILED) {
                    char buf[192];
                    snprintf(buf, sizeof buf,
                        "gagal menyediakan arena malloc otomatis %ld MB", AUTOCAP >> 20);
                    dievm(buf);
                }
                nb = (long *)mp;
            } else {
                nb = (long *)malloc((size_t)maplen);
                if (!nb) {
                    char buf[192];
                    snprintf(buf, sizeof buf,
                        "gagal mengalokasikan arena malloc %ld MB", maplen >> 20);
                    dievm(buf);
                }
            }
            astk[asp].base = heap;
            astk[asp].cap = heap_cap;
            astk[asp].used = heap_used;
            astk[asp].mmapped = heap_mmapped;
            astk[asp].auto_mode = heap_auto;
            asp++;
            heap = nb;
            heap_cap = slots;
            heap_used = 0;
            heap_mmapped = auto_mode;
            heap_auto = auto_mode;
            break;
        }
        case OP_RLEAVE:                        /* v1.21: keluar blok malloc */
            if (asp <= 0) dievm("internal: keluar arena tanpa masuk");
            asp--;
            if (heap_mmapped) munmap(heap, (size_t)heap_cap * 8);
            else free(heap);
            heap = astk[asp].base;
            heap_cap = astk[asp].cap;
            heap_used = astk[asp].used;
            heap_mmapped = astk[asp].mmapped;
            heap_auto = astk[asp].auto_mode;
            break;
        case OP_MARK:                          /* v1.20: memMark manual */
            stackv[sp++] = (int64_t)heap_used;
            break;
        case OP_MRESET: {                      /* v1.20: memReset manual */
            int64_t mark = stackv[--sp];
            if (mark < 0) mark = 0;
            if (mark > heap_used) mark = heap_used;
            heap_used = (long)mark;
            break;
        }
