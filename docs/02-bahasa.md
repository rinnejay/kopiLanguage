# 02 — Bahasa Kopi: tutorial lengkap

Dokumen ini adalah referensi sintaks dan semantik yang berlaku di v1.13.
Semua contoh bisa dikompilasi ke a64 dan VM (kecuali ditandai).

---

## 1. Bentuk program

```java
class Main {
    public static void main(String[] args) {
        System.out.println("halo");
    }
}
```

- `main` boleh `static void main()` atau `static void main(String[] args)`.
- Anggota class bisa **statis** (namespace) atau **instance** (field +
  method `this`, sejak v1.12) — lihat bagian 12.
- Tidak ada `package`, inheritance, interface, enum, atau generics;
  lihat [06 — Batas](06-batas-roadmap.md).

## 2. Komentar

```java
// komentar satu baris
/* komentar
   beberapa baris */
```

## 3. Tipe

| Tipe | Lebar | Catatan |
|---|---|---|
| `boolean` | 1 byte logis | hanya `true`/`false` |
| `byte` / `int8` | 8-bit bertanda | |
| `short` / `int16` | 16-bit bertanda | |
| `int` | 32-bit bertanda | tipe default literal |
| `long` | 64-bit bertanda | sufiks `L` |
| `uint8/uint16/uint32/uint64` | unsigned | sufiks `U` (uint32), `UL` (uint64) |
| `float` | 32-bit IEEE-754 | sufiks `f` |
| `double` | 64-bit IEEE-754 | sufiks `d` opsional |
| `String` | pointer byte-array | representasi sama dengan `int[]` |
| `int[]` | array 64-bit-slot | elemen disimpan sign-extended |
| `fn` | closure | lihat lambda |

```java
int a = 42;
long b = 5000000000L;
double c = 3.14;
float d = 2.5f;
uint32 u = 4000000000U;
byte kecil = (byte)200;       // 200 → -56 (wrap)
int hex = 0xFF;               // 255
int bin = 0b1010;             // 10
```

Literal angka dibaca compile-time dengan pemeriksaan rentang; overflow
aritmetika **membungkus (wrap)**, bukan error. Pembagian integer memotong
ke arah nol; `%` mengikuti tanda kiri (seperti Java).

### Cast & promosi

```java
long l = 7;
int i = (int)l;               // eksplisit, boleh kehilangan presisi
double f = i;                 // pelebaran implisit
int balik = (int)3.99;        // 3 (truncation)
```

Campuran signed/unsigned memakai aturan promosi eksplisit di `ast.h`
(mis. `int + uint32 → long`). Bila ragu, pakai cast eksplisit.

## 4. Variabel & scope

```java
int x = 1;
int y = 2, z = 3;             // multi-deklarasi
x += 4;                       // compound: += -= *= /= %= &= |= ^= <<= >>= >>>=
x++;                          // hanya sebagai statement, bukan ekspresi
```

- Variabel lokal **wajib diinisialisasi** (`int x;` ditolak).
- Scope per blok `{ }`; `if`/`for`/`while` punya scope sendiri.
- Nama variabel tidak boleh dideklarasikan dua kali dalam satu scope.

## 5. Operator

| Kelompok | Operator |
|---|---|
| Aritmetika | `+ - * / %` |
| Perbandingan | `< <= > >= == !=` |
| Logika | `&& || !` (short-circuit) |
| Bitwise | `& | ^ ~ << >> >>>` |
| Ternary | `kondisi ? a : b` |
| Cast | `(tipe)ekspresi` |
| Lain | `++ --` (statement), compound assignment |

```java
int maks = a > b ? a : b;
String kelas = n < 0 ? "negatif" : n == 0 ? "nol" : "positif";
int bit = (x & 0xF0) >>> 4;
```

Cabang ternary harus bertipe sama, atau sama-sama `String`/`int[]`.
Kondisi harus `boolean`.

## 6. Kontrol alir

