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
| `lib/core/` | logika dasar bahasa | `Math` (lengkap: modular, teori bilangan, kombinatorika, bit, geometri), `Digits`, `Stats` (statistik), `Matrix` (aljabar matriks), `Arrays` (termasuk fungsional), **`Json`** (parse/tulis JSON) |
| `lib/crypto/` | kriptografi | `Crypto`, `Cipher` (klasik: RC4/Vigenère/base64/CRC), `Hash` (SHA-256+HMAC), `Kdf` (HKDF+ExpandLabel), `Big` (medan 2²⁵⁵−19), `X25519`, `ChaCha` (stream), `Poly` (Poly1305), `Aead` (ChaCha20-Poly1305), `Rand` (entropi `/dev/urandom`), `Der` (parser DER minimal + pin SPKI SHA-256) |
| `lib/net/` | jaringan | `Tcp` (transport di atas syscall `Net`), `Dns` (DNS-over-TCP), `Http` (HTTP/1.0), `Rfc` (klien protokol era awal: daytime/echo/quote/whois/finger/discard), **`Udp`** (datagram: bind/sendTo/recvFrom/shutdown), **`Tls`** (klien TLS 1.3 — lihat bagian khusus di bawah) |

ATURAN unik: file test TIDAK boleh bernama sama (kasus tak peduli) dengan
modul yang diimpor — file lokal selalu menang atas `lib/`.

## Status & changelog

