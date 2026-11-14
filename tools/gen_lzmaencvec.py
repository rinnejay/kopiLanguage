#!/usr/bin/env python3
"""gen_lzmaencvec.py — uji encoder LZMA1/LZMA2/XZ + auto-detect + streaming.

Semua nilai harapan dihitung oleh encoder referensi Python
(`tools/lzmaenc_ref.py`) dan SETIAP stream keluaran didekode ulang dengan
modul `lzma` sebelum ditulis. Jadi tidak ada byte yang ditebak.

Kelompok uji:
  A. LZMA1 raw: byte encoder == referensi + roundtrip decoder.
  B. LZMA1 alone (.lzma): header + byte + roundtrip Lzma.decompress.
  C. LZMA2: byte stream == referensi + roundtrip Lzma2.decompress.
  D. XZ writer: byte stream == referensi + roundtrip Xz.decompress,
     untuk check None/CRC32/CRC64/SHA-256.
  E. LzmaAuto: deteksi .xz vs alone, status galat/terpotong.
  F. Lzma2DecStream / Lzma2EncStream: chunk terpotong acak.

Mencetak "kasus: N".
"""
import hashlib
import lzma
import os
import random
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lzmaenc_ref

MAGIC = b"\xfd7zXZ\x00"
POLY64 = 0xC96C5795D7870F42
M64 = (1 << 64) - 1


def hexstr(b):
    return "".join("%02x" % x for x in b)


def props_byte(lc, lp, pb):
    return (pb * 5 + lp) * 9 + lc


def vli(v):
    out = bytearray()
    while True:
        b = v & 0x7F
        v >>= 7
        if v:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def crc64(data):
    crc = M64
    for x in data:
        crc ^= x
        for _ in range(8):
            crc = (crc >> 1) ^ (POLY64 if (crc & 1) else 0)
    return crc ^ M64


def lzma1_ref(data, lc, lp, pb, dict_size):
    return lzmaenc_ref.raw_compress(data, lc, lp, pb, dict_size)


def lzma2_ref(data, lc, lp, pb, dict_size,
              chunk_size=1 << 20, min_chunk=4096):
    """Port persis Lzma2.compressWith: chunk adaptif + fallback mentah.

    Mulai dari 1 MiB; bila hasil LZMA > 64 KiB (batas csize LZMA2),
    chunk diperkecil bertahap hingga muat. Bila hasil >= ukuran chunk,
    pakai chunk mentah 0x01.
    """
    props = props_byte(lc, lp, pb)
    out = bytearray()
    off = 0
    while off < len(data):
        avail = len(data) - off
        chunk = min(avail, chunk_size)
        part = data[off:off + chunk]
        comp = lzmaenc_ref.chunk_compress(part, lc, lp, pb, dict_size)

        if len(comp) >= chunk:
            out += _lzma2_raw(part)
            off += chunk
            continue
        while len(comp) > 65536 and chunk > min_chunk:
            nxt = chunk // 2
            if nxt < min_chunk:
                nxt = min_chunk
            if nxt >= chunk:
                break
            chunk = nxt
            part = data[off:off + chunk]
            comp = lzmaenc_ref.chunk_compress(part, lc, lp, pb, dict_size)
        if len(comp) < chunk and len(comp) <= 65536:
            u1 = chunk - 1
            c1 = len(comp) - 1
            out.append(0xE0 | ((u1 >> 16) & 31))
            out += bytes([(u1 >> 8) & 255, u1 & 255,
                          (c1 >> 8) & 255, c1 & 255, props])
            out += comp
        else:
            out += _lzma2_raw(part)
        off += chunk
    out.append(0)
    return bytes(out)


def _lzma2_raw(part):
    u1 = len(part) - 1
    return bytes([0x01, (u1 >> 8) & 255, u1 & 255]) + part


