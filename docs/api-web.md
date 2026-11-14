# Referensi API — Web

HTTP/1.1, HTTP/2, WebSocket, URL, auth, sesi. Semua signature di bawah diekstrak langsung dari source `lib/web/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Auth

`lib/web/Auth.kop` · 8 fungsi

Parser header Authorization: skema, Basic (user:pass), Bearer, tantangan 401.

```java
import Auth;

String h = HttpServer.headerValue(req, "Authorization");
if (!Auth.checkBasic(h, user, pass)) { /* 401 */ }
```

| Fungsi | Keterangan |
|---|---|
| `static int spaceAt(String v)` | (internal) posisi spasi pertama di header |
| `static String scheme(String v)` | skema URL |
| `static String token(String v)` | bagian token setelah skema |
| `static boolean same(String a, String b)` | bandingkan dua nilai |
| `static String basicRaw(String v)` | hasil "user:pass" dari Basic, kosong bila bukan Basic |
| `static boolean checkBasic(String v, String user, String pass)` | true bila Basic cocok user:pass |
| `static String bearer(String v)` | token Bearer |
| `static String challenge(String realm)` | baris header tantangan untuk 401 |

---

## Http

`lib/web/Http.kop` · 44 fungsi

Klien HTTP/1.1: GET/POST, header kustom, chunked, redirect, basic auth, timeout 5 detik.

```java
import Tcp;
import Http;

