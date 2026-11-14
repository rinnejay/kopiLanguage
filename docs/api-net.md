# Referensi API — Net

TCP/UDP, DNS, TLS, dan protokol jaringan dasar. Semua signature di bawah diekstrak langsung dari source `lib/net/`; deskripsi berasal dari komentar sumber atau metadata.

> Impor dengan `import {Nama};` — nama class sama dengan nama file. Contoh pada tiap modul adalah pola pakai, bukan program utuh.

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