```java
if (x > 0) { ... } else if (x == 0) { ... } else { ... }

while (kondisi) { ... }

do {
    ...
} while (kondisi);            // badan minimal sekali; break/continue OK

for (int i = 0; i < 10; i++) { ... }
for (;;) { break; }           // init/kondisi/update boleh kosong
```

`break` keluar dari loop terdekat; `continue` lanjut ke iterasi berikut.
Belum ada `switch`, `try/catch`, `throw`, atau `assert`.

## 7. Array

```java
int[] a = new int[10];        // selalu di-zero-kan (termasuk setelah memReset)
a[0] = 7;
for (int i = 0; i < a.length; i++) a[i] = i * i;

int[] b = a;                  // alias, bukan salinan
int[] c = Str.copy(a);        // salinan (byte-array helper)
```

- Indeks di luar `0..length-1` menghentikan program dengan pesan
  `array tidak valid (indeks/ukuran salah)`.
- Array bisa diparamkan/dikembalikan; ukuran tetap setelah dibuat.
- Belum ada array literal (`{1,2,3}`) atau array bertipe (`String[]`,
  `long[]`); gunakan `int[]`.

### Metode fungsional array (gula)

```java
int[] a = new int[5];
for (int i = 0; i < a.length; i++) a[i] = i + 1;

static void cetak(int x) { System.out.println(x); }
...
a.forEach(x => cetak(x));
int[] b = a.map(x => x * 2);
int[] g = a.filter(x => x % 2 == 0);
int s = a.reduce(0, (acc, x) => acc + x);
boolean ada = a.any(x => x > 4);
int idx = a.findIndex(x => x == 3);
```

Daftar: `forEach forEachIndexed map filter reduce any all countWhere
findIndex sumBy copy reverse`.

## 8. String

String adalah **tipe native** dengan representasi sama seperti `int[]`
byte; literal `"..."` dan `'...'` adalah String (bukan char).

```java
String s = "Kopi";
System.out.println(s.length);       // 4 (byte)
System.out.println(s[0]);           // 75 ('K')
System.out.println(s + " Bahasa");  // konkatenasi String+String
System.out.println(s == "Kopi");    // perbandingan isi
```

`+` untuk String hanya dengan String; untuk angka gunakan `Str.fromInt`
atau template.

### Metode instance (butuh `import Str;`)

```java
import Str;

String t = "  Kopi Bahasa  ";
t.trim()                 // "Kopi Bahasa"
t.upper()                // "KOPI BAHASA"
t.lower()
t.trim().substring(0, 4) // "Kopi"
"arena".charAt(2)        // 101
"game server".indexOf("server")   // 5
"game server".indexOf("x")        // -1
"kopi".contains("op")    // true
"kopi".startsWith("ko")
"kopi".endsWith("pi")
"kopi".equals("kopi")
"Kopi".equalsIgnoreCase("kOPI")
"".isEmpty()
"ab".repeat(3)           // "ababab"
"123".toInt()            // 123
"a,b".replaceSeq(",", ";")
"7".padLeft(3, 48)       // "007"
"teks".countByte(101)
"é".utf8Count()
```

Metode ini hanya gula parser menjadi `Str.<nama>(receiver, ...)`. Bila
`Str` belum diimpor, compiler memberi pesan: *metode String 'x' butuh
'import Str;'*.

### Template string (backtick)

```java
import Str;

int umur = 20;
String nama = "budi";
System.out.println(`Halo {nama}, umur {umur}, tahun depan {umur + 1}`);
System.out.println(`nested {`inner {umur}`}`);
System.out.println(`literal {{x}} \{y\}`);
```

- `{ekspresi}` boleh ekspresi apa pun (panggilan metode, aritmetika,
  template bersarang).
- Angka/boolean memakai `Str.fromInt/fromLong/fromDouble/boolText`,
  jadi **`import Str;` wajib** bila ada interpolasi non-String.
- Escape: `\n`, `\t`, `\r`, `\\`, `\{`, `\}` (backtick di-escape dengan
  backslash + backtick); `{{`/`}}` = kurung literal.
- Hanya backtick yang memformat; `"..."`/`'...'` tetap literal.

