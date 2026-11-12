# Referensi API — Crypto

Hash, PBKDF2, cipher, X25519, AEAD, DER. Semua signature di bawah diekstrak langsung dari source `lib/crypto/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Aead

`lib/crypto/Aead.kop` · 7 fungsi

AEAD ChaCha20-Poly1305 (RFC 7539 §2.8): enkripsi + autentikasi untuk TLS.

```java
import Aead;

int[] ct = Aead.seal(key, nonce, aad, plain);
```

**Catatan**

- Nonce tidak boleh dipakai ulang dengan kunci yang sama.

| Fungsi | Keterangan |
|---|---|
| `static int[] concat(int[] a, int[] b)` | gabungkan dua array |
| `static int[] pad16(int[] a)` | nol-kan sampai kelipatan 16 (RFC 7539 §2.8 pad16) |
| `static int[] le64(int n)` | panjang 64-bit little-endian (8 byte) |
| `static int[] otkPair(int[] key, int[] nonce)` | one-time poly key: 32 byte pertama blok counter-0 |
| `static int[] ctOf(int[] key, int[] nonce, int[] msg)` | ciphertext ChaCha20-Poly1305 |
| `static int[] tagOf(int[] key, int[] nonce, int[] aad, int[] ct)` | tag Poly1305 atas ciphertext + aad |
| `static int eq(int[] a, int[] b)` | perbandingan konstan dua array |

---

## Big

`lib/crypto/Big.kop` · 20 fungsi

Aritmetika medan 2²⁵⁵−19 dengan 17 limb untuk X25519.

```java
import Big;

int[] f = Big.fromBytes(bytes32);
```

**Catatan**

- Dipakai internal TLS/X25519; bukan big-integer umum.

| Fungsi | Keterangan |
|---|---|
| `static int[] pans()` | konstanta p = 2^255 - 19, 17 limb radiks 15-bit (limb0 = (-19 mod 2^15) = 32749 — silang-vektor dari python) |
| `static int[] zero()` | elemen medan nol |
| `static int[] ten(int n)` | (internal) 10^n dalam medan |
| `static int[] bigadd(int[] x, int[] y)` | kedua 17 limb; mengembalikan raw (limb bisa > 2^15 sedikit) |
| `static int[] bigsubRaw(int[] x, int[] y)` | (internal) pengurangan limb mentah |
| `static int cmp(int[] x, int[] y)` | bandingkan dua magnitudo (semua limb < 2^15): 1 x>y, -1 x<y, 0 sama |
| `static void reduce(int[] v)` | car + cap limb (nilai invarian < 2^256), lalu csub 2x |
| `static void carPass(int[] v)` | (internal) normalisasi carry |
| `static void csub2(int[] v)` | (internal) kurangi dua kali (barrett kecil) |
| `static void copy(int[] s, int[] d)` | salinan (array baru) |
| `static int[] femul(int[] x, int[] y)` | perkalian medan penuh: 17x17 limbs → 34 → fold 2^255≡19 → reduce |
| `static int[] fsq(int[] x)` | kuadrat dalam medan |
| `static int[] feadd(int[] x, int[] y)` | (x + y) mod p |
| `static int[] fesub(int[] x, int[] y)` | (x - y) mod p: jalan (x + 2p) - y (y < p) |
| `static void cswap17(int[] pair, int bx, int by, int swap)` | kondisional swap constant-time: 17 limb pada dua basis eksplisit |
| `static int bitOf(int[] v, int t)` | inventory bit de eksponen 17-limb |
| `static int[] feInv(int[] x)` | inverse via Fermat: x^(p-2) |
| `static int[] fromBytes(int[] b)` | byte-as-int little-endian 32 → elemen medan (bit 255 tergeser) |
| `static int[] toBytes(int[] x)` | elemen medan (< p) → 32 byte little-endian |
| `static int[] scalarFromBytes(int[] b)` | scalar dari 32 byte, clamped (RFC 7748 §5): k0&=248 k31&=127 /64 |

---

## ChaCha

`lib/crypto/ChaCha.kop` · 5 fungsi

ChaCha20 stream cipher (RFC 7539) untuk TLS record dan eksperimen.

```java
import ChaCha;

