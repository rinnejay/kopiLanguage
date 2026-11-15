#!/usr/bin/env python3
"""fuzz_diff.py — fuzzer diferensial: backend a64 vs VM.

Menghasilkan program Kopi acak dari gramatika terbatas yang didukung
kedua backend, lalu membandingkan stdout + kode keluar. Perbedaan berarti
codegen a64 dan VM tidak sepakat (bug di salah satu backend, atau di
frontend yang memengaruhi keduanya secara berbeda).

Pemakaian:
    python3 tools/fuzz_diff.py                 # 20 program, seed 1..20
    python3 tools/fuzz_diff.py -n 50 -s 100    # 50 program mulai seed 100
    python3 tools/fuzz_diff.py -k 7            # hanya seed 7 (debug)

Kegagalan menyimpan program tersangka ke $TMPDIR/kopi-fuzz/fail-<seed>.kop
beserta kedua keluaran, lalu keluar dengan status 1.

Batas bahasa yang dipakai sengaja konservatif: int 32-bit, boolean, array
int, loop kanonik, if/else, pemanggilan metode static, dan println. Tidak
ada pembagian dengan nol / indeks di luar batas / shift > 31, jadi setiap
program deterministik dan tidak boleh crash.
"""

import argparse
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KOPICC = os.path.join(ROOT, "kopicc")
KOPIVM = os.path.join(ROOT, "kopivm")
TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "kopi-fuzz")


