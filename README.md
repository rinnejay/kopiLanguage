# Kopi — bahasa bergaya Java yang dikompilasi langsung ke binary

> `kopi` = kopi ☕ — plesetan Java. Filsafatnya sama seperti C/Rust:
> **compiler membaca source, menurunkan assembly/binary langsung** —
> bukan dikonversi ke Java, tidak ada JVM, tidak ada bytecode vendor lain.

```
                ┌────────────────────────────────────────────┐
                │                 kopicc                     │
   hello.kop ──►│  lexer → parser → resolusi → codegen       │
                │                                            │
                │  ┌──────────┬──────────┬────────────────┐  │
                │  │ -t a64   │ -t x64   │ -t vm          │  │
                │  └────┬─────┴────┬─────┴───────┬────────┘  │
                └───────┼──────────┼─────────────┼───────────┘
                        ▼          ▼             ▼
                   hello.s     hello.s       hello.kbc
                 (aarch64)    (x86-64)     (KOP1 bytecode)
                        │          │             │
                        ▼          ▼             ▼
                  cc hello.s  gcc hello.s     kopivm hello.kbc
                        │          │       (mesin virtual stack KOP1)
                        ▼          ▼             
                    ./hello     ./hello        ← hasil: BINARY ASLI
```

`kopicc` ditulis dari nol di C: lexer sendiri, parser recursive-descent
sendiri, type checker sendiri, generator assembly sendiri, dan mesin virtual
sendiri. Tidak ada dependensi selain libc (`printf` saja untuk `println`).

## Struktur pustaka

Pustaka standar (`lib/`) dikelompokkan per fungsi; `import Nama;` tetap
satu nama — kompiler menyapu subfolder dengan sendirinya (urutan:
folder pengimpor → `lib/` rata → `lib/<kategori>/`):

| Lokasi | Kelompok | Isi |
|--------|----------|-----|
| `lib/core/` | logika dasar bahasa | `Math` (lengkap: modular, teori bilangan, kombinatorika, bit, geometri), `Digits`, `Stats` (statistik), `Matrix` (aljabar matriks), `Arrays` |
| `lib/crypto/` | kriptografi | `Crypto`, `Cipher` (klasik: RC4/Vigenère/base64/CRC), `Hash` (SHA-256+HMAC), `Kdf` (HKDF+ExpandLabel), `Big` (medan 2²⁵⁵−19), `X25519`, `ChaCha` (stream), `Poly` (Poly1305), `Aead` (ChaCha20-Poly1305), `Rand` (entropi `/dev/urandom`), `Der` (parser DER minimal + pin SPKI SHA-256) |
| `lib/net/` | jaringan | `Tcp` (transport di atas syscall `Net`), `Dns` (DNS-over-TCP), `Http` (HTTP/1.0), `Rfc` (klien protokol era awal: daytime/echo/quote/whois/finger/discard), **`Tls`** (klien TLS 1.3 — lihat bagian khusus di bawah) |

ATURAN unik: file test TIDAK boleh bernama sama (kasus tak peduli) dengan
modul yang diimpor — file lokal selalu menang atas `lib/`.

## Status & changelog

```
Pengembangan     ███████████░░░░░  ~68%  menuju bahasa serius (v2)
v1.5d-E2 hari ini ████████████████  61/61 test hijau — klien TLS 1.3 end-to-end (a64 + VM)
```

