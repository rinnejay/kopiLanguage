#!/bin/sh
# test.sh — verifikasi konsistensi backend vm & a64 terhadap referensi
cd "$(dirname "$0")"
# bersihkan server uji sisa run yang mungkin meningkat diam-diam
pkill -f "python3 .*kopi/[r]fc192.py" 2>/dev/null
pkill -f "python3 .*kopi/[d]nstest.py" 2>/dev/null
pkill -f "python3 .*kopi/[p]osttest.py" 2>/dev/null
pkill -f "python3 -m http.server [9]911" 2>/dev/null
pkill -f "python3 .*kopi/[e]chosrv.py" 2>/dev/null
pkill -f "python3 .*kopi/[b]ulksrv.py" 2>/dev/null
pkill -f "python3 tools/[h]ttp_test_server.py" 2>/dev/null
pkill -f "python3 tools/[d]ns_test_server.py" 2>/dev/null
sleep 0.3
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

# ─── bitops: operator bitwise native & | ^ ~ << >> >>> + compound assign ─
printf '34\n119\n85\n-103\n48\n6\n-4\n15\n15\n3\n1024\n11\n10\n186\n48\n60\n195\n780\n48\n15\n1\n1\n42\n7\n5\n' > "$T/kopi/bitops.ref"
check bitops test/bitops.kop

# ─── hash: SHA-256/384/512 + HMAC-SHA256/512 (vektor FIPS 180-4 / RFC 4231) ─
python3 tools/gen_hashvec.py "$T/kopi/hashvec.kop" >/dev/null
printf '1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n' > "$T/kopi/hashvec.ref"
check hashvec "$T/kopi/hashvec.kop"

# ─── AES + AES-GCM (FIPS-197 / NIST SP 800-38D lewat vektor python) ───────
python3 tools/gen_aesvec.py "$T/kopi/aesvec.kop" >/dev/null
printf '1\n1\n1\n1\n1\n1\n' > "$T/kopi/aesvec.ref"
check aesvec "$T/kopi/aesvec.kop"

# ─── BLAKE2b + scrypt (RFC 7693 / RFC 7914 lewat vektor python) ──────────
python3 tools/gen_blakevec.py > "$T/kopi/blakevec.ref"
check blakevec test/blakevec.kop

# ─── Regex: mesin backtracking vs oracle python `re` ─────────────────────
python3 tools/gen_regexvec.py > "$T/kopi/regexvec.ref"
check regexvec test/regexvec.kop

# ─── BigInt: bilangan presisi sembarang vs oracle python int ──────────────
python3 tools/gen_bigintvec.py > "$T/kopi/bigintvec.ref"
check bigintvec test/bigintvec.kop

# ─── RSA: PKCS#1 v1.5 sign/verify/encrypt (backend native a64; modexp di
#     VM terlalu lambat untuk key 512-bit — lihat catatan di docs) ─────────
python3 tools/gen_rsavec.py "$T/kopi/rsavec.kop" "$T/kopi/rsavec.ref" >/dev/null
if ./kopicc "$T/kopi/rsavec.kop" -t a64 -o "$T/kopi/rsavec.s" && \
   cc "$T/kopi/rsavec.s" -o "$T/kopi/rsavec.bin" && \
   "$T/kopi/rsavec.bin" > "$T/kopi/rsavec.out"; then
  if diff -q "$T/kopi/rsavec.ref" "$T/kopi/rsavec.out" >/dev/null 2>&1; then
    echo "PASS  rsavec [a64]"; PASS=$((PASS+1))
  else
    echo "FAIL  rsavec [a64]: output tidak cocok"; FAIL=$((FAIL+1))
  fi
else
  echo "FAIL  rsavec [a64]: gagal build/execute"; FAIL=$((FAIL+1))
fi

# ─── ECDSA P-256 (RFC 6979 A.2.5; backend native a64 — skalar mult + modInv
#     besar di VM terlalu lambat) ───────────────────────────────────────────
python3 tools/gen_ecdsavec.py "$T/kopi/ecdsavec.kop" "$T/kopi/ecdsavec.ref" >/dev/null
if ./kopicc "$T/kopi/ecdsavec.kop" -t a64 -o "$T/kopi/ecdsavec.s" && \
   cc "$T/kopi/ecdsavec.s" -o "$T/kopi/ecdsavec.bin" && \
   "$T/kopi/ecdsavec.bin" > "$T/kopi/ecdsavec.out"; then
  if diff -q "$T/kopi/ecdsavec.ref" "$T/kopi/ecdsavec.out" >/dev/null 2>&1; then
    echo "PASS  ecdsavec [a64]"; PASS=$((PASS+1))
  else
    echo "FAIL  ecdsavec [a64]: output tidak cocok"; FAIL=$((FAIL+1))
  fi
else
  echo "FAIL  ecdsavec [a64]: gagal build/execute"; FAIL=$((FAIL+1))
fi

# ─── JWT RS256 (tanda tangan RSA; backend native a64) ─────────────────────
python3 tools/gen_jwtrs.py "$T/kopi/jwtrs.kop" "$T/kopi/jwtrs.ref" >/dev/null
if ./kopicc "$T/kopi/jwtrs.kop" -t a64 -o "$T/kopi/jwtrs.s" && \
   cc "$T/kopi/jwtrs.s" -o "$T/kopi/jwtrs.bin" && \
   "$T/kopi/jwtrs.bin" > "$T/kopi/jwtrs.out"; then
  if diff -q "$T/kopi/jwtrs.ref" "$T/kopi/jwtrs.out" >/dev/null 2>&1; then
    echo "PASS  jwtrs [a64]"; PASS=$((PASS+1))
  else
    echo "FAIL  jwtrs [a64]: output tidak cocok"; FAIL=$((FAIL+1))
  fi
else
  echo "FAIL  jwtrs [a64]: gagal build/execute"; FAIL=$((FAIL+1))
fi

# ─── X.509: parse sertifikat (fixture openssl; oracle gen_x509vec.py) ─────
python3 tools/gen_x509vec.py "$T/kopi/x509vec.kop" "$T/kopi/x509vec.ref" >/dev/null
check x509vec "$T/kopi/x509vec.kop"

# ─── lib/io (Reader, Writer) + Uuid + Csv (oracle gen_iovec.py) ────────────
python3 tools/gen_iovec.py > "$T/kopi/iovec.ref"
check iovec test/iovec.kop

# ─── Big: smoke + field penuh (ground truth python; gen_febig.py) ────────
printf '56088\n0\n1\n0\n' > "$T/kopi/bigsmoke.ref"
check bigsmoke test/bigsmoke.kop
python3 tools/gen_febig.py >/dev/null
check febig "$T/kopi/febig.kop"

# ─── X25519: vektor RFC 7748 §5.2 (2 buah) + §6.1 DH (4 buah) ────────────
python3 tools/gen_x25519.py "$T/kopi/curve25519.kop" >/dev/null
printf '1\n1\n1\n1\n1\n1\n' > "$T/kopi/curve25519.ref"
check curve25519 "$T/kopi/curve25519.kop"

# ─── KDF: HKDF RFC 5869 + ExpandLabel RFC 8446 (vektor python hmac) ──────
python3 tools/gen_kdf.py "$T/kopi/hkdf.kop" >/dev/null
printf '1\n1\n1\n1\n' > "$T/kopi/hkdf.ref"
check hkdf "$T/kopi/hkdf.kop"

# ─── ChaCha20: RFC 7539 §2.4.2 (sunscreen, key 00..1f, nonce 4a, ctr 1) ──
printf '1\n1\n' > "$T/kopi/stream.ref"
check stream test/stream.kop

# ─── Poly1305: RFC 7539 §2.5.2 + Appendix A.3 (8 vektor, oracle python) ──
if [ -f test/data/rfc7539.txt ]; then
    cp test/data/rfc7539.txt "$T/kopi/rfc7539.txt"
fi
if [ -f "$T/kopi/rfc7539.txt" ]; then
    N=$(python3 tools/gen_poly1305.py "$T/kopi/polyvec.kop" | sed -n 's/^kasus: \([0-9]*\).*/\1/p')
    python3 -c "import sys; sys.stdout.write('1\\n' * $N)" > "$T/kopi/polyvec.ref"
    check polyvec "$T/kopi/polyvec.kop"
else
    echo "SKIP  polyvec (teks RFC 7539 tidak ada)"
fi

# ─── Math/Digits/Stats/Matrix: oracle python (puluhan fungsi) ─────────────
N=$(python3 tools/gen_mathvec.py "$T/kopi/mathvec.kop" | sed -n 's/^kasus: \([0-9]*\).*/\1/p')
python3 -c "import sys; sys.stdout.write('1\\n' * $N)" > "$T/kopi/mathvec.ref"
check mathvec "$T/kopi/mathvec.kop"

# ─── tipe numerik baru: long/float/double/uint8..64 (oracle python) ──────
python3 tools/gen_numvec.py "$T/kopi/numvec.kop" "$T/kopi/numvec.ref" >/dev/null
check numvec "$T/kopi/numvec.kop"

# ─── array fungsional: forEach/map/filter/reduce + gula metode ───────────
cat > "$T/kopi/arrayfn.kop" <<'EOF'
import Arrays;

class Main {
    public static void main(String[] args) {
        int[] a = new int[5];
        for (int i = 0; i < 5; i++) a[i] = i + 1;

        Arrays.forEach(a, x => { System.out.println(x); });
        a.forEach(x => { System.out.println(x * 10); });

        int[] b = a.map(x => x + 1);
        Arrays.forEach(b, x => { System.out.println(x); });

        int[] ev = a.filter(x => x % 2 == 0);
        System.out.println(ev.length);
        Arrays.forEach(ev, x => { System.out.println(x); });

        System.out.println(a.reduce(0, (acc, x) => acc + x));
        System.out.println(a.any(x => x > 4));
        System.out.println(a.any(x => x > 9));
        System.out.println(a.all(x => x > 0));
        System.out.println(a.all(x => x > 2));
        System.out.println(a.countWhere(x => x % 2 == 1));
        System.out.println(a.findIndex(x => x == 3));
        System.out.println(a.sumBy(x => x * x));

        int batas = 3;
        System.out.println(a.countWhere(x => x > batas));

        a.forEachIndexed((i, x) => { System.out.println(i * 100 + x); });

        int[] r = Arrays.reverse(a);
        Arrays.forEach(r, x => { System.out.println(x); });
        int[] cp = Arrays.copy(a);
        System.out.println(Arrays.arrEq(cp, a));
    }
}
EOF
printf '1\n2\n3\n4\n5\n10\n20\n30\n40\n50\n2\n3\n4\n5\n6\n2\n2\n4\n15\n1\n0\n1\n0\n3\n2\n55\n2\n1\n102\n203\n304\n405\n5\n4\n3\n2\n1\n1\n' > "$T/kopi/arrayfn.ref"
check arrayfn "$T/kopi/arrayfn.kop"

