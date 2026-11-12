# Referensi API — Core

Tipe dasar, teks, matematika, koleksi, file, waktu. Semua signature di bawah diekstrak langsung dari source `lib/core/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Args

`lib/core/Args.kop` · 10 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int count()` | — |
| `static String at(int i)` | — |
| `static String[] all()` | — |
| `static boolean isFlag(String a)` | — |
| `static boolean hasFlag(String flag)` | cocokkan "--nama", "-n", atau "--nama=nilai" |
| `static String value(String flag, String def)` | nilai "--nama=nilai", "--nama nilai", atau "-n=nilai" |
| `static int intValue(String flag, int def)` | — |
| `static String[] positionals()` | — |
| `static String command()` | perintah = positional pertama, "" bila tidak ada |
| `static String[] flags()` | — |

---

## Arrays

`lib/core/Arrays.kop` · 23 fungsi

Operasi array `int[]`: agregat, potong/gabung, sort, pencarian, dan gaya fungsional (lambda).

```java
import Arrays;

int[] a = new int[4];
a[0] = 3; a[1] = 1; a[2] = 4; a[3] = 2;
System.out.println(Arrays.sum(a));          // 10
int[] s = Arrays.sort(a);                    // [1,2,3,4]
```

**Catatan**

- Fungsi murni: tidak mengubah array masukan kecuali yang didokumentasikan.
- Gula `a.map(f)`/`a.filter(f)` didesugar ke `Arrays.*`.

| Fungsi | Keterangan |
|---|---|
| `static int sum(int[] xs)` | jumlah semua elemen |
| `static int max(int[] xs)` | nilai terbesar |
| `static int min(int[] xs)` | nilai terkecil |
| `static int find(int[] xs, int v)` | pencarian linier: indeks pertama atau -1 |
| `static int arrEq(int[] a, int[] b)` | 1 bila isi sama, 0 bila beda |
| `static int count(int[] xs, int v)` | berapa kali v muncul |
| `static int[] take(int[] xs, int n)` | n elemen pertama |
| `static int[] drop(int[] xs, int n)` | semua kecuali n elemen pertama |
| `static int[] merge(int[] a, int[] b)` | gabung dua array |
| `static int[] sort(int[] xs)` | urut naik (merge sort, tak mengubah elemen masukan) |
| `static int binSearch(int[] xs, int v)` | cari pada array terurut (indeks atau negatif) |
| `static int[] copy(int[] xs)` | salinan (array baru) |
| `static int[] reverse(int[] xs)` | salinan terbalik |
| `static void forEach(int[] xs, fn f)` | panggil f(x) untuk tiap elemen; hasil f diabaikan |
| `static void forEachIndexed(int[] xs, fn f)` | panggil f(i, x) untuk tiap elemen (indeks, nilai) |
| `static int[] map(int[] xs, fn f)` | array baru: out[i] = f(xs[i]) |
| `static int[] filter(int[] xs, fn pred)` | array baru berisi elemen yang pred(x) != 0 |
| `static int reduce(int[] xs, int init, fn f)` | lipat dengan nilai awal dan fungsi (acc, x) |
| `static boolean any(int[] xs, fn pred)` | true bila ada elemen yang lolos predikat |
| `static boolean all(int[] xs, fn pred)` | true bila semua elemen lolos predikat |
| `static int countWhere(int[] xs, fn pred)` | jumlah elemen yang lolos predikat |
| `static int findIndex(int[] xs, fn pred)` | indeks elemen pertama yang lolos, -1 bila tidak ada |
| `static int sumBy(int[] xs, fn f)` | jumlah hasil pemetaan |

---

## Bytes

`lib/core/Bytes.kop` · 48 fungsi

Utilitas buffer byte `int[]`: I/O biner, jaringan, kriptografi. Untuk teks gunakan `Str`.

```java
import Bytes;

int[] b = Net.strbytes("  Kopi  ");
System.out.println(Net.mkstr(Bytes.trim(b))); // Kopi
String t = Bytes.text(Bytes.cat(Bytes.of("a"), Bytes.of("b")));
```

**Catatan**

- Semua parameter/hasil byte memakai `int[]`.
- `String → int[]` implisit; `int[] → String` lewat `Net.mkstr`/`Bytes.text`.
- Gula `buf.trim()` didesugar ke `Bytes.trim(buf)` (butuh `import Bytes;`).

| Fungsi | Keterangan |
|---|---|
| `static int len(int[] s)` | — |
| `static boolean isEmpty(int[] s)` | true bila kosong |
| `static boolean eq(int[] a, int[] b)` | true bila isi kedua buffer sama |
| `static int cmp(int[] a, int[] b)` | — |
| `static int[] copy(int[] a)` | salinan (array baru) |
| `static int[] cat(int[] a, int[] b)` | gabung dua buffer menjadi buffer baru |
| `static int[] cat3(int[] a, int[] b, int[] c)` | — |
| `static int[] slice(int[] s, int from, int n)` | potong `n` byte mulai `from` (di-clamp) |
| `static int[] sub(int[] s, int from, int n)` | — |
| `static int at(int[] s, int i)` | byte pada indeks, -1 di luar batas |
| `static int first(int[] s)` | — |
| `static int last(int[] s)` | — |
| `static int find(int[] s, int b, int from)` | indeks elemen, -1 bila tidak ada |
| `static int indexOf(int[] s, int[] ndl, int from)` | indeks kemunculan pertama dari `from`, -1 bila tidak ada |
| `static int lastIndexOf(int[] s, int[] ndl)` | — |
| `static boolean contains(int[] s, int[] ndl)` | — |
| `static boolean startsWith(int[] s, int[] p)` | — |
| `static boolean endsWith(int[] s, int[] p)` | — |
| `static int utf8Count(int[] s)` | jumlah code point UTF-8 (byte non-continuation) |
| `static int countByte(int[] s, int b)` | — |
| `static int count(int[] s, int[] ndl)` | — |
| `static boolean isSpace(int c)` | — |
| `static int[] trimLeft(int[] s)` | — |
| `static int[] trimRight(int[] s)` | — |
| `static int[] trim(int[] s)` | buang spasi/tab/CR/LF di kedua ujung |
| `static int[] upper(int[] s)` | — |
| `static int[] lower(int[] s)` | — |
| `static boolean equalsIgnoreCase(int[] a, int[] b)` | — |
| `static int[] repeat(int[] s, int n)` | — |
| `static int[] reverse(int[] s)` | salinan terbalik |
| `static int[] replaceByte(int[] s, int from, int to)` | — |
| `static int[] replaceSeq(int[] s, int[] old, int[] nw)` | — |
| `static int[] padLeft(int[] s, int width, int pad)` | — |
| `static int[] padRight(int[] s, int width, int pad)` | — |
| `static void fill(int[] s, int v)` | — |
| `static void zero(int[] s)` | — |
| `static int splitCount(int[] s, int sep)` | — |
| `static int[] splitAt(int[] s, int sep, int idx)` | — |
| `static int[] fromInt(int v)` | — |
| `static int[] fromLong(long v)` | — |
| `static int toInt(int[] s)` | parse desimal dari buffer |
| `static boolean isDigits(int[] s)` | — |
| `static int[] of(String s)` | String → salinan byte |
| `static String text(int[] s)` | buffer → String (materialisasi) |
| `static int[] hex(int[] data)` | byte → teks hex lowercase (int[]) |
| `static int hexChar(int v)` | — |
| `static void print(int[] s)` | — |
| `static void line(int[] s)` | tulis dengan newline |

---

## Cbor

