# 06 — Batas jujur & roadmap

Dokumen ini sengaja dibuat tanpa eufemisme. Kalau sebuah klaim belum
diuji, ditulis "belum".

---

## 1. Bahasa

| Belum ada | Dampak | Workaround sekarang |
|---|---|---|
| ~~Instance class~~ **✅ v1.12–1.15** (field, `new`, method `this`, identitas referensi; field `long`/`String`/class; **constructor**) | sisa: tanpa inheritance/vtable; field `double`/`float` menyusul | dispatch statis; tanpa overload constructor |
| ~~Field instance bertipe array~~ **✅ v1.29** (`int[]`, `long[]`, `String[]`; termasuk penugasan `p.field[i] = v`) | sisa: field `double[]`/`float[]` | simpan dulu ke variabel bila perlu |
| GC / free | arena global 32 MB; `malloc { ... }` otomatis (mmap hingga 4096 MB, RAM sesuai kebutuhan) atau `malloc<N>` batas keras 1 kb–4096 MB; `memMark/memReset` manual opsional | region scope, bukan GC — rujukan tak boleh keluar blok (dicek compiler); batas blok dicek runtime |
| ~~Array bertipe (`String[]`, `long[]`)~~ **✅ v1.16** (elemen 64-bit slot; identitas referensi berlaku) | sisa: `double[]`; metode fungsional masih khusus `int[]` | array `long[]`/`String[]` pakai loop `for` biasa |
| ~~String = `int[]`~~ **✅ v1.17** (String opaque; interop eksplisit `Net.mkstr`/`Net.strbytes`) | ~~byte-per-slot~~ **✅ v1.18 packed** (1 byte/karakter); **✅ v1.23** `Str` hanya `String`, buffer biner di `Bytes` | alokasi String 5× lebih hemat; API teks tak lagi memakai array |
| ~~Utilitas byte/teks bercampur~~ **✅ v1.23** (`Str` String, `Bytes` int[]) | sisa: normalisasi Unicode, `double[]` | `Str.utf8*` untuk hitung/validasi/potong code point |
| `char` & literal char | `'a'` adalah String | pakai kode byte (`97`) atau `charAt` |
| Unicode/UTF-8 penuh | `length` = byte; `upper/lower` ASCII | `utf8Count`; batasi input ke ASCII |
| ~~`switch`~~ **✅ v1.19** (desugar if/else; tanpa fall-through; selector int-ish dievaluasi sekali; break/continue milik loop terluar) `try/catch`, `throw`, `assert`, `goto` | kontrol alur terbatas | `if/else`, kode error negatif |
| static field & inisialisasi kelas | konstanta harus method | `static int W() { return 60; }` |
| generic, enum, anotasi, overloading | API harus unik per nama | sufiks nama (`getInt/getStr`) |
| `finally`/RAII | resource harus ditutup manual | `Net.close`, `File.close` |
| Field instance bertipe `double`/`float` | butuh jalur register float di 3 backend | simpan sebagai `long`/`String` dulu, atau field `int` + pembagian |
| Constructor overload & `this(...)` | satu constructor per class | overload = nama berbeda (`buatKecil`, `buatBesar`) |
| Method/field pada receiver ekspresi (`buat(1,2).x`) | dispatch statis via variabel | simpan dulu: `Kelas k = buat(1,2);` |
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
| Diagnostik runtime | nomor baris + stack trace + `saran:` perbaikan |
| Arena | global 32 MB (a64/x64/VM); `malloc{}` otomatis (mmap hingga 4096 MB) atau `malloc<N>` 1 kb–4096 MB; tanpa GC |
| Cross-compile | a64 hanya di ARM; x64 hanya di x86-64 |

Konsekuensi: service CPU-bound memblokir semua klien; satu request
lambat menahan loop (kecuali I/O non-blocking). Untuk produksi:
reverse proxy + beberapa proses.

## 3. Jaringan

| Belum ada | Catatan |
|---|---|
| IPv6 | hanya `AF_INET` |
| TLS **server** | tidak ada; pakai reverse proxy |
| Verifikasi ECDSA CertificateVerify | TLS klien memakai pin SPKI TOFU + verifikasi Finished; primitif ECDSA P-256 sudah ada sejak v1.35 (`Ecdsa`) |
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

- Suite: **155/155** (v1.23) — a64 + VM + sintaks x64 + dokumen HTML
  (150 PASS termasuk cek x64 yang tidak masuk counter).
- `polyvec` memakai teks RFC 7539 yang dibundel (`test/data`), bukan
  unduhan saat uji.
- Uji UDP/TCP memakai port tetap; pembersihan proses basi ada di awal
  `test.sh`, tetapi dua run paralel bisa bentrok.
- Uji acak harus deterministik; jangan mengulang kesalahan `netbulk`
  (mengharapkan 32/32 byte acak berbeda).

## 7. Roadmap (urutan usulan)

1. ~~Instance class~~ **✅ v1.12–1.15** (field + `new` + method `this` +
   identitas referensi + field `long`/`String`/class + constructor).
   **✅ v1.29**: field instance bertipe array (`int[]`/`long[]`/`String[]`)
   + penugasan indeks pada field (`p.field[i] = v`).
   Tahap lanjut: field `double`/`float`, overload constructor,
   vtable/inheritance.
   - **Keputusan desain**: Kopi tidak meniru Java sepenuhnya. Alih-alih
     `interface`, Kopi akan memakai **`struct` ala C** — agregat data +
     metode namespace / fungsi bebas — agar bahasanya terasa natural di
     antara C dan Java, bukan klon Java. Polymorphism (bila jadi) lewat
     struct ber-pola vtable sederhana, bukan hierarki tipe Java.
2. **GC sederhana** (mark-sweep) atau region eksplisit, agar server tidak
   wajib reset manual.
3. ~~Array bertipe~~ **✅ v1.16** (`long[]`/`String[]` elemen 64-bit slot;
   sisa: `double[]`, array literal, multi-dimensi).
4. **TLS server + verifikasi ECDSA/CertificateVerify + chain/hostname** — fondasi ECDSA P-256 (v1.35) sudah siap dipakai.
5. **HTTP**: multipart, range/ETag, chunked response, gzip.
6. **DB client** (SQLite via protokol file atau Postgres/Redis via socket).
7. **Ekosistem**: package manager, LSP, formatter, CI lintas platform,
   fuzzing, audit keamanan.
8. **Performa**: register allocation/const-folding, `sendfile`,
   buffer pooling, event-loop yang lebih hemat; trap error berline
   untuk backend native (a64/x64) — VM sudah selesai (v1.13).

## 8. Cara berkontribusi tanpa membuat kesalahan

- Jangan menambah fitur hanya di satu backend.
- Setiap bug diperbaiki + diberi uji regresi.
- Jangan mengklaim "x64 berjalan" atau "TLS aman" tanpa bukti runtime.
- Jaga `sh test.sh` 0 fail sebelum commit.
- Tulis batas di docs, bukan hanya di kepala.