```
Pengembangan     ████████████░░░░  ~72%  menuju bahasa serius (v2)
v1.6.5 hari ini  ████████████████  77/77 test hijau — proyek 54 file, eksekusi paralel, ~24 MB RSS, tanpa hang
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
| **v1.6** | **tipe numerik lengkap** (scalar): `long` (64-bit bertanda), `float`/`double` (IEEE-754), `uint8`/`uint16`/`uint32`/`uint64` (unsigned dengan wrap sesuai lebar), literal `L/U/UL/f/d`, hex `0x`, biner `0b`, cast `(tipe)expr`, promosi campuran (int+uint32→long, uint32+uint32→uint32, dst), perbandingan signed/unsigned, `%` float, `println` format per tipe, parameter/return semua tipe, compound assignment (narrowing implisit ala Java). Backend a64 + VM menghasilkan output identik; x64 diperiksa sintaksnya. Uji oracle python `tools/gen_numvec.py` (66 kasus), suite 65/65. Batas jujur: array masih `int[]` (belum `long[]`/`double[]`), lambda masih `int`, dan backend x64 belum dijalankan di mesin x86-64 |
| **v1.6.1** | **`byte`/`int8` (8-bit bertanda) dan `short`/`int16` (16-bit bertanda)** — melengkapi `long`/`float`/`double`/`uint8..64`. Aritmetika wrap sesuai lebar + sign-extend kanonik, cast, literal `-128`/`-32768`, parameter/return/cetak signed. Sekaligus memperbaiki **tabrakan nilai token**: seluruh keyword & token multi-karakter dipindah ke ≥256 agar tidak bertabrakan dengan kode ASCII operator 1-karakter (`%`, `&`, `!`, …), dan dependensi header ditambahkan ke Makefile. Uji numvec diperluas (byte/short) + suite 65/65 |
| **v1.6.2** | **UDP + HTTP header**: builtin baru `Net.bind`, `Net.sendto`, `Net.recvfrom`, `Net.shutdown` (syscall langsung di a64/x64, jalur host di VM) + modul `lib/net/Udp.kop` (`open/bind/sendTo/recvFrom/shutdown/close`), sumber datagram dicatat di `from[0]` (ip) / `from[1]` (port). `Tcp.shutdown` ditambahkan. `lib/net/Http.kop` diperluas: `reqEx`, `requestInto`, `getWith`, `postWith` (header permintaan kustom) serta `headerBlock`, `headerValue` (case-insensitive, nilai di-trim), `hasHeader` untuk membaca header respons. Backend x64 kini mendukung **8 argumen** (arg 7–8 lewat stack). Uji: UDP loopback bolak-balik (bind+sendto+recvfrom) dan server HTTP yang memeriksa header kustom + respons header, di VM & a64; sintaks x64 diperiksa. Batas jujur: datagram maks **240 byte** per kirim/terima (penyangga syscall v1) |
| **v1.6.3** | **array fungsional + gula metode**: `Arrays.forEach`, `forEachIndexed`, `map`, `filter`, `reduce`, `any`, `all`, `countWhere`, `findIndex`, `sumBy`, `copy`, `reverse` (semuanya memakai lambda Kopi). Sintaks metode `a.forEach(f)`, `a.map(f)`, dst. didukung lewat desugaring ke `Arrays.*` (butuh `import Arrays;`). Perbaikan array: `Arrays.max/min` tidak lagi meledak pada array kosong, `take` meng-clamp `n < 0`. Uji baru `arrayfn` dan `arrchk` (return array besar 300 elemen, `new int[0]`, identitas) di VM & a64 + sintaks x64; suite 73/73 |
| **v1.6.4** | **JSON native**: literal `json {...}` / `json [...]` (sampai 8 entri; lebih dari 4 pasangan objek dirangkai via `Json.put`), nilai berupa tape `int[]` sehingga bisa disimpan di variabel, dipassing, dan dikembalikan fungsi. Pustaka `lib/core/Json.kop`: `parse`, `write`, `print`/`line`, `typeOf`/`isNull`/`isBool`/`isNumber`/`isString`/`isArray`/`isObject`, `asInt`/`asDouble`/`asBool`/`asText`, `count`/`at`/`get`/`has`, `put`/`del`/`add` (immutable), konstruktor `nullV`/`boolV`/`num`/`str`/`arrN`/`objN`. Angka disimpan sebagai teks sumber (format dipertahankan), string di-escape saat ditulis, `\uXXXX` didekode ke UTF-8 (tanpa pasangan surrogate). Parser mengenal token `:`; uji `jsontest` di VM & a64 + sintaks x64; suite 75/75 |
| **v1.6.5** | **uji ketangguhan proyek besar** `examples/proyekbesar/`: **54 file `.kop`** (53 modul + main) saling `import`, memakai pustaka (`Math`, `Arrays`, `Stats`, `Matrix`, `Json`, `Cipher`, `Hash`) dan modul lokal (struktur data, algoritma, CSV/JSON, inventory/order/billing, lexer/compiler/evaluator/VM mini). Harness `bench.py`: kompilasi a64/VM/x64 **paralel**, eksekusi a64 & VM **paralel** dengan timeout 120 s, sampling `VmHWM` untuk puncak RSS, banding output byte-per-byte, lalu **4 proses a64 paralel**. Hasil acuan: kompilasi ~0,13 s; a64 ~0,14 s, VM ~0,94 s; puncak RSS ~24 MB; 102 baris output identik; tanpa hang/OOM (fase memori ≈21,6 MB payload pada arena/heap 32 MB). Juga ditambahkan `Json.write` (publik) dan `proyekbesar` masuk `test.sh` (a64/VM/x64) |

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
| tipe numerik                      | `int` (32-bit), `long` (64), `float`/`double`, `uint8/16/32/64`; literal `L/U/UL/f/d`, hex `0x`, biner `0b`, cast `(tipe)x` |
| tipe `boolean`, `void`            | checker tipe sederhana (deteksi salah tipe)       |
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

## Tipe numerik (v1.6)

Kopi kini punya tipe angka lengkap (masih **scalar**; array tetap `int[]`):

| Tipe | Lebar | Semantik | Contoh literal |
|------|-------|----------|----------------|
| `int` | 32 | bertanda, wrap | `42`, `0x2A`, `0b101010` |
| `long` | 64 | bertanda, wrap | `42L`, `0x1F4L` |
| `float` | 32 | IEEE-754 binary32 | `1.5f` |
| `double` | 64 | IEEE-754 binary64 | `1.5`, `1.5d`, `1e-3` |
| `byte` / `int8` | 8 | bertanda, wrap | `-128`, `(byte)200` |
| `short` / `int16` | 16 | bertanda, wrap | `-32768`, `(short)70000` |
| `uint8` | 8 | unsigned, wrap | `255`, `(uint8)300` |
| `uint16` | 16 | unsigned, wrap | `65535` |
| `uint32` | 32 | unsigned, wrap | `4294967295U`, `0xFFFFFFFFU` |
| `uint64` | 64 | unsigned, wrap | `18446744073709551615UL` |

```java
long   total = 5000000000L;
byte   b     = (byte)200;              // 200 → -56 (wrap 8-bit bertanda)
short  s16   = (short)70000;           // → 4464 (wrap 16-bit)
uint8  kecil = (uint8)(250 + 10);      // 260 → 4 (wrap 8-bit)
uint32 besar = 4294967290U + 10U;      // → 4
double rasio = (double)7 / 2.0;        // 3.5
float  f     = 1.5f + 2.25f;           // 3.75
```

**Aturan konversi**

- Pelebaran implisit (tanpa cast): `uint8→uint16→uint32→uint64`,
  `int→long`, dan semua integer → `float`/`double`, `float→double`.
- Penyempitan (`long→int`, `double→int`, `int→uint8`, …) **wajib cast**
  eksplisit `(tipe)x`, kecuali:
  - **literal** yang nilainya muat (`uint8 x = 250;`, `f(200)` untuk
    parameter `uint8`), dan
  - **compound assignment** (`x++`, `x += 1`) yang memakai semantik cast
    implisit seperti Java.
- Campuran signed/unsigned: `int + uint32` → `long`; `uint32 + uint32` →
  `uint32`; `uint64 + long` → `long` (reinterpretasi bit 64-bit).
  Aturan ini eksplisit (Java tidak punya unsigned) dan diuji oracle.
- `%` untuk `float`/`double` = sisa truncasi (`a - trunc(a/b)*b`).
- `println` memakai format per tipe: `%ld` (long), `%lu` (uint64),
  `%u` (uint8/16/32), `%.9g` (float), `%.15g` (double).

**Batas jujur**

- Array baru tersedia untuk `int[]`; `long[]`, `double[]`, dll. belum.
- Lambda (`fn`) masih menerima/mengembalikan `int` saja.
- Overflow membungkus (wrap), tidak melempar galat; `float→int` memakai
  truncation + saturasi backend (perilaku nilai ekstrem/NaN didokumentasikan
  sebagai batas, bukan jaminan lintas-CPU).
- Backend **x64** menerima tipe baru (assembly lolos `cc -target`, diuji
  sintaks); eksekusi runtime baru diverifikasi di **aarch64** dan **VM**.

## JSON (v1.6.4)

JSON adalah nilai kelas satu: literal `json { ... }` / `json [ ... ]`
menghasilkan **tape `int[]`** yang bisa disimpan di variabel, dipassing ke
fungsi, dikembalikan, atau ditaruh di array lain.

```java
import Json;

