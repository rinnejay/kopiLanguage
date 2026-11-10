#!/usr/bin/env python3
"""gen_kdf: tulis test/hkdf.kop — HKDF RFC 5869 kasus 1 + ExpandLabel RFC 8446."""
import hmac, hashlib, sys

ikm  = b'\x0b' * 22
salt = bytes(range(13))
info = bytes(range(240, 250))
prk  = hmac.new(salt, ikm, hashlib.sha256).digest()

def hexp(prk, info, L):
    ok = b""
    t = b""
    for i in range(1, (L + 31) // 32 + 1):
        t = hmac.new(prk, t + info + bytes([i]), hashlib.sha256).digest()
        ok += t
    return ok[:L]

okm42 = hexp(prk, info, 42)
okm80 = hexp(prk, info, 80)
# HkdfLabel = uint16(32) | "tls13 " | uint8(2) | "cd" | uint8(0)
lab = b'\x00\x20tls13 \x02cd\x00'
xl32 = hexp(prk, lab, 32)

def hlp(name, b):
    body = "    static int[] %s() {\n        int[] v = new int[%d];\n" % (name, len(b))
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body

src = """import Kdf;
import Hash;

class Main {
    static int[] ikm() {
        int[] v = new int[22];
        for (int i = 0; i < 22; i++) v[i] = 11;
        return v;
    }
    static int[] salt() {
        int[] v = new int[13];
        for (int i = 0; i < 13; i++) v[i] = i;
        return v;
    }
    static int[] info() {
        int[] v = new int[10];
        for (int i = 0; i < 10; i++) v[i] = 240 + i;
        return v;
    }
    static int[] lab() {
        int[] v = new int[2];
        v[0] = 99; v[1] = 100;
        return v;
    }

    public static void main(String[] args) {
        int[] prk = Kdf.extract(salt(), ikm());
        System.out.println(Hash.eq(prk, prkRef()));          /* RFC 5869 prk */
        System.out.println(Hash.eq(Kdf.expand(prk, info(), 42), okm42Ref()));
        System.out.println(Hash.eq(Kdf.expand(prk, info(), 80), okm80Ref()));
        System.out.println(Hash.eq(Kdf.expandLabel(prk, lab(), new int[0], 32), xlRef()));
    }
""" + hlp("prkRef", prk) + "\n" + hlp("okm42Ref", okm42) + "\n" + \
      hlp("okm80Ref", okm80) + "\n" + hlp("xlRef", xl32) + "}\n"
path = sys.argv[1] if len(sys.argv) > 1 else "/data/data/com.termux/files/usr/tmp/kopi/hkdf.kop"
open(path, "w").write(src)
print("wrote", path)
