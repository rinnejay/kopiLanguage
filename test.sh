#!/bin/sh
# test.sh — verifikasi konsistensi backend vm & a64 terhadap referensi
cd "$(dirname "$0")"
T=${TMPDIR:-/data/data/com.termux/files/usr/tmp/opencode}
mkdir -p "$T/kopi"
PASS=0; FAIL=0

check() { # nama file_kop
    name="$1"; src="$2"
    for tgt in vm a64; do
        out="$T/kopi/$name.$tgt.out"
        case $tgt in
          vm)  ./kopicc "$src" -t vm  -o "$T/kopi/$name.kbc" && ./kopivm "$T/kopi/$name.kbc" > "$out" ;;
          a64) ./kopicc "$src" -t a64 -o "$T/kopi/$name.s"   && cc "$T/kopi/$name.s"  -o "$T/kopi/$name.bin" && "$T/kopi/$name.bin" > "$out" ;;
        esac
        if [ $? -ne 0 ]; then
            echo "FAIL  $name [$tgt]: gagal build/execute"; FAIL=$((FAIL+1)); continue
        fi
        if diff -q "$out" "$T/kopi/$name.ref" >/dev/null 2>&1; then
            echo "PASS  $name [$tgt]"; PASS=$((PASS+1))
        else
            echo "FAIL  $name [$tgt]: output tidak cocok"; diff "$T/kopi/$name.ref" "$out" | head -5; FAIL=$((FAIL+1))
        fi
    done
}

printf '42\n1 ganjil; fakt= 1\n2 genap; fakt= 2\n3 ganjil; fakt= 6\n4 genap; fakt= 24\n5 ganjil; fakt= 120\n6 genap; fakt= 720\n7 ganjil; fakt= 5040\n8 genap; fakt= 40320\n9 ganjil; fakt= 362880\n10 terakhir; fakt= 3628800\nfib(20) = 6765\n303\n404\n505\n1 + 2 = 555\n' > "$T/kopi/demo.ref"
check demo examples/demo.kop

printf '42 == 42\n' > "$T/kopi/first.ref"
check first examples/first.kop

printf 'Halo dunia dari Kopi VM and compiler!\n' > "$T/kopi/hello.ref"
check hello examples/hello.kop

# aritmetika & booleans
cat > "$T/kopi/misc.kop" <<'EOF'
class Main {
    public static void main(String[] args) {
        int a = 1000000;
        System.out.println(a * 1234);
        System.out.println(-a / 7);
        System.out.println(-a % 7);
        System.out.println(2 * 2 == 4);
        System.out.println(!(5 > 3));
        System.out.println((1 < 2) && (3 < 4));
        System.out.println((1 > 2) || (3 < 4));
    }
}
EOF
printf '1234000000\n-142857\n-1\n1\n0\n1\n1\n' > "$T/kopi/misc.ref"
check misc "$T/kopi/misc.kop"

# import / multi-file: program utama memanggil library keranjang.kop
printf 'total belanja    : 45000\nongkir (17km)    : 12000\nsetelah diskon   : 45000\ngrand total      : 57000\n' > "$T/kopi/belanja.ref"
check belanja examples/belanja.kop

# import NamaClass (tanpa path) — dua file ke CLI tanpa pernyataan import
cat > "$T/kopi/labib.kop" <<'EOF'
class Labib {
    static int duaKali(int x) { return x * 2; }
}
EOF
cat > "$T/kopi/utama.kop" <<'EOF'
import Labib;

class Main {
    public static void main(String[] args) {
        System.out.println(Labib.duaKali(21));
    }
}
EOF
printf '42\n' > "$T/kopi/multi.ref"
check multi "$T/kopi/utama.kop"

# library standar lib/ — import Matematika/Angka dengan pencarian lib kopicc
printf 'abs(-5)      = 5\ngcd(12,18)   = 6\nlcm(4,6)     = 12\nakar(100)    = 10\nmax(7,-3)    = 7\nprima(97)    = 1\nprima(96)    = 0\nfaktorial(9) = 362880\nfib(11)      = 89\nbalik(12345) = 54321\ndigit(907)   = 16\n42 biner     = 101010\n' > "$T/kopi/pakai_lib.ref"
check pakai_lib examples/pakai_lib.kop

# ++ -- += dst: desugar statement-level
cat > "$T/kopi/inc.kop" <<'EOF'
class Main {
    static void main(String[] args) {
        int x = 5;
        x++;
        x--;
        x += 3;
        System.out.println(x);
        int s = 0;
        for (int i = 1; i <= 5; i++) { s += i; }
        System.out.println(s);
    }
}
EOF
printf '8\n15\n' > "$T/kopi/inc.ref"
check inc "$T/kopi/inc.kop"

