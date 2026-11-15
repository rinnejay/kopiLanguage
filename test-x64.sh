#!/bin/sh
# test-x64.sh — jalankan backend native di host x86-64 dan bandingkan
# outputnya dengan VM (acuan semantik suite).
#
# Kenapa begini: di host aarch64 backend x64 hanya bisa dirakit, tidak
# dijalankan. Skrip ini menutup celah itu di CI (runner x86-64) tanpa
# menduplikasi 2000 baris test.sh: VM dipakai sebagai oracle, dan
# program yang keluarannya tidak deterministik (waktu, acak, jaringan,
# path Termux) otomatis dilewati.
#
# Pakai:
#   sh test-x64.sh                 # target x64 (default, untuk CI)
#   KOPI_X64_TARGET=a64 sh test-x64.sh   # validasi harness di host ARM
#
# Hasil: PASS bila output native == output VM.

cd "$(dirname "$0")" || exit 1

TGT=${KOPI_X64_TARGET:-x64}
T=${TMPDIR:-/tmp}/kopi-$TGT
mkdir -p "$T"
rm -f "$T"/*.out "$T"/*.vm "$T"/*.bin 2>/dev/null

PASS=0; FAIL=0; SKIP=0
FAILED=""
SKIPPED=""

# Daftar uji yang memang tidak boleh dijalankan di harness ini
# (butuh server/port tertentu, atau hasilnya bergantung platform).
skip_list() {
    f="test/x64-skip.txt"
    [ -f "$f" ] || return 1
    sed 's/#.*//; s/[[:space:]]//g' "$f" | grep -qx "$1"
}

note_skip() {
    SKIP=$((SKIP+1)); SKIPPED="$SKIPPED $1"
    [ -n "$KOPI_X64_VERBOSE" ] && echo "SKIP  $1: $2"
    return 0
}

# compile <src> <target> <out>  → 0 bila kopicc sukses
compile() {
    timeout 120 ./kopicc "$1" -t "$2" -o "$3" >/dev/null 2>&1
}

run_case() { # nama src
    name=$1; src=$2
    [ -f "$src" ] || { note_skip "$name" "sumber tidak ada"; return; }
    skip_list "$name" && { note_skip "$name" "ada di x64-skip.txt"; return; }

    # 1. oracle: VM harus bisa compile + jalan
    compile "$src" vm "$T/$name.kbc" || { note_skip "$name" "VM gagal compile"; return; }
    timeout 60 ./kopivm "$T/$name.kbc" > "$T/$name.vm" 2>/dev/null \
        || { note_skip "$name" "VM gagal jalan"; return; }

    # 2. saring non-determinisme: jalankan VM dua kali
    timeout 60 ./kopivm "$T/$name.kbc" > "$T/$name.vm2" 2>/dev/null \
        || { note_skip "$name" "VM gagal jalan (run 2)"; return; }
    cmp -s "$T/$name.vm" "$T/$name.vm2" || { note_skip "$name" "keluaran tidak deterministik"; return; }

    # 3. target native: compile + rakit + jalan
    compile "$src" "$TGT" "$T/$name.s" \
        || { echo "FAIL  $name [$TGT]: compile"; FAIL=$((FAIL+1)); FAILED="$FAILED $name"; return; }
    cc "$T/$name.s" -o "$T/$name.bin" 2>/dev/null \
        || { echo "FAIL  $name [$TGT]: rakit"; FAIL=$((FAIL+1)); FAILED="$FAILED $name"; return; }
    timeout 120 "$T/$name.bin" > "$T/$name.out" 2>/dev/null \
        || { echo "FAIL  $name [$TGT]: eksekusi"; FAIL=$((FAIL+1)); FAILED="$FAILED $name"; return; }

    # 4. bandingkan dengan VM
    if cmp -s "$T/$name.vm" "$T/$name.out"; then
        PASS=$((PASS+1))
    else
        echo "FAIL  $name [$TGT]: output != VM"
        diff "$T/$name.vm" "$T/$name.out" 2>/dev/null | head -4 | sed 's/^/        /'
        FAIL=$((FAIL+1)); FAILED="$FAILED $name"
    fi
}

for src in test/*.kop examples/*.kop; do
    [ -f "$src" ] || continue
    name=$(basename "$src" .kop)
    run_case "$name" "$src"
done

# proyek besar: 54 file saling import, keluaran 102 baris
run_case proyekbesar examples/proyekbesar/ProyekBesar.kop

echo "----"
echo "hasil [$TGT]: $PASS pass, $FAIL fail, $SKIP dilewati"
[ -n "$FAILED" ] && echo "gagal:$FAILED"
[ -n "$KOPI_X64_VERBOSE" ] && [ -n "$SKIPPED" ] && echo "dilewati:$SKIPPED"
[ "$FAIL" -eq 0 ]
