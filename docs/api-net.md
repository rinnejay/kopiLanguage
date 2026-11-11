# Referensi API — Net

TCP/UDP, HTTP, WebSocket, URL, auth, DNS, TLS. Semua signature di bawah diekstrak langsung dari source `lib/net/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

## Auth

`lib/net/Auth.kop` · 8 fungsi

Parser header Authorization: skema, Basic (user:pass), Bearer, tantangan 401.

```java
import Auth;

int[] h = HttpServer.headerValue(req, Net.bytes("Authorization"));
if (!Auth.checkBasic(h, user, pass)) { /* 401 */ }
```

| Fungsi | Keterangan |
|---|---|
| `static int spaceAt(int[] v)` | (internal) posisi spasi pertama di header |
| `static int[] scheme(int[] v)` | skema URL |
| `static int[] token(int[] v)` | bagian token setelah skema |
| `static boolean same(int[] a, int[] b)` | bandingkan dua nilai |
| `static int[] basicRaw(int[] v)` | hasil "user:pass" dari Basic, kosong bila bukan Basic |
| `static boolean checkBasic(int[] v, int[] user, int[] pass)` | true bila Basic cocok user:pass |
| `static int[] bearer(int[] v)` | token Bearer |
| `static int[] challenge(int[] realm)` | baris header tantangan untuk 401 |

---

## Dns

`lib/net/Dns.kop` · 6 fungsi

Klien DNS-over-TCP (RFC 1035/7766) ke server eksplisit; query A + parser jawaban.

```java
import Dns;

int[] hasil = new int[5];
Dns.lookup(serverIp, 53, Net.bytes("example.com"), hasil, 512);
```

**Catatan**

- Hanya record A; ID 0; satu jawaban pertama.
- Untuk resolver sistem lihat `Resolv`.

| Fungsi | Keterangan |
|---|---|
| `static int[] qname(int[] dotted)` | nama "kopi.test" (byte-as-int, pemisah titik ASCII 46) → QNAME: panjang·oktet ... lalu terminator 0 |
| `static int[] query(int[] dottedName, int qtype)` | permintaan penuh: header + QNAME + QTYPE + QCLASS=1 (IN) |
| `static int skipName(int[] m, int i)` | lewati nama di dalam pesan: label (hitung byte) atau pointer 2 byte (≥ 0xC0); kembalikan indeks sesudah nama |
| `static int rcodeOf(int[] resp)` | nilai RCODE dari flag byte ke-4 |
| `static int parseA(int[] buf, int[] hasil)` | telaah pesan DNS mentah (tanpa awalan panjang TCP) menjadi A. hasil[0]=RCODE, hasil[1..4]=oktet IPv4 (atau -1). |
| `static int lookup(int addr, int port, int[] dottedName, int[] hasil, int cap)` | kirim kueri A (qtype=1) ke addr:port (TCP, awalan panjang 2 byte) dan telaah balasannya melalui hasil: hasil[0] = RCODE (0 = NOERROR, 3 = NXDOMAIN, ...), hasil[1..4] = oktet IPv… |

---

## Http

`lib/net/Http.kop` · 27 fungsi

Klien HTTP/1.1: GET/POST, header kustom, chunked, redirect, basic auth, timeout 5 detik.

```java
import Tcp;
import Http;

