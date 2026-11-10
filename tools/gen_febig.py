#!/usr/bin/env python3
"""febig: uji field medan penuh — nilai acak dari python (ground truth),
program Kopi men-generate helper byte + pengujian, output 32 byte dibandingkan."""
import random

P = (1 << 255) - 19
random.seed(7)

def kop_arr(name, b):
    body = "    static int[] %s() {\n        int[] v = new int[32];\n" % name
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body

def kop_bytes(name, v):     # 32 byte dari int ground truth (< 2^256)
    return kop_arr(name, list(v.to_bytes(32, 'little')))

main = ["    public static void main(String[] args) {"]
helpers = []
refs = []

for k in range(4):
    a = random.randrange(P)
    b = random.randrange(P)
    c = random.randrange(P)
    an, bn, cn = "A%d" % k, "B%d" % k, "C%d" % k
    helpers.append(kop_bytes(an, a))
    helpers.append(kop_bytes(bn, b))
    helpers.append(kop_bytes(cn, c))
    fa, fb, fc = "a%d" % k, "b%d" % k, "c%d" % k
    main.append("        int[] %s = Big.fromBytes(%s()); int[] %s = Big.fromBytes(%s()); int[] %s = Big.fromBytes(%s());"
                % (fa, an, fb, bn, fc, cn))
    t = k * 10
    main.append("        int[] r%d = Big.fesub(%s, %s); show(r%d);" % (t, fa, fb, t))
    refs.append((a - b) % P)
    main.append("        int[] r%d = Big.feadd(%s, %s); show(r%d);" % (t + 1, fa, fb, t + 1))
    refs.append((a + b) % P)
    main.append("        int[] r%d = Big.femul(%s, %s); show(r%d);" % (t + 2, fa, fb, t + 2))
    refs.append((a * b) % P)
    main.append("        int[] r%d = Big.femul(Big.fesub(%s, %s), %s); show(r%d);" % (t + 3, fa, fb, fc, t + 3))
    refs.append(((a - b) * c) % P)
    main.append("        int[] r%d = Big.feInv(%s); int[] r%d = Big.femul(r%d, %s); show(r%d);" % (t + 5, fa, t + 6, t + 5, fa, t + 6))
    refs.append(1)

main.append("    }")

show = """    static void show(int[] f) {
        int[] b = Big.toBytes(f);
        for (int i = 0; i < 32; i++) {
            if (i > 0) System.out.print(" ");
            System.out.print(b[i]);
        }
        System.out.println("");
    }"""

src = "\n".join(["import Big;", "", "class Main {"] + main + [show] + helpers + ["}", ""]) + "\n"
open('/data/data/com.termux/files/usr/tmp/kopi/febig.kop', 'w').write(src)

ref = "\n".join(" ".join(str(x) for x in v.to_bytes(32, 'little')) for v in refs) + "\n"
open('/data/data/com.termux/files/usr/tmp/kopi/febig.ref', 'w').write(ref)
print("kasus:", len(refs))
