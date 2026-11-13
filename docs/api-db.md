# Referensi API — Database

Pembaca SQLite read-only. Semua signature di bawah diekstrak langsung dari source `lib/db/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Sqlite

`lib/db/Sqlite.kop` · 18 fungsi

Pembaca file database SQLite read-only (RFC sqlite file format): b-tree tabel, varint, record serial-type, rantai halaman overflow, sqlite_master, parse CREATE TABLE (kolom + alias rowid).

```java
import Sqlite;

int[] ctx = Sqlite.open("data.db");
if (ctx[0] >= 0) {
    String[] t = Sqlite.tables(ctx);
    int k = Sqlite.query(ctx, "Barang");
    while (Sqlite.next(ctx)) {
        System.out.println(Sqlite.intOf(ctx, 0) + " " + Sqlite.strOf(ctx, 1));
    }
    Sqlite.close(ctx);
}
```

**Catatan**

- Read-only; db yang sedang ditulis bisa terbaca setengah jadi.
- Hanya UTF-8 + tabel rowid biasa (WITHOUT ROWID/virtual ditolak).
- Payload record maks 65536 byte; offset halaman 32-bit (db < 2 GB).
- Regresi: tools/gen_sqlitevec.py (fixture page 512 + oracle python sqlite3).

| Fungsi | Keterangan |
|---|---|
| `static int[] open(String path)` | buka db (validasi header/encoding); ctx[0] < 0 = gagal |
| `static void close(int[] ctx)` | tutup file db |
| `static boolean next(int[] ctx)` | maju ke record berikutnya; false = habis/galat |
| `static int typeOf(int[] ctx, int i)` | tipe serial SQLite kolom saat ini |
| `static int count(int[] ctx)` | jumlah nilai record saat ini |
| `static int errOf(int[] ctx)` | kode galat internal terakhir (0 = ok) |
| `static long rowid(int[] ctx)` | rowid record saat ini (long) |
| `static boolean isNull(int[] ctx, int i)` | true bila NULL (kolom ipk tidak pernah NULL) |
| `static long intOf(int[] ctx, int i)` | nilai kolom sebagai long (ipk NULL = rowid) |
| `static double realOf(int[] ctx, int i)` | nilai kolom sebagai double (REAL/integer) |
| `static String strOf(int[] ctx, int i)` | nilai kolom sebagai String (teks) |
| `static int[] blobOf(int[] ctx, int i)` | nilai kolom sebagai int[] (blob) |
| `static double bitsToDouble(long bits)` | (internal) IEEE-754 64 bit → double |
| `static String[] tables(int[] ctx)` | nama tabel user dari sqlite_master (urutan scan) |
| `static String[] columns(int[] ctx, String table)` | nama kolom dari parse CREATE TABLE |
| `static String[] parseCols(String sql)` | (internal) nama kolom dari CREATE TABLE |
| `static int findIpk(String sql)` | (internal) indeks kolom INTEGER PRIMARY KEY |
| `static int query(int[] ctx, String table)` | siapkan scan tabel; hasil = jumlah kolom, <0 galat |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int MAXPAY()` | — |
| `static int MAXCOL()` | — |
| `static int MAXDEPTH()` | — |

---
