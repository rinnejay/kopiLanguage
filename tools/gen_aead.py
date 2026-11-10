#!/usr/bin/env python3
"""gen_aead: blok uji AEAD ChaCha20-Poly1305 — RFC 7539 §2.8 / RFC 8439 §2.8.2.

ATURAN JUJUR: sebelum menulis test, oracle python murni (chacha20 + poly1305
mod p dengan int besar) HARUS mereproduksi ciphertext & tag RFC; kalau tidak,
generator berhenti dengan galat tanpa membuat test.
"""
import struct, sys

M = 0xFFFFFFFF
MASKP = int("0ffffffc0ffffffc0ffffffc0fffffff", 16)
P = (1 << 130) - 5

KEY = bytes(range(0x80, 0xa0))
NONCE = bytes.fromhex("070000004041424344454647")   # word13=7, word14=0x43424140, word15=0x47464544
AAD = bytes.fromhex("50515253c0c1c2c3c4c5c6c7")
PLAIN = (b"Ladies and Gentlemen of the class of '99: If I could offer you "
         b"only one tip for the future, sunscreen would be it.")
CT = bytes.fromhex(
    "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d6"
    "3dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b36"
    "92ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc"
    "3ff4def08e4b7a9de576d26586cec64b6116")
TAG = bytes.fromhex("1ae10b594f09e26a7e902ecbd0600691")


def rotl(x, n):
    return ((x << n) | (x >> (32 - n))) & M


def qr(s, a, b, c, d):
    s[a] = (s[a] + s[b]) & M
    s[d] = rotl(s[d] ^ s[a], 16)
    s[c] = (s[c] + s[d]) & M
    s[b] = rotl(s[b] ^ s[c], 12)
    s[a] = (s[a] + s[b]) & M
    s[d] = rotl(s[d] ^ s[a], 8)
    s[c] = (s[c] + s[d]) & M
    s[b] = rotl(s[b] ^ s[c], 7)


def block(ist):
    x = list(ist)
    for _ in range(10):
        for args in [(0, 4, 8, 12), (1, 5, 9, 13), (2, 6, 10, 14), (3, 7, 11, 15),
                     (0, 5, 10, 15), (1, 6, 11, 12), (2, 7, 8, 13), (3, 4, 9, 14)]:
            qr(x, *args)
    return b"".join(struct.pack("<I", (v + i) & M) for v, i in zip(x, ist))


def initst(key, nonce, ctr):
    st = [0x61707865, 0x3320646e, 0x79622d32, 0x6b206574]
    st += list(struct.unpack("<8I", key))
    st += [ctr, *struct.unpack("<3I", nonce)]
    return st


def chacha(key, nonce, ctr, data):
    out = bytearray()
    done = 0
    b = 0
    while done < len(data):
        ks = block(initst(key, nonce, ctr + b))
        take = min(64, len(data) - done)
        for i in range(take):
            out.append(data[done + i] ^ ks[i])
        done += take
        b += 1
    return bytes(out)


def pad16(b):
    return b if len(b) % 16 == 0 else b + bytes(16 - (len(b) % 16))


def poly_mac(r_raw, s_raw, md):
    r = int.from_bytes(r_raw, "little") & MASKP
    s_ = int.from_bytes(s_raw, "little")
    a = 0
    for i in range(0, len(md), 16):
        blk = md[i:i + 16]
        n = int.from_bytes(blk, "little") + (1 << (8 * len(blk)))
        a = (a + n) * r % P
    return ((a + s_) & ((1 << 128) - 1)).to_bytes(16, "little")


def oracle():
    otk = chacha(KEY, NONCE, 0, bytes(32))
    r = otk[:16]
    s_ = otk[16:32]
    ct = chacha(KEY, NONCE, 1, PLAIN)
    assert r != bytes(16) and s_ != bytes(16), "otk 0 — feed 32"
    assert ct == CT, "oracle ct beda RFC: %s" % ct.hex()
    md = pad16(AAD) + pad16(ct) + len(AAD).to_bytes(8, "little") + \
        len(ct).to_bytes(8, "little")
    tag = poly_mac(r, s_, md)
    assert tag == TAG, "oracle tag beda RFC: %s vs %s" % (tag.hex(), TAG.hex())
    return otk, ct, tag


def hlp(name, b):
    body = "    static int[] %s() {\n        int[] v = new int[%d];\n" % (name, len(b))
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body


def main():
    otk, ct, tag = oracle()
    kpath = sys.argv[1] if len(sys.argv) > 1 else \
        "/data/data/com.termux/files/usr/tmp/kopi/aeadvec.kop"
    src = """import Aead;
import Hash;

class Main {
    public static void main(String[] args) {
        int[] c = Aead.ctOf(key(), nonce(), plain());
        System.out.println(Hash.eq(c, ctRef()));
        int[] tg = Aead.tagOf(key(), nonce(), aad(), c);
        System.out.println(Hash.eq(tg, tagRef()));
        int[] back = Aead.ctOf(key(), nonce(), c);
        System.out.println(Hash.eq(back, plain()));
        c[5] = c[5] ^ 1;
        int[] tg2 = Aead.tagOf(key(), nonce(), aad(), c);
        System.out.println(Hash.eq(tg2, tagRef()));
    }
""" + hlp("key", KEY) + "\n" + hlp("nonce", NONCE) + "\n" + \
          hlp("aad", AAD) + "\n" + hlp("plain", PLAIN) + "\n" + \
          hlp("ctRef", CT) + "\n" + hlp("tagRef", TAG) + "\n}\n"
    open(kpath, "w").write(src)
    print("oracle OK — otk:", otk.hex()[:8], "… ct/tag cocok RFC; test →", kpath)


if __name__ == "__main__":
    main()
