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
- Evaluasi ekspresi: hasil di `w0`, nilai sementara di-push ke stack
  (`str w0, [sp, #-16]!` → aman align-16 untuk setiap `bl`).
- Frame metode: `stp x29, x30`; local variable disimpan di slot
  `[x29, #-8(k+1)]`; parameter `w0..w7` disalin ke jadwal slot.
- Pemanggilan tidak melanggar ABI aarch64: SP tetap selaras 16.
- `println` = `printf` lewat `:got:` (PIE, siap dirakit di Android/bionic).
- Entry: symbol `main` → `b <Class>__main`, exit code 0.

### Native x86-64 (`-t x64`)
- Sama bentuknya, AT&T, `pushq/popq`; argumen register SysV `rdi..r9`.
- Track paritas SP saat compile-time supaya setiap `call` selaras 16.

### Mesin virtual KOP1 (`-t vm`)
- Format file: magic `KOP1`, tabel metode (nargs/nlocals/kode), cpool string.
- Stack machine: ~28 opcodes (`ICONST ILOAD ISTORE ADD SUB MUL DIV MOD
  NEG NOT LT GT LE GE EQ NE JMP JMPF JMPT CALL RET PRINT...`).
- CALL/RET dikelola dengan frame stack sendiri (`kopivm`) — tumpukan operand
  dibagi antar frame dengan `retbase` supaya nilai sementara caller tidak
  tertimpa.
- `DIV/MOD` dengan nol → pesan runtime error, bukan crash panik.

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

## Roadmap v2 (belum ada, tapi fondasinya sudah rapi)

- `long` (64-bit), `double`, `char`, `String` sebagai tipe sebenarnya
- array `int[]` + heap KOP1 (GC mark-sweep sederhana)
- instance class (field, `new`, virtual dispatch / vtable)
- static fields & inisialisasi
- `switch`, `do/while`, `try/catch` terbatas
- optimasi: register allocation (jangan semua lewat stack), const-folding
- linker kecil sendiri (tanpa `cc`) → output `.so`/`.elf` langsung
