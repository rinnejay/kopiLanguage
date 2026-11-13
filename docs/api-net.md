# Referensi API — Net

TCP/UDP, HTTP, WebSocket, URL, auth, DNS, TLS. Semua signature di bawah diekstrak langsung dari source `lib/net/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Auth

`lib/net/Auth.kop` · 8 fungsi

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

## Dns

`lib/net/Dns.kop` · 17 fungsi

Klien DNS-over-TCP (RFC 1035/7766) ke server eksplisit; query A + parser jawaban.

```java
import Dns;

int[] hasil = new int[5];
Dns.lookup(serverIp, 53, "example.com", hasil, 512);
```

**Catatan**

- Hanya record A; ID 0; satu jawaban pertama.
- Untuk resolver sistem lihat `Resolv`.

| Fungsi | Keterangan |
|---|---|
| `static int[] qname(String dotted)` | nama "kopi.test" (String, pemisah titik ASCII 46) → QNAME biner: panjang·oktet ... lalu terminator 0 |
| `static int[] query(String dottedName, int qtype)` | permintaan penuh: header + QNAME + QTYPE + QCLASS=1 (IN) |
| `static int skipName(int[] m, int i)` | lewati nama di dalam pesan: label (hitung byte) atau pointer 2 byte (≥ 0xC0); kembalikan indeks sesudah nama |
| `static int rcodeOf(int[] resp)` | nilai RCODE dari flag byte ke-4 |
| `static int parseA(int[] buf, int[] hasil)` | telaah pesan DNS mentah (tanpa awalan panjang TCP) menjadi A. hasil[0]=RCODE, hasil[1..4]=oktet IPv4 (atau -1). |
| `static String readName(int[] m, int i)` | nama pada posisi i (mengikuti pointer kompresi) |
| `static int[] findRR(int[] buf, int want)` | posisi awal RDATA record pertama bertipe `want`; {-1} bila tidak ada |
| `static int rrTypeCount(int[] buf, int want)` | — |
| `static int parseAAAA(int[] buf, int[] hasil)` | hasil[0]=RCODE, hasil[1..16]=16 byte IPv6 (atau -1) |
| `static String hex4(int v)` | — |
| `static String ipv6Text(int[] hasil)` | IPv6 kanonik (8 grup 16-bit heksadesimal, tanpa kompresi nol) |
| `static String cname(int[] buf)` | — |
| `static int mxPref(int[] buf)` | — |
| `static String mxHost(int[] buf)` | — |
| `static String txt(int[] buf)` | — |
| `static int lookup(int addr, int port, String dottedName, int[] hasil, int cap)` | kirim kueri A (qtype=1) ke addr:port (TCP, awalan panjang 2 byte) dan telaah balasannya melalui hasil: hasil[0] = RCODE (0 = NOERROR, 3 = NXDOMAIN, ...), hasil[1..4] = oktet IPv… |
| `static int[] lookupType(int addr, int port, String dottedName, int qtype, int cap)` | kueri mentah tipe bebas; hasil = payload DNS (tanpa awalan TCP) |

---

## Http

`lib/net/Http.kop` · 44 fungsi

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

`lib/net/Http2.kop` · 23 fungsi

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

`lib/net/Http2Tabel.kop` · 6 fungsi

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

`lib/net/HttpServer.kop` · 47 fungsi

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

`lib/net/Jwt.kop` · 16 fungsi

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
| `static int checkClaims(String payloadJson, long nowSec)` | cek klaim waktu (exp/nbf); 1 ok, 0 tidak berlaku |
| `static int verifyWithClaims(String token, String secret, int[] payloadOut, long nowSec)` | verifikasi tanda tangan + klaim waktu |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static String HEADER()` | header JWT bawaan {"alg":"HS256"} |

---

## Mime

`lib/net/Mime.kop` · 1 fungsi

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

`lib/net/Multipart.kop` · 17 fungsi

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

## Resolv

`lib/net/Resolv.kop` · 5 fungsi

Resolver DNS UDP + pembaca `/etc/resolv.conf`, dengan timeout.

```java
import Resolv;

int[] hasil = new int[5];
Resolv.lookup("example.com", 2000, hasil, 512);
// hasil[0]=RCODE, hasil[1..4]=oktet IPv4
```

**Catatan**

- Tanpa cache/AAAA/`search`/`ndots`.

| Fungsi | Keterangan |
|---|---|
| `static int defaultServer()` | IP server nama pertama /etc/resolv.conf (0 bila gagal) |
| `static int[] parse(String text)` | telaah teks resolv.conf → daftar ip (flat) |
| `static int[] servers()` | semua IP nameserver dari /etc/resolv.conf |
| `static int lookupUdp(int serverIp, int port, String name, int timeoutMs, int[] hasil, int cap)` | satu kueri A lewat UDP; hasil[0]=RCODE, hasil[1..4]=oktet |
| `static int lookup(String name, int timeoutMs, int[] hasil, int cap)` | coba semua server dari resolv.conf |