`lib/core/Cbor.kop` · 11 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] one(int v)` | — |
| `static int[] cat(int[] a, int[] b)` | — |
| `static int[] encNil()` | — |
| `static int[] encBool(int b)` | — |
| `static int[] encInt(int v)` | — |
| `static int[] encBin(int[] b)` | — |
| `static int[] encStr(String s)` | — |
| `static int[] arrHead(int n)` | — |
| `static int[] mapHead(int n)` | — |
| `static int[] encArr(int[] items, int n)` | — |
| `static int[] encMap(int[] pairs, int n)` | — |

---

## Compress

`lib/core/Compress.kop` · 29 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] push(int[] a, int v)` | — |
| `static int[] push16(int[] a, int v)` | — |
| `static int[] push32(int[] a, int v)` | — |
| `static int get16(int[] a, int p)` | — |
| `static int get32(int[] a, int p)` | — |
| `static int get32be(int[] a, int p)` | — |
| `static int crc32(int[] data)` | CRC-32 (polinom 0xEDB88320) |
| `static int adler32(int[] data)` | Adler-32 (RFC 1950) |
| `static int[] deflateRaw(int[] data)` | — |
| `static int[] zlib(int[] data)` | — |
| `static int[] gzip(int[] data)` | — |
| `static void align(int[] st)` | — |
| `static int bit(int[] in, int[] st)` | — |
| `static int bits(int[] in, int[] st, int n)` | — |
| `static int byteAt(int[] in, int[] st)` | — |
| `static int[] huff(int[] lengths)` | hasil: [0]=maxBits, count[len] @1+len, simbol mulai @max+2 |
| `static int decode(int[] in, int[] st, int[] t)` | — |
| `static int[] fixedLit()` | — |
| `static int[] fixedDist()` | — |
| `static int lenBase(int s)` | — |
| `static int lenExtra(int s)` | — |
| `static int distBase(int s)` | — |
| `static int distExtra(int s)` | — |
| `static int inflateInto(int[] in, int from, int to, int[] out, int cap)` | tulis hasil ke out; -1 bila galat atau melebihi kapasitas |
| `static int[] inflateRaw(int[] data, int cap)` | deflate mentah dengan kapasitas keluaran eksplisit |
| `static int[] unzlib(int[] data)` | — |
| `static int[] gunzip(int[] data)` | — |
| `static String gunzipText(int[] data)` | — |
| `static int[] gzipText(String s)` | — |

---

## Config

`lib/core/Config.kop` · 9 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] parse(String text)` | — |
| `static int[] parseFile(String path)` | — |
| `static String get(int[] cfg, String key)` | ambil nilai |
| `static String getOr(int[] cfg, String key, String def)` | ambil nilai atau default bila tidak ada |
| `static int getInt(int[] cfg, String key, int def)` | — |
| `static boolean getBool(int[] cfg, String key, boolean def)` | — |
| `static boolean has(int[] cfg, String key)` | true bila elemen/kunci ada |
| `static String[] keys(int[] cfg)` | — |
| `static int[] section(int[] cfg, String name)` | ambil semua kunci dalam satu seksi, tanpa prefiks "seksi." |

---

## Cookie

`lib/core/Cookie.kop` · 16 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] parse(String header)` | header "Cookie: a=1; b=2" → peta nama→nilai |
| `static String get(int[] jar, String name)` | ambil nilai |
| `static String getOr(int[] jar, String name, String def)` | ambil nilai atau default bila tidak ada |
| `static boolean has(int[] jar, String name)` | true bila elemen/kunci ada |
| `static String[] names(int[] jar)` | — |
| `static String make(String name, String value)` | buat struktur baru dengan kapasitas awal |
| `static String withPath(String c, String p)` | — |
| `static String withDomain(String c, String d)` | — |
| `static String withMaxAge(String c, int sec)` | — |
| `static String withExpires(String c, int epoch)` | — |
| `static String withFlags(String c, boolean httpOnly, boolean secure)` | — |
| `static String withSameSite(String c, String mode)` | — |
| `static String headerFull(String name, String value, String path, int maxAge, boolean httpOnly, boolean secure, String sameSite)` | Set-Cookie lengkap; maxAge < 0 berarti tanpa Max-Age |
| `static String header(String c)` | — |
| `static String expired(String name, String path)` | cookie yang menghapus (Max-Age=0) |
| `static String httpDate(int epoch)` | "Thu, 12 Nov 2026 00:00:00 GMT" (RFC 1123) |

---

## Csv

`lib/core/Csv.kop` · 5 fungsi

Baca & tulis CSV (RFC 4180): field berkutip, pemisah koma, baris CRLF/LF.

```java
import Csv;

String[] row = Csv.parseRow("a,"b,c"");
System.out.println(Csv.formatRow(row));
```

**Catatan**

- `parseRow` menangani satu baris; pakai `lines` untuk memisahkan baris.

| Fungsi | Keterangan |
|---|---|
| `static String[] parseRow(String line)` | pecah satu baris CSV menjadi field |
| `static String formatRow(String[] fields)` | susun field menjadi satu baris CSV |
| `static String[] lines(String text)` | pecah teks menjadi baris |
| `static int countRows(String text)` | jumlah baris dalam teks |
| `static int[] rowLengths(String text)` | jumlah field tiap baris |

---

## Deque

`lib/core/Deque.kop` · 11 fungsi

Antrean dua ujung (ring buffer) berkapasitas tetap: push/pop depan-belakang.

```java
import Deque;

int[] d = Deque.make(4);
Deque.pushBack(d, 1);
Deque.pushFront(d, 0);
System.out.println(Deque.popFront(d));  // 0
```

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap)` | buat struktur baru dengan kapasitas awal |
| `static int capacity(int[] d)` | kapasitas saat ini |
| `static int size(int[] d)` | jumlah elemen saat ini |
| `static boolean isEmpty(int[] d)` | true bila kosong |
| `static boolean isFull(int[] d)` | true bila kapasitas penuh |
| `static boolean pushBack(int[] d, int v)` | tambah v di belakang; false bila penuh |
| `static boolean pushFront(int[] d, int v)` | tambah v di depan; false bila penuh |
| `static int popFront(int[] d)` | ambil & buang elemen depan (0 bila kosong) |
| `static int popBack(int[] d)` | ambil & buang elemen belakang (0 bila kosong) |
| `static int get(int[] d, int i)` | ambil nilai |
| `static void clear(int[] d)` | kosongkan isi |

---

## Digits

`lib/core/Digits.kop` · 11 fungsi

Manipulasi digit bilangan: jumlah, jumlah digit, palindrom, Armstrong, digital root, dan cetak basis lain.

```java
import Digits;

System.out.println(Digits.digitSum(12345));   // 15
System.out.println(Digits.isPalindrome(12321)); // true
Digits.printBase(255, 16);                    // ff
```

**Catatan**

- Input negatif ditangani dengan nilai absolut kecuali didokumentasikan lain.

| Fungsi | Keterangan |
|---|---|
| `static int reverse(int n)` | balik urutan digit: 12345 → 54321 (tanda tetap) |
| `static int digitSum(int n)` | jumlah digit desimal |
| `static int digitCount(int n)` | hitung berapa digit: 12345 → 5 |
| `static int pow10(int n)` | pangkat tertinggi 10 yang muat: 12345 → 10000 |
| `static int digitProduct(int n)` | hasil kali semua digit: 12345 → 120 (0 bila ada digit 0) |
| `static boolean isPalindrome(int n)` | apakah angka palindrom (negatif → false) |
| `static boolean isArmstrong(int n)` | apakah bilangan Armstrong/Narcissistic: 153 = 1³+5³+3³ |
| `static int digitalRoot(int n)` | akar digital (digital root): 0 untuk 0, selain 1..9 |
| `static int sumSquareDigits(int n)` | jumlah kuadrat digit |
| `static int sumCubeDigits(int n)` | jumlah pangkat tiga digit |
| `static void printBase(int n, int base)` | cetak n dalam basis 2..10. Basis > 10 belum bisa (butuh karakter 'A'-'F' yang belum tersedia); untuk itu printBase dibatasi dan return tanpa apa-apa keluaran basis. |

---

## Env

`lib/core/Env.kop` · 11 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static String get(String name)` | ambil nilai |
| `static String getOr(String name, String def)` | ambil nilai atau default bila tidak ada |
| `static boolean has(String name)` | true bila elemen/kunci ada |
| `static int getInt(String name, int def)` | — |
| `static boolean getBool(String name, boolean def)` | — |
| `static String home()` | — |
| `static String user()` | — |
| `static String shell()` | — |
| `static String tmpdir()` | — |
| `static String pwd()` | — |
| `static String lang()` | — |

