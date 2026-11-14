# Referensi API — Compress

LZMA/LZMA2/XZ, checksum, filter, dan kompresi umum. Semua signature di bawah diekstrak langsung dari source `lib/compress/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Checksum

`lib/compress/Checksum.kop` · 11 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int crc32Init()` | ══ CRC-32 (polinom 0xEDB88320, refleksi) ═════════════════════ |
| `static int crc32Update(int crc, int[] a, int off, int len)` | — |
| `static int crc32Final(int crc)` | — |
| `static int crc32(int[] a)` | — |
| `static long crc64Init()` | ══ CRC-64/XZ (ECMA-182, polinom refleksi 0xC96C5795D7870F42) ═ |
| `static long crc64Update(long crc, int[] a, int off, int len)` | — |
| `static long crc64Final(long crc)` | — |
| `static long crc64(int[] a)` | — |
| `static int vliSize(int[] a, int off)` | ══ VLI (variable-length integer) — format XZ ═════════════════ Setiap byte menyumbang 7 bit (little-endian); bit 0x80 menandakan masih ada byte lanjutan. |
| `static long vliDecode(int[] a, int off)` | — |
| `static int[] vliEncode(long v)` | — |

---

## Compress

`lib/compress/Compress.kop` · 29 fungsi

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

## Filter

`lib/compress/Filter.kop` · 27 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] copy(int[] a)` | salinan (array baru) |
| `static boolean msByte(int b)` | — |
| `static int[] deltaEncode(int[] a, int dist)` | ══ delta ═════════════════════════════════════════════════════ |
| `static int[] deltaDecode(int[] a, int dist)` | — |
| `static int[] x86Encode(int[] a, int start)` | ══ x86 BCJ (E8/E9 rel32) ═════════════════════════════════════ |
| `static int[] x86Decode(int[] a, int start)` | — |
| `static int[] x86(int[] src, int start, int enc)` | — |
| `static int[] armEncode(int[] a, int start)` | ══ ARM (BL, 24-bit rel) ══════════════════════════════════════ |
| `static int[] armDecode(int[] a, int start)` | — |
| `static int[] arm(int[] src, int start, int enc)` | — |
| `static int[] armthumbEncode(int[] a, int start)` | ══ ARM-Thumb (BL pair) ══════════════════════════════════════ |
| `static int[] armthumbDecode(int[] a, int start)` | — |
| `static int[] armthumb(int[] src, int start, int enc)` | — |
| `static int[] powerpcEncode(int[] a, int start)` | ══ PowerPC (b/BL, 24-bit) ═══════════════════════════════════ |
| `static int[] powerpcDecode(int[] a, int start)` | — |
| `static int[] powerpc(int[] src, int start, int enc)` | — |
| `static int[] ia64Encode(int[] a, int start)` | ══ IA-64 (bundles, branch slot) ═════════════════════════════ |
| `static int[] ia64Decode(int[] a, int start)` | — |
| `static int[] ia64(int[] src, int start, int enc)` | — |
| `static int[] sparcEncode(int[] a, int start)` | ══ SPARC (call, 30-bit) ═════════════════════════════════════ |
| `static int[] sparcDecode(int[] a, int start)` | — |
| `static int[] sparc(int[] src, int start, int enc)` | — |
| `static int[] arm64Encode(int[] a, int start)` | ══ ARM64 (BL + ADRP) ════════════════════════════════════════ |
| `static int[] arm64Decode(int[] a, int start)` | — |
| `static int[] arm64(int[] src, int start, int enc)` | — |
| `static int[] encode(int id, int[] a, int arg)` | ══ dispatcher ═══════════════════════════════════════════════ id = filter ID XZ arg = jarak delta (1..256) untuk 0x03, startOffset untuk BCJ |
| `static int[] decode(int id, int[] a, int arg)` | — |

---

## LzmaDec

`lib/compress/Lzma.kop` · 22 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] mk(int n)` | — |
| `static void fill(int[] p)` | — |
| `static boolean ult(int a, int b)` | ══ range decoder (static, state eksplisit) ═══════════════════ |
| `static int rInit(int[] b, int off, int[] r)` | — |
| `static int rNorm(int[] b, int[] r)` | — |
| `static int rBit(int[] b, int[] r, int[] probs, int idx)` | — |
| `static int rDirect(int[] b, int[] r, int n)` | — |
| `static int treeDec(int[] b, int[] r, int[] probs, int base, int nbits)` | — |
| `static int[] mk(int n)` | — |
| `static int[] neg(int n)` | — |
| `static int[] raw(int[] src, int props, int dictSize)` | — |
| `static int[] chunk(int[] src, int props, int dictSize)` | — |
| `static int[] decompress(int[] data)` | ══ API publik ════════════════════════════════════════════════ |
| `static int presetDict(int preset)` | ══ preset (kompatibel dengan skala 0–9 Python/liblzma) ════════ |
| `static int[] compressPreset(int[] data, int preset)` | LZMA alone memakai lc=3, lp=0, pb=2 seperti preset Python. |
| `static int[] compressRawPreset(int[] data, int preset)` | LZMA1 raw dengan end marker, dict mengikuti preset. |
| `static int[] decompressRaw(int[] comp, int props, int outSize)` | — |
| `static int propsOf(int lc, int lp, int pb)` | — |
| `static int[] compressRaw(int[] data, int props, int dictSize)` | LZMA1 raw (tanpa header) dengan end marker. |
| `static int[] compressWith(int[] data, int lc, int lp, int pb, int dictSize)` | LZMA alone (.lzma): header 13 byte + raw; uncompressed size = -1. |
| `static int[] compress(int[] data)` | Default: lc=3, lp=0, pb=2, dict=64 KiB (sama dengan vektor raw). |
| `static String decompressText(int[] data)` | — |

