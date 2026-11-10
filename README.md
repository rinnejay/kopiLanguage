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

## Status & changelog

```
Pengembangan  ██████████░░░░░░  ~62%  menuju bahasa serius (v2)
v1.5d-A hari ini ████████████████  53/53 test hijau — HKDF/ExpandLabel RFC 5869+8446 lulus di 3 backend
```

| Versi | Isi |
|-------|-----|
| **v1.0** | core language: class/static, `int/boolean`, aritmetika semantik Java, kontrol alir, multi-file `import`, 3 backend (a64 native, x64 native, VM KOP1) |
| **v1.1** | lambda/closure sejati: capture by-value, `fn` sebagai argumen, arity check runtime, semua backend |
| **v1.2** | **array `int[]` + arena memori `kopalloc`** (bump allocator, 1 batas 8 MB): `new int[n]`, `a[i]`, `a[i] = v`, `a.length`, bounds check runtime (`kopi: array tidak valid`, OOM rapi); fix ABI entry `main` (frame x30 sendiri); pustaka `lib/Arrays.kop` (sort/binSearch/find/count/...) dan `lib/Crypto.kop` (bitxor + FNV-1a + PRNG Lehmer + XOR-stream/Caesar) — **100% ditulis dalam bahasa Kopi** di atas aritmetika murni |
| **v1.2.1** | scoping per-blok (`if/else`, `while`, `for`, blok `{}` telanjang — deklarasi direset di akhir blok, shadow antar cabang diperbolehkan ala sibling) + multi-deklarasi `int a = 0, b = 1;` — library kini bisa memakai nama loop yang sama di metode yang sama |
| **v1.3** | **jaringan**: builtin `Net.*` = syscall Linux langsung tanpa libc — `socket`, `connect`, `rdb` (baca 1 byte), `wrb` (tulis 1 byte), `close`, dan `Net.bytes("literal")` (byte-as-int, dikonversi compile-time). Codegen native a64 (`svc #0`) & x64 (`syscall`) membangun `sockaddr_in` di stack sendiri; VM KOP1 jalur op `OP_NET`. Pustaka `lib/Tcp.kop` (open/send/recvAll/cat/indexOf — 100% Kopi) dan `lib/Http.kop` (klien HTTP/1.0 minimal: statusOf/bodyOf/getInto) + `examples/httpget.kop`; uji loopback (echo + `python -m http.server`) masuk test.sh (33 tes) |
| **v1.5d-A** | **HKDF-SHA256 + ExpandLabel murni Kopi** (`lib/Kdf.kop`): `extract` (`HMAC(salt, IKM)`) dan `expand` (T(i)=HMAC(PRK, T(i−1) ‖ info ‖ i)) lulus kasus 1 RFC 5869 (PRK + OKM 42 byte + OKM 80 byte jalur potong); `expandLabel` (uint16 len ‖ "tls13 " ‖ uint8 len·label ‖ uint8 len·context) sudah sesuai RFC 8446 §7.1 — fondasi key schedule TLS 1.3; jebakan yang ditulis telanjang: byte-panjang harus MENDAHULUI datanya (salah taruh sesudah soalnya lolos tidak — tertangkap ref python hmac)

`test.sh` memverifikasi **konsistensi output antar-backend**: program yang sama
harus menghasilkan output byte-per-byte identik di VM, native aarch64, dan
(sintaks assembly) x86-64.

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
class Matematika {
    static int tambah(int a, int b) { return a + b; }
}

class Main {
    static boolean isGenap(int n)  { return n % 2 == 0; }
    static void cetak(int x)       { System.out.println(x); }

    public static void main(String[] args) {
        cetak(Matematika.tambah(40, 2));   // panggil lintas-class static
        for (int i = 0; i < 10; i++) {
            if (isGenap(i) && i != 4) { continue; }
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
| `lib/Matematika.kop` | `abs min max gcd lcm pangkat akar prima faktorial fib genap`     |
| `lib/Angka.kop`      | `balik` (12345→54321), `jumlahDigit`, `panjang`, `deka`, `cetakBasis` (2..10) |
| `lib/Arrays.kop`     | `sum max min find count arrEq take drop merge sort` (merge sort O(n log n)) + `binSearch` |
| `lib/Crypto.kop`     | `bitxor` (per-bit, tanpa operator bitwise), `fnv1a` (FNV-1a 32-bit), `nextRand`/`randRange` (Lehmer/Schrage), `xorStream` (XOR cipher nirl), `caesar`/`uncaesar` |

`Arrays` dan `Crypto` adalah bukti paling kuat bahasanya benar-benar
*bahasa*: keduanya ditulis 100% dalam Kopi murni, tanpa satu pun builtin
baru — `bitxor` misalnya mengekstrak bit dengan `a % 2` / `a / 2` lewat satu
loop 31 iterasi, karena fungsi v1.2 belum punya operator `^ & | <<`.
Konvensi byte = `int` 0..255. Kualitas: cocok untuk hash-cache/PRNG/cipher
edukatif; **bukan** kriptografi produksi (belum ada operator bitwise & mod
2^32 asli). Hak intelektual fungsi tetap standar (FNV-1a, Lehmer).

```java
import Matematika;
import Angka;

class Main {
    public static void main(String[] args) {
        System.out.println(Matematika.gcd(12, 18));   // 6
        Angka.cetakBasis(42, 2);                      // 101010
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
- **TLS**: bahasa belum punya aritmetika unsigned 64-bit untuk
  pertukaran kunci modern — road: `u64` + bilangan besar
- linker kecil sendiri (tanpa `cc`) → output `.so`/`.elf` langsung
