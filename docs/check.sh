#!/bin/sh
# check.sh — verifikasi contoh di dokumentasi (a64 + VM + sintaks x64).
set -e
cd "$(dirname "$0")/.."

python3 tools/gen_libref.py >/dev/null
python3 tools/md2html.py >/dev/null
python3 tools/check_docs.py

T="${TMPDIR:-/tmp}/kopi-docs"
mkdir -p "$T"

./kopicc docs/contoh/hello.kop -t a64 -o "$T/hello.s"
cc "$T/hello.s" -o "$T/hello.bin"
test "$("$T/hello.bin")" = "Halo dunia dari Kopi!"

./kopicc docs/contoh/tour.kop -t a64 -o "$T/tour.s"
cc "$T/tour.s" -o "$T/tour.bin"
"$T/tour.bin" > "$T/tour.a64.out"
diff -q "$T/tour.a64.out" docs/contoh/tour.ref

./kopicc docs/contoh/tour.kop -t vm -o "$T/tour.kbc"
./kopivm "$T/tour.kbc" > "$T/tour.vm.out"
diff -q "$T/tour.vm.out" docs/contoh/tour.ref

./kopicc docs/contoh/tour.kop -t x64 -o "$T/tour64.s"
cc -target x86_64-linux-gnu -c "$T/tour64.s" -o /dev/null
echo "docs OK"