int[] ks = ChaCha.block(key, counter, nonce);
```

| Fungsi | Keterangan |
|---|---|
| `static int rotl(int x, int n)` | (internal) rotasi kiri n bit |
| `static void qr(int[] s, int a, int b, int c, int d)` | quarter round: operasi pada referensi array? Kopi tak punya pointer — inline 4-kata lewat indeks state s + offset 0/4/8/12 |
| `static void roundTo(int[] x)` | (internal) satu ronde ChaCha pada state |
| `static int[] block(int[] init)` | satu blok: state inisial (16 kata) → keystream 64 byte |
| `static int[] xctr(int[] key, int[] nonce, int counter, int[] data)` | en/de-kript (keystream XOR): key 32 byte, nonce 12 byte, counter awal, data byte-as-int |

---

## Cipher

`lib/crypto/Cipher.kop` · 11 fungsi

Base64, hex, dan cipher klasik (RC4, Vigenère, XOR) untuk eksperimen/kompatibilitas.

```java
import Cipher;

String b = Cipher.b64enc(Net.bytes("halo"));
System.out.println(b);                           // aGFsbw==
System.out.println(Net.mkstr(Cipher.b64dec(b))); // halo
```

**Catatan**

- Cipher klasik **bukan** untuk keamanan produksi.

| Fungsi | Keterangan |
|---|---|
| `static int[] rc4(int[] data, int[] key)` | enc/dec bisa: fungsi sama (xor nirl). kunci: array byte-as-int (praktik umum: kunci 5..16 byte). hasil: array baru sepanjang data. |
| `static int[] vigenere(int[] txt, int[] key)` | enkripsi Vigenère (ASCII) |
| `static int[] unvigenere(int[] txt, int[] key)` | dekripsi Vigenère |
| `static String b64enc(int[] data)` | data byte-as-int → teks base64 (String), padding '=' |
| `static int[] b64dec(String txt)` | teks base64 → byte-as-int; '=' diabaikan, karakter asing → 0 |
| `static String hexenc(int[] data)` | data → hex lowercase (String), 2 karakter per byte |
| `static int hexdig(int n)` | 0..15 → '0'..'9','a'..'f' |
| `static int hexval(int c)` | nilai hex digit ASCII → 0..15, -1 bila bukan digit hex |
| `static int[] hexdec(String txt)` | teks hex (panjang ganjil dibatasi 0 di akhir) → byte-as-int |
| `static int crc16(int[] data)` | CRC-16-CCITT (poly 0x1021, init 0xFFFF) — vektor terkenal: "123456789" → 0x29B1 |
| `static int adler32(int[] data)` | Adler-32 (RFC 1950): A=1, B=0, mod 65521 dan b·65536+a. Untuk masukan besar b bisa melewati 2^31 — cukup untuk deret kecil |

---

## Crypto

`lib/crypto/Crypto.kop` · 18 fungsi

Kumpulan utilitas bit/hash/cipher lama yang dipakai modul kripto lain (kompatibilitas v1).

```java
import Crypto;

int h = Crypto.hashInt(123);   // hash integer deterministik
```

**Catatan**

- Untuk kode baru pakai `Hash`/`Cipher` yang lebih jelas.

| Fungsi | Keterangan |
|---|---|
| `static int bitxor(int a, int b)` | xor bit-demi-bit untuk nilai NONNEGATIF < 2^31 |
| `static int arrEq(int[] a, int[] b)` | banding dua array: 1 jika sama panjang & tiap elemen sama |
| `static int abs_(int x)` | (internal) nilai absolut |
| `static int fnv1a(int[] data, int len)` | FNV-1a 32-bit darider array byte-as-int (len elemen pertama) |
| `static int nextRand(int x)` | Lehmer/LCG (Schrage) — perkiraan berpisah murni int, seed/mulai 1..2^31-2, hasil [0, M) |
| `static int randRange(int seed, int hi)` | (internal) acak LCG 0..hi-1 (bukan kripto) |
| `static int bitand(int a, int b)` | identitas aritmétika: A + B = (A⊕B) + 2·(A∧B) ⟹ ia juga A∧B = (A+B−A⊕B)/2 dan A∨B = A⊕B + A∧B — tanpa operator & / ^ |
| `static int bitor(int a, int b)` | (internal) bitwise OR |
| `static int bitnot(int x)` | komplemen 31-bit (pelengkap di dalam 0..2^31-1) |
| `static int pow31(int n)` | 2^n untuk n 0..30 (n=31 membanjir tanda) |
| `static int mask31(int n)` | masker n bit terendah |
| `static int shl31(int x, int n)` | geser kiri, bit di atas 2^31 dibuang |
| `static int shr31(int x, int n)` | geser kanan logis (masukan positif) |
| `static int rotl31(int x, int n)` | rotasi kiri 31-bit (bit paling atas mengalirlah ke bawah) |
| `static int rotr31(int x, int n)` | rotasi kanan |
| `static int[] xorStream(int[] inp, int[] key)` | XOR stream: out[i] = in[i] ^ key[i % key.length] XOR nirl: pemanggilan kedua dengan key yang sama = dekripsi. |
| `static int[] caesar(int[] inp, int k)` | Caesar shift (+k mod 256) — disetara beserta 'uncaesar' |
| `static int[] uncaesar(int[] inp, int k)` | dekripsi Caesar (ASCII) |

---

## Der

`lib/crypto/Der.kop` · 4 fungsi

Parser ASN.1 DER minimal untuk mengekstrak SubjectPublicKeyInfo (pin TLS).

```java
import Der;

