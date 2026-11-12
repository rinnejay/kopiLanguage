/* vm_ops_fast.inc — potongan switch run() di kopivm.c. */

        /* ── v1.22: superinstruksi loop panas ─────────────────────── */
        case OP_CMP_JMPT:
        case OP_CMP_JMPF: {
            const uint8_t *c = m->code + f->pc;
            int type = c[0];
            int cm = c[1];
            uint16_t sl = (uint16_t)(c[2] | (c[3] << 8));
            int32_t imm = (int32_t)rd32c(c + 4);
            uint32_t t = rd32c(c + 8);
            f->pc += 12;
            if ((int)sl >= MAXLOC) dievm("slot lokal luar batas");
            int64_t v = localsv[nframes - 1][sl];
            int cond = type ? cmp_i64(v, imm, cm)
                            : cmp_i32((int32_t)v, imm, cm);
            if (op == OP_CMP_JMPF) cond = !cond;
            if (cond) {
                if (t > m->clen) dievm("target lompatan luar batas");
                f->pc = (long)t;
            }
            break;
        }
        case OP_LOOP: {
            const uint8_t *c = m->code + f->pc;
            int type = c[0];
            int cm = c[1];
            uint16_t sl = (uint16_t)(c[2] | (c[3] << 8));
            int32_t delta = (int32_t)rd32c(c + 4);
            int32_t imm = (int32_t)rd32c(c + 8);
            uint32_t t = rd32c(c + 12);
            f->pc += 16;
            if ((int)sl >= MAXLOC) dievm("slot lokal luar batas");
            int64_t v = localsv[nframes - 1][sl] + delta;
            if (!type) v = (int32_t)v;
            localsv[nframes - 1][sl] = v;
            int cond = type ? cmp_i64(v, imm, cm)
                            : cmp_i32((int32_t)v, imm, cm);
            if (cond) {
                if (t > m->clen) dievm("target lompatan luar batas");
                f->pc = (long)t;
            }
            break;
        }
        case OP_IINC: {
            const uint8_t *c = m->code + f->pc;
            int type = c[0];
            uint16_t sl = (uint16_t)(c[2] | (c[3] << 8));
            int32_t delta = (int32_t)rd32c(c + 3);
            f->pc += 7;
            if ((int)sl >= MAXLOC) dievm("slot lokal luar batas");
            int64_t v = localsv[nframes - 1][sl] + delta;
            localsv[nframes - 1][sl] = type ? v : (int32_t)v;
            break;
        }
        case OP_BIN_L: {
            const uint8_t *c = m->code + f->pc;
            int type = c[0];
            int bo = c[1];
            uint16_t sa = (uint16_t)(c[2] | (c[3] << 8));
            uint16_t sb = (uint16_t)(c[4] | (c[5] << 8));
            f->pc += 6;
            if ((int)sa >= MAXLOC || (int)sb >= MAXLOC)
                dievm("slot lokal luar batas");
            int64_t x = localsv[nframes - 1][sa];
            int64_t y = localsv[nframes - 1][sb];
            if (type) {
                switch (bo) {
                case 0: x += y; break;
                case 1: x -= y; break;
                case 2: x *= y; break;
                case 3: x &= y; break;
                case 4: x |= y; break;
                default: x ^= y; break;
                }
            } else {
                x = arith_i32(x, y, bo);
            }
            stackv[sp++] = x;
            break;
        }
        case OP_OP_STORE: {
            const uint8_t *c = m->code + f->pc;
            int type = c[0];
            int bo = c[1];
            uint16_t sl = (uint16_t)(c[2] | (c[3] << 8));
            f->pc += 4;
            if ((int)sl >= MAXLOC) dievm("slot lokal luar batas");
            int64_t y = stackv[--sp];
            int64_t x = localsv[nframes - 1][sl];
            if (type) {
                switch (bo) {
                case 0: x += y; break;
                case 1: x -= y; break;
                case 2: x *= y; break;
                case 3: x &= y; break;
                case 4: x |= y; break;
                default: x ^= y; break;
                }
            } else {
                x = arith_i32(x, y, bo);
            }
            localsv[nframes - 1][sl] = x;
            break;
        }
        case OP_IDXR_L: {
            const uint8_t *c = m->code + f->pc;
            uint16_t sa = (uint16_t)(c[0] | (c[1] << 8));
            uint16_t sb = (uint16_t)(c[2] | (c[3] << 8));
            f->pc += 4;
            if ((int)sa >= MAXLOC || (int)sb >= MAXLOC)
                dievm("slot lokal luar batas");
            long *blk = (long *)localsv[nframes - 1][sa];
            int64_t idx = localsv[nframes - 1][sb];
            if (!blk || !in_heap(blk))
                dievm("array tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks array di luar batas", (long)idx);
            stackv[sp++] = blk[1 + idx];
            break;
        }
        case OP_IDXW_L: {
            const uint8_t *c = m->code + f->pc;
            uint16_t sa = (uint16_t)(c[0] | (c[1] << 8));
            uint16_t sb = (uint16_t)(c[2] | (c[3] << 8));
            f->pc += 4;
            if ((int)sa >= MAXLOC || (int)sb >= MAXLOC)
                dievm("slot lokal luar batas");
            int64_t val = stackv[--sp];
            long *blk = (long *)localsv[nframes - 1][sa];
            int64_t idx = localsv[nframes - 1][sb];
            if (!blk || !in_heap(blk))
                dievm("array tidak valid (di luar arena)");
            if (idx < 0 || blk[0] <= idx)
                dievm2("indeks array di luar batas", (long)idx);
            blk[1 + idx] = val;
            break;
        }