---

## File

`lib/core/File.kop` · 15 fungsi

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
| `static int O_RDONLY()` | flag open: hanya baca |
| `static int O_WRONLY()` | flag open: hanya tulis |
| `static int O_CREAT()` | flag open: buat bila belum ada |
| `static int O_TRUNC()` | flag open: kosongkan bila sudah ada |
| `static int O_APPEND()` | flag open: selalu tulis di akhir |
| `static int MODE_0644()` | izin bawaan file baru (0644) |

---

## Json

`lib/core/Json.kop` · 72 fungsi

Parser/penulis JSON murni. Nilai direpresentasikan sebagai tape `int[]`; literal bahasa memakai `json {...}` / `json [...]`.

```java
import Json;

int[] u = json {"nama": "budi", "umur": 20};
System.out.println(Json.write(u));
int umur = Json.asInt(Json.get(u, "umur"));   // 20
```

**Catatan**

- `put`/`del`/`add` bersifat immutable: mengembalikan tape baru.
- Literal `json` maksimum 8 entri; untuk lebih banyak susun bertahap.
- `\uXXXX` didekode ke UTF-8 tanpa pasangan surrogate.

| Fungsi | Keterangan |
|---|---|
| `static int[] cat(int[] a, int[] b)` | (internal) gabung dua tape |
| `static int[] slice(int[] a, int from, int len)` | (internal) potong tape |
| `static int[] one(int v)` | (internal) tape satu elemen |
| `static int[] two(int a, int b)` | (internal) tape dua elemen |
| `static int[] nullV()` | nilai JSON null |
| `static int[] boolV(int b)` | nilai JSON boolean |
| `static int[] num(String text)` | nilai JSON angka dari teks desimal |
| `static int[] str(String s)` | nilai JSON string (di-escape) |
| `static int[] arr0()` | array JSON kosong |
| `static int[] obj0()` | objek JSON kosong |
| `static int skip(int[] t, int p)` | peta nilai: dipakai untuk skip/akses |
| `static int typeOf(int[] v)` | tipe nilai: 0 null, 1 bool, 2 angka, 3 string, 4 array, 5 objek |
| `static boolean isNull(int[] v)` | true bila null |
| `static boolean isBool(int[] v)` | true bila boolean |
| `static boolean isNumber(int[] v)` | true bila angka |
| `static boolean isString(int[] v)` | true bila string |
| `static boolean isArray(int[] v)` | true bila array |
| `static boolean isObject(int[] v)` | true bila objek |
| `static int asBool(int[] v)` | boolean nilai (0/1) |
| `static String asText(int[] v)` | teks mentah untuk number/string |
| `static int count(int[] v)` | jumlah elemen array / pasangan objek |
| `static int[] at(int[] arr, int idx)` | elemen array pada indeks (null di luar rentang) |
| `static boolean keyEq(int[] t, int at, int len, String key)` | (internal) bandingkan kunci pada tape |
| `static int[] get(int[] obj, String key)` | ambil nilai |
| `static boolean has(int[] obj, String key)` | true bila elemen/kunci ada |
| `static int[] put(int[] obj, String key, int[] val)` | objek baru dengan key diganti/ditambah (immutable) |
| `static int[] del(int[] obj, String key)` | objek tanpa kunci key (tape baru) |
| `static int[] add(int[] arr, int[] val)` | array baru + satu elemen di belakang |
| `static int[] arr1(int[] a)` | array JSON 1 elemen |
| `static int[] arr2(int[] a, int[] b)` | array JSON 2 elemen |
| `static int[] arr3(int[] a, int[] b, int[] c)` | array JSON 3 elemen |
| `static int[] arr4(int[] a, int[] b, int[] c, int[] d)` | array JSON 4 elemen |
| `static int[] arr5(int[] a, int[] b, int[] c, int[] d, int[] e)` | array JSON 5 elemen |
| `static int[] arr6(int[] a, int[] b, int[] c, int[] d, int[] e, int[] f)` | array JSON 6 elemen |
| `static int[] arr7(int[] a, int[] b, int[] c, int[] d, int[] e, int[] f, int[] g)` | array JSON 7 elemen |
| `static int[] arr8(int[] a, int[] b, int[] c, int[] d, int[] e, int[] f, int[] g, int[] h)` | array JSON 8 elemen |
| `static int[] obj1(String k1, int[] v1)` | objek JSON 1 pasangan |
| `static int[] obj2(String k1, int[] v1, String k2, int[] v2)` | objek JSON 2 pasangan |
| `static int[] obj3(String k1, int[] v1, String k2, int[] v2, String k3, int[] v3)` | objek JSON 3 pasangan |
| `static int[] obj4(String k1, int[] v1, String k2, int[] v2, String k3, int[] v3, String k4, int[] v4)` | objek JSON 4 pasangan |
| `static int asInt(int[] v)` | integer dari teks number (bagian pecahan dipotong); wrap int32 |
| `static double asDouble(int[] v)` | double dari teks number (dukung pecahan & eksponen) |
| `static boolean isWs(int c)` | (internal) karakter whitespace JSON |
| `static void pws(int[] t, int[] pos)` | (internal) lewati whitespace pada posisi parser |
| `static int hexv(int c)` | (internal) hex char → nilai |
| `static int[] putByte(int[] a, int b)` | (internal) tambah byte ke tape |
| `static int[] pstr(int[] t, int[] pos)` | parse string JSON; hasil = byte array (sudah di-unescape) |
| `static int[] pval(int[] t, int[] pos)` | (internal) parse satu nilai dari posisi |
| `static int[] parse(String text)` | parse teks JSON menjadi tape (null bila rusak) |
| `static int hexdig(int v)` | (internal) nilai → hex char |
| `static int[] quote(int[] s)` | (internal) string JSON ter-escape |
| `static int[] wr(int[] v)` | (internal) tulis nilai JSON ke teks |
| `static int[] keyAt(int[] obj, int idx)` | kunci pasangan ke-i pada objek |
| `static int[] valAt(int[] obj, int idx)` | nilai pasangan ke-i pada objek |
| `static int getInt(int[] obj, String key, int def)` | — |
| `static long getLong(int[] obj, String key, long def)` | — |
| `static double getDouble(int[] obj, String key, double def)` | — |
| `static boolean getBool(int[] obj, String key, boolean def)` | — |
| `static String getString(int[] obj, String key, String def)` | — |
| `static int[] getArray(int[] obj, String key, int[] def)` | — |
| `static int[] getObject(int[] obj, String key, int[] def)` | — |
| `static int[] atOr(int[] arr, int idx, int[] def)` | — |
| `static String typeName(int[] v)` | — |
| `static int[] path(int[] root, String expr)` | bagian path |
| `static boolean hasPath(int[] root, String expr)` | — |
| `static String pad2(int depth)` | — |
| `static String wrp(int[] v, int depth)` | — |
| `static String write(int[] v)` | API publik: serialisasi ke teks |
| `static String writePretty(int[] v)` | — |
| `static int writePrettyInto(int[] v, int[] buf)` | — |
| `static void print(int[] v)` | tulis ke stdout (fd 1) byte demi byte — melengkapi println(int) |
| `static void line(int[] v)` | tulis dengan newline |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int NULLV()` | tipe: null |
| `static int BOOLV()` | tipe: boolean |
| `static int NUMV()` | tipe: angka |
| `static int STRV()` | tipe: string |
| `static int ARRV()` | tipe: array |
| `static int OBJV()` | tipe: objek |

---

## List

`lib/core/List.kop` · 25 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap)` | buat struktur baru dengan kapasitas awal |
| `static int[] from(int[] src)` | — |
| `static int size(int[] xs)` | jumlah elemen saat ini |
| `static int cap(int[] xs)` | kapasitas saat ini |
| `static boolean isEmpty(int[] xs)` | true bila kosong |
| `static int get(int[] xs, int i)` | ambil nilai |
| `static int getOr(int[] xs, int i, int def)` | ambil nilai atau default bila tidak ada |
| `static boolean setAt(int[] xs, int i, int v)` | — |
| `static int[] grow(int[] xs, int need)` | — |
| `static int[] add(int[] xs, int v)` | tambahkan elemen |
| `static int[] addAll(int[] xs, int[] vs)` | — |
| `static int[] insertAt(int[] xs, int i, int v)` | — |
| `static int[] removeAt(int[] xs, int i)` | — |
| `static int[] removeValue(int[] xs, int v)` | — |
| `static int indexOf(int[] xs, int v)` | — |
| `static int lastIndexOf(int[] xs, int v)` | — |
| `static boolean contains(int[] xs, int v)` | — |
| `static int count(int[] xs, int v)` | — |
| `static void clear(int[] xs)` | kosongkan isi |
| `static int[] copy(int[] xs)` | salinan (array baru) |
| `static int[] toArray(int[] xs)` | — |
| `static int[] reverse(int[] xs)` | salinan terbalik |
| `static int sum(int[] xs)` | — |
| `static int max(int[] xs)` | nilai terbesar |
| `static int min(int[] xs)` | nilai terkecil |