class Main {
    public static void main(String[] args) {
        int[] j = json {
            "nama": "budi",
            "umur": 20,
            "aktif": true,
            "nilai": [1, 2.5, null]
        };
        Json.line(j);                       // {"nama":"budi","umur":20,...}

        int[] umur = Json.get(j, Net.bytes("umur"));
        System.out.println(Json.asInt(umur));          // 20

        int[] nilai = Json.get(j, Net.bytes("nilai"));
        System.out.println(Json.count(nilai));         // 3
        System.out.println(Json.asDouble(Json.at(nilai, 1))); // 2.5
        System.out.println(Json.isNull(Json.at(nilai, 2)));   // 1

        int[] j2 = Json.put(j, Net.bytes("umur"), Json.num(Net.bytes("21")));
        int[] j3 = Json.del(j2, Net.bytes("aktif"));
        int[] lebih = Json.add(nilai, Json.str(Net.bytes("x")));

        int[] dariTeks = Json.parse(Net.bytes("{ \"a\": [true, 1e3] }"));
        if (dariTeks.length == 0) { /* teks JSON tidak valid */ }
    }
}
```

API ringkas:

| Kelompok | Fungsi |
|---|---|
| Baca | `parse`, `typeOf`, `isNull/isBool/isNumber/isString/isArray/isObject`, `count`, `at`, `get`, `has`, `asInt`, `asDouble`, `asBool`, `asText` |
| Tulis | `write` (tape teks), `print`/`line` (langsung ke stdout), `put`/`del`/`add` (immutable, hasil baru) |
| Bangun | `nullV`, `boolV`, `num`, `str`, `arr0..arr8`, `obj0..obj4` |

Batas jujur:

- Literal `json {...}` maksimal **8 pasangan** (objek >4 dirangkai otomatis
  dengan `Json.put`), array literal maksimal **8 elemen**.
- Angka disimpan sebagai **teks sumber**, jadi `1.50` tetap ditulis `1.50`;
  `asInt` memotong bagian pecahan, `asDouble` menghitung nilai double.
- `put`/`add`/`del` bersifat immutable (menyalin tape) — mudah dipakai,
  O(n²) untuk penambahan berulang; cocok untuk dokumen kecil/menengah.
- `\uXXXX` didekode ke UTF-8 sederhana (pasangan surrogate belum digabung).
- `Json.print/line` menulis langsung ke fd 1 (byte-per-byte), sedangkan
  `System.out.println` memakai buffer stdio: bila dicampur, urutan output
  bisa berbeda; gunakan salah satu gaya per blok.

## Array fungsional & `.forEach` (v1.6.3)

`lib/core/Arrays.kop` kini punya gaya fungsional memakai lambda Kopi.
Dua bentuk pemanggilan sama-sama sah:

```java
import Arrays;