# lambda (fn) — capture snapshot by-value, fn sebagai argumen, lambda inline
cp examples/lambda.kop "$T/kopi/lambda.kop"
printf '30\n21\n38\n42\n42\n15\n1\n1\n0\n' > "$T/kopi/lambda.ref"
check lambda "$T/kopi/lambda.kop"

# lambda di target x64: cek sintaks assembly (tidak dieksekusi di aarch64)
./kopicc examples/lambda.kop -t x64 -o "$T/kopi/lambda64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/lambda64.s" -o /dev/null
if [ $? -eq 0 ]; then echo "PASS  lambda [x64]"; PASS=$((PASS+1));
else echo "FAIL  lambda [x64]: sintaks assembly"; FAIL=$((FAIL+1)); fi

# array int[] dari arena memori (bounds check runtime)
printf '81\n285\n11\n0\n' > "$T/kopi/array.ref"
check array examples/array.kop

# pustaka Crypto: aritmetika murni (fnv1a, xorStream round-trip, caesar, PRNG)
cat > "$T/kopi/cryptot.kop" <<'EOF'
import Crypto;

class Main {
    public static void main(String[] args) {
        int[] data = new int[8];
        data[0] = 104; data[1] = 97; data[2] = 108; data[3] = 111;
        data[4] = 10; data[5] = 20; data[6] = 30; data[7] = 40;
        System.out.println(Crypto.fnv1a(data, 8));
        int[] key = new int[3];
        key[0] = 7; key[1] = 42; key[2] = 200;
        System.out.println(Crypto.arrEq(data, Crypto.xorStream(Crypto.xorStream(data, key), key)));
        System.out.println(Crypto.arrEq(data, Crypto.uncaesar(Crypto.caesar(data, 3), 3)));
        int s = 123456789;
        for (int i = 0; i < 5; i++) s = Crypto.nextRand(s);
        System.out.println(s);
        System.out.println(Crypto.bitxor(102, 51));
    }
}
EOF
printf '929486015\n1\n1\n1845549155\n85\n' > "$T/kopi/cryptot.ref"
check cryptot "$T/kopi/cryptot.kop"

# pustaka Arrays: sort, binSearch, find, count, arrEq
cat > "$T/kopi/arrtest.kop" <<'EOF'
import Arrays;

class Main {
    public static void main(String[] args) {
        int[] a = new int[6];
        a[0] = 40; a[1] = 10; a[2] = 30; a[3] = 10; a[4] = 50; a[5] = 20;
        System.out.println(Arrays.sum(a));
        System.out.println(Arrays.max(a));
        System.out.println(Arrays.find(a, 50));
        int[] s = Arrays.sort(a);
        System.out.println(s[0]);
        System.out.println(s[5]);
        System.out.println(Arrays.binSearch(s, 30));
        System.out.println(Arrays.binSearch(s, 55));
        System.out.println(Arrays.count(a, 10));
        System.out.println(Arrays.arrEq(s, s) + Arrays.arrEq(s, a));
    }
}
EOF
printf '160\n50\n4\n10\n50\n3\n-1\n2\n1\n' > "$T/kopi/arrtest.ref"
check arrtest "$T/kopi/arrtest.kop"

# scoping per-blok + multi-deklarasi: var loop boleh dipakai ulang antar blok
cat > "$T/kopi/scope.kop" <<'EOF'
class Scope {
    public static void main(String[] args) {
        int kel = 0;
        for (int i = 0; i < 5; i++) kel += i;
        for (int i = 0; i < 3; i++) kel += 10;      // i boleh dipakai ulang
        { int i = 99; kel += i; }                    // blok telanjang: scope sendiri
        int prob = 0;
        if (kel > 0) { int i = 100; prob = 55; }
        else { int i = 0; prob = 0; }                // else: scope sendiri (shadow OK)
        prob += 1;
        int a = 0, b = 1, c = 2;                     // multi-deklarasi satu baris
        System.out.println(kel);
        System.out.println(prob);
        System.out.println(a + b + c);
    }
}
EOF
printf '139\n56\n3\n' > "$T/kopi/scope.ref"
check scope "$T/kopi/scope.kop"

# sintaks x64 untuk fitur array + library (tidak dieksekusi di aarch64)
cp examples/array.kop "$T/kopi/array.kop"
pas64=1
for t in array cryptot arrtest; do
    ./kopicc "$T/kopi/$t.kop" -t x64 -o "$T/kopi/${t}64.s" && \
    cc -target x86_64-linux-gnu -c "$T/kopi/${t}64.s" -o /dev/null || pas64=0
done
if [ $pas64 -eq 1 ]; then echo "PASS  library [x64]"; PASS=$((PASS+1));
else echo "FAIL  library [x64]: sintaks assembly"; FAIL=$((FAIL+1)); fi

echo "----"
echo "hasil: $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ]