## 9. Fungsi

```java
class Main {
    static int tambah(int a, int b) { return a + b; }

    static void sapa(String nama) {
        System.out.println("Halo " + nama);
    }

    static int faktorial(int n) {
        if (n <= 1) return 1;
        return n * faktorial(n - 1);
    }

    public static void main(String[] args) {
        System.out.println(tambah(40, 2));
        sapa("budi");
        System.out.println(faktorial(6));
    }
}
```

- Maksimum **8 parameter** (batas backend native).
- Tidak ada overloading: dua metode dengan nama sama (di seluruh unit
  compile) ditolak, meskipun beda parameter.
- Rekursi didukung. `return` tanpa nilai hanya untuk `void`.
- `public`/`private` diterima tetapi belum menegakkan akses.

## 10. Lambda & `fn`

```java
class Main {
    static int terapkan(fn f, int x) { return f(x); }

    public static void main(String[] args) {
        int faktor = 3;
        fn kali = x => x * faktor;                 // ekspresi
        fn tambah = (a, b) => {                    // blok
            int r = a + b;
            return r;
        };
        System.out.println(kali(10));              // 30
        System.out.println(terapkan(kali, 7));     // 21
        System.out.println(tambah(40, 2));         // 42
    }
}
```

- Bentuk: `x => ekspresi`, `(a, b) => { ... }`, `() => ...`.
- Tipe parameter/return pada `fn` dinamis adalah `int` (v1).
- Capture **snapshot by value** saat lambda dibuat; mutasi di dalam
  lambda tidak bocor keluar.
- Lambda bisa menjadi argumen `fn` dan dipanggil sebagai `f(...)`;
  maksimum 5 argumen saat pemanggilan dinamis.

## 11. Class sebagai namespace

```java
class Keranjang {
    static int total(int harga, int jumlah) { return harga * jumlah; }
    static int diskon(int total) {
        if (total >= 100000) return total * 9 / 10;
        return total;
    }
}

class Main {
    public static void main(String[] args) {
        System.out.println(Keranjang.diskon(Keranjang.total(25000, 5)));
    }
}
```

Semua class dalam satu build berbagi namespace. Simpan helper di file
sendiri lalu `import Keranjang;`.

## 12. Class instance

Sejak v1.12 sebuah class bisa punya **field** dan **method instance**
(metode tanpa `static`). Instance dibuat dengan `new`, dan `this`
merujuk objek yang sedang berjalan:

```java
class Titik {
    int x;
    int y;
    boolean hidup;

    int kuadran() {                    // method instance (tanpa static)
        if (this.x > 0 && this.y > 0) return 1;
        if (this.x < 0 && this.y > 0) return 2;
        return 3;
    }

    int geser(int dx, int dy) {
        this.x = this.x + dx;
        this.y = this.y + dy;
        return this.kuadran();         // panggil method lain lewat this
    }

    static Titik buat(int ax, int ay) {   // factory (pengganti constructor)
        Titik t = new Titik();
        t.x = ax;
        t.y = ay;
        t.hidup = true;
        return t;
    }
}

class Main {
    public static void main(String[] args) {
        Titik a = new Titik();             // semua field = 0
        System.out.println(a.x);           // 0
        Titik b = Titik.buat(3, 4);
        System.out.println(b.kuadran());   // 1
        b.geser(-10, 0);
        System.out.println(b.x);           // -7
        System.out.println(b == Titik.buat(3, 4));  // 0 — identitas referensi
    }
}
```

Aturan yang berlaku:

- **Deklarasi bertipe class**: `Titik t = new Titik();`, parameter
  `boolean sama(Titik o)`, dan return `static Titik buat()` — semua
  didukung. Class harus didefinisikan **sebelum** dirujuk sebagai tipe.
- **Field** selalu di-zero saat `new` (jaminan sama dengan `new int[n]`).
- **`p == q`** membandingkan **referensi** (identitas), bukan isi —
  sama seperti Java. Gunakan method pembanding untuk isi.