int[] r = Http.getFull(Tcp.loopback(), 8080, "127.0.0.1", "/health", 8192);
System.out.println(Http.statusOf(r));
System.out.println(Http.payload(r));
```

**Catatan**

- API lama (`getInto`, `statusOf`, `bodyOf`) tetap tersedia.
- Redirect hanya diikuti bila host sama.

| Fungsi | Keterangan |
|---|---|
| `static String req(String host, String path)` | susun baris permintaan GET lengkap: "GET <path> HTTP/1.0\r\nHost: <host>\r\n\r\n" |
| `static int statusOf(int[] resp)` | kode status HTTP dari baris pertama ("HTTP/1.0 200 OK"), -1 bila tidak cocol dengan format baris status |
| `static int[] bodyOf(int[] resp)` | isi (body) respons: semua byte setelah pemisah CRLF CRLF. tanpa pemisah (jawaban tak utuh) → seluruh respons dibalikkan. |
| `static int getInto(int addr, int port, String host, String path, int[] buf)` | kirim GET dan muat SELURUH respons (header + body) ke buf. sukses → total byte respons (≥ 0); gagal → kode negatif. buf terlalu kecil → respons terpotong (batas v1 tercatat). |
| `static int postInto(int addr, int port, String host, String path, int[] body, int[] buf)` | kirim POST dengan body (Content-Length dari Tcp.num), muat SELURUH respons ke buf — semantik sama dengan getInto. |
| `static int[] body(int addr, int port, String host, String path, int cap)` | kenyamanan: GET lalu langsung ke body. galat ditandai array hasil [0] = kode negatif (body tidak mungkin negatif). |
| `static String reqEx(String method, String host, String path, String headers, int[] body)` | kirim permintaan generik dengan header tambahan. `headers` berisi baris mentah "Nama: nilai\r\n" (boleh kosong). body kosong (length 0) → tanpa Content-Length (GET/HEAD). HTTP/1… |
| `static int requestInto(int addr, int port, String method, String host, String path, String headers, int[] body, int[] buf)` | satu putaran penuh dengan method/header/body bebas. |
| `static int getWith(int addr, int port, String host, String path, String headers, int[] buf)` | GET dengan header tambahan |
| `static int postWith(int addr, int port, String host, String path, String headers, int[] body, int[] buf)` | POST dengan header tambahan + body |
| `static int bodyStart(int[] resp)` | indeks awal body (setelah CRLF CRLF), -1 bila tidak ada |
| `static int[] headerBlock(int[] resp)` | potongan header mentah (tanpa baris status & tanpa CRLF CRLF) |
| `static int lowerc(int c)` | (internal) huruf kecil ASCII |
| `static boolean nameEq(String line, int at, String name)` | (internal) nama header cocok pada posisi |
| `static String headerValue(int[] resp, String name)` | nilai header (tanpa spasi tepi), array kosong bila tidak ada. perbandingan nama case-insensitive; baris pertama yang cocok dipakai. |
| `static boolean hasHeader(int[] resp, String name)` | true bila respons punya header name |
| `static int hexVal(int c)` | ═══ v1.9.2: HTTP/1.1 — chunked, redirect, form, auth ═══════ |
| `static boolean hasToken(int[] resp, String name, String tok)` | cari token (case-insensitive) di nilai header |
| `static int[] dechunk(int[] raw)` | decode Transfer-Encoding: chunked (hex-size CRLF data CRLF ... 0) |
| `static int[] payload(int[] resp)` | body sebenarnya: chunked → decode; Content-Length → potong |
| `static int[] payloadDecoded(int[] resp)` | body setelah dekompresi Content-Encoding (gzip/deflate) |
| `static String req11(String method, String hostHdr, String path, String headers, int[] body, int keep)` | susun permintaan HTTP/1.1 (keep=1 → keep-alive, else close) |
| `static int[] exchangeDo(int fd, String method, String hostHdr, String path, String headers, int[] body, int cap, int timeoutMs)` | satu transaksi (core); timeoutMs > 0 memasang SO_RCVTIMEO/SO_SNDTIMEO |
| `static int[] exchange(int addr, int port, String method, String hostHdr, String path, String headers, int[] body, int cap)` | satu transaksi; timeoutMs > 0 memasang SO_RCVTIMEO/SO_SNDTIMEO |
| `static int[] getFull(int addr, int port, String hostHdr, String path, int cap)` | GET + respons lengkap (status, header, body) |
| `static int[] getFullT(int addr, int port, String hostHdr, String path, int cap, int timeoutMs)` | — |
| `static int[] followT(int addr, int port, String hostHdr, String path, int maxHops, int cap, int timeoutMs)` | ikuti 301/302/303/307/308 (maks maxHops, host harus sama) |
| `static int[] follow(int addr, int port, String hostHdr, String path, int maxHops, int cap)` | — |
| `static String basic(String user, String pass)` | header "Authorization: Basic ..." dari user:pass |
| `static int[] formPostT(int addr, int port, String hostHdr, String path, String form, int cap, int timeoutMs)` | — |
| `static int[] formPost(int addr, int port, String hostHdr, String path, String form, int cap)` | POST application/x-www-form-urlencoded |
| `static int[] jsonPostT(int addr, int port, String hostHdr, String path, String jsonBody, int cap, int timeoutMs)` | — |
| `static int[] jsonPost(int addr, int port, String hostHdr, String path, String jsonBody, int cap)` | POST application/json |
| `static String[] headerLines(int[] resp)` | — |
| `static int headerCount(int[] resp)` | — |
| `static String headerNameAt(int[] resp, int idx)` | nama header pada baris ke-i (sebelum ':'), "" bila bukan header |
| `static String headerValueAt(int[] resp, int idx)` | — |
| `static String headerLineAt(int[] resp, int idx)` | — |
| `static String headerAll(int[] resp, String name, String sep)` | semua nilai header bernama sama, digabung dengan `sep` |
| `static String multipartType(String boundary)` | — |
| `static String multipartBody(String boundary, String[] names, String[] values)` | — |
| `static int[] multipartPost(int addr, int port, String hostHdr, String path, String boundary, String[] names, String[] values, int cap)` | — |
| `static int[] exchangeTlsDo(int[] ctx, String method, String hostHdr, String path, String headers, int[] body, int cap, int timeoutMs)` | pin = 32 byte SHA-256 SPKI, atau array kosong untuk TOFU. Mengembalikan respons mentah; [0] negatif bila gagal. |
| `static int[] getTls(int addr, int port, String hostHdr, String path, int cap, int[] pin, int timeoutMs)` | — |

---

## Http2

`lib/web/Http2.kop` · 23 fungsi

Klien HTTP/2 (RFC 9113/7540) di atas Tls: gate TLS+ALPN h2+preface, request HEADERS+DATA (stream id bergulir), readResp dengan dekoder HPACK penuh (RFC 7541: indexed, literal, dt dinamis, Huffman).

```java
import Tls;
import Http2;
import Rand;

