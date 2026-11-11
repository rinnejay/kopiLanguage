# Referensi API — Core

Tipe dasar, teks, matematika, koleksi, file, waktu. Semua signature di bawah diekstrak langsung dari source `lib/core/`; deskripsi berasal dari komentar sumber atau metadata.

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

## File

`lib/core/File.kop` · 13 fungsi

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
| `static int openRead(int[] path)` | buka file hanya-baca; -1 bila gagal |
| `static int create(int[] path)` | buat/buka file |
| `static int append(int[] path)` | buka untuk menambah di akhir |
| `static int close(int fd)` | tutup deskriptor |
| `static int writeAll(int fd, int[] data)` | tulis seluruh data; jumlah byte tertulis |
| `static int readInto(int fd, int[] buf)` | baca sampai buffer penuh/EOF; jumlah byte terbaca |
| `static int seekSet(int fd, int off)` | lompat ke offset dari awal |
| `static int seekCur(int fd, int off)` | lompat relatif dari posisi |
| `static int seekEnd(int fd, int off)` | lompat relatif dari akhir |
| `static int size(int fd)` | jumlah elemen saat ini |
| `static int remove(int[] path)` | hapus elemen |
| `static int renameTo(int[] oldp, int[] newp)` | ganti nama file/direktori |
| `static int mkdir(int[] path)` | buat direktori |

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

`lib/core/Json.kop` · 57 fungsi

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
| `static int[] num(int[] text)` | nilai JSON angka dari teks desimal |
| `static int[] str(int[] s)` | nilai JSON string (di-escape) |
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
| `static int[] asText(int[] v)` | teks mentah untuk number/string |
| `static int count(int[] v)` | jumlah elemen array / pasangan objek |
| `static int[] at(int[] arr, int idx)` | elemen array pada indeks (null di luar rentang) |
| `static boolean keyEq(int[] t, int at, int len, int[] key)` | (internal) bandingkan kunci pada tape |
| `static int[] get(int[] obj, int[] key)` | ambil nilai |
| `static boolean has(int[] obj, int[] key)` | true bila elemen/kunci ada |
| `static int[] put(int[] obj, int[] key, int[] val)` | objek baru dengan key diganti/ditambah (immutable) |
| `static int[] del(int[] obj, int[] key)` | objek tanpa kunci key (tape baru) |
| `static int[] add(int[] arr, int[] val)` | array baru + satu elemen di belakang |
| `static int[] arr1(int[] a)` | array JSON 1 elemen |
| `static int[] arr2(int[] a, int[] b)` | array JSON 2 elemen |
| `static int[] arr3(int[] a, int[] b, int[] c)` | array JSON 3 elemen |
| `static int[] arr4(int[] a, int[] b, int[] c, int[] d)` | array JSON 4 elemen |
| `static int[] arr5(int[] a, int[] b, int[] c, int[] d, int[] e)` | array JSON 5 elemen |
| `static int[] arr6(int[] a, int[] b, int[] c, int[] d, int[] e, int[] f)` | array JSON 6 elemen |
| `static int[] arr7(int[] a, int[] b, int[] c, int[] d, int[] e, int[] f, int[] g)` | array JSON 7 elemen |
| `static int[] arr8(int[] a, int[] b, int[] c, int[] d, int[] e, int[] f, int[] g, int[] h)` | array JSON 8 elemen |
| `static int[] obj1(int[] k1, int[] v1)` | objek JSON 1 pasangan |
| `static int[] obj2(int[] k1, int[] v1, int[] k2, int[] v2)` | objek JSON 2 pasangan |
| `static int[] obj3(int[] k1, int[] v1, int[] k2, int[] v2, int[] k3, int[] v3)` | objek JSON 3 pasangan |
| `static int[] obj4(int[] k1, int[] v1, int[] k2, int[] v2, int[] k3, int[] v3, int[] k4, int[] v4)` | objek JSON 4 pasangan |
| `static int asInt(int[] v)` | integer dari teks number (bagian pecahan dipotong); wrap int32 |
| `static double asDouble(int[] v)` | double dari teks number (dukung pecahan & eksponen) |
| `static boolean isWs(int c)` | (internal) karakter whitespace JSON |
| `static void pws(int[] t, int[] pos)` | (internal) lewati whitespace pada posisi parser |
| `static int hexv(int c)` | (internal) hex char → nilai |
| `static int[] putByte(int[] a, int b)` | (internal) tambah byte ke tape |
| `static int[] pstr(int[] t, int[] pos)` | parse string JSON; hasil = byte array (sudah di-unescape) |
| `static int[] pval(int[] t, int[] pos)` | (internal) parse satu nilai dari posisi |
| `static int[] parse(int[] text)` | parse teks JSON menjadi tape (null bila rusak) |
| `static int hexdig(int v)` | (internal) nilai → hex char |
| `static int[] quote(int[] s)` | (internal) string JSON ter-escape |
| `static int[] wr(int[] v)` | (internal) tulis nilai JSON ke teks |
| `static int[] keyAt(int[] obj, int idx)` | kunci pasangan ke-i pada objek |
| `static int[] valAt(int[] obj, int idx)` | nilai pasangan ke-i pada objek |
| `static int[] write(int[] v)` | API publik: serialisasi ke teks |
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

