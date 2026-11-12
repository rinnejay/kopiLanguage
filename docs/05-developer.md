# 05 — Arsitektur compiler & panduan kontribusi

Dokumen ini untuk siapa pun yang ingin mengubah compiler, runtime, atau
pustaka Kopi. Prinsipnya: **satu unit compile, tiga backend, satu suite
uji**.

---

## 1. Peta repositori

```text
kopi/
├── src/
│   ├── lex.h            token & keyword (semua token multi-karakter ≥ 256)
│   ├── lex.c            lexer: angka, string, template backtick, komentar
│   ├── ast.h            Ty, Node, Method, Prog + aturan promosi tipe
│   ├── units.c          import & penggabungan multi-file (splice token)
│   ├── parse.c          parser recursive-descent + resolusi tipe + scope
│   ├── gen_a64.c        codegen aarch64 (assembly)
│   ├── gen_x64.c        codegen x86-64 (assembly)
│   ├── gen_vm.c         codegen bytecode KOP1
│   └── main.c           driver CLI (--help/--version/-t/-o)
├── vm/kopivm.c          mesin virtual KOP1 + builtin Net host
├── lib/                 pustaka standar ditulis di Kopi
├── examples/            program contoh (game, server, ws, proyek besar)
├── test/                uji sumber (oracle/vektor)
├── tools/               generator oracle Python + server uji
├── test.sh              suite regresi (a64 + VM + sintaks x64)
├── docs/                dokumentasi ini
└── Makefile
```

## 2. Pipeline

```text
file .kop
  │ units_load_many(): baca import, gabung token (sekali per realpath)
  ▼
lexer (lex.c) ── token stream
  ▼
parser (parse.c) ── AST Node + daftar Method/Class
  ▼
resolve()/type_expr() ── scope, slot lokal, cek tipe, cache e->dty
  ▼
gen_a64 / gen_x64 / gen_vm ── teks assembly atau bytecode
  ▼
cc (link libc)  atau  kopivm
```

Tidak ada tahap optimizer terpisah. Codegen langsung dari AST.

## 3. Struktur data inti (`ast.h`)

- `Ty`: `T_INT T_BOOL T_VOID T_STR T_FN T_ARI T_LONG T_DOUBLE T_FLOAT
  T_UINT8/16/32/64 T_BYTE T_SHORT`. Helper: `ty_is_int/num/float/uint`,
  `ty_width`, `ty_rank`, `ty_promote`, `ty_can_implicit`.
- `Node`: `kind`, `str`, `ival`, `fval`, `cp`, `dty`, `typed`, `line`,
  `a..d`, `next`, `file`, `meth`.
  - `EX_*` ekspresi, `ST_*` statement.
  - `dty` = tipe hasil/deklarasi; **cache** agar `type_expr` tidak
    dijalankan dua kali (`typed`).
- `Method`: nama, class, param (`Param *params`), `ret`, slot, index.
- `Prog`: daftar method, cpool string, `mainidx`.

> `ast.h` juga dipakai `kopivm.c`; mengubah enum Node/Ty berarti
> menyesuaikan VM juga.

## 4. Lexer

- Semua token multi-karakter/keyword bernilai **≥ 256** (`T_*`, `KW_*`).
  Operator ASCII (`+`, `-`, `*`, `(`, `?`, ...) memakai nilai byte.
- Menambah keyword: tambah `KW_*` di `lex.h`, entri tabel di `lex.c`
  (`kwtab`), lalu parse di `parse.c`.
- String: `"..."` dan `'...'`; escape `\n \t \r \\ \" \'`; `\0`/`\xNN`
  **tidak** didukung.
- Template: hanya backtick; `{expr}`; escape `\{ \}`; `{{`/`}}`.
- Angka: desimal, `0x`, `0b`, sufiks `L U UL f` (dan `.`/`e` untuk float).

## 5. Parser & resolusi

- `units.c` menyisipkan token file impor; file sama hanya sekali.
- `parse.c`: `expr()` (ternary di paling atas, `||`), lalu
  `and → bitor → bitxor → bitand → eq → rel → shift → add → mul →
  unary → postfix → primary`.
