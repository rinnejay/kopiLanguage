/* gen_x64_rt.inc — runtime assembly x64 + data (bagian dari gen_x64.c). */

static void gen_x64_runtime(void) {
    emit(".text");
    emit("\t.globl kop_str_from_cstr");
    emit("kop_str_from_cstr:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tmovq %%rdi, %%r10");
    emit("\txorl %%ecx, %%ecx");
    emit(".LSCXa:");
    emit("\tcmpb $0, (%%r10,%%rcx)");
    emit("\tje .LSCXb");
    emit("\tincq %%rcx");
    emit("\tjmp .LSCXa");
    emit(".LSCXb:");
    /* v1.18: String packed — 8 byte per slot little-endian */
    emit("\tpushq %%r10");
    emit("\tpushq %%rcx");
    emit("\tleaq 7(%%rcx), %%rdi");
    emit("\tshrq $3, %%rdi");
    emit("\tshlq $3, %%rdi");
    emit("\taddq $8, %%rdi");
    emit("\tcall kopalloc");
    emit("\tpopq %%rcx");
    emit("\tpopq %%r10");
    emit("\tmovq %%rcx, %%r9");                  /* n */
    emit("\tmovq %%rax, %%r11");                 /* dst */
    emit("\tleaq 7(%%r9), %%rdx");
    emit("\tshrq $3, %%rdx");                    /* slots */
    emit("\txorl %%r8d, %%r8d");
    emit(".LSCXz:");
    emit("\tcmpq %%rdx, %%r8");
    emit("\tjge .LSCXw");
    emit("\tmovq $0, 8(%%r11,%%r8,8)");          /* zero slot data (v1.18 fix) */
    emit("\tincq %%r8");
    emit("\tjmp .LSCXz");
    emit(".LSCXw:");
    emit("\tmovq %%r9, (%%r11)");                /* len */
    emit("\txorl %%r8d, %%r8d");                 /* i */
    emit(".LSCXc:");
    emit("\tcmpq %%r9, %%r8");
    emit("\tjge .LSCXd");
    emit("\tmovzbl (%%r10,%%r8), %%esi");
    emit("\tmovq %%r8, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* shift */
    emit("\tshlq %%cl, %%rsi");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");                    /* slot offset */
    emit("\torq %%rsi, (%%r11,%%rdx)");
    emit("\tincq %%r8");
    emit("\tjmp .LSCXc");
    emit(".LSCXd:");
    emit("\tmovq %%r11, %%rax");
    emit("\tpopq %%rbp");
    emit("\tret");

    /* ── v1.18: runtime String packed ───────────────────────────── */

    /* rdi = int[] byte-per-slot → rax = String packed */
    emit(".text");
    emit("\t.globl kop_mkstr");
    emit("kop_mkstr:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tpushq %%rdi");
    emit("\tmovslq (%%rdi), %%r9");              /* n */
    emit("\tpushq %%r9");
    emit("\tleaq 7(%%r9), %%rdi");
    emit("\tshrq $3, %%rdi");
    emit("\tshlq $3, %%rdi");
    emit("\taddq $8, %%rdi");
    emit("\tcall kopalloc");
    emit("\tpopq %%r9");
    emit("\tpopq %%r10");
    emit("\tmovq %%rax, %%r11");                 /* dst */
    emit("\tleaq 7(%%r9), %%rdx");
    emit("\tshrq $3, %%rdx");                    /* slots */
    emit("\txorl %%r8d, %%r8d");
    emit(".LMKXz:");
    emit("\tcmpq %%rdx, %%r8");
    emit("\tjge .LMKXw");
    emit("\tmovq $0, 8(%%r11,%%r8,8)");          /* zero slot data (v1.18 fix) */
    emit("\tincq %%r8");
    emit("\tjmp .LMKXz");
    emit(".LMKXw:");
    emit("\tmovq %%r9, (%%r11)");                /* len */
    emit("\txorl %%r8d, %%r8d");                 /* i */
    emit(".LMKXl:");
    emit("\tcmpq %%r9, %%r8");
    emit("\tjge .LMKXd");
    emit("\tmovq 8(%%r10,%%r8,8), %%rsi");       /* byte */
    emit("\tcmpq $255, %%rsi");
    emit("\tja .Kbnd");
    emit("\tmovq %%r8, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* shift */
    emit("\tshlq %%cl, %%rsi");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");                    /* slot offset */
    emit("\torq %%rsi, (%%r11,%%rdx)");
    emit("\tincq %%r8");
    emit("\tjmp .LMKXl");
    emit(".LMKXd:");
    emit("\tmovq %%r11, %%rax");
    emit("\tpopq %%rbp");
    emit("\tret");

    /* rdi = String packed → rax = int[] byte-per-slot */
    emit(".text");
    emit("\t.globl kop_strbytes");
    emit("kop_strbytes:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tpushq %%rdi");
    emit("\tmovslq (%%rdi), %%r9");              /* n */
    emit("\tpushq %%r9");
    emit("\tmovq %%r9, %%rdi");
    emit("\tshlq $3, %%rdi");
    emit("\taddq $8, %%rdi");
    emit("\tcall kopalloc");
    emit("\tpopq %%r9");
    emit("\tpopq %%r10");
    emit("\tmovq %%r9, (%%rax)");                /* len */
    emit("\tmovq %%rax, %%r11");                 /* dst */
    emit("\txorl %%r8d, %%r8d");                 /* i */
    emit(".LSBXl:");
    emit("\tcmpq %%r9, %%r8");
    emit("\tjge .LSBXd");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");                    /* src slot offset */
    emit("\tmovq (%%r10,%%rdx), %%rsi");
    emit("\tmovq %%r8, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* shift */
    emit("\tshrq %%cl, %%rsi");
    emit("\tandq $255, %%rsi");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");                    /* dst offset */
    emit("\tmovq %%rsi, (%%r11,%%rdx)");
    emit("\tincq %%r8");
    emit("\tjmp .LSBXl");
    emit(".LSBXd:");
    emit("\tmovq %%r11, %%rax");
    emit("\tpopq %%rbp");
    emit("\tret");

    /* rdi, rsi = String packed → rax = concat packed */
    emit(".text");
    emit("\t.globl kop_cat_packed");
    emit("kop_cat_packed:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tpushq %%r12");
    emit("\tpushq %%r13");
    emit("\tpushq %%r14");
    emit("\tpushq %%r15");
    emit("\tmovq %%rdi, %%r10");                 /* a */
    emit("\tmovq %%rsi, %%r11");                 /* b */
    emit("\tmovslq (%%r10), %%r12");             /* la */
    emit("\tmovslq (%%r11), %%r13");             /* lb */
    emit("\tmovq %%r12, %%r14");
    emit("\taddq %%r13, %%r14");                 /* total */
    emit("\tmovq %%r14, %%r15");
    emit("\taddq $7, %%r15");
    emit("\tshrq $3, %%r15");                    /* slots */
    emit("\tpushq %%r10");
    emit("\tpushq %%r11");
    emit("\tpushq %%r14");
    emit("\tpushq %%r15");
    emit("\tmovq %%r15, %%rdi");
    emit("\tshlq $3, %%rdi");
    emit("\taddq $8, %%rdi");
    emit("\tcall kopalloc");
    emit("\tpopq %%r15");
    emit("\tpopq %%r14");
    emit("\tpopq %%r11");
    emit("\tpopq %%r10");
    emit("\tmovq %%rax, %%r9");                  /* dst */
    emit("\tmovq %%r15, %%rcx");                 /* slots */
    emit("\txorl %%r8d, %%r8d");
    emit(".LCXz:");
    emit("\tcmpq %%rcx, %%r8");
    emit("\tjge .LCXw");
    emit("\tmovq $0, 8(%%r9,%%r8,8)");           /* zero slot data (v1.18 fix) */
    emit("\tincq %%r8");
    emit("\tjmp .LCXz");
    emit(".LCXw:");
    emit("\tmovq %%r14, (%%r9)");                /* len = total */
    emit("\txorl %%r8d, %%r8d");                 /* i (dari a) */
    emit(".LCXa:");
    emit("\tcmpq %%r12, %%r8");
    emit("\tjge .LCXb");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");
    emit("\tmovq (%%r10,%%rdx), %%rsi");         /* src slot */
    emit("\tmovq %%r8, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* src shift */
    emit("\tshrq %%cl, %%rsi");
    emit("\tandq $255, %%rsi");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");                    /* dst slot offset */
    emit("\tshlq %%cl, %%rsi");
    emit("\torq %%rsi, (%%r9,%%rdx)");
    emit("\tincq %%r8");
    emit("\tjmp .LCXa");
    emit(".LCXb:");
    emit("\txorl %%r8d, %%r8d");                 /* j (dari b) */
    emit(".LCXc:");
    emit("\tcmpq %%r13, %%r8");
    emit("\tjge .LCXd");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");
    emit("\tmovq (%%r11,%%rdx), %%rsi");         /* src slot */
    emit("\tmovq %%r8, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* src shift */
    emit("\tshrq %%cl, %%rsi");
    emit("\tandq $255, %%rsi");
    emit("\tmovq %%r8, %%rdx");
    emit("\taddq %%r12, %%rdx");                 /* pos = la + j */
    emit("\tmovq %%rdx, %%r15");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");                    /* dst slot offset */
    emit("\tmovq %%r15, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* dst shift */
    emit("\tshlq %%cl, %%rsi");
    emit("\torq %%rsi, (%%r9,%%rdx)");
    emit("\tincq %%r8");
    emit("\tjmp .LCXc");
    emit(".LCXd:");
    emit("\tmovq %%r9, %%rax");
    emit("\tpopq %%r15");
    emit("\tpopq %%r14");
    emit("\tpopq %%r13");
    emit("\tpopq %%r12");
    emit("\tpopq %%rbp");
    emit("\tret");

    /* rdi, rsi = String packed → eax = isi sama (1/0) */
    emit(".text");
    emit("\t.globl kop_sequal_packed");
    emit("kop_sequal_packed:");
    emit("\tpushq %%rbp");
    emit("\tmovq %%rsp, %%rbp");
    emit("\tpushq %%r12");
    emit("\tpushq %%r13");
    emit("\tmovq %%rdi, %%r10");                 /* a */
    emit("\tmovq %%rsi, %%r11");                 /* b */
    emit("\tmovslq (%%r10), %%r12");             /* la */
    emit("\tmovslq (%%r11), %%r13");             /* lb */
    emit("\tcmpq %%r13, %%r12");
    emit("\tjne .LSQXn");
    emit("\txorl %%r8d, %%r8d");                 /* i */
    emit(".LSQXl:");
    emit("\tcmpq %%r12, %%r8");
    emit("\tjge .LSQXy");
    emit("\tmovq %%r8, %%rdx");
    emit("\tshrq $3, %%rdx");
    emit("\tshlq $3, %%rdx");
    emit("\taddq $8, %%rdx");
    emit("\tmovq (%%r10,%%rdx), %%rax");         /* slot a */
    emit("\tmovq %%r8, %%rcx");
    emit("\tandq $7, %%rcx");
    emit("\tshlq $3, %%rcx");                    /* shift */
    emit("\tshrq %%cl, %%rax");
    emit("\tandq $255, %%rax");
    emit("\tmovq (%%r11,%%rdx), %%rsi");         /* slot b */
    emit("\tshrq %%cl, %%rsi");
    emit("\tandq $255, %%rsi");
    emit("\tcmpq %%rsi, %%rax");
    emit("\tjne .LSQXn");
    emit("\tincq %%r8");
    emit("\tjmp .LSQXl");
    emit(".LSQXy:");
    emit("\tmovl $1, %%eax");
    emit("\tjmp .LSQXd");
    emit(".LSQXn:");
    emit("\txorl %%eax, %%eax");
    emit(".LSQXd:");
    emit("\tpopq %%r13");
    emit("\tpopq %%r12");
    emit("\tpopq %%rbp");
    emit("\tret");

}