int[] r = Http.getFull(Tcp.loopback(), 8080, Net.bytes("127.0.0.1"), Net.bytes("/health"), 8192);
System.out.println(Http.statusOf(r));
System.out.println(Http.payload(r));
```

**Catatan**

- API lama (`getInto`, `statusOf`, `bodyOf`) tetap tersedia.
- Redirect hanya diikuti bila host sama.

| Fungsi | Keterangan |
|---|---|
| `static int[] req(int[] host, int[] path)` | susun baris permintaan GET lengkap: "GET <path> HTTP/1.0\r\nHost: <host>\r\n\r\n" (Net.bytes literal sama di-dedup compiler ke satu isi cpool) |
| `static int statusOf(int[] resp)` | kode status HTTP dari baris pertama ("HTTP/1.0 200 OK"), -1 bila tidak cocol dengan format baris status |
| `static int[] bodyOf(int[] resp)` | isi (body) respons: semua byte setelah pemisah CRLF CRLF. tanpa pemisah (jawaban tak utuh) → seluruh respons dibalikkan. |
| `static int getInto(int addr, int port, int[] host, int[] path, int[] buf)` | kirim GET dan muat SELURUH respons (header + body) ke buf. sukses → total byte respons (≥ 0); gagal → kode negatif. buf terlalu kecil → respons terpotong (batas v1 tercatat). |
| `static int postInto(int addr, int port, int[] host, int[] path, int[] body, int[] buf)` | kirim POST dengan body (Content-Length dari Tcp.num), muat SELURUH respons ke buf — semantik sama dengan getInto. |
| `static int[] body(int addr, int port, int[] host, int[] path, int cap)` | kenyamanan: GET lalu langsung ke body. galat ditandai array hasil [0] = kode negatif (body tidak mungkin negatif). |
| `static int[] reqEx(int[] method, int[] host, int[] path, int[] headers, int[] body)` | kirim permintaan generik dengan header tambahan. `headers` berisi baris mentah "Nama: nilai\r\n" (boleh kosong). body kosong (length 0) → tanpa Content-Length (GET/HEAD). HTTP/1… |
| `static int requestInto(int addr, int port, int[] method, int[] host, int[] path, int[] headers, int[] body, int[] buf)` | satu putaran penuh dengan method/header/body bebas. |
| `static int getWith(int addr, int port, int[] host, int[] path, int[] headers, int[] buf)` | GET dengan header tambahan |
| `static int postWith(int addr, int port, int[] host, int[] path, int[] headers, int[] body, int[] buf)` | POST dengan header tambahan + body |
| `static int bodyStart(int[] resp)` | indeks awal body (setelah CRLF CRLF), -1 bila tidak ada |
| `static int[] headerBlock(int[] resp)` | potongan header mentah (tanpa baris status & tanpa CRLF CRLF) |
| `static int lowerc(int c)` | (internal) huruf kecil ASCII |
| `static boolean nameEq(int[] line, int at, int[] name)` | (internal) nama header cocok pada posisi |
| `static int[] headerValue(int[] resp, int[] name)` | nilai header (tanpa spasi tepi), array kosong bila tidak ada. perbandingan nama case-insensitive; baris pertama yang cocok dipakai. |
| `static boolean hasHeader(int[] resp, int[] name)` | true bila respons punya header name |
| `static int hexVal(int c)` | ═══ v1.9.2: HTTP/1.1 — chunked, redirect, form, auth ═══════ |
| `static boolean hasToken(int[] resp, int[] name, int[] tok)` | cari token (case-insensitive) di nilai header |
| `static int[] dechunk(int[] raw)` | decode Transfer-Encoding: chunked (hex-size CRLF data CRLF ... 0) |
| `static int[] payload(int[] resp)` | body sebenarnya: chunked → decode; Content-Length → potong |
| `static int[] req11(int[] method, int[] hostHdr, int[] path, int[] headers, int[] body, int keep)` | susun permintaan HTTP/1.1 (keep=1 → keep-alive, else close) |
| `static int[] exchange(int addr, int port, int[] method, int[] hostHdr, int[] path, int[] headers, int[] body, int cap)` | satu transaksi; timeoutMs > 0 memasang SO_RCVTIMEO/SO_SNDTIMEO |
| `static int[] getFull(int addr, int port, int[] hostHdr, int[] path, int cap)` | GET + respons lengkap (status, header, body) |
| `static int[] follow(int addr, int port, int[] hostHdr, int[] path, int maxHops, int cap)` | ikuti 301/302/303/307/308 (maks maxHops, host harus sama) |
| `static int[] basic(int[] user, int[] pass)` | header "Authorization: Basic ..." dari user:pass |
| `static int[] formPost(int addr, int port, int[] hostHdr, int[] path, int[] form, int cap)` | POST application/x-www-form-urlencoded |
| `static int[] jsonPost(int addr, int port, int[] hostHdr, int[] path, int[] jsonBody, int cap)` | POST application/json |

---

## HttpServer

`lib/net/HttpServer.kop` · 33 fungsi

Parser request HTTP/1.1 dan pembangun respons: method/path/query/header/body, keep-alive, HEAD, cookie, chunked.

```java
import HttpServer;