int[] hctx = Http2.gate(ip, 443, pinSpki, Rand.bytes(32));
if (Http2.respErr(hctx) == 0 && hctx[0] == 1) {
    Http2.request(hctx, "GET", "/", "example.com", new int[0]);
    int[] body = Http2.readResp(hctx);
    System.out.println(Http2.respStatus(hctx));
}
Http2.close(hctx);
```

**Catatan**

- Hanya klien; butuh TLS dengan ALPN h2 (Tls mengirim ext ALPN otomatis).
- readResp mengikuti stream respons aktif (hctx[266]); frame stream lain diabaikan.
- Encoder HPACK literal-tanpa-indexing tanpa Huffman; decoder mendukung Huffman penuh + tabel dinamis.
- Regresi: vektor HPACK/Huffman dari tools/gen_http2vec.py (oracle pustaka python hpack).

| Fungsi | Keterangan |
|---|---|
| `static void initTables(int[] hctx)` | (internal) muat tabel Huffman+statik ke hctx |
| `static int bitAt(int[] b, int pos, int bp)` | (internal) ambil bit ke-pos dari byte array |
| `static int[] hufDec(int[] hctx, int[] src, int pos, int bitLen, int[] okOut)` | (internal) decode Huffman HPACK; simbol 256 = EOS |
| `static void decInt(int[] blk, int p, int n, int[] io)` | (internal) decode integer prefix n bit (HPACK §5.1) |
| `static int[] decStr(int[] hctx, int[] blk, int p, int[] io, int[] okOut)` | (internal) decode string literal HPACK (Huffman bila bit-H=1) |
| `static void dtEvict(int[] hctx)` | (internal) buang entri dt terlama sampai muat |
| `static void dtAdd(int[] hctx, int[] name, int[] val)` | (internal) tambah entri tabel dinamis (name‖val) |
| `static void lookup(int[] hctx, int idx, int[] out, int[] okOut)` | (internal) idx HPACK → name‖val (statik ≥1..61, dinamis ≥62) |
| `static boolean isStatAt(int[] b, int off, int n)` | (internal) cocokkan byte dengan teks statik |
| `static int digitsVal(int[] b, int off, int n)` | (internal) byte ASCII digit → nilai |
| `static void decodeBlock(int[] hctx, int[] blk, int[] okOut)` | (internal) decode blok header HPACK penuh (§6) |
| `static int[] frameHdr(int len, int type, int flags, int sid)` | (internal) header frame 9 byte |
| `static int sendFrame(int[] hctx, int type, int flags, int sid, int[] payload)` | (internal) kirim satu frame lewat Tls |
| `static int[] gate(int ip, int port, int[] pin, int[] randSes)` | buka TLS + ALPN h2, kirim preface+SETTINGS+WINDOW_UPDATE; hctx[1]=1 sukses |
| `static int encStr(int[] out, int n, int[] v)` | (internal) encode string literal tanpa Huffman (§5.2) |
| `static int litIdx(int[] out, int n, int idx, int[] val)` | (internal) literal tanpa indexing, name-index ≤15 |
| `static int litNew(int[] out, int n, int[] name, int[] val)` | (internal) literal tanpa indexing, nama baru |
| `static int request(int[] hctx, String method, String path, String authority, int[] body)` | kirim request (HEADERS+DATA) di stream baru (id 1,3,5,...); hctx[266]=id stream respons |
| `static int fillRb(int[] hctx, int need)` | (internal) isi buffer baca frame dari record TLS |
| `static int[] readResp(int[] hctx)` | baca respons stream aktif sampai END_STREAM/GOAWAY/RST; hasil body |
| `static int respStatus(int[] hctx)` | status :status respons terakhir (0 bila belum ada) |
| `static int respErr(int[] hctx)` | kode galat internal terakhir (0 = ok) |
| `static void close(int[] hctx)` | tutup koneksi TLS |

---

## Http2Tabel

`lib/web/Http2Tabel.kop` · 6 fungsi

Tabel HPACK (RFC 7541) untuk Http2: kode Huffman kanonik 257 simbol (Appendix B) dan tabel statik 61 entri (Appendix A). Hasil generator tools/gen_http2.py — jangan edit manual.

```java
import Http2Tabel;

int[] kode = Http2Tabel.hufCode();
System.out.println(kode[0]);  // simbol 0
```

**Catatan**

- Sumber data: tools/hpack_huffman.tsv (257 baris, commit).

| Fungsi | Keterangan |
|---|---|
| `static int[] hufCode()` | kode Huffman 257 simbol (u32, Appendix B) |
| `static int[] hufLen()` | panjang kode Huffman 257 simbol (byte) |
| `static int[] stName()` | nama tabel statik (ASCII berdempet) |
| `static int[] stNameOff()` | offset nama per entri (62 = sentinel) |
| `static int[] stVal()` | nilai tabel statik (ASCII berdempet) |
| `static int[] stValOff()` | offset nilai per entri (62 = sentinel) |

---

## HttpServer

`lib/web/HttpServer.kop` · 47 fungsi

Parser request HTTP/1.1 dan pembangun respons: method/path/query/header/body, keep-alive, HEAD, cookie, chunked.

```java
import HttpServer;