class Gen:
    def __init__(self, rng):
        self.r = rng
        self.nvar = 0
        self.narr = 0
        self.nfun = 0

    def fresh(self, prefix):
        self.nvar += 1
        return "%s%d" % (prefix, self.nvar)

    def lit(self):
        return str(self.r.randint(-500, 500))

    def int_expr(self, vars_, arrs, funs, depth=0):
        """Ekspresi int; vars_ = daftar variabel int, arrs = (nama, panjang)."""
        r = self.r
        choices = ["lit", "lit"]
        if vars_:
            choices += ["var", "var", "var"]
        if depth < 3:
            choices += ["un", "bin", "bin", "bin", "tern", "cmpcast"]
            if arrs:
                choices += ["idx"]
            if funs:
                choices += ["call"]
        k = r.choice(choices)

        if k == "lit":
            return self.lit()
        if k == "var":
            return r.choice(vars_) if vars_ else self.lit()
        if k == "un":
            op = r.choice(["-", "~"])
            return "(%s(%s))" % (op, self.int_expr(vars_, arrs, funs, depth + 1))
        if k == "bin":
            op = r.choice(["+", "-", "*", "&", "|", "^", "<<", ">>", ">>>",
                           "/", "%"])
            a = self.int_expr(vars_, arrs, funs, depth + 1)
            b = self.int_expr(vars_, arrs, funs, depth + 1)
            if op in ("/", "%"):
                # pembagi positif 1..8: tidak ada bagi nol, tidak ada INT_MIN/-1
                b = "((%s & 7) + 1)" % b
            elif op in ("<<", ">>", ">>>"):
                b = "((%s) & 31)" % b
            return "(%s %s %s)" % (a, op, b)
        if k == "tern":
            c = self.bool_expr(vars_, arrs, funs, depth + 1)
            return "(%s ? %s : %s)" % (
                c,
                self.int_expr(vars_, arrs, funs, depth + 1),
                self.int_expr(vars_, arrs, funs, depth + 1))
        if k == "cmpcast":
            c = self.bool_expr(vars_, arrs, funs, depth + 1)
            return "(%s ? 1 : 0)" % c
        if k == "idx":
            name, n = r.choice(arrs)
            # indeks selalu di dalam batas
            return "%s[(%s & %d)]" % (name,
                                      self.int_expr(vars_, arrs, funs, depth + 1),
                                      n - 1 if n > 1 else 0)
        if k == "call":
            f = r.choice(funs)
            args = ", ".join(self.int_expr(vars_, arrs, funs, depth + 1)
                             for _ in range(f[1]))
            return "%s(%s)" % (f[0], args)
        return self.lit()

    def bool_expr(self, vars_, arrs, funs, depth=0):
        r = self.r
        if depth >= 3:
            k = r.choice(["lit", "lit", "cmp"])
        else:
            k = r.choice(["lit", "lit", "cmp", "cmp", "not", "and", "or"])
        if k == "lit":
            return r.choice(["true", "false"])
        if k == "cmp":
            op = r.choice(["<", ">", "<=", ">=", "==", "!="])
            return "(%s %s %s)" % (self.int_expr(vars_, arrs, funs, depth + 1),
                                   op,
                                   self.int_expr(vars_, arrs, funs, depth + 1))
        if k == "not":
            return "(!%s)" % self.bool_expr(vars_, arrs, funs, depth + 1)
        if k == "and":
            return "(%s && %s)" % (self.bool_expr(vars_, arrs, funs, depth + 1),
                                   self.bool_expr(vars_, arrs, funs, depth + 1))
        return "(%s || %s)" % (self.bool_expr(vars_, arrs, funs, depth + 1),
                               self.bool_expr(vars_, arrs, funs, depth + 1))

    def helper(self):
        """Metode static: (nama, jumlah_param) beserta badannya."""
        name = "f%d" % self.nfun
        self.nfun += 1
        npar = self.r.randint(1, 3)
        params = ["p%d" % i for i in range(npar)]
        pvars = list(params)
        body = []
        for _ in range(self.r.randint(1, 4)):
            v = self.fresh("t")
            body.append("        int %s = %s;" %
                        (v, self.int_expr(pvars, [], [])))
            pvars.append(v)
        for _ in range(self.r.randint(0, 2)):
            if self.r.random() < 0.5:
                c = self.bool_expr(pvars, [], [])
                body.append("        if (%s) { %s = %s; }" %
                            (c, self.r.choice(pvars),
                             self.int_expr(pvars, [], [])))
        body.append("        return %s;" % self.int_expr(pvars, [], []))
        src = "    static int %s(%s) {\n%s\n    }\n" % (
            name, ", ".join("int " + p for p in params), "\n".join(body))
        return (name, npar), src

    def stmts(self, vars_, arrs, funs, indent, depth, budget):
        r = self.r
        out = []
        pad = " " * indent
        for _ in range(budget):
            k = r.choice(["decl", "decl", "assign", "print", "print", "if",
                          "loop", "arr", "compound", "callprint"])
            if k == "decl":
                v = self.fresh("v")
                out.append("%sint %s = %s;" %
                           (pad, v, self.int_expr(vars_, arrs, funs)))
                vars_.append(v)
            elif k == "assign" and vars_:
                out.append("%s%s = %s;" %
                           (pad, r.choice(vars_),
                            self.int_expr(vars_, arrs, funs)))
            elif k == "compound" and vars_:
                op = r.choice(["+=", "-=", "*=", "&=", "|=", "^="])
                out.append("%s%s %s %s;" %
                           (pad, r.choice(vars_), op,
                            self.int_expr(vars_, arrs, funs)))
            elif k == "print":
                if r.random() < 0.3:
                    out.append("%sSystem.out.println(%s);" %
                               (pad, self.bool_expr(vars_, arrs, funs)))
                else:
                    out.append("%sSystem.out.println(%s);" %
                               (pad, self.int_expr(vars_, arrs, funs)))
            elif k == "callprint" and funs:
                out.append("%sSystem.out.println(%s);" %
                           (pad, self.int_expr(vars_, arrs, funs)))
            elif k == "if" and depth < 3:
                c = self.bool_expr(vars_, arrs, funs)
                sub = self.stmts(list(vars_), list(arrs), funs,
                                 indent + 4, depth + 1, r.randint(1, 3))
                out.append("%sif (%s) {" % (pad, c))
                out.extend(sub)
                if r.random() < 0.5:
                    sub2 = self.stmts(list(vars_), list(arrs), funs,
                                      indent + 4, depth + 1, r.randint(1, 2))
                    out.append("%s} else {" % pad)
                    out.extend(sub2)
                out.append("%s}" % pad)
            elif k == "loop" and depth < 3:
                i = self.fresh("i")
                n = r.randint(1, 6)
                out.append("%sfor (int %s = 0; %s < %d; %s++) {" %
                           (pad, i, i, n, i))
                # variabel induksi tidak boleh ditugasi di badan loop:
                # `i |= -108` membuat loop tak pernah selesai
                inner_vars = list(vars_)
                # list(arrs) menyalin: array yang dideklarasikan di badan
                # loop tidak boleh bocor ke scope luar
                sub = self.stmts(inner_vars, list(arrs), funs,
                                 indent + 4, depth + 1, r.randint(1, 3))
                out.extend(sub)
                if r.random() < 0.4:
                    out.append("%s    if (%s == %d) { continue; }" %
                               (pad, i, r.randint(0, n - 1)))
                out.append("%s}" % pad)
            elif k == "arr" and depth < 3:
                name = self.fresh("a")
                n = r.randint(2, 8)
                out.append("%sint[] %s = new int[%d];" % (pad, name, n))
                for j in range(r.randint(1, min(n, 4))):
                    out.append("%s%s[%d] = %s;" %
                               (pad, name, j,
                                self.int_expr(vars_, arrs, funs)))
                arrs.append((name, n))
                if r.random() < 0.7:
                    out.append("%sSystem.out.println(%s.length);" % (pad, name))
                    idx = r.randint(0, n - 1)
                    out.append("%sSystem.out.println(%s[%d]);" %
                               (pad, name, idx))
        return out

    def program(self):
        r = self.r
        funs = []
        helpers = []
        for _ in range(r.randint(0, 3)):
            f, src = self.helper()
            funs.append(f)
            helpers.append(src)
        vars_ = []
        for _ in range(r.randint(1, 4)):
            v = self.fresh("v")
            vars_.append(v)
        main = []
        for v in vars_:
            main.append("        int %s = %s;" %
                        (v, self.int_expr([], [], funs)))
        main.extend(self.stmts(vars_, [], funs, 8, 0, r.randint(4, 10)))
        return ("class Main {\n" + "".join(helpers) +
                "    public static void main(String[] args) {\n" +
                "\n".join(main) +
                "\n    }\n}\n")