int[] req = HttpServer.readRequest(c, 16384);
int keep = 0;
if (HttpServer.keepAlive(req)) keep = 1;
int[] resp = HttpServer.responseFor(req, 200, Net.bytes("text/plain"), Net.bytes("ok\n"), keep);
HttpServer.send(c, resp);
```

**Catatan**

- Tidak ada multipart/HTTP2/TLS server.
- Routing manual; lihat examples/server.

| Fungsi | Keterangan |
|---|---|
| `static int findByte(int[] s, int b, int from, int to)` | (internal) cari byte dalam rentang |
| `static int[] readRequest(int fd, int max)` | baca request (header + body sesuai Content-Length). hasil = seluruh byte request; array kosong bila EOF/galat. |
| `static int[] requestLine(int[] req)` | baris pertama request (method target versi) |
| `static int[] method(int[] req)` | method request (GET/POST/...) |
| `static int[] target(int[] req)` | target request (path + query) |
| `static int[] path(int[] req)` | bagian path |
| `static int[] pathOnly(int[] target)` | path tanpa query |
| `static int[] version(int[] req)` | versi HTTP dari baris request |
| `static int[] headerValue(int[] req, int[] name)` | nilai header pertama bernama name |
| `static int contentLength(int[] req)` | nilai Content-Length (0 bila tidak ada) |
| `static int[] body(int[] req)` | isi body request |
| `static int hexv(int c)` | (internal) hex char → nilai |
| `static int[] urlDecode(int[] s)` | decode %XX dan + menjadi byte |
| `static int[] queryValue(int[] req, int[] key)` | nilai parameter query bernama key |
| `static int[] statusText(int code)` | teks status standar untuk kode |
| `static int[] responseKA(int code, int[] ctype, int[] body, int keep)` | bangun respons dengan keep-alive eksplisit |
| `static int[] respond(int code, int[] ctype, int[] body, int[] extra, int keep)` | respons dengan header tambahan (mis. Set-Cookie) |
| `static int[] response(int code, int[] ctype, int[] body)` | bangun respons (Connection: close) |
| `static int[] headOnly(int[] resp)` | HEAD: header tetap (Content-Length body), tanpa body |
| `static int[] responseFor(int[] req, int code, int[] ctype, int[] body, int keep)` | respons yang meniru kepala request (keep-alive/HEAD) |
| `static boolean methodIs(int[] req, int[] m)` | true bila method request = m |
| `static int[] text(int code, int[] body)` | ubah nilai menjadi teks |
| `static int[] html(int code, int[] body)` | respons HTML lengkap dengan status |
| `static int[] jsonResponse(int[] body)` | respons application/json lengkap |
| `static int[] notFound()` | respons 404 siap kirim |
| `static int[] forbidden()` | respons 403 siap kirim |
| `static boolean keepAlive(int[] req)` | true bila klien minta koneksi tetap terbuka |
| `static int[] cookie(int[] req, int[] name)` | nilai cookie dari header "Cookie: a=1; b=2" |
| `static int[] setCookie(int[] name, int[] value, int[] path, int maxAge)` | baris Set-Cookie; maxAge < 0 → tanpa Max-Age |
| `static int hexv2(int c)` | decode body chunked (permintaan upload) |
| `static int[] dechunk(int[] raw)` | uraikan body chunked menjadi data biasa |
| `static int[] bodyAny(int[] req)` | body permintaan: chunked bila Transfer-Encoding: chunked |
| `static int send(int fd, int[] resp)` | kirim respons ke koneksi; jumlah byte |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int[] CRLF()` | (internal) \r\n |
| `static int[] CRLF2()` | (internal) \r\n\r\n |
| `static int[] SP()` | (internal) spasi |

---

## Jwt

`lib/net/Jwt.kop` · 6 fungsi