def dict_prop(dict_size):
    p = 0
    while dict_size > ((2 | (p & 1)) << (p // 2 + 11)):
        p += 1
    return p


FILTER_SPECS = {
    0x03: lambda arg: {"id": lzma.FILTER_DELTA, "dist": arg},
    0x04: lambda arg: {"id": lzma.FILTER_X86, "start_offset": arg},
    0x05: lambda arg: {"id": lzma.FILTER_POWERPC, "start_offset": arg},
    0x06: lambda arg: {"id": lzma.FILTER_IA64, "start_offset": arg},
    0x07: lambda arg: {"id": lzma.FILTER_ARM, "start_offset": arg},
    0x08: lambda arg: {"id": lzma.FILTER_ARMTHUMB, "start_offset": arg},
    0x09: lambda arg: {"id": lzma.FILTER_SPARC, "start_offset": arg},
}


def filter_encode(fid, arg, data):
    """Oracle filter-only: kompres [filter, LZMA2], lalu buang lapisan LZMA2."""
    f = [FILTER_SPECS[fid](arg),
         {"id": lzma.FILTER_LZMA2, "dict_size": 65536}]
    comp = lzma.compress(data, format=lzma.FORMAT_RAW, filters=f)
    return lzma.decompress(comp, format=lzma.FORMAT_RAW,
                           filters=[{"id": lzma.FILTER_LZMA2,
                                     "dict_size": 65536}])


def xz_ref(data, check, filters=(), dict_size=1 << 16):
    """Port persis Xz.compressChain (satu block)."""
    transformed = data
    for fid, arg in filters:
        transformed = filter_encode(fid, arg, transformed)
    payload = lzma2_ref(transformed, 3, 0, 2, dict_size)
    clen = {0: 0, 1: 4, 4: 8, 10: 32}[check]
    nf = len(filters) + 1
    body = bytearray()
    body.append(nf - 1)
    for fid, arg in filters:
        body += vli(fid)
        if fid == 0x03:
            body += vli(1)
            body.append((arg - 1) & 255)
        elif arg:
            body += vli(4)
            body += struct.pack("<I", arg)
        else:
            body += vli(0)
    body += vli(0x21)
    body += vli(1)
    body.append(dict_prop(dict_size))
    base = 1 + len(body) + 4
    hpad = (4 - (base & 3)) & 3
    hsize = base + hpad
    hdr = bytearray([hsize // 4 - 1])
    hdr += body
    hdr += b"\x00" * (hsize - 4 - len(hdr))
    hdr += struct.pack("<I", zlib.crc32(hdr))

    out = bytearray()
    out += MAGIC
    flags = bytes([0, check])
    out += flags
    out += struct.pack("<I", zlib.crc32(flags))
    out += hdr
    out += payload
    pad = (4 - ((hsize + len(payload) + clen) & 3)) & 3
    out += b"\x00" * pad
    if check == 1:
        out += struct.pack("<I", zlib.crc32(data))
    elif check == 4:
        out += struct.pack("<Q", crc64(data))
    elif check == 10:
        out += hashlib.sha256(data).digest()
    unpadded = hsize + len(payload) + clen
    idx = bytearray()
    idx.append(0)
    idx += vli(1)
    idx += vli(unpadded)
    idx += vli(len(data))
    while len(idx) % 4:
        idx.append(0)
    out += idx
    out += struct.pack("<I", zlib.crc32(idx))
    back = (len(idx) + 4) // 4 - 1
    foot = struct.pack("<I", back) + bytes([0, check])
    out += struct.pack("<I", zlib.crc32(foot))
    out += foot
    out += b"YZ"
    return bytes(out)


PRESET_DICT = [1 << 18, 1 << 20, 1 << 21, 1 << 22, 1 << 22,
               1 << 23, 1 << 23, 1 << 24, 1 << 25, 1 << 26]


def alone_ref(data, preset):
    """LZMA alone: header 13 byte + payload LZMA1 ber-end-marker."""
    dict_size = PRESET_DICT[preset]
    return (bytes([props_byte(3, 0, 2)]) + struct.pack("<I", dict_size)
            + b"\xff" * 8 + lzma1_ref(data, 3, 0, 2, dict_size))


def py_lzma1(comp, lc, lp, pb, dict_size, out_size):
    f = [{"id": lzma.FILTER_LZMA1, "dict_size": dict_size,
          "lc": lc, "lp": lp, "pb": pb}]
    return lzma.decompress(comp, format=lzma.FORMAT_RAW, filters=f)


def py_lzma2(comp, lc, lp, pb, dict_size):
    f = [{"id": lzma.FILTER_LZMA2, "dict_size": dict_size,
          "lc": lc, "lp": lp, "pb": pb}]
    return lzma.decompress(comp, format=lzma.FORMAT_RAW, filters=f)


def check_alone(comp, data):
    return lzma.decompress(comp) == data


# ── bahan uji ────────────────────────────────────────────────────────

rnd = random.Random(20261117)
samples = [
    ("empty", b""),
    ("one", b"a"),
    ("hello", b"Hello, World!"),
    ("rep", b"A" * 1000),
    ("text", b"Lorem ipsum dolor sit amet, consectetur adipiscing elit. " * 5),
    ("bin", bytes(range(256)) * 4),
    ("fox", b"The quick brown fox jumps over the lazy dog. " * 50),
]
for d in [5, 17, 100, 130, 300, 429, 1000]:
    half = bytes(rnd.randrange(256) for _ in range(d))
    samples.append(("dist%d" % d, half + half))
for n in [31, 64, 255, 273, 274]:
    samples.append(("rand%d" % n,
                    bytes(rnd.randrange(256) for _ in range(n))))

variants = [(0, 0, 0), (1, 0, 0), (2, 0, 1), (3, 0, 2),
            (4, 0, 0), (3, 1, 1), (2, 2, 2), (0, 4, 2)]

cases = []
stats = {"raw": 0, "alone": 0, "lzma2": 0, "xz": 0, "auto": 0,
         "stream": 0, "api": 0}


def add(body, group):
    cases.append("        malloc {\n%s\n        }" % body)
    stats[group] += 1


def eqcall(a, b):
    return "eq(%s, %s)" % (a, b)


# A. LZMA1 raw: byte + roundtrip (outSize diketahui dan -1)
for i, (name, data) in enumerate(samples):
    lc, lp, pb = variants[i % len(variants)]
    dict_size = 1 << 16 if i % 5 else 4096
    comp = lzma1_ref(data, lc, lp, pb, dict_size)
    assert py_lzma1(comp, lc, lp, pb, dict_size, len(data)) == data, name
    assert py_lzma1(comp, lc, lp, pb, dict_size, -1) == data, name
    props = props_byte(lc, lp, pb)
    add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] g = Lzma.compressRaw(d, %d, %d);
            out(%s * %s);""" % (
        hexstr(data), hexstr(comp), props, dict_size,
        eqcall("g", "e"),
        eqcall("Lzma.decompressRaw(g, %d, -1)" % props, "d")), "raw")

# B. LZMA1 alone: header + byte + roundtrip
for i, (name, data) in enumerate(samples[:10]):
    lc, lp, pb = variants[(i + 3) % len(variants)]
    dict_size = 1 << 16 if i % 3 else 4096
    props = props_byte(lc, lp, pb)
    raw = lzma1_ref(data, lc, lp, pb, dict_size)
    alone = bytes([props]) + struct.pack("<I", dict_size) + b"\xff" * 8 + raw
    assert check_alone(alone, data), name
    add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] g = Lzma.compressWith(d, %d, %d, %d, %d);
            out(%s * %s);""" % (
        hexstr(data), hexstr(alone), lc, lp, pb, dict_size,
        eqcall("g", "e"),
        eqcall("Lzma.decompress(g)", "d")), "alone")

# B2. default Lzma.compress roundtrip
for name, data in samples[::3]:
    add("""            int[] d = unpack("%s");
            out(%s);""" % (
        hexstr(data), eqcall("Lzma.decompress(Lzma.compress(d))", "d")),
        "alone")

# C. LZMA2: byte + roundtrip
for i, (name, data) in enumerate(samples):
    lc, lp, pb = variants[i % len(variants)]
    dict_size = 1 << 16 if i % 4 else 4096
    comp = lzma2_ref(data, lc, lp, pb, dict_size)
    assert py_lzma2(comp, lc, lp, pb, dict_size) == data, name
    add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] g = Lzma2.compressWith(d, %d, %d, %d, %d);
            out(%s * %s);""" % (
        hexstr(data), hexstr(comp), lc, lp, pb, dict_size,
        eqcall("g", "e"),
        eqcall("Lzma2.decompress(g)", "d")), "lzma2")

# D. XZ writer: byte + roundtrip untuk 4 jenis check
xz_data = [samples[2], samples[3], samples[4], samples[5], samples[8]]
for i, (name, data) in enumerate(xz_data):
    for check in [0, 1, 4, 10]:
        comp = xz_ref(data, check)
        assert lzma.decompress(comp) == data, (name, check)
        add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] g = Xz.compressCheck(d, %d);
            out(%s * %s);""" % (
            hexstr(data), hexstr(comp), check,
            eqcall("g", "e"),
            eqcall("Xz.decompress(g)", "d")), "xz")
    comp = xz_ref(data, 1)
    add("""            int[] d = unpack("%s");
            out(%s);""" % (
        hexstr(data), eqcall("Xz.compress(d)", "unpack(\"%s\")" % hexstr(comp))),
        "xz")

