#!/usr/bin/env python3
"""gen_checksumvec.py — hasilkan uji Checksum (CRC32/CRC64/VLI) lawan oracle.

Oracle:
  - CRC-32  : zlib.crc32 (identik dengan XZ check type 1)
  - CRC-64  : implementasi referensi CRC-64/XZ (ECMA-182) di skrip ini
  - VLI     : encoder referensi XZ (little-endian 7 bit/byte)

Mencetak "kasus: N" agar test.sh bisa membuat file .ref berisi N baris "1".
"""
import os
import sys
import zlib

MASK64 = (1 << 64) - 1
POLY64 = 0xC96C5795D7870F42


def crc64(data):
    crc = MASK64
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ POLY64
            else:
                crc >>= 1
    return crc ^ MASK64


def vli(v):
    out = []
    while True:
        b = v & 0x7F
        v >>= 7
        if v:
            out.append(b | 0x80)
        else:
            out.append(b)
            break
    return out


def i32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v >= (1 << 31) else v


def i64(v):
    v &= MASK64
    return v - (1 << 64) if v >= (1 << 63) else v


def arr(name, b):
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(b)
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }"
    return body


samples = {
    "empty": b"",
    "std": b"123456789",
    "hello": b"Hello, World!",
    "text": b"The quick brown fox jumps over the lazy dog. " * 7,
    "bin": bytes(range(256)),
    "rand": bytes((i * 37 + 11) & 255 for i in range(500)),
}

vlivals = [0, 1, 127, 128, 255, 300, 16383, 16384, 1234567890123]

helper = []
cases = []

for name, data in samples.items():
    helper.append(arr("d_" + name, data))

    # CRC32 one-shot
    cases.append("""        malloc {
            int got = Checksum.crc32(d_%s());
            System.out.println(Str.fromInt(got == %d ? 1 : 0));
        }""" % (name, i32(zlib.crc32(data))))

    # CRC32 incremental (pecah dua)
    h = len(data) // 2
    cases.append("""        malloc {
            int[] d = d_%s();
            int c = Checksum.crc32Init();
            c = Checksum.crc32Update(c, d, 0, %d);
            c = Checksum.crc32Update(c, d, %d, %d);
            int got = Checksum.crc32Final(c);
            System.out.println(Str.fromInt(got == %d ? 1 : 0));
        }""" % (name, h, h, len(data) - h, i32(zlib.crc32(data))))

    # CRC64 one-shot
    cases.append("""        malloc {
            long got = Checksum.crc64(d_%s());
            System.out.println(Str.fromLong(got == %dL ? 1L : 0L));
        }""" % (name, i64(crc64(data))))

    # CRC64 incremental
    cases.append("""        malloc {
            int[] d = d_%s();
            long c = Checksum.crc64Init();
            c = Checksum.crc64Update(c, d, 0, %d);
            c = Checksum.crc64Update(c, d, %d, %d);
            long got = Checksum.crc64Final(c);
            System.out.println(Str.fromLong(got == %dL ? 1L : 0L));
        }""" % (name, h, h, len(data) - h, i64(crc64(data))))

for v in vlivals:
    enc = vli(v)
    helper.append(arr("vli_%d" % v, enc))
    cases.append("""        malloc {
            int[] want = vli_%d();
            int[] got = Checksum.vliEncode(%dL);
            int ok = (got.length == want.length) ? 1 : 0;
            if (ok == 1) {
                for (int i = 0; i < want.length; i++) {
                    if (got[i] != want[i]) { ok = 0; break; }
                }
            }
            if (Checksum.vliSize(got, 0) != want.length) ok = 0;
            if (Checksum.vliDecode(got, 0) != %dL) ok = 0;
            System.out.println(Str.fromInt(ok));
        }""" % (v, v, v))

out = "// checksumvec.kop — uji Checksum lawan oracle python (zlib + CRC-64/XZ).\n"
out += "import Checksum;\nimport Str;\n\nclass Main {\n"
for h in helper:
    out += h + "\n\n"
out += "    static void main() {\n"
out += "\n".join(cases)
out += "\n    }\n}\n"

os.makedirs(os.path.dirname(sys.argv[1]) or ".", exist_ok=True)
open(sys.argv[1], "w").write(out)
print("kasus: %d" % len(cases))