| Versi | Isi |
|-------|-----|
| **v1.0** | core language: class/static, `int/boolean`, aritmetika semantik Java, kontrol alir, multi-file `import`, 3 backend (a64 native, x64 native, VM KOP1) |
| **v1.1** | lambda/closure sejati: capture by-value, `fn` sebagai argumen, arity check runtime, semua backend |
| **v1.2** | **array `int[]` + arena memori `kopalloc`** (bump allocator): `new int[n]`, `a[i]`, `a[i] = v`, `a.length`, bounds check runtime (`kopi: array tidak valid`, OOM rapi); fix ABI entry `main` (frame x30 sendiri); pustaka `lib/Arrays.kop` (sort/binSearch/find/count/...) dan `lib/Crypto.kop` (bitxor + FNV-1a + PRNG Lehmer + XOR-stream/Caesar) — **100% ditulis dalam bahasa Kopi** di atas aritmetika murni |
| **v1.2.1** | scoping per-blok (`if/else`, `while`, `for`, blok `{}` telanjang — deklarasi direset di akhir blok, shadow antar cabang diperbolehkan ala sibling) + multi-deklarasi `int a = 0, b = 1;` — library kini bisa memakai nama loop yang sama di metode yang sama |
| **v1.3** | **jaringan**: builtin `Net.*` = syscall Linux langsung tanpa libc — `socket`, `connect`, `rdb` (baca 1 byte), `wrb` (tulis 1 byte), `close`, dan `Net.bytes("literal")` (byte-as-int, dikonversi compile-time). Codegen native a64 (`svc #0`) & x64 (`syscall`) membangun `sockaddr_in` di stack sendiri; VM KOP1 jalur op `OP_NET`. Pustaka `lib/Tcp.kop` (open/send/recvAll/cat/indexOf — 100% Kopi) dan `lib/Http.kop` (klien HTTP/1.0 minimal: statusOf/bodyOf/getInto) + `examples/httpget.kop`; uji loopback (echo + `python -m http.server`) masuk test.sh |
| **v1.5d-D** | **AEAD ChaCha20-Poly1305 murni Kopi** (`lib/Aead.kop`): otk = 32 byte pertama blok counter-0, ct = ChaCha20 counter-1, `macData = aad ‖ pad16 ‖ ct ‖ pad16 ‖ lenLE64(aad) ‖ lenLE64(ct)`, tag = Poly1305(otk); **lulus vektor §2.8.2 RFC 7539 = RFC 8439** (key `80..9f`, nonce `00000007 40414243 44454647`, tag `1ae10b59 …`) + roundtrip dekripsi + kasus-ganti ciphertext terdeteksi, di VM, native a64, sintaks x64. Generator fail-keras: oracle python murni (mod p, int besar) wajib mereproduksi ct & tag RFC sebelum test dibuat. Jebakan hari ini: (1) tag `d63103…` yang tadinya menjadi kecurigaan tak berlaku — 7539 dan 8439 sama-sama `1ae10b59…`; (2) nonce AEAD LE-word: `07 00 00 00 40 41 42 43 …` (word13 = 7), bukan `00 00 00 07 …`; (3) otk butuh feed 32 byte-nol — 16 byte menghasilkan s kosong dan tag menyertainya salah. Semua backend harus menghasilkan output byte-per-byte identik: VM, native aarch64, dan x86-64 (sintaks assembly) |
| **v1.5d-E** | **klien TLS 1.3 (RFC 8446) murni Kopi** di `lib/net/Tls.kop` + `lib/crypto/Der.kop` (pin SPKI) : record layer AEAD ChaCha20-Poly1305, key schedule penuh (§7.1), X25519, sertifikat di-pin (TOFU), server Finished **diverifikasi** (HMAC finished_key atas transkrip), client Finished dikirim, `writeApp`/`readApp` untuk data aplikasi dua arah. Lolos lawan `openssl s_server` (2× NewSessionTicket, `close_notify` warning) dan server echo TLS |
| **v1.5d-E2** | **akar-akar bug TLS dibereskan** (semua diverifikasi oracle python/RFC, bukan tambal nilai): ① `Kdf.expandLabel` — urutan `HkdfLabel` §7.1 salah (panjang label harus termasuk `"tls13 "`). ② **Poly1305** — carry `carPass` bisa tersisa di slot 9 (`2^135 ≡ 160 mod p`) lalu dibuang `mulFold` berikutnya → tag benar untuk pesan pendek dan salah ~50% untuk 700–900 B; kini dikuras `fold9()` tiap blok. ③ nonce record — seq 64-bit **big-endian** di byte 4..11 (§5.2), shift ≥32 dihindari (UB C/ARM). ④ `seal()` — loop menyalin melewati batas array (panjang record salah). ⑤ key schedule — `early = Extract(0³²,0³²)`, `master = Extract(DS(hs,"derived",H("")),0³²)`, `c_ap`/`s_ap` dari transkrip s.d. **server Finished**. ⑥ client Finished — kunci handshake klien (CH+SH), seq reset 0, inner type **22**. Test 61/61; generator Poly1305 menambah 3 vektor panjang deterministik, `gen_kdf.py` diperbaiki ke label RFC |
| **v1.5d-E3** | **penamaan pustaka inti jadi Inggris**: `Matematika`→**`Math`** (`pangkat`→`pow`, `akar`→`sqrt`, `prima`→`isPrime`, `faktorial`→`factorial`, `genap`→`isEven`), `Angka`→**`Digits`** (`balik`→`reverse`, `jumlahDigit`→`digitSum`, `panjang`→`digitCount`, `deka`→`pow10`, `cetakBasis`→`printBase`); `examples/pakai_lib.kop` + `test.sh` + README diselaraskan |
| **v1.5d-E4** | **pustaka matematika diperluas besar-besaran** (`int` 32-bit, murni Kopi): `Math` kini mencakup 80+ fungsi — aritmetika modular aman-overflow (`mulMod` doubling, `powMod`, `modInverse` via Bezout), teori bilangan (sieve `primesUpTo`, `primeCount`, `nthPrime`, `factorize`, σ/φ/radikal/Möbius, prima/abundant/deficient), kombinatorika (`binomial`, `permutation`, `catalan`, `derangement`, `stirling2`, `bell`, Lucas/Tribonacci), bit (`popcount`, `highestBit`, `reverseBits`, `nextPow2`), akar/log bulat (`sqrt`, `cbrt`, `log2`, `log10`), geometri integer (`heron16`, `isRightTriangle`, jarak). Modul baru `Digits` (palindrom/Armstrong/akar digital), `Stats` (median/modus/varians/simpangan baku), `Matrix` (aljabar matriks persegi). Uji oracle python: **154 kasus** di `tools/gen_mathvec.py`, lulus di VM & a64, sintaks x64 diperiksa; total suite 63/63 |