class Main {
    public static void main(String[] args) {
        int[] a = new int[5];
        for (int i = 0; i < 5; i++) a[i] = i + 1;      // 1..5

        Arrays.forEach(a, x => { System.out.println(x); });   // pustaka
        a.forEach(x => { System.out.println(x * 10); });      // gula metode

        int[] b  = a.map(x => x + 1);                  // 2..6
        int[] ev = a.filter(x => x % 2 == 0);          // {2,4}
        int s    = a.reduce(0, (acc, x) => acc + x);   // 15

        System.out.println(a.any(x => x > 4));         // 1
        System.out.println(a.all(x => x > 0));         // 1
        System.out.println(a.countWhere(x => x % 2 == 1)); // 3
        System.out.println(a.findIndex(x => x == 3));  // 2
        System.out.println(a.sumBy(x => x * x));       // 55

        int batas = 3;                                 // capture by-value
        System.out.println(a.countWhere(x => x > batas)); // 2
    }
}
```

Aturan:

- Callback menerima `int` (atau `(indeks, nilai)` untuk
  `forEachIndexed`); nilai kembalian lambda dipakai oleh `map`/`reduce`/
  `sumBy`, sedangkan `forEach` mengabaikannya.
- Predikat dianggap **benar bila != 0** (lambda boleh mengembalikan
  `boolean`; compiler memperlakukan hasil panggilan dinamis sebagai int).
- Gula `a.map(...)` **didahulukan** bila `a` adalah variabel bertipe
  `int[]`; jika tidak, `a.map` diperlakukan sebagai pemanggilan
  `class.method` seperti biasa. `import Arrays;` tetap wajib agar metode
  `Arrays.*` ada.
- `Arrays` tidak mengubah array masukan; `map`/`filter`/`copy`/`reverse`
  mengembalikan array baru. `sort` tetap mengembalikan array terurut baru.

Perbaikan pada kesempatan ini: `Arrays.max/min` mengembalikan 0 untuk
array kosong (sebelumnya membaca `xs[0]`), `take` meng-clamp `n < 0`, dan
regresi `return` array (300 elemen), `new int[0]`, serta identitas array
diuji di kedua backend (`arrchk`).

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
| `lib/core/Arrays.kop`     | `sum max min find count arrEq take drop merge copy reverse sort` (merge sort O(n log n)) + `binSearch`; **fungsional**: `forEach forEachIndexed map filter reduce any all countWhere findIndex sumBy` |
| `lib/core/Json.kop`       | `parse write print line typeOf isNull isBool isNumber isString isArray isObject asInt asDouble asBool asText count at get has put del add nullV boolV num str arrN objN` |
| `lib/crypto/Crypto.kop`   | `bitxor` (per-bit, tanpa operator bitwise), `fnv1a` (FNV-1a 32-bit), `nextRand`/`randRange` (Lehmer/Schrage), `xorStream` (XOR cipher nirl), `caesar`/`uncaesar` |

### Batas jujur pustaka matematika

Pustaka `Math`/`Stats`/`Matrix` masih bekerja pada **`int` 32-bit bertanda**:
hasil yang melampaui ±2³¹ **membungkus (wrap)**, bukan melempar galat —
rentang aman didokumentasikan per fungsi. Bahasa kini punya `long`, `float`,
`double`, dan `uint8..64` (v1.6), tetapi fungsi pustaka ini belum dinaikkan
ke tipe tersebut; trigonometri/log real juga belum ada. `sqrt`/`cbrt`/
`log2`/`log10` adalah versi **bulat**. `primesUpTo`/`primeCount` memakai
sieve O(n) memori; `Matrix` hanya matriks persegi dan `det` umum dibatasi
n ≤ 3. Untuk kriptografi, pakai `lib/crypto/` yang punya aritmetika modular
khusus (`Big`, `X25519`) — bukan `Math` umum.

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

## Jaringan: TCP, UDP, HTTP (v1.6.2)

Builtin `Net` adalah syscall Linux langsung tanpa libc. Set lengkapnya:

| Builtin | Arti |
|---|---|
| `Net.socket(domain, type)` | `type` 1 = TCP, 2 = UDP |
| `Net.connect(fd, ipBE, port)` | sambung/atur peer |
| `Net.bind(fd, ipBE, port)` | ikat alamat lokal (server UDP) |
| `Net.sendto(fd, ipBE, port, buf)` | kirim satu datagram → n / -errno |
| `Net.recvfrom(fd, buf, from)` | terima satu datagram; `from[0]`=ip, `from[1]`=port |
| `Net.sendb` / `Net.recvb` | TCP: kirim/terima buffer (byte-as-int) |
| `Net.rdb` / `Net.wrb` | baca/tulis satu byte |
| `Net.shutdown(fd, how)` | half-close: 0=RD, 1=WR, 2=RDWR |
| `Net.openr`, `Net.close`, `Net.bytes` | file & literal byte |

### UDP — `lib/net/Udp.kop`

```java
import Udp;

