# Kopi ☕ — bahasa ala Java, compiler & VM 100% C

`kopi` adalah plesetan Java. Compiler membaca source dan menurunkan
assembly/binary sendiri — tanpa JVM, tanpa bytecode vendor lain.

```
             ┌─────────────────────────────────────────┐
  hello.kop ─│ kopicc: lexer → parser → typecheck → IR │
             └──────┬─────────────┬─────────────┬──────┘
                    ▼             ▼             ▼
               -t a64         -t x64         -t vm
              hello.s        hello.s       hello.kbc
                 │              │              │
              cc/gcc         cc/gcc         kopivm
                 ▼              ▼              ▼
              ./hello        ./hello        program jalan
```

- **aarch64 native** — jalan langsung di Termux/Android.
- **x86-64 native** — target desktop/WSL; verifikasi saat ini sebatas
  assembly dapat dirakit (`cc -c`), belum dijalankan di mesin x86-64.
- **VM KOP1** — bytecode portabel, dipakai Kopi IDE Android.

## Mulai cepat

```sh
make                 # kopicc + kopivm
make test            # 155 uji lintas backend
./kopicc --version   # kopicc 1.42.0 (Kopi) — target a64 | x64 | vm

# binary asli (aarch64)
./kopicc examples/hello.kop -t a64 -o hello.s
cc hello.s -o hello && ./hello

# bytecode + VM
./kopicc examples/hello.kop -t vm -o hello.kbc
./kopivm hello.kbc
```

```java
class Main {
    public static void main(String[] args) {
        System.out.println("Halo dari Kopi!");
    }
}
```

## Fitur bahasa

- Class + method static/instance, field 32/64-bit, **constructor**,
  `new`, identitas referensi, rantai field.
- Tipe `int`, `long`, `byte`, `short`, `float`, `double`, `uint8..64`,
  `boolean`, `String`; cast, literal hex/biner, promosi campuran.
- Array `int[]`, `long[]`, `String[]`; buffer biner **`byte[]`/`uint8[]`**
  (packed 1 byte/elemen, v1.43); bounds check runtime, zero-init,
  `length`, helper `Arrays` + `.forEach/.map/.filter/.reduce`.
- `String` native (1 byte/karakter, packed) + pustaka **`Str`** khusus
  teks (`trim`, `split/join`, `replace`, `pad`, `to*/from*` semua tipe
  skalar, hex, UTF-8) + metode instance + **template string** backtick.
- **`Bytes`** untuk buffer `int[]` dan jembatan `byte[]`/`uint8[]`
  (`lenB/widen/narrow/copyB/textB/printB` + varian `*U`); buffer
  "multiple" (v1.44): `int[]` ↔ `byte[]`/`uint8[]` ↔ `String` dikonversi
  otomatis di batas API, builtin `Net`/`File` menerima buffer packed,
  varian `*B` di File/Tcp/Udp.
- Pustaka teks ikut `String`: `File`, `Path`, `Time.format`, `Log`, `Url`,
  `Mime`, `Http` (teks request), `Dns`, `Resolv`, `Auth`, `Jwt`, `Session`,
  `Json`, dan kunci `MapStr`; buffer respons/payload tetap `int[]`.
- Kontrol alir: `if/else`, `while`, `do/while`, `for`, `break`,
  `continue`, ternary, **`switch`** tanpa fall-through.
- Lambda/closure sejati (`fn`), capture by-value.
- **`malloc { ... }` otomatis** — arena sampai 4096 MB (RAM sesuai
  kebutuhan), tanpa menebak ukuran; `malloc<64mb>` untuk batas keras
  (`b/kb/mb/gb`), dibebaskan di semua jalur keluar blok.
- **Diagnostik compiler pintar** — baris sumber + penunjuk kolom +
  `saran:` perbaikan (mis. lupa `import Str;`, cast, batas malloc).
- JSON literal `json {...}` + pustaka `Json` (parse/write/get/put).
- Diagnostik runtime: nomor baris + stack trace + saran saat error.
- Multi-file: `import Nama;` mencari `lib/<kategori>/Nama.kop`.

## Pustaka standar (84 modul, ditulis dengan Kopi)