## Bangun

```sh
cd ~/kopi
make          # menghasilkan kopicc (compiler) dan kopivm (mesin virtual)
make test     # uji konsistensi backend vm & a64 terhadap referensi
```

## Coba cepat

```sh
# 1. compile ke BINARY asli (aarch64 — jalan langsung di HP/termux ini)
./kopicc examples/hello.kop -t a64 -o hello.s
cc hello.s -o hello               # rakit + link pakai cc, seperti C
./hello                           # → Halo dunia dari Kopi VM and compiler!

# 2. compile ke BYTECODE lalu jalankan lewat mesin virtual
./kopicc examples/hello.kop -t vm -o hello.kbc
./kopivm hello.kbc                # → sama, tapi harus lewat kopivm

# 3. untuk PC x86-64 (WSL/Linux desktop):
./kopicc examples/demo.kop -t x64 -o demo.s
gcc demo.s -o demo && ./demo

# 4. program multi-file (contoh import):
./kopicc examples/belanja.kop -t vm -o belanja.kbc
./kopivm belanja.kbc   # → total 45000, ongkir 12000, grand total 57000
```

Lihat `examples/demo.kop`: rekursi (`fib`, `faktorial`), `if/else`,
`while`, `for`, `break/continue`, operator boolean short-circuit, &&
class-method →
`fib(20) = 6765`, `faktorial(10) = 3628800`, dsb — dieksekusi direkt oleh
CPU.

## Bahasa

