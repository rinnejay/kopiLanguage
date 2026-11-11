# 01 — Mulai

## 1. Build

```sh
cd ~/kopi
make            # menghasilkan ./kopicc dan ./kopivm
make test       # menjalankan seluruh suite (butuh python3)
```

`Makefile` hanya memakai `cc` (clang/gcc), `-O2 -std=c11`. Tidak ada
dependency pihak ketiga.

Cek versi:

```sh
./kopicc --version
# kopicc 1.11.0 (Kopi) — target a64 | x64 | vm
```

## 2. Program pertama

Buat `hello.kop`:

```java
class Main {
    public static void main(String[] args) {
        System.out.println("Halo dunia dari Kopi!");
    }
}
```

Kompilasi ke native (aarch64, di HP/ARM):

```sh
./kopicc hello.kop -t a64 -o hello.s
cc hello.s -o hello
./hello
```

Atau lewat VM portabel:

```sh
./kopicc hello.kop -t vm -o hello.kbc
./kopivm hello.kbc
```

Atau target x86-64 (di PC):

```sh
./kopicc hello.kop -t x64 -o hello64.s
cc hello64.s -o hello64
./hello64
```

> `kopicc` hanya menghasilkan **assembly/bytecode**. Menjadi executable
> masih lewat `cc`. Di x86-64 gunakan `gcc`/`clang` biasa. Di Termux,
> `cc -target x86_64-linux-gnu -c` bisa dipakai untuk cek sintaks x64
> tanpa menjalankannya.

## 3. Anatomi program

- Wajib ada satu metode bernama `main` di class mana pun:
  `static void main()` atau `static void main(String[] args)`.
- Class hanya berisi **metode static** (belum ada instance/field).
- Semua file `.kop` yang diimpor digabung jadi satu unit compile.
- Tidak ada `package`; nama class harus unik dalam satu build.

```java
class Util {
    static int dua() { return 2; }
}

class Main {
    public static void main(String[] args) {
        System.out.println(Util.dua() * 21);
    }
}
```

Kompilasi multi-file langsung juga boleh:

```sh
./kopicc Main.kop Util.kop -t a64 -o app.s
```

## 4. Argumen program

`Net.argc()` / `Net.argv(i)` berisi argumen **setelah** nama program
(native) atau setelah path bytecode (VM).

```java
import Str;

class Main {
    public static void main(String[] args) {
        int port = 8080;
        if (Net.argc() > 0) port = Str.toInt(Net.argv(0));
        System.out.println(port);
    }
}
```

```sh
./app 9090        # port = 9090
./kopivm app.kbc 9090
```

## 5. Import

```java
import Str;              // cari Str.kop di folder pengimpor, lalu lib/
import "util.kop";       // path relatif terhadap file pengimpor
import Http;
```

Urutan pencarian modul `import Nama;`:

1. folder file pengimpor — `Nama.kop`, lalu `nama.kop` (lokal menang)
2. `lib/` di samping `kopicc` (bisa dioverride `KOPPATH`)
3. subfolder `lib/<kategori>/Nama.kop` (`core/`, `net/`, `crypto/`)

File yang sama (realpath) hanya dimuat sekali. **Jebakan**: file lokal
bernama sama dengan modul (tanpa peduli huruf besar/kecil) akan menang,
sehingga `import Str` dari folder yang berisi `str.kop` akan memuat file
lokal. Ini alasan file uji tidak boleh dinamai sama dengan modul.

## 6. Alur kerja singkat

```text
source .kop ──lex/parse/type──> AST ──gen──> .s (a64/x64) ──cc──> binary
                                   └──────> .kbc (KOP1) ──kopivm──> jalan
```

- Untuk debug cepat gunakan `-t vm`: error runtime VM lebih mudah dibaca.
- Untuk performa gunakan native `-t a64` (di ARM) atau `-t x64` (di PC).
- Assembly yang dihasilkan manusiawi; buka `.s` bila ingin tahu kenapa
  sesuatu lambat/aneh.

## 7. Kesalahan umum saat mulai

| Gejala | Sebab / solusi |
|---|---|
| `tidak ada metode main()` | tidak ada `static void main(...)` |
| `variabel lokal wajib diinisialisasi` | `int x;` tidak boleh; tulis `int x = 0;` |
| `metode String 'trim' butuh 'import Str;'` | tambahkan `import Str;` |
| `karakter tidak dikenal '?'` | versi lama; v1.11+ mendukung ternary |
| `arena memori habis` | alokasi >32 MB tanpa `Net.memReset` (lihat 02 & 06) |
| `array tidak valid` | indeks negatif / ≥ panjang; cek `.length` |

Lanjut ke [02 — Bahasa](02-bahasa.md).
