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
| ~~Array bertipe (`String[]`, `long[]`)~~ **✅ v1.16** (elemen 64-bit slot; identitas referensi berlaku); **✅ v1.43** `byte[]`/`uint8[]` buffer packed 1 byte/elemen; **✅ v1.44** buffer "multiple" (`int[]`↔`byte[]`/`uint8[]`/`String` koersi otomatis, builtin `Net.recvb/sendb/recv1/sendto/recvfrom` menerima packed) | sisa: `double[]`; metode fungsional masih khusus `int[]`; varian `*B` belum ada di semua modul lib; field instance `byte[]`/`uint8[]` | array angka tetap `int[]`; buffer biner baru pakai `byte[]`/`uint8[]` + `Bytes.*`/varian B |
| ~~String = `int[]`~~ **✅ v1.17** (String opaque; interop eksplisit `Net.mkstr`/`Net.strbytes`) | ~~byte-per-slot~~ **✅ v1.18 packed** (1 byte/karakter); **✅ v1.23** `Str` hanya `String`, buffer biner di `Bytes` | alokasi String 5× lebih hemat; API teks tak lagi memakai array |
| ~~Utilitas byte/teks bercampur~~ **✅ v1.23** (`Str` String, `Bytes` int[]) | sisa: normalisasi Unicode, `double[]` | `Str.utf8*` untuk hitung/validasi/potong code point |
| `char` & literal char | `'a'` adalah String | pakai kode byte (`97`) atau `charAt` |
| Unicode/UTF-8 penuh | `length` = byte; `upper/lower` ASCII | `utf8Count`; batasi input ke ASCII |
| ~~`switch`~~ **✅ v1.19**; ~~`throw`/`try`/`catch`~~ **✅ v1.49** (nilai String atau integer, rethrow, try bersarang; tanpa `finally`) | sisa: `finally`, `assert`, `goto`, tipe exception berjenjang | `if/else`, kode error negatif, tangkap lalu ulangi |
| ~~static field & inisialisasi kelas~~ **✅ v1.41** (`static T X = v;` sebagai konstanta read-only; `Class.X` / `X`) | sisa: static field yang bisa diubah, static array/tipe class, inisialisasi berurutan saat startup | pakai konstanta; untuk state yang berubah pakai objek singleton |
| ~~enum~~ **✅ v1.46**; ~~trait/impl~~ **✅ v1.46**; ~~struct~~ **✅ v1.50**; ~~generics~~ **✅ v1.51** (monomorfisasi, satu parameter) | sisa: anotasi, overloading, generic bersarang/array | API harus unik per nama; sufiks nama (`getInt/getStr`) |
| `finally`/RAII | resource harus ditutup manual | `Net.close`, `File.close` |
| ~~Field instance bertipe `double`/`float`~~ **✅ v1.50** (class & struct; nilai int dikonversi saat penugasan) | — | — |
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
| JIT VM (aarch64) | **✅ v1.46** — loop angka panas (`OP_LOOP`/`OP_LOOP_R` dengan badan `OP_MULADD`/`OP_ADDSHR`) dan loop checksum array packed (`OP_ILOAD` + `OP_IDXR_UB`/`OP_IDXR_B` + `OP_OP_STORE`, mis. `acc += buf[i]`) diterjemahkan ke kode mesin dan seluruh sisa iterasi dijalankan native; indeks di luar batas membuat JIT bail ke interpreter. `KOPI_NOJIT=1` mematikan JIT. Belum ada JIT untuk panggilan, `OP_ISTORE` di tengah badan, cabang, atau array 64-bit (`int[]`/`long[]`) |
| x86-64 native | **belum lulus runtime.** Sintaks assembly lolos; `test-x64.sh` di CI sudah bisa mengompilasi, merakit, dan me-link (v1.46.2: label stub `.Klam` yang hilang + `-lm`), tetapi **semua program masih crash saat eksekusi**. Job CI-nya bersifat sinyal (`continue-on-error`), bukan gerbang |
| Thread OS | **✅ v1.45** — sintaks `thread <long> { ... }` + `wait h` (pthread; a64 & VM; handle `long`/`long[]`). **✅ v1.46** — `Chan` (CSP, payload `long`/`fn`) + `Mutex` (`lock`/`unlock`/`tryLock`). Belum ada thread pool bawaan, `select`, atau `fork`/`async` |
| Signal handler / graceful shutdown | tidak ada |
| Diagnostik runtime | nomor baris + stack trace + `saran:` perbaikan |
| Arena | per-thread (a64 TLS, VM heap/thread, x64 global); `malloc{}` otomatis (mmap hingga 4096 MB) atau `malloc<N>` 1 kb–4096 MB; tanpa GC |
| Cross-compile | a64 hanya di ARM; x64 hanya di x86-64 |

