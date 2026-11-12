#!/usr/bin/env python3
"""gen_iovec.py — oracle python untuk test/iovec.kop (Reader/Writer/Uuid/Csv)."""
import csv
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, "test", "data", "berkas.txt")

out = []

# ── Reader ────────────────────────────────────────────────────────────
data = open(FIX, "rb").read()
text = data.decode()
lines = text.replace("\r\n", "\n").split("\n")
if lines and lines[-1] == "":
    lines = lines[:-1]
out.append("1")                      # Reader.open valid (file ada)
out += lines                         # 3 baris
out.append("")                      # readLine ke-4 → EOF (string kosong)
out.append(str(len(data)))           # readAll().length
out.append(data.hex())               # isi hex (CR/LF tidak merusak baris)
out.append("5")                      # readBytes(5).length
out.append(data[:5].hex())           # hex 5 byte pertama
out.append("0")                      # file tidak ada → valid() false

# ── Writer ────────────────────────────────────────────────────────────
out.append("11")                     # "halo\ndunia\n"
out.append(b"halo\ndunia\n".hex())

# ── Uuid ──────────────────────────────────────────────────────────────
out.append("36")
out.append("1")
out.append("4")
out.append("0")
out.append("00000000-0000-0000-0000-000000000000")
out.append("1")

# ── Csv ───────────────────────────────────────────────────────────────
row = next(csv.reader(io.StringIO('a,"b,c","d""e",f')))
out.append(str(len(row)))
out += row
out.append(",".join(row) if '"' not in ",".join(row) else 'a,"b,c","d""e",f')
out.append("2")                      # countRows
out.append("2")                      # lines().length
out.append("2")                      # rowLengths[0]
out.append("3")                      # rowLengths[1]
out.append("1")                      # parseRow("") → 1 field
out.append("1")                      # parseRow("satu") → 1 field

print("\n".join(out))