int[] req = HttpServer.readRequest(c, 16384);
int keep = 0;
if (HttpServer.keepAlive(req)) keep = 1;
int[] resp = HttpServer.responseFor(req, 200, "text/plain", "ok\n", keep);
HttpServer.send(c, resp);
```

**Catatan**

- Tidak ada multipart/HTTP2/TLS server.
- Routing manual; lihat examples/server.

| Fungsi | Keterangan |
|---|---|
| `static int[] readRequest(int fd, int max)` | baca request (header + body sesuai Content-Length). hasil = seluruh byte request; array kosong bila EOF/galat. |
| `static int[] requestLine(int[] req)` | baris pertama request (method target versi) |
| `static String method(int[] req)` | method request (GET/POST/...) |
| `static String target(int[] req)` | target request (path + query) |
| `static String path(int[] req)` | bagian path |
| `static String pathOnly(String target)` | path tanpa query |
| `static String version(int[] req)` | versi HTTP dari baris request |
| `static int findChar(String s, int b, int from, int to)` | cari byte pada String dalam rentang [from,to) |
| `static String headerValue(int[] req, String name)` | nilai header pertama bernama name |
| `static int lowerc(int c)` | — |
| `static boolean nameAt(String line, int ls, int le, String name)` | true bila baris diawali `name` lalu ':' (case-insensitive) |
| `static int contentLength(int[] req)` | nilai Content-Length (0 bila tidak ada) |
| `static int[] body(int[] req)` | isi body request |
| `static int hexv(int c)` | (internal) hex char → nilai |
| `static String urlDecode(String s)` | decode %XX dan + menjadi byte |
| `static String queryValue(int[] req, String key)` | nilai parameter query bernama key |
| `static String statusText(int code)` | teks status standar untuk kode |
| `static int[] responseKA(int code, String ctype, int[] body, int keep)` | bangun respons dengan keep-alive eksplisit |
| `static int[] respond(int code, String ctype, int[] body, String extra, int keep)` | respons dengan header tambahan (mis. Set-Cookie) |
| `static int[] response(int code, String ctype, int[] body)` | bangun respons (Connection: close) |
| `static int[] headOnly(int[] resp)` | HEAD: header tetap (Content-Length body), tanpa body |
| `static int[] responseFor(int[] req, int code, String ctype, int[] body, int keep)` | respons yang meniru kepala request (keep-alive/HEAD) |
| `static boolean methodIs(int[] req, String m)` | true bila method request = m |
| `static int[] text(int code, String body)` | ubah nilai menjadi teks |
| `static int[] html(int code, String body)` | respons HTML lengkap dengan status |
| `static int[] jsonResponse(String body)` | respons application/json lengkap |
| `static int[] notFound()` | respons 404 siap kirim |
| `static int[] forbidden()` | respons 403 siap kirim |
| `static boolean keepAlive(int[] req)` | true bila klien minta koneksi tetap terbuka |
| `static String cookie(int[] req, String name)` | nilai cookie dari header "Cookie: a=1; b=2" |
| `static String setCookie(String name, String value, String path, int maxAge)` | baris Set-Cookie; maxAge < 0 → tanpa Max-Age |
| `static int hexv2(int c)` | decode body chunked (permintaan upload) |
| `static int[] dechunk(int[] raw)` | uraikan body chunked menjadi data biasa |
| `static int[] bodyAny(int[] req)` | body permintaan: chunked bila Transfer-Encoding: chunked |
| `static int[] readRequestLimit(int fd, int max, int limit)` | seperti readRequest tetapi `max` dibatasi `limit` byte |
| `static boolean bodyOver(int[] req, int limit)` | — |
| `static int[] tooLarge()` | — |
| `static String boundaryOf(int[] req)` | boundary dari header Content-Type multipart |
| `static int multipartCount(int[] req)` | — |
| `static String multipartName(int[] req, int idx)` | — |
| `static String multipartFilename(int[] req, int idx)` | — |
| `static int[] multipartData(int[] req, int idx)` | — |
| `static String multipartValue(int[] req, String name)` | — |
| `static int[] rangeOf(int[] req, int total)` | Range: bytes=awal-akhir / awal- / -sufiks → {start,end} inklusif |
| `static int[] partial(String ctype, int[] data, int start, int end)` | respons 206 Partial Content dengan Content-Range |
| `static int[] partialFor(int[] req, String ctype, int[] data)` | — |
| `static int send(int fd, int[] resp)` | kirim respons ke koneksi; jumlah byte |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int[] CRLF()` | (internal) \r\n |
| `static int[] CRLF2()` | (internal) \r\n\r\n |
| `static int[] SP()` | (internal) spasi |