```java
class Calc {
    static int add(int a, int b) { return a + b; }
}

class Main {
    static boolean isEven(int n)   { return n % 2 == 0; }
    static void print(int x)       { System.out.println(x); }

    public static void main(String[] args) {
        print(Calc.add(40, 2));            // panggil lintas-class static
        for (int i = 0; i < 10; i++) {
            if (isEven(i) && i != 4) { continue; }
            System.out.println(i);
        }
    }
}
```

Fitur v1.0 (semua sudah diuji di backend a64 maupun vm):

| Fitur                             | Keterangan                                        |
|-----------------------------------|---------------------------------------------------|
| `class`, metode `static`          | multi-class, panggilan lintas-class `Cls.nama()`  |
| tipe `int`, `boolean`, `void`     | checker tipe sederhana (deteksi salah tipe)       |
| array `int[]`                     | `new int[n]`, `a[i]`, `a[i] = v`, `a.length` — heap arena `kopalloc` + bounds check runtime |
| aritmetika                        | `+ - * / %` (semantik Java: trunc ke 0)           |
| pembanding                        | `< > <= >= == !=`                                 |
| logika                            | `&& || !` dengan **short-circuit**                |
| kontrol alir                      | `if/else`, `while`, `for`, `break/continue`, `else if` |
| `++/-=` dsb                       | `x++`, `x--`, `x += -= *= /= %=` (per-statement/for-step) |
| lambda `fn`                       | `x => expr`, `(a,b) => { ... }`, `() => ...` + capture by-value |
| rekursi                           | `fib`, `faktorial`                                |
| multi-file + `import`             | gabung banyak `.kop` jadi satu compilation unit   |
| laporan error                     | `kopicc: error: file: baris N: pesan`             |
| `System.out.println/print`        | string literal atau term int/boolean              |
| komentar                          | `//` dan `/* ... */`                              |

## Lambda (`fn` / arrow function)

Kopi punya lambda sejati (closure) — bukan hanya callback tanpa memori:

```java
class Main {
    static int terapkan(fn f, int x) { return f(x); }
    static int gabung(fn g, int a, int b) { return g(a, b); }

    public static void main(String[] args) {
        int faktor = 3;
        fn p = x => x * faktor;          // lambat memanggil apa; capture faktor
        System.out.println(p(10));       // 30
        System.out.println(terapkan(p, 7));  // 21 — lambda sebagai argumen
        System.out.println(gabung((a, b) => a - b, 40, 2));  // 38 — inline
        fn q = (a, b) => { int r = a; r += b; return r; };   // badan pernyataan
        System.out.println(gabung(q, 40, 2));  // 42
        fn b = () => 1;                  // tanpa parameter / tanpa capture
        System.out.println(b());         // 1
    }
}
```

Cara kerja:

- tipe `fn` = nilai closure (bisa disimpan, jadi parameter, dipanggil `f(x)`)
- lambda = **metode tersembunyi** `Lam_<Class>_<n>` dengan param pertama `$env`
- capture = **snapshot by-value** saat lambda dibuat (ala Java effectively-final,
  tapi lebih permisif): lambat menulis variabel luar hanya menyentuh salinan
  privatnya
- layout closure 64-bit di ketiga backend: `[0]=kode [1]=ncapt [2]=nuser
  [3+j]=capture` — arity dicek runtime (error rapi, bukan SIGSEGV)
- batas: maks 5 argumen pemanggilan `f(a,b,...)` dan maks 32 capture;
  kembalian lambda masih `int`/`boolean`; return lambda belum didukung

## Arena memori & array `int[]` (v1.2)

Semua alokasi dinamis (elemen array, closure) masuk satu **arena bump
allocator** — sederhana, cepat, tanpa free-per-objek:

```
kopalloc(n):  used += n; if (used > 8MB) die("arena memori habis");
              return base + (used - n);
```

- **Native a64/x64**: `base` = satu blok `malloc(1<<23)` di entry `main` (stub
  `main` punya **frame sendiri** — tanpa itu, `bl malloc` menimpa `x30` dan
  `ret` di class-main mengeksekusi ulang `main` = loop; bug ABI yang
  menarik!). Limit 8 MB, lalu pesan rapi + exit 1, bukan SIGSEGV.