# ─── array: return lintas-fungsi (besar), new int[0], identitas ──────────
cat > "$T/kopi/arrchk.kop" <<'EOF'
class Main {
    static int[] buat(int n) {
        int[] a = new int[n];
        for (int i = 0; i < n; i++) a[i] = i * 3;
        return a;
    }
    static int[] identitas(int[] x) { return x; }
    static int jumlah(int[] x) {
        int s = 0;
        for (int i = 0; i < x.length; i++) s = s + x[i];
        return s;
    }
    public static void main(String[] args) {
        int[] a = buat(300);
        System.out.println(a.length);
        System.out.println(a[0]);
        System.out.println(a[299]);
        System.out.println(a[150]);
        int[] b = identitas(a);
        System.out.println(b.length);
        System.out.println(jumlah(a));
        int[] z = new int[0];
        System.out.println(z.length);
        int[] one = buat(1);
        System.out.println(one.length);
        System.out.println(one[0]);
    }
}
EOF
printf '300\n0\n897\n450\n300\n134550\n0\n1\n0\n' > "$T/kopi/arrchk.ref"
check arrchk "$T/kopi/arrchk.kop"

# ─── JSON: literal, parse, akses, ubah, tulis ───────────────────────────
cat > "$T/kopi/jsontest.kop" <<'EOF'
import Json;

class Main {
    public static void main(String[] args) {
        int[] j = json { "nama": "budi", "umur": 20, "aktif": true, "nilai": [1, 2.5, null] };
        Json.line(j);
        System.out.println(Json.typeOf(j));
        System.out.println(Json.asInt(Json.get(j, "umur")));
        System.out.println(Json.isBool(Json.get(j, "aktif")));
        Json.line(Json.get(j, "nilai"));
        int[] n = Json.get(j, "nilai");
        System.out.println(Json.count(n));
        System.out.println(Json.asInt(Json.at(n, 0)));
        System.out.println(Json.asDouble(Json.at(n, 1)));
        System.out.println(Json.isNull(Json.at(n, 2)));

        int[] j2 = Json.put(j, "umur", Json.num("21"));
        Json.line(j2);
        int[] j3 = Json.add(Json.get(j2, "nilai"), Json.str("x"));
        Json.line(j3);
        int[] j4 = Json.parse("{ \"a\" : [ true, 1e3, {\"b\":\"c\\n\"} ], \"z\": null }");
        Json.line(j4);
        System.out.println(Json.asDouble(Json.at(Json.get(j4, "a"), 1)));
        Json.line(Json.parse("[]"));
        System.out.println(Json.parse("{rusak").length);
        Json.line(Json.del(j, "aktif"));
    }
}
EOF
printf '%s\n' \
 '{"nama":"budi","umur":20,"aktif":true,"nilai":[1,2.5,null]}' \
 '[1,2.5,null]' \
 '{"nama":"budi","umur":21,"aktif":true,"nilai":[1,2.5,null]}' \
 '[1,2.5,null,"x"]' \
 '{"a":[true,1e3,{"b":"c\n"}],"z":null}' \
 '[]' \
 '{"nama":"budi","umur":20,"nilai":[1,2.5,null]}' \
 '5' '20' '1' '3' '1' '2.5' '1' '1000' '0' > "$T/kopi/jsontest.ref"
check jsontest "$T/kopi/jsontest.kop"

# ─── proyek besar: 54 file saling import + fase memori ──────────────────
cp examples/proyekbesar/expected.out "$T/kopi/proyekbesar.ref"
check proyekbesar examples/proyekbesar/ProyekBesar.kop
./kopicc examples/proyekbesar/ProyekBesar.kop -t x64 -o "$T/kopi/proyekbesar64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/proyekbesar64.s" -o /dev/null \
    && echo "PASS  proyekbesar [x64]" || { echo "FAIL  proyekbesar [x64]"; FAIL=$((FAIL+1)); }

# ─── OS/file I/O + Str/Map/Set/Deque/Path/Time (v1.7) ───────────────────
rm -rf ostest_tmp1.txt ostest_tmp2.txt ostest_dir
cat > "$T/kopi/ostest.kop" <<'EOF'
import File;
import Bytes;

class Main {
    public static void main(String[] args) {
        String p1 = "ostest_tmp1.txt";
        int fd = File.create(p1);
        System.out.println(fd > 0);
        int[] data = Net.bytes("halo dunia");
        System.out.println(File.writeAll(fd, data));
        File.close(fd);

        fd = File.openRead(p1);
        System.out.println(fd > 0);
        int[] buf = new int[64];
        int n = File.readInto(fd, buf);
        System.out.println(n);
        System.out.println(Bytes.eq(Bytes.slice(buf, 0, n), data));
        System.out.println(File.size(fd));
        System.out.println(File.seekSet(fd, 5));
        int[] b2 = new int[16];
        int n2 = File.readInto(fd, b2);
        System.out.println(n2);
        System.out.println(Bytes.eq(Bytes.slice(b2, 0, n2), Net.bytes("dunia")));
        File.close(fd);

        String p2 = "ostest_tmp2.txt";
        System.out.println(File.renameTo(p1, p2));
        fd = File.openRead(p2);
        System.out.println(File.size(fd));
        File.close(fd);
        System.out.println(File.remove(p2));
        System.out.println(File.openRead(p2) < 0);

        String dir = "ostest_dir";
        int mr = File.mkdir(dir);
        System.out.println(mr == 0 || mr == -17);   /* baru atau sudah ada */
        String fp = "ostest_dir/f.txt";
        fd = File.create(fp);
        System.out.println(fd > 0);
        File.writeAll(fd, Net.bytes("abc"));
        File.close(fd);
        fd = File.openRead(fp);
        System.out.println(File.size(fd));
        File.close(fd);
        System.out.println(File.remove(fp));
    }
}
EOF
printf '%s\n' 1 0 1 10 1 10 5 5 1 0 10 0 1 1 1 3 0 > "$T/kopi/ostest.ref"
check ostest "$T/kopi/ostest.kop"
rm -rf ostest_tmp1.txt ostest_tmp2.txt ostest_dir

cat > "$T/kopi/textmap.kop" <<'EOF'
import Bytes;
import Map;
import Set;
import Deque;
import Path;
import Time;

class Main {
    public static void main(String[] args) {
        int[] s = Net.bytes("  Kopi Language  ");
        System.out.println(Bytes.len(s));
        System.out.println(Bytes.len(Bytes.trim(s)));
        System.out.println(Bytes.eq(Bytes.upper(Net.bytes("kopi")), Net.bytes("KOPI")));
        System.out.println(Bytes.indexOf(s, Net.bytes("Lang"), 0));
        System.out.println(Bytes.splitCount(Net.bytes("a,b,c"), 44));
        System.out.println(Bytes.eq(Bytes.splitAt(Net.bytes("a,b,c"), 44, 1), Net.bytes("b")));
        System.out.println(Bytes.toInt(Net.bytes("-42")));
        System.out.println(Bytes.len(Bytes.fromInt(12345)));
        System.out.println(Bytes.eq(Bytes.replaceSeq(Net.bytes("a-b-a"), Net.bytes("a"), Net.bytes("xy")), Net.bytes("xy-b-xy")));
        System.out.println(Bytes.eq(Bytes.padLeft(Net.bytes("7"), 3, 48), Net.bytes("007")));
        System.out.println(Bytes.utf8Count(Net.bytes("é")));

        int[] m = Map.make(128);
        for (int i = 0; i < 100; i++) Map.put(m, i * 7, i);
        System.out.println(Map.size(m));
        System.out.println(Map.get(m, 49));
        System.out.println(Map.has(m, 999));
        System.out.println(Map.remove(m, 49));
        System.out.println(Map.size(m));
        System.out.println(Map.getOr(m, 49, -1));
        int[] m2 = Map.grow(m);
        System.out.println(Map.size(m2));
        int[] acc = new int[1];
        Map.forEach(m2, (k, v) => { acc[0] = acc[0] + v; });
        System.out.println(acc[0]);

        int[] st = Set.make(8);
        Set.add(st, 1);
        Set.add(st, 2);
        Set.add(st, 2);
        Set.add(st, 3);
        System.out.println(Set.size(st));
        System.out.println(Set.has(st, 2));
        System.out.println(Set.remove(st, 2));
        System.out.println(Set.size(st));

        int[] d = Deque.make(4);
        Deque.pushBack(d, 1);
        Deque.pushBack(d, 2);
        Deque.pushFront(d, 0);
        System.out.println(Deque.size(d));
        System.out.println(Deque.popFront(d));
        System.out.println(Deque.popBack(d));
        System.out.println(Deque.size(d));

        System.out.println(Path.base("/a/b/c.txt") == "c.txt");
        System.out.println(Path.dir("/a/b/c.txt") == "/a/b");
        System.out.println(Path.ext("/a/b/c.txt") == "txt");
        System.out.println(Path.join("/a", "b") == "/a/b");

        System.out.println(Time.format(1700000000) == "2023-11-14 22:13:20");
        System.out.println(Time.weekday(1700000000));
        int[] c = Time.civil(0);
        System.out.println(c[0]);
        System.out.println(c[1]);
        System.out.println(c[2]);
        long t0 = Time.monoMs();
        Time.sleepMs(5);
        long dt = Time.monoMs() - t0;
        System.out.println(dt >= 0);
        System.out.println(Time.epoch() > 1600000000);
    }
}
EOF
printf '%s\n' 17 13 1 7 3 1 -42 5 1 1 1 100 7 0 1 99 -1 99 4943 3 1 1 2 3 0 2 1 1 1 1 1 1 2 1970 1 1 1 1 > "$T/kopi/textmap.ref"
check textmap "$T/kopi/textmap.kop"