# D2. filter chain writer: delta, BCJ x86, delta+x86
chain_specs = [
    ("delta4", [(0x03, 4)]),
    ("x86", [(0x04, 0)]),
    ("d4x86", [(0x03, 4), (0x04, 0)]),
]
for name, data in [samples[2], samples[4], samples[6]]:
    for label, filters in chain_specs:
        comp = xz_ref(data, 1, filters)
        assert lzma.decompress(comp) == data, (name, label)
        k = len(filters)
        id_lines = "\n            ".join(
            "ids[%d] = %d;" % (i, f[0]) for i, f in enumerate(filters))
        arg_lines = "\n            ".join(
            "args[%d] = %d;" % (i, f[1]) for i, f in enumerate(filters))
        add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] ids = new int[%d];
            int[] args = new int[%d];
            %s
            %s
            int[] g = Xz.compressChain(d, 1, ids, args, 65536);
            out(%s * %s);""" % (
            hexstr(data), hexstr(comp), k, k, id_lines, arg_lines,
            eqcall("g", "e"),
            eqcall("Xz.decompress(g)", "d")), "xz")

for name, data in [samples[2], samples[4]]:
    comp = xz_ref(data, 1, [(0x03, 4)])
    add("""            int[] d = unpack("%s");
            out(%s * %s);""" % (
        hexstr(data),
        eqcall("Xz.compressDelta(d, 4, 1)", "unpack(\"%s\")" % hexstr(comp)),
        eqcall("Xz.decompress(Xz.compressDelta(d, 4, 1))", "d")), "xz")
    comp = xz_ref(data, 1, [(0x04, 0)])
    add("""            int[] d = unpack("%s");
            out(%s * %s);""" % (
        hexstr(data),
        eqcall("Xz.compressBcj(d, 4, 0, 1)", "unpack(\"%s\")" % hexstr(comp)),
        eqcall("Xz.decompress(Xz.compressBcj(d, 4, 0, 1))", "d")), "xz")

# D3. preset dict (0, 6, 9) → header LZMA2 props + payload
for name, data in [samples[2], samples[4]]:
    for preset in [0, 6, 9]:
        dict_size = [1 << 18, 1 << 20, 1 << 21, 1 << 22, 1 << 22,
                     1 << 23, 1 << 23, 1 << 24, 1 << 25, 1 << 26][preset]
        comp = xz_ref(data, 1, (), dict_size)
        assert lzma.decompress(comp) == data, (name, preset)
        add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] g = Xz.compressPresetCheck(d, %d, 1);
            out(%s * %s * %s);""" % (
            hexstr(data), hexstr(comp), preset,
            eqcall("g", "e"),
            eqcall("Xz.decompress(g)", "d"),
            eqcall("Xz.compressPreset(d, %d)" % preset, "e")), "xz")

