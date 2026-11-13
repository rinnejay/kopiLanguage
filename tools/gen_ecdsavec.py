#!/usr/bin/env python3
"""gen_ecdsavec.py — vektor uji ECDSA P-256 dari RFC 6979 A.2.5.

Nilai r/s/k/x/Ux/Uy diambil langsung dari RFC 6979 (vektor resmi),
sehingga uji tidak bergantung implementasi python apa pun.
"""
import sys

# ── RFC 6979 A.2.5, kurva NIST P-256, SHA-256 ──────────────────────────
X_HEX = "C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721"
UX_HEX = "60FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6"
UY_HEX = "7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299"
# SHA-256, message = "sample"
R1_HEX = "EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716"
S1_HEX = "F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8"
# SHA-256, message = "test"
R2_HEX = "F1ABB023518351CD71D881567B1EA663ED3EFCF6C5132B354F28D3B0B7D38367"
S2_HEX = "019F4113742A2B14BD25926B49C649155F267E60D3814B4C0CC84250E46F0083"

MSG_SAMPLE = "73616d706c65"   # "sample"
MSG_TEST = "74657374"         # "test"

MS = MSG_SAMPLE
MT = MSG_TEST


def kop_arr(name, hexstr):
    b = bytes.fromhex(hexstr)
    body = "    static int[] %s() {\n" % name
    body += "        int[] v = new int[%d];\n" % len(b)
    for i, x in enumerate(b):
        body += "        v[%d] = %d;\n" % (i, x)
    body += "        return v;\n    }\n"
    return body


def main():
    kop_path = sys.argv[1]
    ref_path = sys.argv[2]
    src = f"""import Ecdsa;
import BigInt;
import Cipher;
import Str;

// vektor RFC 6979 A.2.5 (P-256, SHA-256).
// tiap tahap dibungkus malloc<64mb> agar arena dibebaskan: perkalian titik
// & modInv besar mengalokasi banyak; default 32 MB habis bila tak dibebaskan.
class Main {{
    public static void main(String[] args) {{
        malloc<64mb> {{
            int[] pub = Ecdsa.pubKey(dBig());
            System.out.println(Cipher.hexenc(pub));
        }}
        malloc<64mb> {{
            System.out.println(Cipher.hexenc(Ecdsa.sign(dBig(), Cipher.hexdec("{MS}"))));
        }}
        malloc<64mb> {{
            System.out.println(Cipher.hexenc(Ecdsa.sign(dBig(), Cipher.hexdec("{MT}"))));
        }}
        // verifikasi: tanda tangan sah
        malloc<64mb> {{
            int[] pub = Ecdsa.pubKey(dBig());
            int[] s1 = Ecdsa.sign(dBig(), Cipher.hexdec("{MS}"));
            System.out.println(Str.fromBool(Ecdsa.verify(pub, Cipher.hexdec("{MS}"), s1)));
        }}
        // pesan salah
        malloc<64mb> {{
            int[] pub = Ecdsa.pubKey(dBig());
            int[] s1 = Ecdsa.sign(dBig(), Cipher.hexdec("{MS}"));
            System.out.println(Str.fromBool(Ecdsa.verify(pub, Cipher.hexdec("{MT}"), s1)));
        }}
        // tanda tangan dirusak (bit terakhir s dibalik)
        malloc<64mb> {{
            int[] pub = Ecdsa.pubKey(dBig());
            int[] bad = Ecdsa.sign(dBig(), Cipher.hexdec("{MS}"));
            bad[63] = (bad[63] ^ 1) & 255;
            System.out.println(Str.fromBool(Ecdsa.verify(pub, Cipher.hexdec("{MS}"), bad)));
        }}
        // kunci publik salah
        malloc<64mb> {{
            int[] pub = Ecdsa.pubKey(dBig());
            pub[0] = (pub[0] ^ 1) & 255;
            int[] s1 = Ecdsa.sign(dBig(), Cipher.hexdec("{MS}"));
            System.out.println(Str.fromBool(Ecdsa.verify(pub, Cipher.hexdec("{MS}"), s1)));
        }}
    }}

    /* skalar privat sebagai BigInt (limb) — pubKey/sign menerima BigInt */
    static int[] dBig() {{
        return BigInt.fromBytes(dvec());
    }}

"""
    src += kop_arr("dvec", X_HEX) + "}\n"
    open(kop_path, "w").write(src)

    ref = [
        (UX_HEX + UY_HEX).lower(),
        (R1_HEX + S1_HEX).lower(),
        (R2_HEX + S2_HEX).lower(),
        "true", "false", "false", "false",
    ]
    open(ref_path, "w").write("\n".join(ref) + "\n")
    print("wrote", kop_path, "dan", ref_path)


if __name__ == "__main__":
    main()
