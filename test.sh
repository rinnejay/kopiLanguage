#!/bin/sh
# test.sh — verifikasi konsistensi backend vm & a64 terhadap referensi
cd "$(dirname "$0")"
# bersihkan server uji sisa run yang mungkin meningkat diam-diam
pkill -f "python3 .*kopi/[r]fc192.py" 2>/dev/null
pkill -f "python3 .*kopi/[d]nstest.py" 2>/dev/null
pkill -f "python3 .*kopi/[p]osttest.py" 2>/dev/null
pkill -f "python3 -m http.server [9]911" 2>/dev/null
pkill -f "python3 .*opencode/[e]chosrv.py" 2>/dev/null
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
for t in array cryptot arrtest ciphertest netrfc netdns netget netpost; do
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
        d = c.recv(4096)
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
        int[] host = Net.bytes("127.0.0.1");
        int[] path = Net.bytes("/halo.txt");
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
        int[] e = Cipher.b64enc(Net.bytes("Man"));
        System.out.println(e[0]);          // 'T' 84
        System.out.println(e[1]);          // 'W' 87
        System.out.println(e[2]);          // 'F' 70
        System.out.println(e[3]);          // 'u' 117
        System.out.println(Crypto.arrEq(Net.bytes("Man"), Cipher.b64dec(e)));// 1
        // hex: 1,2,255 → "0102ff"; roundtrip
        int[] h = Cipher.hexenc(dec(1, 2, 255));
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
        int r = Dns.lookup(Tcp.loopback(), 9953, Net.bytes("kopi.test"), hasil, 512);
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
        int[] path = Net.bytes("/kirim");
        int[] body = Net.bytes("kopi-body");
        int[] buf = new int[256];
        int n = Http.postInto(Tcp.loopback(), 9927, Net.bytes("h"), path, body, buf);
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

echo "----"
echo "hasil: $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ]