---

## Lzma2

`lib/compress/Lzma2.kop` · 15 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] decompress(int[] data)` | — |
| `static int[] decode(int[] data, int[] status)` | — |
| `static int streamSizeStatus(int[] data, int off, int[] state)` | Panjang stream LZMA2 (termasuk control 0x00), -1 bila rusak/terpotong; state[0] = -1 rusak, -2 terpotong. |
| `static int streamSize(int[] data, int off)` | — |
| `static int maxChunkSize()` | ══ encoder ═══════════════════════════════════════════════════ Chunk adaptif: mulai dari 1 MiB; bila hasil LZMA melebihi batas csize LZMA2 (64 KiB), chunk diperkecil bertahap. S… |
| `static int minChunkSize()` | — |
| `static int[] slice(int[] data, int off, int n)` | — |
| `static void emitComp(int[] out, int[] p, int[] part, int[] comp, int props)` | — |
| `static void emitRaw(int[] out, int[] p, int[] part)` | — |
| `static int encAdaptive(int[] out, int[] p, int[] data, int off, int avail, int props, int dictSize)` | Enkode satu chunk dari data[off..off+avail); kembalikan panjang chunk yang dipakai (bisa lebih kecil dari avail). |
| `static int[] compress(int[] data)` | — |
| `static int[] compressPreset(int[] data, int preset)` | — |
| `static int[] compressWith(int[] data, int lc, int lp, int pb, int dictSize)` | — |
| `static int[] decodeFrom(int[] data, int off, int[] status)` | — |
| `static Lzma2EncStream create()` | buat/buka file |

---

## LzmaAuto

`lib/compress/LzmaAuto.kop` · 3 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static boolean isXz(int[] data)` | — |
| `static int[] decompress(int[] data)` | — |
| `static int[] decode(int[] data, int[] status)` | — |

---

## LzmaStream