- Metode tanpa `static` = metode instance. `main` wajib `static`.

Batas v1.12 (dirancang jujur):

- Field hanya 32-bit: `int`, `boolean`, `byte`, `short`, `uint8`,
  `uint16`. Field `long`/`double`/`String`/objek belum bisa disimpan.
- Tidak ada constructor — semua field mulai dari 0; pakai factory
  (`static Titik buat(...)`) untuk inisialisasi.
- Dispatch **statis** (tanpa inheritance/interface/vtable); `Titik`
  secara internal adalah tape `int[]` ber-ukuran jumlah field.
- Receiver method/field harus variabel sederhana: `b.kuadran()` ok,
  `buat(1,2).kuadran()` belum — simpan dulu ke variabel.
- Objek bisa dipakai di lambda (ditangkap by-value seperti variabel lain).

## 13. JSON literal

```java
import Json;

class Main {
    public static void main(String[] args) {
        int[] u = json {"nama": "budi", "umur": 20, "aktif": true};
        System.out.println(Json.write(u));
        System.out.println(Json.asInt(Json.get(u, "umur")));   // 20
        int[] a = json [1, 2, 3];
        System.out.println(Json.count(a));
    }
}
```

- Literal `json { ... }` / `json [ ... ]` adalah tape `int[]` yang bisa
  disimpan, diparamkan, dan diubah dengan `Json.put/del/add` (immutable).
- Maksimum 8 entri per literal; untuk lebih banyak susun bertahap.
- `Json.parse` untuk teks JSON dari luar; `Json.write` untuk serialisasi.

## 14. Memori: arena 32 MB, tanpa GC

Semua alokasi dinamis (array, string hasil operasi) masuk **arena bump
allocator** 32 MB. Tidak ada free per objek.

```java
int mark = Net.memMark();
int[] sementara = new int[10000];
// ... pakai ...
Net.memReset(mark);       // buang semua alokasi SETELAH mark
```

Aturan penting:

1. Alokasikan data yang harus hidup **sebelum** `memMark`.
2. `memReset(mark)` membebaskan **semua** alokasi setelah mark — jangan
   menyimpan pointer hasil alokasi setelah mark lalu memakainya lagi.
3. `new int[n]` selalu men-zero-kan elemen, termasuk memori daur ulang.
4. Tanpa reset, program batch panjang bisa kehabisan arena; server wajib
   reset per request/iterasi.
5. Lebih dari 32 MB → `kopi: arena memori habis` + exit 1 (bukan crash).

## 15. Keluar & kode status

```java
Net.exit(0);      // keluar seketika; tidak ada finally/atexit
```

`Net.exit` memakai syscall `exit` langsung sehingga buffer stdio tidak
di-flush. Selesaikan output (mis. `println`) sebelum memanggilnya.

### Error runtime: nomor baris + stack trace

Saat program **VM** menabrak error runtime — pembagian dengan nol,
indeks array di luar batas, arena penuh, dsb. — `kopivm` mencetak
pesan dengan **baris sumber** dan **stack trace** lalu keluar dengan
kode status 1:

```
kopivm: error: indeks array di luar batas (5)
  stack trace (terdalam dulu):
    at luar (utama.kop:9)
    at main (utama.kop:13)
```

- Informasi baris membawa ongkos 3–5 byte per statement di bytecode
  dan tidak menambah operasi host (hanya menyimpan nomor baris).
- Nama file di-intern ke cpool; byte tambahan umumnya sekali per metode.
- Backend native (a64/x64) saat ini tetap mencetak pesan singkat tanpa
  trace — ini fitur VM (lihat roadmap: trap berline untuk native).
- Error saat *kompilasi* (`kopicc`) selalu sudah berline sejak awal.

## 16. Contoh tur lengkap

Lihat `docs/contoh/tour.kop` (dijalankan oleh `docs/check.sh`). Isinya
menggabungkan: tipe, ternary, loop, array fungsional, String method,
template, lambda, JSON, dan memMark/memReset.

Lanjut ke [03 — Pustaka](03-pustaka.md).