// dipakai Tls: pin SPKI = SHA-256 dari SubjectPublicKeyInfo
```

**Catatan**

- Hanya struktur DER yang diperlukan TLS; bukan parser X.509 lengkap.

| Fungsi | Keterangan |
|---|---|
| `static void tlv(int[] x, int pos, int[] out)` | baca satu TLV dari pos. tulis hasil ke out (4 slot): out[0] = tag (atau -1 jika rusak), out[1] = panjang isi, out[2] = posisi isi, out[3] = posisi berikutnya. |
| `static int[] sliceOf(int[] x, int from, int len)` | (internal) potong array |
| `static int[] spkiOf(int[] x)` | SPKI (DER utuh) dari sertifikat leaf. kosong (panjang 0) bila gagal. |
| `static int[] spkiPin(int[] cert)` | fingerprint pin: SHA-256 dari DER SPKI |

---

## Hash

`lib/crypto/Hash.kop` · 11 fungsi

SHA-1/256/384/512 dan HMAC-SHA256/SHA512 murni Kopi untuk integritas, JWT, PBKDF2, dan TLS.

```java
import Hash;
import Cipher;

System.out.println(Cipher.hexenc(Hash.sha256(Net.bytes("abc"))));
System.out.println(Cipher.hexenc(Hash.sha512(Net.bytes("abc"))));
System.out.println(Cipher.hexenc(Hash.sha1(Net.bytes("abc"))));
```

**Catatan**

- Semua varian bekerja one-shot; pesan besar memakai memori O(panjang).
- `hmac(key,msg)` mengembalikan 32 byte; `hmacSha512` 64 byte.
- SHA-512/384 memakai 80 ronde, blok 128 byte.

| Fungsi | Keterangan |
|---|---|
| `static int uadd(int x, int y)` | penjumlahan modulo 2^32 |
| `static int rotr(int x, int n)` | rotasi kanan 32-bit; count 0 aman |
| `static int[] sha256(int[] msg)` | satu-shot: pesan byte-as-int (0..255) → digest 32 byte-as-int |
| `static int[] hmac(int[] key, int[] msg)` | HMAC-SHA256 (RFC 2104); key > 64 byte di-hash dulu |
| `static int[] sha512(int[] msg)` | SHA-512 (FIPS 180-4): pesan byte-as-int → digest 64 byte |
| `static int[] sha384(int[] msg)` | SHA-384 (FIPS 180-4): pesan byte-as-int → digest 48 byte |
| `static int[] hmacSha512(int[] key, int[] msg)` | HMAC-SHA512 (RFC 2104); kunci > 128 byte di-hash dulu |
| `static int[] toHex(int[] dig)` | digest → teks hex lowercase |
| `static int eq(int[] a, int[] b)` | perbandingan konstan dua digest |
| `static int rotl(int x, int n)` | (internal) rotasi kiri n bit |
| `static int[] sha1(int[] msg)` | satu-shot SHA-1: pesan byte-as-int → digest 20 byte-as-int |

---

## Kdf

`lib/crypto/Kdf.kop` · 8 fungsi

HKDF-SHA256 (RFC 5869) dan ExpandLabel TLS 1.3 (RFC 8446 §7.1).

```java
import Kdf;

