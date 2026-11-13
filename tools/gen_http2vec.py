#!/usr/bin/env python3
"""gen_http2vec.py — oracle python (hpack) + program uji .kop untuk HPACK.

Vektor:
  1. Huffman RFC 7541 Appendix B: string di-encode pustaka hpack
     (diverifikasi ulang dengan decode_huffman pustaka), didekode Kopi
     hufDec → byte harus sama.
  2. Blok header respons HPACK penuh: hpack.Encoder (Huffman +
     incremental indexing + tabel dinamis) untuk DUA respons berurutan
     pada satu encoder — decodeBlock Kopi harus menghasilkan :status
     yang benar untuk keduanya (menguji tabel dinamis lintas blok).
  3. blok + indexed static (0x88 :status 200) murni.

Pemakaian: tools/gen_http2vec.py <out.kop> <out.ref>
"""
import sys

from hpack import Encoder
from hpack.huffman_table import decode_huffman


class Bits:
    def __init__(self):
        self.b = bytearray()
        self.cur = 0
        self.n = 0

    def put(self, val, ln):
        for i in range(ln - 1, -1, -1):
            self.cur = (self.cur << 1) | ((val >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.b.append(self.cur)
                self.cur = 0
                self.n = 0

    def finish(self):
        if self.n:
            pad = 8 - self.n
            self.cur = ((self.cur << pad) | ((1 << pad) - 1))
            self.b.append(self.cur)
            self.cur = 0
            self.n = 0
        return bytes(self.b)


def load_huffman_table():
    """kode Huffman dari tools/hpack_huffman.tsv (RFC 7541 App. B);
    validitas tiap vektor diverifikasi dengan decode_huffman pustaka
    hpack (dekode independen)."""
    import os
    tsv = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hpack_huffman.tsv")
    codes = {}
    for line in open(tsv):
        if line.startswith("#") or not line.strip():
            continue
        sym, hx, l = line.split("\t")
        s = int(sym)
        if s <= 255:
            codes[s] = (int(hx, 16), int(l))
    assert len(codes) == 256, len(codes)
    return codes


def main():
    kop_path, ref_path = sys.argv[1], sys.argv[2]

    # ── 1. huffman: encode via pohon HUFFMAN_TABLE hpack ──
    codes = load_huffman_table()
    assert len(codes) == 256, len(codes)
    veks = [
        b"www.example.com",
        b"no-cache",
        b"custom-key",
        b"private",
        b"Mon, 21 Oct 2013 20:13:21 GMT",
        b"application/xhtml+xml",
        b"a",
        b"",
    ]
    hlines = []
    for s in veks:
        bits = Bits()
        for ch in s:
            c, l = codes[ch]
            bits.put(c, l)
        enc = bits.finish()
        assert decode_huffman(enc) == s, s
        hlines.append((enc.hex(), len(enc) * 8, s))

    # ── 2. blok respons HPACK (hpack.Encorer = Huffman + dt dinamis) ──
    enc = Encoder()
    h1 = [(":status", "200"), ("server", "kopi-oracle"),
          ("content-type", "text/plain"), ("content-length", "5")]
    b1 = bytes(enc.encode(h1))
    h2 = [(":status", "304"), ("server", "kopi-oracle"),
          ("content-type", "text/html"), ("content-length", "0"),
          ("etag", "abc123")]
    b2 = bytes(enc.encode(h2))
    st1 = 200
    st2 = 304

    # ── 3. indexed statik murni: :status 200 = 0x88 ──
    b3 = bytes([0x88])
    st3 = 200

    src = '''/* http2vec.kop — vektor HPACK/Huffman (oracle python hpack).
 * HASIL GENERATOR — tools/gen_http2vec.py; jangan edit manual.
 */
import Http2;
import Hash;
import Cipher;
import Str;

class Main {
    public static void main(String[] args) {
        int[] hctx = new int[94300];
        Http2.initTables(hctx);
'''
    for i, (hx, bl, s) in enumerate(hlines):
        src += '        vek(hctx, Cipher.hexdec("%s"), %d, "%s");\n' % (hx, bl, s.decode("ascii").replace("\\", "\\\\").replace('"', '\\"'))
    src += '        blk(hctx, Cipher.hexdec("%s"), %d);\n' % (b1.hex(), st1)
    src += '        blk(hctx, Cipher.hexdec("%s"), %d);\n' % (b2.hex(), st2)
    src += '        blk(hctx, Cipher.hexdec("%s"), %d);\n' % (b3.hex(), st3)
    src += '''    }

    static void vek(int[] hctx, int[] data, int bits, String want) {
        int[] ok = new int[1];
        int[] out = Http2.hufDec(hctx, data, 0, bits, ok);
        String got = Net.mkstr(out);
        System.out.println(Str.fromInt(ok[0]) + Str.eq(got, want) ? "1" : "0");
```
'''
    # — bentuk di atas salah; tulis ulang dengan pola aman —
    src = '''/* http2vec.kop — vektor HPACK/Huffman (oracle python hpack).
 * HASIL GENERATOR — tools/gen_http2vec.py; jangan edit manual.
 */
import Http2;
import Hash;
import Cipher;
import Str;

class Main {
    public static void main(String[] args) {
        int[] hctx = new int[94300];
        Http2.initTables(hctx);
'''
    for i, (hx, bl, s) in enumerate(hlines):
        w = s.decode("ascii").replace("\\", "\\\\").replace('"', '\\"')
        src += '        vek(hctx, Cipher.hexdec("%s"), %d, "%s");\n' % (hx, bl, w)
    src += '        blk(hctx, Cipher.hexdec("%s"), %d);\n' % (b1.hex(), st1)
    src += '        blk(hctx, Cipher.hexdec("%s"), %d);\n' % (b2.hex(), st2)
    src += '        blk(hctx, Cipher.hexdec("%s"), %d);\n' % (b3.hex(), st3)
    src += '''    }

    static void vek(int[] hctx, int[] data, int bits, String want) {
        int[] ok = new int[1];
        int[] out = Http2.hufDec(hctx, data, 0, bits, ok);
        String got = Net.mkstr(out);
        if (ok[0] == 1 && Str.eq(got, want)) {
            System.out.println("1");
        } else {
            System.out.println("0");
        }
    }

    static void blk(int[] hctx, int[] data, int want) {
        int[] ok = new int[1];
        Http2.decodeBlock(hctx, data, ok);
        int st = Http2.respStatus(hctx);
        if (ok[0] == 1 && st == want) {
            System.out.println("1");
        } else {
            System.out.println("0 " + Str.fromInt(ok[0]) + " " + Str.fromInt(st)
                + " want=" + Str.fromInt(want));
        }
    }
}
'''
    ref = []
    for _ in hlines:
        ref.append("1")
    for _ in range(3):
        ref.append("1")
    open(kop_path, "w").write(src)
    open(ref_path, "w").write("\n".join(ref) + "\n")
    print("gen_http2vec: %d huffman + %d blok → %s" % (len(hlines), 3, kop_path))


if __name__ == "__main__":
    main()
