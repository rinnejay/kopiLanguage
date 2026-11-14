# Referensi API — Core

Tipe dasar, koleksi, matematika, dan utilitas bahasa. Semua signature di bawah diekstrak langsung dari source `lib/core/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

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

## BigInt

`lib/core/BigInt.kop` · 22 fungsi

Bilangan bulat presisi sembarang: tambah, kali, bagi, pangkat modulo, invers modulo, FPB, konversi byte/desimal.

```java
import BigInt;

int[] a = BigInt.fromDecimal("12345678901234567890");
System.out.println(BigInt.toDecimal(BigInt.mul(a, a)));
```

**Catatan**

- Nilai non-negatif; `sub` butuh a >= b.
- Pembagian panjang biner: benar, tapi lambat untuk modulus besar.

| Fungsi | Keterangan |
|---|---|
| `static int[] copy(int[] x)` | salinan (array baru) |
| `static boolean isZero(int[] x)` | — |
| `static int[] fromInt(int v)` | int → big |
| `static int toInt(int[] x)` | big → int |
| `static int[] fromBytes(int[] b)` | byte big-endian → big |
| `static int[] toBytes(int[] x, int len)` | big → byte big-endian |
| `static int bitLength(int[] x)` | jumlah bit significant |
| `static int[] add(int[] a, int[] b)` | a + b |
| `static int[] sub(int[] a, int[] b)` | a - b (a >= b) |
| `static int[] mul(int[] a, int[] b)` | a * b |
| `static int cmp(int[] a, int[] b)` | perbandingan -1/0/1 |
| `static int[] shr(int[] a, int n)` | a >> n |
| `static int[] shl(int[] a, int n)` | a << n |
| `static boolean testBit(int[] a, int n)` | uji bit ke-n |
| `static int[] div(int[] a, int[] b)` | kuosien a / b |
| `static int[] rem(int[] a, int[] b)` | sisa a % b |
| `static int[] mod(int[] a, int[] m)` | sisa a % m |
| `static int[] modexp(int[] base, int[] exp, int[] m)` | (base^exp) mod m |
| `static int[] gcd(int[] a, int[] b)` | FPB(a, b) |
| `static int[] modInv(int[] a, int[] m)` | invers modular a^-1 mod m |
| `static String toDecimal(int[] x)` | big → teks desimal |
| `static int[] fromDecimal(String s)` | teks desimal → big |

---

## Bytes

`lib/core/Bytes.kop` · 55 fungsi

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
| `static int lenB(byte[] b)` | — |
| `static int[] widen(byte[] b)` | byte[] signed → int[] 0..255 (nilai mentah buffer) |
| `static byte[] narrow(int[] a)` | int[] → byte[] (nilai dipotong ke 8 bit) |
| `static byte[] copyB(byte[] b)` | — |
| `static String textB(byte[] b)` | — |
| `static void printB(byte[] b)` | — |
| `static void lineB(byte[] b)` | — |
| `static int[] of(String s)` | String → salinan byte |
| `static String text(int[] s)` | buffer → String (materialisasi) |
| `static int[] hex(int[] data)` | byte → teks hex lowercase (int[]) |
| `static int hexChar(int v)` | — |
| `static void print(int[] s)` | — |
| `static void line(int[] s)` | tulis dengan newline |

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
| `static int EMPTY = (int)-2147483648L` | marker slot kosong |
| `static int TOMB = (int)-2147483647L` | marker slot terhapus |

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
| `static int TOMB = 2147483647` | — |

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
| `static int TOMB = 2147483647` | marker slot terhapus |

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
