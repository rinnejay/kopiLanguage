# Referensi API — Text

Teks native String, transformasi teks, dan regex. Semua signature di bawah diekstrak langsung dari source `lib/text/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Regex

`lib/text/Regex.kop` · 6 fungsi

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

## Str

`lib/text/Str.kop` · 109 fungsi

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

## Text

`lib/text/Text.kop` · 19 fungsi

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