class Main {
    public static void main(String[] args) {
        int srv = Udp.open();
        Udp.bind(srv, Udp.loopback(), 9918);

        int cli = Udp.open();
        Udp.sendTo(cli, Udp.loopback(), 9918, Net.bytes("HALO"));

        int[] buf  = new int[240];
        int[] from = new int[2];
        int n = Udp.recvFrom(srv, buf, from);   // buf[0..n-1], from[0]=ip, from[1]=port
        Udp.sendTo(srv, from[0], from[1], Net.bytes("BALAS"));
    }
}
```

`Udp.sendTo`/`Udp.recvFrom` memakai **satu datagram per panggilan**. Batas
v1: payload yang dikirim/diterima maksimal **240 byte** (penyangga syscall
`kop_iobuf` 256 byte); lebih dari itu dipotong. Tidak ada
retransmisi/urutan (memang sifat UDP).

### HTTP — `lib/net/Http.kop`

Selain `getInto`/`postInto`/`statusOf`/`bodyOf`, tersedia permintaan
dengan header kustom dan pembacaan header respons:

```java
import Tcp;
import Http;

int[] headers = Net.bytes("X-Kopi: 123\r\nUser-Agent: Kopi/1\r\n");
int[] buf = new int[2048];
int n = Http.getWith(Tcp.loopback(), 8080, Net.bytes("127.0.0.1"),
                     Net.bytes("/halo"), headers, buf);

