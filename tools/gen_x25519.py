#!/usr/bin/env python3
"""gen_x25519: tulis test/x25519.kop berisi vektor RFC 7748 (§5.2 + §6.1)."""
import sys, os.path

SC1 = "a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4"
U1  = "e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c"
O1  = "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552"
SC2 = "4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d"
U2  = "e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493"
O2  = "95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957"
SKA = "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"
PKA = "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"
SKB = "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"
PKB = "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"
SHK = "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742"


def hlp(name, hx):
    body = "    static int[] %s() {\n        int[] v = new int[32];\n" % name
    for i in range(16):
        b = int(hx[i*4:i*4+2], 16)
        body += "        v[%d] = %d;\n" % (2*i, b)
        body += "        v[%d] = %d;\n" % (2*i + 1, int(hx[i*4+2:i*4+4], 16))
    body += "        return v;\n    }"
    return body


def arr_eq(name_m, name_v):
    return ("static int eq%s(int[] x) {\n"
            "        int[] v = %s();\n"
            "        for (int i = 0; i < 32; i++) if (x[i] != v[i]) return 0;\n"
            "        return 1;\n    }") % (name_m, name_v)


main = []
ops = []
for name, hx in [("K1", SC1), ("U1", U1), ("W1", O1), ("K2", SC2), ("U2", U2), ("W2", O2),
                 ("SA", SKA), ("PA", PKA), ("SB", SKB), ("PB", PKB), ("TK", SHK)]:
    ops.append(hlp(name, hx))

for name in ["W1", "W2", "PA", "PB", "TK"]:
    ops.append(arr_eq("_" + name, name))

main.append("    public static void main(String[] args) {")
main.append("        int[] o1 = X25519.x25519(K1(), U1());")
main.append("        System.out.println(eq_W1(o1));")
main.append("        int[] o2 = X25519.x25519(K2(), U2());")
main.append("        System.out.println(eq_W2(o2));")
main.append("        int[] pa = X25519.pub(SA());")
main.append("        System.out.println(eq_PA(pa));")
main.append("        int[] pb = X25519.pub(SB());")
main.append("        System.out.println(eq_PB(pb));")
main.append("        int[] kab = X25519.x25519(SA(), PB());")
main.append("        System.out.println(eq_TK(kab));")
main.append("        int[] kba = X25519.x25519(SB(), PA());")
main.append("        System.out.println(eq_TK(kba));")
main.append("    }")

src = "\n".join(["import X25519;", "", "class Main {"] + main + ops + ["}", ""]) + "\n"
path = sys.argv[1] if len(sys.argv) > 1 else "x25519.kop"
open(path, "w").write(src)
print("wrote", path)