for t in ostest textmap; do
    ./kopicc "$T/kopi/$t.kop" -t x64 -o "$T/kopi/${t}64.s" \
        && cc -target x86_64-linux-gnu -c "$T/kopi/${t}64.s" -o /dev/null \
        && echo "PASS  $t [x64]" || { echo "FAIL  $t [x64]"; FAIL=$((FAIL+1)); }
done

# ─── template string backtick `...{expr}...` (v1.7.1) ───────────────────
cat > "$T/kopi/tstrtest.kop" <<'EOF'
import Str;
import Bytes;

class Main {
    static int dua() { return 2; }

    static int[] sapa(int umur) {
        return `umur={umur}`;
    }

    public static void main(String[] args) {
        int umur = 20;
        long besar = 5000000000L;
        uint32 u = 4294967295U;
        uint64 uq = 18446744073709551615UL;
        boolean aktif = true;
        double pi = 3.5;
        float f = 1.25f;
        int[] nama = Net.bytes("budi");

        System.out.println(`Halo {nama}, umur {umur} aktif={aktif}`);
        System.out.println(`long={besar} u32={u} u64={uq}`);
        System.out.println(`pi={pi} f={f} neg={-7}`);
        System.out.println(`longm={-5000000000L}`);
        System.out.println(`d={0.1} t={1.0 / 3.0} f3={1.0f / 3.0f}`);
        System.out.println(`b={false} not={!true}`);
        System.out.println(`multi={1}{2}{3}`);
        System.out.println(`literal {{x}} \{y\} dan \`bt\` dollar=$`);
        System.out.println(sapa(umur));
        System.out.println(`nested {`inner {umur}`}`);
        System.out.println(`bytes={(nama)} expr={umur + dua()}`);
        System.out.print(`tanpa newline`);
        System.out.println("");
        System.out.println('kutip tunggal {tetap literal}');
        System.out.println("kutip ganda {tetap literal}");
        System.out.println(Str.len(`a{1}b`));
        int[] t = `x{2}y`;
        System.out.println(t.length);
        System.out.println(Bytes.eq(t, Net.bytes("x2y")));
    }
}
EOF
printf '%s\n' \
 'Halo budi, umur 20 aktif=true' \
 'long=5000000000 u32=4294967295 u64=18446744073709551615' \
 'pi=3.5 f=1.25 neg=-7' \
 'longm=-5000000000' \
 'd=0.1 t=0.333333 f3=0.333333' \
 'b=false not=false' \
 'multi=123' \
 'literal {x} {y} dan `bt` dollar=$' \
 'umur=20' \
 'nested inner 20' \
 'bytes=budi expr=22' \
 'tanpa newline' \
 'kutip tunggal {tetap literal}' \
 'kutip ganda {tetap literal}' \
 '3' '3' '1' > "$T/kopi/tstrtest.ref"
check tstrtest "$T/kopi/tstrtest.kop"
./kopicc "$T/kopi/tstrtest.kop" -t x64 -o "$T/kopi/tstrtest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/tstrtest64.s" -o /dev/null \
    && echo "PASS  tstrtest [x64]" || { echo "FAIL  tstrtest [x64]"; FAIL=$((FAIL+1)); }

# ─── String native: literal, concat, ==, length, indexing, interop ──────
rm -f strnative_tmp.txt strnative_net.txt
cat > "$T/kopi/strnative.kop" <<'EOF'
import Str;
import Json;
import File;

class Main {
    static String pair(String a, String b) { return a + b; }

    static String lab(int n) {
        return "n=" + Str.fromInt(n);
    }

    public static void main(String[] args) {
        String s = "Halo";
        String t = 'Dunia';
        System.out.println(s);
        System.out.println(t);
        System.out.println(s + " " + t);
        System.out.println(s == "Halo");
        System.out.println(s != "X");
        System.out.println(s.length);
        System.out.println(s[0]);

        String u = s + t;
        System.out.println(u.length);
        String v = Net.bytes("bytes");
        System.out.println(v);
        int[] w = Net.strbytes(u);
        System.out.println(w.length);
        System.out.println(Str.upper(s));
        System.out.println(pair("a", "b"));
        System.out.println(lab(7));
        System.out.println(`template {s} {u.length}`);

        String e = "";
        System.out.println(e.length);
        System.out.println(s == t);
        System.out.println("literal" + "" == "literal");

        int[] lit = Net.strbytes("abc");
        System.out.println(lit.length);
        System.out.println(Json.parse("{\"a\":1}").length > 0);
        int fd2 = Net.openb("strnative_net.txt", 577, 420);
        System.out.println(fd2 > 0);
        File.close(fd2);
        File.remove("strnative_net.txt");

        String path = "strnative_tmp.txt";
        int fd = File.create(path);
        File.writeAll(fd, "isi file");
        File.close(fd);
        fd = File.openRead(path);
        System.out.println(File.size(fd));
        File.close(fd);
        File.remove(path);
    }
}
EOF
printf '%s\n' 'Halo' 'Dunia' 'Halo Dunia' '1' '1' '4' '72' '9' 'bytes' '9' 'HALO' 'ab' 'n=7' 'template Halo 9' '0' '0' '1' '3' '1' '1' '8' > "$T/kopi/strnative.ref"
check strnative "$T/kopi/strnative.kop"
rm -f strnative_tmp.txt strnative_net.txt
./kopicc "$T/kopi/strnative.kop" -t x64 -o "$T/kopi/strnative64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/strnative64.s" -o /dev/null \
    && echo "PASS  strnative [x64]" || { echo "FAIL  strnative [x64]"; FAIL=$((FAIL+1)); }

# ─── AEAD: RFC 7539 §2.8.2 / RFC 8439 §2.8.2 (oracle python fail-hard) ───
python3 tools/gen_aead.py "$T/kopi/aeadvec.kop" >/dev/null
printf '1\n1\n1\n0\n' > "$T/kopi/aeadvec.ref"
check aeadvec "$T/kopi/aeadvec.kop"

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

# library standar lib/ — import Math/Digits dengan pencarian lib kopicc
printf 'abs(-5)       = 5\ngcd(12,18)    = 6\nlcm(4,6)      = 12\nsqrt(100)     = 10\nmax(7,-3)     = 7\nisPrime(97)   = 1\nisPrime(96)   = 0\nfactorial(9)  = 362880\nfib(11)       = 89\nreverse(12345)= 54321\ndigitSum(907) = 16\n42 biner      = 101010\n' > "$T/kopi/pakai_lib.ref"
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
cp test/bitops.kop  "$T/kopi/bitops.kop"
pas64=1
for t in array cryptot arrtest ciphertest netrfc netdns netget netpost netbulk bitops hashvec aesvec rsavec bigsmoke febig curve25519 hkdf stream polyvec aeadvec mathvec numvec arrayfn arrchk jsontest; do
    [ -f "$T/kopi/$t.x64.s" ] || continue
    ./kopicc "$T/kopi/$t.kop" -t x64 -o "$T/kopi/${t}64.s" && \
    cc -target x86_64-linux-gnu -c "$T/kopi/${t}64.s" -o /dev/null || pas64=0
done
# x64: syntax check modul kripto baru
for t in aesvec x509vec; do
    [ -f "$T/kopi/$t.kop" ] || continue
    ./kopicc "$T/kopi/$t.kop" -t x64 -o "$T/kopi/${t}64.s" && \
    cc -target x86_64-linux-gnu -c "$T/kopi/${t}64.s" -o /dev/null || pas64=0
done
if [ $pas64 -eq 1 ]; then echo "PASS  library [x64]"; PASS=$((PASS+1));
else echo "FAIL  library [x64]: sintaks assembly"; FAIL=$((FAIL+1)); fi

# ─── Net: builtin + pustaka Tcp/Http (uji loopback, server disedot sendiri) ───
mkdir -p "$T/kopi/www"
printf 'Halo Net Kopi!\n' > "$T/kopi/www/halo.txt"

cat > "$T/kopi/echosrv.py" <<'EOF'
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 9912)); s.listen(8)
while True:
    c, _ = s.accept()
    try:
        d = b""
        while len(d) < 4:
            chunk = c.recv(4096)
            if not chunk: break
            d += chunk
        c.sendall(b"ACK:" + d)
        c.shutdown(socket.SHUT_WR)
    except OSError:
        pass
    try: c.close()
    except OSError: pass
EOF
python3 "$T/kopi/echosrv.py" & ECHO_PID=$!
python3 -m http.server 9911 --bind 127.0.0.1 --directory "$T/kopi/www" & HTTP_PID=$!
sleep 1

# raw: builtin socket/connect/wrb/rdb + pustaka Tcp
cat > "$T/kopi/netraw.kop" <<'EOF'
import Tcp;

class Main {
    public static void main(String[] args) {
        int fd = Tcp.open(Tcp.loopback(), 9912);
        if (fd < 0) { System.out.println(fd); return; }
        System.out.println(Tcp.sendBuf(fd, Net.bytes("ping")));
        int[] buf = new int[100];
        int n = Tcp.recvAll(fd, buf);
        System.out.println(n);
        for (int i = 0; i < n; i++) { System.out.print(buf[i]); System.out.print(" "); }
        System.out.println("");
        Tcp.drop(fd);
    }
}
EOF
printf '0\n8\n65 67 75 58 112 105 110 103 \n' > "$T/kopi/netraw.ref"
check netraw "$T/kopi/netraw.kop"

# HTTP/1.0 GET via pustaka Http (status, panjang body, isi body)
cat > "$T/kopi/netget.kop" <<'EOF'
import Tcp;
import Http;

class Main {
    public static void main(String[] args) {
        String host = "127.0.0.1";
        String path = "/halo.txt";
        int[] buf = new int[8192];
        int n = Http.getInto(Tcp.loopback(), 9911, host, path, buf);
        if (n < 0) { System.out.println(n); return; }
        int[] resp = new int[n];
        for (int i = 0; i < n; i++) resp[i] = buf[i];
        System.out.println(Http.statusOf(resp));
        int[] b = Http.bodyOf(resp);
        System.out.println(b.length);
        for (int i = 0; i < b.length; i++) { System.out.print(b[i]); System.out.print(" "); }
        System.out.println("");
    }
}
EOF
printf '200\n15\n72 97 108 111 32 78 101 116 32 75 111 112 105 33 10 \n' > "$T/kopi/netget.ref"
check netget "$T/kopi/netget.kop"