---

## Rfc

`lib/net/Rfc.kop` · 5 fungsi

Klien protokol TCP klasik (daytime, echo, quote, whois, finger, discard) untuk latihan.

```java
import Rfc;

int[] balas = Rfc.daytime(serverIp);
```

**Catatan**

- Protokol lama; jangan dipakai untuk layanan baru.

| Fungsi | Keterangan |
|---|---|
| `static int echoInto(int addr, int port, int[] data, int[] out)` | kirim data, baca balikan sebanyak data.length; sukses → jumlah byte ≥ 0 (biasanya == data.length); gagal → negatif |
| `static int discard(int addr, int port, int[] data)` | kirim data ke port discard (9); 0 bila sukses |
| `static int readAll(int addr, int port, int[] buf)` | buka, muat ke buf hingga EOF/server tutup, tutup; sukses → n ≥ 0 |
| `static int fill(int addr, int port, int[] buf)` | muat buf PENUH pattern (server tak berhenti sendiri) lalu tutup |
| `static int ask(int addr, int port, int[] enqu, int[] buf)` | kirim pertanyaan (harus sudah diakhiri '\r\n'), muat balasan |

---

## Session

`lib/net/Session.kop` · 6 fungsi

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

`lib/net/Static.kop` · 7 fungsi

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

## Tcp

`lib/net/Tcp.kop` · 13 fungsi

Transport TCP di atas builtin `Net`: buka koneksi, kirim/terima buffer, shutdown, helper alamat.

```java
import Tcp;

int fd = Tcp.open(Tcp.loopback(), 8080);
Tcp.sendBuf(fd, Net.bytes("GET / HTTP/1.1\r\n\r\n"));
int[] buf = new int[4096];
int n = Tcp.recvAll(fd, buf);
Tcp.drop(fd);
```

**Catatan**

- `recvAll` berhenti saat EOF atau buffer penuh.
- `open` menutup fd sendiri bila connect gagal.

| Fungsi | Keterangan |
|---|---|
| `static int ip(int a, int b, int c, int d)` | susun alamat dari 4 oktet a.b.c.d |
| `static int loopback()` | alamat khas untuk uji: 127.0.0.1 |
| `static int open(int addr, int port)` | buka TCP klien. sukses → fd ≥ 0; gagal → kode negatif (sumber galat dibedakan: socket → 0, connect → -errno). |
| `static int drop(int fd)` | tutup koneksi (alias builtin close agar satu API yang diingat) |
| `static int shutdown(int fd, int how)` | half-close: SHUT_RD=0, SHUT_WR=1, SHUT_RDWR=2 |
| `static int send(int fd, int[] buf, int len)` | kirim len byte pertama buf. sukses → 0; gagal → kode negatif dan sisa byte tidak jadi terkirim (penelepon yang memutus). |
| `static int sendBuf(int fd, int[] buf)` | kirim seluruh buf. sukses → 0; gagal → kode negatif |
| `static int recvAll(int fd, int[] buf)` | baca hingga EOF atau buf penuh. sukses → jumlah byte ≥ 0. satu syscall = satu byte, jadi galat baca builtin hanya muncul sekali lalu identik EOF: keduanya mengerucut ke akhir baca. |
| `static int[] cat(int[] a, int[] b)` | gabung a lalu b jadi array baru |
| `static int[] tail(int[] xs, int from)` | potong mulai indeks from sampai akhir |
| `static int indexOf(int[] hay, int[] needle, int from)` | indeks pertama needle dalam hay mulai dari indeks from; -1 bila tak ada |
| `static int recvN(int fd, int[] buf, int n)` | baca tepat n byte (atau berhenti di EOF/buf penuh); kembalikan jumlah byte yang benar-benar terbaca — untuk protokol berbatas panjang eksplisit (DNS-over-TCP misalnya) |
| `static int[] num(int v)` | v ≥ 0 → digit ASCII desimal (tanpa tanda); dipakai Content-Length |

---

## Tls

`lib/net/Tls.kop` · 27 fungsi

Klien TLS 1.3: X25519, ChaCha20-Poly1305, HKDF, pin SPKI TOFU, verifikasi Finished, data aplikasi dua arah.

```java
import Tls;
import Rand;

int[] ctx = Tls.handshake(ip, 443, pinSpki, Rand.bytes(32));
Tls.writeApp(ctx, Net.bytes("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"));
int[] data = Tls.readApp(ctx);
```

**Catatan**

