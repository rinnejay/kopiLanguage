#!/usr/bin/env python3
"""gen_rsavec.py — oracle python (RSA PKCS#1 v1.5) + program uji .kop.

Kunci uji: RSA-512-bit deterministik (p/q tetap, bukan acak).
Menghasilkan berkas .kop (program uji) dan .ref (harapan).
"""
import hashlib
import os
import sys

P = 0xe9686befa32f445eeb4995a65d3437a221709310d584232104abcdab2dbc1cd7
Q = 0xe4374f030c657bb61854234621ebea3b83eeeb6e10d13958753b4fac027697a7
E = 65537
N = P * Q
D = pow(E, -1, (P - 1) * (Q - 1))
DP = D % (P - 1)
DQ = D % (Q - 1)
QINV = pow(Q, -1, P)


def der_len(n):
    if n < 0x80:
        return bytes([n])
    if n < 0x100:
        return bytes([0x81, n])
    return bytes([0x82, n >> 8, n & 0xFF])


def der_int(v):
    if v == 0:
        b = b"\x00"
    else:
        b = v.to_bytes((v.bit_length() + 7) // 8, 'big')
        if b[0] & 0x80:
            b = b"\x00" + b
    return b"\x02" + der_len(len(b)) + b


def der_seq(body):
    return b"\x30" + der_len(len(body)) + body


PRIV_DER = der_seq(b"".join(der_int(v) for v in [0, N, E, D, P, Q, DP, DQ, QINV]))
PUB_DER = der_seq(der_int(N) + der_int(E))

PREFIX = {
    "sha1": bytes.fromhex("3021300906052b0e03021a05000414"),
    "sha256": bytes.fromhex("3031300d060960864801650304020105000420"),
    "sha384": bytes.fromhex("3041300d060960864801650304020205000430"),
    "sha512": bytes.fromhex("3051300d060960864801650304020305000440"),
}


def pkcs1_sign(digest, alg):
    t = PREFIX[alg] + digest
    k = (N.bit_length() + 7) // 8
    em = b"\x00\x01" + b"\xff" * (k - len(t) - 3) + b"\x00" + t
    m = pow(int.from_bytes(em, 'big'), D, N)
    return m.to_bytes(k, 'big')


def pkcs1_encrypt(msg):
    k = (N.bit_length() + 7) // 8
    ps = bytes(((i % 255) + 1) for i in range(k - len(msg) - 3))
    em = b"\x00\x02" + ps + b"\x00" + msg
    return pow(int.from_bytes(em, 'big'), E, N).to_bytes(k, 'big')


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
    digest = hashlib.sha256(b"abc").digest()
    sig = pkcs1_sign(digest, "sha256")
    sig512 = pkcs1_sign(hashlib.sha512(b"abc").digest(), "sha512")
    ct = pkcs1_encrypt(b"halo kopi")

    src = """import Rsa;
import Hash;
import Cipher;

// kunci uji RSA-512-bit (PKCS#1), tetap/ deterministik
class Main {
    public static void main(String[] args) {
        int[] priv = privDer();
        int[] pub = pubDer();
        int[] digest = Hash.sha256(Net.bytes("abc"));
        int[] sig = Rsa.sign(priv, digest, "SHA-256");

        System.out.println(Cipher.hexenc(sig));
        System.out.println(Rsa.verify(pub, digest, sig, "SHA-256"));
        System.out.println(Rsa.verify(pub, Hash.sha256(Net.bytes("abd")), sig, "SHA-256"));
        System.out.println(Cipher.hexenc(Rsa.publicKey(priv)));
        int[] ct = Rsa.encrypt(pub, Net.bytes("halo kopi"));
        System.out.println(Cipher.hexenc(ct));
        System.out.println(Net.mkstr(Rsa.decrypt(priv, ct)));
    }
"""
    src += "\n" + arr("privDer", list(PRIV_DER)) + "\n\n" + arr("pubDer", list(PUB_DER)) + "\n}\n"
    open(kop_path, "w").write(src)

    ref = [
        sig.hex(),
        "1",
        "0",
        PUB_DER.hex(),
        ct.hex(),
        "halo kopi",
    ]
    open(ref_path, "w").write("\n".join(ref) + "\n")
    print("wrote", kop_path, "dan", ref_path)


if __name__ == "__main__":
    main()
