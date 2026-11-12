#!/usr/bin/env python3
"""gen_hashvec: menghasilkan test/vector/hash.kop dengan vektor python hash.

Cakupan: SHA-256, HMAC-SHA256, SHA-512, SHA-384, HMAC-SHA512 (RFC 4231).
"""
import hashlib, hmac, os, sys

keys = {
    "dig1":   hashlib.sha256(b"").digest(),
    "dig2":   hashlib.sha256(b"abc").digest(),
    "dig3":   hashlib.sha256(b"x" * 112).digest(),
    "dig4":   hmac.new(b"\x0b" * 20, b"Halo dunia Kopi", hashlib.sha256).digest(),
    "dig5":   hmac.new(b"ini kunci yang jauh melebihi enam puluh empat byte jadi HMAC musti hash dulu-DONE!", b"", hashlib.sha256).digest(),
    "s512e":  hashlib.sha512(b"").digest(),
    "s512a":  hashlib.sha512(b"abc").digest(),
    "s512l":  hashlib.sha512(b"x" * 200).digest(),
    "s384e":  hashlib.sha384(b"").digest(),
    "s384a":  hashlib.sha384(b"abc").digest(),
    "s384l":  hashlib.sha384(b"x" * 200).digest(),
    "h512a":  hmac.new(b"key", b"abc", hashlib.sha512).digest(),
    "h512l":  hmac.new(b"k" * 200, b"abc", hashlib.sha512).digest(),
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

    static int[] sh200() {
        int[] v = new int[200];
        for (int i = 0; i < 200; i++) v[i] = 120;
        return v;
    }

    static int[] kunciPanjang() {
        int[] v = Net.bytes("ini kunci yang jauh melebihi enam puluh empat byte jadi HMAC musti hash dulu");
        int[] out = new int[v.length + 6];
        for (int i = 0; i < v.length; i++) out[i] = v[i];
        out[v.length] = 45; out[v.length + 1] = 68; out[v.length + 2] = 79;
        out[v.length + 3] = 78; out[v.length + 4] = 69; out[v.length + 5] = 33;
        return out;
    }

    static int[] kunciPanjang512() {
        int[] v = new int[200];
        for (int i = 0; i < 200; i++) v[i] = 107;
        return v;
    }''')

main = "    public static void main(String[] args) {\n"
main += "        System.out.println(Hash.eq(Hash.sha256(Net.bytes(\"\")), dig1()));\n"
main += "        System.out.println(Hash.eq(Hash.sha256(Net.bytes(\"abc\")), dig2()));\n"
main += "        System.out.println(Hash.eq(Hash.sha256(sh112()), dig3()));\n"
main += "        System.out.println(Hash.eq(Hash.hmac(b0b20(), Net.bytes(\"Halo dunia Kopi\")), dig4()));\n"
main += "        System.out.println(Hash.eq(Hash.hmac(kunciPanjang(), Net.bytes(\"\")), dig5()));\n"
main += "        System.out.println(Hash.eq(Hash.sha512(Net.bytes(\"\")), s512e()));\n"
main += "        System.out.println(Hash.eq(Hash.sha512(Net.bytes(\"abc\")), s512a()));\n"
main += "        System.out.println(Hash.eq(Hash.sha512(sh200()), s512l()));\n"
main += "        System.out.println(Hash.eq(Hash.sha384(Net.bytes(\"\")), s384e()));\n"
main += "        System.out.println(Hash.eq(Hash.sha384(Net.bytes(\"abc\")), s384a()));\n"
main += "        System.out.println(Hash.eq(Hash.sha384(sh200()), s384l()));\n"
main += "        System.out.println(Hash.eq(Hash.hmacSha512(Net.bytes(\"key\"), Net.bytes(\"abc\")), h512a()));\n"
main += "        System.out.println(Hash.eq(Hash.hmacSha512(kunciPanjang512(), Net.bytes(\"abc\")), h512l()));\n"
main += "    }"

src = "import Hash;\n\nclass Main {\n" + main + "\n" + "\n".join(helpers) + "\n}\n"
path = sys.argv[1] if len(sys.argv) > 1 else "hashvec.kop"
open(path, "w").write(src)
print("wrote", path, len(src))