Konsekuensi: thread OS tersedia untuk tugas CPU-bound (v1.45) dan
sinkronisasi antar-thread lewat `Mutex`/`Chan` (v1.46), tetapi contoh
server tetap **single-thread**; thread pool/executor masih harus
dirakit sendiri dengan `long[]` + `Chan.sendFn`. Service yang butuh
isolasi penuh tetap paling aman dijalankan sebagai beberapa proses di
belakang reverse proxy.

## 3. Jaringan

| Belum ada | Catatan |
|---|---|
| IPv6 | hanya `AF_INET` |
| TLS **server** | ✅ v1.36 — `TlsServer` (X25519 + ECDSA P-256, ChaCha/AES-GCM); belum mTLS |
| Verifikasi ECDSA CertificateVerify | ✅ sisi server (v1.36): CV klien diverifikasi; sisi klien masih pin SPKI TOFU |
| Validasi rantai/PKI, hostname, expiry | belum |
| Session resumption/PSK TLS | belum |
| DNS sistem penuh | `Resolv` hanya A, tanpa cache/`search`/AAAA/DNS-over-TLS |
| HTTP/2 (klien) | ✅ v1.37 — `Http2` (gate TLS+ALPN h2, HPACK decode penuh + Huffman, dt dinamis); sisi server h2 & HPACK encode dinamis belum |
| WebSocket fragmentation, kompresi | belum |
| Multipart, range request, ETag/Last-Modified, chunked response streaming | belum |
| DB client (`Sqlite` via protokol file; Postgres/Redis via socket belum) | `Sqlite` |
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
| CI lintas arsitektur | ✅ GitHub Actions: build x86-64 + cek docs sinkron + suite x64 eksperimental. Runtime x64 belum lulus; a64 tetap diuji di Termux |
| Spesifikasi formal | docs + kode adalah acuan |
| Benchmark otomatis | `bench/bench.sh` (sum/fib/array/str vs C -O0/-O2, ambil waktu terbaik 3×); belum jadi gerbang CI |
| Fuzzing | ✅ `tools/fuzz_diff.py` — fuzzer **diferensial** a64 vs VM (seed tetap, masuk suite); fuzzing parser/AST secara luas belum |
| Audit memori (ASAN) | belum dijadikan gerbang |

## 6. Uji

- Suite: **270/270** (v1.51.0) — a64 + VM + sintaks x64 + fuzzer + dokumen HTML
  (270 PASS, 0 fail). Backend x64 masih diverifikasi sintaks di mesin ARM;
  `test-x64.sh` + CI menjalankannya secara nyata di runner x86-64.
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
   **✅ v1.43**: `byte[]`/`uint8[]` sebagai buffer packed 1 byte/elemen
   (header 8 byte) + helper `Bytes.lenB/widen/narrow/copyB/textB/printB`.
   **✅ v1.44**: buffer "multiple" — koersi implisit `int[]` ↔
   `byte[]`/`uint8[]` ↔ `String` di batas API, builtin I/O menerima
   buffer packed langsung (`recvb/sendb/recv1/sendto/recvfrom`), varian
   `*B` di File/Tcp/Udp + helper `Bytes.*U`.
   Sisa: varian `*B` di seluruh modul (crypto/compress/web masih API
   `int[]` lama, tetap bisa dipanggil dengan buffer packed lewat koersi),
   field instance `byte[]`.
4. **TLS server** ✅ v1.36 (`TlsServer`, oracle OpenSSL/Python). Sisa:
   verifikasi rantai/hostname di klien, mTLS, session resumption.
5. **HTTP/2 (klien)** ✅ v1.37 — `Http2` (gate TLS+ALPN h2, HPACK decode penuh + Huffman, dt dinamis); sisi server h2 & HPACK encode dinamis belum.
6. **DB client** ✅ v1.38 — `Sqlite` (pembaca file `.db` read-only; Postgres/Redis via socket belum).
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
