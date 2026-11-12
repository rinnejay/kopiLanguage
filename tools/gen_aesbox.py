#!/usr/bin/env python3
"""gen_aesbox: isi tabel S-box AES di lib/crypto/Aes.kop dari daftar terverifikasi.

Sumber daftar: tools/gen_aesvec.py (dicek terhadap FIPS-197 §C).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_aesvec import SBOX  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(ROOT, "lib", "crypto", "Aes.kop")

def main():
    src = open(PATH, encoding="utf-8").read()
    if "/*__SBOX_ROWS__*/" not in src:
        print("marker tidak ada (tabel sudah terisi?)")
        return
    rows = []
    for i in range(0, 256, 8):
        vals = ", ".join(str(v) for v in SBOX[i:i + 8])
        rows.append("        /* %3d */ t[%d] = %s;" % (i, i, vals))
        # satu baris berisi 8 indeks berurutan
        parts = []
        for j in range(8):
            parts.append("t[%d] = %d;" % (i + j, SBOX[i + j]))
        rows[-1] = "        " + " ".join(parts)
    src = src.replace("        /*__SBOX_ROWS__*/", "\n".join(rows))
    open(PATH, "w", encoding="utf-8").write(src)
    print("tabel S-box (%d entri) ditulis ke %s" % (len(SBOX), PATH))

if __name__ == "__main__":
    main()