---

## ListStr

`lib/core/ListStr.kop` · 24 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int base(int[] xs)` | — |
| `static int[] make(int cap, int poolCap)` | buat struktur baru dengan kapasitas awal |
| `static int[] from(String[] src)` | — |
| `static int size(int[] xs)` | jumlah elemen saat ini |
| `static int cap(int[] xs)` | kapasitas saat ini |
| `static boolean isEmpty(int[] xs)` | true bila kosong |
| `static String get(int[] xs, int i)` | ambil nilai |
| `static String getOr(int[] xs, int i, String def)` | ambil nilai atau default bila tidak ada |
| `static int[] growCap(int[] xs, int need)` | — |
| `static int[] growPool(int[] xs, int need)` | — |
| `static int[] add(int[] xs, String s)` | tambahkan elemen |
| `static int[] addAll(int[] xs, String[] vs)` | — |
| `static boolean setAt(int[] xs, int i, String s)` | — |
| `static int[] insertAt(int[] xs, int i, String s)` | — |
| `static int[] removeAt(int[] xs, int i)` | — |
| `static int[] removeValue(int[] xs, String s)` | — |
| `static int indexOf(int[] xs, String s)` | — |
| `static int lastIndexOf(int[] xs, String s)` | — |
| `static boolean contains(int[] xs, String s)` | — |
| `static void clear(int[] xs)` | kosongkan isi |
| `static String[] toArray(int[] xs)` | — |
| `static int[] copy(int[] xs)` | salinan (array baru) |
| `static String[] reverse(int[] xs)` | salinan terbalik |
| `static String join(int[] xs, String sep)` | — |

---

## Log

`lib/core/Log.kop` · 18 fungsi

Logging ke stderr dengan timestamp UTC dan level.

```java
import Log;

Log.info("boot", "server siap");
Log.error("db", "gagal konek");
```

**Catatan**

- stderr tidak di-buffer; aman untuk supervisor.

| Fungsi | Keterangan |
|---|---|
| `static void emit(String level, String tag, String msg)` | (internal) tulis baris level + pesan ke stderr |
| `static void info(String tag, String msg)` | log INFO |
| `static void warn(String tag, String msg)` | log WARN |
| `static void error(String tag, String msg)` | log ERROR |
| `static String levelName(int level)` | — |
| `static boolean enabled(int minLevel, int level)` | — |
| `static void emitTo(int fd, int minLevel, int level, String tag, String msg)` | tulis ke deskriptor tertentu (2 = stderr, 1 = stdout) |
| `static void debug(String tag, String msg)` | — |
| `static void infoMin(int minLevel, String tag, String msg)` | — |
| `static void warnMin(int minLevel, String tag, String msg)` | — |
| `static void errorMin(int minLevel, String tag, String msg)` | — |
| `static String lineOf(int level, String tag, String msg)` | — |
| `static boolean toFile(String path, int minLevel, int level, String tag, String msg)` | tulis ke file (append) bila level lolos; true bila ditulis |
| `static int fileSize(String path)` | — |
| `static boolean rotate(String path, int maxBytes, int keep)` | rotasi sederhana: path → path.1 → path.2 ... (keep=0 tanpa arsip) |
| `static boolean toFileRotating(String path, int maxBytes, int keep, int minLevel, int level, String tag, String msg)` | — |
| `static String jsonLine(int level, String tag, String msg)` | satu baris JSON: {"ts":...,"level":"INFO","tag":"...","msg":"..."} |
| `static boolean jsonToFile(String path, int level, String tag, String msg)` | — |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int DEBUG()` | — |
| `static int INFO()` | — |
| `static int WARN()` | — |
| `static int ERROR()` | — |
| `static int OFF()` | — |

---

## Map

`lib/core/Map.kop` · 13 fungsi

Hash map int→int dengan open addressing dan `grow` eksplisit.

```java
import Map;

int[] m = Map.make(16);
Map.put(m, 7, 70);
System.out.println(Map.get(m, 7));    // 70
System.out.println(Map.size(m));      // 1
```

**Catatan**

- Panggil `grow` bila ingin kapasitas lebih besar.
- Iterasi memakai `forEach(m, (k,v) => ...)`.

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap)` | buat struktur baru dengan kapasitas awal |
| `static int cap(int[] m)` | kapasitas saat ini |
| `static int size(int[] m)` | jumlah elemen saat ini |
| `static int slot(int[] m, int k)` | (internal) cari slot untuk kunci (probing) |
| `static int find(int[] m, int k)` | indeks elemen, -1 bila tidak ada |
| `static boolean has(int[] m, int k)` | true bila elemen/kunci ada |
| `static int get(int[] m, int k)` | ambil nilai |
| `static int getOr(int[] m, int k, int def)` | ambil nilai atau default bila tidak ada |
| `static boolean put(int[] m, int k, int v)` | simpan pasangan |
| `static boolean remove(int[] m, int k)` | hapus elemen |
| `static void clear(int[] m)` | kosongkan isi |
| `static int[] grow(int[] m)` | peta baru berkapasitas 2x + salin isi |
| `static void forEach(int[] m, fn f)` | iterasi semua pasangan: f(key, value) |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int EMPTY()` | marker slot kosong |
| `static int TOMB()` | marker slot terhapus |

---

## MapSS

`lib/core/MapSS.kop` · 19 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap, int poolCap)` | buat struktur baru dengan kapasitas awal |
| `static int cap(int[] m)` | kapasitas saat ini |
| `static int size(int[] m)` | jumlah elemen saat ini |
| `static int poolUsed(int[] m)` | — |
| `static boolean isEmpty(int[] m)` | true bila kosong |
| `static boolean has(int[] m, String key)` | true bila elemen/kunci ada |
| `static String get(int[] m, String key)` | ambil nilai |
| `static String getOr(int[] m, String key, String def)` | ambil nilai atau default bila tidak ada |
| `static boolean put(int[] m, String key, String val)` | simpan pasangan |
| `static boolean remove(int[] m, String key)` | hapus elemen |
| `static boolean removeAt(int[] m, int slot)` | — |
| `static void clear(int[] m)` | kosongkan isi |
| `static boolean used(int[] m, int i)` | — |
| `static String keyAtSlot(int[] m, int i)` | — |
| `static String valAtSlot(int[] m, int i)` | — |
| `static String[] keys(int[] m)` | daftar kunci sesuai urutan slot (melewati tombstone) |
| `static String[] values(int[] m)` | — |
| `static int[] growTo(int[] m, int cap, int poolCap)` | salin seluruh pasangan ke peta baru dengan kapasitas lebih besar |
| `static String joinKeys(int[] m, String sep)` | — |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int TOMB()` | — |