---

## Jwt

`lib/web/Jwt.kop` · 18 fungsi

JWT HS256: tanda tangan, verifikasi, ambil payload.

```java
import Jwt;

String token = Jwt.sign("{\"sub\":\"budi\"}", secret);
int ok = Jwt.verify(token, secret, new int[128]); // 1 sah
```

**Catatan**

- Hanya HS256; `exp`/`iat` tidak diperiksa otomatis.

| Fungsi | Keterangan |
|---|---|
| `static String headerFor(String alg)` | header untuk algoritma tertentu |
| `static String b64(int[] raw)` | base64url tanpa padding (biner → teks) |
| `static int[] unb64(String s)` | (internal) base64url decode |
| `static String signAlg(String payload, String secret, String alg)` | tanda tangani dengan algoritma HS256/HS384/HS512 |
| `static String sign(String payload, String secret)` | tanda tangani payload dengan HS256 → token |
| `static String signHs384(String payload, String secret)` | tanda tangani HS384 (HMAC-SHA384) |
| `static String signHs512(String payload, String secret)` | tanda tangani HS512 (HMAC-SHA512) |
| `static boolean same(int[] a, int[] b)` | bandingkan dua array byte secara konstan (panjang sama) |
| `static String algOf(String token)` | algoritma dari header token |
| `static int verifyAlg(String token, String secret, int[] payloadOut)` | verifikasi sesuai algoritma header; 1 sah, 0 salah, -1 bentuk |
| `static int verify(String token, String secret, int[] payloadOut)` | hasil: 1 valid, 0 tanda tangan salah, -1 bentuk salah |
| `static String payloadOf(String token)` | payload JSON dari token tanpa verifikasi (hati-hati) |
| `static String signRs256(String payload, int[] privKeyDer)` | tanda tangani payload dengan kunci privat DER PKCS#1 (RSAPrivateKey) |
| `static int verifyRs256(String token, int[] pubKeyDer, int[] payloadOut)` | verifikasi token RS256 dengan kunci publik DER PKCS#1 (RSAPublicKey). hasil: 1 sah, 0 tanda tangan salah, -1 bentuk/alg tak didukung |
| `static String signEs256(String payload, int[] priv32)` | tanda tangani payload dengan kunci privat 32 byte (skalar P-256) |
| `static int verifyEs256(String token, int[] pub64, int[] payloadOut)` | verifikasi token ES256 dengan kunci publik 64 byte (X // Y). hasil: 1 sah, 0 tanda tangan salah, -1 bentuk/alg tak didukung |
| `static int checkClaims(String payloadJson, long nowSec)` | cek klaim waktu (exp/nbf); 1 ok, 0 tidak berlaku |
| `static int verifyWithClaims(String token, String secret, int[] payloadOut, long nowSec)` | verifikasi tanda tangan + klaim waktu |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static String HEADER = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}"` | header JWT bawaan {"alg":"HS256"} |

---

## Mime

`lib/web/Mime.kop` · 1 fungsi

Pemetaan ekstensi file → header Content-Type.

```java
import Mime;

String ct = Mime.of("app.js"); // application/javascript
```

| Fungsi | Keterangan |
|---|---|
| `static String of(String path)` | Content-Type berdasarkan ekstensi |

---

## Multipart