`lib/compress/LzmaStream.kop` · 15 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static boolean isCheckSupported(int check)` | liblzma mendukung keempat check ini; nilai lain tidak didukung. |
| `static LzmaCompressor create()` | Default Python: FORMAT_XZ, check -1 (liblzma memakai CRC64), preset 6. |
| `static LzmaCompressor xz(int check0, int preset0)` | — |
| `static LzmaCompressor alone(int preset0)` | — |
| `static LzmaCompressor raw(int dict0)` | Raw LZMA2 dengan ukuran kamus eksplisit. |
| `static LzmaCompressor chain(int[] fids, int[] fargs, int dict0, int check0)` | XZ dengan filter chain pra-LZMA2 (id/arg seperti Xz.compressChain). |
| `static LzmaDecompressor create()` | buat/buka file |
| `static LzmaDecompressor xz()` | — |
| `static LzmaDecompressor alone()` | — |
| `static LzmaDecompressor raw()` | — |
| `static boolean exists(String path)` | — |
| `static int[] read(String path)` | Membaca seluruh berkas; array kosong bila gagal atau berkas kosong. |
| `static boolean write(String path, int[] data)` | — |
| `static boolean compress(String src, String dst, int preset)` | Kompres berkas src (XZ, CRC32) ke dst. |
| `static boolean decompress(String src, String dst)` | Dekompres .xz atau LZMA alone dari src ke dst. |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int FORMAT_AUTO = 0` | Nilai konstanta sama dengan modul lzma Python. |
| `static int FORMAT_XZ = 1` | — |
| `static int FORMAT_ALONE = 2` | — |
| `static int FORMAT_RAW = 3` | — |
| `static int CHECK_NONE = 0` | — |
| `static int CHECK_CRC32 = 1` | — |
| `static int CHECK_CRC64 = 4` | — |
| `static int CHECK_SHA256 = 10` | — |
| `static long FILTER_LZMA1 = 4611686018427387905L` | ID filter Python; sama dengan ID XZ kecuali LZMA1. |
| `static int FILTER_LZMA2 = 0x21` | — |
| `static int FILTER_DELTA = 0x03` | — |
| `static int FILTER_X86 = 0x04` | — |
| `static int FILTER_POWERPC = 0x05` | — |
| `static int FILTER_IA64 = 0x06` | — |
| `static int FILTER_ARM = 0x07` | — |
| `static int FILTER_ARMTHUMB = 0x08` | — |
| `static int FILTER_SPARC = 0x09` | — |
| `static int FILTER_ARM64 = 0x0A` | — |

---

## Xz

`lib/compress/Xz.kop` · 21 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static boolean isXz(int[] data)` | — |
| `static boolean isXzAt(int[] data, int p)` | — |
| `static int get32le(int[] a, int p)` | — |
| `static long get64le(int[] a, int p)` | — |
| `static int[] append(int[] a, int[] b)` | buka untuk menambah di akhir |
| `static int checkLen(int type)` | — |
| `static int verifyCheck(int type, int[] a, int off, int[] data)` | verifikasi check; return 0 cocok, -3 tidak, -1 tipe tak didukung |
| `static void put(int[] out, int[] p, int v)` | ══ encoder .xz (satu block, filter LZMA2) ════════════════════ |
| `static void put32le(int[] out, int[] p, int v)` | — |
| `static void putVli(int[] out, int[] p, long v)` | — |
| `static int[] compress(int[] data)` | — |
| `static int dictProp(int dict)` | Nilai properti filter LZMA2 untuk ukuran kamus (XZ spec). |
| `static int[] compressPreset(int[] data, int preset)` | — |
| `static int[] compressPresetCheck(int[] data, int preset, int checkType)` | — |
| `static int[] compressDelta(int[] data, int dist, int checkType)` | Satu filter tambahan: delta (id 0x03, arg jarak 1..256). |
| `static int[] compressBcj(int[] data, int id, int startOffset, int checkType)` | Satu filter BCJ (id 0x04..0x0A, arg startOffset). |
| `static int[] compressChain(int[] data, int checkType, int[] ids, int[] args, int dictSize)` | Filter chain: `ids`/`args` adalah filter pra-LZMA2 (data-first), LZMA2 selalu ditambahkan sebagai filter terakhir. checkType: 0 None, 1 CRC32, 4 CRC64, 10 SHA-256. |
| `static int[] compressCheck(int[] data, int checkType)` | — |
| `static int[] decompress(int[] data)` | — |
| `static int[] decodeStream(int[] data, int start, int[] state)` | decode satu stream mulai start; state[0] = posisi akhir, state[1] = status. |
| `static int[] decode(int[] data, int[] status)` | — |

---