- Pemanggilan `a.m(args)` dibuat oleh `call_node()` menjadi `EX_CALL`
  dengan `str="a.m"`. Untuk ekspresi berbentuk lain, `postfix()` menulis
  ulang menjadi `Arrays.m(receiver,args)`; **type_expr** kemudian
  membelokkan ke `Str.*` bila receiver `String`, atau `Bytes.*` bila
  receiver `int[]` (lihat `is_str_method`/`str_method_target`).
- Scope: array `scope[1024]`, `nextslot`; tiap blok menyimpan/memulihkan
  `nscope`. Variabel lokal wajib punya inisialisasi.
- `resolve_stmt` menetapkan `s->ival` = slot, `s->dty`; `type_expr`
  menetapkan `e->dty` dan `e->typed`.
- Batas yang disengaja: maks 8 parameter metode, tidak ada overloading
  (nama metode unik per unit), `main` 0/1 parameter.

## 6. Konvensi backend

### aarch64 (`gen_a64.c`)

- Nilai ekspresi di `w0` (int/bool) atau `x0` (long/double/pointer/fn);
  `gen_expr` mengembalikan `1` bila 64-bit.
- Operand stack: setiap push memakai **16 byte** (`str x0,[sp,#-16]!`).
  Setiap builtin/ekspresi **wajib** menyeimbangkan `sp`; kebocoran 16
  byte pernah menyebabkan variabel lokal rusak (bug `recvfrom`, v1.9.3).
- Lokal: `(slot+1)*8` dari `x29`.
- Array: `[0]=length`, elemen 8-byte sign-extended; `new int[n]`
  men-zero-kan elemen.
- Pemeriksaan batas melompat ke `.Karr` (pesan runtime).
- Label unik: pakai `nlab++` (jangan hardcode `.L1`).
- `brk_lab/cont_lab` menangani `break`/`continue` bersarang.
- Panggilan libc (`getenv`, `system`, `poll`) memakai `bl`; simpan
  register caller-saved yang masih dibutuhkan setelah panggilan.
- Entry `main`: alokasi arena global 32 MB via `malloc`, simpan argc/argv,
  lalu panggil `main` Kopi.
- Blok `malloc` memakai helper `kop_arena_enter`/`kop_arena_leave`:
  handler menyimpan (base, cap, used, flag mmap/auto) arena luar, lalu
  menyiapkan arena baru. Ukuran 0 = **otomatis**: `mmap` 4096 MB dengan
  `MAP_NORESERVE` (RAM sesuai halaman yang disentuh); ukuran eksplisit =
  `malloc`. Saat keluar, arena auto di-`munmap`, arena eksplisit di-`free`.
  `kopalloc` membaca batas dari `kop_arena_cap`, bukan konstanta 32 MB.

### x86-64 (`gen_x64.c`)

- Sama secara konsep; stack slot 8 byte, `par` melacak paritas agar
  panggilan libc 16-byte aligned. Arg 1–6 di register, 7–8 di stack
  (maks 8 parameter).
- Panggilan libc lewat `@PLT`.
- `is64ty`, `push_val`, label, dan cek batas harus konsisten dengan a64.
- Backend ini **belum dijalankan runtime** di CI; verifikasi hanya
  sintaks: `cc -target x86_64-linux-gnu -c out.s`.

### VM (`gen_vm.c` + `vm/kopivm.c`)

- Mesin stack; opcode di `vm/kopivm.c` (`OP_*`).
- Nilai selalu 64-bit; operasi int32 harus dinormalisasi (`OP_NORM`)
  agar semantik sama dengan native.
- `OP_NET` + field `bi` memetakan builtin `Net.*` ke syscall host.
- Heap global 32 MB (`HEAPLEN` = 4M elemen 64-bit); `new` men-zero-kan.
  Blok `malloc` mengalokasi heap sendiri (`OP_RENTER`) dan membebaskannya
  (`OP_RLEAVE`); ukuran 0 = auto (`mmap` 4096 MB), selain itu `malloc`.
  `heap_cap` menyesuaikan batas aktif; `heap_mmapped`/`heap_auto`
  menentukan `munmap` vs `free` dan pesan OOM.
- v1.22: loop panas memakai superinstruksi (`OP_LOOP`, `OP_CMP_JMP*`,
  `OP_OP_STORE`, `OP_IDXR_L`, `OP_IDXW_L`, `OP_BIN_L`) + lipat konstanta;
  emitter ada di `gen_vm_fuse.inc`.
