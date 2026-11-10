#!/usr/bin/env python3
"""gen_poly1305: blok test Poly1305 — RFC 7539 §2.5.2 + Appendix A.3.

ATURAN: tag dihitung oleh oracle python murni (mod p, int besar) lalu
DIVERIFIKASI sama dengan tag yang tercetak di RFC; bila beda → generator
menolak (jangan menghasilkan test yang menipu diri).
"""
import re, sys

RFC = "/data/data/com.termux/files/usr/tmp/kopi/rfc7539.txt"
MASK = int("0ffffffc0ffffffc0ffffffc0fffffff", 16)
P = (1 << 130) - 5


def poly1305_ref(r_bytes, s_bytes, msg):
    r = int.from_bytes(r_bytes, "little") & MASK
    s = int.from_bytes(s_bytes, "little")
    a = 0
    n = len(msg)
    for i in range(0, n, 16):
        blk = msg[i:i + 16]
        nb = int.from_bytes(blk, "little") + (1 << (8 * len(blk)))
        a = (a + nb) * r % P
    tag = (a + s) & ((1 << 128) - 1)
    return tag.to_bytes(16, "little")


def parse_tail_block(text_after):
    """Parse blok '000  xx xx ...' (R:/S:/data:/tag: form A.3 kecil)."""
    lbl = {}
    for want in ("R", "S", "data", "tag"):
        m = re.search(r"^\s*%s:\n((?:\s{3}(?:[0-9A-F]{2}\s)+[0-9A-F]{2}\s*$)+)" % want,
                      text_after, re.M | re.I)
        if m:
            hexs = "".join(re.findall(r"[0-9A-Fa-f]{2}", m.group(1)))
            lbl[want] = bytes.fromhex(hexs)
    return lbl


def parse_dump_block(label, seg, n16):
    """Parse blok hexdump '000  xx .. / 016  xx..' setelah `label`."""
    m = re.search(label + r"\n\n?((?:\s+\d{3}\s+[0-9a-f ]{6,}\n)+)", seg)
    if not m:
        return None
    out = bytearray()
    for ln in m.group(1).split("\n"):
        hxs = re.findall(r"[0-9a-f]{2}", ln.split(None, 2)[2] if len(ln.split(None, 2)) > 2 else ln)
        out += bytes.fromhex("".join(hxs))
    return bytes(out)


def main():
    s = open(RFC).read()

    kasus = []

    # § 2.5.2: satu vektor utuh
    key2 = bytes.fromhex(
        "85d6be7857556d337f4452fe42d506a8"
        "0103808afb0db2fd4abff6af4149f51b")
    msg2 = b"Cryptographic Forum Research Group"
    tag2 = bytes.fromhex("a8061dc1305136c6c22b8baf0c0127a9")
    got = poly1305_ref(key2[:16], key2[16:], msg2)
    assert got == tag2, "oracle beda RFC §2.5.2: %s vs %s" % (got.hex(), tag2.hex())
    kasus.append(("t25", key2[:16], key2[16:], msg2, tag2))

    # Appendix A.3 vektor r-kecil (jawaban FAQ) — format "R:/S:/data:/tag:"
    i = s.index("A.3.  Poly1305 Message Authentication Code", 20000)
    seg = s[i:i+22000]
    # cari blok R:\n...\nS:\n...\ndata:\n...\ntag:\n...
    pat = re.compile(
        r"R:\n((?:\s+[0-9A-F]{2}(?:\s+[0-9A-F]{2}){15}\n)+)"
        r"\s*S:\n((?:\s+[0-9A-F]{2}(?:\s+[0-9A-F]{2}){15}\n)+)"
        r"\s*data:\n((?:\s+[0-9A-F]{2}(?:\s+[0-9A-F]{2}){15}\n)+)"
        r"\s*tag:\n((?:\s+[0-9A-F]{2}(?:\s+[0-9A-F]{2}){15}\n)+)", re.I)
    idx = 0
    for m in pat.finditer(seg):
        idx += 1
        def h(g):
            return bytes.fromhex("".join(re.findall(r"[0-9A-Fa-f]{2}", m.group(g))))
        r, sb, dat, tg = h(1), h(2), h(3), h(4)
        got = poly1305_ref(r, sb, dat)
        if got != tg:
            print("vektor A.3-%d MISMATCH oracle vs RFC (skip):" % idx,
                  got.hex(), tg.hex())
            continue
        print("A.3-%d cocok oracle (%d byte data)" % (idx, len(dat)))
        kasus.append(("a3v%d" % idx, r, sb, dat, tg))

    if not any(k[0].startswith("a3v") for k in kasus):
        sys.exit("tidak ada vektor A.3 yang cocok — cek parser")

    # Vektor panjang deterministik (bulatkan bug carry/slot-9 pada Poly);
    # tag dihitung oracle python murni, bukan mesin Kopi.
    import random
    rnd = random.Random(20261110)
    for k, ln in enumerate((701, 809, 900)):
        data = bytes(rnd.randrange(256) for _ in range(ln))
        kb = bytes(rnd.randrange(256) for _ in range(32))
        r, sb = kb[:16], kb[16:]
        kasus.append(("big%d" % k, r, sb, data, poly1305_ref(r, sb, data)))

    # tulis generator Kopi
    def hlp(name, b):
        body = "    static int[] %s() {\n        int[] v = new int[%d];\n" % (name, len(b))
        for i, x in enumerate(b):
            body += "        v[%d] = %d;\n" % (i, x)
        body += "        return v;\n    }"
        return body

    main = []
    helpers = ["""    static int eq(int[] a, int[] b) {
        if (a.length != b.length) return 0;
        for (int i = 0; i < a.length; i++) if (a[i] != b[i]) return 0;
        return 1;
    }"""]
    main.append("    public static void main(String[] args) {")
    for j, (nm, r, sb, dat, tg) in enumerate(kasus):
        main.append("        int[] d%d = Poly.mac(rf%d(), sf%d(), data%d());"
                    % (j, j, j, j))
        main.append("        System.out.println(eq(d%d, tag%d()));" % (j, j))
        helpers.append(hlp("rf%d" % j, r))
        helpers.append(hlp("sf%d" % j, sb))
        helpers.append(hlp("data%d" % j, dat))
        helpers.append(hlp("tag%d" % j, tg))
    main.append("    }")

    src = "import Poly;\n\nclass Main {\n" + "\n".join(main + helpers) + "\n}\n"
    kpath = sys.argv[1] if len(sys.argv) > 1 else \
        "/data/data/com.termux/files/usr/tmp/kopi/polyvec.kop"
    open(kpath, "w").write(src)
    print("kasus:", len(kasus), "→", kpath)


if __name__ == "__main__":
    main()