## Log

`lib/core/Log.kop` · 4 fungsi

Logging ke stderr dengan timestamp UTC dan level.

```java
import Log;

Log.info(Net.bytes("boot"), Net.bytes("server siap"));
Log.error(Net.bytes("db"), Net.bytes("gagal konek"));
```

**Catatan**

- stderr tidak di-buffer; aman untuk supervisor.

| Fungsi | Keterangan |
|---|---|
| `static void emit(int[] level, int[] tag, int[] msg)` | (internal) tulis baris level + pesan ke stderr |
| `static void info(int[] tag, int[] msg)` | log INFO |
| `static void warn(int[] tag, int[] msg)` | log WARN |
| `static void error(int[] tag, int[] msg)` | log ERROR |

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

## MapStr

`lib/core/MapStr.kop` · 17 fungsi

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
| `static int hash(int[] k)` | (internal) hash FNV kunci |
| `static boolean same(int[] m, int slot, int[] key, int h)` | bandingkan dua nilai |
| `static int find(int[] m, int[] key)` | indeks elemen, -1 bila tidak ada |
| `static boolean has(int[] m, int[] key)` | true bila elemen/kunci ada |
| `static int get(int[] m, int[] key)` | ambil nilai |
| `static int getOr(int[] m, int[] key, int def)` | ambil nilai atau default bila tidak ada |
| `static boolean place(int[] m, int at, int[] key, int h, int v)` | (internal) tulis pasangan pada slot |
| `static boolean put(int[] m, int[] key, int v)` | simpan pasangan |
| `static boolean remove(int[] m, int[] key)` | hapus elemen |
| `static void clear(int[] m)` | kosongkan isi |
| `static boolean used(int[] m, int i)` | (internal) slot i terisi |
| `static int valAtSlot(int[] m, int i)` | (internal) nilai pada slot i |
| `static int[] keyAtSlot(int[] m, int i)` | (internal) kunci pada slot i |

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
| `static int[] sep()` | string pemisah path (/) |
| `static int[] join(int[] a, int[] b)` | gabung dua path dengan pemisah |
| `static int[] base(int[] p)` | nama file terakhir tanpa direktori |
| `static int[] dir(int[] p)` | bagian direktori (tanpa nama file) |
| `static int[] ext(int[] p)` | ekstensi tanpa titik |
| `static boolean isAbsolute(int[] p)` | true bila path absolut |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int SEP()` | byte pemisah path |

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

`lib/core/Str.kop` · 40 fungsi

Utilitas teks: perbandingan, potong/gabung, pencarian, trim, padding, dan konversi angka/boolean ke teks. Bekerja pada String maupun `int[]` byte.

```java
import Str;