- **VM**: arena 4 MB di heap `kopivm`, semantik penuh.
- layout array identik di semua backend: `[0]=length, [8+8*i]=elemen` —
  elemen integer 32-bit disimpan sign-extended 8 byte penuh, jadi layout
  pointer array portable antar backend.
- bounds check runtime: indeks negatif/`>= length` →
  `kopi: array tidak valid (indeks/ukuran salah)` (cek unsigned di a64:
  `cmp; b.hs` menutupi negatif sekaligus lewat `lsl #3` di atas elemen 64-bit).

```java
class Data {
    static int jumlah(int[] xs) {
        int s = 0;
        for (int i = 0; i < xs.length; i++) s += xs[i];
        return s;
    }
    public static void main(String[] args) {
        int[] a = new int[10];
        for (int i = 0; i < a.length; i++) a[i] = i * i;
        System.out.println(jumlah(a));          // 285
    }
}
```

Contoh penuh: `examples/array.kop` (81, 285, 11, 0 — diuji vm & a64
byte-per-byte sama).

## Import & multi-file

Kopi mendukung pemisahan program ke beberapa file. Modelnya **compile-time
merge** ala `#include` (bukan separate compilation per-object): semua file
yang terlibat digabung jadi satu compilation unit, jadi tidak ada format
object file tambahan — dan tidak ada linker yang perlu ditulis (dulu).

Dua cara mengimpor, keduanya diuji di semua backend:

```java
// belanja.kop — program utama
import "keranjang.kop";   // Form A: path eksplisit, relatif ke BELANJA.KOP sendiri
import Keranjang;         // Form B: dicari Keranjang.kop / keranjang.kop
                          //         di folder file yang mengimpor

import java.util.Scanner; // ✗ library eksternal → error jelas dari compiler
```

Dan **export tidak butuh deklarasi**: semua class/metode `static` di file
yang diimpor otomatis bisa dipanggil (`Keranjang.total(15000, 3)` dll).

Cara lain tanpa `import` sama sekali — sebutkan semua file di CLI:

```sh
./kopicc utama.kop labib.kop -t vm -o utama.kbc
./kopivm utama.kbc
```

Aturan penting:

- File yang sama (dibanding via `realpath`) hanya dimasukkan **sekali** —
  import melingkar ganda (A → B → A) tidak membengkak dan tidak crash.
- Definisi metode duplikat (file diimpor dua kali lewat nama berbeda)
  terdeteksi: `metode 'Cls.nama' dideklarasikan dua kali`.
- Error runtime/compile menunjuk file & baris asal token:
  `kopicc: error: examples/util.kop: baris 12: pesan`.
- Satu program tetap butuh persis satu `main` (`static void main`
  di class mana pun), dan `main` boleh ada di file yang diimpor.

Contoh lengkap import: `examples/belanja.kop` + `examples/keranjang.kop`
(backend vm & a64 sudah diverifikasi outputnya identik oleh `test.sh`).

## Library standar (ditulis dalam bahasa Kopi sendiri)

Folder `lib/` berisi library yang bisa diimpor langsung — semua 100% bahasa
Kopi, bukan kode built-in compiler:

| File              | Isi                                                                 |
|-------------------|---------------------------------------------------------------------|
| `lib/core/Math.kop`    | **dasar** `abs sign min max min3 max3 clamp diff sumTo avgFloor`; **bit** `isEven isOdd popcount highestBit lowestBit bitLength isPowerOfTwo nextPow2 prevPow2 reverseBits`; **pangkat/akar/log** `pow mod addMod mulMod powMod sqrt cbrt log2 log10 isPerfectSquare isPerfectCube`; **FPB/modular** `gcd gcd3 lcm lcm3 isCoprime extendedGcd modInverse`; **prima** `isPrime nextPrime prevPrime primesUpTo primeCount nthPrime factorize numDivisors sumDivisors aliquotSum isPerfect isAbundant isDeficient eulerPhi radical mobius`; **kombinatorika** `factorial binomial permutation catalan fib lucas tribonacci triangular derangement stirling2 bell`; **geometri** `distanceSq manhattan rectArea rectPerimeter isRightTriangle heron16 isTriangle` |
| `lib/core/Digits.kop`  | `reverse digitSum digitCount pow10 digitProduct isPalindrome isArmstrong digitalRoot sumSquareDigits sumCubeDigits printBase` |
| `lib/core/Stats.kop`   | `sum product min max range mean sumSquares sumCubes median mode variance stddev countEven countOdd` |
| `lib/core/Matrix.kop`  | `zero identity add sub scale transpose mul trace det1 det2 det3 det det isIdentity` (matriks persegi, `int[]` datar) |
| `lib/core/Arrays.kop`     | `sum max min find count arrEq take drop merge sort` (merge sort O(n log n)) + `binSearch` |
| `lib/crypto/Crypto.kop`   | `bitxor` (per-bit, tanpa operator bitwise), `fnv1a` (FNV-1a 32-bit), `nextRand`/`randRange` (Lehmer/Schrage), `xorStream` (XOR cipher nirl), `caesar`/`uncaesar` |

### Batas jujur pustaka matematika

Semua fungsi di atas bekerja pada **`int` 32-bit bertanda** (semantik Java):
hasil yang melampaui ±2³¹ **membungkus (wrap)**, bukan melempar galat —
rentang aman didokumentasikan per fungsi. Belum ada `long`/`double`, jadi
**tidak ada** trigonometri, logaritma real, pecahan, atau bilangan besar
umum; `sqrt`/`cbrt`/`log2`/`log10` adalah versi **bulat**. `primesUpTo`/
`primeCount` memakai sieve O(n) memori; `Matrix` hanya matriks persegi dan
`det` umum dibatasi n ≤ 3. Untuk kriptografi, pakai `lib/crypto/` yang punya
aritmetika modular khusus (`Big`, `X25519`) — bukan `Math` umum.

`Arrays` dan `Crypto` adalah bukti paling kuat bahasanya benar-benar
*bahasa*: keduanya ditulis 100% dalam Kopi murni, tanpa satu pun builtin
baru — `bitxor` misalnya mengekstrak bit dengan `a % 2` / `a / 2` lewat satu
loop 31 iterasi, karena fungsi v1.2 belum punya operator `^ & | <<`.
Konvensi byte = `int` 0..255. Kualitas: cocok untuk hash-cache/PRNG/cipher
edukatif; **bukan** kriptografi produksi. Ditulis sebelum operator bitwise
native lahir — kini `& | ^ << >> >>>` sudah tersedia di bahasa. Hak
intelektual fungsi tetap standar (FNV-1a, Lehmer).

```java
import Math;
import Digits;

class Main {
    public static void main(String[] args) {
        System.out.println(Math.gcd(12, 18));   // 6
        Digits.printBase(42, 2);                // 101010
        System.out.println("");
    }
}
```

Urutan pencarian `import Nama`:

1. folder file pengimpor — `Nama.kop`, lalu `nama.kop` (lokal menang)
2. **lib standar proyek**: folder `lib/` di samping `kopicc`
3. bisa dioverride lewat variabel lingkungan: `KOPPATH=/jalan/ke/lib ./kopicc prog.kop`

Contoh siap pakai: `examples/pakai_lib.kop` (→ diuji di semua backend oleh
`test.sh`).

## Klien TLS 1.3 (`lib/net/Tls.kop`)

Klien TLS 1.3 ditulis **100% dalam bahasa Kopi** di atas pustaka kripto
murni Kopi (`X25519`, `ChaCha`, `Poly`, `Aead`, `Kdf`, `Hash`, `Der`, `Rand`).
Tidak ada OpenSSL/libc kripto — hanya syscall `Net.*` untuk transport.