`lib/web/Multipart.kop` · 17 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static String contentType(String boundary)` | — |
| `static int findSeq(int[] hay, int[] ndl, int from)` | — |
| `static int[] partAt(int[] body, String boundary, int idx)` | posisi [headers, contentStart, contentEnd] part ke-idx; kosong bila tidak ada |
| `static int count(int[] body, String boundary)` | — |
| `static String disposition(int[] body, String boundary, int idx)` | — |
| `static String contentTypeAt(int[] body, String boundary, int idx)` | — |
| `static String attr(String disp, String key)` | ambil atribut name="..." / filename="..." dari Content-Disposition |
| `static String nameAt(int[] body, String boundary, int idx)` | — |
| `static String filenameAt(int[] body, String boundary, int idx)` | — |
| `static int[] dataAt(int[] body, String boundary, int idx)` | — |
| `static String value(int[] body, String boundary, String name)` | data teks field pertama dengan nama tertentu; "" bila tidak ada |
| `static int[] make()` | buat struktur baru dengan kapasitas awal |
| `static int[] addField(int[] b, String boundary, String name, String value)` | — |
| `static int[] addFile(int[] b, String boundary, String name, String filename, String ctype, int[] data)` | — |
| `static int[] finish(int[] b, String boundary)` | — |
| `static String body(int[] b, String boundary)` | — |
| `static int[] bodyBytes(int[] b, String boundary)` | — |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int[] CRLF()` | — |

---

## Router

`lib/web/Router.kop` · 14 fungsi

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

## Session

`lib/web/Session.kop` · 6 fungsi

Sesi in-memory ber-expiry di atas dua MapStr (id→uid, id→expiry).

```java
import Session;
import MapStr;

int[] ids = MapStr.make(256, 8192);
int[] exps = MapStr.make(256, 8192);
String sid = Session.create(ids, exps, uid, 3600);
int uid2 = Session.get(ids, exps, sid, Net.time());
```

**Catatan**

- Belum persisten; restart server = sesi hilang.

| Fungsi | Keterangan |
|---|---|
| `static String newId()` | id sesi acak 128-bit ter-encode hex |
| `static boolean put(int[] ids, int[] exps, String id, int uid, int ttl, int now)` | simpan pasangan |
| `static String create(int[] ids, int[] exps, int uid, int ttl)` | buat/buka file |
| `static int get(int[] ids, int[] exps, String id, int now)` | -1 bila tidak ada / kedaluwarsa (entri dibersihkan sekalian) |
| `static boolean drop(int[] ids, int[] exps, String id)` | hapus sesi |
| `static int sweep(int[] ids, int[] exps, int now)` | buang semua sesi kedaluwarsa; hasil = jumlah yang dibuang |

---

## Static

`lib/web/Static.kop` · 7 fungsi

Penyajian file statis + MIME; menolak path traversal `..`.

```java
import Static;

int[] resp = Static.serve("public/index.html");
```

**Catatan**

- Membaca seluruh file ke memori; file besar sebaiknya lewat reverse proxy.

| Fungsi | Keterangan |
|---|---|
| `static String mime(String path)` | kompatibilitas: dulu ada di Static, kini delegasi ke Mime |
| `static int[] serve(String path)` | sajikan file; tolak path traversal "..". |
| `static String etag(int[] body)` | — |
| `static boolean notModified(int[] req, String tag)` | — |
| `static int[] serveReqKA(int[] req, String path, int keep)` | respons file dengan ETag, Range, dan 304 (v1.24) |
| `static int[] serveReq(int[] req, String path)` | — |
| `static int[] serveKA(String path, int keep)` | varian keep-alive |

---

## Url

`lib/web/Url.kop` · 26 fungsi

URL/URI RFC 3986: percent-encoding, form encoding, parsing komponen, resolusi relatif, normalisasi path, IPv4.

```java
import Url;

String u = Url.resolve("http://a/b/c/d;p?q", "../g"); // http://a/b/g
String q = Url.appendQuery(Url.pair("q", "a b"), Url.pair("n", "3"));
```

**Catatan**

- `decode` memperlakukan `+` sebagai spasi (form).

