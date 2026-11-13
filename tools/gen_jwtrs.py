#!/usr/bin/env python3
"""gen_jwtrs.py — oracle python untuk uji JWT RS256 (kunci RSA-512 deterministik)."""
import base64
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_rsavec import PRIV_DER, PUB_DER, P, Q, E, N, D  # noqa: E402


def b64u(b):
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def rs256_sign(payload_bytes, d, n):
    t = bytes.fromhex("3031300d060960864801650304020105000420") + hashlib.sha256(payload_bytes).digest()
    k = (n.bit_length() + 7) // 8
    em = b"\x00\x01" + b"\xff" * (k - len(t) - 3) + b"\x00" + t
    return pow(int.from_bytes(em, "big"), d, n).to_bytes(k, "big")


def arr(name, data):
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(data)
    for i, x in enumerate(data):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body


def main():
    kop_path = sys.argv[1]
    ref_path = sys.argv[2]
    payload = b'{"sub":"budi","n":1}'
    header = b'{"alg":"RS256","typ":"JWT"}'
    signing = (b64u(header) + "." + b64u(payload)).encode()
    sig = rs256_sign(signing, D, N)
    token = signing.decode() + "." + b64u(sig)

    # kunci publik lain (n sama, e tetap, tapi dipadankan dengan d ke-1 → salah)
    wrong_pub = None

    src = """import Jwt;
import Hash;
import Cipher;
import Str;

// kunci RSA-512-bit deterministik (p/q tetap)
class Main {
    public static void main(String[] args) {
        int[] priv = privDer();
        int[] pub = pubDer();
        String token = Jwt.signRs256("{\\"sub\\":\\"budi\\",\\"n\\":1}", priv);
        System.out.println(token);
        int[] out = new int[64];
        System.out.println(Jwt.verifyRs256(token, pub, out));
        System.out.println(Jwt.verifyRs256(token, pubLain(), out));
        System.out.println(Jwt.verifyRs256("abc", pub, out));
        String pl = Net.mkstr(out);
        int e = pl.length;
        while (e > 0 && pl.charAt(e - 1) == 0) e = e - 1;
        System.out.println(Str.sub(pl, 0, e));
    }
"""
    src += "\n" + arr("privDer", list(PRIV_DER)) + "\n\n" + arr("pubDer", list(PUB_DER))
    # kunci publik "lain": modulus sama, eksponen 3 → tanda tangan tak cocok
    alt = bytearray(PUB_DER)
    # ganti eksponen 0x010001 (17) dengan 0x000003 (3) pada posisi terakhir
    alt[-1] = 3
    src += "\n\n" + arr("pubLain", list(alt)) + "\n}\n"
    open(kop_path, "w").write(src)

    ref = [token, "1", "0", "-1", payload.decode()]
    open(ref_path, "w").write("\n".join(ref) + "\n")
    print("wrote", kop_path, "dan", ref_path)


if __name__ == "__main__":
    main()
