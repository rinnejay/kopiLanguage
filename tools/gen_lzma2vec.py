#!/usr/bin/env python3
"""gen_lzma2vec.py — hasilkan uji LZMA2 lawan oracle python lzma.

Kelompok vektor:
  1. Stream LZMA2 FORMAT_RAW dari Python (berbagai ukuran/konten).
  2. Stream multi-chunk (2,5 MB data berpola) — menguji state persisten
     antar-chunk, termasuk chunk lanjutan reset mode 0.
  3. Stream chunk tak terkompresi buatan tangan (control 0x01/0x02).
  4. Status galat: data rusak (-1) dan terpotong (-2).

Mencetak "kasus: N".
"""
import lzma
import os
import random
import sys


def lzma2(data, preset=6):
    return lzma.compress(data, format=lzma.FORMAT_RAW,
                         filters=[{"id": lzma.FILTER_LZMA2, "preset": preset}])


def u16(v):
    v -= 1
    return [(v >> 8) & 255, v & 255]


def arr(name, b):
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(b)
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body


def eq_case(name, comp, orig):
    return """        malloc {
            int[] d = Lzma2.decompress(c_%s());
            int[] o = o_%s();
            int ok = (d.length == o.length) ? 1 : 0;
            if (ok == 1) {
                for (int i = 0; i < o.length; i++) {
                    if (d[i] != o[i]) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (name, name)


rnd = random.Random(4242)
samples = {
    "empty": b"",
    "one": b"a",
    "hello": b"Hello, LZMA2!",
    "repeat": b"Z" * 1000,
    "text": b"The quick brown fox jumps over the lazy dog. " * 20,
    "binary": bytes(range(256)),
    "random": bytes(rnd.randrange(256) for _ in range(512)),
}

helper = []
cases = []

for name, raw in samples.items():
    comp = lzma2(raw)
    helper.append(arr("c_" + name, comp))
    helper.append(arr("o_" + name, raw))
    cases.append(eq_case(name, comp, raw))

# ── jarak match: reverse-tree spec-pos (slot < 14) & align LSB-first ──
rnd2 = random.Random(20261115)
for _d in [5, 17, 100, 130, 300]:
    _half = bytes(rnd2.randrange(256) for _ in range(_d))
    _data = _half + _half
    _comp = lzma2(_data)
    _nm = "dist%d" % _d
    helper.append(arr("c_" + _nm, _comp))
    helper.append(arr("o_" + _nm, _data))
    cases.append(eq_case(_nm, _comp, _data))

# ── multi-chunk 2,5 MB (pola 10 byte) ─────────────────────────────────
big = b"abcdefghij" * 250000
cbig = lzma2(big)
helper.append(arr("c_big", cbig))
cases.append("""        malloc {
            int[] d = Lzma2.decompress(c_big());
            int ok = (d.length == 2500000) ? 1 : 0;
            if (ok == 1) {
                for (int i = 0; i < d.length; i++) {
                    if (d[i] != (i % 10) + 97) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""")

# ── chunk tak terkompresi buatan tangan ──────────────────────────────
rawA = b"hello uncompressed chunk A; "
rawB = b"B" * 300
stream = [0x01] + u16(len(rawA)) + list(rawA)
stream += [0x02] + u16(len(rawB)) + list(rawB)
stream += [0x00]
helper.append(arr("c_raw", stream))
raw_case = """        malloc {
            int[] d = Lzma2.decompress(c_raw());
            int ok = (d.length == %d) ? 1 : 0;
            if (ok == 1) {
                for (int i = 0; i < %d; i++) {
                    if (d[i] != raw_want(i)) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (len(rawA) + len(rawB), len(rawA) + len(rawB))
cases.append(raw_case)

want_fn = "    static int raw_want(int i) {\n"
for i, x in enumerate(rawA):
    want_fn += "        if (i == %d) return %d;\n" % (i, x)
want_fn += "        return 66;\n    }"
helper.append(want_fn)

# ── status galat ─────────────────────────────────────────────────────
corrupt = list(lzma2(b"corrupt me please"))
corrupt[0] = 0x03
helper.append(arr("c_bad", corrupt))
cases.append("""        malloc {
            int[] st = new int[1];
            int[] d = Lzma2.decode(c_bad(), st);
            System.out.println(Str.fromInt(st[0] == -1 ? 1 : 0));
        }""")

trunc = list(lzma2(b"truncated stream payload payload payload"))
trunc = trunc[:len(trunc) // 2]
helper.append(arr("c_trunc", trunc))
cases.append("""        malloc {
            int[] st = new int[1];
            int[] d = Lzma2.decode(c_trunc(), st);
            System.out.println(Str.fromInt(st[0] < 0 ? 1 : 0));
        }""")

out = "// lzma2vec.kop — uji Lzma2 lawan oracle python lzma.\n"
out += "import Lzma2;\nimport Str;\n\nclass Main {\n"
for h in helper:
    out += h + "\n\n"
out += "    static void main() {\n"
out += "\n".join(cases)
out += "\n    }\n}\n"

os.makedirs(os.path.dirname(sys.argv[1]) or ".", exist_ok=True)
open(sys.argv[1], "w").write(out)
print("kasus: %d" % len(cases))