| Fungsi | Keterangan |
|---|---|
| `static int hexv(int c)` | (internal) hex char → nilai |
| `static int hexc(int v)` | (internal) nilai → hex char |
| `static boolean unreserved(int c)` | (internal) karakter RFC 3986 unreserved |
| `static int find(String s, int b, int from)` | indeks elemen, -1 bila tidak ada |
| `static String encode(String s)` | percent-encode semua kecuali unreserved |
| `static String decode(String s)` | percent-decode; '+' diperlakukan sebagai spasi |
| `static String formEncode(String s)` | form-urlencoded: spasi → '+', lalu encode sisanya |
| `static String formDecode(String s)` | decode form: %XX, + menjadi spasi |
| `static String pair(String name, String val)` | nama=nilai (keduanya di-encode sebagai form) |
| `static String appendQuery(String q, String p)` | gabungkan query: q kosong → p; selain itu q + "&" + p |
| `static int schemeEnd(String u)` | indeks ':' skema (ALPHA ( ALPHA / DIGIT / "+" / "-" / "." ) ':') |
| `static boolean hasScheme(String u)` | true bila URL diawali skema: |
| `static String scheme(String u)` | skema URL |
| `static int authStart(String u)` | awal otoritas "//" setelah skema; -1 bila tidak ada |
| `static int authEnd(String u)` | (internal) posisi akhir authority |
| `static String authority(String u)` | bagian authority (host:port) |
| `static String host(String u)` | host tanpa port |
| `static int portOf(String u, int def)` | port eksplisit atau default skema |
| `static int pathStart(String u)` | (internal) posisi awal path |
| `static String path(String u)` | bagian path |
| `static String query(String u)` | bagian query tanpa tanda ? |
| `static String fragment(String u)` | bagian fragment tanpa tanda # |
| `static int ip4(String s)` | empat oktet "a.b.c.d" → ip; 0 bila tidak valid |
| `static String normalize(String p)` | normalisasi path (titik-titik RFC 3986 §5.2.4) |
| `static String dropFragment(String u)` | URL tanpa fragment |
| `static String resolve(String base, String rel)` | resolusi referensi relatif terhadap base absolut |

---

## WebSocket

`lib/web/WebSocket.kop` · 12 fungsi

Server WebSocket RFC 6455: handshake, frame teks/biner/ping/pong/close, decode frame klien bermask.

```java
import WebSocket;

if (WebSocket.isUpgrade(req)) {
    Net.sendb(c, WebSocket.handshake(req));
    int[] f = WebSocket.decode(buf, n);
    Net.sendb(c, WebSocket.text(WebSocket.payload(f)));
}
```

**Catatan**

- Satu frame per read; tanpa fragmentasi/deflate; payload < 64 KiB.

| Fungsi | Keterangan |
|---|---|
| `static String accept(String key)` | Sec-WebSocket-Accept = base64(SHA1(key + GUID)) |
| `static boolean isUpgrade(int[] req)` | true bila request minta upgrade websocket |
| `static int[] handshake(int[] req)` | respons 101 Switching Protocols |
| `static int[] frame(int op, int[] payload)` | bangun frame server (tanpa mask) |
| `static int[] text(int[] s)` | ubah nilai menjadi teks |
| `static int[] binary(int[] s)` | frame op=2 (biner) |
| `static int[] ping(int[] s)` | frame ping |
| `static int[] pong(int[] s)` | frame pong |
| `static int[] close(int code)` | tutup deskriptor |
| `static int[] decode(int[] b, int n)` | hasil: [opcode, panjang, payload...]; array kosong bila < 2 byte |
| `static int opcode(int[] f)` | opcode frame (1 teks, 2 biner, 8 close) |
| `static int[] payload(int[] f)` | payload frame (unmasked) |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static String GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"` | GUID RFC 6455 untuk handshake |

---

## WsClient

`lib/web/WsClient.kop` · 15 fungsi

Modul pustaka Kopi.

| Fungsi | Keterangan |
|---|---|
| `static int[] recvN(int fd, int n)` | — |
| `static String recvUntil(int fd, String marker, int max)` | baca sampai penanda (mis. CRLF CRLF) atau max byte |
| `static String randomKey()` | — |
| `static int[] connect(int ip, int port, String host, String path)` | hasil ctx: [0]=fd, [1]=ok(0/1), [2]=tertutup |
| `static int sendFrame(int[] ctx, int op, int[] payload)` | — |
| `static int sendText(int[] ctx, String s)` | — |
| `static int sendBinary(int[] ctx, int[] b)` | — |
| `static int sendPing(int[] ctx, String s)` | — |
| `static int sendPong(int[] ctx, String s)` | — |
| `static int closeClient(int[] ctx, int code)` | — |
| `static int[] recv(int[] ctx)` | — |
| `static int opcode(int[] f)` | — |
| `static int[] payload(int[] f)` | — |
| `static String recvText(int[] ctx)` | — |
| `static boolean isClosed(int[] ctx)` | — |

---
