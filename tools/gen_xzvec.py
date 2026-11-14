#!/usr/bin/env python3
"""gen_xzvec.py — hasilkan uji decoder .xz lawan oracle python lzma / xz.

Cakupan:
  - check None / CRC32 / CRC64 / SHA-256
  - filter chain: LZMA2 saja, delta+LZMA2, BCJ x86/ARM + LZMA2,
    delta + x86 + LZMA2
  - beberapa stream .xz bersambung (concatenated)
  - multi-block dalam satu stream (via CLI xz --block-size)
  - filter ARM64 (via CLI xz --arm64)
  - status galat: header rusak (-1), terpotong (<0), check salah (-3)

Mencetak "kasus: N".
"""
import lzma
import os
import random
import shutil
import subprocess
import sys
import tempfile

MAGIC = b"\xfd7zXZ\x00"
CHECKS = {
    "none": (lzma.CHECK_NONE, 0),
    "crc32": (lzma.CHECK_CRC32, 4),
    "crc64": (lzma.CHECK_CRC64, 8),
    "sha256": (lzma.CHECK_SHA256, 32),
}


def vli_read(b, p):
    v = 0
    s = 0
    while True:
        x = b[p]
        p += 1
        v |= (x & 0x7F) << s
        if not (x & 0x80):
            return v, p
        s += 7


def lzma2_size(b, p):
    start = p
    while p < len(b):
        c = b[p]
        p += 1
        if c == 0:
            return p - start
        if c < 0x80:
            sz = ((b[p] << 8) | b[p + 1]) + 1
            p += 2 + sz
        else:
            cs = ((b[p + 2] << 8) | b[p + 3]) + 1
            reset = (c >> 5) & 3
            p += 4 + (1 if reset >= 2 else 0) + cs
    raise ValueError("lzma2 tak selesai")


def first_block_check_off(b):
    """offset field check block pertama (stream tunggal, blok pertama)."""
    pos = 12
    hsize = (b[pos] + 1) * 4
    h0 = pos
    flags = b[h0 + 1]
    p = h0 + 2
    csize = None
    if flags & 0x40:
        csize, p = vli_read(b, p)
    if flags & 0x80:
        _, p = vli_read(b, p)
    nf = (flags & 3) + 1
    for _ in range(nf):
        _, p = vli_read(b, p)
        psz, p = vli_read(b, p)
        p += psz
    data_off = h0 + hsize
    if csize is None:
        csize = lzma2_size(b, data_off)
    return data_off + csize


def hexstr(b):
    return "".join("%02x" % x for x in b)


rnd = random.Random(777)
base = (b"The quick brown fox jumps over the lazy dog. " * 8
        + bytes(range(256))
        + bytes(rnd.randrange(256) for _ in range(256)))
armdata = bytes([0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x90,
                 0x10, 0x20, 0x30, 0x94, 0x21, 0x00, 0x00, 0x90]) + base

helper = ['''    static int hexv(int c) {
        if (c <= 57) return c - 48;
        if (c <= 70) return c - 55;
        return c - 87;
    }

    static int[] unpack(String h) {
        int n = Str.len(h) / 2;
        int[] v = new int[n];
        for (int i = 0; i < n; i++) {
            v[i] = (hexv(Str.at(h, i * 2)) << 4) | hexv(Str.at(h, i * 2 + 1));
        }
        return v;
    }

    static void cek(int[] d, int[] o) {
        int ok = 1;
        if (d.length != o.length) ok = 0;
        else {
            for (int i = 0; i < o.length; i++) {
                if (d[i] != o[i]) { ok = 0; break; }
            }
        }
        System.out.println(Str.fromInt(ok));
    }''']
cases = []


def add(name, comp, data, label, multi_expected=None):
    if multi_expected is not None:
        plen, pat = multi_expected
        data = bytes((i % pat) + 48 for i in range(plen))
    cases.append('''        malloc {
            cek(Xz.decompress(unpack("%s")), unpack("%s"));
        }''' % (hexstr(comp), hexstr(data)))


# ── XZ via Python: check & filter chain ──────────────────────────────
py_cases = [
    ("def_crc64", {}, "crc64"),
    ("chk_none", {"check": lzma.CHECK_NONE}, "none"),
    ("chk_crc32", {"check": lzma.CHECK_CRC32}, "crc32"),
    ("chk_sha256", {"check": lzma.CHECK_SHA256}, "sha256"),
    ("empty", {"check": lzma.CHECK_CRC32}, "empty"),
]
for name, extra, tag in py_cases:
    data = b"" if tag == "empty" else base
    kw = {"format": lzma.FORMAT_XZ}
    kw.update(extra)
    if "filters" not in kw:
        kw["filters"] = [{"id": lzma.FILTER_LZMA2, "preset": 6}]
    comp = lzma.compress(data, **kw)
    add(name, comp, data, tag)

