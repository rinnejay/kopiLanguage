#!/usr/bin/env python3
"""gen_http2.py — generator tabel HPACK untuk lib/net/Http2Tabel.kop.

Pemakaian: tools/gen_http2.py [keluaran.kop]

Sumber data:
  - tools/hpack_huffman.tsv : kode Huffman kanonik 257 simbol
    (RFC 7541 Appendix B; TSV hasil parse RFC, lihat header berkas)
  - tabel statik 61 entri (RFC 7541 Appendix A) tertanam di bawah.

Memvalidasi sifat kanonik (kode berurutan per panjang, firstCode
rekursif), lalu menulis Http2Tabel.kop — jangan edit manual.
"""
import os
import sys

# RFC 7541 Appendix A — tabel statik HPACK (61 entri)
STATIC_TABLE = [
    (":authority", ""),
    (":method", "GET"),
    (":method", "POST"),
    (":path", "/"),
    (":path", "/index.html"),
    (":scheme", "http"),
    (":scheme", "https"),
    (":status", "200"),
    (":status", "204"),
    (":status", "206"),
    (":status", "304"),
    (":status", "400"),
    (":status", "404"),
    (":status", "500"),
    ("accept-charset", ""),
    ("accept-encoding", "gzip, deflate"),
    ("accept-language", ""),
    ("accept-ranges", ""),
    ("accept", ""),
    ("access-control-allow-origin", ""),
    ("age", ""),
    ("allow", ""),
    ("authorization", ""),
    ("cache-control", ""),
    ("content-disposition", ""),
    ("content-encoding", ""),
    ("content-language", ""),
    ("content-length", ""),
    ("content-location", ""),
    ("content-range", ""),
    ("content-type", ""),
    ("cookie", ""),
    ("date", ""),
    ("etag", ""),
    ("expect", ""),
    ("expires", ""),
    ("from", ""),
    ("host", ""),
    ("if-match", ""),
    ("if-modified-since", ""),
    ("if-none-match", ""),
    ("if-range", ""),
    ("if-unmodified-since", ""),
    ("last-modified", ""),
    ("link", ""),
    ("location", ""),
    ("max-forwards", ""),
    ("proxy-authenticate", ""),
    ("proxy-authorization", ""),
    ("range", ""),
    ("referer", ""),
    ("refresh", ""),
    ("retry-after", ""),
    ("server", ""),
    ("set-cookie", ""),
    ("strict-transport-security", ""),
    ("transfer-encoding", ""),
    ("user-agent", ""),
    ("vary", ""),
    ("via", ""),
    ("www-authenticate", ""),
]


def parse_tsv(path):
    code = [0] * 257
    ln = [0] * 257
    n = 0
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        sym, hx, l = line.split("\t")
        s = int(sym)
        assert 0 <= s < 257
        code[s] = int(hx, 16)
        ln[s] = int(l)
        n += 1
    assert n == 257, "tabel Huffman tidak lengkap: %d/257" % n
    assert max(code) < (1 << 31)
    fc = 0
    for L in range(5, 31):
        cs = sorted(code[s] for s in range(257) if ln[s] == L)
        if cs:
            assert cs[0] == fc, "firstCode L=%d" % L
            for i in range(1, len(cs)):
                assert cs[i] == cs[i - 1] + 1, "gap L=%d" % L
        fc = (fc + sum(1 for s in range(257) if ln[s] == L)) << 1
    return code, ln


def gen_tabel(code, ln, st):
    names = "".join(x[0] for x in st)
    vals = "".join(x[1] for x in st)
    code_hex = b"".join(code[i].to_bytes(4, "big") for i in range(257)).hex()
    len_hex = bytes(ln).hex()
    no, o = [], 0
    for nm, _ in st:
        no.append(o)
        o += len(nm)
    no.append(o)
    vo, o = [], 0
    for _, v in st:
        vo.append(o)
        o += len(v)
    vo.append(o)
    no_hex = b"".join(x.to_bytes(2, "big") for x in no).hex()
    vo_hex = b"".join(x.to_bytes(2, "big") for x in vo).hex()

    def lit(h):
        return '        "%s"' % h

    return """/* Http2Tabel.kop — tabel HPACK (RFC 7541) untuk Http2.kop.
 *
 * HASIL GENERATOR — jangan edit manual: tools/gen_http2.py
 *   - hufCode/hufLen : kode Huffman kanonik, 257 simbol (Appendix B)
 *   - stName/stVal   : tabel statik 61 entri (Appendix A), ASCII
 *   - stNameOff/stValOff : offset u16 per entri (62 = sentinel akhir)
 */
import Cipher;

class Http2Tabel {

    static int[] hufCode() {
        int[] b = Cipher.hexdec(
""" + lit(code_hex) + """);
        int[] out = new int[257];
        for (int i = 0; i < 257; i++) {
            out[i] = ((b[4 * i] & 255) << 24) | ((b[4 * i + 1] & 255) << 16)
                | ((b[4 * i + 2] & 255) << 8) | (b[4 * i + 3] & 255);
        }
        return out;
    }

    static int[] hufLen() {
        return Cipher.hexdec(
""" + lit(len_hex) + """);
    }

    static int[] stName() {
        return Cipher.hexdec(
""" + lit(names.encode().hex()) + """);
    }

    static int[] stNameOff() {
        int[] b = Cipher.hexdec(
""" + lit(no_hex) + """);
        int[] out = new int[62];
        for (int i = 0; i < 62; i++) {
            out[i] = ((b[2 * i] & 255) << 8) | (b[2 * i + 1] & 255);
        }
        return out;
    }

    static int[] stVal() {
        return Cipher.hexdec(
""" + lit(vals.encode().hex()) + """);
    }

    static int[] stValOff() {
        int[] b = Cipher.hexdec(
""" + lit(vo_hex) + """);
        int[] out = new int[62];
        for (int i = 0; i < 62; i++) {
            out[i] = ((b[2 * i] & 255) << 8) | (b[2 * i + 1] & 255);
        }
        return out;
    }
}
"""


def main():
    outp = sys.argv[1] if len(sys.argv) > 1 else "lib/net/Http2Tabel.kop"
    tsv = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hpack_huffman.tsv")
    code, ln = parse_tsv(tsv)
    assert len(STATIC_TABLE) == 61
    open(outp, "w").write(gen_tabel(code, ln, STATIC_TABLE))
    print("gen_http2: %s (Huffman 257 simbol, statik 61 entri)" % outp)


if __name__ == "__main__":
    main()