- Bukan TLS server; belum verifikasi ECDSA CertificateVerify/rantai/hostname.
- Pin SPKI = SHA-256 DER SPKI leaf (TOFU).

| Fungsi | Keterangan |
|---|---|
| `static int[] cat(int[] a, int[] b)` | (internal) gabung byte |
| `static int[] cat3(int[] a, int[] b, int[] c)` | (internal) gabung tiga array |
| `static int[] arr1(int a)` | (internal) array satu byte |
| `static int[] arr2(int a, int b)` | (internal) array dua byte |
| `static int[] arr3(int a, int b, int cc)` | (internal) array tiga byte |
| `static int[] sliceOf(int[] x, int from, int len)` | (internal) potong array |
| `static int[] u16(int v)` | (internal) encode uint16 besar-endian |
| `static int[] u24(int v)` | (internal) encode uint24 besar-endian |
| `static int get16(int[] x, int p)` | (internal) baca uint16 pada posisi |
| `static int get24(int[] x, int p)` | (internal) baca uint24 pada posisi |
| `static int[] tkey(int[] secret)` | (internal) client write key dari secret |
| `static int[] tiv(int[] secret)` | (internal) client write iv dari secret |
| `static int[] nonceOf(int[] iv, int seq)` | nonce = iv ^ (seq 64-bit big-endian di byte 4..11) (RFC 8446 §5.2). seq 32-bit: s[0..3] nol, sehingga hanya n[8..11] yang ter-XOR. Hindari shift >= 32 (UB di C). |
| `static int[] seal(int[] key, int[] iv, int seq, int ctype, int[] payload)` | seal: payload + ctype → record luar penuh |
| `static int[] openRec(int[] key, int[] iv, int seq, int[] rec, int[] okOut)` | open: verifikasi tag dulu, lalu unplug keystream. okOut[0] = -1 galat, selain itu inner type (1..255). balikan = isi plaintext (tanpa inner type). |
| `static int[] recvExact(int fd, int n)` | (internal) baca tepat n byte dari fd |
| `static int[] recvRecord(int fd)` | (internal) baca satu record TLS |
| `static int[] extOf(int id, int[] body)` | (internal) bangun extension |
| `static int[] alpnExts()` | ALPN (16): daftar protokol; kita tawarkan "h2" saja. body = u16(len daftar) ‖ u8(len nama) ‖ nama (RFC 7301/7540) |
| `static int[] chMsg(int[] sk, int[] rnd)` | handshake message ClientHello (tanpa header record) |
| `static int[] parseSH(int[] isi, int[] alpnOut)` | isi fragment SH (dari record header). balikan = key_share pub server (32 byte) atau kosong bila gagal. alpnOut[0] = 1 bila server memilih protokol "h2" (ALPN id 16, RFC 7540 §3.3). |
| `static void parseEEalpn(int[] plain, int[] alpnOut)` | isi EncryptedExtensions ( plaintext pesan): cari ALPN (id 16) — di TLS 1.3 protokol yang dipilih server ada DI SINI (§4.3.2), bukan di ServerHello. alpnOut[0] = 1 bila "h2". |
| `static int[] gate(int ip, int port, int[] pin)` | Untuk uji: gate() pakai Rand; gateFixed dipatok kunci ephemeral. Inti bersama di handshake(). |
| `static int[] gateFixed(int ip, int port, int[] pin, int[] fsk, int[] frnd)` | (internal) handshake dengan pin & kunci tetap (uji) |
| `static int[] handshake(int ip, int port, int[] pin, int[] randSes)` | handshake TLS 1.3; context untuk writeApp/readApp |
| `static int writeApp(int[] ctx, int[] data)` | kirim data aplikasi terenkripsi |
| `static int[] readApp(int[] ctx)` | baca data aplikasi (deenkripsi) |

---

## TlsServer

`lib/net/TlsServer.kop` · 28 fungsi

Server TLS 1.3: X25519 + ECDSA P-256 self-signed, ChaCha20-Poly1305/AES-GCM, verifikasi client Finished, data aplikasi dua arah.

```java
import TlsServer;
import Tcp;

int lfd = Net.socket(2, 1);
Net.bind(lfd, Tcp.loopback(), 9443);
Net.listen(lfd, 4);
int fd = Net.accept(lfd);
int[] ctx = TlsServer.accept(fd, certDer, priv32, Rand.bytes(64));
int[] pesan = TlsServer.readApp(ctx);
TlsServer.writeApp(ctx, Net.bytes("pong"));
```

**Catatan**

- Suite: TLS_CHACHA20_POLY1305_SHA256 (0x1303), AES-128-GCM (0x1301), AES-256-GCM (0x1302) — dipilih dengan prioritas SHA-256.
- Sertifikat self-signed ECDSA-SHA256 dibangun oleh certDer; kunci privat = skalar P-256 32 byte.
- Handshake tervalidasi terhadap klien OpenSSL/Python (oracle).