def run(cmd, cwd=None, timeout=60):
    try:
        return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired:
        class R:
            returncode = 124
            stdout = b""
            stderr = ("timeout: %s" % " ".join(cmd)).encode()
        return R()


def one(seed, keep):
    rng = random.Random(seed)
    src = Gen(rng).program()
    os.makedirs(TMP, exist_ok=True)
    kop = os.path.join(TMP, "case-%d.kop" % seed)
    with open(kop, "w") as f:
        f.write(src)

    vm_bc = os.path.join(TMP, "case-%d.kbc" % seed)
    a64_s = os.path.join(TMP, "case-%d.s" % seed)
    a64_bin = os.path.join(TMP, "case-%d.bin" % seed)

    p = run([KOPICC, kop, "-t", "vm", "-o", vm_bc])
    if p.returncode != 0:
        return ("compile-vm", kop, p.stderr.decode("utf-8", "replace"), "")
    p = run([KOPIVM, vm_bc])
    if p.returncode not in (0,):
        return ("run-vm", kop, p.stdout.decode("utf-8", "replace"),
                p.stderr.decode("utf-8", "replace"))
    vm_out, vm_rc = p.stdout, p.returncode

    p = run([KOPICC, kop, "-t", "a64", "-o", a64_s])
    if p.returncode != 0:
        return ("compile-a64", kop, p.stderr.decode("utf-8", "replace"), "")
    p = run(["cc", a64_s, "-o", a64_bin])
    if p.returncode != 0:
        return ("cc-a64", kop, p.stderr.decode("utf-8", "replace"), "")
    p = run([a64_bin])
    a64_out, a64_rc = p.stdout, p.returncode

    if vm_out != a64_out or vm_rc != a64_rc:
        return ("mismatch", kop, vm_out.decode("utf-8", "replace"),
                a64_out.decode("utf-8", "replace"))
    if not keep:
        for path in (kop, vm_bc, a64_s, a64_bin):
            try:
                os.remove(path)
            except OSError:
                pass
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-n", "--count", type=int, default=20)
    ap.add_argument("-s", "--start", type=int, default=1)
    ap.add_argument("-k", "--seed", type=int, default=None)
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    seeds = [args.seed] if args.seed is not None else \
        list(range(args.start, args.start + args.count))

    bad = 0
    for seed in seeds:
        res = one(seed, args.keep)
        if res:
            bad += 1
            kind, kop, a, b = res
            print("FAIL seed=%d (%s): %s" % (seed, kind, kop))
            if kind == "mismatch":
                import difflib
                diff = difflib.unified_diff(
                    a.splitlines(True), b.splitlines(True),
                    fromfile="vm", tofile="a64")
                sys.stdout.writelines(list(diff)[:60])
            elif a:
                print(a[:2000])
            if b:
                print(b[:2000])
            if bad >= 3:
                print("berhenti setelah 3 kegagalan")
                break
        else:
            print("ok   seed=%d" % seed)
    if bad:
        print("fuzz: %d/%d program gagal" % (bad, len(seeds)))
        return 1
    print("fuzz: %d program OK" % len(seeds))
    return 0


if __name__ == "__main__":
    sys.exit(main())
