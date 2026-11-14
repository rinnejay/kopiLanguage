# Referensi API — Data

JSON, CBOR, MessagePack, CSV, konfigurasi, cookie. Semua signature di bawah diekstrak langsung dari source `lib/data/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Cbor

`lib/data/Cbor.kop` · 11 fungsi

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

## Config

`lib/data/Config.kop` · 9 fungsi

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

`lib/data/Cookie.kop` · 16 fungsi

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

`lib/data/Csv.kop` · 5 fungsi

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

## Json

`lib/data/Json.kop` · 72 fungsi

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
| `static int NULLV = 0` | tipe: null |
| `static int BOOLV = 1` | tipe: boolean |
| `static int NUMV = 2` | tipe: angka |
| `static int STRV = 3` | tipe: string |
| `static int ARRV = 4` | tipe: array |
| `static int OBJV = 5` | tipe: objek |

---

## MessagePack

`lib/data/MessagePack.kop` · 11 fungsi

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