---

## MapStr

`lib/core/MapStr.kop` · 14 fungsi

Peta String→int dengan open addressing dan kolam kunci tetap. Dipakai untuk header HTTP, session, dan tabel kecil.

```java
import MapStr;

int[] m = MapStr.make(64, 512);
MapStr.put(m, "Content-Type", 1);
System.out.println(MapStr.get(m, "Content-Type")); // 1
```

**Catatan**

- Kapasitas dan pool kunci **tetap**; tidak ada grow.
- Kunci disalin ke pool; remove memakai tombstone.

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap, int poolCap)` | buat struktur baru dengan kapasitas awal |
| `static int cap(int[] m)` | kapasitas saat ini |
| `static int size(int[] m)` | jumlah elemen saat ini |
| `static int poolUsed(int[] m)` | byte pool kunci yang sudah terpakai |
| `static boolean has(int[] m, String key)` | true bila elemen/kunci ada |
| `static int get(int[] m, String key)` | ambil nilai |
| `static int getOr(int[] m, String key, int def)` | ambil nilai atau default bila tidak ada |
| `static boolean put(int[] m, String key, int v)` | simpan pasangan |
| `static boolean remove(int[] m, String key)` | hapus elemen |
| `static boolean removeAt(int[] m, int slot)` | hapus berdasarkan slot yang sudah diketahui (mis. saat sweep) |
| `static void clear(int[] m)` | kosongkan isi |
| `static boolean used(int[] m, int i)` | (internal) slot i terisi |
| `static int valAtSlot(int[] m, int i)` | (internal) nilai pada slot i |
| `static String keyAtSlot(int[] m, int i)` | (internal) kunci pada slot i |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int TOMB()` | marker slot terhapus |

---

## Math

`lib/core/Math.kop` · 72 fungsi

Matematika integer 32-bit: bit, pangkat/akar/log, FPB/modular, prima, kombinatorika, dan geometri.

```java
import Math;

System.out.println(Math.gcd(48, 18));       // 6
System.out.println(Math.isPrime(97));        // true
System.out.println(Math.factorial(6));       // 720
System.out.println(Math.powMod(2, 10, 1000)); // 24
```

**Catatan**

- Aritmetika membungkus 32-bit (wrap), bukan overflow error.
- Untuk nilai > 2^31 gunakan `long`/`Big`.

| Fungsi | Keterangan |
|---|---|
| `static int abs(int x)` | nilai absolut |
| `static int sign(int x)` | -1/0/1 |
| `static int min(int a, int b)` | nilai terkecil |
| `static int max(int a, int b)` | nilai terbesar |
| `static int min3(int a, int b, int c)` | terkecil dari tiga nilai |
| `static int max3(int a, int b, int c)` | terbesar dari tiga nilai |
| `static int clamp(int x, int lo, int hi)` | batasi ke rentang |
| `static int diff(int a, int b)` | /a - b/ |
| `static int sumTo(int n)` | 1 + 2 + ... + n (n >= 0; rumus Gauss) |
| `static int avgFloor(int a, int b)` | rata-rata dua bilangan tanpa overflow a+b |
| `static boolean isEven(int n)` | ═══════════════ paritas & bit ═══════════════ |
| `static boolean isOdd(int n)` | true bila ganjil |
| `static int popcount(int n)` | jumlah bit 1 dalam representasi 32-bit (two's complement) |
| `static int highestBit(int n)` | posisi bit 1 tertinggi (0..31); -1 bila n == 0 |
| `static int lowestBit(int n)` | posisi bit 1 terendah (0..31); -1 bila n == 0 |
| `static int bitLength(int n)` | jumlah bit signifikan (0 untuk 0; negatif dianggap 32 bit) |
| `static boolean isPowerOfTwo(int n)` | true bila pangkat dua |
| `static int nextPow2(int n)` | pangkat dua terkecil >= n (n >= 1); 0 bila meluap |
| `static int prevPow2(int n)` | pangkat dua terbesar <= n; 0 bila n < 1 |
| `static int reverseBits(int n)` | balik urutan 32 bit |
| `static int pow(int a, int e)` | pangkat integer |
| `static int mod(int a, int m)` | sisa non-negatif: selalu 0 .. m-1 (m > 0) |
| `static int addMod(int a, int b, int m)` | (a+b) mod m, aman overflow |
| `static int mulMod(int a, int b, int m)` | (a b) mod m tanpa overflow (doubling), m > 0 |
| `static int powMod(int a, int e, int m)` | pangkat modular (basis^exp mod m) |
| `static int sqrt(int n)` | akar kuadrat dibulatkan ke bawah |
| `static int cbrt(int n)` | akar pangkat tiga bulat ke bawah, tanda dipertahankan |
| `static int log2(int n)` | log2 bulat ke bawah (n >= 1) |
| `static int log10(int n)` | log10 bulat ke bawah (n >= 1) |
| `static boolean isPerfectSquare(int n)` | true bila kuadrat sempurna |
| `static boolean isPerfectCube(int n)` | true bila kubik sempurna |
| `static int gcd(int a, int b)` | faktor persekutuan terbesar |
| `static int gcd3(int a, int b, int c)` | FPB tiga nilai |
| `static int lcm(int a, int b)` | kelipatan persekutuan terkecil |
| `static int lcm3(int a, int b, int c)` | KPK tiga nilai |
| `static boolean isCoprime(int a, int b)` | true bila FPB(a,b) = 1 |
| `static int[] extendedGcd(int a, int b)` | Bezout: hasil {g, x, y} dengan a x + b y = g = gcd(a,b) |
| `static int modInverse(int a, int m)` | invers a modulo m (m > 1); -1 bila tidak ada |
| `static boolean isPrime(int n)` | uji prima deterministik untuk int |
| `static int nextPrime(int n)` | prima berikutnya > n |
| `static int prevPrime(int n)` | prima terbesar < n; -1 bila tidak ada |
| `static int[] primesUpTo(int n)` | daftar prima <= n (sieve Eratosthenes) |
| `static int primeCount(int n)` | banyaknya prima <= n (π(n)) |
| `static int nthPrime(int k)` | prima ke-k, k mulai 1 (2,3,5,...); 0 bila melampaui int |
| `static int[] factorize(int n)` | faktor prima menaik dengan multiplisitas: 360 → {2,2,2,3,3,5} |
| `static int numDivisors(int n)` | jumlah pembagi positif d(n) |
| `static int sumDivisors(int n)` | jumlah semua pembagi positif σ(n) |
| `static int aliquotSum(int n)` | jumlah pembagi sejati (kecuali n) |
| `static boolean isPerfect(int n)` | true bila bilangan sempurna |
| `static boolean isAbundant(int n)` | true bila bilangan berlebih |
| `static boolean isDeficient(int n)` | true bila bilangan kekurangan |
| `static int eulerPhi(int n)` | φ(n) Euler |
| `static int radical(int n)` | hasil kali faktor prima berbeda: 360 → 2 3 5 = 30 |
| `static int mobius(int n)` | fungsi Möbius μ(n) |
| `static int factorial(int n)` | n! (n kecil; wrap bila melebihi int) |
| `static int binomial(int n, int k)` | C(n,k) — perkalian eksak bertahap (aman selama hasil muat int) |
| `static int permutation(int n, int k)` | P(n,k) = n!/(n-k)! |
| `static int catalan(int n)` | bilangan Catalan C(n) = C(2n,n)/(n+1) |
| `static int fib(int n)` | bilangan Fibonacci ke-n |
| `static int lucas(int n)` | Lucas L(n): 2,1,3,4,7,11,... |
| `static int tribonacci(int n)` | Tribonacci: 0,0,1,1,2,4,7,13,... |
| `static int triangular(int n)` | bilangan segitiga T(n) = n(n+1)/2 (n >= 0) |
| `static int derangement(int n)` | derangement !n (subfaktorial) |
| `static int stirling2(int n, int k)` | bilangan Stirling jenis kedua S(n,k) |
| `static int bell(int n)` | bilangan Bell B(n) = Σ S(n,k) |
| `static int distanceSq(int x1, int y1, int x2, int y2)` | kuadrat jarak Euclidean |
| `static int manhattan(int x1, int y1, int x2, int y2)` | jarak Manhattan |
| `static int rectArea(int w, int h)` | luas persegi panjang |
| `static int rectPerimeter(int w, int h)` | keliling persegi panjang |
| `static boolean isRightTriangle(int a, int b, int c)` | apakah (a,b,c) sisi segitiga siku-siku (urut sembarang) |
| `static int heron16(int a, int b, int c)` | 16·luas² segitiga (rumus Heron, bilangan bulat): sisi a,b,c |
| `static boolean isTriangle(int a, int b, int c)` | apakah tiga sisi membentuk segitiga valid |

---

## Matrix

`lib/core/Matrix.kop` · 15 fungsi

Matriks persegi yang disimpan sebagai `int[]` datar (baris demi baris): aritmetika, transpose, trace, determinan.

```java
import Matrix;

int[] m = Matrix.identity(3);
int[] t = Matrix.transpose(m);
System.out.println(Matrix.trace(m));   // 3
```

**Catatan**

- Ukuran matriks diturunkan dari akar panjang array; input harus persegi.

| Fungsi | Keterangan |
|---|---|
| `static int[] zero(int n)` | matriks nol n×n |
| `static int[] identity(int n)` | matriks identitas n×n |
| `static int get(int[] a, int n, int r, int c)` | ambil nilai |
| `static void set(int[] a, int n, int r, int c, int v)` | set elemen (baris r, kolom c) = v |
| `static int[] add(int[] a, int[] b)` | a + b (ukuran sama) |
| `static int[] sub(int[] a, int[] b)` | a − b |
| `static int[] scale(int[] a, int k)` | k·a |
| `static int[] transpose(int n, int[] a)` | transpose n×n |
| `static int[] mul(int n, int[] a, int[] b)` | perkalian dua matriks n×n |
| `static int trace(int n, int[] a)` | jumlah elemen diagonal utama |
| `static int det1(int[] a)` | determinan 1×1 |
| `static int det2(int[] a)` | determinan 2×2 |
| `static int det3(int[] a)` | determinan 3×3 (aturan Sarrus) |
| `static int det(int n, int[] a)` | determinan umum n<=3; di luar itu 0 (jujur: belum ada rekursi kofaktor umum tanpa alokasi dinamis bersarang) |
| `static boolean isIdentity(int n, int[] a)` | apakah matriks identitas |

---

## MessagePack

`lib/core/MessagePack.kop` · 11 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] one(int v)` | — |
| `static int[] cat(int[] a, int[] b)` | — |
| `static int[] encNil()` | — |
| `static int[] encBool(int b)` | — |
| `static int[] encInt(int v)` | — |
| `static int[] encBin(int[] b)` | — |
| `static int[] encStr(String s)` | — |
| `static int[] arrHead(int n)` | — |
| `static int[] encArr(int[] items, int n)` | — |
| `static int[] encMap(int[] pairs, int n)` | — |
| `static int[] mapHead(int n)` | — |

---

## Path

`lib/core/Path.kop` · 6 fungsi

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
| `static int SEP()` | byte pemisah path |

---

## Regex

`lib/core/Regex.kop` · 6 fungsi

Mesin regex backtracking (mirip PCRE/Python): kelas, kuantifier, grup, alternasi, anchor.

```java
import Regex;