```java
import Tls;

class Main {
    public static void main(String[] args) {
        int[] pin = new int[32];              // SHA-256(DER SPKI leaf) — TOFU
        // ... isi 32 byte pin dari kredensial server yang dipercaya ...

        int[] ctx = Tls.gate(2130706433, 443, pin);   // 127.0.0.1:443
        if (ctx[1] != 1) {                             // 1 = handshake penuh + SF valid
            System.out.println(ctx[6]);                // kode tahap/galat
            return;
        }
        System.out.println(Tls.writeApp(ctx, Net.bytes("GET / HTTP/1.0\r\n\r\n")));
        int[] balasan = Tls.readApp(ctx);              // plaintext aplikasi
    }
}
```

`ctx` (int[]): `0` fd, `1` sukses penuh, `2` pin cocok, `3` server-Finished
valid, `4` seq keluar, `5` seq masuk, `8..39` kunci app klien, `40..51` IV
klien, `56..87` kunci app server, `88..99` IV server. API: `Tls.gate(ip,port,pin)`
(acak dari `Rand`), `Tls.gateFixed(ip,port,pin,sk,rnd)` (deterministik untuk uji),
`Tls.writeApp`, `Tls.readApp`.

**Yang benar-benar diverifikasi:**

- **RFC 8448** (1-RTT sample) sebagai oracle key schedule: `ecd`, `early`,
  `derived`, `handshake secret`, `TH`, `s hs traffic` — semua cocok.
- **OpenSSL `s_server`** sungguhan: 4 rekaman server (EE/Cert/CertVerify/
  Finished) terbuka dengan tag yang **sama persis** dengan rekaman asli;
  server menerima ClientFinished dan mengirim **2× NewSessionTicket** lalu
  `close_notify` warning (bukan alert).
- **App data dua arah**: lawan server echo TLS, server mendekripsi
  `kopi ping` dan klien mendekripsi balasan `PONG:kopi ping`.
- Backend **a64 native** dan **VM KOP1** memberi hasil identik.

**Batas jujur (bukan klaim berlebih):**

- Autentikasi memakai **pin SPKI ala SSH/TOFU**: `pin = SHA-256(DER SPKI
  leaf)`. Ini menutup MITM selama pin berasal dari kanal tepercaya, tetapi
  **bukan** validasi rantai sertifikat/PKI (tidak ada CA store, hostname,
  expiry, atau OCSP).
- **CertificateVerify** saat ini hanya masuk transkrip; verifikasi tanda
  ECDSA P-256 adalah agenda berikutnya. Jadi kepercayaan bertumpu pada pin,
  bukan pada pembuktian penguasaan kunci privat oleh server.
- Satu suite cipher (`TLS_CHACHA20_POLY1305_SHA256`), satu grup (`x25519`),
  tanpa session resumption/PSK/0-RTT, tanpa HRR, tanpa kompresi.

**Enam akar bug yang dibereskan di v1.5d-E2** (rincian di changelog):
urutan `HkdfLabel`; carry slot-9 Poly1305 (tag salah data-dependent untuk
pesan panjang); nonce record big-endian + hindari shift ≥32; panjang record
di `seal()`; derivasi `early`/`master`/app secret; dan kunci + inner type
ClientFinished.

## Arsitektur backend

### Native aarch64 (`-t a64`)
- Evaluasi ekspresi: hasil di `w0` (int) atau `x0` (pointer closure/array);
  nilai sementara di-push ke stack (`str w0/x0, [sp, #-16]!` → aman align-16
  untuk setiap `bl`).
- Frame metode: `stp x29, x30`; local variable disimpan di slot
  `[x29, #-8(k+1)]`; parameter `w0..w7` disalin ke jadwal slot. Slot 64-bit
  (`T_FN`/`T_ARI`) di-load/store dengan `ldr x` penuh.