# sintaks x64 untuk Net (tidak dieksekusi di aarch64)
./kopicc "$T/kopi/netget.kop" -t x64 -o "$T/kopi/netget64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/netget64.s" -o /dev/null
if [ $? -eq 0 ]; then echo "PASS  net [x64]"; PASS=$((PASS+1));
else echo "FAIL  net [x64]: sintaks assembly"; FAIL=$((FAIL+1)); fi

kill $ECHO_PID $HTTP_PID 2>/dev/null

# ─── Cipher: RC4/Vigenère/base64/hex/CRC-16/Adler-32 + bitwise Crypto ────
cat > "$T/kopi/ciphertest.kop" <<'EOF'
import Crypto;
import Cipher;

class Main {
    public static void main(String[] args) {
        // bitwise murni aritmetika (identitas A+B = (A⊕B)+2(A∧B))
        System.out.println(Crypto.bitand(102, 51));      // 34
        System.out.println(Crypto.bitor(102, 51));       // 119
        System.out.println(Crypto.bitnot(102));          // 2147483545
        System.out.println(Crypto.rotl31(1073741824, 1));// 1 (bit30 melorot)
        System.out.println(Crypto.shr31(65535, 8));      // 255
        System.out.println(Crypto.shl31(3, 31));         // 0
        System.out.println(Crypto.mask31(4));            // 15
        // CRC-16-CCITT & Adler-32: vektor terkenal
        System.out.println(Cipher.crc16(Net.bytes("123456789")));      // 10673 (0x29B1)
        System.out.println(Cipher.adler32(Net.bytes("Wikipedia")));    // 300286872
        // base64: vektor klasik "Man" → TWFu
        String e = Cipher.b64enc(Net.bytes("Man"));
        System.out.println(e[0]);          // 'T' 84
        System.out.println(e[1]);          // 'W' 87
        System.out.println(e[2]);          // 'F' 70
        System.out.println(e[3]);          // 'u' 117
        System.out.println(Crypto.arrEq(Net.bytes("Man"), Cipher.b64dec(e)));// 1
        // hex: 1,2,255 → "0102ff"; roundtrip
        String h = Cipher.hexenc(dec(1, 2, 255));
        int cocok = 1;
        int[] hx = Net.bytes("0102ff");
        for (int i = 0; i < hx.length; i++) if (hx[i] != h[i]) cocok = 0;
        System.out.println(cocok);         // 1
        System.out.println(Crypto.arrEq(dec(1, 2, 255), Cipher.hexdec(h)));// 1
        // RC4: nirl (enc lalu dec = data asli)
        int[] data = Net.bytes("rahasia jangan bocor");
        int[] key = Net.bytes("s4l4h1");
        System.out.println(Crypto.arrEq(data, Cipher.rc4(Cipher.rc4(data, key), key))); // 1
        // Vigenère: ATTACKATDAWN + LEMON → LXFOPVEFRNHR (kalimat terkenal)
        int[] v = Cipher.vigenere(Net.bytes("ATTACKATDAWN"), Net.bytes("LEMON"));
        System.out.println(Crypto.arrEq(v, Net.bytes("LXFOPVEFRNHR")));  // 1
        System.out.println(Crypto.arrEq(Net.bytes("ATTACKATDAWN"),
                                        Cipher.unvigenere(v, Net.bytes("LEMON"))));   // 1
    }

    static int[] dec(int a, int b, int c) {
        int[] x = new int[3];
        x[0] = a; x[1] = b; x[2] = c;
        return x;
    }
}
EOF
printf '34\n119\n2147483545\n1\n255\n0\n15\n10673\n300286872\n84\n87\n70\n117\n1\n1\n1\n1\n1\n1\n' > "$T/kopi/ciphertest.ref"
check ciphertest "$T/kopi/ciphertest.kop"

# ─── Rfc: echo/discard/daytime/quote/chargen terhadap python server uji ──
cat > "$T/kopi/rfc192.py" <<'EOF'
import socket, threading, time

def srv(port, fn):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", port)); s.listen(8)
    while True:
        c, _ = s.accept()
        try: fn(c)
        except OSError: pass
        try: c.close()
        except OSError: pass

def echo(c):
    c.settimeout(0.15)
    try:
        while True:
            d = c.recv(4096)
            if not d: break
            c.sendall(d)               # pantul per batch yang masuk
    except OSError: pass
    c.shutdown(socket.SHUT_WR)

def drain(c):        # close dengan data belum terbaca = RST, bukan EOF
    c.settimeout(0.15)
    try:
        while c.recv(4096): pass
    except OSError: pass
    c.settimeout(None)

def daytime(c):
    drain(c)
    c.sendall(b"2026-11-09 Kopi\n")
    c.shutdown(socket.SHUT_WR)

def quote(c):
    drain(c)
    c.sendall(b"Kopi suka uap dan bintang\n")
    c.shutdown(socket.SHUT_WR)

def discard(c):
    c.recv(4096)

def chargen(c):
    pat = b"0123456789 "
    for _ in range(64):
        c.sendall(pat)
        time.sleep(0.01)
    c.shutdown(socket.SHUT_WR)

for p, f in [(9921, echo), (9922, daytime), (9923, quote), (9924, discard), (9925, chargen)]:
    threading.Thread(target=srv, args=(p, f), daemon=True).start()
time.sleep(600)
EOF
python3 "$T/kopi/rfc192.py" & RFC_PID=$!
sleep 1

cat > "$T/kopi/netrfc.kop" <<'EOF'
import Rfc;
import Tcp;

class Main {
    public static void main(String[] args) {
        int a = Tcp.loopback();
        int[] out = new int[256];
        int n = Rfc.echoInto(a, 9921, Net.bytes("ping!"), out);   // RFC 862
        System.out.println(n);                                    // 5
        for (int i = 0; i < n; i++) { System.out.print(out[i]); System.out.print(" "); }
        System.out.println("");
        int[] day = new int[64];
        int m = Rfc.readAll(a, 9922, day);                        // RFC 867
        System.out.println(m);                                    // 16
        System.out.println(day[0]);                               // '2' = 50
        int[] qu = new int[64];
        int q = Rfc.readAll(a, 9923, qu);                         // RFC 865
        System.out.println(q);                                    // 26
        System.out.println(qu[0]);                                // 'K' = 75
        System.out.println(Rfc.discard(a, 9924, Net.bytes("buang")));  // RFC 863 → 0
        int[] cg = new int[128];                                  // RFC 864
        int g = Rfc.fill(a, 9925, cg);
        System.out.println(g);                                    // 128
        System.out.println(cg[0]); System.out.println(cg[10]); System.out.println(cg[11]);    // '0' 48, ' ' 32
        // RFC 3912 WHOIS pola kirim-tanya/baca (server uji = daytime)
        int w = Rfc.ask(a, 9922, Net.bytes("kopi.test\r\n"), out);
        System.out.println(w);                                    // 16
    }
}
EOF
printf '5\n112 105 110 103 33 \n16\n50\n26\n75\n0\n128\n48\n32\n48\n16\n' > "$T/kopi/netrfc.ref"
check netrfc "$T/kopi/netrfc.kop"
kill $RFC_PID 2>/dev/null

# ─── Dns: klien DNS-over-TCP (RFC 1035/7766) terhadap resolver uji ───────
cat > "$T/kopi/dnstest.py" <<'EOF'
import socket, struct
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 9953)); s.listen(8)

def rx(c, n):                       # TCP recv TIDAK dijamin penuh: loop
    b = b""
    while len(b) < n:
        d = c.recv(n - len(b))
        if not d: break
        b += d
    return b

while True:
    c, _ = s.accept()
    try:
        head = rx(c, 2)
        if len(head) == 2:
            tlen = struct.unpack(">H", head)[0]
            q = rx(c, tlen)
            if len(q) == tlen:
                header = q[0:2] + b"\x81\x80" + q[4:6] + b"\x00\x01" + b"\x00\x00\x00\x00"
                ans = (b"\xc0\x0c" + b"\x00\x01" + b"\x00\x01" + b"\x00\x00\x00\x78"
                       + b"\x00\x04" + bytes([10, 20, 30, 40]))
                msg = header + q[12:] + ans
                c.sendall(struct.pack(">H", len(msg)) + msg)
                c.shutdown(socket.SHUT_WR)
    except OSError:
        pass
    try: c.close()
    except OSError: pass
EOF
python3 "$T/kopi/dnstest.py" & DNS_PID=$!
sleep 1

cat > "$T/kopi/netdns.kop" <<'EOF'
import Dns;
import Tcp;

class Main {
    public static void main(String[] args) {
        int[] hasil = new int[5];
        int r = Dns.lookup(Tcp.loopback(), 9953, "kopi.test", hasil, 512);
        System.out.println(r);          // 0 sukses jaringan
        System.out.println(hasil[0]);   // RCODE 0 = NOERROR
        System.out.println(hasil[1]);   // 10
        System.out.println(hasil[2]);   // 20
        System.out.println(hasil[3]);   // 30
        System.out.println(hasil[4]);   // 40
    }
}
EOF
printf '0\n0\n10\n20\n30\n40\n' > "$T/kopi/netdns.ref"
check netdns "$T/kopi/netdns.kop"
kill $DNS_PID 2>/dev/null

# ─── Http.postInto: kirim POST + Content-Length ke server uji ────────────
cat > "$T/kopi/posttest.py" <<'EOF'
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 9927)); s.listen(8)

def rx(c, n):
    b = b""
    while len(b) < n:
        d = c.recv(n - len(b))
        if not d: break
        b += d
    return b

while True:
    c, _ = s.accept()
    try:
        head = b""
        while b"\r\n\r\n" not in head:
            ch = c.recv(1)
            if not ch: break
            head += ch
        if b"Content-Length: " in head:
            n = int(head.split(b"Content-Length: ")[1].split(b"\r")[0])
            body = rx(c, n)
            resp = b"HTTP/1.0 201 OK\r\n\r\nBody: " + body
            c.sendall(resp)
            c.shutdown(socket.SHUT_WR)
    except OSError:
        pass
    try: c.close()
    except OSError: pass
EOF
python3 "$T/kopi/posttest.py" & POST_PID=$!
sleep 1