int[] resp = /* potong buf sebanyak n */;
System.out.println(Http.statusOf(resp));                       // 201
System.out.println(Http.hasHeader(resp, Net.bytes("Content-Length")));
int[] v = Http.headerValue(resp, Net.bytes("content-length")); // "5" sebagai byte
```

- `headers` adalah blok mentah baris `"Nama: nilai\r\n"` (boleh kosong).
- `headerValue` membandingkan nama **case-insensitive**, memangkas spasi
  tepi, dan mengembalikan array kosong bila tidak ada.
- `postWith` menambahkan `Content-Length` otomatis; `requestInto` menerima
  method/body bebas.
- Masih HTTP/1.0: server menutup koneksi setelah respons (tanpa keep-alive,
  chunked, atau HTTP/2).

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

## Uji ketangguhan: `examples/proyekbesar`

Proyek contoh berisi **54 file `.kop`** yang saling import (53 modul + main),
mencakup pustaka standar dan modul lokal: struktur data, algoritma, CSV/JSON,
inventory/order/billing, lexer→compiler→evaluator, dan VM mini. Semua
deterministik (PRNG seed tetap, tanpa jaringan/waktu).

```sh
cd examples/proyekbesar
./run.sh
```

`bench.py` melakukan:

1. kompilasi **a64, VM, x64 paralel** (x64 diperiksa sintaksnya);
2. eksekusi **a64 & VM paralel** dengan **timeout 120 s** (deteksi hang)
   dan sampling `VmHWM` (puncak RSS);
3. perbandingan output byte-per-byte;
4. **4 proses a64 paralel** dengan output harus tetap identik.

Hasil acuan di Termux aarch64:

| Metrik | a64 | VM |
|---|---|---|
| kompilasi | ~0,13 s | ~0,13 s |
| eksekusi | ~0,14 s | ~0,94 s |
| puncak RSS | ~24 MB | ~24 MB |
| output | 102 baris | 102 baris (identik) |

Fase memori mengalokasikan 3×400.000 + 3×500.000 elemen `int[]`
(≈21,6 MB payload) di atas arena native / heap VM 32 MB — tidak OOM.
Empat proses a64 paralel selesai < 0,3 s tanpa hang, semua output sama.
`expected.out` menjadi acuan dan dipakai di `test.sh` utama.

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

- `char`, `String` sebagai tipe sebenarnya; **array bertipe** (`long[]`, `double[]`, ...); lambda dengan tipe numerik baru
- operator bitwise `& ^ | << >>` (Crypto bisa diubah jadi native shift) +
  ternary `? :`
- instance class (field, `new`, virtual dispatch / vtable)
- static fields & inisialisasi
- GC proper: saat ini arena = bump allocator yang tidak pernah dibebaskan
  (cukup untuk program batch); mark-sweep sederhana di jadwal
- `switch`, `do/while`, `try/catch` terbatas
- optimasi `-O1`: register allocation (jangan semua lewat stack),
  const-folding
- **TCP server** (`listen`/`accept`), datagram UDP > 240 byte, dan
  HTTP/1.1 (keep-alive, chunked, header respons berulang)
- **TLS lanjutan** (klien 1.3 sudah ada): verifikasi tanda **ECDSA P-256**
  untuk CertificateVerify, validasi rantai/PKI + hostname + masa berlaku,
  session resumption/PSK, dan dukungan `u64` asli supaya X25519 tidak lagi
  memakai limb 32-bit
- linker kecil sendiri (tanpa `cc`) → output `.so`/`.elf` langsung
