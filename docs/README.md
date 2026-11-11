# Dokumentasi Kopi

Kopi (☕) adalah bahasa pemrograman statis ala Java yang **compiler, type
checker, generator assembly, dan mesin virtualnya ditulis 100% di C**.
Program Kopi bisa dikompilasi ke **assembly aarch64**, **assembly x86-64**,
atau **bytecode KOP1** yang dijalankan `kopivm`.

Versi **HTML** untuk GitHub Pages ada di folder ini (`index.html`,
`style.css`, `.nojekyll`) dan sudah live di
**https://rinnejay.github.io/**. Regenerasi setelah mengubah `.md`:

```sh
make docs
```

Dokumentasi ini dibagi menjadi jalur belajar dan jalur kontributor.

## Jalur pemakai bahasa

| Dokumen | Isi |
|---|---|
| [01 — Mulai](01-mulai.md) | build, `kopicc`, `kopivm`, hello world, target, alur compile/link |
| [02 — Bahasa](02-bahasa.md) | tutorial lengkap sintaks & semantik |
| [03 — Pustaka](03-pustaka.md) | `Str`, `Math`, `Arrays`, `Json`, `Map`, `File`, jaringan, kripto, auth |
| [Referensi pustaka](api.md) | **indeks 42 modul `lib/` + cara menerapkan** → [Core](api-core.md) · [Net](api-net.md) · [Crypto](api-crypto.md) |
| [04 — Server](04-server.md) | TCP/UDP, HTTP klien/server, event loop, WebSocket, TLS klien, game realtime |
| [06 — Batas & roadmap](06-batas-roadmap.md) | yang sudah ada, yang belum, dan jebakan yang harus diketahui |

## Jalur pengembang compiler

| Dokumen | Isi |
|---|---|
| [05 — Arsitektur & kontribusi](05-developer.md) | pipeline compiler, struktur file, konvensi 3 backend, cara menambah fitur, aturan uji |

## Contoh kode

- `docs/contoh/hello.kop` — program paling kecil
- `docs/contoh/tour.kop` — tur fitur bahasa (dijalankan di uji dokumen)
- `examples/` — contoh nyata: game realtime (`examples/game`), server HTTP
  (`examples/server`), WebSocket (`examples/ws`), proyek 54 file
  (`examples/proyekbesar`)

## Prasyarat

- Linux/aarch64 (Termux Android didukung) atau Linux/x86-64
- `cc` (clang/gcc), `make`, `sh`, `python3` (untuk uji oracle)
- Tanpa dependensi runtime selain libc. Link tetap memakai `cc`.

## Uji cepat

```sh
make            # kopicc + kopivm
make test       # 119 uji (a64, VM, sintaks x64, dokumen)
./kopicc --version
```

Jika semua hijau, Anda siap mengikuti tutorial.