cat > "$T/kopi/netpost.kop" <<'EOF'
import Tcp;
import Http;

class Main {
    public static void main(String[] args) {
        String path = "/kirim";
        int[] body = Net.bytes("kopi-body");
        int[] buf = new int[256];
        int n = Http.postInto(Tcp.loopback(), 9927, "h", path, body, buf);
        if (n < 0) { System.out.println(n); return; }
        int[] resp = new int[n];
        for (int i = 0; i < n; i++) resp[i] = buf[i];
        System.out.println(Http.statusOf(resp));        // 201
        int[] b = Http.bodyOf(resp);
        System.out.println(b.length);                   // 15 = "Body: kopi-body" ("Body: " 6 karakter)
        System.out.println(b[0]);                        // 'B' = 66
    }
}
EOF
printf '201\n15\n66\n' > "$T/kopi/netpost.ref"
check netpost "$T/kopi/netpost.kop"
kill $POST_PID 2>/dev/null

# ─── Net.bulk: recvb/sendb + openr entropy Rand (pukal, server khusus) ───
cat > "$T/kopi/bulksrv.py" <<'EOF'
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 9913)); s.listen(4); s.settimeout(60)
while True:
    c, _ = s.accept()
    d = b""
    while len(d) < 3000:
        chunk = c.recv(3000 - len(d))
        if not chunk: break
        d += chunk
    c.sendall(b"ACK:" + d)
    try: c.close()
    except OSError: pass
EOF
python3 "$T/kopi/bulksrv.py" & BULK_PID=$!
sleep 0.5

cat > "$T/kopi/netbulk.kop" <<'EOF'
import Rand;

class Main {
    public static void main(String[] args) {
        /* recvb + sendb: pukal 3000 byte via echo 9913 */
        int[] msg = new int[3000];
        for (int i = 0; i < 3000; i++) msg[i] = i % 251;
        int fd = Net.socket(2, 1);
        int c = Net.connect(fd, 2130706433, 9913);
        Net.sendb(fd, msg);
        int[] bal = new int[100];
        int total = 0;
        while (total < 3004) {
            int nr = Net.recvb(fd, bal);
            if (nr <= 0) break;
            total = total + nr;
        }
        System.out.println(total);       /* 3004 = "ACK:" + 3000 */
        Net.close(fd);
        /* openr: file ada → fd > 0; file kosong? /dev/null tak berisi tetap fd */
        int f1 = Net.openr("/dev/null");
        int o1 = 0;
        if (f1 > 0) o1 = 1;
        System.out.println(o1);
        Net.close(f1);
        int f2 = Net.openr("/tidak/ada/inilah.txt");
        int o2 = 0;
        if (f2 < 0) o2 = 1;
        System.out.println(o2);
        Net.close(f2);
        /* Rand: /dev/urandom — panjang + dua undian berbeda */
        int[] a = Rand.bytes(32);
        int[] b = Rand.bytes(32);
        System.out.println(a.length);
        int diff = 0;                 /* cukup "ada beda", bukan 32/32 */
        for (int i = 0; i < 32; i++) {
            if (a[i] != b[i]) diff = 1;
        }
        System.out.println(diff);
        int[] satu = Rand.bytes(1);
        System.out.println(satu.length);
    }
}
EOF
printf '3004\n1\n1\n32\n1\n1\n' > "$T/kopi/netbulk.ref"
check netbulk "$T/kopi/netbulk.kop"
kill $BULK_PID 2>/dev/null

# ─── UDP: bind/sendto/recvfrom (loopback, tanpa server luar) ─────────────
cat > "$T/kopi/udptest.kop" <<'EOF'
import Udp;

class Main {
    public static void main(String[] args) {
        int s = Udp.open();
        if (s < 0) { System.out.println(s); return; }
        System.out.println(Udp.bind(s, Udp.loopback(), 9918));

        int c = Udp.open();
        System.out.println(Udp.sendTo(c, Udp.loopback(), 9918, Net.bytes("HALO")));

        int[] buf = new int[240];
        int[] from = new int[2];
        int n = Udp.recvFrom(s, buf, from);
        System.out.println(n);
        System.out.println(from[0] == Udp.loopback());
        for (int i = 0; i < n; i++) System.out.println(buf[i]);

        System.out.println(Udp.sendTo(s, from[0], from[1], Net.bytes("BALAS")));

        int[] buf2 = new int[240];
        int[] from2 = new int[2];
        int n2 = Udp.recvFrom(c, buf2, from2);
        System.out.println(n2);
        for (int i = 0; i < n2; i++) System.out.println(buf2[i]);

        Udp.close(s);
        Udp.close(c);
    }
}
EOF
printf '0\n4\n4\n1\n72\n65\n76\n79\n5\n5\n66\n65\n76\n65\n83\n' > "$T/kopi/udptest.ref"
check udptest "$T/kopi/udptest.kop"

# ─── HTTP: header permintaan kustom + pembacaan header respons ───────────
cat > "$T/kopi/httpsrv.py" <<'EOF'
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 9919)); s.listen(4)
while True:
    try:
        c, _ = s.accept()
    except OSError:
        break
    try:
        req = b""
        while b"\r\n\r\n" not in req:
            d = c.recv(4096)
            if not d: break
            req += d
        saw = b"X-Kopi: 123" in req and b"User-Agent: Kopi/1" in req
        body = b"HALO\n"
        resp = (b"HTTP/1.0 201 Created\r\n"
                + (b"X-Saw: yes\r\n" if saw else b"X-Saw: no\r\n")
                + b"X-Kopi: ok\r\nContent-Length: 5\r\n\r\n" + body)
        c.sendall(resp)
    except OSError:
        pass
    try: c.close()
    except OSError: pass
EOF
python3 "$T/kopi/httpsrv.py" & HTTP_PID2=$!
sleep 0.5

cat > "$T/kopi/httphead.kop" <<'EOF'
import Tcp;
import Http;

class Main {
    static int[] slice(int[] a, int n) {
        int[] o = new int[n];
        for (int i = 0; i < n; i++) o[i] = a[i];
        return o;
    }

    public static void main(String[] args) {
        String h = "X-Kopi: 123\r\nUser-Agent: Kopi/1\r\n";
        int[] buf = new int[2048];
        int n = Http.getWith(2130706433, 9919, "127.0.0.1",
                             "/halo", h, buf);
        System.out.println(n > 0);
        int m = 0;
        if (n > 0) m = n;
        int[] resp = slice(buf, m);
        System.out.println(Http.statusOf(resp));
        String saw = Http.headerValue(resp, "x-saw");
        System.out.println(saw.length);
        System.out.println(saw[0]);
        int[] body = Http.bodyOf(resp);
        System.out.println(body.length);
        for (int i = 0; i < body.length; i++) System.out.println(body[i]);
        String v = Http.headerValue(resp, "X-Kopi");
        System.out.println(v.length);
        for (int i = 0; i < v.length; i++) System.out.println(v[i]);
        System.out.println(Http.hasHeader(resp, "content-length"));
        String cl = Http.headerValue(resp, "Content-Length");
        System.out.println(cl.length);
        System.out.println(cl[0]);
    }
}
EOF
printf '1\n201\n3\n121\n5\n72\n65\n76\n79\n10\n2\n111\n107\n1\n1\n53\n' > "$T/kopi/httphead.ref"
check httphead "$T/kopi/httphead.kop"
kill $HTTP_PID2 2>/dev/null

# sintaks x64 untuk Net baru (UDP + HTTP header)
for t in udptest httphead; do
    ./kopicc "$T/kopi/$t.kop" -t x64 -o "$T/kopi/${t}64.s" \
        && cc -target x86_64-linux-gnu -c "$T/kopi/${t}64.s" -o /dev/null || {
        echo "FAIL  $t [x64]: sintaks assembly"; FAIL=$((FAIL+1)); pas64=0; }
done

# ─── HTTP server minimal (.kop): listen/accept + HttpServer + arena reset ─
mkdir -p "$T/kopi/srvdir"
printf 'HALO STATIC\n' > "$T/kopi/srvdir/hello.txt"
cat > "$T/kopi/srvtest.py" <<'EOF'
import http.client, socket, sys, time
port = int(sys.argv[1])
def req(method, path, body=None, headers=None):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=8)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse(); d = r.read(); c.close()
    return r.status, r.getheader("Content-Type"), d
def wait_up():
    for _ in range(50):
        try:
            req("GET", "/health"); return
        except OSError:
            time.sleep(0.1)
    raise SystemExit("server tidak siap")
wait_up()
st, ct, d = req("GET", "/"); assert st == 200 and b"Kopi server" in d
st, ct, d = req("GET", "/health"); assert st == 200 and d == b"ok\n"
st, ct, d = req("GET", "/json"); assert st == 200 and ct == "application/json" and b'"ok"' in d
st, ct, d = req("GET", "/time"); assert st == 200 and len(d) >= 19
st, ct, d = req("GET", "/static/hello.txt"); assert st == 200 and d == b"HALO STATIC\n" and "text/plain" in ct
st, ct, d = req("GET", "/nope"); assert st == 404
st, ct, d = req("POST", "/echo", b"abc"); assert st == 200 and d == b"abc"
st, ct, d = req("GET", "/sum?a=2&b=3"); assert st == 200 and d == b"5\n"
st, ct, d = req("GET", "/header?name=X-Test", headers={"X-Test": "yes"}); assert st == 200 and d == b"yes\n"
st, ct, d = req("GET", "/static/../x"); assert st == 403
ok = 0
for i in range(200):
    st, _, d = req("GET", "/health")
    if st == 200 and d == b"ok\n": ok += 1
assert ok == 200

# keep-alive: tiga request pada satu koneksi
c = http.client.HTTPConnection("127.0.0.1", port, timeout=8)
c.request("GET", "/health"); r = c.getresponse(); d = r.read()
assert r.status == 200 and d == b"ok\n" and r.getheader("Connection") == "keep-alive"
sk = c.sock
c.request("GET", "/json"); r = c.getresponse(); d = r.read()
assert r.status == 200 and c.sock is sk
c.request("GET", "/time"); r = c.getresponse(); r.read()
assert r.status == 200
# HEAD: body kosong, Content-Length tetap
c.request("HEAD", "/health"); r = c.getresponse(); d = r.read()
assert r.status == 200 and d == b"" and r.getheader("Content-Length") == "3"
# cookie
c.request("GET", "/setcookie"); r = c.getresponse(); r.read()
assert "sesi=abc123" in (r.getheader("Set-Cookie") or "")
c.request("GET", "/cookie", headers={"Cookie": "x=1; sesi=abc123"})
r = c.getresponse(); d = r.read()
assert d == b"sesi=abc123\n", d
c.close()
# body chunked pada request
s = socket.create_connection(("127.0.0.1", port), timeout=8)
s.sendall(b"POST /echo HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
          b"4\r\nABCD\r\n3\r\nEFG\r\n0\r\n\r\n")
