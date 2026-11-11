# 06 — Batas jujur & roadmap

Dokumen ini sengaja dibuat tanpa eufemisme. Kalau sebuah klaim belum
diuji, ditulis "belum".

---

## 1. Bahasa

| Belum ada | Dampak | Workaround sekarang |
|---|---|---|
| Instance class (`new Kelas()`, field, inheritance, interface, vtable) | state harus dibungkus `int[]` + method static | pola `MapStr`/array + class namespace |
| GC / free | arena 32 MB; wajib `memMark/memReset` | reset per request/iterasi; hindari alokasi di loop panas |
| Array bertipe (`String[]`, `long[]`, `double[]`) | semua array praktis `int[]` | simpan indeks/offset ke array datar |
| `char` & literal char | `'a'` adalah String | pakai kode byte (`97`) atau `charAt` |
| Unicode/UTF-8 penuh | `length` = byte; `upper/lower` ASCII | `utf8Count`; batasi input ke ASCII |
| `switch`, `try/catch`, `throw`, `assert`, `goto` | kontrol alur terbatas | `if/else`, kode error negatif |
| static field & inisialisasi kelas | konstanta harus method | `static int W() { return 60; }` |
| generic, enum, anotasi, overloading | API harus unik per nama | sufiks nama (`getInt/getStr`) |
| `finally`/RAII | resource harus ditutup manual | `Net.close`, `File.close` |
| `System.out.print/println` sebagai ekspresi | tidak bisa dipakai di badan lambda ekspresi | bungkus dengan method `static void cetak(int x)` lalu `a.forEach(x => cetak(x))` |

Batas teknis lain:

- Maksimum **8 parameter** metode (batas native).
- Lambda dinamis maksimum **5 argumen**.
- Literal `json` maksimum **8 entri** per objek/array.
- Nilai `int` membungkus (wrap), bukan overflow error.
- `Net.bytes` hanya menerima literal string (bukan variabel).

## 2. Runtime & backend

| Hal | Status |
|---|---|
| aarch64 native | **dijalankan** di Termux/ARM, diuji suite |
| VM KOP1 | **dijalankan**, diuji suite |
| x86-64 native | **hanya verifikasi sintaks** (`cc -target x86_64-linux-gnu -c`); belum dieksekusi di CI |
| Thread / fork / async | tidak ada; server single-thread |
| Signal handler / graceful shutdown | tidak ada |
| Diagnostik runtime | pesan tanpa nomor baris/stack trace |
| Arena | 32 MB (a64/x64/VM), tanpa GC |
| Cross-compile | a64 hanya di ARM; x64 hanya di x86-64 |

Konsekuensi: service CPU-bound memblokir semua klien; satu request
lambat menahan loop (kecuali I/O non-blocking). Untuk produksi:
reverse proxy + beberapa proses.

## 3. Jaringan

| Belum ada | Catatan |
|---|---|
| IPv6 | hanya `AF_INET` |
| TLS **server** | tidak ada; pakai reverse proxy |
| Verifikasi ECDSA CertificateVerify | TLS klien memakai pin SPKI TOFU + verifikasi Finished |
| Validasi rantai/PKI, hostname, expiry | belum |
| Session resumption/PSK TLS | belum |
| DNS sistem penuh | `Resolv` hanya A, tanpa cache/`search`/AAAA/DNS-over-TLS |
| HTTP/2, WebSocket fragmentation, kompresi | belum |
| Multipart, range request, ETag/Last-Modified, chunked response streaming | belum |
| DB client (SQLite/Postgres/Redis) | belum |
| `sendfile`/zero-copy | belum; `Static` membaca file ke memori |

Batas angka:

- Datagram UDP maksimum **1200 byte** per kirim/terima.
- `HttpServer.readRequest(c, max)` dibatasi `max` byte; body lebih besar
  terpotong.
- WebSocket: satu frame per `read`, payload < 64 KiB, tanpa continuation.
- `MapStr`: kapasitas & pool tetap (tanpa grow).
- Antrian/backpressure tidak ada; klien lambat bisa menahan buffer.

## 4. Keamanan

- TLS klien: TOFU pin; bukan PKI lengkap.
- `Auth`/`Jwt` memakai perbandingan konstan sederhana, tetapi belum ada
  audit keamanan independen.
- Tidak ada sandbox; program Kopi punya akses penuh syscall lewat
  `Net.*` (file, jaringan, `system` via `Net.ttyRaw`).
- Tidak ada limit ukuran header/URL bawaan; lakukan validasi sendiri.
- Jangan memakai `Jwt`/`Session` untuk produksi tanpa masa berlaku,
  rotasi kunci, dan penyimpanan persisten.

## 5. Tooling & ekosistem

| Belum ada | Dampak |
|---|---|
| Package manager/versioning | `import` menggabungkan source; tidak ada dependensi versi |
| Debugger, stack trace, breakpoint | debug via `println`, VM, dan inspeksi `.s` |
| LSP, formatter, linter, syntax highlight resmi | editor belum terintegrasi |
| CI lintas arsitektur | suite berjalan di Termux/aarch64; x64 runtime belum |
| Spesifikasi formal | docs + kode adalah acuan |
| Benchmark otomatis | ada `bench.py` untuk proyek besar, bukan regresi perf |
| Fuzzing parser | belum |
| Audit memori (ASAN) | belum dijadikan gerbang |

## 6. Uji

- Suite: **117/117** (v1.11) — a64 + VM + sintaks x64 + dokumen HTML.
- `polyvec` memakai teks RFC 7539 yang dibundel (`test/data`), bukan
  unduhan saat uji.
- Uji UDP/TCP memakai port tetap; pembersihan proses basi ada di awal
  `test.sh`, tetapi dua run paralel bisa bentrok.
- Uji acak harus deterministik; jangan mengulang kesalahan `netbulk`
  (mengharapkan 32/32 byte acak berbeda).

## 7. Roadmap (urutan usulan)

1. **Instance class** — field, `new`, method instance, vtable; ini
   mengubah parser, type system, dan 3 backend. Lakukan bertahap dengan
   suite tetap hijau.
2. **GC sederhana** (mark-sweep) atau region eksplisit, agar server tidak
   wajib reset manual.
3. **Array bertipe + `char` + Unicode dasar**.
4. **TLS server + verifikasi ECDSA/CertificateVerify + chain/hostname**.
5. **HTTP**: multipart, range/ETag, chunked response, gzip.
6. **DB client** (SQLite via protokol file atau Postgres/Redis via socket).
7. **Ekosistem**: package manager, LSP, formatter, CI lintas platform,
   fuzzing, audit keamanan.
8. **Performa**: register allocation/const-folding, `sendfile`,
   buffer pooling, event-loop yang lebih hemat.

## 8. Cara berkontribusi tanpa membuat kesalahan

- Jangan menambah fitur hanya di satu backend.
- Setiap bug diperbaiki + diberi uji regresi.
- Jangan mengklaim "x64 berjalan" atau "TLS aman" tanpa bukti runtime.
- Jaga `sh test.sh` 0 fail sebelum commit.
- Tulis batas di docs, bukan hanya di kepala.
