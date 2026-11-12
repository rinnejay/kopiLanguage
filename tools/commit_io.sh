#!/bin/bash
# commit_io.sh — rapikan, docs, suite, commit & push v1.29.
set -u
cd ~/kopi || exit 1
LOG=$TMPDIR/opencode/commitio.log
: > "$LOG"
exec >> "$LOG" 2>&1

echo "== patch roadmap =="
python3 tools/patch_roadmap_io.py

echo "== bersihkan skrip bantu =="
rm -f tools/patch_field_array.py tools/patch_field_index.py tools/patch_io_meta.py \
      tools/patch_io_fixes.py tools/patch_io_null.py tools/patch_csv_chr.py \
      tools/patch_uuid_hex.py tools/patch_roadmap_io.py tools/patch_io_batch*.py \
      tools/run_fieldarray.sh tools/run_io_batch.sh tools/run_io_batch2.sh \
      tools/run_io_batch3.sh tools/run_rdprobe.sh tools/run_io.sh \
      tools/bisect_reader.py tools/bisect2.py tools/bisect3.py
rm -f tools/run_*.sh
rm -rf tools/__pycache__

echo "== make docs =="
make docs

echo "== sh test.sh =="
sh test.sh
echo "test_exit=$?"

echo "== commit =="
git status --short
git add -A
git commit -F - <<'MSG'
v1.29: field instance bertipe array + lib/io (Reader, Writer) + Uuid + Csv

Bahasa (compiler, ketiga backend):
- field instance boleh bertipe array: int[], long[], String[]
- penugasan indeks pada field: p.field[i] = v
- Parser statement recognizes "p.field[i] = v" (sebelumnya hanya
  p.field = v dan a[i] = v)
- Slot field sudah 64-bit, jadi array (pointer) aman disimpan

Pustaka baru:
- lib/io/Reader.kop: pembaca berkas berbuffer (baris/byte/seluruh isi)
- lib/io/Writer.kop: penulis berkas berbuffer (teks/baris/byte, flush)
- lib/core/Uuid.kop: UUID v4 + format/parse/validasi
- lib/core/Csv.kop: baca & tulis CSV (RFC 4180)

Dokumentasi & uji:
- kategori IO baru di referensi API (docs/api-io.md)
- test/iovec.kop + oracle tools/gen_iovec.py
- roadmap diperbarui (field array ✅)
MSG
git log --oneline -2
git push origin main
echo "push_exit=$?"
git status --short
echo "== selesai =="