buf = b""
while True:
    d = s.recv(4096)
    if not d: break
    buf += d
s.close()
assert buf.split(b"\r\n\r\n", 1)[1] == b"ABCDEFG", buf[-20:]
print("OK")
EOF

# build server a64 + VM (+ x64 sintaks)
KOPIVM_ABS="$(pwd)/kopivm"
if ./kopicc examples/server/ServerDemo.kop -t a64 -o "$T/kopi/srvdemo.s" \
   && cc "$T/kopi/srvdemo.s" -o "$T/kopi/srvdemo.bin" \
   && ./kopicc examples/server/ServerDemo.kop -t vm -o "$T/kopi/srvdemo.kbc"; then
    for tgt in a64 vm; do
        port=9930
        if [ "$tgt" = vm ]; then port=9931; fi
        if [ "$tgt" = a64 ]; then
            ( cd "$T/kopi/srvdir" && exec "$T/kopi/srvdemo.bin" $port > "$T/kopi/srv.$port.log" 2>&1 ) &
        else
            ( cd "$T/kopi/srvdir" && exec "$KOPIVM_ABS" "$T/kopi/srvdemo.kbc" $port > "$T/kopi/srv.$port.log" 2>&1 ) &
        fi
        SPID=$!
        sleep 0.4
        if python3 "$T/kopi/srvtest.py" $port > "$T/kopi/srvtest.$tgt.out" 2>&1; then
            echo "PASS  server [${tgt}]"; PASS=$((PASS+1))
        else
            echo "FAIL  server [${tgt}]"; FAIL=$((FAIL+1))
            tail -3 "$T/kopi/srvtest.$tgt.out"
        fi
        kill $SPID 2>/dev/null
        wait $SPID 2>/dev/null
    done
    ./kopicc examples/server/ServerDemo.kop -t x64 -o "$T/kopi/srvdemo64.s" \
        && cc -target x86_64-linux-gnu -c "$T/kopi/srvdemo64.s" -o /dev/null \
        && echo "PASS  server [x64]" || { echo "FAIL  server [x64]"; FAIL=$((FAIL+1)); }
else
    echo "FAIL  server: build"; FAIL=$((FAIL+1))
fi

# ─── game realtime: aturan Arena + server/bot/klien .kop ───
mkdir -p "$T/kopi/game"
cp examples/game/arenaunit.out "$T/kopi/arenaunit.ref"
check arenaunit examples/game/arenaunit.kop
cp examples/game/bot.out "$T/kopi/bot.ref"

cat > "$T/kopi/gamecli.py" <<'EOF'
import sys
data = open(sys.argv[1], "rb").read()
assert data.count(b"KOPI ARENA") >= 30, "frame terlalu sedikit"
assert b"skor:" in data and b"WASD" in data
assert b"@" in data, "pemain tidak tergambar"
EOF

game_ok=1
./kopicc examples/game/GameServer.kop -t a64 -o "$T/kopi/game/gs.s" \
  && cc "$T/kopi/game/gs.s" -o "$T/kopi/game/gs.bin" || game_ok=0
./kopicc examples/game/BotClient.kop -t a64 -o "$T/kopi/game/bot.s" \
  && cc "$T/kopi/game/bot.s" -o "$T/kopi/game/bot.bin" || game_ok=0
./kopicc examples/game/GameClient.kop -t a64 -o "$T/kopi/game/cli.s" \
  && cc "$T/kopi/game/cli.s" -o "$T/kopi/game/cli.bin" || game_ok=0
./kopicc examples/game/GameServer.kop -t vm -o "$T/kopi/game/gs.kbc" || game_ok=0
./kopicc examples/game/BotClient.kop -t vm -o "$T/kopi/game/bot.kbc" || game_ok=0
./kopicc examples/game/GameClient.kop -t vm -o "$T/kopi/game/cli.kbc" || game_ok=0

for f in GameServer BotClient GameClient; do
    ./kopicc "examples/game/$f.kop" -t x64 -o "$T/kopi/game/$f.x64.s" \
      && cc -target x86_64-linux-gnu -c "$T/kopi/game/$f.x64.s" -o /dev/null \
      && echo "PASS  game [$f x64]" && PASS=$((PASS+1)) \
      || { echo "FAIL  game [$f x64]"; FAIL=$((FAIL+1)); }
done

if [ $game_ok -eq 1 ]; then
    for tgt in a64 vm; do
        port=9940
        [ "$tgt" = vm ] && port=9941
        if [ "$tgt" = a64 ]; then
            "$T/kopi/game/gs.bin" $port 50 > "$T/kopi/game/srv.$tgt.log" 2>&1 &
        else
            "$KOPIVM_ABS" "$T/kopi/game/gs.kbc" $port 50 > "$T/kopi/game/srv.$tgt.log" 2>&1 &
        fi
        SPID=$!
        sleep 0.5

        if [ "$tgt" = a64 ]; then
            timeout 25 "$T/kopi/game/bot.bin" $port > "$T/kopi/game/bot.$tgt.out" 2>&1
        else
            timeout 25 "$KOPIVM_ABS" "$T/kopi/game/bot.kbc" $port > "$T/kopi/game/bot.$tgt.out" 2>&1
        fi
        if [ $? -eq 0 ] && diff -q "$T/kopi/game/bot.$tgt.out" "$T/kopi/bot.ref" >/dev/null 2>&1; then
            echo "PASS  gamebot [$tgt]"; PASS=$((PASS+1))
        else
            echo "FAIL  gamebot [$tgt]"; FAIL=$((FAIL+1)); tail -3 "$T/kopi/game/bot.$tgt.out"
        fi

        if [ "$tgt" = a64 ]; then
            timeout 40 "$T/kopi/game/cli.bin" $port 127.0.0.1 uji 40 \
              < /dev/null > "$T/kopi/game/cli.$tgt.out" 2>&1
        else
            timeout 40 "$KOPIVM_ABS" "$T/kopi/game/cli.kbc" $port 127.0.0.1 uji 40 \
              < /dev/null > "$T/kopi/game/cli.$tgt.out" 2>&1
        fi
        if [ $? -eq 0 ] && python3 "$T/kopi/gamecli.py" "$T/kopi/game/cli.$tgt.out"; then
            echo "PASS  gamecli [$tgt]"; PASS=$((PASS+1))
        else
            echo "FAIL  gamecli [$tgt]"; FAIL=$((FAIL+1)); tail -3 "$T/kopi/game/srv.$tgt.log"
        fi

        kill $SPID 2>/dev/null
        wait $SPID 2>/dev/null
    done
else
    echo "FAIL  game: build"; FAIL=$((FAIL+1))
fi

# ─── contoh dokumentasi harus tetap hidup ───
cp docs/contoh/tour.ref "$T/kopi/dochtour.ref"
check dochtour docs/contoh/tour.kop

# ─── fitur bahasa v1.11: ternary, do/while, metode String ───
cp test/langtest.ref "$T/kopi/langtest.ref"
check langtest test/langtest.kop