System.out.println(Regex.search("\\d{4}-\\d{2}", "tahun 2024-05"));
String[] g = Regex.groups("(\\w+)@(\\w+)", "halo@dunia");
```

**Catatan**

- `groups` indeks 0 = seluruh kecocokan, lalu grup tangkap.
- Kelas: [abc], [^abc], [a-z]; escape: \d \w \s (dan huruf besar).

| Fungsi | Keterangan |
|---|---|
| `static boolean search(String pat, String txt)` | true bila ada kecocokan di mana saja |
| `static boolean matches(String pat, String txt)` | true bila seluruh teks cocok |
| `static int[] findAll(String pat, String txt)` | int[] offset awal semua kecocokan |
| `static String[] groups(String pat, String txt)` | String[] grup tangkap (indeks 0 = seluruh) |
| `static String replaceAll(String pat, String txt, String rep)` | ganti semua kecocokan dengan teks lain |
| `static int count(String pat, String txt)` | jumlah kecocokan |

---

## Router

`lib/core/Router.kop` · 14 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int base(int[] r)` | — |
| `static int[] make(int cap, int poolCap)` | buat struktur baru dengan kapasitas awal |
| `static int size(int[] r)` | jumlah elemen saat ini |
| `static int cap(int[] r)` | kapasitas saat ini |
| `static int[] growCap(int[] r, int need)` | — |
| `static int[] growPool(int[] r, int need)` | — |
| `static int[] add(int[] r, String method, String pattern, int id)` | tambah rute; id bebas ditentukan pemanggil |
| `static String methodAt(int[] r, int i)` | — |
| `static String patternAt(int[] r, int i)` | — |
| `static int idAt(int[] r, int i)` | — |
| `static boolean matchPattern(String pat, String path, int[] params, boolean capture)` | cocokkan satu pola; isi params bila tidak null |
| `static int matchInto(int[] r, String method, String path, int[] params)` | — |
| `static int match(int[] r, String method, String path)` | — |
| `static int allowMask(int[] r, String path)` | id rute yang metodenya cocok (untuk header Allow), -1 bila kosong |

---

## Set

`lib/core/Set.kop` · 8 fungsi

Himpunan int di atas Map: tambah/hapus/uji keanggotaan.

```java
import Set;

int[] s = Set.make(16);
Set.add(s, 4);
System.out.println(Set.has(s, 4));  // true
```

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap)` | buat struktur baru dengan kapasitas awal |
| `static boolean add(int[] s, int v)` | tambahkan elemen |
| `static boolean has(int[] s, int v)` | true bila elemen/kunci ada |
| `static boolean remove(int[] s, int v)` | hapus elemen |
| `static int size(int[] s)` | jumlah elemen saat ini |
| `static void clear(int[] s)` | kosongkan isi |
| `static int[] grow(int[] s)` | salin set dengan kapasitas lebih besar |
| `static void forEach(int[] s, fn f)` | panggil f(x) untuk tiap elemen |

---

## SetStr

`lib/core/SetStr.kop` · 11 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap, int poolCap)` | buat struktur baru dengan kapasitas awal |
| `static int size(int[] s)` | jumlah elemen saat ini |
| `static int cap(int[] s)` | kapasitas saat ini |
| `static boolean isEmpty(int[] s)` | true bila kosong |
| `static boolean add(int[] s, String v)` | tambahkan elemen |
| `static boolean has(int[] s, String v)` | true bila elemen/kunci ada |
| `static boolean remove(int[] s, String v)` | hapus elemen |
| `static void clear(int[] s)` | kosongkan isi |
| `static String[] values(int[] s)` | — |
| `static boolean containsAll(int[] s, String[] vs)` | — |
| `static int addAll(int[] s, String[] vs)` | — |

---

## Stats

`lib/core/Stats.kop` · 14 fungsi

