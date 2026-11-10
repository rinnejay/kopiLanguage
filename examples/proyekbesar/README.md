# ProyekBesar — uji ketangguhan bahasa Kopi

Proyek contoh **54 file `.kop`** (53 modul + `ProyekBesar.kop`) yang saling
`import`, mencakup pustaka standar (`Math`, `Arrays`, `Stats`, `Matrix`,
`Json`, `Cipher`, `Hash`) dan puluhan modul lokal:

- **dasar**: `Bytes`, `Conv`, `Case`, `Trim`, `Fnv`, `Crc`, `Rng`, `Base64X`
- **numerik/algoritma**: `MathX`, `SortX`, `SearchX`, `MatrixX`, `StatsX`,
  `ArraysX`, `DpFib`, `DpKnap`, `SieveX`, `ModularX`, `PermuteX`, `BitOpsX`
- **struktur data**: `StackX`, `QueueX`, `ListX`, `HeapX`, `BitsetX`
- **domain**: `Csv`, `JsonX`, `Config`, `Inventory`, `Orders`, `Customer`,
  `Billing`, `Invoice`, `Report`, `Validator`, `Pipeline`, `Scheduler`,
  `LogX`, `EventBus`, `Metrics`, `TextTable`, `Checksum`
- **mini sistem**: `LexerX`, `CompilerX`, `EvalX`, `MiniVm`
- **lain-lain**: `GraphBfs`, `GraphDfs`, `DijkstraX`, `KmpX`, `RleX`,
  `CipherX`, `HashX`

## Menjalankan

```sh
cd examples/proyekbesar
./run.sh
```

`bench.py` akan:

1. mengompilasi **a64, VM, dan x64 secara paralel** (x64 dicek sintaksnya);
2. menjalankan **a64 dan VM secara paralel**, dengan **timeout 120 s**
   (deteksi hang) dan sampling `VmHWM` untuk puncak RSS;
3. membandingkan output a64 vs VM byte-per-byte;
4. menjalankan **4 proses a64 paralel** dan memastikan output tetap sama.

## Hasil acuan (Termux aarch64)

| Metrik | a64 | VM |
|---|---|---|
| kompilasi | ~0.13 s | ~0.13 s |
| eksekusi | ~0.14 s | ~0.94 s |
| puncak RSS | ~24 MB | ~24 MB |
| output | 102 baris | 102 baris (identik) |

- 4 proses a64 paralel: semua selesai < 0.3 s, output identik, tanpa hang.
- Fase memori mengalokasikan 3×400.000 + 3×500.000 elemen `int[]`
  (≈ 21,6 MB payload) untuk menguji arena/heap 32 MB; tidak OOM.
- Deterministik: PRNG memakai seed tetap, tanpa input jaringan/waktu.

`expected.out` adalah keluaran acuan (dihasilkan backend a64) dan juga
dipakai oleh `test.sh` utama.
