# Referensi API — System

Argumen, environment, waktu, log, UUID, channel, dan mutex. Semua signature di bawah diekstrak langsung dari source `lib/system/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Args

`lib/system/Args.kop` · 10 fungsi

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

## Chan

`lib/system/Chan.kop` · 9 fungsi

Channel CSP antar-thread: payload `long` atau closure `fn`, dengan buffer melingkar dan penunggu condvar.

```java
import Chan;

Chan q = new Chan(16);
q.sendFn(() => { System.out.println("tugas"); });
fn job = q.recvFn();
if (!q.isClosed()) job();
q.close();
```

**Catatan**

- `send`/`recv` untuk `long`; `sendFn`/`recvFn` untuk closure (antrean executor).
- `recv` mengembalikan 0 setelah channel ditutup dan kosong; pakai `recvOk`/`recvFnOk` agar nilai terakhir tidak hilang.
- `recvOk`/`recvFnOk` mengisi `ok[0] = 1` bila nilai diterima, `0` bila channel tutup/kosong.
- `close` membangunkan semua penunggu; `send` setelah tutup diabaikan.

| Fungsi | Keterangan |
|---|---|
| `void send(long v)` | kirim nilai `long` (memblokir bila penuh) |
| `long recv()` | terima nilai `long` (memblokir; 0 bila tutup/kosong) |
| `long recvOk(int[] ok)` | terima `long`; `ok[0]` = 1 bila dapat nilai |
| `void sendFn(fn f)` | kirim closure (antrean tugas) |
| `fn recvFn()` | terima closure (closure kosong bila tutup/kosong) |
| `fn recvFnOk(int[] ok)` | terima closure; `ok[0]` = 1 bila dapat closure |
| `void close()` | tutup channel dan bangunkan semua penunggu |
| `int len()` | jumlah nilai yang sedang di buffer |
| `boolean isClosed()` | true bila channel sudah ditutup |

---

## Env

`lib/system/Env.kop` · 11 fungsi

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

## Log

`lib/system/Log.kop` · 18 fungsi

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
| `static int DEBUG = 0` | — |
| `static int INFO = 1` | — |
| `static int WARN = 2` | — |
| `static int ERROR = 3` | — |
| `static int OFF = 4` | — |

---

## Mutex

`lib/system/Mutex.kop` · 3 fungsi

Kunci mutual-exclusion antar-thread untuk melindungi data bersama.

```java
import Mutex;

Mutex m = new Mutex();
m.lock();
// ... akses data bersama ...
m.unlock();
```

**Catatan**

- `tryLock` tidak memblokir: true bila kunci berhasil diambil.
- Selalu `unlock` pada jalur yang sama; tidak ada RAII/finalizer.

| Fungsi | Keterangan |
|---|---|
| `void lock()` | ambil kunci (memblokir sampai tersedia) |
| `void unlock()` | lepas kunci |
| `boolean tryLock()` | coba ambil kunci tanpa menunggu; true bila berhasil |

---

## Time

`lib/system/Time.kop` · 32 fungsi

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

`lib/system/Uuid.kop` · 6 fungsi

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