- Label = indeks instruksi (`newlab/mark`), bukan string.

### Struktur berkas (v1.22)

Berkas besar dipecah menjadi fragmen `.inc` yang tetap **satu unit
kompilasi** (di-`#include`), sehingga state `static` tidak perlu
diekspor dan tidak ada biaya dispatch tambahan:

| Induk | Fragmen |
|---|---|
| `src/parse.c` | `parse_stmt.inc`, `parse_class.inc`, `parse_expr.inc`, `parse_scope.inc`, `parse_type.inc`, `parse_resolve.inc` |
| `src/gen_a64.c` | `gen_a64_expr.inc`, `gen_a64_rt.inc` |
| `src/gen_x64.c` | `gen_x64_expr.inc`, `gen_x64_rt.inc` |
| `src/gen_vm.c` | `gen_vm_fuse.inc`, `gen_vm_expr.inc` |
| `vm/kopivm.c` | `vm_core.inc`, `vm_ops_base.inc`, `vm_ops_net.inc`, `vm_ops_num.inc`, `vm_ops_fast.inc` |

`make` sudah memantau fragmen: mengubah `.inc` otomatis membangun ulang.

## 7. Menambah fitur bahasa (checklist)

Contoh nyata: ternary & do/while (v1.11).

1. **Token/keyword** — `lex.h` (+ tabel `lex.c` bila keyword).
2. **AST** — tambah `EX_*`/`ST_*` di `ast.h`; dokumentasikan field
   (`a`,`b`,`c`).
3. **Parser** — buat node di `parse.c` pada precedence/posisi yang
   benar; pastikan `expect()` untuk delimiter.
4. **Resolusi** — `type_expr` (ekspresi) atau `resolve_stmt` (statement):
   cek tipe, isi `e->dty`, dan `s->dty`.
5. **a64** — `gen_expr`/`gen_stmt`; alokasikan label `nlab`; jaga `sp`.
6. **x64** — implementasi setara; jaga paritas `par`.
7. **VM** — opcode/`jmp/mark`; pastikan nilai di stack sesuai.
8. **Uji** — tambah `test/*.kop` + ref; jalankan a64 & VM, plus cek
   sintaks x64.
9. **Docs** — `docs/`, `README.md` changelog, dan contoh bila perlu.

Aturan: **jangan menambah fitur di satu backend saja.** Kalau belum
sempat, tolak di parser dengan pesan jelas.

## 8. Menambah builtin `Net.*` (checklist)

1. `parse.c`: cabang `bi = N; want = jumlah_arg` + validasi tipe argumen
   dan tipe hasil (`e->dty`).
2. `gen_a64.c`: blok `if (e->ival == N)`, hitung layout stack (arg ×16),
   syscall/libc, **bersihkan `sp` persis**.
3. `gen_x64.c`: blok setara; perhatikan `par` dan register caller-saved.
4. `vm/kopivm.c`: cabang `bi == N` di `OP_NET`; konversi pointer array
   (`long*`) dan hasil.
5. Bungkus dengan API `.kop` bila perlu (`lib/net/*.kop`).
6. Uji a64 + VM (nilai harus sama), lalu `cc -target x86_64-linux-gnu -c`
   untuk asm x64.

Contoh yang bisa ditiru: `Net.pollfd`, `Net.settimeout`,
`Net.recv1`, `Net.localport`.

### Pustaka teks vs buffer (v1.23)

- `lib/core/Str.kop` — **hanya `String`**: parameter dan hasil teks tidak
  memakai `int[]`. Berisi trim/case, potong/cari, split/join/lines/words,
  padding, konversi skalar (`fromShort/fromFloat/...`, `toLong/toBool/...`),
  hex, dan utilitas UTF-8.
- `lib/core/Bytes.kop` — **hanya `int[]`**: buffer I/O biner, jaringan,
  kriptografi (`cat/slice/indexOf/trim/hex/...`).
- Pustaka teks lain sudah `String` di API publik: `File`, `Path`,
  `Time.format`, `Log`, `Url`, `Mime`, `Http` (baris/header/body request),
  `Dns`, `Resolv`, `Auth`, `Jwt`, `Session`, `Json`, dan kunci `MapStr`.
  Buffer respons/HTTP, TCP/TLS, hash, cipher, DER, dan tape JSON internal
  tetap `int[]`.
