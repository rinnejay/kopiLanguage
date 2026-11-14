# Referensi API — IO

Berkas, path, pembaca, dan penulis berbuffer. Semua signature di bawah diekstrak langsung dari source `lib/io/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## File

`lib/io/File.kop` · 15 fungsi

I/O file berbasis deskriptor: buka, baca/tulis, seek, ukuran, hapus, rename, mkdir.

```java
import File;

int fd = File.create("hasil.txt");
File.writeAll(fd, Net.bytes("halo\n"));
File.close(fd);

int r = File.openRead("hasil.txt");
int[] buf = new int[File.size(r)];
File.readInto(r, buf);
File.close(r);
```

**Catatan**

- Selalu tutup fd; tidak ada finalizer/GC untuk resource.
- `readInto` membaca sampai buffer penuh/EOF.

| Fungsi | Keterangan |
|---|---|
| `static int openRead(String path)` | buka file hanya-baca; -1 bila gagal |
| `static int create(String path)` | buat/buka file |
| `static int append(String path)` | buka untuk menambah di akhir |
| `static int close(int fd)` | tutup deskriptor |
| `static int writeAll(int fd, int[] data)` | tulis seluruh data; jumlah byte tertulis |
| `static int readInto(int fd, int[] buf)` | baca sampai buffer penuh/EOF; jumlah byte terbaca |
| `static int seekSet(int fd, int off)` | lompat ke offset dari awal |
| `static int seekCur(int fd, int off)` | lompat relatif dari posisi |
| `static int seekEnd(int fd, int off)` | lompat relatif dari akhir |
| `static int size(int fd)` | jumlah elemen saat ini |
| `static int remove(String path)` | hapus elemen |
| `static int renameTo(String oldp, String newp)` | ganti nama file/direktori |
| `static int mkdir(String path)` | buat direktori |
| `static String readText(String path)` | v1.17.1: I/O teks langsung — kode biasa tak perlu sentuh int[] |
| `static boolean writeText(String path, String text)` | — |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int O_RDONLY = 0` | flag open: hanya baca |
| `static int O_WRONLY = 1` | flag open: hanya tulis |
| `static int O_CREAT = 64` | flag open: buat bila belum ada |
| `static int O_TRUNC = 512` | flag open: kosongkan bila sudah ada |
| `static int O_APPEND = 1024` | flag open: selalu tulis di akhir |
| `static int MODE_0644 = 420` | izin bawaan file baru (0644) |

---

## Path

`lib/io/Path.kop` · 6 fungsi

Utilitas path POSIX: gabung, ambil direktori/nama/ekstensi, uji absolut.

```java
import Path;

System.out.println(Path.ext("index.html"));  // html
System.out.println(Path.dir("/a/b/c.txt"));  // /a/b
```

| Fungsi | Keterangan |
|---|---|
| `static String sep()` | string pemisah path (/) |
| `static String join(String a, String b)` | gabung dua path dengan pemisah |
| `static String base(String p)` | nama file terakhir tanpa direktori |
| `static String dir(String p)` | bagian direktori (tanpa nama file) |
| `static String ext(String p)` | ekstensi tanpa titik |
| `static boolean isAbsolute(String p)` | true bila path absolut |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int SEP = 47` | byte pemisah path |

---

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
