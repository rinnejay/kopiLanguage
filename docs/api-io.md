# Referensi API — IO

Pembaca & penulis berkas berbuffer. Semua signature di bawah diekstrak langsung dari source `lib/io/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Reader

`lib/io/Reader.kop` · 1 fungsi

Pembaca berkas berbuffer: baris demi baris, byte, atau seluruh isi tanpa memuat file ke memori.

```java
import Reader;

Reader r = Reader.open("data.txt");
String baris = r.readLine();
r.close();
```

**Catatan**

- `Reader.open` mengembalikan null bila file tidak ada.

| Fungsi | Keterangan |
|---|---|
| `static Reader open(String path)` | buka file untuk dibaca (null bila gagal) |

---

## Writer

`lib/io/Writer.kop` · 2 fungsi

Penulis berkas berbuffer: teks, baris, atau byte; flush otomatis saat buffer penuh.

```java
import Writer;

Writer w = Writer.open("out.txt");
w.writeLine("halo");
w.close();
```

**Catatan**

- `close()` melakukan flush lalu menutup file.

| Fungsi | Keterangan |
|---|---|
| `static Writer open(String path)` | buat/truncate file untuk ditulis |
| `static Writer append(String path)` | buka file untuk ditambahkan di akhir |

---