- Pindah representasi eksplisit: `Net.strbytes(String) → int[]`,
  `Net.mkstr(int[]) → String` (atau `Bytes.of` / `Bytes.text`).
- Gula metode di parser memilih kelas dari tipe penerima: `T_STR → Str`,
  `T_ARI → Bytes` (lihat `parse_type.inc`), jadi `buf.trim()` dan
  `teks.trim()` sama-sama bekerja dengan impor masing-masing.

## 9. Menambah pustaka `.kop`

- Cukup buat `lib/<kategori>/Nama.kop` dengan `class Nama { static ... }`.
- Import relatif: modul boleh `import` modul lain; hindari siklus.
- Ingat aturan pencarian: file lokal menang; **jangan menamai file uji
  sama dengan modul** (mis. `test/Str.kop` akan menutupi `lib/core/Str`).
- Tulis uji deterministik: program `.kop` yang mencetak hasil, ref dari
  oracle Python bila ada standar (mis. `hashlib`, `urllib.parse`).

## 10. Uji & kualitas

`test.sh`:

```sh
check nama path.kop          # jalankan VM & a64, diff ke $T/kopi/nama.ref
```

- Ref dibuat dari oracle/skrip, bukan dari salah satu backend, kecuali
  output memang didefinisikan oleh implementasi (mis. aturan game).
- Uji x64 hanya **assembler** (`cc -target x86_64-linux-gnu -c`), bukan
  runtime. Jangan mengklaim x64 berjalan bila belum dijalankan.
- Uji acak harus deterministik: contoh nyata bug uji `netbulk` yang
  mengharapkan 32/32 byte acak berbeda (flaky ~12%) — diperbaiki menjadi
  "ada beda".
- Bila menemukan bug: tambahkan uji regresi yang gagal sebelum fix.
- Jalankan `sh test.sh` sampai **0 fail** sebelum commit.

## 11. Debugging

| Masalah | Cara |
|---|---|
| hasil beda a64 vs VM | bandingkan output; cek normalisasi int/`OP_NORM`, `is64ty` |
| crash/abort | pesan runtime; cek indeks array, arena, `memReset` |
| variabel lokal berubah | stack tidak seimbang di codegen/builtin |
| hash/uang aneh setelah `memReset` | array tidak ter-zero (sudah diperbaiki; jangan andalkan) |
| asm x64 gagal | label ganda (pakai `nlab`), `%` escape di `emit` |
| VM salah | periksa urutan `--sp`/`sp++`, tipe 32/64 |
| lambat | buka `.s`; cari panggilan per byte (`Net.wrb`) di loop |

Tips:

```sh
./kopicc prog.kop -t vm -o prog.kbc && ./kopivm prog.kbc
./kopicc prog.kop -t a64 -o prog.s && cc prog.s -o prog && ./prog
grep -n "Karr\|kopalloc" prog.s
```

VM sering lebih mudah karena tidak ada masalah ABI/stack, tetapi bug
codegen native hanya muncul di a64/x64.

## 12. Gaya kode C

- C11, tanpa dependensi; alokasi compiler pakai `malloc/realloc` biasa.
- Error compile: `die("baris %d: ...", e->line, ...)`; jangan silent.
- Output assembly lewat `emit(...)`; ingat `%` harus `%%` di format.
- Setiap case codegen harus punya label unik (`nlab`).
- Komentar menjelaskan **mengapa**, bukan mengulang kode.
- Jalankan `make` bebas warning (warning yang ada saat ini sudah
  diketahui; jangan menambah yang baru).

## 13. Alur rilis

1. `make && sh test.sh` → 0 fail.
2. Perbarui `README.md` (status + changelog), `docs/` bila fitur berubah.
3. Commit pesan ringkas berisi fitur + hasil uji.
4. `git push origin HEAD`.
5. Tag rilis (opsional): `git tag vX.Y.Z && git push --tags`.

## 14. Ide kontribusi berikutnya

Lihat [06 — Batas & roadmap](06-batas-roadmap.md). Yang paling
berdampak: instance class (`new`/field/vtable), GC, array bertipe,
TLS server + ECDSA, multipart/range HTTP, DB client.