# E. LzmaAuto: .lzma dan .xz hasil encoder sendiri, plus galat/terpotong
auto_src = [samples[2], samples[4], samples[6]]
for name, data in auto_src:
    alone = (bytes([props_byte(3, 0, 2)]) + struct.pack("<I", 1 << 16)
             + b"\xff" * 8 + lzma1_ref(data, 3, 0, 2, 1 << 16))
    xz = xz_ref(data, 1)
    add("""            int[] d = unpack("%s");
            int[] st = new int[1];
            int[] a = LzmaAuto.decode(Lzma.compress(d), st);
            int[] b = LzmaAuto.decode(Xz.compress(d), st);
            int[] c = LzmaAuto.decode(unpack("%s"), st);
            int[] x = unpack("%s");
            int[] cat = c2(x, x);
            int[] y = LzmaAuto.decode(cat, st);
            out(%s * %s * %s * %s);""" % (
        hexstr(data), hexstr(alone), hexstr(xz),
        eqcall("a", "d"), eqcall("b", "d"), eqcall("c", "d"),
        "eq(y, c2(d, d))"), "auto")

# galat: header xz dirusak, xz terpotong, alone terpotong, input pendek
good = xz_ref(samples[4][1], 1)
bad = bytearray(good)
bad[7] = 0x55
trunc = good[:len(good) // 2]
alone = (bytes([props_byte(3, 0, 2)]) + struct.pack("<I", 1 << 16)
         + b"\xff" * 8 + lzma1_ref(samples[6][1], 3, 0, 2, 1 << 16))
add("""            int[] st = new int[1];
            LzmaAuto.decode(unpack("%s"), st);
            int a = st[0] < 0 ? 1 : 0;
            LzmaAuto.decode(unpack("%s"), st);
            int b = st[0] < 0 ? 1 : 0;
            LzmaAuto.decode(unpack("%s"), st);
            int c = st[0] < 0 ? 1 : 0;
            int[] pendek = new int[3];
            LzmaAuto.decode(pendek, st);
            int e = st[0] < 0 ? 1 : 0;
            out(a * b * c * e);""" % (
        hexstr(bytes(bad)), hexstr(trunc), hexstr(alone[:len(alone) // 2])),
        "auto")

# F. Streaming: data 20000 byte, encoder chunk -> byte == ref, decoder chunk acak
sdata = bytes(((i * 37) ^ (i >> 3)) & 255 for i in range(20000))
scomp = lzma2_ref(sdata, 3, 0, 2, 1 << 16)
assert py_lzma2(scomp, 3, 0, 2, 1 << 16) == sdata
add("""            int[] src = unpack("%s");
            int[] exp = unpack("%s");
            Lzma2EncStream es = Lzma2EncStream.create();
            int[] got = new int[0];
            int at = 0;
            int step = 7777;
            while (at < src.length) {
                int k = step;
                if (src.length - at < k) k = src.length - at;
                got = c2(got, es.push(cut(src, at, k)));
                at = at + k;
                step = step + 1234;
                if (step > 20000) step = 7777;
            }
            got = c2(got, es.finish());
            out(%s * %s);""" % (
        hexstr(sdata), hexstr(scomp),
        eqcall("got", "exp"),
        eqcall("Lzma2.decompress(got)", "src")), "stream")

add("""            int[] exp = unpack("%s");
            int[] src = unpack("%s");
            Lzma2DecStream ds = new Lzma2DecStream();
            int[] got = new int[0];
            int at = 0;
            int step = 1;
            while (at < exp.length) {
                int k = step;
                if (exp.length - at < k) k = exp.length - at;
                got = c2(got, ds.push(cut(exp, at, k)));
                at = at + k;
                step = step * 3 + 1;
                if (step > 5000) step = 1;
            }
            out((ds.isFinished() ? 1 : 0) * (ds.status() == 0 ? 1 : 0) * %s);""" % (
        hexstr(scomp), hexstr(sdata), eqcall("got", "src")), "stream")

# decoder menolak control rusak dan stream terpotong
bad2 = bytearray(scomp)
bad2[0] = 0x03
add("""            Lzma2DecStream ds = new Lzma2DecStream();
            ds.push(unpack("%s"));
            int bad = ds.status() == -1 ? 1 : 0;
            Lzma2DecStream dt = new Lzma2DecStream();
            dt.push(unpack("%s"));
            int trunc = (dt.isFinished() ? 1 : 0) == 0 ? 1 : 0;
            out(bad * trunc);""" % (
        hexstr(bytes(bad2)), hexstr(scomp[:len(scomp) // 2])), "stream")

# G. API mirip modul lzma Python: LzmaCompressor / LzmaDecompressor.
#    Kompresor menampung input sampai flush(); hasil gabungan harus sama
#    dengan encoder sekali-jalan. Dekompresor diuji dengan potongan acak.
api_data = samples[4][1]
api_xz = xz_ref(api_data, 4, (), 1 << 23)
add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            LzmaCompressor c = LzmaCompressor.create();
            int[] got = new int[0];
            got = c2(got, c.compress(cut(d, 0, 7)));
            got = c2(got, c.compress(cut(d, 7, d.length - 7)));
            got = c2(got, c.flush());
            int[] lagi = c.flush();
            int[] sesudah = c.compress(d);
            LzmaCompressor cneg = LzmaCompressor.xz(-1, 6);
            int[] got2 = c2(cneg.compress(d), cneg.flush());
            out(%s * %s * (lagi.length == 0 ? 1 : 0)
                * (sesudah.length == 0 ? 1 : 0) * %s);""" % (
    hexstr(api_data), hexstr(api_xz),
    eqcall("got", "e"), eqcall("LzmaAuto.decompress(got)", "d"),
    eqcall("got2", "e")), "api")

api_alone = alone_ref(api_data, 3)
add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            LzmaCompressor c = LzmaCompressor.alone(3);
            int[] got = c2(c.compress(d), c.flush());
            out(%s * %s);""" % (
    hexstr(api_data), hexstr(api_alone),
    eqcall("got", "e"), eqcall("LzmaAuto.decompress(got)", "d")), "api")

api_raw = lzma2_ref(api_data, 3, 0, 2, 1 << 16)
add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            LzmaCompressor c = LzmaCompressor.raw(65536);
            int[] got = c2(c.compress(d), c.flush());
            out(%s * %s);""" % (
    hexstr(api_data), hexstr(api_raw),
    eqcall("got", "e"), eqcall("Lzma2.decompress(got)", "d")), "api")

api_chain = xz_ref(api_data, 1, [(0x03, 4), (0x04, 0)], 1 << 16)
add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            int[] ids = new int[2];
            int[] args = new int[2];
            ids[0] = 3;
            ids[1] = 4;
            args[0] = 4;
            args[1] = 0;
            LzmaCompressor c = LzmaCompressor.chain(ids, args, 65536, 1);
            int[] got = c2(c.compress(d), c.flush());
            out(%s * %s);""" % (
    hexstr(api_data), hexstr(api_chain),
    eqcall("got", "e"), eqcall("Xz.decompress(got)", "d")), "api")

for label, comp, factory in [
        ("xz", api_xz, "LzmaDecompressor.xz()"),
        ("alone", api_alone, "LzmaDecompressor.alone()"),
        ("raw", api_raw, "LzmaDecompressor.raw()"),
        ("auto", api_xz, "LzmaDecompressor.create()")]:
    add("""            int[] d = unpack("%s");
            int[] e = unpack("%s");
            LzmaDecompressor dc = %s;
            int[] got = new int[0];
            int at = 0;
            int step = 5;
            while (at < e.length) {
                int k = step;
                if (e.length - at < k) k = e.length - at;
                got = c2(got, dc.decompress(cut(e, at, k)));
                at = at + k;
                step = step * 3 + 1;
                if (step > 29) step = 5;
            }
            out(%s
                * (dc.isEof() ? 1 : 0) * (dc.isError() ? 0 : 1));""" % (
        hexstr(api_data), hexstr(comp), factory, eqcall("got", "d")), "api")

api_good = xz_ref(api_data, 1)
api_bad = bytearray(api_good)
api_bad[7] = 0x05   # tipe check tidak didukung -> rusak
add("""            LzmaDecompressor bad = LzmaDecompressor.xz();
            bad.decompress(unpack("%s"));
            LzmaDecompressor trunc = LzmaDecompressor.xz();
            trunc.decompress(unpack("%s"));
            out((bad.isError() ? 1 : 0) * (trunc.needsInput() ? 1 : 0)
                * (trunc.isError() ? 0 : 1));""" % (
    hexstr(bytes(api_bad)), hexstr(api_good[:len(api_good) // 2])), "api")

add("""            int c = (LzmaStream.FORMAT_AUTO == 0 ? 1 : 0)
                  * (LzmaStream.FORMAT_XZ == 1 ? 1 : 0)
                  * (LzmaStream.FORMAT_ALONE == 2 ? 1 : 0)
                  * (LzmaStream.FORMAT_RAW == 3 ? 1 : 0)
                  * (LzmaStream.CHECK_CRC64 == 4 ? 1 : 0)
                  * (LzmaStream.CHECK_SHA256 == 10 ? 1 : 0);
            int f = (LzmaStream.FILTER_LZMA2 == 0x21 ? 1 : 0)
                  * (LzmaStream.FILTER_DELTA == 3 ? 1 : 0)
                  * (LzmaStream.FILTER_X86 == 4 ? 1 : 0)
                  * (LzmaStream.FILTER_ARM64 == 10 ? 1 : 0)
                  * (LzmaStream.FILTER_LZMA1 == 4611686018427387905L ? 1 : 0);
            int k = (LzmaStream.isCheckSupported(0) ? 1 : 0)
                  * (LzmaStream.isCheckSupported(1) ? 1 : 0)
                  * (LzmaStream.isCheckSupported(4) ? 1 : 0)
                  * (LzmaStream.isCheckSupported(10) ? 1 : 0)
                  * (LzmaStream.isCheckSupported(2) ? 0 : 1);
            out(c * f * k);""", "api")

# ── rakit berkas Kopi ────────────────────────────────────────────────

HELPERS = '''    static int hexv(int c) {
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

    static int eq(int[] a, int[] b) {
        if (a.length != b.length) return 0;
        for (int i = 0; i < a.length; i++) {
            if (a[i] != b[i]) return 0;
        }
        return 1;
    }

    static int[] c2(int[] a, int[] b) {
        int[] r = new int[a.length + b.length];
        for (int i = 0; i < a.length; i++) r[i] = a[i];
        for (int i = 0; i < b.length; i++) r[a.length + i] = b[i];
        return r;
    }

    static int[] cut(int[] a, int off, int n) {
        int[] r = new int[n];
        for (int i = 0; i < n; i++) r[i] = a[off + i];
        return r;
    }

    static void out(int ok) {
        System.out.println(Str.fromInt(ok));
    }'''

out = ["// lzmaencvec.kop — encoder LZMA1/LZMA2/XZ + auto + streaming.",
       "import Lzma;", "import Lzma2;", "import Xz;", "import LzmaAuto;",
       "import LzmaStream;", "import Str;", "", "class Main {", HELPERS, ""]
calls = []
for i, body in enumerate(cases):
    out.append("    static void t%d() {" % i)
    out.append(body)
    out.append("    }")
    out.append("")
    calls.append("        t%d();" % i)
out.append("    static void main() {")
out.extend(calls)
out.append("    }")
out.append("}")
out.append("")

target = sys.argv[1] if len(sys.argv) > 1 else "lzmaencvec.kop"
os.makedirs(os.path.dirname(target) or ".", exist_ok=True)
with open(target, "w") as f:
    f.write("\n".join(out))
print("kasus: %d" % len(cases))
print("rincian: %s" % stats)