chains = [
    ("delta", [{"id": lzma.FILTER_DELTA, "dist": 4},
               {"id": lzma.FILTER_LZMA2, "preset": 6}]),
    ("x86", [{"id": lzma.FILTER_X86},
             {"id": lzma.FILTER_LZMA2, "preset": 6}]),
    ("arm", [{"id": lzma.FILTER_ARM},
             {"id": lzma.FILTER_LZMA2, "preset": 6}]),
    ("deltax86", [{"id": lzma.FILTER_DELTA, "dist": 2},
                  {"id": lzma.FILTER_X86},
                  {"id": lzma.FILTER_LZMA2, "preset": 6}]),
]
for name, filters in chains:
    comp = lzma.compress(base, format=lzma.FORMAT_XZ,
                         check=lzma.CHECK_CRC32, filters=filters)
    add(name, comp, base, name)

# ── jarak match: reverse-tree spec-pos (slot < 14) & align LSB-first ──
rnd2 = random.Random(20261116)
for _d in [17, 100, 130, 300]:
    _half = bytes(rnd2.randrange(256) for _ in range(_d))
    _data = _half + _half
    _comp = lzma.compress(_data, format=lzma.FORMAT_XZ,
                          check=lzma.CHECK_CRC32,
                          filters=[{"id": lzma.FILTER_LZMA2, "preset": 6}])
    add("dist%d" % _d, _comp, _data, "dist%d" % _d)

# ── concatenated streams ────────────────────────────────────────────
d1 = b"stream pertama. " * 20
d2 = b"stream kedua!! " * 25
c1 = lzma.compress(d1, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32,
                   filters=[{"id": lzma.FILTER_LZMA2, "preset": 6}])
c2 = lzma.compress(d2, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC64,
                   filters=[{"id": lzma.FILTER_LZMA2, "preset": 6}])
add("concat", c1 + c2, d1 + d2, "concat")

# ── status galat ────────────────────────────────────────────────────
good = lzma.compress(base, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32,
                     filters=[{"id": lzma.FILTER_LZMA2, "preset": 6}])

badhdr = list(good)
badhdr[7] = 0x55  # reserved stream flag
cases.append('''        malloc {
            int[] st = new int[1];
            int[] d = Xz.decode(unpack("%s"), st);
            System.out.println(Str.fromInt(st[0] < 0 ? 1 : 0));
        }''' % hexstr(badhdr))

trunc = list(good[:len(good) // 2])
cases.append('''        malloc {
            int[] st = new int[1];
            int[] d = Xz.decode(unpack("%s"), st);
            System.out.println(Str.fromInt(st[0] < 0 ? 1 : 0));
        }''' % hexstr(trunc))

badcheck = list(good)
co = first_block_check_off(good)
badcheck[co] ^= 0xFF
cases.append('''        malloc {
            int[] st = new int[1];
            int[] d = Xz.decode(unpack("%s"), st);
            System.out.println(Str.fromInt(st[0] == -3 ? 1 : 0));
        }''' % hexstr(badcheck))

# ── multi-block via CLI xz ──────────────────────────────────────────
xz = shutil.which("xz")
if xz:
    big = (b"0123456789" * 20000)  # 200000 byte
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "big.bin")
        with open(src, "wb") as f:
            f.write(big)
        comp = subprocess.run([xz, "--format=xz", "--check=crc32",
                               "--block-size=65536", "-c", src],
                              check=True, stdout=subprocess.PIPE).stdout
    add("multiblock", comp, None, "multiblock", (len(big), 10))

# ── ARM64 via CLI xz ────────────────────────────────────────────────
if xz:
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "arm.bin")
        with open(src, "wb") as f:
            f.write(armdata)
        comp = subprocess.run([xz, "--format=xz", "--check=crc32",
                               "--arm64", "--lzma2=preset=6", "-c", src],
                              check=True, stdout=subprocess.PIPE).stdout
    add("arm64", comp, armdata, "arm64")

out = "// xzvec.kop — uji decoder .xz lawan oracle python lzma + xz.\n"
out += "import Xz;\nimport Str;\n\nclass Main {\n"
for h in helper:
    out += h + "\n\n"
out += "    static void main() {\n"
out += "\n".join(cases)
out += "\n    }\n}\n"

os.makedirs(os.path.dirname(sys.argv[1]) or ".", exist_ok=True)
open(sys.argv[1], "w").write(out)
print("kasus: %d" % len(cases))