JWT HS256: tanda tangan, verifikasi, ambil payload.

```java
import Jwt;

int[] token = Jwt.sign(Net.bytes("{\"sub\":\"budi\"}"), secret);
int ok = Jwt.verify(token, secret, new int[128]); // 1 sah
```

**Catatan**

- Hanya HS256; `exp`/`iat` tidak diperiksa otomatis.

| Fungsi | Keterangan |
|---|---|
| `static int[] b64(int[] raw)` | base64url tanpa padding |
| `static int[] unb64(int[] s)` | (internal) base64url decode |
| `static int[] sign(int[] payload, int[] secret)` | tanda tangani payload dengan HS256 → token |
| `static boolean same(int[] a, int[] b)` | bandingkan dua array byte secara konstan (panjang sama) |
| `static int verify(int[] token, int[] secret, int[] payloadOut)` | hasil: 1 valid, 0 tanda tangan salah, -1 bentuk salah |
| `static int[] payloadOf(int[] token)` | payload JSON dari token tanpa verifikasi (hati-hati) |

**Konstanta**

| Nama | Keterangan |
|---|---|
| `static int[] HEADER()` | header JWT bawaan {"alg":"HS256"} |

---

## Mime

`lib/net/Mime.kop` · 1 fungsi

Pemetaan ekstensi file → header Content-Type.

```java
import Mime;

int[] ct = Mime.of(Net.bytes("app.js")); // application/javascript
```

| Fungsi | Keterangan |
|---|---|
| `static int[] of(int[] path)` | Content-Type berdasarkan ekstensi |

---

## Resolv

`lib/net/Resolv.kop` · 5 fungsi

Resolver DNS UDP + pembaca `/etc/resolv.conf`, dengan timeout.

```java
import Resolv;

int[] hasil = new int[5];
Resolv.lookup(Net.bytes("example.com"), 2000, hasil, 512);
// hasil[0]=RCODE, hasil[1..4]=oktet IPv4
```

**Catatan**

- Tanpa cache/AAAA/`search`/`ndots`.

| Fungsi | Keterangan |
|---|---|
| `static int defaultServer()` | IP server nama pertama /etc/resolv.conf (0 bila gagal) |
| `static int[] parse(int[] text)` | telaah teks resolv.conf → daftar ip (flat) |
| `static int[] servers()` | semua IP nameserver dari /etc/resolv.conf |
| `static int lookupUdp(int serverIp, int port, int[] name, int timeoutMs, int[] hasil, int cap)` | satu kueri A lewat UDP; hasil[0]=RCODE, hasil[1..4]=oktet |
| `static int lookup(int[] name, int timeoutMs, int[] hasil, int cap)` | coba semua server dari resolv.conf |

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
int[] sid = Session.create(ids, exps, uid, 3600);
int uid2 = Session.get(ids, exps, sid, Net.time());
```

**Catatan**

- Belum persisten; restart server = sesi hilang.

| Fungsi | Keterangan |
|---|---|
| `static int[] newId()` | id sesi acak 128-bit ter-encode hex |
| `static boolean put(int[] ids, int[] exps, int[] id, int uid, int ttl, int now)` | simpan pasangan |
| `static int[] create(int[] ids, int[] exps, int uid, int ttl)` | buat/buka file |
| `static int get(int[] ids, int[] exps, int[] id, int now)` | -1 bila tidak ada / kedaluwarsa (entri dibersihkan sekalian) |
| `static boolean drop(int[] ids, int[] exps, int[] id)` | hapus sesi |
| `static int sweep(int[] ids, int[] exps, int now)` | buang semua sesi kedaluwarsa; hasil = jumlah yang dibuang |

---

## Static

`lib/net/Static.kop` · 3 fungsi

Penyajian file statis + MIME; menolak path traversal `..`.

```java
import Static;