String s = "  Kopi  ";
System.out.println(Str.trim(s));              // Kopi
System.out.println(Str.padLeft(Str.fromInt(7), 3, 48)); // 007
```

**Catatan**

- `length` adalah jumlah byte, bukan karakter Unicode.
- `fromDouble` maksimum 6 digit pecahan; format specifier belum ada.

| Fungsi | Keterangan |
|---|---|
| `static int len(int[] s)` | panjang byte |
| `static boolean isEmpty(int[] s)` | true bila kosong |
| `static boolean eq(int[] a, int[] b)` | true bila isi kedua teks sama |
| `static int cmp(int[] a, int[] b)` | perbandingan leksikografis (-1/0/1) |
| `static int[] copy(int[] a)` | salinan (array baru) |
| `static int[] cat(int[] a, int[] b)` | gabung dua array menjadi array baru |
| `static int[] slice(int[] s, int from, int n)` | potong `n` byte mulai `from` (di-clamp) |
| `static int at(int[] s, int i)` | byte pada indeks (0..255) |
| `static int indexOf(int[] s, int[] ndl, int from)` | indeks kemunculan pertama dari `from`, -1 bila tidak ada |
| `static int lastIndexOf(int[] s, int[] ndl)` | indeks kemunculan terakhir, -1 bila tidak ada |
| `static boolean contains(int[] s, int[] ndl)` | true bila memuat substring |
| `static boolean startsWith(int[] s, int[] p)` | true bila diawali ndl |
| `static boolean endsWith(int[] s, int[] p)` | true bila diakhiri ndl |
| `static int[] trimLeft(int[] s)` | buang whitespace di ujung kiri |
| `static int[] trimRight(int[] s)` | buang whitespace di ujung kanan |
| `static int[] trim(int[] s)` | buang spasi/tab/CR/LF di kedua ujung |
| `static int[] upper(int[] s)` | huruf besar (ASCII) |
| `static int[] lower(int[] s)` | huruf kecil (ASCII) |
| `static int[] repeat(int[] s, int n)` | ulang string n kali |
| `static int[] reverse(int[] s)` | salinan terbalik |
| `static int[] replaceByte(int[] s, int from, int to)` | ganti semua byte from → to |
| `static int[] replaceSeq(int[] s, int[] old, int[] nw)` | ganti semua kemunculan old → nw |
| `static int countByte(int[] s, int b)` | berapa kali byte b muncul |
| `static int splitCount(int[] s, int sep)` | berapa banyak potongan dengan pemisah sep |
| `static int[] splitAt(int[] s, int sep, int idx)` | potongan ke-idx dengan pemisah sep |
| `static int[] padLeft(int[] s, int width, int pad)` | tambah byte pad di kiri sampai width |
| `static int[] padRight(int[] s, int width, int pad)` | tambah byte pad di kanan sampai width |
| `static int[] fromInt(int v)` | int → teks desimal |
| `static int toInt(int[] s)` | parse desimal; 0 bila bukan angka |
| `static boolean isDigits(int[] s)` | true bila seluruh isi digit desimal |
| `static boolean equalsIgnoreCase(int[] a, int[] b)` | perbandingan isi tanpa peduli huruf besar/kecil |
| `static int utf8Count(int[] s)` | jumlah karakter Unicode (bukan byte) |
| `static int[] hex(int[] data)` | byte → teks hex lowercase |
| `static int[] boolText(boolean b)` | boolean → "true"/"false" |
| `static int[] fromUInt(int v)` | uint32 (nilai kanonik zero-extended di int) |
| `static int[] fromLong(long v)` | long → teks desimal |
| `static int[] fromULong(long v)` | uint64: divisi unsigned bit-per-bit |
| `static int[] fromDouble(double d)` | double → desimal sederhana (maks 6 digit pecahan, nol ekor dibuang). Batas jujur: bukan algoritma pembulatan Java/printf. |
| `static void print(int[] s)` | tulis byte ke stdout tanpa newline |
| `static void line(int[] s)` | tulis dengan newline |

---

## Time

`lib/core/Time.kop` · 6 fungsi

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
| `static int[] format(int sec)` | format "YYYY-MM-DD HH:MM:SS" UTC |

---
