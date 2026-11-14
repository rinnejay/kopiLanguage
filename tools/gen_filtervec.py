#!/usr/bin/env python3
"""gen_filtervec.py — hasilkan uji Filter (delta + BCJ) lawan oracle.

Oracle filter-only diperoleh dengan trik: kompres data memakai rantai
[filter, LZMA2] dalam FORMAT_RAW, lalu dekompres hanya lapisan LZMA2
(filters=[LZMA2]) sehingga keluarannya tepat hasil filter.

  - Python lzma mendukung: delta, x86, powerpc, ia64, arm, armthumb, sparc.
  - ARM64 tidak diekspos binding Python; dipakai CLI `xz --format=raw
    --arm64 --lzma2=dict=64KiB` lalu lapisan LZMA2 didekompres Python.
    Bila `xz` tidak ada, ARM64 hanya diuji roundtrip.

Mencetak "kasus: N" agar test.sh bisa membuat file .ref.
"""
import lzma
import os
import random
import shutil
import subprocess
import sys
import tempfile

DICT = 65536


def oracle(fid, extra, data):
    f = [dict({"id": fid}, **extra), {"id": lzma.FILTER_LZMA2, "dict_size": DICT}]
    comp = lzma.compress(data, format=lzma.FORMAT_RAW, filters=f)
    return lzma.decompress(comp, format=lzma.FORMAT_RAW,
                           filters=[{"id": lzma.FILTER_LZMA2, "dict_size": DICT}])


def oracle_arm64(data):
    exe = shutil.which("xz")
    if not exe:
        return None
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "in.bin")
        with open(src, "wb") as f:
            f.write(data)
        raw = subprocess.run([exe, "--format=raw", "--arm64",
                              "--lzma2=dict=64KiB", "-c", src],
                             check=True, stdout=subprocess.PIPE).stdout
    return lzma.decompress(raw, format=lzma.FORMAT_RAW,
                           filters=[{"id": lzma.FILTER_LZMA2, "dict_size": DICT}])


def arr(name, b):
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(b)
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body


# data umum: acak deterministik + teks + pola berulang
rnd = random.Random(20261114)
generic = bytes(rnd.randrange(256) for _ in range(256))
generic += b"the quick brown fox jumps over the lazy dog. " * 2
generic += bytes(range(64))

crafted = {
    0x03: bytes(range(40)),
    0x04: bytes([0xE8, 0x01, 0x02, 0x03, 0x00, 0xE9, 0xFF, 0xFF, 0xFF, 0xFF,
                 0x90, 0xE8, 0x10, 0x20, 0x30, 0x00]),
    0x05: bytes([0x48, 0x00, 0x12, 0x01, 0x4B, 0xFF, 0xEE, 0x01]),
    0x06: bytes([22, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 (5 << 44) & 0xFF, (5 << 44 >> 8) & 0xFF,
                 (5 << 44 >> 16) & 0xFF, (5 << 44 >> 24) & 0xFF,
                 (5 << 44 >> 32) & 0xFF, (5 << 44 >> 40) & 0xFF]),
    0x07: bytes([0x00, 0x00, 0x00, 0xEB, 0x10, 0x20, 0x30, 0xEB]),
    0x08: bytes([0x00, 0xF0, 0x00, 0xF8, 0x11, 0xF0, 0x22, 0xF8]),
    0x09: bytes([0x40, 0x00, 0x00, 0x00, 0x7F, 0xC0, 0x00, 0x00]),
    0x0A: bytes([0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x90,
                 0x03, 0x00, 0x00, 0x94, 0x21, 0x00, 0x00, 0x90]),
}

py_filters = {
    0x03: (lzma.FILTER_DELTA, {"dist": 3}, 3),
    0x04: (lzma.FILTER_X86, {}, 0),
    0x05: (lzma.FILTER_POWERPC, {}, 0),
    0x06: (lzma.FILTER_IA64, {}, 0),
    0x07: (lzma.FILTER_ARM, {}, 0),
    0x08: (lzma.FILTER_ARMTHUMB, {}, 0),
    0x09: (lzma.FILTER_SPARC, {}, 0),
}

names = {
    0x03: "delta", 0x04: "x86", 0x05: "powerpc", 0x06: "ia64",
    0x07: "arm", 0x08: "armthumb", 0x09: "sparc", 0x0A: "arm64",
}

helper = []
cases = []
kid = 0


def emit(idv, arg, data, expect, label):
    """expect None → hanya roundtrip."""
    global kid
    kid += 1
    tag = "%s_%d" % (names[idv], kid)
    helper.append(arr("in_" + tag, data))
    if expect is not None:
        helper.append(arr("exp_" + tag, expect))
        cases.append("""        malloc {
            int[] want = exp_%s();
            int[] got = Filter.encode(%d, in_%s(), %d);
            int ok = (got.length == want.length) ? 1 : 0;
            if (ok == 1) {
                for (int i = 0; i < want.length; i++) {
                    if (got[i] != want[i]) { ok = 0; break; }
                }
            }
            int[] back = Filter.decode(%d, want, %d);
            if (back.length != in_%s().length) ok = 0;
            else {
                int[] orig = in_%s();
                for (int i = 0; i < orig.length; i++) {
                    if (back[i] != orig[i]) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (tag, idv, tag, arg, idv, arg, tag, tag))
    else:
        cases.append("""        malloc {
            int[] orig = in_%s();
            int[] enc = Filter.encode(%d, orig, %d);
            int[] back = Filter.decode(%d, enc, %d);
            int ok = 1;
            if (back.length != orig.length) ok = 0;
            else {
                for (int i = 0; i < orig.length; i++) {
                    if (back[i] != orig[i]) { ok = 0; break; }
                }
            }
            System.out.println(Str.fromInt(ok));
        }""" % (tag, idv, arg, idv, arg))


# delta beberapa jarak
for dist in [1, 2, 3, 16, 256]:
    d = crafted[0x03] + generic
    emit(0x03, dist, d, oracle(lzma.FILTER_DELTA, {"dist": dist}, d), "delta")

# BCJ yang didukung Python
for idv in [0x04, 0x05, 0x06, 0x07, 0x08, 0x09]:
    fid, extra, arg = py_filters[idv]
    d = crafted[idv] + generic
    emit(idv, arg, d, oracle(fid, extra, d), names[idv])

# ARM64: oracle via xz CLI, fallback roundtrip
d = crafted[0x0A] + generic
exp = oracle_arm64(d)
emit(0x0A, 0, d, exp, "arm64")

out = "// filtervec.kop — uji Filter (delta/BCJ) lawan oracle python lzma + xz.\n"
out += "import Filter;\nimport Str;\n\nclass Main {\n"
for h in helper:
    out += h + "\n\n"
out += "    static void main() {\n"
out += "\n".join(cases)
out += "\n    }\n}\n"

os.makedirs(os.path.dirname(sys.argv[1]) or ".", exist_ok=True)
open(sys.argv[1], "w").write(out)
print("kasus: %d" % len(cases))