- Pemanggilan tidak melanggar ABI aarch64: SP tetap selaras 16.
- `println` = `printf` lewat `:got:` (PIE, siap dirakit di Android/bionic).
- Entry: symbol `main` → frame sendiri + satu `malloc` arena + `bl
  <Class>__main` (pemanggilan normal: `bl`, bukan `b`), exit code 0.
- Trap runtime: `.Klam` (arity lambda), `.Karr` (bounds array), `.Koom`
  (arena habis) — semuanya printf + exit, bukan SIGSEGV.

### Native x86-64 (`-t x64`)
- Sama bentuknya, AT&T, `pushq/popq`; argumen register SysV `rdi..r9`.
- Hasil int di `%eax`, pointer di `%rax`; elemen array 8 byte dengan
  `cmpl (%r10), %eax; jae .Karr`.
- Track paritas SP saat compile-time supaya setiap `call` selaras 16.

### Mesin virtual KOP1 (`-t vm`)
- Format file: magic `KOP1`, tabel metode (nargs/nlocals/kode), cpool string.
- Stack machine: ~35 opcodes (`ICONST ILOAD ISTORE ADD SUB MUL DIV MOD NEG
  NOT LT GT LE GE EQ NE JMP JMPF JMPT CALL RET PRINT...` + `NEWARR IDXR IDXW
  ALEN` untuk array + `NEWCLO CALLIND ENVLOAD` untuk lambda).
- Semantik int 32-bit: hasil `ADD/SUB/MUL` di-truncate `(int32_t)` supaya
  output identik dengan backend native (semantik wrap ala Java).
- CALL/RET dikelola dengan frame stack sendiri (`kopivm`) — tumpukan operand
  dibagi antar frame dengan `retbase` supaya nilai sementara caller tidak
  tertimpa.
- `DIV/MOD` dengan nol, bounds array, arena habis → pesan runtime error,
  bukan crash panik.

## Struktur proyek

```
kopi/
├── src/
│   ├── lex.c / lex.h      → lexer (token, string unescape, komentar)
│   ├── parse.c            → parser recursive-descent + resolusi simbol
│   │                         (slot variabel, index metode, cpool, tipe)
│   ├── units.c            → penggabungan source: import + multi-file CLI
│   ├── ast.h              → AST + tabel metode/class
│   ├── gen_a64.c          → codegen ARM64
│   ├── gen_x64.c          → codegen x86-64
│   ├── gen_vm.c           → codegen bytecode KOP1
│   └── main.c             → driver perintah kopicc
├── vm/kopivm.c            → mesin virtual KOP1
├── lib/                   → library standar (murni bahasa Kopi, diimpor)
├── examples/*.kop         → contoh program
├── Makefile
└── test.sh                → konsistensi output antar-backend
```

## Roadmap (fondasinya sudah rapi)

- `long` (64-bit), `double`, `char`, `String` sebagai tipe sebenarnya
- operator bitwise `& ^ | << >>` (Crypto bisa diubah jadi native shift) +
  ternary `? :`
- instance class (field, `new`, virtual dispatch / vtable)
- static fields & inisialisasi
- GC proper: saat ini arena = bump allocator yang tidak pernah dibebaskan
  (cukup untuk program batch); mark-sweep sederhana di jadwal
- `switch`, `do/while`, `try/catch` terbatas
- optimasi `-O1`: register allocation (jangan semua lewat stack),
  const-folding
- **UDP & shutdown**: builtin `snd/rcv` (datagram — satu syscall = satu paket)
  dan syscall `shutdown` (half-close) — klien TCP v1.4 berjalan penuh
  tanpa keduanya; server-side `bind/listen/accept` tinggal tiga syscall
  di atas kerangka `Net` yang sama
- **TLS lanjutan** (klien 1.3 sudah ada): verifikasi tanda **ECDSA P-256**
  untuk CertificateVerify, validasi rantai/PKI + hostname + masa berlaku,
  session resumption/PSK, dan dukungan `u64` asli supaya X25519 tidak lagi
  memakai limb 32-bit
- linker kecil sendiri (tanpa `cc`) → output `.so`/`.elf` langsung