int[] okm = Kdf.hkdf(secret, salt, info, 32);
```

**Catatan**

- Dipakai TLS; untuk password gunakan `Pbkdf2`.

| Fungsi | Keterangan |
|---|---|
| `static int[] extract(int[] salt, int[] ikm)` | HKDF-Extract: PRK = HMAC(salt, IKM) |
| `static int[] expand(int[] prk, int[] info, int len)` | HKDF-Expand: OKM = T(1) / T(2) / ... (L byte) T(i) = HMAC(PRK, T(i-1) / info / i) dengan i byte tunggal |
| `static int[] concat(int[] a, int[] b)` | gabungkan dua array |
| `static int[] concat3(int[] a, int[] b, int[] c)` | (internal) gabung tiga array |
| `static int[] u16(int v)` | uint16 big-endian dua byte |
| `static int[] tls13()` | "tls13 " — ukuran 6 byte tetap |
| `static int[] expandLabel(int[] secret, int[] label, int[] context, int len)` | HKDF-Expand-Label(Secret, Label, Context, Length): HkdfLabel = uint16(Length) / uint8(len("tls13 " + Label)) / "tls13 " / Label / uint8(Context.len) / Context (RFC 8446 §7.1) |
| `static int[] deriveSecret(int[] secret, int[] label, int[] msgs)` | Derive-Secret(Secret, Label, Messages) = ExpandLabel(Secret, Label, SHA256(messages), 32) |

---

## Pbkdf2

`lib/crypto/Pbkdf2.kop` · 1 fungsi

PBKDF2-HMAC-SHA256 untuk menyimpan kata sandi dengan iterasi tinggi.

```java
import Pbkdf2;
import Rand;

int[] salt = Rand.bytes(16);
int[] hash = Pbkdf2.sha256(pass, salt, 100000, 32);
```

**Catatan**

- Buffer sementara di-reset internal; aman untuk 100k+ iterasi.
- Simpan salt + hash; verifikasi dengan perbandingan konstan.

| Fungsi | Keterangan |
|---|---|
| `static int[] sha256(int[] pass, int[] salt, int iter, int dkLen)` | derive key PBKDF2-HMAC-SHA256 (pass, salt, iterasi, panjang) |

---

## Poly

`lib/crypto/Poly.kop` · 10 fungsi

Poly1305 one-time MAC (RFC 7539) dengan aritmetika limb 15-bit.

```java
import Poly;

int[] tag = Poly.mac(msg, key);
```

| Fungsi | Keterangan |
|---|---|
| `static int[] plimbs()` | (internal) state limb awal |
| `static int[] maskLE()` | (internal) mask limb 130-bit |
| `static void addByte(int[] v, int pos, int val)` | tambah byte val pada posisi bit 8·pos (pos = indeks byte blok) |
| `static void carPass(int[] v)` | cari-angkut 9 slot; sisa ke slot 9 (kecil, tetap dipantau) |
| `static void fold9(int[] v)` | kuras slot 9 (koefisien 2^135): 2^135 ≡ 160 (mod p), jadi lipat v[9]·160 ke slot 0/1 lalu angkut ulang. Sisa carry bisa balik ke slot 9, ulangi sampai benar-benar nol (pasti cep… |
| `static void foldAll(int[] v)` | lipat slot 9..len-1: 2^(15k) → 160·2^(15(k-9)); k dari besar ke kecil agar k contributions hi dari slot besar ikut terserap di pass ini |
| `static void mulFold(int[] acc, int[] rsl)` | (acc + blok)·r: schoolbook 9×9 → 18 slot, lalu fold + carPass 3 kali. acc[0..8] masuk ternormalisasi (< 2^15); keluar < 2^135, slot 9 ~0 |
| `static int cmpP(int[] v)` | v ternormalisasi (slot < 2^15, slot 9 ≈ 0): bandingkan dengan p |
| `static void subP(int[] v)` | (internal) kurangi p bila >= p |
| `static int[] mac(int[] r, int[] s, int[] msg)` | MAC: r 16 byte, s 16 byte, msg → tag 16 byte |

---

## Rand

`lib/crypto/Rand.kop` · 1 fungsi

Byte acak kriptografis dari `/dev/urandom`.

```java
import Rand;

int[] k = Rand.bytes(32);   // kunci 256-bit
```

| Fungsi | Keterangan |
|---|---|
| `static int[] bytes(int n)` | n byte acak dari /dev/urandom |

---

## X25519

`lib/crypto/X25519.kop` · 2 fungsi

Pertukaran kunci X25519 (RFC 7748) untuk TLS 1.3.

```java
import X25519;

int[] shared = X25519.shared(mySecret, peerPublic);
```

| Fungsi | Keterangan |
|---|---|
| `static int[] x25519(int[] k32, int[] u32)` | X25519(k, u) — kedua arg byte-as-int 32 (scalar, u-coord) |
| `static int[] pub(int[] sk)` | kunci publik: u = 9 (little-endian: byte[0] = 9) |

---