int[] resp = Static.serve(Net.bytes("public/index.html"));
```

**Catatan**

- Membaca seluruh file ke memori; file besar sebaiknya lewat reverse proxy.

| Fungsi | Keterangan |
|---|---|
| `static int[] mime(int[] path)` | kompatibilitas: dulu ada di Static, kini delegasi ke Mime |
| `static int[] serve(int[] path)` | sajikan file; tolak path traversal "..". |
| `static int[] serveKA(int[] path, int keep)` | varian keep-alive |

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

`lib/net/Tls.kop` · 25 fungsi

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
| `static int[] chMsg(int[] sk, int[] rnd)` | handshake message ClientHello (tanpa header record) |
| `static int[] parseSH(int[] isi)` | isi fragment SH (dari record header). balikan = key_share pub server (32 byte) atau kosong bila gagal. |
| `static int[] gate(int ip, int port, int[] pin)` | Untuk uji: gate() pakai Rand; gateFixed dipatok kunci ephemeral. Inti bersama di handshake(). |
| `static int[] gateFixed(int ip, int port, int[] pin, int[] fsk, int[] frnd)` | (internal) handshake dengan pin & kunci tetap (uji) |
| `static int[] handshake(int ip, int port, int[] pin, int[] randSes)` | handshake TLS 1.3; context untuk writeApp/readApp |
| `static int writeApp(int[] ctx, int[] data)` | kirim data aplikasi terenkripsi |
| `static int[] readApp(int[] ctx)` | baca data aplikasi (deenkripsi) |

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

int[] u = Url.resolve("http://a/b/c/d;p?q", "../g"); // http://a/b/g
int[] q = Url.appendQuery(Url.pair("q", "a b"), Url.pair("n", "3"));
```

**Catatan**

- `decode` memperlakukan `+` sebagai spasi (form).

| Fungsi | Keterangan |
|---|---|
| `static int hexv(int c)` | (internal) hex char → nilai |
| `static int hexc(int v)` | (internal) nilai → hex char |
| `static boolean unreserved(int c)` | (internal) karakter RFC 3986 unreserved |
| `static int find(int[] s, int b, int from)` | indeks elemen, -1 bila tidak ada |
| `static int[] encode(int[] s)` | percent-encode semua kecuali unreserved |
| `static int[] decode(int[] s)` | percent-decode; '+' diperlakukan sebagai spasi |
| `static int[] formEncode(int[] s)` | form-urlencoded: spasi → '+', lalu encode sisanya |
| `static int[] formDecode(int[] s)` | decode form: %XX, + menjadi spasi |
| `static int[] pair(int[] name, int[] val)` | nama=nilai (keduanya di-encode sebagai form) |
| `static int[] appendQuery(int[] q, int[] p)` | gabungkan query: q kosong → p; selain itu q + "&" + p |
| `static int schemeEnd(int[] u)` | indeks ':' skema (ALPHA ( ALPHA / DIGIT / "+" / "-" / "." ) ':') |
| `static boolean hasScheme(int[] u)` | true bila URL diawali skema: |
| `static int[] scheme(int[] u)` | skema URL |
| `static int authStart(int[] u)` | awal otoritas "//" setelah skema; -1 bila tidak ada |
| `static int authEnd(int[] u)` | (internal) posisi akhir authority |
| `static int[] authority(int[] u)` | bagian authority (host:port) |
| `static int[] host(int[] u)` | host tanpa port |
| `static int portOf(int[] u, int def)` | port eksplisit atau default skema |
| `static int pathStart(int[] u)` | (internal) posisi awal path |
| `static int[] path(int[] u)` | bagian path |
| `static int[] query(int[] u)` | bagian query tanpa tanda ? |
| `static int[] fragment(int[] u)` | bagian fragment tanpa tanda # |
| `static int ip4(int[] s)` | empat oktet "a.b.c.d" → ip; 0 bila tidak valid |
| `static int[] normalize(int[] p)` | normalisasi path (titik-titik RFC 3986 §5.2.4) |
| `static int[] dropFragment(int[] u)` | URL tanpa fragment |
| `static int[] resolve(int[] base, int[] rel)` | resolusi referensi relatif terhadap base absolut |

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
| `static int[] accept(int[] key)` | Sec-WebSocket-Accept = base64(SHA1(key + GUID)) |
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
| `static int[] GUID()` | GUID RFC 6455 untuk handshake |

---
