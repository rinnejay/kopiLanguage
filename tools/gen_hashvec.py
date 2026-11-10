#!/usr/bin/env python3
"""gen_hashvec: menghasilkan test/vector/hash.kop dengan vektor python hash."""
import hashlib, hmac, os, sys

keys = {
    "dig1":   hashlib.sha256(b"").digest(),
    "dig2":   hashlib.sha256(b"abc").digest(),
    "dig3":   hashlib.sha256(b"x" * 112).digest(),
    "dig4":   hmac.new(b"\x0b" * 20, b"Halo dunia Kopi", hashlib.sha256).digest(),
    "dig5":   hmac.new(b"ini kunci yang jauh melebihi enam puluh empat byte jadi HMAC musti hash dulu-DONE!", b"", hashlib.sha256).digest(),
}

def dec(name, b):
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(b)
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body

helpers = []
for name, b in keys.items():
    helpers.append(dec(name, b))

helpers.append('''    static int[] b0b20() {
        int[] v = new int[20];
        for (int i = 0; i < 20; i++) v[i] = 11;
        return v;
    }

    static int[] sh112() {
        int[] v = new int[112];
        for (int i = 0; i < 112; i++) v[i] = 120;
        return v;
    }

    static int[] kunciPanjang() {
        int[] v = Net.bytes("ini kunci yang jauh melebihi enam puluh empat byte jadi HMAC musti hash dulu");
        int[] out = new int[v.length + 6];
        for (int i = 0; i < v.length; i++) out[i] = v[i];
        out[v.length] = 45; out[v.length + 1] = 68; out[v.length + 2] = 79;
        out[v.length + 3] = 78; out[v.length + 4] = 69; out[v.length + 5] = 33;
        return out;
    }''')

main = "    public static void main(String[] args) {\n"
main += "        System.out.println(Hash.eq(Hash.sha256(Net.bytes(\"\")), dig1()));\n"
main += "        System.out.println(Hash.eq(Hash.sha256(Net.bytes(\"abc\")), dig2()));\n"
main += "        System.out.println(Hash.eq(Hash.sha256(sh112()), dig3()));\n"
main += "        System.out.println(Hash.eq(Hash.hmac(b0b20(), Net.bytes(\"Halo dunia Kopi\")), dig4()));\n"
main += "        System.out.println(Hash.eq(Hash.hmac(kunciPanjang(), Net.bytes(\"\")), dig5()));\n"
main += "    }"

src = "import Hash;\n\nclass Main {\n" + main + "\n" + "\n".join(helpers) + "\n}\n"
path = sys.argv[1] if len(sys.argv) > 1 else "hashvec.kop"
open(path, "w").write(src)
print("wrote", path, len(src))