| Fungsi | Keterangan |
|---|---|
| `static int[] tlv(int tag, int[] body)` | (internal) DER TLV |
| `static int[] cat(int[] a, int[] b)` | — |
| `static int[] cat3(int[] a, int[] b, int[] c)` | — |
| `static int[] arr1(int a)` | — |
| `static int[] arr2(int a, int b)` | — |
| `static int[] u16(int v)` | — |
| `static int[] u24(int v)` | — |
| `static int[] hexp(String hex)` | — |
| `static int[] spaces64()` | 64 spasi untuk konten CertificateVerify (RFC 8446 §4.4.3) |
| `static int[] oid(String raw)` | (internal) OID dari teks titik |
| `static int[] seq(int[] body)` | (internal) DER SEQUENCE |
| `static int[] setOf(int[] body)` | — |
| `static int[] integer1()` | — |
| `static int[] utctimeA()` | — |
| `static int[] utctimeB()` | — |
| `static int[] bitstr(int[] body)` | — |
| `static int[] explicit0(int[] body)` | — |
| `static int[] nameKopi()` | Name satu RDN: CN=kopi (Net.bytes butuh literal) |
| `static int[] certDer(int[] priv32)` | bangun sertifikat self-signed ECDSA-SHA256 (X.509 v3) dari skalar privat |
| `static int[] intDer32(int[] v)` | INTEGER DER dari 32 byte skalar (kanonik: leading-zero dipangkas, 0x00 didepan bila bit tertinggi terpasang) |
| `static int[] sigDer(int[] rs)` | (internal) bungkus r//s jadi DER ECDSA-Sig-Value |
| `static int[] pinOf(int[] cert)` | pin SPKI sertifikat (SHA-256 DER SPKI) |
| `static int[] keyOf(int[] secret, int alg)` | kunci AEAD sesuai alg (RFC 8446 §7.3): aes128=16, aes256=32, chacha=32 |
| `static int[] sealR(int alg, int[] key, int[] iv, int seq, int ctype, int[] payload)` | record keluar penuh: header ‖ ciphertext ‖ tag |
| `static int[] openR(int alg, int[] key, int[] iv, int seq, int[] rec, int[] okOut)` | record masuk → plaintext (tanpa byte tipe dalam). okOut[0]: -1 galat, selain itu tipe dalam (20 finished, 4 NST, 21 alert, 23 app). |
| `static int[] accept(int fd, int[] certDer, int[] priv32, int[] randSes)` | handshake server TLS 1.3; context untuk writeApp/readApp (ctx[1]=1 sukses) |
| `static int writeApp(int[] ctx, int[] data)` | kirim data aplikasi terenkripsi |
| `static int[] readApp(int[] ctx)` | baca data aplikasi (deenkripsi); kosong = tutup/galat |

---

## Udp

`lib/net/Udp.kop` · 7 fungsi

Datagram UDP: bind, kirim/terima satu datagram, shutdown.

```java
import Udp;

int s = Udp.open();
Udp.bind(s, 0, 9000);
int[] buf = new int[1200];
int[] from = new int[2];
int n = Udp.recvFrom(s, buf, from);
Udp.sendTo(s, from[0], from[1], Net.bytes("pong"));
```

**Catatan**

- Maksimum 1200 byte per datagram.
- Tidak ada retransmisi/urutan; paket bisa hilang.

| Fungsi | Keterangan |
|---|---|
| `static int loopback()` | alamat 127.0.0.1 |
| `static int open()` | buka socket UDP (AF_INET, SOCK_DGRAM) → fd / negatif |
| `static int bind(int fd, int addr, int port)` | ikat ke alamat lokal → 0 / -errno |
| `static int sendTo(int fd, int addr, int port, int[] data)` | kirim satu datagram: data[0..len-1] (maks 1200 byte) → n / -errno |
| `static int recvFrom(int fd, int[] buf, int[] from)` | terima satu datagram ke buf; sumber diisi ke from[0]=ip, from[1]=port. hasil = jumlah byte (≥0) atau kode negatif. |
| `static int shutdown(int fd, int how)` | half-close (SHUT_RD=0, SHUT_WR=1, SHUT_RDWR=2) |
| `static int close(int fd)` | tutup deskriptor |

---

## Url

`lib/net/Url.kop` · 26 fungsi

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

`lib/net/WebSocket.kop` · 12 fungsi

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
| `static String GUID()` | GUID RFC 6455 untuk handshake |

---

## WsClient

`lib/net/WsClient.kop` · 15 fungsi

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