Statistik deskriptif sederhana untuk array int: mean, median, modus, variansi, standar deviasi.

```java
import Stats;

int[] a = new int[5];
a[0] = 2; a[1] = 4; a[2] = 4; a[3] = 6; a[4] = 9;
System.out.println(Stats.mean(a));      // 5
System.out.println(Stats.median(a));    // 4
```

| Fungsi | Keterangan |
|---|---|
| `static int sum(int[] xs)` | jumlah semua elemen |
| `static int product(int[] xs)` | hasil kali semua elemen (mudah meluap) |
| `static int min(int[] xs)` | nilai terkecil |
| `static int max(int[] xs)` | nilai terbesar |
| `static int range(int[] xs)` | maks - min |
| `static int mean(int[] xs)` | rata-rata dengan pembagian bulat (pemotongan ke arah nol) |
| `static int sumSquares(int[] xs)` | jumlah kuadrat elemen |
| `static int sumCubes(int[] xs)` | jumlah pangkat tiga elemen |
| `static int median(int[] xs)` | median; genap → rata-rata bulat dua tengah |
| `static int mode(int[] xs)` | modus; seri → nilai terkecil yang menang |
| `static int variance(int[] xs)` | variansi populasi (bulat, pendekatan): E[x²] − (E[x])² |
| `static int stddev(int[] xs)` | simpangan baku populasi (akar bulat dari variansi) |
| `static int countEven(int[] xs)` | jumlah elemen genap / ganjil |
| `static int countOdd(int[] xs)` | jumlah elemen ganjil |

---

## Str

`lib/core/Str.kop` · 109 fungsi

Utilitas teks native `String`: perbandingan, potong/gabung, cari/ganti, trim/case, split/join, padding, konversi semua tipe skalar, hex, dan UTF-8. Tidak memakai `int[]`.

```java
import Str;

String s = "  Kopi  ";
System.out.println(Str.trim(s));                        // Kopi
System.out.println(Str.padLeft(Str.fromInt(7), 3, 48)); // 007
System.out.println(Str.join(Str.split("a,b", ","), "|")); // a|b
```

**Catatan**

- `length` adalah jumlah byte, bukan karakter Unicode.
- `fromDouble` maksimum 6 digit pecahan; format specifier belum ada.
- Buffer byte ada di modul `Bytes`; konversi `Net.strbytes`/`Net.mkstr`.

| Fungsi | Keterangan |
|---|---|
| `static int len(String s)` | panjang byte |
| `static boolean isEmpty(String s)` | true bila kosong |
| `static boolean isSpace(int c)` | — |
| `static boolean isBlank(String s)` | — |
| `static boolean eq(String a, String b)` | true bila isi kedua teks sama |
| `static boolean equals(String a, String b)` | — |
| `static int cmp(String a, String b)` | perbandingan leksikografis (-1/0/1) |
| `static int compare(String a, String b)` | — |
| `static int compareIgnoreCase(String a, String b)` | — |
| `static boolean equalsIgnoreCase(String a, String b)` | perbandingan isi tanpa peduli huruf besar/kecil |
| `static String copy(String s)` | String immutable: salinan aman dipakai sebagai nilai baru |
| `static String cat(String a, String b)` | gabung dua teks |
| `static String join(String[] parts, String sep)` | gabung String[] dengan pemisah |
| `static String sub(String s, int from, int n)` | potong `n` byte mulai `from` (di-clamp) |
| `static String slice(String s, int from, int n)` | alias `sub` untuk String |
| `static String left(String s, int n)` | — |
| `static String right(String s, int n)` | — |
| `static String dropLeft(String s, int n)` | — |
| `static String dropRight(String s, int n)` | — |
| `static String before(String s, String sep)` | potongan sebelum/sesudah pemisah pertama ("" bila tidak ada) |
| `static String after(String s, String sep)` | — |
| `static String beforeLast(String s, String sep)` | potongan sebelum/sesudah pemisah terakhir |
| `static String afterLast(String s, String sep)` | — |
| `static String between(String s, String open, String close)` | — |
| `static int at(String s, int i)` | byte pada indeks (0..255), -1 di luar batas |
| `static int charAt(String s, int i)` | — |
| `static int first(String s)` | — |
| `static int last(String s)` | — |
| `static String removePrefix(String s, String p)` | — |
| `static String removeSuffix(String s, String p)` | — |
| `static int indexOf(String s, String ndl, int from)` | indeks kemunculan pertama dari `from`, -1 bila tidak ada |
| `static int lastIndexOf(String s, String ndl)` | indeks kemunculan terakhir, -1 bila tidak ada |
| `static boolean contains(String s, String ndl)` | true bila memuat substring |
| `static boolean startsWith(String s, String p)` | true bila diawali ndl |
| `static boolean endsWith(String s, String p)` | true bila diakhiri ndl |
| `static int count(String s, String ndl)` | jumlah kemunculan tidak tumpang tindih |
| `static int countByte(String s, int b)` | berapa kali byte b muncul |
| `static String trimLeft(String s)` | buang whitespace di ujung kiri |
| `static String trimRight(String s)` | buang whitespace di ujung kanan |
| `static String trim(String s)` | buang spasi/tab/CR/LF di kedua ujung |
| `static String strip(String s)` | — |
| `static String capitalize(String s)` | — |
| `static String uncapitalize(String s)` | — |
| `static String upper(String s)` | huruf besar (ASCII) |
| `static String lower(String s)` | huruf kecil (ASCII) |
| `static int lowerByte(int c)` | — |
| `static String swapCase(String s)` | — |
| `static String repeat(String s, int n)` | ulang string n kali |
| `static String reverse(String s)` | balik byte (ASCII). Untuk UTF-8 gunakan utf8Slice/utf8Reverse. |
| `static String replaceSeq(String s, String old, String nw)` | ganti semua kemunculan old → nw |
| `static String replace(String s, String old, String nw)` | — |
| `static String replaceByte(String s, int from, int to)` | ganti semua byte from → to |
| `static String padLeft(String s, int width, int pad)` | tambah byte pad di kiri sampai width |
| `static String padRight(String s, int width, int pad)` | tambah byte pad di kanan sampai width |
| `static String center(String s, int width, int pad)` | — |
| `static String truncate(String s, int max)` | — |
| `static String ellipsis(String s, int max)` | — |
| `static int splitCountChar(String s, int sep)` | — |
| `static String splitAtChar(String s, int sep, int idx)` | — |
| `static String[] splitChar(String s, int sep)` | — |
| `static int splitCount(String s, String sep)` | berapa banyak potongan dengan pemisah sep |
| `static String splitAt(String s, String sep, int idx)` | potongan ke-idx dengan pemisah sep |
| `static String[] split(String s, String sep)` | pecah teks dengan pemisah String menjadi String[] |
| `static String[] lines(String s)` | baris: pemisah \n, \r di ujung tiap baris dibuang |
| `static String[] words(String s)` | kata: pemisah spasi/tab/CR/LF, pemisah berurutan dilewati |
| `static String fromInt(int v)` | int → teks desimal |
| `static String fromUInt(int v)` | uint32 dalam nilai kanonik int (zero-extended) |
| `static String fromLong(long v)` | long → teks desimal |
| `static String fromULong(long v)` | uint64: divisi unsigned bit-per-bit |
| `static String boolText(boolean b)` | boolean → "true"/"false" |
| `static String fromBool(boolean b)` | — |
| `static String fromByte(byte v)` | varian skalar: tidak ada overloading di Kopi, jadi nama eksplisit |
| `static String fromShort(short v)` | — |
| `static String fromUInt8(uint8 v)` | — |
| `static String fromUInt16(uint16 v)` | — |
| `static String fromUInt32(uint32 v)` | — |
| `static String fromUInt64(uint64 v)` | — |
| `static String fromFloat(float v)` | — |
| `static String fromChar(int c)` | satu code point (0..255) menjadi teks |
| `static String fromDouble(double d)` | double → desimal sederhana (maks 6 digit pecahan, nol ekor dibuang) |
| `static int toInt(String s)` | parse desimal; 0 bila bukan angka |
| `static int toIntOr(String s, int def)` | — |
| `static long toLong(String s)` | — |
| `static int parseInt(String s, int radix)` | — |
| `static long parseLong(String s, int radix)` | — |
| `static int digitValue(int c)` | — |
| `static double toDouble(String s)` | double sederhana: [-]digit[.digit][e[+-]digit] |
| `static byte toByte(String s)` | varian skalar hasil parsing |
| `static short toShort(String s)` | — |
| `static uint8 toUInt8(String s)` | — |
| `static uint16 toUInt16(String s)` | — |
| `static uint32 toUInt32(String s)` | — |
| `static uint64 toUInt64(String s)` | — |
| `static float toFloat(String s)` | — |
| `static boolean toBool(String s)` | true: "true", "1", "yes", "on" (tanpa beda huruf besar/kecil) |
| `static boolean isDigits(String s)` | true bila seluruh isi digit desimal |
| `static boolean isNumber(String s)` | — |
| `static boolean isHex(String s)` | — |
| `static String toHex(String s)` | byte teks → hex lowercase |
| `static int hexChar(int v)` | — |
| `static String fromHex(String s)` | hex → teks byte |
| `static int utf8Count(String s)` | jumlah code point UTF-8 |
| `static boolean utf8Valid(String s)` | — |
| `static int utf8Offset(String s, int cp)` | offset byte untuk indeks code point ke-cp (0..utf8Count) |
| `static String utf8Slice(String s, int from, int n)` | potong n code point mulai code point from (bukan byte) |
| `static int utf8CodeAt(String s, int i)` | code point pertama pada byte offset i (-1 bila tidak valid) |
| `static String utf8Reverse(String s)` | balik per code point (UTF-8 aman untuk teks valid) |
| `static void print(String s)` | tulis teks ke stdout tanpa newline |
| `static void line(String s)` | tulis dengan newline |