| Kelompok | Modul |
|---|---|
| `lib/core/` | `Arrays`, `BigInt`, `Bytes`, `Deque`, `Digits`, `List`, `ListStr`, `Map`, `MapSS`, `MapStr`, `Math`, `Matrix`, `Set`, `SetStr`, `Stats`, `Test` |
| `lib/text/` | `Str`, `Text`, `Regex` |
| `lib/data/` | `Cbor`, `Config`, `Cookie`, `Csv`, `Json`, `MessagePack` |
| `lib/io/` | `File`, `Path`, `Reader`, `Writer` |
| `lib/system/` | `Args`, `Env`, `Log`, `Time`, `Uuid` |
| `lib/compress/` | `Checksum`, `Compress`, `Filter`, `Lzma`, `Lzma2`, `LzmaAuto`, `LzmaStream`, `Xz` |
| `lib/crypto/` | `Aead`, `Aes`, `Big`, `Blake2b`, `ChaCha`, `Cipher`, `Crypto`, `Der`, `Ecdsa`, `Gcm`, `Hash`, `Kdf`, `Pbkdf2`, `Pem`, `Poly`, `Rand`, `Rsa`, `Scrypt`, `X25519`, `X509` |
| `lib/net/` | `Dns`, `Resolv`, `Rfc`, `Tcp`, `Tls`, `TlsServer`, `Udp` |
| `lib/web/` | `Auth`, `Http`, `Http2`, `Http2Tabel`, `HttpServer`, `Jwt`, `Mime`, `Multipart`, `Router`, `Session`, `Static`, `Url`, `WebSocket`, `WsClient` |
| `lib/db/` | `Sqlite` (pembaca read-only `.db`, RFC sqlite) |

Termasuk klien TLS 1.3, HTTP/1.1, WebSocket, server HTTP, event loop,
DNS, PBKDF2/HMAC, ChaCha20-Poly1305, dan X25519 — semuanya diuji
terhadap vektor RFC/oracle Python.

## Kopi IDE (Android)

APK standalone VM-only: editor + tab berkas + autocomplete + build/run,
tanpa Termux.

**Unduh:** [Kopi IDE 1.0.6](https://github.com/rinnejay/kopiLanguage/releases/latest)

- Tab multi-file, penanda belum tersimpan, auto-save, restore tab.
- Kelola berkas: buat/rename/hapus berkas & folder, salin/cut/tempel,
  dan detail ukuran/izin/waktu ubah.
- Autocomplete `System.out.`, `Str.`, `Net.`, `String`, snippet.
- Build `kopicc -t vm` → run `kopivm`, output streaming.
- 84 modul bahasa ikut dibundel; arm64-v8a, Android 7+.
- Detail build/install: [`android/README.md`](android/README.md).

## Dokumentasi

| Dokumen | Isi |
|---|---|
| [01 — Mulai](docs/01-mulai.md) | build, target, alur compile/link |
| [02 — Bahasa](docs/02-bahasa.md) | tutorial sintaks & semantik |
| [03 — Pustaka](docs/03-pustaka.md) | `Str`, `Math`, `Json`, `File`, jaringan, kripto |
| [04 — Server](docs/04-server.md) | TCP/UDP, HTTP, WebSocket, TLS, game |
| [05 — Arsitektur](docs/05-developer.md) | pipeline compiler, 3 backend, kontribusi |
| [06 — Batas & roadmap](docs/06-batas-roadmap.md) | yang belum ada + jebakan |
| [Referensi API](docs/api.md) | 84 modul, 10 kelompok + builtin `Net.*` |

Versi HTML: **https://rinnejay.github.io/** · [`docs/`](docs/README.md)

## Struktur repo

```
src/        lexer, parser, units, ast, gen_a64/x64/vm, main
vm/         kopivm.c — mesin virtual KOP1
lib/        pustaka standar (.kop), per kategori
examples/   contoh: hello, demo, server, ws, game, proyekbesar
docs/       tutorial + referensi API (+ HTML GitHub Pages)
android/    sumber Kopi IDE (WebView + bridge Java)
tools/      generator vektor uji, docs, deploy Pages
test.sh     suite lintas backend
```

## Status

`v1.42.0` — **193 pass, 0 fail**. Riwayat versi lengkap:
[`CHANGELOG.md`](CHANGELOG.md). Batas jujur & rencana:
[docs/06-batas-roadmap.md](docs/06-batas-roadmap.md).

Aturan proyek: setiap fitur bahasa wajib bekerja di **a64, x64, dan
VM** dan disertai uji regresi di `test.sh` — lihat
[docs/05-developer.md](docs/05-developer.md).

## Lisensi

[MIT](LICENSE)
