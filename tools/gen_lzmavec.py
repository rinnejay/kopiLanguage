#!/usr/bin/env python3
"""gen_lzmavec.py — hasilkan uji LZMA vs oracle python `lzma`.

Dua kelompok vektor:
  1. FORMAT_ALONE (.lzma): props + dictSize + uncompressedSize + data
     diuji lewat Lzma.decompress().
  2. FORMAT_RAW: LZMA1 tanpa header, dengan berbagai lc/lp/pb
     diuji lewat Lzma.decompressRaw(comp, props, outSize) — outSize
     diketahui maupun -1 (berhenti pada end marker / akhir input).

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

def arr(name, b):
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(b)
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body

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

helpers = []
cases = []

for name, raw in samples.items():
    comp = lzma_alone(raw)
    helpers.append(arr("comp_" + name, comp))
    helpers.append(arr("orig_" + name, raw))
    cases.append("""        malloc {
            int[] c = comp_%s();
            int[] d = Lzma.decompress(c);
            int[] o = orig_%s();
            int ok = 1;
            if (d.length != o.length) ok = 0;
            else {
                for (int i = 0; i < o.length; i++) {
                    if (d[i] != o[i]) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (name, name))

for name, raw, lc, lp, pb in raw_cases:
    comp = lzma_raw(raw, lc, lp, pb)
    props = props_byte(lc, lp, pb)
    helpers.append(arr("comp_" + name, comp))
    helpers.append(arr("orig_" + name, raw))
    cases.append("""        malloc {
            int[] c = comp_%s();
            int[] d = Lzma.decompressRaw(c, %d, %d);
            int[] o = orig_%s();
            int ok = 1;
            if (d.length != o.length) ok = 0;
            else {
                for (int i = 0; i < o.length; i++) {
                    if (d[i] != o[i]) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (name, props, len(raw), name))
    cases.append("""        malloc {
            int[] c = comp_%s();
            int[] d = Lzma.decompressRaw(c, %d, -1);
            int[] o = orig_%s();
            int ok = 1;
            if (d.length != o.length) ok = 0;
            else {
                for (int i = 0; i < o.length; i++) {
                    if (d[i] != o[i]) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (name, props, name))

out = "// lzmavec.kop — uji Lzma (alone + raw) lawan oracle python lzma.\n"
out += "import Lzma;\nimport Str;\n\n"
out += "class Main {\n"
for h in helpers:
    out += h + "\n\n"
out += "    static void main() {\n"
out += "\n".join(cases)
out += "\n    }\n}\n"

os.makedirs(os.path.dirname(sys.argv[1]) or ".", exist_ok=True)
open(sys.argv[1], "w").write(out)
print("kasus: %d" % len(cases))
