#!/bin/sh
# bench/bench.sh — benchmark a64/VM dengan pembanding C -O0 dan -O2.
#
#   sh bench/bench.sh            # semua benchmark
#   sh bench/bench.sh sum fib    # sebagian
#
# Setiap program dijalankan 3x dan diambil waktu tercepat (ms). Keluaran
# ketiga mesin dibandingkan; perbedaan ditandai MISMATCH supaya optimasi
# codegen tidak diam-diam mengubah semantik.
set -u
cd "$(dirname "$0")/.." || exit 1
T=${TMPDIR:-/data/data/com.termux/files/usr/tmp/opencode}/kopi-bench
mkdir -p "$T" || exit 1

if [ $# -gt 0 ]; then
    BENCHES="$*"
else
    BENCHES="sum fib array str"
fi

now_ms() { date +%s%N | cut -c1-13; }

# waktu terbaik dari 3 percobaan, dalam ms
best_ms() { # perintah...
    best=999999999
    i=0
    while [ $i -lt 3 ]; do
        a=$(date +%s%N)
        "$@" > "$T/last.out" 2>/dev/null
        rc=$?
        b=$(date +%s%N)
        [ $rc -ne 0 ] && { echo -1; return; }
        d=$(( (b - a) / 1000000 ))
        [ "$d" -lt "$best" ] && best=$d
        i=$((i + 1))
    done
    echo "$best"
}

printf '%-8s %10s %10s %10s %10s   %8s %8s\n' \
       bench 'kopi-a64' 'kopi-vm' 'C -O0' 'C -O2' 'a64/O0' 'a64/O2'
printf '%-8s %10s %10s %10s %10s   %8s %8s\n' \
       ------ ---------- ---------- ---------- ---------- -------- --------

for b in $BENCHES; do
    src="bench/$b.kop"
    [ -f "$src" ] || { echo "bench: $src tidak ada"; continue; }

    ./kopicc "$src" -t a64 -o "$T/$b.s" 2> "$T/$b.err" &&
        cc -O2 "$T/$b.s" -o "$T/$b.a64" -lm 2>> "$T/$b.err" || {
            echo "$b: gagal build a64"; sed 's/^/    /' "$T/$b.err" | head -3; continue; }
    ./kopicc "$src" -t vm -o "$T/$b.kbc" 2>> "$T/$b.err" || {
            echo "$b: gagal build vm"; head -3 "$T/$b.err"; continue; }
    cc -O0 "bench/$b.c" -o "$T/$b.c0" 2>> "$T/$b.err" || {
            echo "$b: gagal build C -O0"; continue; }
    cc -O2 "bench/$b.c" -o "$T/$b.c2" 2>> "$T/$b.err" || {
            echo "$b: gagal build C -O2"; continue; }

    "$T/$b.a64" > "$T/$b.a64.out" 2>/dev/null; ra=$?
    ./kopivm "$T/$b.kbc" > "$T/$b.vm.out" 2>/dev/null; rv=$?
    "$T/$b.c2" > "$T/$b.c.out" 2>/dev/null; rc=$?
    mark=""
    if [ $ra -ne 0 ] || [ $rv -ne 0 ] || [ $rc -ne 0 ]; then
        mark="  (exit a64=$ra vm=$rv c=$rc)"
    elif ! cmp -s "$T/$b.a64.out" "$T/$b.vm.out" ||
         ! cmp -s "$T/$b.a64.out" "$T/$b.c.out"; then
        mark="  MISMATCH"
    fi

    ta=$(best_ms "$T/$b.a64")
    tv=$(best_ms ./kopivm "$T/$b.kbc")
    t0=$(best_ms "$T/$b.c0")
    t2=$(best_ms "$T/$b.c2")

    printf '%-8s %10s %10s %10s %10s   %8s %8s%s\n' \
        "$b" "$ta" "$tv" "$t0" "$t2" \
        "$(awk -v a="$ta" -v b="$t0" 'BEGIN{ if (b>0) printf "%.2fx", a/b; else print "-" }')" \
        "$(awk -v a="$ta" -v b="$t2" 'BEGIN{ if (b>0) printf "%.2fx", a/b; else print "-" }')" \
        "$mark"
done

echo
echo "ms, waktu terbaik dari 3x. Rasio >1 berarti Kopi lebih lambat dari C."
echo "artefak: $T"