# ─── fitur bahasa v1.12: class instance (new, field, method this) ───
cp test/objtest.ref "$T/kopi/objtest.ref"
check objtest test/objtest.kop
./kopicc test/objtest.kop -t x64 -o "$T/kopi/objtest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/objtest64.s" -o /dev/null \
    && echo "PASS  objtest [x64]" || { echo "FAIL  objtest [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.14: field instance 64-bit (long/String/class) ───
cp test/reftest.ref "$T/kopi/reftest.ref"
check reftest test/reftest.kop
./kopicc test/reftest.kop -t x64 -o "$T/kopi/reftest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/reftest64.s" -o /dev/null \
    && echo "PASS  reftest [x64]" || { echo "FAIL  reftest [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.15: constructor — new Kelas(args) ───
cp test/ctortest.ref "$T/kopi/ctortest.ref"
check ctortest test/ctortest.kop
./kopicc test/ctortest.kop -t x64 -o "$T/kopi/ctortest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/ctortest64.s" -o /dev/null \
    && echo "PASS  ctortest [x64]" || { echo "FAIL  ctortest [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.16: array bertipe long[] dan String[] ───
cp test/arrtest.ref "$T/kopi/arrtest.ref"
check arrtest test/arrtest.kop
./kopicc test/arrtest.kop -t x64 -o "$T/kopi/arrtest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/arrtest64.s" -o /dev/null \
    && echo "PASS  arrtest [x64]" || { echo "FAIL  arrtest [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.17: String opaque — Net.mkstr/Net.strbytes ───
cp test/mkstrtest.ref "$T/kopi/mkstrtest.ref"
check mkstrtest test/mkstrtest.kop
./kopicc test/mkstrtest.kop -t x64 -o "$T/kopi/mkstrtest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/mkstrtest64.s" -o /dev/null \
    && echo "PASS  mkstrtest [x64]" || { echo "FAIL  mkstrtest [x64]"; FAIL=$((FAIL+1)); }

# ─── v1.23: Str native String + Bytes buffer (API lengkap) ───
cp test/strbytes.ref "$T/kopi/strbytes.ref"
check strbytes test/strbytes.kop
./kopicc test/strbytes.kop -t x64 -o "$T/kopi/strbytes64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/strbytes64.s" -o /dev/null \
    && echo "PASS  strbytes [x64]" || { echo "FAIL  strbytes [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.18: String packed 1 byte/karakter ───
cp test/packedreuse.ref "$T/kopi/packedreuse.ref"
check packedreuse test/packedreuse.kop
./kopicc test/packedreuse.kop -t x64 -o "$T/kopi/packedreuse64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/packedreuse64.s" -o /dev/null \
    && echo "PASS  packedreuse [x64]" || { echo "FAIL  packedreuse [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.19: switch (desugar if/else, tanpa fall-through) ───
cp test/switchtest.ref "$T/kopi/switchtest.ref"
check switchtest test/switchtest.kop
./kopicc test/switchtest.kop -t x64 -o "$T/kopi/switchtest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/switchtest64.s" -o /dev/null \
    && echo "PASS  switchtest [x64]" || { echo "FAIL  switchtest [x64]"; FAIL=$((FAIL+1)); }

# ─── fitur bahasa v1.20: malloc { ... } region arena otomatis ───
cp test/regiontest.ref "$T/kopi/regiontest.ref"
check regiontest test/regiontest.kop
./kopicc test/regiontest.kop -t x64 -o "$T/kopi/regiontest64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/regiontest64.s" -o /dev/null \
    && echo "PASS  regiontest [x64]" || { echo "FAIL  regiontest [x64]"; FAIL=$((FAIL+1)); }
# diagnostik escape: rujukan tidak boleh keluar malloc{}
# contoh resmi examples/malloc.kop ikut diuji (output = dokumentasi hidup)
printf '36\n150\n16\n4\nKOPI LANG\n9\n9\n7\n3\n1\nserver: 5 request tanpa memReset\n285\n6000000000\n40\n42\nselesai\n' > "$T/kopi/mallex.ref"
check mallex examples/malloc.kop
./kopicc examples/malloc.kop -t x64 -o "$T/kopi/mallex64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/mallex64.s" -o /dev/null \
    && echo "PASS  mallex [x64]" || { echo "FAIL  mallex [x64]"; FAIL=$((FAIL+1)); }

cat > "$T/kopi/rgesc.kop" <<'EOF'
class Main {
    public static void main(String[] args) {
        String luar = "awal";
        malloc { String s = "dalam"; luar = s; }
    }
}
EOF
if ./kopicc "$T/kopi/rgesc.kop" -t vm -o /dev/null 2>&1 | grep -q "hidup di luar malloc{}"; then
    echo "PASS  region escape [err]"; PASS=$((PASS+1))
else
    echo "FAIL  region escape [err]"; FAIL=$((FAIL+1))
fi
cat > "$T/kopi/rgres.kop" <<'EOF'
class Main {
    static String f() {
        malloc { return "dalam"; }
    }
    public static void main(String[] args) { System.out.println(f()); }
}
EOF
if ./kopicc "$T/kopi/rgres.kop" -t vm -o /dev/null 2>&1 | grep -q "return bertipe rujukan dari dalam malloc{}"; then
    echo "PASS  region return escape [err]"; PASS=$((PASS+1))
else
    echo "FAIL  region return escape [err]"; FAIL=$((FAIL+1))
fi

# ─── v1.21: ukuran arena per blok malloc<64mb> { ... } ───
cp test/mallocsize.ref "$T/kopi/mallocsize.ref"
check mallocsize test/mallocsize.kop
./kopicc test/mallocsize.kop -t x64 -o "$T/kopi/mallocsize64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/mallocsize64.s" -o /dev/null \
    && echo "PASS  mallocsize [x64]" || { echo "FAIL  mallocsize [x64]"; FAIL=$((FAIL+1)); }

# diagnostik ukuran: nol, terlalu besar, satuan salah
for spec in "0mb:lebih besar dari 0" "5000mb:maksimum 4096 MB" "64xb:tidak dikenal"; do
    sz=${spec%%:*}; msg=${spec#*:}
    sed "s/malloc<1mb>/malloc<$sz>/" test/mallocsize.kop > "$T/kopi/msbad.kop"
    if ./kopicc "$T/kopi/msbad.kop" -t vm -o /dev/null 2>&1 | grep -q "$msg"; then
        echo "PASS  malloc<$sz> [err]"; PASS=$((PASS+1))
    else
        echo "FAIL  malloc<$sz> [err]"; FAIL=$((FAIL+1))
    fi
done

# runtime: alokasi melewati batas blok harus error dengan ukuran batasnya
cat > "$T/kopi/moom.kop" <<'EOF'
class Main {
    public static void main(String[] args) {
        malloc<1mb> {
            int[] a = new int[200000];     // 1,6 MB > 1 MB
            a[0] = 1;
            System.out.println(a[0]);
        }
    }
}
EOF
./kopicc "$T/kopi/moom.kop" -t vm -o "$T/kopi/moom.kbc" \
    && ./kopivm "$T/kopi/moom.kbc" > "$T/kopi/moom.out" 2>&1
if [ $? -ne 0 ] && grep -q "batas 1 MB terlampaui" "$T/kopi/moom.out"; then
    echo "PASS  malloc limit [vm]"; PASS=$((PASS+1))
else
    echo "FAIL  malloc limit [vm]"; FAIL=$((FAIL+1)); head -3 "$T/kopi/moom.out"
fi
./kopicc "$T/kopi/moom.kop" -t a64 -o "$T/kopi/moom.s" \
    && cc "$T/kopi/moom.s" -o "$T/kopi/moom.bin" \
    && "$T/kopi/moom.bin" > "$T/kopi/moom.out" 2>&1
if [ $? -ne 0 ] && grep -q "batas 1 MB terlampaui" "$T/kopi/moom.out"; then
    echo "PASS  malloc limit [a64]"; PASS=$((PASS+1))
else
    echo "FAIL  malloc limit [a64]"; FAIL=$((FAIL+1)); head -3 "$T/kopi/moom.out"
fi

# ─── v1.22: malloc { ... } otomatis (tanpa <...>, >32 MB) ───
cp test/autosize.ref "$T/kopi/autosize.ref"
check autosize test/autosize.kop
./kopicc test/autosize.kop -t x64 -o "$T/kopi/autosize64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/autosize64.s" -o /dev/null \
    && echo "PASS  autosize [x64]" || { echo "FAIL  autosize [x64]"; FAIL=$((FAIL+1)); }

# ─── v1.22: pola loop panas (superinstruksi VM) tetap benar ───
cp test/loopfast.ref "$T/kopi/loopfast.ref"
check loopfast test/loopfast.kop
./kopicc test/loopfast.kop -t x64 -o "$T/kopi/loopfast64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/loopfast64.s" -o /dev/null \
    && echo "PASS  loopfast [x64]" || { echo "FAIL  loopfast [x64]"; FAIL=$((FAIL+1)); }

# ─── v1.22: diagnostik pintar — baris sumber + saran perbaikan ───
cat > "$T/kopi/diagstr.kop" <<'EOF'
class Main {
    public static void main(String[] args) {
        int x = 7;
        System.out.println(`nilai {x}`);
    }
}
EOF
if ./kopicc "$T/kopi/diagstr.kop" -t vm -o /dev/null 2>&1 \
        | grep -q "saran: Tambahkan \`import Str;\`"; then
    echo "PASS  diagnostic hint [err]"; PASS=$((PASS+1))
else
    echo "FAIL  diagnostic hint [err]"; FAIL=$((FAIL+1))
fi
cat > "$T/kopi/diagidx.kop" <<'EOF'
class Main {
    public static void main(String[] args) {
        int[] a = new int[2];
        a[5] = 1;
        System.out.println(a[0]);
    }
}
EOF
./kopicc "$T/kopi/diagidx.kop" -t vm -o "$T/kopi/diagidx.kbc" \
    && ./kopivm "$T/kopi/diagidx.kbc" > "$T/kopi/diagidx.out" 2>&1
if [ $? -ne 0 ] && grep -q "saran: indeks valid" "$T/kopi/diagidx.out"; then
    echo "PASS  runtime hint [vm]"; PASS=$((PASS+1))
else
    echo "FAIL  runtime hint [vm]"; FAIL=$((FAIL+1)); head -3 "$T/kopi/diagidx.out"
fi

# ─── v1.22: lib Pbkdf2 membebaskan buffer HMAC tiap iterasi ───
cp test/pbkdf2long.ref "$T/kopi/pbkdf2long.ref"
check pbkdf2long test/pbkdf2long.kop
./kopicc test/pbkdf2long.kop -t x64 -o "$T/kopi/pbkdf2long64.s" \
    && cc -target x86_64-linux-gnu -c "$T/kopi/pbkdf2long64.s" -o /dev/null \
    && echo "PASS  pbkdf2long [x64]" || { echo "FAIL  pbkdf2long [x64]"; FAIL=$((FAIL+1)); }

# ─── URL (oracle Python urllib.parse) + SHA-1 (hashlib) ───
python3 tools/gen_urlvec.py > "$T/kopi/urltest.ref"
check urltest test/urltest.kop

python3 - > "$T/kopi/sha1vec.ref" <<'EOF'
import hashlib
for s in ["", "abc",
          "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "The quick brown fox jumps over the lazy dog"]:
    print(hashlib.sha1(s.encode()).hexdigest())
EOF
check sha1vec test/sha1vec.kop

# ─── klien HTTP/1.1: chunked, redirect, basic auth, form, JSON ───
cat > "$T/kopi/http11.ref" <<'EOF'
200
body=halo dunia!
200
final=sampai
auth=Basic YnVkaTpyYWhhc2lh
form=POST:nama=Kopi%20Arena&n=3
json=JSON:{"ok":true}
EOF
./kopicc test/http11.kop -t a64 -o "$T/kopi/http11.s" \
  && cc "$T/kopi/http11.s" -o "$T/kopi/http11.bin" || true
./kopicc test/http11.kop -t vm -o "$T/kopi/http11.kbc" || true
python3 tools/http_test_server.py 10050 > "$T/kopi/http11.srv.log" 2>&1 &
HPID=$!
sleep 0.5
for tgt in a64 vm; do
    if [ "$tgt" = a64 ]; then
        timeout 20 "$T/kopi/http11.bin" 10050 > "$T/kopi/http11.$tgt.out" 2>&1
    else
        timeout 20 "$KOPIVM_ABS" "$T/kopi/http11.kbc" 10050 > "$T/kopi/http11.$tgt.out" 2>&1
    fi
    if [ $? -eq 0 ] && diff -q "$T/kopi/http11.$tgt.out" "$T/kopi/http11.ref" >/dev/null 2>&1; then
        echo "PASS  http11 [$tgt]"; PASS=$((PASS+1))
    else
        echo "FAIL  http11 [$tgt]"; FAIL=$((FAIL+1)); tail -4 "$T/kopi/http11.$tgt.out"
    fi
done
kill $HPID 2>/dev/null
wait $HPID 2>/dev/null

# ─── WebSocket: handshake + frame (probe Python) ───
./kopicc examples/ws/WsEcho.kop -t a64 -o "$T/kopi/ws.s" \
  && cc "$T/kopi/ws.s" -o "$T/kopi/ws.bin" || true
./kopicc examples/ws/WsEcho.kop -t vm -o "$T/kopi/ws.kbc" || true
./kopicc examples/ws/WsEcho.kop -t x64 -o "$T/kopi/ws64.s" \
  && cc -target x86_64-linux-gnu -c "$T/kopi/ws64.s" -o /dev/null \
  && echo "PASS  websocket [x64]" && PASS=$((PASS+1)) \
  || { echo "FAIL  websocket [x64]"; FAIL=$((FAIL+1)); }

for tgt in a64 vm; do
    port=10060
    [ "$tgt" = vm ] && port=10061
    if [ "$tgt" = a64 ]; then
        "$T/kopi/ws.bin" $port > "$T/kopi/ws.$tgt.log" 2>&1 &
    else
        "$KOPIVM_ABS" "$T/kopi/ws.kbc" $port > "$T/kopi/ws.$tgt.log" 2>&1 &
    fi
    WPID=$!
    sleep 0.5
    if python3 tools/ws_probe.py $port > "$T/kopi/ws.$tgt.out" 2>&1; then
        echo "PASS  websocket [$tgt]"; PASS=$((PASS+1))
    else
        echo "FAIL  websocket [$tgt]"; FAIL=$((FAIL+1)); tail -4 "$T/kopi/ws.$tgt.out"
    fi
    kill $WPID 2>/dev/null
    wait $WPID 2>/dev/null
done

# ─── event loop (Net.pollfd): ServerEvent multi-klien ───
./kopicc examples/server/ServerEvent.kop -t a64 -o "$T/kopi/sev.s" \
  && cc "$T/kopi/sev.s" -o "$T/kopi/sev.bin" || true
./kopicc examples/server/ServerEvent.kop -t vm -o "$T/kopi/sev.kbc" || true
./kopicc examples/server/ServerEvent.kop -t x64 -o "$T/kopi/sev64.s" \
  && cc -target x86_64-linux-gnu -c "$T/kopi/sev64.s" -o /dev/null \
  && echo "PASS  event [x64]" && PASS=$((PASS+1)) \
  || { echo "FAIL  event [x64]"; FAIL=$((FAIL+1)); }
for tgt in a64 vm; do
    port=10080
    [ "$tgt" = vm ] && port=10081
    if [ "$tgt" = a64 ]; then
        "$T/kopi/sev.bin" $port > "$T/kopi/sev.$tgt.log" 2>&1 &
    else
        "$KOPIVM_ABS" "$T/kopi/sev.kbc" $port > "$T/kopi/sev.$tgt.log" 2>&1 &
    fi
    EPID=$!
    sleep 0.5
    if python3 tools/event_probe.py $port > "$T/kopi/sev.$tgt.out" 2>&1; then
        echo "PASS  event [$tgt]"; PASS=$((PASS+1))
    else
        echo "FAIL  event [$tgt]"; FAIL=$((FAIL+1)); tail -4 "$T/kopi/sev.$tgt.out"
    fi
    kill $EPID 2>/dev/null
    wait $EPID 2>/dev/null
done

# ─── PBKDF2 / JWT / Session / Auth (oracle Python hashlib+hmac) ───
python3 tools/gen_authvec.py > "$T/kopi/authvec.ref"
check authvec test/authvec.kop

# ─── resolver DNS UDP + parser resolv.conf ───
printf '2\n16909060\n167772213\n0\n0\n10\n1\n2\n3\n0\n3\n' > "$T/kopi/dnsresolv.ref"
python3 tools/dns_test_server.py 10055 > "$T/kopi/dns.log" 2>&1 &
DPID=$!
sleep 0.4
check dnsresolv test/dnsresolv.kop
kill $DPID 2>/dev/null
wait $DPID 2>/dev/null

# ─── v1.13: error runtime berline + stack trace di VM ───
cat > "$T/kopi/traceerr.kop" <<'EOF'
class Main {
    static int bagi(int a, int b) {
        return a / b;
    }

    static void main() {
        System.out.println(bagi(10, 0));
    }
}
EOF
./kopicc "$T/kopi/traceerr.kop" -t vm -o "$T/kopi/traceerr.kbc"
errout="$(./kopivm "$T/kopi/traceerr.kbc" 2>&1 >/dev/null)"
trc=$?
if [ $trc -ne 0 ] && echo "$errout" | grep -q "kopivm: error: pembagian dengan nol" \
   && echo "$errout" | grep -q "stack trace" \
   && echo "$errout" | grep -Eq "at bagi \(.*traceerr\.kop:3\)" \
   && echo "$errout" | grep -Eq "at main \(.*traceerr\.kop:7\)"; then
    echo "PASS  traceerr [vm]"; PASS=$((PASS+1))
else
    echo "FAIL  traceerr [vm]"; FAIL=$((FAIL+1)); echo "$errout" | head -6
fi

# ─── v1.37: HPACK/Huffman (Http2) — vektor oracle python hpack ─────
python3 tools/gen_http2vec.py "$T/kopi/http2vec.kop" "$T/kopi/http2vec.ref" >/dev/null
check http2vec "$T/kopi/http2vec.kop"

# ─── v1.37: klien HTTP/2 end-to-end — oracle python h2 (TLS+ALPN h2) ──
pkill -f "h2_[o]racle.py" 2>/dev/null
sleep 0.3
if ./kopicc test/http2cli.kop -t a64 -o "$T/kopi/h2cli.s" && \
   cc "$T/kopi/h2cli.s" -o "$T/kopi/h2cli.bin"; then
    if [ ! -f "$T/h2_cert.pem" ]; then
        openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
            -nodes -keyout "$T/h2_key.pem" -out "$T/h2_cert.pem" \
            -days 3650 -subj /CN=h2oracle 2>/dev/null
    fi
    openssl x509 -in "$T/h2_cert.pem" -outform der -out "$T/h2_cert.der" 2>/dev/null
    python3 tools/h2_oracle.py 10096 "$T/h2_cert.pem" "$T/h2_key.pem" \
        > "$T/kopi/h2oracle.log" 2>&1 &
    OPID=$!
    ok=0
    for i in 1 2 3 4 5 6 7 8 9 10; do
        sleep 1
        if python3 -c "import socket; socket.create_connection(('127.0.0.1', 10096), timeout=1).close()" 2>/dev/null; then
            ok=1
            break
        fi
    done
    if [ $ok -eq 1 ]; then
        cliout="$("$T/kopi/h2cli.bin" 10096 "$T/h2_cert.der" 2>&1)"
        sleep 1
        if echo "$cliout" | grep -q "gate=1" \
           && echo "$cliout" | grep -q "st=200 n=11" \
           && echo "$cliout" | grep -q "st2=200 n2=12 echo=1"; then
            echo "PASS  http2 [a64]"; PASS=$((PASS+1))
        else
            echo "FAIL  http2 [a64]"; FAIL=$((FAIL+1))
            echo "$cliout" | head -6
        fi
    else
        echo "FAIL  http2 [a64]: oracle tidak siap"; FAIL=$((FAIL+1))
        head -5 "$T/kopi/h2oracle.log"
    fi
    kill $OPID 2>/dev/null
    wait $OPID 2>/dev/null
else
    echo "FAIL  http2 [a64]: build gagal"; FAIL=$((FAIL+1))
fi

# ─── v1.36: TLS server (TlsServer) — oracle python ssl + klien Kopi ──
pkill -f "tlssrv\.[b]in" 2>/dev/null
sleep 0.3
if ./kopicc test/tlssrv.kop -t a64 -o "$T/kopi/tlssrv.s" &&
   cc "$T/kopi/tlssrv.s" -o "$T/kopi/tlssrv.bin" &&
   ./kopicc test/tlssrvcli.kop -t a64 -o "$T/kopi/tlssrvcli.s" &&
   cc "$T/kopi/tlssrvcli.s" -o "$T/kopi/tlssrvcli.bin"; then
    rm -f "$T/kopi/tls_cert.der" "$T/kopi/srvout.log" "$T/srvdbg.log"
    "$T/kopi/tlssrv.bin" 10085 "$T/kopi/tls_cert.der" > "$T/kopi/srvout.log" 2>&1 &
    SPID=$!
    ok=0
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        sleep 1
        if grep -q "listen ok" "$T/opencode/srvdbg.log" 2>/dev/null ||
           grep -q "listen ok" "$T/srvdbg.log" 2>/dev/null; then
            ok=1
            break
        fi
    done
    if [ $ok -eq 1 ]; then
        pyout="$(python3 tools/tls_srv_probe.py 10085 2>&1 | tail -1)"
        cliout="$("$T/kopi/tlssrvcli.bin" 10085 "$T/kopi/tls_cert.der" 2>&1)"
        sleep 1
        if [ "$pyout" = "py TLSv1.3 True" ] &&
           echo "$cliout" | grep -q "cli pin=1 fin=1 echo=pong"; then
            echo "PASS  tlssrv [a64]"; PASS=$((PASS+1))
        else
            echo "FAIL  tlssrv [a64]"; FAIL=$((FAIL+1))
            echo "py=$pyout"
            echo "$cliout" | head -3
        fi
    else
        echo "FAIL  tlssrv [a64]: server tidak siap"; FAIL=$((FAIL+1))
        tail -5 "$T/kopi/srvout.log"
    fi
    kill $SPID 2>/dev/null
    wait $SPID 2>/dev/null
else
    echo "FAIL  tlssrv [a64]: build gagal"; FAIL=$((FAIL+1))
fi

# ─── situs dokumentasi HTML (GitHub Pages) ───
# gen_libref dulu: signature lib/*.kop harus sinkron dengan docs/api-*.md
if python3 tools/gen_libref.py > "$T/kopi/libref.out" 2>&1 &&
   python3 tools/check_docs.py > "$T/kopi/docshtml.out" 2>&1; then
    echo "PASS  docshtml"; PASS=$((PASS+1))
else
    echo "FAIL  docshtml"; FAIL=$((FAIL+1)); tail -3 "$T/kopi/docshtml.out"
fi

echo "----"
echo "hasil: $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ]