---

## Test

`lib/core/Test.kop` · 16 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] make()` | buat struktur baru dengan kapasitas awal |
| `static int passed(int[] t)` | — |
| `static int failed(int[] t)` | — |
| `static void ok(int[] t, boolean cond, String name)` | — |
| `static void fail(int[] t, String name)` | — |
| `static void eqInt(int[] t, int got, int want, String name)` | — |
| `static void neInt(int[] t, int got, int bad, String name)` | — |
| `static void eqLong(int[] t, long got, long want, String name)` | — |
| `static void eqStr(int[] t, String got, String want, String name)` | — |
| `static void eqBool(int[] t, boolean got, boolean want, String name)` | — |
| `static void near(int[] t, double got, double want, double eps, String name)` | — |
| `static void true_(int[] t, boolean cond, String name)` | — |
| `static void false_(int[] t, boolean cond, String name)` | — |
| `static int report(int[] t, String tag)` | — |
| `static int code(int[] t)` | — |
| `static void reset(int[] t)` | — |

---

## Text

`lib/core/Text.kop` · 19 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] make(int cap)` | buat struktur baru dengan kapasitas awal |
| `static int len(int[] b)` | — |
| `static int cap(int[] b)` | kapasitas saat ini |
| `static boolean isEmpty(int[] b)` | true bila kosong |
| `static int[] reserve(int[] b, int need)` | pastikan ruang untuk `need` byte lagi; kembalikan buffer (mungkin baru) |
| `static int[] appendByte(int[] b, int v)` | — |
| `static int[] append(int[] b, String s)` | buka untuk menambah di akhir |
| `static int[] appendBytes(int[] b, int[] s)` | — |
| `static int[] appendInt(int[] b, int v)` | — |
| `static int[] appendLong(int[] b, long v)` | — |
| `static int[] appendDouble(int[] b, double v)` | — |
| `static int[] appendBool(int[] b, boolean v)` | — |
| `static int[] appendLine(int[] b, String s)` | — |
| `static int[] newline(int[] b)` | — |
| `static int[] repeatByte(int[] b, int c, int n)` | ulang byte yang sama `n` kali |
| `static int[] truncate(int[] b, int n)` | buang `n` byte terakhir (di-clamp) |
| `static void clear(int[] b)` | kosongkan isi |
| `static String build(int[] b)` | — |
| `static String toString(int[] b)` | — |

---

## Time

`lib/core/Time.kop` · 32 fungsi

Waktu UTC: epoch detik, monotonic milidetik, sleep, format tanggal, dan nama hari.

```java
import Time;

System.out.println(Time.format(Time.epoch()));
Time.sleepMs(10);
```

**Catatan**

- `epoch()` 32-bit (batas 2038); `monoMs()` 64-bit.
- Format selalu UTC; timezone lokal belum ada.

| Fungsi | Keterangan |
|---|---|
| `static int epoch()` | detik sejak 1970 (UTC, 32-bit) |
| `static long monoMs()` | milidetik monotonic (tidak mundur) |
| `static int sleepMs(int ms)` | tidur ms milidetik |
| `static int[] civil(int sec)` | [tahun, bulan(1..12), hari(1..31), jam, menit, detik] UTC |
| `static int weekday(int sec)` | nama hari dari epoch detik |
| `static String format(int sec)` | format "YYYY-MM-DD HH:MM:SS" UTC |
| `static boolean isLeap(int y)` | — |
| `static int daysInMonth(int y, int m)` | — |
| `static int daysFromCivil(int y, int m, int d)` | tanggal → jumlah hari sejak 1970-01-01 (algoritma Hinnant) |
| `static int fromCivil(int y, int m, int d, int hh, int mm, int ss)` | epoch dari komponen UTC; -1 bila komponen tidak valid |
| `static int[] civilZone(int sec, int off)` | — |
| `static int startOfDay(int sec, int off)` | — |
| `static int startOfMonth(int sec, int off)` | — |
| `static int parseOffset(String s, int at)` | "+07:00" / "-0530" → detik; 0 bila polos/Z |
| `static int parseISO(String s)` | "2026-11-12", "2026-11-12T08:30:00", "...Z", "...+07:00" |
| `static int parseISOOr(String s, int def)` | — |
| `static String isoDate(int sec)` | — |
| `static String isoTime(int sec)` | — |
| `static String formatISO(int sec)` | — |
| `static String formatISOZone(int sec, int off)` | — |
| `static String formatZone(int sec, int off)` | — |
| `static int addSeconds(int sec, int n)` | — |
| `static int addMinutes(int sec, int n)` | — |
| `static int addHours(int sec, int n)` | — |
| `static int addDays(int sec, int n)` | — |
| `static int addMonths(int sec, int n)` | tambah bulan kalender dengan clamp akhir bulan |
| `static int diffSeconds(int a, int b)` | — |
| `static int diffDays(int a, int b)` | — |
| `static int diffMinutes(int a, int b)` | — |
| `static int diffHours(int a, int b)` | — |
| `static String duration(int sec)` | — |
| `static String nowISO()` | — |

---

## Uuid

`lib/core/Uuid.kop` · 6 fungsi

UUID versi 4 (RFC 4122) acak + format/parse/validasi UUID.

```java
import Uuid;

String id = Uuid.v4();
System.out.println(Uuid.valid(id));
```

**Catatan**

- Format 8-4-4-4-12; versi 4 & varian RFC 4122.

| Fungsi | Keterangan |
|---|---|
| `static String v4()` | UUID acak versi 4 |
| `static String nil()` | UUID kosong |
| `static String format(int[] b)` | 16 byte → teks UUID |
| `static int[] parse(String s)` | teks UUID → 16 byte |
| `static boolean valid(String s)` | true bila teks UUID valid |
| `static int version(String s)` | versi UUID (1..5) |

---
