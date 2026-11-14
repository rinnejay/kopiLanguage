#!/usr/bin/env python3
"""gen_lzmavec.py — hasilkan uji LZMA vs oracle python `lzma`.

Dua kelompok vektor:
  1. FORMAT_ALONE (.lzma): props + dictSize + uncompressedSize + data
     diuji lewat Lzma.decompress().
  2. FORMAT_RAW: LZMA1 tanpa header, dengan berbagai lc/lp/pb
     diuji lewat Lzma.decompressRaw(comp, props, outSize) — outSize
     diketahui maupun -1 (berhenti pada end marker / akhir input).

Data disimpan sebagai string hex dan diurai oleh helper `unpack`, agar
kode program tetap kecil (batas jangkauan branch assembler a64).
Mencetak "kasus: N" agar test.sh bisa membuat file .ref berisi N baris "1".
"""
import lzma
import os
import sys

def lzma_alone(data):
    return lzma.compress(data, format=lzma.FORMAT_ALONE,
                         filters=[{"id": lzma.FILTER_LZMA1, "preset": 6}])

def lzma_raw(data, lc, lp, pb):
    return lzma.compress(data, format=lzma.FORMAT_RAW, filters=[{
        "id": lzma.FILTER_LZMA1, "preset": 6,
        "lc": lc, "lp": lp, "pb": pb, "dict_size": 1 << 16}])

def props_byte(lc, lp, pb):
    return (pb * 5 + lp) * 9 + lc

def hexstr(b):
    return "".join("%02x" % x for x in b)

samples = {
    "empty":  b"",
    "one":    b"a",
    "hello":  b"Hello, World!",
    "repeat": b"A" * 1000,
    "text":   b"Lorem ipsum dolor sit amet, consectetur adipiscing elit. " * 5,
    "binary": bytes(range(256)) * 4,
    "long":   b"The quick brown fox jumps over the lazy dog. " * 50,
}

# (nama, data, lc, lp, pb) — liblzma mensyaratkan lc + lp <= 4
raw_cases = [
    ("raw000", b"", 0, 0, 0),
    ("raw001", b"a", 3, 0, 2),
    ("raw002", b"Hello, raw LZMA!", 3, 0, 2),
    ("raw003", b"repeat-" * 200, 0, 0, 0),
    ("raw004", bytes(range(200)), 2, 0, 1),
    ("raw005", b"abcabcabcabc" * 100, 4, 0, 1),
    ("raw006", b"lp dan pb bervariasi. " * 30, 2, 2, 2),
    ("raw007", b"the quick brown fox. " * 40, 1, 3, 4),
    ("raw008", b"pb nol dan lp maksimal. " * 30, 0, 4, 2),
]

# jarak match yang menguji reverse-tree "spec pos" dan align LSB-first.
# Setiap data = dua paruh identik acak -> satu match jarak D.
import random as _random
_rnd = _random.Random(20261114)
_dist_sizes = [5, 7, 9, 10, 13, 17, 20, 33, 50, 65, 99, 100, 127, 128,
               129, 130, 133, 150, 199, 200, 255, 256, 257, 300, 400, 429]
for _i, _d in enumerate(_dist_sizes):
    _half = bytes(_rnd.randrange(256) for _ in range(_d))
    raw_cases.append(("rawd%03d" % _i, _half + _half, 3, 0, 2))

helpers = ['''    static int hexv(int c) {
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

for name, raw in samples.items():
    comp = lzma_alone(raw)
    cases.append('''        malloc {
            cek(Lzma.decompress(unpack("%s")), unpack("%s"));
        }''' % (hexstr(comp), hexstr(raw)))

for name, raw, lc, lp, pb in raw_cases:
    comp = lzma_raw(raw, lc, lp, pb)
    props = props_byte(lc, lp, pb)
    hc = hexstr(comp)
    ho = hexstr(raw)
    cases.append('''        malloc {
            cek(Lzma.decompressRaw(unpack("%s"), %d, %d), unpack("%s"));
        }''' % (hc, props, len(raw), ho))
    cases.append('''        malloc {
            cek(Lzma.decompressRaw(unpack("%s"), %d, -1), unpack("%s"));
        }''' % (hc, props, ho))

out = "// lzmavec.kop — uji Lzma (alone + raw) lawan oracle python lzma.\n"
out += "import Lzma;\nimport Str;\n\n"
out += "class Main {\n"
for h in helpers:
    out += h + "\n\n"
calls = []
for i, body in enumerate(cases):
    name = "t%d" % i
    out += "    static void %s() {\n%s\n    }\n\n" % (name, body)
    calls.append("        %s();" % name)
out += "    static void main() {\n"
out += "\n".join(calls)
out += "\n    }\n}\n"

os.makedirs(os.path.dirname(sys.argv[1]) or ".", exist_ok=True)
open(sys.argv[1], "w").write(out)
print("kasus: %d" % len(cases))
